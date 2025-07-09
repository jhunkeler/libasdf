#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "context.h"
#include "error.h"
#include "log.h"
#include "util.h"


asdf_context_t *asdf_context_create() {
    asdf_context_t *ctx = malloc(sizeof(asdf_context_t));

    if (!ctx)
        return ctx;

    atomic_init(&ctx->refcount, 1);
    ctx->error = NULL;
    ctx->error_type = ASDF_ERROR_NONE;

    /* Initialize log configuration to defaults; later should allow overriding */
    ctx->log.level = ASDF_LOG_DEFAULT_LEVEL;
    ctx->log.stream = stderr;
    return ctx;
}


void asdf_context_destroy(asdf_context_t *ctx) {
    if (!ctx)
        return;

    if (ctx->error_type == ASDF_ERROR_HEAP)
        free((void *)ctx->error);

    free(ctx);
}


void asdf_context_retain(asdf_context_t *ctx) {
    if (UNLIKELY(!ctx))
        return;

    atomic_fetch_add_explicit(&ctx->refcount, 1, memory_order_relaxed);
}


void asdf_context_release(asdf_context_t *ctx) {
    if (UNLIKELY(!ctx))
        return;
    if (atomic_fetch_sub_explicit(&ctx->refcount, 1, memory_order_acq_rel) == 1)
        asdf_context_destroy(ctx);
}
