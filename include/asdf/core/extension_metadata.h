/* Declarations for the extension_metadata-1.0.0 core resource */
#ifndef ASDF_CORE_EXTENSION_METADATA_H
#define ASDF_CORE_EXTENSION_METADATA_H

#include <asdf/core/software.h>
#include <asdf/extension.h>
#include <asdf/value.h>


typedef struct {
    const char *extension_class;
    const asdf_software_t *package;
    asdf_value_t *metadata;
} asdf_extension_metadata_t;


ASDF_DECLARE_EXTENSION(extension_metadata, asdf_extension_metadata_t);


#endif /* ASDF_CORE_EXTENSION_METADATA_H */
