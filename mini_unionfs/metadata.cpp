#define FUSE_USE_VERSION 31
#include "common.h"

static int unionfs_getattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi) {
    (void)fi;
    (void)path;
    (void)stbuf;

    /* TODO Day 2: call resolve_path, then lstat on resolved path */
    return -ENOENT;
}

extern struct fuse_operations unionfs_oper;

__attribute__((constructor))
void init_metadata() {
    unionfs_oper.getattr = unionfs_getattr;
}