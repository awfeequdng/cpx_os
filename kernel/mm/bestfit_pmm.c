#include <bestfit_pmm.h>
#include <list.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void bestfit_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

static void bestfit_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    list_add(&free_list, &(base->page_link));
}

// bestfit 就是找到最小页块，并能满足n个连续页要求
static struct Page *bestfit_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *entry = &free_list;
    while ((entry = list_next(entry)) != &free_list) {
        struct Page *p = le2page(entry, page_link);
        if (p->property >= n) {
            if (page == NULL || p->property < page->property) {
                page = p;
            }
        }
    }
    if (page != NULL) {
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            list_add(&free_list, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}

static void bestfit_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    list_entry_t *entry = list_next(&free_list);
    while (entry != &free_list) {
        p = le2page(entry, page_link);
        entry = list_next(entry);
        if (base + base->property == p) {
            // 释放页块的最后一页和另一个页块相邻，
            // 则将这两个页块拼接成一个大页块
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        } else if (p + p->property == base) {
            // 释放页块的第一页和另一个页块相邻，
            // 则将这两个页块拼接成一个大页块
            p->property += base->property;
            ClearPageProperty(base);
            base = p;
            list_del(&(p->page_link));
        }
    }
    nr_free += n;
    list_add(&free_list, &(base->page_link));
}

size_t bestfit_nr_free_pages(void) {
    return nr_free;
}

static void basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < get_npage() * PAGE_SIZE);
    assert(page2pa(p1) < get_npage() * PAGE_SIZE);
    assert(page2pa(p2) < get_npage() * PAGE_SIZE);


    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

static void bestfit_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    printk("================bestfit_check!=============\n");
    while((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count++;
        total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();
    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);
    
    free_pages(p0, 2);
    free_page(p2);
    
    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count--;
        total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
    printk("============ bestfit_check successed ===========\n");
}

const struct PmmManager bestfit_pmm_manager = {
    .name = "bestfit_pmm_manager",
    .init = bestfit_init,
    .init_memmap = bestfit_init_memmap,
    .alloc_pages = bestfit_alloc_pages,
    .free_pages = bestfit_free_pages,
    .nr_free_pages = bestfit_nr_free_pages,
    .check = bestfit_check,
};

const struct PmmManager *get_bestfit_pmm_manager(void) {
    return &bestfit_pmm_manager;
}