/**
 * Miscellaneous utility constants and subroutines for parse
 *
 * Split into a separate source module for decluttering and ease of testing
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdbool.h>

#include "block.h"
#include "parse.h"
#include "util.h"


ASDF_LOCAL const char *asdf_standard_comment;
ASDF_LOCAL const char *asdf_version_comment;
ASDF_LOCAL const char *asdf_yaml_directive;

/* Internal error helper functions */
ASDF_LOCAL void asdf_parser_set_oom_error(asdf_parser_t *parser);
ASDF_LOCAL void asdf_parser_set_error(asdf_parser_t *parser, const char *fmt, ...);
ASDF_LOCAL void asdf_parser_set_static_error(asdf_parser_t *parser, const char *error);
ASDF_LOCAL void asdf_parser_set_common_error(asdf_parser_t *parser, asdf_parser_error_code_t code);


/**
 * Returns `true` if the given buffer begins with the ASDF block magic
 */
static inline bool is_block_magic(const char *buf, size_t len) {
    if (len < ASDF_BLOCK_MAGIC_SIZE)
        return false;

    return memcmp(buf, ASDF_BLOCK_MAGIC, ASDF_BLOCK_MAGIC_SIZE) == 0;
}


ASDF_LOCAL bool is_generic_yaml_directive(const char *buf, size_t len);


#define ASDF_YAML_DIRECTIVE_SIZE 9


static inline bool is_yaml_1_1_directive(const char *buf, size_t len) {
    size_t dirlen = strlen(asdf_yaml_directive);
    if (len < ASDF_YAML_DIRECTIVE_SIZE + 1)
        return false;

    if (strncmp(buf, asdf_yaml_directive, ASDF_YAML_DIRECTIVE_SIZE) != 0)
        return false;

    if (buf[9] == '\n' || (buf[9] == '\r' && len >= 11 && buf[10] == '\n'))
        return true;

    return false;
}


/**
 * Returns `true` if the given buffer contains a valid %YAML directive line.
 *
 * First checks the happy path for containing exactly ``%YAML 1.1\r?\n`` as required
 * by the ASDF 1.6.0 standard.  Then fall back on accepting any non-standard (but still
 * syntactically valid) ``%YAML`` directive.
 */
static bool is_yaml_directive(const char *buf, size_t len) {
    if (is_yaml_1_1_directive(buf, len))
        return true;

    return is_generic_yaml_directive(buf, len);
}
