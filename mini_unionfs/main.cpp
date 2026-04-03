#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include "common.h"

/* fuse_operations struct — handlers will be registered here
 * by each module's __attribute__((constructor)) function */
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

    /* TODO Day 2: add error handling for bad paths */
    state->lower_dir = argv[1];
    state->upper_dir = argv[2];

    return fuse_main(argc - 2, argv + 2, &unionfs_oper, state);
}