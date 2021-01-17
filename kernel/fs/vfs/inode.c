#include <types.h>
#include <stdio.h>
#include <slab.h>
#include <inode.h>
#include <error.h>
#include <assert.h>
#include <atomic.h>
#include <string.h>

Inode *__alloc_inode(int type) {
    Inode *node = NULL;
    if ((node = kmalloc(sizeof(Inode))) != NULL) {
        node->in_type = type;
    }
    return node;
}

void inode_init(Inode *node, const InodeOperations *ops, struct fs *fs) {
    atomic_set(&(node->ref_count), 0);
    atomic_set(&(node->open_count), 0);
    node->in_ops = ops;
    node->in_fs = fs;
    vop_ref_inc(node);
}

void inode_kill(Inode *node) {
    assert(inode_ref_count(node) == 0);
    assert(inode_open_count(node) == 0);
    kfree(node);
}

int inode_ref_inc(Inode *node) {
    return atomic_add_return(&(node->ref_count), 1);
}

int inode_ref_dec(Inode *node) {
    assert(inode_ref_count(node) > 0);
    int ref, ret;
    if ((ref = atomic_sub_return(&(node->ref_count), 1)) == 0) {
        if ((ret = vop_reclaim(node)) != 0 && ret != -E_BUSY) {
            printk("vfs: warning: vop_reclaim: %e.\n", ret);
        }
    }
    return ref;
}

int inode_open_inc(Inode *node) {
    return atomic_add_return(&(node->open_count), 1);
}

int inode_open_dec(Inode *node) {
    assert(inode_open_count(node) > 0);
    int open_cnt, ret;
    if ((open_cnt = atomic_sub_return(&(node->open_count), 1)) == 0) {
        if ((ret = vop_close(node)) != 0) {
            printk("vfs: warning: vop_close: %e.\n", ret);
        }
    }
    return open_cnt;
}


void inode_check(Inode *node, const char *opstr) {
    assert(node != NULL && node->in_ops != NULL);
    assert(node->in_ops->vop_magic == VOP_MAGIC);
    int ref_cnt = inode_ref_count(node);
    int open_cnt = inode_open_count(node);
    assert(ref_cnt >= open_cnt && open_cnt >= 0);
    assert(ref_cnt < MAX_INODE_COUNT && open_cnt < MAX_INODE_COUNT);
}

int null_vop_pass(void) {
    return 0;
}

int null_vop_invalid(void) {
    return -E_INVAL;
}

int null_vop_unimplemented(void) {
    return -E_UNIMP;
}

int null_vop_isdir(void) {
    return -E_ISDIR;
}

int null_vop_notdir(void) {
    return -E_NOTDIR;
}
