/**
 * Useful utilities for extension / schema parsing (internal header)
 *
 * Includes the public header and adds internal-only items.
 */

#pragma once

#include "asdf/extension_util.h" // IWYU pragma: export

#include "util.h"
#include "value.h"


/* Internal override: use UNLIKELY() for the error-check macro */
#undef ASDF_IS_ERR
#define ASDF_IS_ERR(err) (UNLIKELY(!ASDF_IS_OK((err))))
