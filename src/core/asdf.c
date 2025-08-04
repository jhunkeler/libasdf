#include <asdf/core/asdf.h>
#include <asdf/core/software.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


asdf_software_t libasdf_software = {
    .name = PACKAGE_NAME,
    .version = PACKAGE_VERSION,
    .homepage = PACKAGE_URL,
    .author = "The libasdf Developers"};
