#define FUSE_USE_VERSION 31
#include "common.h"

static int unionfs_getattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi) {
    (void)fi;

    char resolved[PATH_MAX];
    bool is_upper;

    int ret = resolve_path(path, resolved, &is_upper);
    if (ret < 0) return ret;

    if (lstat(resolved, stbuf) == -1)
        return -errno;

    return 0;
}

extern struct fuse_operations unionfs_oper;

__attribute__((constructor))
void init_metadata() {
    unionfs_oper.getattr = unionfs_getattr;
}