#include <bitmap.h>
#include <types.h>
#include <assert.h>
#include <slab.h>
#include <error.h>
#include <string.h>

SfsBitmap *bitmap_create(uint32_t nbits) {
    static_assert(WORD_BITS != 0);
    assert(nbits != 0 && nbits + WORD_BITS > nbits);

    SfsBitmap *bitmap = NULL;
    if ((bitmap = kmalloc(sizeof(SfsBitmap))) == NULL) {
        return NULL;
    }

    uint32_t nwords = ROUNDUP_DIV(nbits, WORD_BITS);
    WORD_TYPE *map = NULL;
    if ((map = kmalloc(sizeof(WORD_TYPE) * nwords)) == NULL) {
        kfree(bitmap);
        return NULL;
    }

    bitmap->nbits = nbits;
    bitmap->nwords = nwords;
    bitmap->map = memset(map, 0xFF, sizeof(WORD_TYPE) * nwords);

    if (nbits != nwords * WORD_BITS) {
        // 最后一个word有空余的bit没有使用
        uint32_t ix = nwords - 1;
        uint32_t over_bits = nbits - ix * WORD_BITS;
        assert(nbits / WORD_BITS == ix);
        assert(over_bits > 0 && over_bits < WORD_BITS);

        for (; over_bits < WORD_BITS; over_bits++) {
            // 将最后没有使用的bit清零
            bitmap->map[ix] ^= (1 << over_bits);
        }
    }
    return bitmap;
}

int bitmap_alloc(SfsBitmap *bitmap, uint32_t *index_store) {
    WORD_TYPE *map = bitmap->map;
    uint32_t ix, offset;
    uint32_t nwords = bitmap->nwords;
    for (ix = 0; ix < nwords; ix++) {
        if (map[ix] != 0) {
            // 不等于0表示有bit为1
            for (offset = 0; offset < WORD_BITS; offset++) {
                WORD_TYPE mask = (1 << offset);
                if (map[ix] & mask) {
                    map[ix] ^= mask;
                    *index_store = ix * WORD_BITS + offset;
                    return 0;
                }
            }
        }
    }
    return -E_NO_MEM;
}

static void bitmap_translate(SfsBitmap *bitmap, uint32_t index, WORD_TYPE **word, WORD_TYPE *mask) {
    assert(index < bitmap->nbits);
    uint32_t ix = index / WORD_BITS;
    uint32_t offset = index % WORD_BITS;
    *word = bitmap->map + ix;
    *mask = (1 << offset);
}

bool bitmap_test(SfsBitmap *bitmap, uint32_t index) {
    WORD_TYPE mask;
    WORD_TYPE *word = NULL;
    bitmap_translate(bitmap, index, &word, &mask);
    return (*word & mask);
}

void bitmap_free(SfsBitmap *bitmap, uint32_t index) {
    WORD_TYPE mask;
    WORD_TYPE *word = NULL;
    bitmap_translate(bitmap, index, &word, &mask);
    assert(!(*word & mask));
    *word |= mask;
}

void bitmap_destory(SfsBitmap *bitmap) {
    kfree(bitmap->map);
    kfree(bitmap);
}

void *bitmap_get_data(SfsBitmap *bitmap, size_t *len_store) {
    if (len_store != NULL) {
        *len_store = sizeof(WORD_TYPE) * bitmap->nwords;
    }
    return bitmap->map;
}