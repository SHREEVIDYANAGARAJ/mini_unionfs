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

    char upper[PATH_MAX];
    snprintf(upper, sizeof(upper), "%s%s", UNIONFS_DATA->upper_dir, path);

    filler(buf, ".",  NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);
    seen.insert(".");
    seen.insert("..");

    /* Scan upper layer */
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

    /* TODO Day 3: scan lower layer and merge */

    return 0;
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
void init_dir() {
    unionfs_oper.readdir = unionfs_readdir;
    unionfs_oper.mkdir   = unionfs_mkdir;
    unionfs_oper.rmdir   = unionfs_rmdir;
}