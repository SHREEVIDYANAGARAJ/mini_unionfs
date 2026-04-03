#define FUSE_USE_VERSION 31
#include "common.h"
#include <set>
#include <string>

static int unionfs_readdir(const char *path, void *buf,
                            fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi,
                            enum fuse_readdir_flags flags) {
    (void)path; (void)buf; (void)filler;
    (void)offset; (void)fi; (void)flags;

    /* TODO Day 2: scan upper layer
     * TODO Day 3: merge lower layer, handle whiteouts  */
    return 0;
}

extern struct fuse_operations unionfs_oper;

__attribute__((constructor))
void init_dir() {
    unionfs_oper.readdir = unionfs_readdir;
}