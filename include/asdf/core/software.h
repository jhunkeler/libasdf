/* Data type an extension for http://stsci.edu/schemas/asdf/core/software-1.0.0 schema */
#ifndef ASDF_CORE_SOFTWARE_H
#define ASDF_CORE_SOFTWARE_H

typedef struct {
    const char *name;
    const char *author;
    const char *homepage;
    const char *version;
} asdf_software_t;

#endif /* ASDF_CORE_SOFTWARE_H */

