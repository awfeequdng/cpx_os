#ifndef __KERNEL_FS_BITMAP_H__
#define __KERNEL_FS_BITMAP_H__

#include <types.h>

#define WORD_TYPE       uint32_t
#define WORD_BITS       (sizeof(WORD_TYPE) * BYTE_BITS)
typedef struct sfs_bitmap {
    uint32_t nbits;
    uint32_t nwords;
    WORD_TYPE *map;
} SfsBitmap;

SfsBitmap *bitmap_create(uint32_t nbits);
int bitmap_alloc(SfsBitmap *bitmap, uint32_t *index_store);
bool bitmap_test(SfsBitmap *bitmap, uint32_t index);
void bitmap_free(SfsBitmap *bitmap, uint32_t index);
void bitmap_destory(SfsBitmap *bitmap);
void *bitmap_get_data(SfsBitmap *bitmap, size_t *len_store);

#endif //__KERNEL_FS_BITMAP_H__
