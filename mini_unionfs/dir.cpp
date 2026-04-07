#define FUSE_USE_VERSION 31
#include "common.h"
#include <set>
#include <string>

static int unionfs_readdir(const char *path, void *buf,
                            fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi,
                            enum fuse_readdir_flags flags) {
    (void)offset; (void)fi; (void)flags;

    std::set<std::string> seen;
    std::set<std::string> whiteouts;

    char upper[PATH_MAX], lower[PATH_MAX];
    snprintf(upper, sizeof(upper), "%s%s", UNIONFS_DATA->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s", UNIONFS_DATA->lower_dir, path);

    filler(buf, ".",  NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);
    seen.insert(".");
    seen.insert("..");

    /* Scan upper layer first */
    DIR *dp = opendir(upper);
    if (dp) {
        struct dirent *de;
        while ((de = readdir(dp)) != NULL) {
            std::string name = de->d_name;
            if (name == "." || name == "..") continue;

            if (name.size() > 4 && name.substr(0, 4) == ".wh.") {
                whiteouts.insert(name.substr(4));
                continue;
            }

            if (seen.count(name)) continue;
            seen.insert(name);
            filler(buf, name.c_str(), NULL, 0, (fuse_fill_dir_flags)0);
        }
        closedir(dp);
    }

    /* Merge lower layer — skip whiteouts and already seen */
    dp = opendir(lower);
    if (dp) {
        struct dirent *de;
        while ((de = readdir(dp)) != NULL) {
            std::string name = de->d_name;
            if (name == "." || name == "..") continue;
            if (seen.count(name))      continue;
            if (whiteouts.count(name)) continue;

            seen.insert(name);
            filler(buf, name.c_str(), NULL, 0, (fuse_fill_dir_flags)0);
        }
        closedir(dp);
    }

    return 0;
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
void init_dir() {
    unionfs_oper.readdir = unionfs_readdir;
    unionfs_oper.mkdir   = unionfs_mkdir;
    unionfs_oper.rmdir   = unionfs_rmdir;
}