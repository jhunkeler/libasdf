/** Tag parsing utilities */
#pragma once

#include <asdf/extension.h>
#include <asdf/util.h>


/**
 * Parse a tag string of the form "name" or "name-version" into an
 * ``asdf_tag_t``.  Returns NULL on OOM.  The caller owns the result
 * and must free it with ``asdf_tag_free``.
 */
ASDF_LOCAL asdf_tag_t *asdf_tag_parse(const char *tag);

/** Free a tag returned by ``asdf_tag_parse``. */
ASDF_LOCAL void asdf_tag_free(asdf_tag_t *tag);
