#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

#include <libfyaml.h>


extern const char *asdf_standard_comment;
extern const char *asdf_version_comment;

#define ASDF_COMMENT_CHAR '#'
#define ASDF_ASDF_VERSION_BUFFER_SIZE 16
#define ASDF_STANDARD_VERSION_BUFFER_SIZE 16
#define ASDF_PARSER_READ_BUFFER_INIT_SIZE 512


typedef enum {
    ASDF_PARSER_STATE_INITIAL,
    ASDF_PARSER_STATE_ASDF_VERSION,
    ASDF_PARSER_STATE_STANDARD_VERSION,
    ASDF_PARSER_STATE_COMMENT,
    ASDF_PARSER_STATE_YAML,
    ASDF_PARSER_STATE_BLOCK,
    ASDF_PARSER_STATE_PADDING,
    ASDF_PARSER_STATE_BLOCK_INDEX,
    ASDF_PARSER_STATE_END,
    ASDF_PARSER_STATE_ERROR
} asdf_parser_state_t;


typedef enum {
    ASDF_ERROR_NONE,
    ASDF_ERROR_STATIC,
    ASDF_ERROR_HEAP,
} asdf_error_type_t;


typedef enum {
    ASDF_ERR_NONE = 0,

    ASDF_ERR_UNKNOWN_STATE,
    ASDF_ERR_INVALID_ASDF_HEADER,
    ASDF_ERR_UNEXPECTED_EOF,
    ASDF_ERR_INVALID_BLOCK_HEADER,
    ASDF_ERR_BLOCK_MAGIC_MISMATCH,
    ASDF_ERR_YAML_PARSER_INIT_FAILED,
    ASDF_ERR_YAML_PARSE_FAILED,
    ASDF_ERR_OUT_OF_MEMORY,
} asdf_parser_error_code_t;


#define _ASDF_PARSER_OPTS(X) \
    X(ASDF_PARSER_OPT_EMIT_YAML_EVENTS, 0) \
    X(ASDF_PARSER_OPT_BUFFER_TREE, 1)


typedef enum {
// clang-format off
#define X(flag, bit) flag = (1UL << bit),
    _ASDF_PARSER_OPTS(X)
#undef X
// clang-format on
} asdf_parser_opt_t;


_Static_assert(ASDF_PARSER_OPT_BUFFER_TREE < (1UL << 63), "too many flags for 64-bit int");


typedef uint64_t asdf_parser_optflags_t;


typedef struct asdf_parser_cfg {
    asdf_parser_optflags_t flags;
} asdf_parser_cfg_t;


typedef struct asdf_parser {
    const asdf_parser_cfg_t *config;
    asdf_parser_state_t state;
    asdf_error_type_t error_type;
    const char *error;
    FILE *file;
    char asdf_version[ASDF_ASDF_VERSION_BUFFER_SIZE];
    char standard_version[ASDF_STANDARD_VERSION_BUFFER_SIZE];
    struct fy_parser *yaml_parser;
    off_t tree_start;
    off_t tree_end;
    char *tree_buffer;
    size_t found_blocks;
    uint8_t *read_buffer;
    size_t read_buffer_size;
    bool peek_last;
    bool done;
} asdf_parser_t;


/* Forward declaration for asdf_event_t */
typedef struct asdf_event asdf_event_t;


/* Public API functions */
int asdf_parser_init(asdf_parser_t *parser, asdf_parser_cfg_t *config);
int asdf_parser_set_input_file(asdf_parser_t *parser, FILE *file, const char *name);
int asdf_parser_parse(asdf_parser_t *parser, asdf_event_t *event);
void asdf_parser_destroy(asdf_parser_t *parser);
bool asdf_parser_has_error(const asdf_parser_t *parser);
const char *asdf_parser_get_error(const asdf_parser_t *parser);
