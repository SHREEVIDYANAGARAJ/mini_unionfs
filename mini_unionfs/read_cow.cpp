#define FUSE_USE_VERSION 31
#include "common.h"

static int unionfs_open(const char *path,
                        struct fuse_file_info *fi) {
    char resolved[PATH_MAX];
    bool is_upper;

    int ret = resolve_path(path, resolved, &is_upper);
    if (ret < 0) return ret;

    /* CoW trigger: file is in lower and caller wants to write */
    if (!is_upper && (fi->flags & (O_WRONLY | O_RDWR))) {
        ret = copy_to_upper(path);
        if (ret < 0) return ret;
        snprintf(resolved, PATH_MAX, "%s%s",
                 UNIONFS_DATA->upper_dir, path);
    }

    int fd = open(resolved, fi->flags);
    if (fd == -1) return -errno;

    /* Store fd in fi->fh so read/write reuse it */
    fi->fh = (uint64_t)fd;
    return 0;
}

static int unionfs_read(const char *path, char *buf,
                        size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void)path;
    int fd;
    bool own_fd = false;

    if (fi && fi->fh) {
        fd = (int)fi->fh;
    } else {
        /* Fallback: no prior open, resolve and open ourselves */
        char resolved[PATH_MAX];
        bool is_upper;
        int ret = resolve_path(path, resolved, &is_upper);
        if (ret < 0) return ret;
        fd = open(resolved, O_RDONLY);
        if (fd == -1) return -errno;
        own_fd = true;
    }

    ssize_t res = pread(fd, buf, size, offset);
    if (own_fd) close(fd);

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