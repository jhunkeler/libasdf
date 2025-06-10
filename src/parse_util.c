#include <assert.h>
#include <ctype.h>

#include "block.h"
#include "parse_util.h"
#include "stream.h"

const char *asdf_standard_comment = "#ASDF_STANDARD ";
const char *asdf_version_comment = "#ASDF ";


#define _ASDF_YAML_DIRECTIVE_PREFIX "%YAML "
#define _ASDF_YAML_DOCUMENT_END_MARKER "\n..."


const char *ASDF_YAML_DIRECTIVE_PREFIX = _ASDF_YAML_DIRECTIVE_PREFIX;
const char *ASDF_YAML_DIRECTIVE = "%YAML 1.1";
const char *ASDF_YAML_DOCUMENT_END_MARKER = _ASDF_YAML_DOCUMENT_END_MARKER;


#define TOKEN(t) ((const uint8_t*)(t))


const asdf_parse_token_t asdf_parse_tokens[] = {
    [ASDF_YAML_DIRECTIVE_TOK] = {TOKEN(_ASDF_YAML_DIRECTIVE_PREFIX), ASDF_YAML_DIRECTIVE_PREFIX_SIZE},
    [ASDF_YAML_DOCUMENT_END_TOK] = {TOKEN(_ASDF_YAML_DOCUMENT_END_MARKER), ASDF_YAML_DOCUMENT_END_MARKER_SIZE},
    [ASDF_BLOCK_MAGIC_TOK] = {ASDF_BLOCK_MAGIC, ASDF_BLOCK_MAGIC_SIZE}
};


static const char *const parser_error_messages[] = {
    [ASDF_ERR_NONE] = NULL,
    [ASDF_ERR_UNKNOWN_STATE] = "Unknown parser state",
    [ASDF_ERR_STREAM_INIT_FAILED] = "Failed to initialize stream",
    [ASDF_ERR_UNEXPECTED_EOF] = "Unexpected end of file",
    [ASDF_ERR_INVALID_ASDF_HEADER] = "Invalid ASDF header",
    [ASDF_ERR_INVALID_BLOCK_HEADER] = "Invalid block header",
    [ASDF_ERR_BLOCK_MAGIC_MISMATCH] = "Block magic mismatch",
    [ASDF_ERR_YAML_PARSER_INIT_FAILED] = "YAML parser initialization failed",
    [ASDF_ERR_YAML_PARSE_FAILED] = "YAML parsing failed",
    [ASDF_ERR_OUT_OF_MEMORY] = "Out of memory",
};


/* Parsing helpers */

/**
 * Wrapper around asdf_stream_scan with an interface that returns a matched token as
 * a value of asdf_parse_token_id_t
 */
int asdf_parser_scan_tokens(
        asdf_parser_t *parser, const asdf_parse_token_id_t *token_ids, size_t *match_offset,
        asdf_parse_token_id_t *match_token) {
    assert(token_ids);
    asdf_parse_token_id_t used_token_ids[ASDF_LAST_TOK];
    asdf_parse_token_t tokens[ASDF_LAST_TOK] = {0};
    size_t n_tokens = 0;

    while (token_ids[n_tokens] < ASDF_LAST_TOK) {
        used_token_ids[n_tokens] = token_ids[n_tokens];
        tokens[n_tokens] = asdf_parse_tokens[token_ids[n_tokens]];
        n_tokens++;
    }

    if (n_tokens == 0)
        return 1;

    const uint8_t *token_vals[n_tokens];
    size_t token_lens[n_tokens];
    size_t matched_idx = 0;

    for (size_t idx = 0; idx < n_tokens; idx++) {
        token_vals[idx] = tokens[idx].tok;
        token_lens[idx] = tokens[idx].tok_len;
    }

    int ret = asdf_stream_scan(parser->stream, token_vals, token_lens, n_tokens, match_offset,
                               &matched_idx);
    if (0 == ret) {
        *match_token = used_token_ids[matched_idx];
    }

    return ret;
}


/* Error helpers */
#define ASDF_ERR(code) (parser_error_messages[(code)])


void asdf_parser_set_oom_error(asdf_parser_t *parser) {
    parser->error = ASDF_ERR(ASDF_ERR_OUT_OF_MEMORY);
    parser->error_type = ASDF_ERROR_STATIC;
}


void asdf_parser_set_error(asdf_parser_t *parser, const char *fmt, ...) {
    va_list args;
    int size;
    assert(parser);

    if (parser->error_type == ASDF_ERROR_HEAP)
        // Heap-allocated errors can be safely cast to (void *)
        // and freed.
        free((void *)parser->error);

    parser->error = NULL;
    parser->state = ASDF_PARSER_STATE_ERROR;

    va_start(args, fmt);

    // Bug in clang-tidy: https://github.com/llvm/llvm-project/issues/40656
    // Should be fixed in newer versions though...
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // Ensure space for null termination
    char *error = malloc(size + 1);

    if (!error) {
        asdf_parser_set_oom_error(parser);
        return;
    }

    va_start(args, fmt);
    vsnprintf(error, size + 1, fmt, args);
    va_end(args);
    parser->error = error; // implicit char* -> const char*; OK
    parser->error_type = ASDF_ERROR_HEAP;
}


void asdf_parser_set_static_error(asdf_parser_t *parser, const char *error) {
    if (parser->error_type == ASDF_ERROR_HEAP)
        free((void *)parser->error);

    parser->error = error;
    parser->error_type = ASDF_ERROR_STATIC;
    parser->state = ASDF_PARSER_STATE_ERROR;
}


void inline asdf_parser_set_common_error(asdf_parser_t *parser, asdf_parser_error_code_t code) {
    asdf_parser_set_static_error(parser, ASDF_ERR(code));
}


bool is_generic_yaml_directive(const char *buf, size_t len) {
    if (len < ASDF_YAML_DIRECTIVE_SIZE + 1)
        return false;

    if (strncmp(buf, ASDF_YAML_DIRECTIVE_PREFIX, ASDF_YAML_DIRECTIVE_PREFIX_SIZE) != 0)
        return false;

    const char *version = buf + ASDF_YAML_DIRECTIVE_PREFIX_SIZE;
    const char *end = buf + len;

    // Parse X.Y where X and Y are one or more ASCII digits
    const char *p = version;
    if (!isdigit((unsigned char)*p))
        return false;

    while (isdigit((unsigned char)*p) && p < end)
        ++p;

    if (p < end && *p++ != '.')
        return false;

    if (!isdigit((unsigned char)*p))
        return false;

    while (isdigit((unsigned char)*p) && p < end)
        ++p;

    // Accept optional \r before \n
    if (*p == '\r' && p < end)
        ++p;

    return *p == '\n';
}


