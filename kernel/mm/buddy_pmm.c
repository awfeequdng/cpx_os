#include <buddy_pmm.h>
#include <list.h>
#include <string.h>
#include <assert.h>


#define MAX_ORDER   10
static free_area_t free_area[MAX_ORDER + 1];

#define free_list(x) (free_area[x].free_list)
#define nr_free(x)  (free_area[x].nr_free)

#define MAX_ZONE_NUM    10
struct Zone {
    struct Page *mem_base;
} zones[MAX_ZONE_NUM] = {{NULL}};

static void buddy_init(void) {
    int i;
    for (i = 0; i <= MAX_ORDER; i++) {
        list_init(&free_list(i));
        nr_free(i) = 0;
    }
}

static void buddy_init_memmap(struct Page *base, size_t n) {
    static int zone_num = 0;
    assert(n > 0 && zone_num < MAX_ZONE_NUM);
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        p->zone_num = zone_num;
        set_page_ref(p, 0);
    }
    p = zones[zone_num++].mem_base = base;
    size_t order = MAX_ORDER;
    size_t order_size = (1 << order);
}

const struct PmmManager *get_buddy_pmm_manager(void) {
    return NULL;
}