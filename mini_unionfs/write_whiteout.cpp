#define FUSE_USE_VERSION 31
#include "common.h"
#include <cstring>

static int unionfs_write(const char *path, const char *buf,
                          size_t size, off_t offset,
                          struct fuse_file_info *fi) {
    int fd;
    bool own_fd = false;

    if (fi && fi->fh) {
        fd = (int)fi->fh;
    } else {
        char resolved[PATH_MAX];
        bool is_upper;
        int ret = resolve_path(path, resolved, &is_upper);
        if (ret < 0) return ret;

        if (!is_upper) {
            ret = copy_to_upper(path);
            if (ret < 0) return ret;
            snprintf(resolved, PATH_MAX, "%s%s",
                     UNIONFS_DATA->upper_dir, path);
        }

        fd = open(resolved, O_WRONLY);
        if (fd == -1) return -errno;
        own_fd = true;
    }

    ssize_t res = pwrite(fd, buf, size, offset);
    if (own_fd) close(fd);

    if (res == -1) return -errno;
    return (int)res;
}

static int unionfs_create(const char *path, mode_t mode,
                           struct fuse_file_info *fi) {
    int ret = ensure_upper_dir(path);
    if (ret < 0) return ret;

    char upper[PATH_MAX];
    snprintf(upper, sizeof(upper), "%s%s",
             UNIONFS_DATA->upper_dir, path);

    int fd = open(upper, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd == -1) return -errno;

    /* Hand fd back so write() reuses it via fi->fh */
    fi->fh = (uint64_t)fd;
    return 0;
}

static int unionfs_unlink(const char *path) {
    char upper[PATH_MAX], lower[PATH_MAX];
    snprintf(upper, sizeof(upper), "%s%s",
             UNIONFS_DATA->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s",
             UNIONFS_DATA->lower_dir, path);

    if (access(upper, F_OK) == 0) {
        if (unlink(upper) == -1) return -errno;
        return 0;
    }

    if (access(lower, F_OK) == 0) {
        const char *filename = strrchr(path, '/');
        filename = filename ? filename + 1 : path;

        char parent[PATH_MAX];
        strncpy(parent, path, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        char *slash = strrchr(parent, '/');
        if (slash) *slash = '\0';
        else parent[0] = '\0';

        ensure_upper_dir(path);

        char whiteout[PATH_MAX * 2];
        snprintf(whiteout, sizeof(whiteout), "%s%s/.wh.%s",
                 UNIONFS_DATA->upper_dir,
                 parent[0] ? parent : "", filename);

        int fd = open(whiteout, O_CREAT | O_WRONLY, 0644);
        if (fd == -1) return -errno;
        close(fd);
        return 0;
    }

    return -ENOENT;
}

static int unionfs_mkdir(const char *path, mode_t mode) {
    int ret = ensure_upper_dir(path);
    if (ret < 0) return ret;

    char upper[PATH_MAX];
    snprintf(upper, sizeof(upper), "%s%s",
             UNIONFS_DATA->upper_dir, path);

    if (mkdir(upper, mode) == -1)
        return -errno;

    return 0;
}

static int unionfs_rmdir(const char *path) {
    char upper[PATH_MAX], lower[PATH_MAX];
    snprintf(upper, sizeof(upper), "%s%s",
             UNIONFS_DATA->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s",
             UNIONFS_DATA->lower_dir, path);

    if (access(upper, F_OK) == 0) {
        if (rmdir(upper) == -1) return -errno;
        return 0;
    }

    if (access(lower, F_OK) == 0) {
        const char *dirname = strrchr(path, '/');
        dirname = dirname ? dirname + 1 : path;

        char parent[PATH_MAX];
        strncpy(parent, path, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        char *slash = strrchr(parent, '/');
        if (slash) *slash = '\0';
        else parent[0] = '\0';

        ensure_upper_dir(path);

        char whiteout[PATH_MAX * 2];
        snprintf(whiteout, sizeof(whiteout), "%s%s/.wh.%s",
                 UNIONFS_DATA->upper_dir,
                 parent[0] ? parent : "", dirname);

        int fd = open(whiteout, O_CREAT | O_WRONLY, 0644);
        if (fd == -1) return -errno;
        close(fd);
        return 0;
    }

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
