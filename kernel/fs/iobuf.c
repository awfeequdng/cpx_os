#include <types.h>
#include <string.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>

IOBuf *iobuf_init(IOBuf *iob, void *base, size_t len, off_t offset) {
    iob->io_base = base;
    iob->io_offset = offset;
    iob->io_len = iob->io_resid = len;
    return iob;
}
// iob:
// data: data to move
// len: buffer size
// m2b: is move data to buffer? true: move data to iob; false: move data from iob
int iobuf_move(IOBuf *iob, void *data, size_t len, bool m2b, size_t *copiedp) {
    size_t move_len;
    if ((move_len = iob->io_resid) > len) {
        move_len = len;
    }
    if (move_len > 0) {
        void *src = iob->io_base;
        void *dst = data;
        if (m2b) {
            void *tmp = src;
            src = dst;
            dst = tmp;
        }
        memmove(dst, src, move_len);
        iobuf_skip(iob, move_len);
        len -= move_len;
    }
    if (copiedp != NULL) {
        *copiedp = move_len;
    }
    return (len == 0) ? 0 : -E_NO_MEM;
}

int iobuf_move_zeros(IOBuf *iob, size_t len, size_t *copiedp) {
    size_t move_len;
    if ((move_len = iob->io_resid) > len) {
        move_len = len;
    }
    if (move_len > 0) {
        memset(iob->io_base, 0, move_len);
        iobuf_skip(iob, move_len);
        len -= move_len;
    }

    if (copiedp != NULL) {
        *copiedp = move_len;
    }
    return (len == 0) ? 0 : -E_NO_MEM;
}

void iobuf_skip(IOBuf *iob, size_t n) {
    assert(iob->io_resid >= n);
    iob->io_base += n;
    iob->io_offset += n;
    iob->io_resid -= n;
}