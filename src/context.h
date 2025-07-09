/**
 * Context object shared between different libasdf internal structures, mostly used by the parser
 * but can also be used by streams, etc.  For a single ASDF file the same context is shared between
 * all internals.
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdatomic.h>

#include "util.h"


typedef enum {
    ASDF_ERROR_NONE,
    ASDF_ERROR_STATIC,
    ASDF_ERROR_HEAP,
} asdf_error_type_t;


typedef struct {
    atomic_uint refcount;
    asdf_error_type_t error_type;
    const char *error;
} asdf_context_t;


/**
 * Base for all libasdf objects that host the `asdf_context_t` object
 */
typedef struct {
    asdf_context_t *ctx;
} asdf_base_t;


ASDF_LOCAL asdf_context_t *asdf_context_create(void);
ASDF_LOCAL void asdf_context_destroy(asdf_context_t *ctx);
ASDF_LOCAL void asdf_context_retain(asdf_context_t *ctx);
ASDF_LOCAL void asdf_context_release(asdf_context_t *ctx);


#ifdef DEBUG
#define __ASDF_GET_CONTEXT(obj) \
    static_assert( \
        offsetof(typeof(*(obj)), base) == 0, "object must have asdf_base_t as first member"); \
    asdf_context_t *__ctx = ((asdf_base_t *)(obj))->ctx;
#else
#define __ASDF_GET_CONTEXT(obj) asdf_context_t *__ctx = ((asdf_base_t *)(obj))->ctx;
#endif
