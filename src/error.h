#pragma once

#include "context.h"


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
} asdf_error_code_t;


/* Internal error helper functions */
ASDF_LOCAL void asdf_context_error_set(asdf_context_t *ctx, const char *fmt, ...);
ASDF_LOCAL void asdf_context_error_set_oom(asdf_context_t *ctx);
ASDF_LOCAL void asdf_context_error_set_static(asdf_context_t *ctx, const char *error);
ASDF_LOCAL void asdf_context_error_set_common(asdf_context_t *ctx, asdf_error_code_t code);

/**
 * Macros for setting errors on arbitrary ASDF base types
 *
 * These should be used more generally than the ``asdf_context_error_*`` functions.
 */
#define ASDF_ERROR(obj, fmt, ...) \
    do { \
        __ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set(__ctx, (fmt), ...); \
    } while (0)


#define ASDF_ERROR_OOM(obj) \
    do { \
        __ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_oom(__ctx); \
    } while (0)


#define ASDF_ERROR_STATIC(obj, error) \
    do { \
        __ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_static(__ctx, (error)); \
    } while (0)


#define ASDF_ERROR_COMMON(obj, code) \
    do { \
        __ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_common(__ctx, (code)); \
    } while (0)
