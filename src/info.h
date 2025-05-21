#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "util.h"


/**
 * Optional configuration options for the `asdf_info` function
 */
typedef struct {
    const char *filename;
    bool print_tree;
    bool print_blocks;
} asdf_info_cfg_t;


ASDF_EXPORT int asdf_info(FILE *in_file, FILE *out_file, const asdf_info_cfg_t *cfg);
