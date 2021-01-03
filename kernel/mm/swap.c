#include <pmm.h>
#include <vmm.h>
#include <swap.h>
#include <list.h>
#include <stdlib.h>
#include <slab.h>
#include <error.h>
#include <swapfs.h>
#include <string.h>

size_t max_swap_offset;

size_t swap_max_offset(void) {
    return max_swap_offset;
}

void swap_set_max_offset(size_t max_offset) {
    max_swap_offset = max_offset;
}

typedef struct {
    list_entry_t swap_link;
    size_t nr_pages;
} swap_list_t;

static swap_list_t active_list;
static swap_list_t inactive_list;

#define nr_active_pages     (active_list.nr_pages)
#define nr_inactive_pages   (inactive_list.nr_pages)

// 有多少个页表项引用swap entry，那么对应的swap entry的引用计数就是多少
static unsigned short *mem_map;

#define SWAP_UNUSED         0XFFFF
#define MAX_SWAP_REF        0XFFFE

static volatile bool swap_init_ok = 0;

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define entry_hashfn(x)     (hash32(x, HASH_SHIFT)) 

static list_entry_t hash_list[HASH_LIST_SIZE];

static lock_t swap_in_lock;

static void swap_list_init(swap_list_t *list) {
    list_init(&(list->swap_link));
    list->nr_pages = 0;
}

static inline void swap_active_list_add(struct Page *page) {
    assert(PageSwap(page));
    SetPageActive(page);
    swap_list_t *list = &active_list;
    list->nr_pages++;
    // 刚添加进来的page放在swap链表的末尾
    list_add_before(&(list->swap_link), &(page->swap_link));
}

static inline void swap_inactive_list_add(struct Page *page) {
    assert(PageSwap(page));
    ClearPageActive(page);
    swap_list_t *list = &inactive_list;
    list->nr_pages++;
    list_add_before(&(list->swap_link), &(page->swap_link));
}

static inline void swap_list_del(struct Page *page) {
    assert(PageSwap(page));
    (PageActive(page) ? &active_list : &inactive_list)->nr_pages--;
    list_del(&(page->swap_link));
}
void check_swap(void);

void swap_init(void) {
    swapfs_init();
    swap_list_init(&active_list);
    swap_list_init(&inactive_list);

    // todo: 为什么时1024, 是不是swap的section数量不能太少
    if (!(1024 <= max_swap_offset && max_swap_offset < MAX_SWAP_OFFSET_LIMIT)) {
        panic("bad max_swap_offset %08x.\n", max_swap_offset);
    }

    mem_map = kmalloc(sizeof(short) * max_swap_offset);
    assert(mem_map != NULL);

    size_t offset;
    for (offset = 0; offset < max_swap_offset; offset++) {
        mem_map[offset] = SWAP_UNUSED;
    }
    int i;
    for (i = 0; i < HASH_LIST_SIZE; i++) {
        list_init(hash_list + i);
    }
    lock_init(&swap_in_lock);

    check_swap();
}

bool try_free_pages(size_t n) {
    if (!swap_init_ok) {
        return 0;
    }
    panic("not implemented yet.!\n");
}

static swap_entry_t try_alloc_swap_entry(void);

static bool swap_page_add(struct Page *page, swap_entry_t entry) {
    // page在加入swap之前没有设置swap标志
    assert(!PageSwap(page));

    if (entry == 0) {
        // 为新添加进swap管理框架的page申请一个entry,这个entry和page构成了一一映射
        // 也就page和swap frame的映射关系
        if ((entry = try_alloc_swap_entry()) == 0) {
            return false;
        }
        // mem_map[swap_offset(entry)]表示swap分区中entry索引的swapframe是否被使用，以及是否有内存的拷贝
        // （大于0表示有拷贝，0表示没有）
        assert(mem_map[swap_offset(entry)] == SWAP_UNUSED);
        // entry索引处的值为0，表示swap 分区中没有内存中数据的拷贝
        mem_map[swap_offset(entry)] = 0;
        // 为什么要设置page的dirty位？？？
        // todo:
        SetPageDirty(page);
    }
    // page对应的swap frame映射已经存在了，设置相应的映射参数
    SetPageSwap(page);
    // 该page被存放在swap分区的哪个索引处
    page->index = entry;
    list_add(hash_list + entry_hashfn(entry), &(page->page_link));
    return true;
}

static void swap_page_del(struct Page *page) {
    assert(PageSwap(page));
    ClearPageSwap(page);
    list_del(&(page->page_link));
}

static void swap_free_page(struct Page *page) {
    // page是可回收的，并且当前没有对该page的引用
    assert(PageSwap(page) && page_ref(page) == 0);
    // 从链表中将page摘下，swap_list是使用swap_link进行链接的，而此处是从page_link这个链表摘下
    // 疑问：这个链表是什么链表？？
    // 答：这个page使用page_link挂载在hash_list上了
    // 疑问：hash_list的作用是什么？
    // 答：用于根据swap的entry索引号快速查找到映射的page
    swap_page_del(page);
    // 该页没有任何用处了，将其归还给buddy system
    free_page(page);
}

// 根据entry，从hash list中找到对应的page
static struct Page *swap_hash_find(swap_entry_t entry) {
    list_entry_t *head = hash_list + entry_hashfn(entry);
    list_entry_t *le = head;
    while ((le = list_next(le)) != head) {
        struct Page *page = le2page(le, page_link);
        if (page->index == entry) {
            return page;
        }
    }
    return NULL;
}

static swap_entry_t try_alloc_swap_entry(void) {
    static size_t next = 1;
    size_t empty = 0;
    size_t zero = 0;
    size_t end = next;
    do {
        switch (mem_map[next]) {
        case SWAP_UNUSED:
            // 记录下为SWAP_UNUSED的索引
            empty = next;
            break;
        case 0:
            if (zero == 0) {
                // 记录下第一个为0的索引
                zero = next;
            }
            break;
        default:
            break;
        }
        if (++next == max_swap_offset) {
            next = 1;
        }
    } while (empty == 0 && next != end);

    swap_entry_t entry = 0;
    if (empty != 0) {
        // empty标识为SWAP_UNUSED的索引
        entry = (empty << 8);
    } else if (zero != 0) {
        // 找了一圈都没有找到SWAP_UNUSED的索引
        // zero标识查找时第一个为0的索引
        entry = (zero << 8);
        // entry引用为0，说明swap分区上没有page的副本，但此时swap entry和page的映射关系是存在的
        struct Page *page = swap_hash_find(entry);
        // 在swap的hash_list中能找到page，这个page被链接到swap_list了
        assert(page != NULL && PageSwap(page));
        // 由于没有可用的swap entry了，将entry引用计数为0的映射关系拆除用于建立新的映射
        // （即清除page的swap标志，从链表中将page取下，最后可能的话释放page）
        // page 在inactive或active链表上
        swap_list_del(page);
        if (page_ref(page) == 0) {
            // 从hash_list上将page摘下来，并将page归还给buddy system
            swap_free_page(page);
        } else {
            // page的引用计数不为0，仅将page从hash_list上摘除下来
            swap_page_del(page);
        }
        // 对应的page已经释放，此时再将swap entry设置为未使用状态
        mem_map[zero] = SWAP_UNUSED;
    }
    static unsigned int failed_counter = 0;
    // entry == 0标识既没有SWAP_UNUSED的页面，也没有标识为0的页面，
    // 也就是此时没有可以被释放的页面，因此释放page会失败
    if (entry == 0 && ((++failed_counter) % 0x1000) == 0) {
        warn("swap: try_alloc_swap_entry: failed too many times.\n");
    }
    return entry;
}

// 将entry引用计数减1，如果存在映射关系，并且page的引用计数为0，则拆除这个映射关系，最后释放映射的page
void swap_remove_entry(swap_entry_t entry) {
    size_t offset = swap_offset(entry);
    // 此时swap分区的offset处的swap frame有内存page的副本
    assert(mem_map[offset] > 0);
    if (--mem_map[offset] == 0) {
        // swap 分区中对应的索引offset处的frame没有page的拷贝，
        // 可以将这个page释放掉（如果可以释放的话）
        struct Page *page = swap_hash_find(entry);
        if (page != NULL) {
            if (page_ref(page) != 0) {
                return;
            }
            // 从active或inactive中摘除page
            swap_list_del(page);
            // 从hash_list链表中摘除page，并释放page
            swap_free_page(page);
        }
        // swap分区的offset处的swap frame没有使用了
        mem_map[offset] = SWAP_UNUSED;
    }
}

int swap_page_count(struct Page *page) {
    if (!PageSwap(page)) {
        return 0;
    }
    size_t offset = swap_offset(page->index);
    assert(mem_map[offset] >= 0);
    return mem_map[offset];
}

void swap_decrease(swap_entry_t entry) {
    size_t offset = swap_offset(entry);
    assert(mem_map[offset] > 0 && mem_map[offset] <= MAX_SWAP_REF);
    // 当解除页表项pte指向swap entry时，该swap entry的引用计数就减1
    mem_map[offset]--;
}

void swap_duplicate(swap_entry_t entry) {
    size_t offset = swap_offset(entry);
    assert(mem_map[offset] >= 0 && mem_map[offset] < MAX_SWAP_REF);
    // 页表项pte指向swap entry时，该swap entry的引用计数就加1
    mem_map[offset]++;
}

// 如果swap entry映射的page没有被释放（在hash list上通过entry查找的到page），则直接返回这个page；
// 如果在hash list找不到page，则申请一个新的page，然后通过这个swap entry将swap分区中的数据换入到这个新的page中
// 然后将这个新的page加入swap管理框架，放入active swap list，最后将这个page返回
// return: 0表示成功，其他值为失败
int swap_in_page(swap_entry_t entry, struct Page **pagep) {
    if (pagep == NULL) {
        return -E_INVAL;
    }
    size_t offset = swap_offset(entry);

    // todo :为什么是大于等于0
    assert(mem_map[offset] >= 0);

    int ret;
    struct Page *page, *new_page;
    // 这个page可能被释放掉了，需要重新申请一个新的page来存放swap frame
    if ((page = swap_hash_find(entry)) != NULL) {
        goto found;
    }

    new_page = alloc_page();
    lock(&swap_in_lock);
    // 查找到加锁的这段时间内，可能这个page又被放入hash_list了
    if ((page = swap_hash_find(entry)) != NULL) {
        if (new_page != NULL) {
            free_page(new_page);
        }
        goto found_unlock;
    }
    if (new_page == NULL) {
        // 申请新页面失败，此时已没有足够的内存了
        ret = -E_NO_MEM;
        goto failed_unlock;
    }
    page = new_page;
    // 将entry的内容读入到page中，返回非0表示读取失败
    if (swapfs_read(entry, page) != 0) {
        free_page(page);
        ret = -E_SWAP_FAULT;
        goto failed_unlock;
    }
    // page存在swap的映射内容，将page添加到swap管理框架中区
    swap_page_add(page, entry);
    // page刚刚才被访问，将其放入swap的active链表
    swap_active_list_add(page);
found_unlock:
    unlock(&swap_in_lock);

found:
    *pagep = page;
    return 0;

failed_unlock:
    unlock(&swap_in_lock);
    return ret;
}

// 将一个swap out page的内容复制到一个新的page中
// 这个新的page需要设置PG_swap标志，并且加入到swap active list
int swap_copy_entry(swap_entry_t entry, swap_entry_t *store) {
    if (store == NULL) {
        return -E_INVAL;
    }

    int ret = -E_NO_MEM;
    struct Page *page = NULL;
    struct Page *new_page = NULL;
    // 将swap的entry项的引用计数加1，避免以下的操作过程中swap frame被释放掉了
    swap_duplicate(entry);

    if ((new_page = alloc_page()) == NULL) {
        goto failed;
    }
    // 将entry换入，如果本身存在于内存就不用换入了
    if ((ret = swap_in_page(entry, &page)) != 0) {
        goto failed_free_page;
    }

    ret = -E_NO_MEM;
    if (!swap_page_add(new_page, 0)) {
        goto failed_free_page;
    }
    // 将新alloc的page加入swap active list
    swap_active_list_add(new_page);
    memcpy(page2kva(new_page), page2kva(page), PAGE_SIZE);
    *store = new_page->index;
    ret = 0;
out: 
    swap_remove_entry(entry);
    return ret;

failed_free_page:
    free_page(new_page);
failed:
    goto out;
}

static bool try_free_swap_entry(swap_entry_t entry) {
    size_t offset = swap_offset(entry);
    if (mem_map[offset] == 0) {
        // swap分区的offset处的swap frame可以被释放了
        mem_map[offset] = SWAP_UNUSED;
        return true;
    }
    return false;
}

// 将swap inactive list中的page换出到swap分区中去（调用swapfs_write函数实现），实现方式如下：
// 1、当page的引用计数不为0时，将这个page放入active swap list
// 2、当page的引用计数为0时，并且swap entry的引用不为0，如果为dirty page，则将这个page的内容写入到swap
//    分区后释放page，否则直接释放这个page（此时的page引用计数一定是0，不为0的page被放入active swap list了）。
//    最后的状态就变成了swap entry不为0（和对应pte的值相等），但对应的page在hash list找不到了
// 3、当page的引用计数为0时，并且swap entry的引用计数为0，则释放这个swap entry,并且释放这个page（page的引用计数是0，可以被释放），即swap entry 和 page都被释放了
// 
static int page_launder(void) {

    size_t max_scan = nr_inactive_pages;
    size_t free_count = 0;
    list_entry_t *head = &(inactive_list.swap_link);
    list_entry_t *entry = list_next(head);
    while (max_scan-- > 0 && entry != head) {
        struct Page *page = le2page(entry, swap_link);
        entry = list_next(entry);
        if (!(PageSwap(page) && !PageActive(page))) {
            panic("inactive: wrong swap list.\n");
        }
        swap_list_del(page);
        if (page_ref(page) != 0) {
            // page的引用计数不为0，将其挂接在swap active list
            swap_active_list_add(page);
            continue;
        }
        swap_entry_t entry = page->index;
        // 如果page在swap分区没有副本，则直接将page释放（这个函数只释放swap的entry对应的swap frame）
        // 如果page在swap分区有副本，则将page写入swap分区
        if (!try_free_swap_entry(entry)) {
            // page在swap分区有副本，不能释放这个swap的entry
            if (PageDirty(page)) {
                // page的内容已经被修改，则需要写入swap分区
                ClearPageDirty(page);
                // 开始复制page内容到swap分区，现将swap分区的entry索引的frame引用加1（以防被释放）
                // 该函数仅仅将entry的引用计数加1
                swap_duplicate(entry);
                if (swapfs_write(entry, page) != 0) {
                    // 写入swap分区失败
                    SetPageDirty(page);
                }
                // page复制内容到swap分区结束，将对swap的entry引用计数减1
                mem_map[swap_offset(entry)]--;

                if (page_ref(page) != 0) {
                    // page的引用计数不为0，也就是暂时不能被释放，将其添加到swap active list
                    swap_active_list_add(page);
                    continue;
                }
                if (PageDirty(page)) {
                    // page写入swap分区失败，继续留在swap inactive list
                    swap_inactive_list_add(page);
                    continue;
                }
                // 试着释放swap entry
                // 为什么要释放？
                // todo: 疑问：如果我们将page换出到swap分区了，再把这个swap entry释放，岂不是刚写入的swap分区内容就释放了？？
                try_free_swap_entry(entry);
            }
        }
        free_count++;
        // page(dirty page)的内容写到swap分区了，或者page没有对应的swap分区映射，亦或是这是一个非diry page，那么这个page可以被释放了
        swap_free_page(page);
    }

    return free_count;
}

// 尝试着将swap active list中的page放入swap inactive list
// 当page的引用计数为0时，将page放入swap inactive list
static void refill_inactive_scan(void) {
    size_t max_scan = nr_active_pages;
    list_entry_t *head = &(active_list.swap_link);
    list_entry_t *entry = list_next(head);
    while (max_scan-- > 0 && entry != head) {
        struct Page *page = le2page(entry, swap_link);
        entry = list_next(entry);
        if (!(PageSwap(page) && PageActive(page))) {
            panic("active: wrong swap list.\n");
        }
        if (page_ref(page) == 0) {
            swap_list_del(page);
            swap_inactive_list_add(page);
        }
    }
}

// 尝试着解除pte的映射，并且将这些解除映射的page加入swap active list
// 释放从addr 到 vm->end间映射的page，最多释放require个page
static int swap_out_vma(struct MmStruct *mm, struct VmaStruct *vma, uintptr_t addr, size_t require) {
    if (require == 0 || !(addr >= vma->vm_start && addr < vma->vm_end)) {
        // addr 不在vma的地址范围，不能释放这个vma的page
        // todo: require == 0为何意？
        return 0;
    }
    uintptr_t end;
    size_t free_count = 0;
    addr = ROUNDDOWN(addr, PAGE_SIZE);
    end = ROUNDUP(vma->vm_end, PAGE_SIZE);
    while (addr < end && require != 0) {
        pte_t *ptep = get_pte(mm->page_dir, addr, 0);
        if (ptep == NULL) {
            // 对应的页表不存在
            addr = ROUNDDOWN(addr + PAGE_SIZE, PAGE_SIZE);
            continue;
        }
        if (*ptep & PTE_P) {
            struct Page *page = pte2page(*ptep);
            // 用户态地址申请的页都不是保留页
            // 内核态地址映射的页都是保留页
            assert(!PageReserved(page));

            // 时钟（clock）页置换算法：
            // 第一遍将如果找到置位了PTE_A的pte，则将PTE_A位清除，然后查找下一个pte
            if (*ptep & PTE_A) {
                // page最近被访问了，不能将该页换出，此时将访问标志清零
                *ptep &= ~PTE_A;
                tlb_invalidate(mm->page_dir, addr);
                goto try_next_entry;
            }
            // PTE_A没有置位
            if (!PageSwap(page)) {
                // page还没有放入swap管理框架中（也就是目前没有该page的pte指向swap entry），给page申请一个swap分区的映射，
                // 然后将page放入swap的hash_list中
                if (!swap_page_add(page, 0)) {
                    // 放入swap管理框架失败，主要是申请不到swap的索引了（可能是swap分区的空间都被用掉了）
                    goto try_next_entry;
                }
                // 上面给page申请了swap 的entry，并且放入了hash_list，接下来将page放入swap active list
                swap_active_list_add(page);
            } else if (*ptep & PTE_D) {
                // 当前page已经被放入了swap管理框架,
                // 当pte的dirty被设置后，将对应page的dirty标志置位
                // todo: 疑问：这个page都被放到swap管理框架了，怎么还会被修改？当放入swap管理框架中时，对应的pte设置为swap的entry了，此时没法访问这个page才对啊？
                // 回答：可能这个page被引用多次，加入swap时就多次加入，第一次加入后，page可以通过其他的pte进行访问
                SetPageDirty(page);
            }
            swap_entry_t entry = page->index;
            // 为什么要给entry的引用加1？
            swap_duplicate(entry);
            // page被驱逐到swap管理框架了，少了一个pte引用该page，因此将page的引用计数减1
            page_ref_dec(page);
            *ptep = entry;
            tlb_invalidate(mm->page_dir, addr);
            // 设置下一次swap扫描的起始地址
            mm->swap_address = addr + PAGE_SIZE;
            free_count++;
            require--;
        }
try_next_entry:
        addr += PAGE_SIZE;
    }
    return free_count;
}

// 尝试着调用swap_out_vma将所有vma的pte映射拆除
// require表示需要拆除映射的page的个数
static int swap_out_mm(struct MmStruct *mm, size_t require) {
    assert(mm != NULL);

    if (require == 0 || mm->map_count == 0) {
        return 0;
    }
    assert(!list_empty(&(mm->mmap_link)));

    // 下一个swap的起始地址
    uintptr_t addr = mm->swap_address;
    struct VmaStruct *vma;
    if ((vma = find_vma(mm, addr)) == NULL) {
        // 从addr这个地址开始查找vma，如果找不到vma则从addr = 0的这个地址开始查找vma
        addr = mm->swap_address = 0;
        vma = le2vma(list_next(&(mm->mmap_link)), vma_link);
    }

    // addr应该是小于vma->vm_end
    assert(vma != NULL && addr < vma->vm_end);

    if (addr < vma->vm_start) {
        addr = vma->vm_start;
    }

    int i;
    size_t free_count = 0;
    for (i = 0; i <= mm->map_count; i++) {
        int ret = swap_out_vma(mm, vma, addr, require);
        free_count += ret;
        require -= ret;
        if (require == 0) {
            break;
        }
        list_entry_t *entry = list_next(&(vma->vma_link));
        if (entry == &(mm->mmap_link)) {
            entry = list_next(entry);
        }
        vma = le2vma(entry, vma_link);
        addr = vma->vm_start;
    }

    return free_count;
}

void check_swap(void) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    size_t offset;
    // offset = 2开始的所有swap entry都被使用了
    for (offset = 2; offset < max_swap_offset; offset ++) {
        mem_map[offset] = 1;
    }

    struct MmStruct *mm = mm_create();
    assert(mm != NULL);

    extern struct MmStruct *check_mm_struct;
    assert(check_mm_struct == NULL);

    check_mm_struct = mm;

    pde_t *pgdir = mm->page_dir = get_boot_page_dir();
    assert(pgdir[0] == 0);

    // 创建一个4M大小的vma
    struct VmaStruct *vma = vma_create(0, PT_SIZE, VM_WRITE | VM_READ);
    assert(vma != NULL);

    // 将vma插入mm
    insert_vma_struct(mm, vma);

    // 申请两个page rp0和rp1
    struct Page *rp0 = alloc_page(), *rp1 = alloc_page();
    assert(rp0 != NULL && rp1 != NULL);


    uint32_t perm = PTE_U | PTE_W;
    // 在设置页表的0地址页框，该指向的页为rp1，因此rp1的引用计数为1
    int ret = page_insert(pgdir, rp1, 0, perm);
    assert(ret == 0 && page_ref(rp1) == 1);

    // 增加rp1的引用计数，此时rp1的引用计数为2
    page_ref_inc(rp1); 
    // 将rp0替换rp1的页框，rp1减1后引用计数为1，rp0增加引用计数后为1; rp0用于页框，rp1悬空未被引用
    ret = page_insert(pgdir, rp0, 0, perm);
    assert(ret == 0 && page_ref(rp1) == 1 && page_ref(rp0) == 1);

    // check try_alloc_swap_entry

    // 当前只有索引为1的entry未使用，申请到的entry必定等于1 
    swap_entry_t entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1);
    // 将entry为1的引用计数设置为1，表示entry为1的索引也不可用了，
    mem_map[1] = 1;
    // 当前已经没有可用的entry了，只能返回0了（0表示没有可用的swap entry索引）
    assert(try_alloc_swap_entry() == 0);

    // set rp1, Swap, Active, add to hash_list, active_list
    // 将rp1和entry = 1建立映射关系，将rp1加入hash_list，设置page的swap标志，page的index指向entry；
    // rp0位0地址页框，rp1和swap建立映射关系
    swap_page_add(rp1, entry);
    // 将rp1加入swap active list
    swap_active_list_add(rp1);
    // rp1的swap标志必然设置了
    assert(PageSwap(rp1));

    // 此时磁盘不再有page的副本，但时page和swap的entry映射关系还在（在page被swap out时，可直接丢弃page）
    mem_map[1] = 0;
    // 将swap entry 1 和rp1的映射关系拆除，此时rp1的引用计数为1，不会被释放;
    // rp0位0地址页框，rp1悬空
    entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1);
    // 和swap的映射关系已经被拆除，那么swap标志也被清除了
    assert(!PageSwap(rp1));

    // check swap_remove_entry

    // entry:1 和page的映射关系已经拆除，还没有建立新的映射关系，
    assert(swap_hash_find(entry) == NULL);
    mem_map[1] = 2;
    // 由于entry:1的引用计数为2，此时swap_remove_entry只是将计数减1，
    // 不会拆除映射关系（此时本来就没有和page的映射关系，这种情况）,
    // 一般情况下，如果swap entry的引用计数不为0，那么就一定会和page存在映射关系，
    // 例外情况是：我们人为的设置entry的引用，而没有建立映射关系（正常的程序流程不会这么做）
    swap_remove_entry(entry);
    assert(mem_map[1] == 1);

    // 建立rp1很entry的映射关系
    // rp0为0地址页框，rp1和swap建立映射关系
    swap_page_add(rp1, entry);
    swap_inactive_list_add(rp1);
    // 此时entry的引用计数为1，并且存在和page的映射，rp1的引用为1
    // 那么下面函数执行后，entry的引用计数变成了0，rp1的引用计数依然时1，
    // 因此映射关系不会被释放，暂时保留，可能后面还会使用这个映射关系
    swap_remove_entry(entry);
    // 映射还在，那么swap标志就会在
    assert(PageSwap(rp1));
    // rp1和entry:1的映射还在，但是entry:1的引用已经为0了，在后面swap entry不够用时，可以将这个映射关系拆除
    assert(rp1->index == entry && mem_map[1] == 0);

    // check page_launder, move page from inactive_list to active_list

    // rp1的引用依然是1
    assert(page_ref(rp1) == 1);
    // rp1和swap的映射存在，并且rp1在inactive链表上
    assert(nr_active_pages == 0 && nr_inactive_pages == 1);
    assert(list_next(&(inactive_list.swap_link)) == &(rp1->swap_link));

    // 将active swap list的page移入inactive swap list，并将inactive swap list的page内容拷贝到swap分区，
    // 然后释放page（page引用计数为0的话）；如果page的引用计数不为0，则将这个page移入active swap list
    page_launder();
    assert(nr_active_pages == 1 && nr_inactive_pages == 0);
    assert(PageSwap(rp1) && PageActive(rp1));

    // 此时没有可用的swap entry，并且entry:1这个引用计数为0，但是和rp1映射关系存在；
    // 因此将entry:1和rp1的映射关系拆除（此时page的引用计数不为0，不需要将page内容写入swap分区），释放出entry:1，
    // rp0为0地址页框，rp1悬空
    entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1);
    assert(!PageSwap(rp1) && nr_active_pages == 0);
    assert(list_empty(&(active_list.swap_link)));

    // set rp1 inactive again
    // rp1的引用计数保持为1
    assert(page_ref(rp1) == 1);
    // 给rp1建立一个新的和swap的映射关系，此时swap entry:1可用
    // rp0为0地址页框，rp1和swap建立映射关系
    swap_page_add(rp1, 0);
    assert(PageSwap(rp1) && swap_offset(rp1->index) == 1);
    swap_inactive_list_add(rp1);
    mem_map[1] = 1;
    assert(nr_inactive_pages == 1);
    // rp1的引用计数为0了
    page_ref_dec(rp1);

    size_t count = nr_free_pages();
    // 将entry:1的引用计数减1，如果entry:1的引用计数为0，并且rp1的引用也为0，
    // 则拆除swap entry:1和rp1的映射关系，最后释放rp1
    // rp0为0地址页框，rp1被释放，swap entry:1被释放
    swap_remove_entry(entry);
    // rp1被释放到buddy system了，所以free pages 多1了
    assert(nr_inactive_pages == 0 && nr_free_pages() == count + 1);

    // check swap_out_mm
    
    // rp0用于0地址页框，rp1被释放，此时ptep0指向rp0
    pte_t *ptep0 = get_pte(pgdir, 0, 0), *ptep1;
    assert(ptep0 != NULL && pte2page(*ptep0) == rp0);

    // require为0，不驱逐任何page
    ret = swap_out_mm(mm, 0);
    assert(ret == 0);

    // 目前只有一个page用于虚拟地址的映射，那就是rp0；rp0被驱逐后，rp0放入swap框里框架的active swap list，
    // 下一个驱逐的起始地址从PAGE_SIZE开始，刚刚驱逐的rp0和swap entry:1做映射，rp0的index == entry:1
    ret = swap_out_mm(mm, 10);
    assert(ret == 1 && mm->swap_address == PAGE_SIZE);

    // vma没有使用的物理内存了，因此驱逐0个page
    ret = swap_out_mm(mm, 10);
    // 页表的0地址页表项的内容为swap的索引entry:1
    // 当在发生page fault时，根据pte中保存的entry:1在swap框架中查找对应的page，
    // 如果entry:1对应的page没有释放，则将page和该pte建立映射，
    // 如果page被释放，则申请一个新的page，然后从swap分区中将entry:1的内容拷贝到新page，接着pte和新page建立映射
    assert(ret == 0 && *ptep0 == entry && mem_map[1] == 1);
    // 当前rp0放在active swap list，引用计数为减为1，并且page在放入swap管理框架时被设置为dirty page
    // rp0和swap entry:1建立映射，在active swap list链表，rp1被释放
    assert(PageDirty(rp0) && PageActive(rp0) && page_ref(rp0) == 0);
    assert(nr_active_pages == 1 && list_next(&(active_list.swap_link)) == &(rp0->swap_link));

    // check refill_inactive_scan()
    // rp0倍放入inactive swap list，引用计数为0，rp1被释放
    refill_inactive_scan();
    assert(!PageActive(rp0) && page_ref(rp0) == 0);
    assert(nr_inactive_pages == 1 && list_next(&(inactive_list.swap_link)) == &(rp0->swap_link));
    // 增加rp0的引用计数，此时rp0的引用计数为1，
    page_ref_inc(rp0);
    // 由于rp0的引用计数为1，并且在inactive swap list，page_launder会将这个rp0放入active swap list，而不是释放这个page
    page_launder();
    assert(PageActive(rp0) && page_ref(rp0) == 1);
    assert(nr_active_pages == 1 && list_next(&(active_list.swap_link)) == &(rp0->swap_link));
    // rp0引用计数减为0，
    page_ref_dec(rp0);
    // refill_inactive_scan将扫描active swap list链表，如果page引用计数为0，则将该page放入inactive swap list，否则不动；
    // 当前rp0引用计数为0，所以被放入inactive swap list
    refill_inactive_scan();
    assert(!PageActive(rp0));

    // save data in rp0

    int i;
    for (i = 0; i < PAGE_SIZE; i ++) {
        // 向rp0的这个page中写入数据（这个写的地址是内核虚拟地址，不会产生page fault）
        ((char *)page2kva(rp0))[i] = (char)i;
    }
    // 由于rp0在inactive swap list，并且引用计数为0，rp0对应的swap entry:1的引用计数为1
    // 因此rp0会被释放（rp0的内容写入了swap分区），而swap entry:1不会释放，同时地址0的pte指向swap entry:1
    page_launder();
    assert(nr_inactive_pages == 0 && list_empty(&(inactive_list.swap_link)));
    // 这个swap entry:1 引用计数还是1
    assert(mem_map[1] == 1);

    // 此时rp0已经被释放，rp1有一个新的页，但是没有映射，也没有放入swap管理框架
    rp1 = alloc_page();
    assert(rp1 != NULL);
    // 将swap entry:1的内容从swap 分区读取到rp1中，此时rp1和之前rp0（rp0当前已经被释放掉了）的内容相同
    ret = swapfs_read(entry, rp1);
    assert(ret == 0);

    // 从swap分区中换入之前rp0的数据到rp1,所以此时rp1的数据和之前rp0的数据相等
    for (i = 0; i < PAGE_SIZE; i ++) {
        assert(((char *)page2kva(rp1))[i] == (char)i);
    }

    // page fault now
    // 0好地址对应的页表项pte指向swap entry:1(pte对应的页表刚刚被换出了)，
    // 所以这个pte的PTE_P位为0，所以会产生page fault，在page fault中将0地址的pte设置成正确值
    // 也就是通过pte在swap 框架中找到映射的page，但此时page是找不到的，已经被释放，我们就需要从swap分区中
    // 将swap entry:1的内容换入到一个新的page，然后pte指向这个新的page
    *(char *)0 = 0xEF;

    // rp0指向新的页，这个页的数据从swap分区中换入进来，并且这个page被放入在active swap list中
    rp0 = pte2page(*ptep0);
    assert(page_ref(rp0) == 1);
    assert(PageSwap(rp0) && PageActive(rp0));

    // 当前swap entry:1的引用计数为0，其他的都被使用了，因此会将entry:1腾出来，
    // 即rp0和swap entry:1的映射关系被拆除，同时rp0的引用计数为1，不会被释放
    entry = try_alloc_swap_entry();

    assert(swap_offset(entry) == 1 && mem_map[1] == SWAP_UNUSED);
    assert(!PageSwap(rp0) && nr_active_pages == 0 && nr_inactive_pages == 0);

    // clear accessed flag

    assert(rp0 == pte2page(*ptep0));
    // rp0已经从swap框架中移除了
    assert(!PageSwap(rp0));

    // 当前vma只有一个rp0，并且这个rp0刚刚被访问了，此时PTE_A置位了
    // 所以对应的pte PTE_A标志被清除，并且rp0不会被驱逐
    ret = swap_out_mm(mm, 10);
    assert(ret == 0);
    // rp0用作0地址页表项的物理页，并且没有放入swap管理框架中
    assert(!PageSwap(rp0) && (*ptep0 & PTE_P));

    // change page table
    // rp0的 PTE_A标志没有置位了，因此可以被驱逐
    // 被驱逐后，page的引用计数为0，并且ptep0指向swap entry:1，即mem_map[1]计数为1
    ret = swap_out_mm(mm, 10);
    assert(ret == 1);
    
    assert(*ptep0 == entry && page_ref(rp0) == 0 && mem_map[1] == 1);

    count = nr_free_pages();
    // rp0的引用计数为0，因此从active swap list移到inactive swap list
    refill_inactive_scan();
    // 接着将inactive的引用计数为0的page页swap out，引用计数不为0的page放入active swap list
    // rp0的引用计数为0，因此rp0被换出，并且被释放了
    page_launder();
    assert(count + 1 == nr_free_pages());

    // 当前状态是rp0已经被释放，rp1有一个新的页，但是没有映射，也没有放入swap管理框架
    // 如下的函数将swap entry:1的内容从swap分区换入到rp1
    ret = swapfs_read(entry, rp1);
    // rp1的第一个字节为0xEF(这是上面 '*(char *)0 = 0xEF;' 这行代码写入的)
    assert(ret == 0 && *(char *)(page2kva(rp1)) == (char)0xEF);
    free_page(rp1);
    // 当前状态是rp0、rp1都被释放

    // duplictate *ptep0


    // swap entry:1被驱逐了，因此引用计数为1
    assert(mem_map[1] == 1);
    // ptep1为指向PAGE_SIZE地址的页表项（这个页表项没有任何值）
    ptep1 = get_pte(pgdir, PAGE_SIZE, 0);
    assert(ptep1 != NULL && *ptep1 == 0);
    // *ptep0指向的page已经被驱逐了，下面再此将*ptep0的swap entry引用计数加1是为了让其他的pte引用这个swap entry:1
    swap_duplicate(*ptep0);
    // ptep1也指向ptep0的swap entry（entry:1）
    // 也就是说多个pte引用同一个swap entry，则这个swap entry的引用计数会增加
    *ptep1 = *ptep0;

    // page fault again
    // 0地址会产生一次page fault，并将swap entry:1的内容加载进一个新的page,
    // 并建立swap entry:1和这个新page的映射关系，然后这个page和0地址的页表项建立映射
    // 当page被换入后，swap entry:1的引用计数减1
    *(char *)0 = 0xFF;
    // PAGE_SIZE + 1地址再一次产生page fault，在swap框架中搜索swap entry:1映射的page（是可以找到的，刚刚建立的映射），
    // 找到这个page后，和PAGE_SIZE地址的页表项建立映射关系，
    // 此时0地址的页表项和PAGE_SIZE地址的页表项指向同一个page，
    // 此时swap entry:1的引用计数再减1，最后的引用计数就是0
    *(char *)(PAGE_SIZE + 1) = 0x88;
    assert(pte2page(*ptep0) == pte2page(*ptep1));
    rp0 = pte2page(*ptep0);
    assert(*(char *)1 == (char)0x88 && *(char *)PAGE_SIZE == (char)0xFF);

    // 没有pte指向swap entry:1，因此mem_map[1]的值为0
    assert(page_ref(rp0) == 2 && rp0->index == entry && mem_map[1] == 0);

    // mem_map[1]的值为0，说明rp0还存在和swap的映射关系
    assert(PageSwap(rp0) && PageActive(rp0));
    // 此时swap entry:1的引用计数为0，可以使用，
    // 因此将rp0和swap entry:1的映射关系拆除，以腾出swap entry:1
    entry = try_alloc_swap_entry();
    assert(swap_offset(entry) == 1 && mem_map[1] == SWAP_UNUSED);
    // rp0和swap没有映射关系了
    assert(!PageSwap(rp0));

    // 当前不存在page和swap entry的映射关系了
    assert(list_empty(&(active_list.swap_link)));
    assert(list_empty(&(inactive_list.swap_link)));

    // check swap_out_mm

    *(char *)0 = *(char *)PAGE_SIZE = 0xEE;
    mm->swap_address = PAGE_SIZE * 2;
    // 0地址和PAGE_SIZE地址的页表项pte刚刚都被访问了，因此映射的page不会被驱逐，但是pte相应的访问位会被清除
    ret = swap_out_mm(mm, 2);
    assert(ret == 0);
    assert((*ptep0 & PTE_P) && !(*ptep0 & PTE_A));
    assert((*ptep1 & PTE_P) && !(*ptep1 & PTE_A));
    // 两个页表项pte的访问位都被清除了，因此映射的page可以被驱逐出去
    ret = swap_out_mm(mm, 2);
    assert(ret == 2);
    // 有两个pte项指向swap entry:1，因此mem_map[1]计数值为2
    assert(mem_map[1] == 2 && page_ref(rp0) == 0);

    // 将active swap list中的page移到inactive swaplist
    refill_inactive_scan();
    // 将inactive swap list的这个page页swap out到swap 分区，并且释放这个page页
    page_launder();
    assert(mem_map[1] == 2 && swap_hash_find(entry) == NULL);

    // check copy entry
    // 将swap entry:1的引用计数减1，如果引用计数减1后不为0，直接返回
    // 将swap entry:1的引用计数减1，如果引用计数减1后为0后，查找映射的page：
    // 1、如果查找不到page，则直接将swap entry:1的引用值设置为SWAP_UNUSED,表示该swap entry:1可被重新分配
    // 2、如果能够查找的到page，则将page和swap entry:1的映射关系拆除，然后将swap entry:1的引用值设置为SWAP_UNUSED
    swap_remove_entry(entry);
    // ptep1不再指向swap entry:1，因此上面这行代码将swap entry:1引用计数减1
    *ptep1 = 0;
    assert(mem_map[1] == 1);

    swap_entry_t store;
    // 将swap entry:1映射的赋值一份给store，赋值之前需要给store分配一个swap entry,
    // 但此时我们是分配不到可用的swap entry了，因此返回-E_NO_MEM
    ret = swap_copy_entry(entry, &store);
    assert(ret == -E_NO_MEM);
    // 释放处swap entry:2，此时可以申请这个swap entry了
    mem_map[2] = SWAP_UNUSED;
    // 下面函数执行后，store为2，entry为1，
    // 并且entry和store映射的page不同，但是page的内容相同
    ret = swap_copy_entry(entry, &store);
    assert(ret == 0 && swap_offset(store) == 2 && mem_map[2] == 0);
    mem_map[2] = 1;
    *ptep1 = store;

    // 地址PAGE_SIZE处的page和地址0处的page不是相同的page，但是page内容是相同的
    assert(*(char *)PAGE_SIZE == (char)0xEE && *(char *)(PAGE_SIZE + 1)== (char)0x88);

    // 对地址PAGE_SIZE的page修改不会影响地址为0的page的内容
    *(char *)PAGE_SIZE = 1, *(char *)(PAGE_SIZE + 1) = 2;
    assert(*(char *)0 == (char)0xEE && *(char *)1 == (char)0x88);

    // entry和store映射的page是不同的
    ret = swap_in_page(entry, &rp0);
    assert(ret == 0);
    ret = swap_in_page(store, &rp1);
    assert(ret == 0);
    assert(rp1 != rp0);

    // free memory
    // 从swap框架中解除rp0和rp1的映射
    swap_list_del(rp0), swap_list_del(rp1);
    swap_page_del(rp0), swap_page_del(rp1);

    assert(page_ref(rp0) == 1 && page_ref(rp1) == 1);
    assert(nr_active_pages == 0 && list_empty(&(active_list.swap_link)));
    assert(nr_inactive_pages == 0 && list_empty(&(inactive_list.swap_link)));

    // swap框架中不存在任何与pag的映射了
    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        assert(list_empty(hash_list + i));
    }

    // 解除0地址页表项的映射，该页表的的page会被释放
    page_remove(pgdir, 0);
    // 解除PAGE_SIZE地址页表项的映射，该页表的page会被释放
    page_remove(pgdir, PAGE_SIZE);

    // 释放页目录第一项指向的页表，并将页目录项设置为0
    free_page(pa2page(pgdir[0]));
    pgdir[0] = 0;

    mm->page_dir = NULL;
    mm_destory(mm);
    check_mm_struct = NULL;

    assert(nr_active_pages == 0 && nr_inactive_pages == 0);
    for (offset = 0; offset < max_swap_offset; offset ++) {
        mem_map[offset] = SWAP_UNUSED;
    }

    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());

    printk("check_swap() succeeded.\n");
}

static void check_mm_swap(void) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    int ret, i, j;
    for (i = 0; i < max_swap_offset; i++) {
        assert(mem_map[i] == SWAP_UNUSED);
    }

    extern struct MmStruct *check_mm_struct;
    assert(check_mm_struct == NULL);

    // step1: check mm_map

    struct MmStruct *mm0 = mm_create(), *mm1;
    assert(mm0 != NULL && list_empty(&(mm0->mmap_link)));  

    uintptr_t addr0, addr1;

    addr0 = 0;
    do {
        // ret = mm_map()
    } while (addr0 != 0);
}