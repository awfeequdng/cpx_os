#include <vmm.h>
#include <shmem.h>
#include <assert.h>
#include <pmm.h>
#include <slab.h>
#include <sync.h>
#include <error.h>
#include <swap.h>
#include <string.h>
#include <process.h>
#include <stdio.h>

static int vma_compare(rbtree_node_t *node1, rbtree_node_t *node2) {
    VmaStruct *vma1 = rbn2vma(node1, rb_link);
    VmaStruct *vma2 = rbn2vma(node2, rb_link);
    uintptr_t start1 = vma1->vm_start;
    uintptr_t start2 = vma2->vm_start;
    return (start1 < start2) ? -1 : ((start1 > start2) ? 1 : 0);
}

MmStruct *mm_create(void) {
    MmStruct *mm = kmalloc(sizeof(MmStruct));
    if (mm != NULL) {
        list_init(&(mm->mmap_link));
        mm->mmap_cache = NULL;
        mm->page_dir = NULL;
        rbtree_init(&(mm->mmap_tree), vma_compare);
        mm->map_count = 0;
        mm->swap_address = 0;
        set_mm_count(mm, 0);
        lock_init(&(mm->mm_lock));
        mm->brk_start = mm->brk = 0;
    }

    return mm;
}

VmaStruct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags) {
    VmaStruct *vma = kmalloc(sizeof(VmaStruct));
    if (vma != NULL) {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
        // rbtree_node_init(&(vma->rb_link), rbtree_sentinel(tree));
        list_init(&(vma->vma_link));
    }
    return vma;
}

// 找到addr右邊最近的vma
static inline VmaStruct *find_vma_rb(rbtree_t *tree, uintptr_t addr) {
    rbtree_node_t *node = rbtree_root(tree);
    VmaStruct *tmp = NULL;
    VmaStruct *vma = NULL;
    while (node  != rbtree_sentinel(tree)) {
        tmp = rbn2vma(node, rb_link);
        if (tmp->vm_end > addr) {
            // vma爲addr右邊第一個出現的vma，包括addr在vma的地址範圍內
            vma = tmp;
            if (tmp->vm_start <= addr) {
                // addr 在vma的地址範圍內
                break;
            }
            node = node->left;
        } else {
            node = node->right;
        }
    }
    return vma;
}

VmaStruct *find_vma(MmStruct *mm, uintptr_t addr) {
    VmaStruct *vma = NULL;
    if (mm != NULL) {
        vma = mm->mmap_cache;
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
            // 紅黑樹非空
            if (rbtree_root(&(mm->mmap_tree)) != rbtree_sentinel(&(mm->mmap_tree))) {
                vma = find_vma_rb(&(mm->mmap_tree), addr);
            } else {
                bool found = 0;
                ListEntry *head = &(mm->mmap_link);
                ListEntry *entry = head;
                // vma鏈表是按地址從小到大排序的
                while ((entry = list_next(entry)) != head) {
                    vma = le2vma(entry, vma_link);
                    if (addr < vma->vm_end) {
                        // addr可能在vma的左邊，也可能在vma內部
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    vma = NULL;
                }
            }
        }
        if (vma != NULL) {
            mm->mmap_cache = vma;
        }
    }
    return vma;
}

// 找一个地址和[start, end)有交叉的vma
VmaStruct *find_vma_intersection(MmStruct *mm, uintptr_t start, uintptr_t end) {
    VmaStruct *vma = find_vma(mm, start);
    if (vma != NULL && end <= vma->vm_start) {
        // vma地址和[start, end)没有交叉
        vma = NULL;
    }
    return vma;
}



static inline void check_vma_overlap(VmaStruct *prev, VmaStruct *next) {
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);
}

static inline void insert_vma_rb(rbtree_t *tree, VmaStruct *vma, VmaStruct **vma_prevp) {
    rbtree_node_t *node = &(vma->rb_link);
    rbtree_node_t *prev = NULL;
    rbtree_insert(tree, node);
    if (vma_prevp != NULL) {
        prev = rbtree_predecessor(tree, node);
        if (prev != rbtree_sentinel(tree)) {
            *vma_prevp = rbn2vma(prev, rb_link);
        } else {
            *vma_prevp = NULL;
        }
    }
}

// 插入新的vma时，即使两个vma相邻，并且vm_flags也相同，也没有将两个vma合并
// todo: 此处有必要做一下优化，将相邻的并且属性相同的vma合并
void insert_vma_struct(MmStruct *mm, VmaStruct *vma) {
    assert(vma->vm_start < vma->vm_end);
    ListEntry *head = &(mm->mmap_link);
    ListEntry *entry_prev = head;
    ListEntry *entry_next = NULL;
    ListEntry *entry = NULL;
    
    if (rbtree_root(&(mm->mmap_tree)) != rbtree_sentinel(&(mm->mmap_tree))) {
        VmaStruct *mmap_prev = NULL;
        insert_vma_rb(&(mm->mmap_tree), vma, &mmap_prev);
        if (mmap_prev != NULL) {
            entry_prev = &(mmap_prev->vma_link);
        }
    } else {
        ListEntry *entry = head;
        while ((entry = list_next(entry)) != head) {
            VmaStruct *mmap_prev = le2vma(entry, vma_link);
            if (mmap_prev->vm_start > vma->vm_start) {
                // vma插入在mmap_prev前面
                break;
            }
            entry_prev = entry;
        }
    }
    
    entry_next = list_next(entry_prev);

    if (entry_prev != head) {
        check_vma_overlap(le2vma(entry_prev, vma_link), vma);
    }
    if (entry_next != head) {
        check_vma_overlap(vma, le2vma(entry_next, vma_link));
    }

    vma->vm_mm = mm;
    list_add_after(entry_prev, &(vma->vma_link));

    mm->map_count++;

    if (rbtree_root(&(mm->mmap_tree)) == rbtree_sentinel(&(mm->mmap_tree)) &&
        mm->map_count >= RB_MIN_MAP_COUNT) {
        head = &(mm->mmap_link);
        entry = head;
        while ((entry = list_next(entry)) != head) {
            insert_vma_rb(&(mm->mmap_tree), le2vma(entry, vma_link), NULL);
        }
    }
}

// 申请[addr, addr + len)这块地址作为堆内存使用
int mm_brk(MmStruct *mm, uintptr_t addr, size_t len) {
    uintptr_t start = ROUNDDOWN(addr, PAGE_SIZE);
    uintptr_t end = ROUNDUP(addr + len, PAGE_SIZE);
    if (!USER_ACCESS(start, end)) {
        return -E_INVAL;
    }

    int ret;
    // todo: 为什么需要unmap？
    if ((ret = mm_unmap(mm, start, end - start)) != 0) {
        return ret;
    }

    uint32_t vm_flags = VM_READ | VM_WRITE;
    VmaStruct *vma = find_vma(mm, start - 1);
    if (vma != NULL && vma->vm_end == start && vma->vm_flags == vm_flags) {
        // [start, end)之前的vma与[start, end)正好相邻，并且vm_flags也相同，则直接将之前的vma扩大以下end地址范围即可
        vma->vm_end = end;
        return 0;
    }
    if ((vma = vma_create(start, end, vm_flags)) == NULL) {
        return -E_NO_MEM;
    }
    insert_vma_struct(mm, vma);
    return 0;
}

static int remove_vma_struct(MmStruct *mm, VmaStruct *vma) {
    assert(mm == vma->vm_mm);
    if (rbtree_root(&(mm->mmap_tree)) != rbtree_sentinel(&(mm->mmap_tree))) {
        rbtree_delete(&(mm->mmap_tree), &(vma->rb_link));
    }
    list_del(&(vma->vma_link));
    if (vma == mm->mmap_cache) {
        mm->mmap_cache = NULL;
    }
    mm->map_count--;
    return 0;
}

static void vma_destory(VmaStruct *vma) {
    if (vma->vm_flags & VM_SHARE) {
        if (shmem_ref_dec(vma->shmem) == 0) {
            // 此共享内存结构没有vma引用了，将其释放
            shmem_destory(vma->shmem);
        }
    }
    kfree(vma);
}

void mm_destory(MmStruct *mm) {
    // if (rbtree_root(&(mm->mmap_tree)) != rbtree_sentinel(&(mm->mmap_tree))) {
    // }
    ListEntry *head = &(mm->mmap_link);
    ListEntry *entry = head;
    while ((entry = list_next(entry)) != head) {
        list_del(entry);
        vma_destory(le2vma(entry, vma_link));
    }
    kfree(mm);
}

static void check_vmm(void);
static void check_vma_struct();
static void check_page_fault();

void vmm_init(void) {
    check_vmm();
}

// 从[addr, addr + len)建立一个vma映射
int mm_map(MmStruct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
         VmaStruct **vma_store) {
    uintptr_t start = ROUNDDOWN(addr, PAGE_SIZE);
    uintptr_t end = ROUNDUP(addr + len, PAGE_SIZE);
    if (!USER_ACCESS(start, end)) {
        return -E_INVAL;
    }

    assert (mm != NULL);

    int ret = -E_INVAL;
    VmaStruct *vma;
    if ((vma = find_vma(mm, start)) != NULL && end > vma->vm_start) {
        // 如果找到的地址范围[start， end)和存在vma地址范围重叠，则直接退出，返回失败
        goto out;
    }

    ret = -E_NO_MEM;
    // todo: 为什么这个时候将VM_SHARE的标志清除掉？
    vm_flags &= ~VM_SHARE;
    if ((vma = vma_create(start, end, vm_flags)) == NULL) {
        goto out;
    }

    // 将刚创建的vma加入mm
    insert_vma_struct(mm, vma);
    if (vma_store != NULL) {
        *vma_store = vma;
    }
    ret = 0;
out:
    return ret;
}

int mm_map_shmem(MmStruct *mm, uintptr_t addr, uint32_t vm_flags,
        ShareMemory *shmem, VmaStruct **vma_store) {
    if ((addr % PAGE_SIZE) != 0 || shmem == NULL) {
        return -E_INVAL;
    }
    int ret;
    VmaStruct *vma;
    // 又有一个vma引用该shmem结构，因此引用计数加1
    shmem_ref_inc(shmem);
    // 当创建一个共享的vma时，mm_map中占时不设置VM_SHARE标志，
    // 等到创建完vma后再设置这个标志
    if ((ret = mm_map(mm, addr, shmem->len, vm_flags, &vma)) != 0) {
        shmem_ref_dec(shmem);
        return ret;
    }
    vma->shmem = shmem;
    vma->shmem_off = 0;
    vma->vm_flags |= VM_SHARE;
    if (vma_store != NULL) {
        *vma_store = vma;
    }
    return 0;
}

// 检查用户空间的内存是否可写（write为true），或者可读（write为false）
// 当mm不为NULL时，检查mm的用户空间地址是否合法，此时addr必须时用户态的地址；
// 当mm为NULL时，addr地址就是内核态的地址范围
bool user_mem_check(MmStruct *mm, uintptr_t addr, size_t len, bool write) {
    if (mm != NULL) {
        if (!USER_ACCESS(addr, addr + len)) {
            return 0;
        }
        VmaStruct *vma = NULL;
        uintptr_t start = addr;
        uintptr_t end = addr + len;
        while (start < end) {
            if ((vma = find_vma(mm, start)) == NULL) {
                return false;
            }
            if (!(vma->vm_flags & (write ? VM_WRITE : VM_READ))) {
                return false;
            }
            // 地址空间可写，并且时STACK区域
            if (write && (vma->vm_flags & VM_STACK)) {
                // todo: 这是何意？？
                if (start < vma->vm_start + PAGE_SIZE) {
                    return false;
                }
            }
            start = vma->vm_end;
        }
        return true;
    }

    return KERNEL_ACCESS(addr, addr + len);
}

static void vma_resize(VmaStruct *vma, uintptr_t start, uintptr_t end) {
    assert(start % PAGE_SIZE == 0 && end % PAGE_SIZE == 0);
    // 调整vma大小时，只能缩小范围
    assert(vma->vm_start <= start && start < end && vma->vm_end >= end);
    if (vma->vm_flags & VM_SHARE) {
        // shmem_off表示什么？
        vma->shmem_off += start - vma->vm_start;
    }
    vma->vm_start = start;
    vma->vm_end = end;
}

// 释放[start, end)虚拟地址范围内映射的物理页
static void unmap_range(pte_t *page_dir, uintptr_t start, uintptr_t end) {
    assert(start % PAGE_SIZE == 0 && end % PAGE_SIZE == 0);
    assert(USER_ACCESS(start, end));

    do {
        pte_t *ptep = get_pte(page_dir, start, 0);
        if (ptep == NULL) {
            start = ROUNDDOWN(start + PT_SIZE, PT_SIZE);
            continue;
        }
        if (*ptep != 0) {
            page_remove_pte(page_dir, start, ptep);
        }
        start += PAGE_SIZE;
    } while (start != 0 && start < end);
}

int mm_unmap(MmStruct *mm, uintptr_t addr, size_t len) {
    uintptr_t start = ROUNDDOWN(addr, PAGE_SIZE);
    uintptr_t end = ROUNDUP(addr + len, PAGE_SIZE);
    if (!USER_ACCESS(start, end)) {
        return -E_INVAL;
    }
    assert(mm != NULL);

    VmaStruct *vma;
    if ((vma = find_vma(mm, start)) == NULL || end <= vma->vm_start) {
        // 没有找到addr对应的vma
        return 0;
    }
    // 如果[start, end)在vma的地址范围内，则将vma分成左边界和右边界两个vma
    if (vma->vm_start < start && end < vma->vm_end) {
        VmaStruct *left_vma;
        if ((left_vma = vma_create(vma->vm_start, start, vma->vm_flags)) == NULL) {
            return -E_NO_MEM;
        }
        // 解除[start, end)的映射，将vma拆成了[vma->start, start)和[end, vma->end)
        // 将vma范围缩小为[end, vma->vm_end)
        vma_resize(vma, end, vma->vm_end);
        insert_vma_struct(mm, left_vma);
        // 将[start, end)的范围内映射的page释放掉
        unmap_range(mm->page_dir, start, end);
        return 0;
    }

    ListEntry free_list, *entry;
    list_init(&free_list);
    // vma->vm_start < end那么[start, end)和[vma->vm_start, vma->vm_end)一定有交叉
    while (vma->vm_start < end) {
        entry = list_next(&(vma->vma_link));
        // 将vma从mm的链表和红黑树上摘下来
        remove_vma_struct(mm, vma);
        list_add(&free_list, &(vma->vma_link));
        if (entry == &(mm->mmap_link)) {
            break;
        }
        vma = le2vma(entry, vma_link);
    }

    entry = list_next(&free_list);
    while (entry != &free_list) {
        vma = le2vma(entry, vma_link);
        entry = list_next(entry);
        uintptr_t un_start, un_end;
        if (vma->vm_start < start) {
            un_start = start;
            un_end = vma->vm_end;
            vma_resize(vma, vma->vm_start, un_start);
            insert_vma_struct(mm, vma);
        } else {
            un_start = vma->vm_start;
            un_end = vma->vm_end;
            if (un_end <= end) {
                vma_destory(vma);
            } else {
                un_end = end;
                vma_resize(vma, end, vma->vm_end);
                insert_vma_struct(mm, vma);
            }
        }
        unmap_range(mm->page_dir, un_start, un_end);
    }
    return 0;
}

// 删除物理页表
void exit_range(pde_t *page_dir, uintptr_t start, uintptr_t end) {
    assert(start % PAGE_SIZE == 0 && end % PAGE_SIZE == 0);
    assert(USER_ACCESS(start, end));

    start = ROUNDDOWN(start, PT_SIZE);
    do {
        int pde_idx = PDX(start);
        if (page_dir[pde_idx] & PTE_P) {
            free_page(pde2page(page_dir[pde_idx]));
            page_dir[pde_idx] = 0;
        }
        start += PT_SIZE;
    } while (start != 0 && start < end);
}

// 将from的页目录以及页表项的内容拷贝到to，
// 如果share为true的话，那么from的页表项和to的页表项指向相同的物理内存，
// 也就是实现共享内存。当share为false的话，from的页表项和to的页表项指向不同的物理内存，
// to页表项指向的物理内存时from的拷贝。
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share) {
    assert(start % PAGE_SIZE == 0 && end % PAGE_SIZE == 0);
    assert(USER_ACCESS(start, end));
    // 目前share字段没有使用，默认采用非共享的方式赋值内存
    do {
        pte_t *ptep = get_pte(from, start, 0);
        pte_t *new_ptep = NULL;
        if (ptep == NULL) {
            start = ROUNDDOWN(start + PT_SIZE, PT_SIZE);
            continue;
        }
        if (*ptep != 0) {
            if ((new_ptep = get_pte(to, start, 1)) == NULL) {
                return -E_NO_MEM;
            }
            int ret;
            struct Page *new_page = NULL;
            assert(*ptep != 0 && *new_ptep == 0);
            if (*ptep & PTE_P) {
                uint32_t perm = (*ptep & PTE_USER);
                new_page = pte2page(*ptep);
                if (!share && (*ptep & PTE_W)) {
                    // 实现写时复制功能，当父进程的pte具有写标志，将这个标志清除掉，以便在写操作的时候产生page fault
                    perm &= ~PTE_W;
                    // 父进程保留页表项之前的PTE_A和PTE_D的标志
                    page_insert(from, new_page, start, perm | (*ptep & PTE_SWAP));
                }
                // 如果父进程之前有PTE_W标志，这个标志会被清除，因此子进程也没有设置PTE_W标志。在有写入时会产生page fault，从而实现写时复制功能
                ret = page_insert(to, new_page, start, perm);
                assert(ret == 0);
            } else {
                // ptep指向swap entry
                swap_entry_t entry = *ptep;
                swap_duplicate(entry);
                *new_ptep = entry;
            }
        }
        start += PAGE_SIZE;
    } while (start != 0 && start < end);
    return 0;
}

int dup_mmap(MmStruct *to, MmStruct *from) {
    assert(to != NULL && from != NULL);
    ListEntry *head = &(from->mmap_link);
    ListEntry *entry = head;
    while ((entry = list_prev(entry)) != head) {
        VmaStruct *vma = NULL, *new_vma = NULL;
        vma = le2vma(entry, vma_link);
        new_vma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags);
        if (new_vma == NULL) {
            return -E_NO_MEM;
        } else {
            if (vma->vm_flags & VM_SHARE) {
                new_vma->shmem = vma->shmem;
                new_vma->shmem_off = vma->shmem_off;
                shmem_ref_inc(vma->shmem);
            }
        }
        insert_vma_struct(to, new_vma);
        bool share = (vma->vm_flags & VM_SHARE);
        // 复制页表项
        if (copy_range(to->page_dir, from->page_dir, vma->vm_start, vma->vm_end, share) != 0) {
            return -E_NO_MEM;
        }
    }
    return 0;
}

// 删除vma映射的物理页，以及所有页表，只留下一个页目录
void exit_mmap(MmStruct *mm) {
    assert(mm != NULL && mm_count(mm) == 0);
    pde_t *page_dir = mm->page_dir;
    ListEntry *head = &(mm->mmap_link);
    ListEntry *entry = head;
    while ((entry = list_next(entry)) != head) {
        VmaStruct *vma = le2vma(entry, vma_link);
        unmap_range(page_dir, vma->vm_start, vma->vm_end);
    }
    while ((entry = list_next(entry)) != head) {
        VmaStruct *vma = le2vma(entry, vma_link);
        exit_range(page_dir, vma->vm_start, vma->vm_end);
    }
}

static void check_vmm(void) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    check_vma_struct();
    check_page_fault();
    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());

    printk("check_vmm successed.\n");
}


static void check_vma_struct() {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    MmStruct *mm = mm_create();
    assert(mm != NULL);

    int step1 = RB_MIN_MAP_COUNT * 2;
    int step2 = step1 * 10;
    int i;
    for (i = step1; i >= 0; i --) {
        VmaStruct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    for (i = step1 + 1; i <= step2; i ++) {
        VmaStruct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    ListEntry *entry = list_next(&(mm->mmap_link));

    for (i = 0; i <= step2; i++) {
        assert(entry != &(mm->mmap_link));
        VmaStruct *mmap = le2vma(entry, vma_link);
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
        entry = list_next(entry);
    }

    for (i = 0; i < 5 * step2 + 2; i++) {
        VmaStruct *vma = find_vma(mm, i);
        assert(vma != NULL);
        int j = i / 5;
        if (i >= 5 * j + 2) {
            j++;
        }
        assert(vma->vm_start == j * 5 && vma->vm_end == j * 5 + 2);
    }
    mm_destory(mm);
    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());
    printk("check_vma_struct: successed\n");
}

MmStruct *check_mm_struct;

static void check_page_fault() {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    check_mm_struct = mm_create();
    assert(check_mm_struct != NULL);

    MmStruct *mm = check_mm_struct;
    pde_t *page_dir = mm->page_dir = get_boot_page_dir();
    assert(page_dir[0] == 0);

    VmaStruct *vma = vma_create(0, PT_SIZE, VM_WRITE);
    assert(vma != NULL);

    insert_vma_struct(mm, vma);

    uintptr_t addr = 0x100;
    assert(find_vma(mm, addr) == vma);

    int i = 0, sum = 0;
    // 此處會產生page fault中斷
    for (i = 0; i < 100; i++) {
        *(char *)(addr + i) = i;
        sum += i;
    }
    for (i = 0; i < 100; i++) {
        sum -= *(char *)(addr + i);
    }
    assert(sum == 0);

    page_remove(page_dir, ROUNDDOWN(addr, PAGE_SIZE));

    free_page(pa2page(page_dir[0]));
    page_dir[0] = 0;
    mm->page_dir = NULL;
    mm_destory(mm);
    check_mm_struct = NULL;

    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());
}

int do_page_fault(MmStruct *mm, uint32_t error_code, uintptr_t addr) {
    if (mm == NULL) {
        assert(current != NULL);
        panic("page fault in kernel thread: pid = %d, %d %08x.\n",
            current->pid, error_code, addr);
    }
    lock_mm(mm);
    int ret = -E_INVAL;

    VmaStruct *vma = find_vma(mm, addr);
    // 找到的vma可能在addr的右邊
    if (vma == NULL || vma->vm_start > addr) {
        goto failed;
    }

    if (vma->vm_flags & VM_STACK) {
        // todo: 为什么需要判断addr的值不能在vma->vm_start + PAGE_SIZE的下面，
        //是不是需要用一个page的空间来和其他段分开（比如和堆分开）
        if (addr < vma->vm_start + PAGE_SIZE) {
            goto failed;
        }
    }

    switch (error_code & 3) {
        default:    // default is 3: write, present
        case 2:     // write, not present
            if (!(vma->vm_flags & VM_WRITE)) {
                // 該vma不能寫
                goto failed;
            }
            break;
        case 1:     // read, present
            goto failed;
        case 0:     // read, not preset
            if (!(vma->vm_flags & (VM_READ | VM_EXEC))) {
                // 該vma不可讀並且不可執行，page fault直接錯誤退出
                goto failed;
            }
    }
    // 用户权限，只有用户空间地址才会产生page fault
    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= PTE_W;
    }
    addr = ROUNDDOWN(addr, PAGE_SIZE);
    
    ret = -E_NO_MEM;

    pte_t *ptep = NULL;
    if ((ptep = get_pte(mm->page_dir, addr, 1)) == NULL) {
        goto failed;
    }

    if (*ptep == 0) {
        if (!(vma->vm_flags & VM_SHARE)) {
            // vma不是共享内存
            // 如果页表项为0，表示即不存在和page的映射，也不存在和swap的映射
            if (page_dir_alloc_page(mm->page_dir, addr, perm) == 0) {
                goto failed;
            }
        } else {
            // vma是共享内存
            shmem_lock(vma->shmem);
            // vma->shmem_off表示vma->vm_start的值与共享内存开始的地址的偏移量
            uintptr_t shmem_addr = addr - vma->vm_start + vma->shmem_off;
            // 当发生page fault时，vma中的pte是直接从共享内存的pte拷贝过来的，
            // 因此vma设置了VM_SHARE标志时，发生了page fault就先创建共享内存结构中的pte，
            // 然后再将共享内存结构中的pte拷贝到vma
            pte_t *shmem_ptep = shmem_get_entry(vma->shmem, shmem_addr, 1);
            if (shmem_ptep == NULL || *shmem_ptep == 0) {
                // 创建共享内存结构中的pte失败，可能是没有内存了
                shmem_unlock(vma->shmem);
                goto failed;
            }
            shmem_unlock(vma->shmem);
            if (*shmem_ptep & PTE_P) {
                // 在共享内存结构中存在pte值，将其指向的page插入页表项
                page_insert(mm->page_dir, pte2page(*shmem_ptep), addr, perm);
            } else {
                // todo: 这里为什么不设置页表项为page的地址，而是swap entry的值，
                // 这就导致还会进行一次的page fault，干嘛不一次到位
                // swap_duplicate(*shmem_ptep);
                // *ptep = *shmem_ptep;

                // 我们可以一步到位，不需要产生两次page fault
                struct Page *page = NULL;
                if ((ret = swap_in_page(*shmem_ptep, &page)) != 0) {
                    goto failed;
                }
                page_insert(mm->page_dir, page, addr, perm);
            }
        }
    } else {
        // 页表项不为0，那么产生page fault的原因就可能是PTE_P没有置位（PTE_P没有置位，但是pte的值不为0，说明pte指向swap entry），
        // 或者PTE_P置位了，但是没有设置PTE_W标志而发生了写错误
        struct Page *page = NULL;
        struct Page *new_page = NULL;
        // vma不是共享内存的，并且可写，
        // 此时产生了page fault将执行cow，也就是会产生新的page
        bool cow = ((vma->vm_flags & (VM_SHARE | VM_WRITE)) == VM_WRITE);
        bool may_copy = true;
        bool dec_swap_entry = false;
        swap_entry_t swap_entry = 0;
        // 页表项没有指向page，或者
        // 写错误(error_code & 2)，并且pte没有写权限，但是该vma有写的权限没有共享内存。也就是说该vma有写权限，但是页表项没有设置PTE_W标志，从而导致写时page fault
        assert(!(*ptep & PTE_P) || ((error_code & 2) && !(*ptep & PTE_W) && cow));

        if (cow) {
            new_page = alloc_page();
        }

        if (*ptep & PTE_P) {
            // PTE_P位存在，说明发生了权限错误，可能是写操作时pte没有设置PTE_W标志导致的page fault
            page = pte2page(*ptep);
        } else {
            // PTE_P标志不存在而导致的page fault
            // 此时pte的值指向swap entry，
            // 通过该swap entry找到page或者将数据换入进新的page
            if ((ret = swap_in_page(*ptep, &page)) != 0) {
                if (new_page != NULL) {
                    free_page(new_page);
                }
                goto failed;
            }
            // page从swap中取出来，对应的pte可能需要指向这个这个page，
            // 因此swap entry的计数就需要减1
            dec_swap_entry = true;
            swap_entry = page->index;

            if (!(error_code & 2) && cow) {
                // 不是写错误（即pte有写权限），并且支持写时复制，（此次是读产生的，不需要复制page）
                // 既然是写时复制的vma，当前产生的page fault又不是写入产生的，那么就是PTE_P没有置位，
                // 那么相应的pte应该清除PTE_W标志，让其在写入时发生page fault，从而进行page的拷贝
                // 将swap entry对应的page加载进来，然后清除PTE_W（此次没必要复制page），并且不复制page
                perm &= ~PTE_W;
                // 次轮page fault不是写入导致的，不需要复制page
                may_copy = false;
            }
        }
        
        // if ((ret = swap_in_page(*ptep, &page)) != 0) {
        //     goto failed;
        // }
        // page_insert(mm->page_dir, page, addr, perm);
        // // 页表项pte不指向swap entry时，对应的swap entry引用计数减1
        // swap_decrease(page->index);

        // 支持写时复制，并且当前需要拷贝page
        if (cow && may_copy) {
            // 有pte引用这个page，或者有swap entry引用这个page，
            // 那么page中的内容是有用的，写时复制就需要拷贝这个page
            // page总的引用计数大于1，说明有多个pte引用同一个页表，此时发生page fault就需要写时复制
            // 如果page的引用计数为1，说明page只有一个pte引用了，就没有必要再复制page，发生page fault可能就是PTE_W没有置位，现在将这个标志设置上即可
            if (page_ref(page) + swap_page_count(page) > 1) {
                if (new_page == NULL) {
                    goto failed;
                }
                memcpy(page2kva(new_page), page2kva(page), PAGE_SIZE);
                page = new_page;
                new_page = NULL;
            }
        }

        page_insert(mm->page_dir, page, addr, perm);
        
        if (dec_swap_entry) {
            // 如果page是从swap框架中加载来的，则将对应的swap entry引用计数减1
            swap_decrease(swap_entry);
        }
        if (new_page != NULL) {
            free_page(new_page);
        }
    }

    ret = 0;

failed:
    unlock_mm(mm);
    return ret;
}

void print_vma(void) {
    MmStruct *mm = NULL;
    if (current == NULL || current->mm == NULL || current->mm->map_count == 0) {
        if (check_mm_struct == NULL || check_mm_struct->map_count == 0) {
            return;
        }
        mm = check_mm_struct;
    } else {
        mm = current->mm;
    }
    VmaStruct *vma = NULL;
    ListEntry *head = &(mm->mmap_link);
    ListEntry *entry = list_next(head);
    while (entry != head) {
        vma = le2vma(entry, vma_link);
        printk("(start,end)=(%08x,%08x), flags=%08x \n", vma->vm_start, vma->vm_end, vma->vm_flags);
        entry = list_next(entry);
    }
    
}

