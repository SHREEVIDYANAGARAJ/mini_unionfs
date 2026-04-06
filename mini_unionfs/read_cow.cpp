#define FUSE_USE_VERSION 31
#include "common.h"

static int unionfs_open(const char *path,
                        struct fuse_file_info *fi) {
    (void)path; (void)fi;


    char resolved[PATH_MAX];
    bool is_upper;

    int ret = resolve_path(path, resolved, &is_upper);
    if (ret < 0) return ret;

    int fd = open(resolved, fi->flags);
    if (fd == -1) return -errno;

    close(fd);
    return 0;
}

static int unionfs_read(const char *path, char *buf,
                        size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void)fi;

    char resolved[PATH_MAX];
    bool is_upper;

    int ret = resolve_path(path, resolved, &is_upper);
    if (ret < 0) return ret;

    int fd = open(resolved, O_RDONLY);
    if (fd == -1) return -errno;

    ssize_t res = pread(fd, buf, size, offset);
    close(fd);

    if (res == -1) return -errno;
    return (int)res;
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