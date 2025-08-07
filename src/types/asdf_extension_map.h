/**
 * STC hash map mapping tag names to associated ASDF extensions
 */
#pragma once

#include <stc/cstr.h>

#include "../extension_registry.h"

#define i_type asdf_extension_map
#define i_keypro cstr
#define i_val asdf_extension_t *
#include <stc/hmap.h>

typedef asdf_extension_map asdf_extension_map_t;
