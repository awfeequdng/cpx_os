#ifndef __KERNEL_FS_VFS_INODE_H__
#define __KERNEL_FS_VFS_INODE_H__

#include <types.h>
#include <atomic.h>
#include <assert.h>
#include <dev.h>

struct stat;
struct iobuf;
struct fs;
struct inode_ops;

typedef struct inode {
    union {
        Device __device_info
    } in_info;
    enum {
        inode_type_device_info = 0,
    } in_type;
    atomic_t ref_count;
    atomic_t open_count;
    struct fs *in_fs;
    const struct inode_ops *in_ops;
} Inode;

#define __in_type(type)                 inode_type_##type##_info

#define check_inode_type(node, type)    ((node)->in_type == __in_type(type))

#define __vop_info(node, type)          \
    ({                                  \
        Inode *__node = (node);         \
        assert(__node != NULL && check_inode_type(__node, type));   \
        &(__node->in_info.__##type##_info);     \
    })

#define vop_info(node, type)            __vop_info(node, type)

#define info2node(info, type)           \
    container_of((info), Inode, in_info.__##type##_info)

Inode *__alloc_inode(int type);

#define alloc_inode(type)       __alloc_inode(__in_type(type))

#define MAX_INODE_COUNT         0x10000

int inode_ref_inc(Inode *node);
int inode_ref_dec(Inode *node);
int inode_open_inc(Inode *node);
int inode_open_dec(Inode *node);

// void inode_init(Inode *node, const )

#define VOP_MAGIC   0x8c4ba476

typedef struct inode_ops {
    unsigned long vop_magic;
    int (*vop_open)(Inode *node, uint32_t open_flags);
    int (*vop_close)(Inode *node);
    int (*vop_read)(Inode *node, struct iobuf *iob);
    int (*vop_write)(Inode *node, struct iobuf *iob);
    int (*vop_fstat)(Inode *node, struct stat *stat);
    int (*vop_fsync)(Inode *node);
    int (*vop_mkdir)(Inode *node, const char *name);
    int (*vop_link)(Inode *node, const char *name, Inode *link_node);
    int (*vop_rename)(Inode *node, const char *name, Inode *new_node, const char *new_name);
    int (*vop_readlink)(Inode *node, struct iobuf *iob);
    int (*vop_symlink)(Inode *node, const char *name, const char *path);
    int (*vop_namefile)(Inode *node, struct iobuf *iob);
    int (*vop_getdirentry)(Inode *node, struct iobuf *iob);
    int (*vop_reclaim)(Inode *node);
    int (*vop_ioctl)(Inode *node, int op, void *data);
    int (*vop_gettype)(Inode *node, uint32_t *type_store);
    int (*vop_tryseek)(Inode *node, off_t pos);
    int (*vop_truncate)(Inode *node, off_t len);
    int (*vop_create)(Inode *node, const char *name, bool excl, Inode **node_store);
    int (*vop_unlink)(Inode *node, const char *name);
    int (*vop_lookup)(Inode *node, char *path, Inode **node_store);
    int (*vop_lookup_parent)(Inode *node, char *path, Inode **node_store, char **endp);
} InodeOperations;

int null_vop_pass(void);
int null_vop_invalid(void);
int null_vop_unimplemented(void);
int null_vop_isdir(void);
int null_vop_notdir(void);

// 一致性检查
void inode_check(Inode *node, const char *opstr);

#define __vop_op(node, sym)             \
    ({                                  \
        Inode *__node = (node);                                \
        assert(__node != NULL && __node->in_ops != NULL && __node->in_ops->vop_##sym != NULL);       \
        inode_check(__node, #sym);  \
        __node->in_ops->vop_##sym;  \
     })

#define vop_open(node, open_flags)  (__vop_op(node, open)(node, open_flags))
#define vop_close(node)             (__vop_op(node, close)(node))
#define vop_read(node, iob)         (__vop_op(node, read)(node, iob))
#define vop_write(node, iob)        (__vop_op(node, write)(node, iob))
#define vop_fstat(node, stat)       (__vop_op(node, fstat)(node, stat))
#define vop_fsync(node)             (__vop_op(node, fsync)(node))
#define vop_mkdir(node, name)       (__vop_op(node, mkdir)(node, name))
#define vop_link(node, name, link_node)                 (__vop_op(node, link)(node, name, link_node))
#define vop_rename(node, name, new_node, new_name)      (__vop_op(node, rename)(node, name, new_node, new_name))
#define vop_readlink(node, iob)                         (__vop_op(node, readlink)(node, iob))
#define vop_symlink(node, name, path)                   (__vop_op(node, symlink)(node, name, path))
#define vop_namefile(node, iob)                         (__vop_op(node, namefile)(node, iob))
#define vop_getdirentry(node, iob)                      (__vop_op(node, getdirentry)(node, iob))
#define vop_reclaim(node)                               (__vop_op(node, reclaim)(node))
#define vop_ioctl(node, op, data)                       (__vop_op(node, ioctl)(node, op, data))
#define vop_gettype(node, type_store)                   (__vop_op(node, gettype)(node, type_store))
#define vop_tryseek(node, pos)                          (__vop_op(node, tryseek)(node, pos))
#define vop_truncate(node, len)                         (__vop_op(node, truncate)(node, len))
#define vop_create(node, name, excl, node_store)        (__vop_op(node, create)(node, name, excl, node_store))
#define vop_unlink(node, name)                          (__vop_op(node, unlink)(node, name))
#define vop_lookup(node, path, node_store)              (__vop_op(node, lookup)(node, path, node_store))
#define vop_lookup_parent(node, path, node_store, endp) (__vop_op(node, lookup_parent)(node, path, node_store, endp))


#define vop_ref_inc(node)       inode_ref_inc(node) 
#define vop_ref_dec(node)       inode_ref_dec(node) 

#define vop_open_inc(node)      inode_open_inc(node)
#define vop_open_dec(node)      inode_open_dec(node)

#define NULL_VOP_PASS           ((void *)null_vop_pass)
#define NULL_VOP_INVAL          ((void *)null_vop_invalid)
#define NULL_VOP_UNIMP          ((void *)null_vop_unimplemented)
#define NULL_VOP_ISDIR          ((void *)null_vop_isdir)
#define NULL_VOP_NOTDIR         ((void *)null_vop_notdir)

static inline int inode_ref_count(Inode *node) {
    return atomic_read(&(node->ref_count));
}

static inline int inode_open_count(Inode *node) {
    return atomic_read(&(node->open_count));
}

void inode_init(Inode *node, const InodeOperations *ops, struct fs *fs);
void inode_kill(Inode *node);

#define vop_fs(node)                ((node)->in_fs)
#define vop_init(node, ops, fs)     inode_init(node, ops, fs)
#define vop_kill(node)              inode_kill(node)

#endif // __KERNEL_FS_VFS_INODE_H__