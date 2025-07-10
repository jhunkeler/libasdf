#include <stdarg.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "context.h"
#include "log.h"
#include "util.h"


#define ANSI_RESET "\033[0m"
#define ANSI_DIM "\033[2m"
#define COLOR_BRIGHT_BLUE "\033[94m"
#define COLOR_CYAN "\033[36m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED "\033[31m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_DIM_GREY "\033[90m"
#define COLOR(color, str) color str ANSI_RESET
#define DIM(str) ANSI_DIM str ANSI_RESET

#define ASDF_LOG_LEVEL_ENV_VAR "ASDF_LOG_LEVEL"


static const char *level_names[] = {"NONE", "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};


_Static_assert(
    (ASDF_LOG_FATAL + 1) == (sizeof(level_names) / sizeof(level_names[0])),
    "Mismatch between log level enum and level_strings array");


#ifdef ASDF_LOG_COLOR
static const char *level_colors[] = {
    "", COLOR_BRIGHT_BLUE, COLOR_CYAN, COLOR_GREEN, COLOR_YELLOW, COLOR_RED, COLOR_MAGENTA};
_Static_assert(
    (ASDF_LOG_FATAL + 1) == (sizeof(level_colors) / sizeof(level_colors[0])),
    "Mismatch between log level enum and level_strings array");
#endif


static asdf_log_level_t asdf_log_level_from_string(const char *s) {
    for (unsigned int idx = 1; idx < sizeof(level_names); idx++) {
        if (0 == strcasecmp(s, level_names[idx]))
            return (asdf_log_level_t)idx;
    }

    // Fallback
    return ASDF_LOG_DEFAULT_LEVEL;
}


asdf_log_level_t asdf_log_level_from_env() {
    const char *env = getenv(ASDF_LOG_LEVEL_ENV_VAR);

    if (!env)
        return ASDF_LOG_DEFAULT_LEVEL;

    return asdf_log_level_from_string(env);
}


void asdf_log(
    asdf_context_t *ctx,
    asdf_log_level_t level,
    const char *file,
    int lineno,
    const char *fmt,
    ...) {
    if (UNLIKELY(!ctx))
        return;

    if (ctx->log.level > level)
        return;

    va_list args;
    va_start(args, fmt);
#ifdef ASDF_LOG_COLOR
    fprintf(
        ctx->log.stream,
        DIM("[") COLOR("%s", "%s")
            DIM("]") " " COLOR(COLOR_DIM_GREY, "(" PACKAGE_NAME ")%s:%d:") " ",
        level_colors[level],
        level_names[level],
        file,
        lineno);
#else
    fprintf(ctx->log.stream, "[%-5s] (" PACKAGE_NAME ")%s:%d: ", level_names[level], file, lineno);
#endif

    // Bug in clang-tidy: https://github.com/llvm/llvm-project/issues/40656
    // Should be fixed in newer versions though...
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    vfprintf(ctx->log.stream, fmt, args);
    fprintf(ctx->log.stream, "\n");
    va_end(args);
}
