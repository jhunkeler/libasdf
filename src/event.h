/**
 * Parser events
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "block.h"
#include "parse.h"
#include "util.h"
#include "yaml.h"


#define ASDF_EVENT_TYPES(X) \
    X(ASDF_NONE_EVENT) \
    X(ASDF_BEGIN_EVENT) \
    X(ASDF_ASDF_VERSION_EVENT) \
    X(ASDF_STANDARD_VERSION_EVENT) \
    X(ASDF_COMMENT_EVENT) \
    X(ASDF_TREE_START_EVENT) \
    X(ASDF_YAML_EVENT) \
    X(ASDF_TREE_END_EVENT) \
    X(ASDF_BLOCK_EVENT) \
    X(ASDF_PADDING_EVENT) \
    X(ASDF_BLOCK_INDEX_EVENT) \
    X(ASDF_END_EVENT)

typedef enum {
// clang-format off
#define X(member) member,
    ASDF_EVENT_TYPES(X)
#undef X
    ASDF_EVENT_TYPE_COUNT
    // clang-format on
} asdf_event_type_t;


static const char *const asdf_event_type_names[] = {
#define X(name) #name,
    ASDF_EVENT_TYPES(X)
#undef X
};


typedef struct {
    char *version;
} asdf_version_t;


typedef struct {
    size_t start;
    size_t end;
    const char *buf;
} asdf_tree_info_t;


typedef struct fy_event asdf_yaml_event_t;


typedef struct asdf_event {
    asdf_event_type_t type;
    union {
        // Only if ASDF_*_VERSION_EVENT
        asdf_version_t *version;
        // Only if ASDF_COMMENT_EVENT
        char *comment;
        // Only if ASDF_TREE_*_EVENT
        asdf_tree_info_t *tree;
        // Only if ASDF_YAML_EVENT
        asdf_yaml_event_t *yaml;
        // Only if ASDF_BLOCK_EVENT
        asdf_block_info_t *block;
    } payload;
} asdf_event_t;


/* Public API functions */
ASDF_EXPORT asdf_event_type_t asdf_event_type(asdf_event_t *event);
ASDF_EXPORT const char *asdf_event_comment(const asdf_event_t *event);
ASDF_EXPORT const asdf_tree_info_t *asdf_event_tree_info(const asdf_event_t *event);
ASDF_EXPORT const asdf_block_info_t *asdf_event_block_info(const asdf_event_t *event);
ASDF_EXPORT int asdf_event_iterate(asdf_parser_t *parser, asdf_event_t *event);
ASDF_EXPORT const char *asdf_event_type_name(asdf_event_type_t event_type);
ASDF_EXPORT void asdf_event_print(const asdf_event_t *event, FILE *file, bool verbose);
ASDF_EXPORT void asdf_event_destroy(asdf_parser_t *parser, asdf_event_t *event);
