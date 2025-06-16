#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdint.h>

#include <libfyaml.h>

#include <asdf/parse.h>

#include "event.h"
#include "stream.h"
#include "util.h"


#define ASDF_COMMENT_CHAR '#'
#define ASDF_ASDF_VERSION_BUFFER_SIZE 16
#define ASDF_STANDARD_VERSION_BUFFER_SIZE 16
#define ASDF_PARSER_READ_BUFFER_INIT_SIZE 512


typedef enum {
    ASDF_PARSER_STATE_INITIAL,
    ASDF_PARSER_STATE_ASDF_VERSION,
    ASDF_PARSER_STATE_STANDARD_VERSION,
    ASDF_PARSER_STATE_COMMENT,
    // This state comes after comment parsing, and tries to determine
    // if we have a YAML tree or of the next thing in the file is a block
    ASDF_PARSER_STATE_TREE_OR_BLOCK,
    // This state means we have found the start of the YAML tree
    ASDF_PARSER_STATE_TREE,
    // This state means we are generating YAML events
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
    ASDF_ERR_STREAM_INIT_FAILED,
    ASDF_ERR_INVALID_ASDF_HEADER,
    ASDF_ERR_UNEXPECTED_EOF,
    ASDF_ERR_INVALID_BLOCK_HEADER,
    ASDF_ERR_BLOCK_MAGIC_MISMATCH,
    ASDF_ERR_YAML_PARSER_INIT_FAILED,
    ASDF_ERR_YAML_PARSE_FAILED,
    ASDF_ERR_OUT_OF_MEMORY,
} asdf_parser_error_code_t;


typedef struct asdf_parser_tree_info {
    off_t start;
    off_t end;
    uint8_t *buf;
    size_t size;
    // Found the full YAML tree
    bool found;
} asdf_parser_tree_info_t;


/* Structure for asdf_event_t freelist */
struct asdf_event_p {
    asdf_event_t event;
    struct asdf_event_p *next;
};


typedef struct asdf_parser {
    const asdf_parser_cfg_t *config;
    asdf_parser_state_t state;
    asdf_stream_t *stream;
    asdf_error_type_t error_type;
    const char *error;
    struct asdf_event_p *event_freelist;
    struct asdf_event_p *current_event_p;
    char asdf_version[ASDF_ASDF_VERSION_BUFFER_SIZE];
    char standard_version[ASDF_STANDARD_VERSION_BUFFER_SIZE];
    struct fy_parser *yaml_parser;
    asdf_parser_tree_info_t tree;
    size_t found_blocks;
    bool done;
} asdf_parser_t;


static inline bool asdf_parser_has_opt(asdf_parser_t *parser, asdf_parser_opt_t opt) {
    assert(parser);
    return (parser->config && ((parser->config->flags & opt) == opt));
}
