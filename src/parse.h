#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libfyaml.h>


extern const char *asdf_standard_comment;
extern const char *asdf_version_comment;


#define ASDF_ASDF_VERSION_BUFFER_SIZE 16
#define ASDF_STANDARD_VERSION_BUFFER_SIZE 16
#define ASDF_PARSER_READ_BUFFER_SIZE 512


typedef enum {
    ASDF_PARSER_STATE_INITIAL,
    ASDF_PARSER_STATE_ASDF_VERSION,
    ASDF_PARSER_STATE_STANDARD_VERSION,
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


typedef struct asdf_parser {
    asdf_parser_state_t state;
    asdf_error_type_t error_type;
    const char *error;
    FILE *file;
    char asdf_version[ASDF_ASDF_VERSION_BUFFER_SIZE];
    char standard_version[ASDF_STANDARD_VERSION_BUFFER_SIZE];
    struct fy_parser *yaml_parser;
    off_t yaml_start;
    off_t yaml_end;
    size_t found_blocks;
    uint8_t read_buffer[ASDF_PARSER_READ_BUFFER_SIZE];
} asdf_parser_t;


/* Forward declaration for asdf_event_t */
typedef struct asdf_event asdf_event_t;


/* Public API functions */
int asdf_parser_init(asdf_parser_t *parser);
int asdf_parser_set_input_file(asdf_parser_t *parser, FILE *file, const char *name);
int asdf_parser_parse(asdf_parser_t *parser, asdf_event_t *event);
void asdf_parser_destroy(asdf_parser_t *parser);
bool asdf_parser_has_error(const asdf_parser_t *parser);
const char *asdf_parser_get_error(const asdf_parser_t *parser);
