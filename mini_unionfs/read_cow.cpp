#define FUSE_USE_VERSION 31
#include "common.h"

static int unionfs_open(const char *path,
                        struct fuse_file_info *fi) {
    (void)path; (void)fi;
          
    return -ENOENT;
}

static int unionfs_read(const char *path, char *buf,
                        size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void)path; (void)buf;
    (void)size; (void)offset; (void)fi;


    return -ENOENT;
}

static int unionfs_release(const char *path,
                            struct fuse_file_info *fi) {
    (void)path;

    if (fi->fh) {
        close((int)fi->fh);
        fi->fh = 0;
    }
    return 0;
}

extern struct fuse_operations unionfs_oper;

__attribute__((constructor))
void init_read() {
    unionfs_oper.open    = unionfs_open;
    unionfs_oper.read    = unionfs_read;
    unionfs_oper.release = unionfs_release;
}