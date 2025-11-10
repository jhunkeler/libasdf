/**
 * Internal routines for fitswcs_imaging
 */

#include <asdf/file.h>
#define ASDF_GWCS_INTERNAL
#include <asdf/gwcs/wcs.h>
#undef ASDF_GWCS_INTERNAL
#include <asdf/util.h>


ASDF_LOCAL asdf_gwcs_err_t asdf_gwcs_fits_get_ctype(
    asdf_file_t *file, asdf_gwcs_t *gwcs, const char **ctype1, const char **ctype2);
