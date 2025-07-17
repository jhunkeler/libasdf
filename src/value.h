#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <libfyaml.h>

#include <asdf/value.h>

#include "file.h"
#include "util.h"
#include "value.h"


/* Enum for YAML Common Schema types */
typedef enum {
    ASDF_YAML_COMMON_TAG_UNKNOWN,
    ASDF_YAML_COMMON_TAG_NULL,
    ASDF_YAML_COMMON_TAG_BOOL,
    ASDF_YAML_COMMON_TAG_INT,
    ASDF_YAML_COMMON_TAG_FLOAT,
    ASDF_YAML_COMMON_TAG_STR
} asdf_yaml_common_tag_t;


typedef struct asdf_value {
    asdf_file_t *file;
    asdf_value_type_t type;
    asdf_value_err_t err;
    struct fy_node *node;
    union {
        bool b;
        int64_t i;
        uint64_t u;
        double d;
        void *ext;
    } scalar;
} asdf_value_t;


ASDF_LOCAL asdf_value_t *asdf_value_create(asdf_file_t *file, struct fy_node *node);
ASDF_LOCAL asdf_yaml_common_tag_t asdf_common_tag_get(const char *tagstr);
