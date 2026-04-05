#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "common.h"

struct fuse_operations unionfs_oper = {};

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <lower_dir> <upper_dir> <mount_point>\n",
            argv[0]);
        return 1;
    }

    mini_unionfs_state *state =
        (mini_unionfs_state *)malloc(sizeof(*state));
    if (!state) return 1;

    state->lower_dir = realpath(argv[1], NULL);
    state->upper_dir = realpath(argv[2], NULL);

    if (!state->lower_dir || !state->upper_dir) {
        fprintf(stderr,
            "Error: could not resolve lower_dir or upper_dir\n");
        return 1;
    }

    return fuse_main(argc - 2, argv + 2, &unionfs_oper, state);
}