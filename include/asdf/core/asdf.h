#ifndef ASDF_CORE_ASDF_H
#define ASDF_CORE_ASDF_H

#include <asdf/core/history_entry.h>
#include <asdf/core/software.h>
#include <asdf/extension.h>
#include <asdf/util.h>

ASDF_EXPORT extern asdf_software_t libasdf_software;


#define ASDF_STANDARD_TAG_PREFIX "stsci.edu:asdf/"
#define ASDF_CORE_TAG_PREFIX ASDF_STANDARD_TAG_PREFIX "core/"


typedef struct {
    asdf_software_t *asdf_library;
    asdf_history_entry_t **history;
} asdf_meta_t;


#endif /* ASDF_CORE_ASDF_H */
