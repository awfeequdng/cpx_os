#include <vmm.h>

#include <assert.h>
#include <pmm.h>
#include <slab.h>
#include <sync.h>
#include <error.h>
#include <swap.h>

static int vma_compare(rbtree_node_t *node1, rbtree_node_t *node2) {
    struct VmaStruct *vma1 = rbn2vma(node1, rb_link);
    struct VmaStruct *vma2 = rbn2vma(node2, rb_link);
    uintptr_t start1 = vma1->vm_start;
    uintptr_t start2 = vma2->vm_start;
    return (start1 < start2) ? -1 : ((start1 > start2) ? 1 : 0);
}

struct MmStruct *mm_create(void) {
    struct MmStruct *mm = kmalloc(sizeof(struct MmStruct));
    if (mm != NULL) {
        list_init(&(mm->mmap_link));
        mm->mmap_cache = NULL;
        mm->page_dir = NULL;
        rbtree_init(&(mm->mmap_tree), vma_compare);
        mm->map_count = 0;
        mm->swap_address = 0;
    }

    return mm;
}

struct VmaStruct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags) {
    struct VmaStruct *vma = kmalloc(sizeof(struct VmaStruct));
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
static inline struct VmaStruct *find_vma_rb(rbtree_t *tree, uintptr_t addr) {
    rbtree_node_t *node = rbtree_root(tree);
    struct VmaStruct *tmp = NULL;
    struct VmaStruct *vma = NULL;
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

struct VmaStruct *find_vma(struct MmStruct *mm, uintptr_t addr) {
    struct VmaStruct *vma = NULL;
    if (mm != NULL) {
        vma = mm->mmap_cache;
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
            // 紅黑樹非空
            if (rbtree_root(&(mm->mmap_tree)) != rbtree_sentinel(&(mm->mmap_tree))) {
                vma = find_vma_rb(&(mm->mmap_tree), addr);
            } else {
                bool found = 0;
                list_entry_t *head = &(mm->mmap_link);
                list_entry_t *entry = head;
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

static inline void check_vma_overlap(struct VmaStruct *prev, struct VmaStruct *next) {
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);
}

static inline void insert_vma_rb(rbtree_t *tree, struct VmaStruct *vma, struct VmaStruct **vma_prevp) {
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

void insert_vma_struct(struct MmStruct *mm, struct VmaStruct *vma) {
    assert(vma->vm_start < vma->vm_end);
    list_entry_t *head = &(mm->mmap_link);
    list_entry_t *entry_prev = head;
    list_entry_t *entry_next = NULL;
    list_entry_t *entry = NULL;
    
    if (rbtree_root(&(mm->mmap_tree)) != rbtree_sentinel(&(mm->mmap_tree))) {
        struct VmaStruct *mmap_prev = NULL;
        insert_vma_rb(&(mm->mmap_tree), vma, &mmap_prev);
        if (mmap_prev != NULL) {
            entry_prev = &(mmap_prev->vma_link);
        }
    } else {
        list_entry_t *entry = head;
        while ((entry = list_next(entry)) != head) {
            struct VmaStruct *mmap_prev = le2vma(entry, vma_link);
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

static int remove_vma_struct(struct MmStruct *mm, struct VmaStruct *vma) {
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

void mm_destory(struct MmStruct *mm) {
    list_entry_t *head = &(mm->mmap_link);
    list_entry_t *entry = head;
    while ((entry = list_next(entry)) != head) {
        list_del(entry);
        kfree(le2vma(entry, vma_link));
    }
    kfree(mm);
}

static void vma_destory(struct VmaStruct *vma) {
    kfree(vma);
}

static void check_vmm(void);
static void check_vma_struct();
static void check_page_fault();

void vmm_init(void) {
    check_vmm();
}

// 从[addr, addr + len)建立一个vma映射
int mm_map(struct MmStruct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
         struct VmaStruct **vma_store) {
    uintptr_t start = ROUNDDOWN(addr, PAGE_SIZE);
    uintptr_t end = ROUNDUP(addr + len, PAGE_SIZE);
    if (!USER_ACCESS(start, end)) {
        return -E_INVAL;
    }

    assert (mm != NULL);

    int ret = -E_INVAL;
    struct VmaStruct *vma;
    if ((vma = find_vma(mm, start)) != NULL && end > vma->vm_start) {
        // 如果找到的地址范围[start， end)和存在vma地址范围重叠，则直接退出，返回失败
        goto out;
    }
    ret = -E_NO_MEM;
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

static void vma_resize(struct VmaStruct *vma, uintptr_t start, uintptr_t end) {
    assert(start % PAGE_SIZE == 0 && end % PAGE_SIZE == 0);
    // 调整vma大小时，只能缩小范围
    assert(vma->vm_start <= start && start < end && vma->vm_end >= end);
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

int mm_unmap(struct MmStruct *mm, uintptr_t addr, size_t len) {
    uintptr_t start = ROUNDDOWN(addr, PAGE_SIZE);
    uintptr_t end = ROUNDUP(start + len, PAGE_SIZE);
    if (!USER_ACCESS(start, end)) {
        return -E_INVAL;
    }
    assert(mm != NULL);

    struct VmaStruct *vma;
    if ((vma = find_vma(mm, start)) == NULL || end <= vma->vm_start) {
        // 没有找到addr对应的vma
        return 0;
    }
    // 如果[start, end)在vma的地址范围内，则将vma分成左边界和右边界两个vma
    if (vma->vm_start < start && end < vma->vm_end) {
        struct VmaStruct *left_vma;
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

    list_entry_t free_list, *entry;
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

    struct MmStruct *mm = mm_create();
    assert(mm != NULL);

    int step1 = RB_MIN_MAP_COUNT * 2;
    int step2 = step1 * 10;
    int i;
    for (i = step1; i >= 0; i --) {
        struct VmaStruct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    for (i = step1 + 1; i <= step2; i ++) {
        struct VmaStruct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    list_entry_t *entry = list_next(&(mm->mmap_link));

    for (i = 0; i <= step2; i++) {
        assert(entry != &(mm->mmap_link));
        struct VmaStruct *mmap = le2vma(entry, vma_link);
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
        entry = list_next(entry);
    }

    for (i = 0; i < 5 * step2 + 2; i++) {
        struct VmaStruct *vma = find_vma(mm, i);
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

struct MmStruct *check_mm_struct;

static void check_page_fault() {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    check_mm_struct = mm_create();
    assert(check_mm_struct != NULL);

    struct MmStruct *mm = check_mm_struct;
    pde_t *page_dir = mm->page_dir = get_boot_page_dir();
    assert(page_dir[0] == 0);

    struct VmaStruct *vma = vma_create(0, PT_SIZE, VM_WRITE);
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

int do_page_fault(struct MmStruct *mm, uint32_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;

    struct VmaStruct *vma = find_vma(mm, addr);
    // 找到的vma可能在addr的右邊
    if (vma == NULL || vma->vm_start > addr) {
        goto failed;
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
    // 用戶權限
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
        // 如果页表项为0，表示即不存在和page的映射，也不存在和swap的映射
        if (page_dir_alloc_page(mm->page_dir, addr, perm) == 0) {
            goto failed;
        }
    } else {
        // 页表项不为0，那么产生page fault的原因就可能是PTE_P没有置位，
        // 但是存在swap entry，即pte的值就是swap entry值，
        // 通过该swap entry找到page或者将数据换入进新的page
        struct Page *page = NULL;
        if ((ret = swap_in_page(*ptep, &page)) != 0) {
            goto failed;
        }
        page_insert(mm->page_dir, page, addr, perm);
        // 页表项pte不指向swap entry时，对应的swap entry引用计数减1
        swap_decrease(page->index);
    }

    ret = 0;

failed:
    return ret;
}

void print_vma(void) {
    if (check_mm_struct == NULL || check_mm_struct->map_count == 0) {
        return;
    }
    struct VmaStruct *vma = NULL;
    list_entry_t *head = &(check_mm_struct->mmap_link);
    list_entry_t *entry = list_next(head);
    while (entry != head) {
        vma = le2vma(entry, vma_link);
        printk("(start,end)=(%d,%d), flags=%08x \n", vma->vm_start, vma->vm_end, vma->vm_flags);
        entry = list_next(entry);
    }
    
}

