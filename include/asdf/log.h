#ifndef ASDF_LOG_H
#define ASDF_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <asdf/util.h>

typedef enum {
    ASDF_LOG_NONE = 0,
    ASDF_LOG_TRACE,
    ASDF_LOG_DEBUG,
    ASDF_LOG_INFO,
    ASDF_LOG_WARN,
    ASDF_LOG_ERROR,
    ASDF_LOG_FATAL,
} asdf_log_level_t;

#define ASDF_LOG_NUM_LEVELS (ASDF_LOG_FATAL + 1)


#define ASDF_LOG_FIELDS(X) \
    X(ASDF_LOG_FIELD_LEVEL, 0) \
    X(ASDF_LOG_FIELD_PACKAGE, 1) \
    X(ASDF_LOG_FIELD_FILE, 2) \
    X(ASDF_LOG_FIELD_LINE, 3) \
    X(ASDF_LOG_FIELD_MSG, 4)


#define ASDF_LOG_FIELD_ALL \
    (ASDF_LOG_FIELD_LEVEL | ASDF_LOG_FIELD_PACKAGE | ASDF_LOG_FIELD_FILE | ASDF_LOG_FIELD_LINE | \
     ASDF_LOG_FIELD_MSG)


typedef enum {
// clang-format off
#define X(flag, bit) flag = (1UL << (bit)),
    ASDF_LOG_FIELDS(X)
#undef X
    // clang-format on
} asdf_log_field_t;


typedef uint64_t asdf_log_fields_t;


typedef struct {
    FILE *stream; /* Currently just defaults to stderr */
    asdf_log_level_t level;
    /**
     * Basic configuration of what fields are included in the standard log
     * formatter
     *
     * Log formatting is not fully customizable yet but specific fields may be
     * enabled / disabled.
     */
    asdf_log_fields_t fields;
    bool no_color;
} asdf_log_cfg_t;


/* Forward declaration — full definition in <asdf/file.h> */
typedef struct asdf_file asdf_file_t;


ASDF_BEGIN_DECLS

/**
 * Public logging API for extension authors.
 *
 * Log a message associated with the given file's log configuration.
 * Use asdf_value_file(value) to get the file pointer from a value.
 */
ASDF_EXPORT void asdf_file_log(
    const asdf_file_t *file,
    asdf_log_level_t level,
    const char *src_file,
    int lineno,
    const char *fmt,
    ...);

#ifdef ASDF_LOG_ENABLED
#define ASDF_LOG(file, level, ...) \
    asdf_file_log((file), (level), __FILE__, __LINE__, __VA_ARGS__)
#else
#define ASDF_LOG(file, level, ...) ((void)0)
#endif

ASDF_END_DECLS


#endif /* ASDF_LOG_H */
