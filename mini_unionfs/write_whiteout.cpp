#define FUSE_USE_VERSION 31
#include "common.h"
#include <cstring>

static int unionfs_write(const char *path, const char *buf,
                          size_t size, off_t offset,
                          struct fuse_file_info *fi) {
    (void)fi;

    char resolved[PATH_MAX];
    bool is_upper;

    int ret = resolve_path(path, resolved, &is_upper);
    if (ret < 0) return ret;

    /* TODO Day 3: CoW safety fallback if is_upper is false */
    int fd = open(resolved, O_WRONLY);
    if (fd == -1) return -errno;

    ssize_t res = pwrite(fd, buf, size, offset);
    close(fd);

    if (res == -1) return -errno;
    return (int)res;
}

static int unionfs_create(const char *path, mode_t mode,
                           struct fuse_file_info *fi) {
    (void)fi;

    char upper[PATH_MAX];
    snprintf(upper, sizeof(upper), "%s%s",
             UNIONFS_DATA->upper_dir, path);

    /* TODO Day 3: ensure parent dir exists, store fd in fi->fh */
    int fd = open(upper, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd == -1) return -errno;

    close(fd);
    return 0;
}

static int unionfs_unlink(const char *path) {
    char upper[PATH_MAX], lower[PATH_MAX];
    snprintf(upper, sizeof(upper), "%s%s",
             UNIONFS_DATA->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s",
             UNIONFS_DATA->lower_dir, path);

    /* If in upper -> physical delete */
    if (access(upper, F_OK) == 0) {
        if (unlink(upper) == -1) return -errno;
        return 0;
    }

    /* TODO Day 3: if in lower -> create whiteout marker */
    if (access(lower, F_OK) == 0)
        return -EROFS;

    return -ENOENT;
}

static int unionfs_mkdir(const char *path, mode_t mode) {
    (void)path; (void)mode;
    /* TODO Day 3: create directory in upper layer */
    return -ENOENT;
}

static int unionfs_rmdir(const char *path) {
    (void)path;
    /* TODO Day 3: rmdir upper or whiteout lower */
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
