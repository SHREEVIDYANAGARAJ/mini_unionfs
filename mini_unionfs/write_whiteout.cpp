#define FUSE_USE_VERSION 31
#include "common.h"
#include <cstring>

static int unionfs_write(const char *path, const char *buf,
                          size_t size, off_t offset,
                          struct fuse_file_info *fi) {
    (void)path; (void)buf;
    (void)size; (void)offset; (void)fi;

    return -ENOENT;
}

static int unionfs_create(const char *path, mode_t mode,
                           struct fuse_file_info *fi) {
    (void)path; (void)mode; (void)fi;

    return -ENOENT;
}

static int unionfs_unlink(const char *path) {
    (void)path;

    return -ENOENT;
}

static int unionfs_mkdir(const char *path, mode_t mode) {
    (void)path; (void)mode;

    return -ENOENT;
}

static int unionfs_rmdir(const char *path) {
    (void)path;

    return -ENOENT;
}

extern struct fuse_operations unionfs_oper;

__attribute__((constructor))
void init_write() {
    unionfs_oper.write  = unionfs_write;
    unionfs_oper.create = unionfs_create;
    unionfs_oper.unlink = unionfs_unlink;
    unionfs_oper.mkdir  = unionfs_mkdir;
    unionfs_oper.rmdir  = unionfs_rmdir;
}
