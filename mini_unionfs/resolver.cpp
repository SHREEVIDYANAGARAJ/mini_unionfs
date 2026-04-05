#define FUSE_USE_VERSION 31
#include "common.h"
#include <limits.h>

int ensure_upper_dir(const char *path) {
    char tmp[PATH_MAX * 2];
    snprintf(tmp, sizeof(tmp), "%s%s", UNIONFS_DATA->upper_dir, path);

    char *slash = strrchr(tmp, '/');
    if (!slash || slash == tmp + strlen(UNIONFS_DATA->upper_dir))
        return 0;
    *slash = '\0';

    char build[PATH_MAX * 2];
    strncpy(build, tmp, sizeof(build) - 1);
    build[sizeof(build) - 1] = '\0';

    for (char *p = build + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(build, 0755) == -1 && errno != EEXIST)
                return -errno;
            *p = '/';
        }
    }
    if (mkdir(build, 0755) == -1 && errno != EEXIST)
        return -errno;

    return 0;
}

int copy_to_upper(const char *path) {
    char lower[PATH_MAX], upper[PATH_MAX];
    snprintf(lower, sizeof(lower), "%s%s", UNIONFS_DATA->lower_dir, path);
    snprintf(upper, sizeof(upper), "%s%s", UNIONFS_DATA->upper_dir, path);

    int ret = ensure_upper_dir(path);
    if (ret < 0) return ret;

    struct stat st;
    if (stat(lower, &st) == -1) return -errno;

    int src = open(lower, O_RDONLY);
    if (src == -1) return -errno;

    int dst = open(upper, O_CREAT | O_WRONLY | O_TRUNC, st.st_mode & 0777);
    if (dst == -1) {
        int saved = errno;
        close(src);
        return -saved;
    }

    char buf[65536];
    ssize_t n;
    while ((n = read(src, buf, sizeof(buf))) > 0) {
        if (write(dst, buf, (size_t)n) != n) {
            int saved = errno;
            close(src);
            close(dst);
            return -saved;
        }
    }

    close(src);
    close(dst);
    return (n < 0) ? -errno : 0;
}

int resolve_path(const char *path, char *resolved, bool *is_upper) {
    char upper[PATH_MAX], lower[PATH_MAX], whiteout[PATH_MAX * 2];

    snprintf(upper, sizeof(upper), "%s%s", UNIONFS_DATA->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s", UNIONFS_DATA->lower_dir, path);

    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    char dir[PATH_MAX];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    else dir[0] = '\0';

    snprintf(whiteout, sizeof(whiteout), "%s%s/.wh.%s",
             UNIONFS_DATA->upper_dir, dir[0] ? dir : "", filename);

    if (access(whiteout, F_OK) == 0)
        return -ENOENT;

    if (access(upper, F_OK) == 0) {
        strncpy(resolved, upper, PATH_MAX - 1);
        resolved[PATH_MAX - 1] = '\0';
        *is_upper = true;
        return 0;
    }

    if (access(lower, F_OK) == 0) {
        strncpy(resolved, lower, PATH_MAX - 1);
        resolved[PATH_MAX - 1] = '\0';
        *is_upper = false;
        return 0;
    }

    return -ENOENT;
}