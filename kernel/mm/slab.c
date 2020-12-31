#include <slab.h>
#include <list.h>
#include <atomic.h>
#include <pmm.h>
#include <assert.h>
#include <sync.h>
#include <intr.h>
#include <buddy_pmm.h>
#include <stdio.h>
#include <rbtree.h>

/* The slab allocator is base on a paper, and the paper can be download from 
   http://citeseer.ist.psu.edu/bonwick94slab.html 
 */

#define BUFCTL_END  0XFFFFFFFFL     // the signature of the last bufctl
#define SLAB_LIMIT  0xfffffffel     // obj的最大个数

typedef size_t kmem_bufctl_t;
typedef struct slab_s {
    list_entry_t slab_link;
    void *s_mem;        // slab中第一个obj的内核虚拟地址
    size_t inuse;       // 有多少个obj已经被分配了
    size_t offset;      // 第一个obj在slab中的偏移量
    kmem_bufctl_t free; // 第一个空闲obj在slab中的索引
} slab_t;

#define le2slab(le, member)             \
    container_of((le), slab_t, member)

typedef struct kmem_cache_s {
    list_entry_t slabs_full;    // 所有obj都被分配出去的slab链接在此链表
    list_entry_t slabs_partial;  // slab的obj没有全部分配出去时挂接在该链表
    size_t objsize; // slab中obj的大小（以字节为单位）
    size_t num;     // 每个slab中obj的个数
    size_t offset;  // slab中第一个obj的偏移量
    bool off_slab;  // slab的控制参数是存放在slab内还是存放在slab外面

    size_t page_order;  // 每个slab使用的page数目（page_order为2的幂，即page数为：2^page_order）

    struct kmem_cache_s *slab_cachep;
} kmem_cache_t;

#define MIN_SIZE_ORDER      5   // slab中obj的最小值，也就是 2^5 = 32B
#define MAX_SIZE_ORDER      17  // slab中obj的最大值，也就是 2^17 = 128K
#define SLAB_CACHE_NUM      (MAX_SIZE_ORDER - MIN_SIZE_ORDER + 1)   // 总共有13个不同obj大小的slab缓冲区

static kmem_cache_t slab_cache[SLAB_CACHE_NUM];

static void init_kmem_cache(kmem_cache_t *cachep, size_t objsize, size_t align);
void check_slab(void);



void slab_init(void) {
    size_t i;
    // slab中obj的对齐值，最好为2^n
    size_t align = 16;
    for (i = 0; i < SLAB_CACHE_NUM; i++) {
        init_kmem_cache(slab_cache + i, 1 << (i + MIN_SIZE_ORDER), align);
    }
    check_slab();
    check_rbtree();
}

size_t slab_allocated(void) {
    size_t total = 0;
    int i;
    bool flag;
    local_intr_save(flag);
    {
        for (i = 0; i < SLAB_CACHE_NUM; i++) {
            kmem_cache_t *cachep = slab_cache + i;
            list_entry_t *head, *entry;
            head = entry = &(cachep->slabs_full);
            while ((entry = list_next(entry)) != head) {
                total += cachep->num * cachep->objsize;
            }
            head = entry = &(cachep->slabs_partial);
            while ((entry = list_next(entry)) != head) {
                slab_t *slabp = le2slab(entry, slab_link);
                total += cachep->objsize * slabp->inuse;
            }
        }
    }
    local_intr_restore(flag);

    return total;
}

// slab控制字段的大小，控制字段包括slab结构体自身大小，以及用于着色的部分，最后再对齐
static size_t slab_mgmt_size(size_t num, size_t align) {
    return ROUNDUP(sizeof(slab_t) + num * sizeof(kmem_bufctl_t), align);
}

// 根据slab的大小、obj的大小、控制字段是否在slab内部以及对齐等参数估算一个slab可以容纳多少个obj
static void cache_estimate(size_t order, size_t objsize, size_t align,
    bool off_slab, size_t *remainder, size_t *num) {
    size_t nr_objs;
    size_t mgmt_size = 0;
    size_t slab_size = (PAGE_SIZE << order);
    
    if (off_slab) {
        nr_objs = slab_size / objsize;
        if (nr_objs > SLAB_LIMIT) {
            nr_objs = SLAB_LIMIT;
        }
    } else {
        // 先计算出一个大概的obj数目，这个值已经接近真是值了
        // 疑问：这个nr_objs是偏大还是偏小？？，todo:
        nr_objs = (slab_size - sizeof(slab_t)) / (objsize + sizeof(kmem_bufctl_t));
        // 接着根据需要的obj数目计算需要多少个kmem_bufctl_t结构，这个结构的每一位代表一个obj，
        // 例如：最终有64个obj，kmem_bufctl_t为32位，那么就需要两个kmem_bufctl_t来标识slab
        // 中的obj是否被使用，如果相应位为1，标识对应的obj被分配了，为0标志对应obj为空闲
        while (slab_mgmt_size(nr_objs, align) + nr_objs * objsize > slab_size) {
            nr_objs--;
        }
        if (nr_objs > SLAB_LIMIT) {
            nr_objs = SLAB_LIMIT;
        }
        mgmt_size = slab_mgmt_size(nr_objs, align);
    }
    *num = nr_objs;
    *remainder = slab_size - nr_objs * objsize - mgmt_size;
}

// 根据objsize的大小来分配slab。
// 分配slab大小的原则如下：
// 
static void calculate_slab_order(kmem_cache_t *cachep, size_t objsize, size_t align,
    bool off_slab, size_t *left_over) {
    size_t order;
    for (order = 0; order <= KMALLOC_MAX_ORDER; order++) {
        size_t num, remainder;
        cache_estimate(order, objsize, align, off_slab, &remainder, &num);
        if (num != 0) {
            if (off_slab) {
                size_t off_slab_limit = objsize - sizeof(slab_t);
                off_slab_limit /= sizeof(kmem_bufctl_t);
                // todo: 这是何意？？
                // 难道obj的数量不能超过一个obj中能够容纳的kmem_bufctl_t的数量？
                // 是不是可以认为obj太多,判断的标准以一个obj容纳的kmem_bufctl_t的数量为准，
                // 也就是在大obj的slab中，obj的数量不能超过一个obj容纳的kmem_bufctl_t的数量
                if (num > off_slab_limit) {
                    panic("off_slab: objsize = %d, num = %d.\n", objsize, num);
                }
            }
            // 如果剩余量不超过slab总大小的8分之1，则这个slab大小是可以接收的
            if (remainder * 8 <= (PAGE_SIZE << order)) {
                cachep->num = num;
                cachep->page_order = order;
                if (left_over != NULL) {
                    *left_over = remainder;
                }
                return;
            }
        }
    }
    panic("caculate_slab_over: failed.");
}


static inline size_t get_order(size_t n) {
    size_t order = MIN_SIZE_ORDER;
    size_t order_size = (1 << order);

    for (; order <= MAX_SIZE_ORDER; order++, order_size <<= 1) {
        if (n <= order_size) {
            return order;
        }
    }
    panic("get_order failed. %d\n", n);
}


static void init_kmem_cache(kmem_cache_t *cachep, size_t objsize, size_t align) {
    list_init(&(cachep->slabs_full));
    list_init(&(cachep->slabs_partial));

    objsize = ROUNDUP(objsize, align);
    cachep->objsize = objsize;
    // 如果objsize大于512（包含512）字节，则将slab控制信息放在slab外面,
    // 这是因为obj太大时，控制信息需要单独占据一个obj，但实际控制信息远远用不了一个obj，
    // 这就造成了内存浪费，因此obj太大，slab控制信息放在slab外面
    cachep->off_slab = (objsize >= (PAGE_SIZE >> 3));

    size_t left_over;
    // 该函数计算出cachep的num和order
    calculate_slab_order(cachep, objsize, align, cachep->off_slab, &left_over);

    assert(cachep->num > 0);

    size_t mgmt_size = slab_mgmt_size(cachep->num, align);

    if (cachep->off_slab) {
        cachep->offset = 0;
        // cache的控制信息不放在slab自身处，而是放在最小的cache中(cache中的obj大小正好大于等于mgmt_size)
        cachep->slab_cachep = slab_cache + (get_order(mgmt_size) - MIN_SIZE_ORDER);
    } else {
        cachep->offset = mgmt_size;
        cachep->slab_cachep = NULL;
    }
}

static void *kmem_cache_alloc(kmem_cache_t *cachep);

#define slab_bufctl(slabp)  \
    ((kmem_bufctl_t *)(((slab_t *)(slabp)) + 1))


// 初始化slab，并将slab的s_mem成员设置为第一个obj的地址
static slab_t *kmem_cache_slabmgmt(kmem_cache_t *cachep, struct Page *page) {
    void *objp = page2kva(page);
    slab_t *slabp = NULL;

    if (cachep->off_slab) {
        if ((slabp = kmem_cache_alloc(cachep->slab_cachep)) == NULL) {
            return NULL;
        }
    } else {
        slabp = page2kva(page);
    }
    slabp->inuse = 0;
    slabp->offset = cachep->offset;
    slabp->s_mem = objp + cachep->offset;

    return slabp;
}


// page的page_link.next用来指向kmem_cache_t
#define SET_PAGE_CACHE(page, cachep)                                    \
    do {                                                                \
        struct Page *__page = (struct Page *)(page);                    \
        kmem_cache_t **__cachepp = (kmem_cache_t **)&(__page->page_link.next); \
        *__cachepp = (kmem_cache_t *)(cachep);                          \
    } while (0)

// page的page_link.prev用来指向slab_t
#define SET_PAGE_SLAB(page, slabp)                                      \
    do {                                                                \
        struct Page *__page = (struct Page *)(page);                    \
        slab_t **__slabpp = (slab_t **)&(__page->page_link.prev);      \
        *__slabpp = (slab_t *)(slabp);                                 \
    } while (0)


// 项cache中添加一个新的slab
static bool kmem_cache_grow(kmem_cache_t *cachep) {
    // 该cache管理的slab大小为(1 << page_order)
    struct Page *page = alloc_pages(1 << cachep->page_order);
    if (page == NULL) {
        goto failed;
    }
    
    slab_t *slabp;
    // slab结构要么放在slab缓冲区的开始位置，要么另外申请一片内存用于存放slab结构
    if ((slabp = kmem_cache_slabmgmt(cachep, page)) == NULL) {
        printk("oops: slabp == NULL\n");
        goto oops;
    }
    size_t order_size = (1 << cachep->page_order);

    do {
        SET_PAGE_CACHE(page, cachep);
        SET_PAGE_SLAB(page, slabp);
        // 该page用于slab缓存
        SetPageSlab(page);
        page++;
    } while (--order_size);

    int i;
    // cache的slab中有num个obj，用num个kmem_bufctl_t来管理num个obj
    for (i = 0; i < cachep->num; i++) {
        slab_bufctl(slabp)[i] = i + 1;
    }
    // 第一个可用kmem_bufctl_t指向第二个可用kmem_bufctl_t，
    // 最后一个kmem_bufctl_t指向BUFCTL_END，表示没有可用的obj了
    slab_bufctl(slabp)[cachep->num - 1] = BUFCTL_END;
    // 第一个可用obj的索引设置为0，也就是从第0个obj开始申请obj
    slabp->free = 0; 
    bool flag;
    local_intr_save(flag);
    {
        list_add(&(cachep->slabs_partial), &(slabp->slab_link));
    }
    local_intr_restore(flag);
    return 1;

oops:
    free_pages(page, 1 << cachep->page_order);
failed:
    return 0;
}

static void *kmem_cache_alloc_one(kmem_cache_t *cachep, slab_t *slabp) {
    slabp->inuse++;
    void *objp = slabp->s_mem + slabp->free * cachep->objsize;
    slabp->free = slab_bufctl(slabp)[slabp->free];

    if (slabp->free == BUFCTL_END) {
        // slab中没有可用obj了，将其挂在cache->slabs_full这个链表
        list_del(&(slabp->slab_link));
        list_add(&(cachep->slabs_full), &(slabp->slab_link));
    }
    return objp;
}

static void *kmem_cache_alloc(kmem_cache_t *cachep) {
    void *objp;
    bool flag;

try_again:
    local_intr_save(flag);
    if (list_empty(&(cachep->slabs_partial))) {
        goto alloc_new_slab;
    }
    slab_t *slabp = le2slab(list_next(&(cachep->slabs_partial)), slab_link);
    objp = kmem_cache_alloc_one(cachep, slabp);
    local_intr_restore(flag);
    return objp;

alloc_new_slab:
    local_intr_restore(flag);

    if (kmem_cache_grow(cachep)) {
        goto try_again;
    }
    return NULL;
}

void *kmalloc(size_t size) {
    assert(size > 0);
    size_t order = get_order(size);
    if (order > MAX_SIZE_ORDER) {
        return NULL;
    }
    return kmem_cache_alloc(slab_cache + (order - MIN_SIZE_ORDER));
}

static void kmem_cache_free(kmem_cache_t *cachep, void *obj);

static void kmem_slab_destory(kmem_cache_t *cachep, slab_t *slabp) {
    struct Page *page = kva2page(slabp->s_mem - slabp->offset);

    struct Page *p = page;
    size_t order_size = (1 << cachep->page_order);
    do {
        assert(PageSlab(p));
        ClearPageSlab(p);
        p++;
    } while (--order_size);

    free_pages(page, 1 << cachep->page_order);

    if (cachep->off_slab) {
        kmem_cache_free(cachep->slab_cachep, slabp);
    }
}

static void kmem_cache_free_one(kmem_cache_t *cachep, slab_t *slabp, void *objp) {
    // 在内核中尽量不要使用除法
    size_t nr_obj = (objp - slabp->s_mem) / cachep->objsize;
    slab_bufctl(slabp)[nr_obj] = slabp->free;
    slabp->free = nr_obj;

    slabp->inuse--;

    if (slabp->inuse == 0) {
        // 如果slab中的obj没有被使用，就释放这个slab
        // 我们可以优化一下：在slab中的obj没有被使用时，
        // 暂时将其放入empty链表，等待以后使用（目前没有empty这个链表，需要添加）
        list_del(&(slabp->slab_link));
        kmem_slab_destory(cachep, slabp);
    } else if (slabp->inuse == cachep->num - 1) {
        // 说明之前满了，现在空出一个obj出来了
        list_del(&(slabp->slab_link));
        list_add(&(cachep->slabs_partial), &(slabp->slab_link));
    }
}

#define GET_PAGE_CACHE(page)    \
    (kmem_cache_t *)((page)->page_link.next)

#define GET_PAGE_SLAB(page)    \
    (slab_t *)((page)->page_link.prev)
  
static void kmem_cache_free(kmem_cache_t *cachep, void *objp) {
    bool flag;
    struct Page *page = kva2page(objp);
    if (!PageSlab(page)) {
        panic("not a slab page %08x\n", objp);
    }

    local_intr_save(flag);
    {
        kmem_cache_free_one(cachep, GET_PAGE_SLAB(page), objp);
    }
    local_intr_restore(flag);
}

void kfree(void *objp) {
    kmem_cache_free(GET_PAGE_CACHE(kva2page(objp)), objp);
}


static inline void check_slab_empty(void) {
    int i;
    for(i = 0; i < SLAB_CACHE_NUM; i++) {
        kmem_cache_t *cachep = slab_cache + i;
        assert(list_empty(&(cachep->slabs_full)));
        assert(list_empty(&(cachep->slabs_partial)));
    }
}

void check_slab(void) {
    int i;
    void *v0, *v1;
    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    check_slab_empty();
    assert(slab_allocated() == 0);

    kmem_cache_t *cachep0, *cachep1;

    cachep0 = slab_cache;
    // 第1个cache的obj size为32，并且slab结构体存储在自身的slab缓存内
    assert(cachep0->objsize == 32 && cachep0->num > 1 && !cachep0->off_slab);
    // 申请一个obj，这个obj会在obj等于32的cache上分配
    assert((v0 = kmalloc(16)) != NULL);
    slab_t *slabp0, *slabp1;
    // slab已经分配了一个obj，因此存在一个非空的slab，当然这里默认slab中obj的个数大于1
    assert(!list_empty(&cachep0->slabs_partial));
    // 分配16字节所在的slab
    slabp0 = le2slab(list_next(&(cachep0->slabs_partial)), slab_link);
    // 在slabp0中只申请了一个obj，并且slab所在的cache只有一个slab
    assert(slabp0->inuse == 1 && list_next(&(slabp0->slab_link)) == &(cachep0->slabs_partial));

    struct Page *p0, *p1;
    size_t order_size;

    // slabp0所在的page为p0
    p0 = kva2page(slabp0->s_mem - slabp0->offset);
    p1 = p0;
    // cache0中slab的大小为order_size个page
    order_size = (1 << cachep0->page_order);
    for (i = 0; i < cachep0->page_order; i++, p1++) {
        // cache中的page都设置了slab标志
        assert(PageSlab(p1));
        // 所有page都指向了page所在的cache和slab
        assert(GET_PAGE_CACHE(p1) == cachep0 && GET_PAGE_SLAB(p1) == slabp0);
    }

    // v0 分配在slabp0的第一个obj上
    assert(v0 == slabp0->s_mem);
    // v1 分配在slabp0的第二个obj上（每个obj大小为32）
    assert((v1 = kmalloc(16)) != NULL && v1 == v0 + 32);

    kfree(v0);
    // 释放v0，那么slabp0的下一个可用obj索引为0
    assert(slabp0->free == 0);
    // 释放v1，那么slabp0的下一个可用obj索引为1（但是由于这个slab没有obj被使用，因此会被释放掉）
    kfree(v1);
    // slabp0已经被释放
    assert(list_empty(&(cachep0->slabs_partial)));

    // slabp0中的所有page都被释放掉了，因此之前申请的page不再有slab标志
    for (i = 0; i < cachep0->page_order; i++, p0++) {
        assert(!PageSlab(p0));
    }

    // 申请一个obj，在obj大小为32的cache上申请
    v0 = kmalloc(16);
    // obj大小为32的cache的slabs_partial链表非空
    assert(!list_empty(&(cachep0->slabs_partial)));
    // slabp0位obj大小为32的slab缓存指针
    slabp0 = le2slab(list_next(&(cachep0->slabs_partial)), slab_link);
    // 将slabp0中剩余的num - 1个obj也申请掉，此时slabp0变成full 了，因此这个slabp0倍挂载到full链表
    for (i = 0; i < cachep0->num - 1; i++) {
        kmalloc(16);
    }
    // slabp0所有obj都被使用了
    assert(slabp0->inuse == cachep0->num);
    // slabp0被挂载在full链表了
    assert(list_next(&(cachep0->slabs_full)) == &(slabp0->slab_link));
    // 此时cache的partial没有slab对象了
    assert(list_empty(&(cachep0->slabs_partial)));
    // v1申请占据一个32字节的obj，此时cache的partial链表非空，full链表也非空

    v1 = kmalloc(16);
    // partial链表非空
    assert(!list_empty(&(cachep0->slabs_partial)));
    // slabp1为非full的slab，
    slabp1 = le2slab(list_next(&(cachep0->slabs_partial)), slab_link);

    // slabp0为full的slab，slabp1为非full的slab，两个值不会相等
    assert(slabp1 != slabp0);



    kfree(v0);
    assert(list_empty(&(cachep0->slabs_full)));
    assert(list_next(&(slabp0->slab_link)) == &(slabp1->slab_link) ||
        list_next(&(slabp1->slab_link)) == &(slabp0->slab_link));
    
    kfree(v1);
    assert(!list_empty(&(cachep0->slabs_partial)));
    assert(list_next(&(cachep0->slabs_partial)) == &(slabp0->slab_link));
    assert(list_next(&(slabp0->slab_link)) == &(cachep0->slabs_partial));

    v1 = kmalloc(16);
    assert(v1 == v0);
    assert(list_next(&(cachep0->slabs_full)) == &(slabp0->slab_link));
    assert(list_empty(&(cachep0->slabs_partial)));

    for (i = 0; i < cachep0->num; i ++) {
        kfree(v1 + i * cachep0->objsize);
    }

    assert(list_empty(&(cachep0->slabs_full)));
    assert(list_empty(&(cachep0->slabs_partial)));
    
    cachep0 = slab_cache;

    bool has_off_slab = 0;
    for (i = 0; i < SLAB_CACHE_NUM; i ++, cachep0 ++) {
        if (cachep0->off_slab) {
            has_off_slab = 1;
            cachep1 = cachep0->slab_cachep;
            if (!cachep1->off_slab) {
                break;
            }
        }
    }

    if (!has_off_slab) {
        goto check_pass;
    }

    assert(cachep0->off_slab && !cachep1->off_slab);
    assert(cachep1 < cachep0);

    assert(list_empty(&(cachep0->slabs_full)));
    assert(list_empty(&(cachep0->slabs_partial)));

    assert(list_empty(&(cachep1->slabs_full)));
    assert(list_empty(&(cachep1->slabs_partial)));

    v0 = kmalloc(cachep0->objsize);
    p0 = kva2page(v0);
    assert(page2kva(p0) == v0);

    if (cachep0->num == 1) {
        assert(!list_empty(&(cachep0->slabs_full)));
        slabp0 = le2slab(list_next(&(cachep0->slabs_full)), slab_link);
    }
    else {
        assert(!list_empty(&(cachep0->slabs_partial)));
        slabp0 = le2slab(list_next(&(cachep0->slabs_partial)), slab_link);
    }
    assert(slabp0 != NULL);

    if (cachep1->num == 1) {
        assert(!list_empty(&(cachep1->slabs_full)));
        slabp1 = le2slab(list_next(&(cachep1->slabs_full)), slab_link);
    }
    else {
        assert(!list_empty(&(cachep1->slabs_partial)));
        slabp1 = le2slab(list_next(&(cachep1->slabs_partial)), slab_link);
    }
    assert(slabp1 != NULL);

    order_size = (1 << cachep0->page_order);
    for (i = 0; i < order_size; i++, p0++) {
        assert(PageSlab(p0));
        assert(GET_PAGE_CACHE(p0) == cachep0 && GET_PAGE_SLAB(p0) == slabp0);
    }
    
    kfree(v0);

check_pass:
    // check_rb_tree();
    check_slab_empty();
    assert(slab_allocated() == 0);
    assert(slab_allocated() == slab_allocated_store);
    assert(nr_free_pages() == nr_free_pages_store);

    printk("------------check_slab: successed---------------\n");

}