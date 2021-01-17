#ifndef __KERNEL_FS_IOBUF_H__
#define __KERNEL_FS_IOBUF_H__


#include <types.h>

typedef struct iobuf {
    void *io_base;
    off_t io_offset;
    size_t io_len;
    size_t io_resid;    /* Remaining amt of data to xfer */
} IOBuf;

#define iobuf_used(iob)     ((size_t)((iob)->io_len - (iob)->io_resid))

IOBuf *iobuf_init(IOBuf *, void *base, size_t len, off_t offset);
int iobuf_move(IOBuf *iob, void *data, size_t len, bool m2b, size_t *copiedp);
int iobuf_move_zeros(IOBuf *iob, size_t len, size_t *copiedp);
void iobuf_skip(IOBuf *iob, size_t len);

#endif // __KERNEL_FS_IOBUF_H__