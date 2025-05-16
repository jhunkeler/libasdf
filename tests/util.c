/**
 * Utilities for unit tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef REFERENCE_FILES_DIR
#error "REFERENCE_FILES_DIR not defined"
#endif


const char* get_reference_file_path(const char* relative_path) {
    static char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", REFERENCE_FILES_DIR, relative_path);
    return full_path;
}
