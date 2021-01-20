#include <types.h>
#include <vfs.h>
#include <error.h>
#include <assert.h>
#include <string.h>
#include <inode.h>

static int get_device(char *path, char **sub_path, Inode **node_store) {
    int i;
    int slash = -1;
    int colon = -1;
    
    assert(node_store != NULL);

    for (i = 0; path[i] != '\0'; i++) {
        // ':' 前面的内容为磁盘名称
        if (path[i] == ':') { colon = i; break; }
        if (path[i] == '/') { slash = i; break; }
    }
    if (colon < 0 && slash != 0) {
        *sub_path = path;
        return vfs_get_current_dir(node_store);
    }
    if (colon > 0) {
        path[colon] = 0;
        while (path[++colon] == '/');
        *sub_path = path + colon;
        return vfs_get_root(path, node_store);
    }

    int ret;
    if (*path == '/') {
        if ((ret = vfs_get_bootfs(node_store)) != 0) {
            return ret;
        }
    } else {
        assert(*path == ':');
        Inode *node = NULL;
        if ((ret = vfs_get_current_dir(&node)) != 0) {
            return ret;
        }
        assert(node->in_fs != NULL);
        *node_store = fsop_get_root(node->in_fs);
        vop_ref_dec(node);
    }

    while (*(++path) == '/');
    *sub_path = path;
    return 0;
}

int vfs_lookup(char *path, Inode **node_store) {
    int ret;
    Inode *node = NULL;
    if ((ret = get_device(path, &path, &node)) != 0) {
        return ret;
    }
    if (*path != '\0') {
        ret = vop_lookup(node, path, node_store);
        vop_ref_dec(node);
        return ret;
    }
    assert(node_store != NULL);
    *node_store = node;
    return 0;
}

int vfs_lookup_parent(char *path, Inode **node_store, char **endp) {
    int ret;
    Inode *node = NULL;
    if ((ret = get_device(path, &path, &node)) != 0) {
        return ret;
    }
    ret = (*path != '\0') ? vop_lookup_parent(node, path, node_store, endp) : -E_INVAL;
    return ret;
}
