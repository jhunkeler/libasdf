#include <stdlib.h>

#include "context.h"
#include "error.h"


asdf_context_t *asdf_context_create() {
    asdf_context_t *ctx = malloc(sizeof(asdf_context_t));

    if (!ctx)
        return ctx;

    ctx->error = NULL;
    ctx->error_type = ASDF_ERROR_NONE;
    return ctx;
}


void asdf_context_destroy(asdf_context_t *ctx) {
    if (!ctx)
        return;

    if (ctx->error_type == ASDF_ERROR_HEAP)
        free((void *)ctx->error);
}
