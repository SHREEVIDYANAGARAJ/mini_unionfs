#define FUSE_USE_VERSION 31
#ifndef COMMON_H
#define COMMON_H

#include <fuse3/fuse.h>
#include <string>
#include <set>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <limits.h>

struct mini_unionfs_state {
    const char *lower_dir;
    const char *upper_dir;
};
# 
#define UNIONFS_DATA \
    ((mini_unionfs_state *) fuse_get_context()->private_data)

int resolve_path(const char *path, char *resolved, bool *is_upper);
int copy_to_upper(const char *path);
int ensure_upper_dir(const char *path);

#endif