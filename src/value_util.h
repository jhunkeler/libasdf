#pragma once

#include "util.h"


/* Enum for YAML Common Schema types */
typedef enum {
    ASDF_YAML_COMMON_TAG_UNKNOWN,
    ASDF_YAML_COMMON_TAG_NULL,
    ASDF_YAML_COMMON_TAG_BOOL,
    ASDF_YAML_COMMON_TAG_INT,
    ASDF_YAML_COMMON_TAG_FLOAT,
    ASDF_YAML_COMMON_TAG_STR
} asdf_yaml_common_tag_t;


ASDF_LOCAL asdf_yaml_common_tag_t asdf_common_tag_get(const char *tagstr);
