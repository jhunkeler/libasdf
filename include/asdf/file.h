#ifndef ASDF_FILE_H
#define ASDF_FILE_H

#include <stddef.h>
#include <stdio.h>

#include <asdf/util.h>

/* Forward declaration */
typedef struct asdf_file asdf_file_t;

ASDF_EXPORT asdf_file_t *asdf_open_file(const char *filename, const char* mode);
ASDF_EXPORT asdf_file_t *asdf_open_fp(FILE *fp, const char *filename);
ASDF_EXPORT asdf_file_t *asdf_open_mem(const void *buf, size_t size);
ASDF_EXPORT void asdf_close(asdf_file_t *file);

#endif /* ASDF_FILE_H */
