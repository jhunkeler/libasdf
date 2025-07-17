#ifndef ASDF_FILE_H
#define ASDF_FILE_H

#include <stddef.h>
#include <stdio.h>

#include <asdf/util.h>
#include <asdf/value.h>

/* Forward declaration */
typedef struct asdf_file asdf_file_t;

ASDF_EXPORT asdf_file_t *asdf_open_file(const char *filename, const char* mode);
ASDF_EXPORT asdf_file_t *asdf_open_fp(FILE *fp, const char *filename);
ASDF_EXPORT asdf_file_t *asdf_open_mem(const void *buf, size_t size);
ASDF_EXPORT void asdf_close(asdf_file_t *file);

/* As a convenience, asdf_open() is available as an alias for asdf_open_file */
inline asdf_file_t *asdf_open(const char *filename, const char *mode) {
    return asdf_open_file(filename, mode);
}

/* Value getters */
ASDF_EXPORT asdf_value_t *asdf_get(asdf_file_t *file, const char *path);

#endif /* ASDF_FILE_H */
