/** Internal utilities for parsing generic transforms */
#pragma once

#define ASDF_GWCS_INTERNAL
#include <asdf/gwcs/transform.h>
#undef ASDF_GWCS_INTERNAL
#include <asdf/util.h>
#include <asdf/value.h>

ASDF_LOCAL asdf_value_err_t
asdf_gwcs_transform_parse(asdf_value_t *value, asdf_gwcs_transform_t *transform);

/**
 * Release memory held by fields in an `asdf_gwcs_transform_t` and clear it in preparation
 * for releasing the transform's memory
 *
 * Does not free the transform struct's own memory.
 */
ASDF_LOCAL void asdf_gwcs_transform_clean(asdf_gwcs_transform_t *transform);
