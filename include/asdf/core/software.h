/* Data type an extension for http://stsci.edu/schemas/asdf/core/software-1.0.0 schema */
#ifndef ASDF_CORE_SOFTWARE_H
#define ASDF_CORE_SOFTWARE_H

#include <asdf/extension.h>


/* NOTE: asdf_software_t is defined in asdf/extension.h due to the circularity between them */
ASDF_DECLARE_EXTENSION(software, asdf_software_t);


#endif /* ASDF_CORE_SOFTWARE_H */
