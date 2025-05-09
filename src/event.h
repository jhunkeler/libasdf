/**
 * Parser events
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libfyaml.h>

#include "block.h"
#include "parse.h"


#define ASDF_EVENT_TYPES(X) \
    X(ASDF_BEGIN_EVENT) \
    X(ASDF_COMMENT_EVENT) \
    X(ASDF_ASDF_VERSION_EVENT) \
    X(ASDF_STANDARD_VERSION_EVENT) \
    X(ASDF_YAML_VERSION_EVENT) \
    X(ASDF_YAML_EVENT) \
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


static const char* const asdf_event_type_names[] = {
#define X(name) #name,
    ASDF_EVENT_TYPES(X)
#undef X
};


typedef struct fy_event asdf_yaml_event_t;


typedef struct {
    char *version;
} asdf_version_t;


typedef struct asdf_event {
    asdf_event_type_t type;
    union {
        asdf_block_info_t *block;
        asdf_yaml_event_t *yaml;
        asdf_version_t *version;
    } payload;
} asdf_event_t;


/* Public API functions */
int asdf_event_iterate(asdf_parser_t *parser, asdf_event_t *event);
const char* asdf_event_type_name(asdf_event_type_t event_type);
void asdf_event_print(const asdf_event_t *event, FILE *file, bool verbose);
void asdf_event_destroy(asdf_parser_t *parser, asdf_event_t *event);
