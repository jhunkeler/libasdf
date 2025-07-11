#pragma once

#include <libfyaml.h>

#include <asdf/file.h>

#include "context.h"
#include "parse.h"
#include "util.h"


typedef struct asdf_file {
    asdf_base_t base;
    asdf_parser_t *parser;
    struct fy_document *tree;
} asdf_file_t;


/* Internal helper to get the `struct fy_document` for the tree, if any */
ASDF_LOCAL struct fy_document *asdf_file_get_tree_document(asdf_file_t *file);
