#include <ctype.h>

#include "parse_util.h"

const char *asdf_standard_comment = "#ASDF_STANDARD ";
const char *asdf_version_comment = "#ASDF ";
const char *asdf_yaml_directive = "%YAML 1.1";


static const char *const parser_error_messages[] = {
    [ASDF_ERR_NONE] = NULL,
    [ASDF_ERR_UNKNOWN_STATE] = "Unknown parser state",
    [ASDF_ERR_UNEXPECTED_EOF] = "Unexpected end of file",
    [ASDF_ERR_INVALID_ASDF_HEADER] = "Invalid ASDF header",
    [ASDF_ERR_INVALID_BLOCK_HEADER] = "Invalid block header",
    [ASDF_ERR_BLOCK_MAGIC_MISMATCH] = "Block magic mismatch",
    [ASDF_ERR_YAML_PARSER_INIT_FAILED] = "YAML parser initialization failed",
    [ASDF_ERR_YAML_PARSE_FAILED] = "YAML parsing failed",
    [ASDF_ERR_OUT_OF_MEMORY] = "Out of memory",
};


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

    if (strncmp(buf, "%YAML ", 6) != 0)
        return false;

    const char *version = buf + 6;
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
