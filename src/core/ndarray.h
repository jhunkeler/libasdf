#pragma once

#define ASDF_CORE_NDARRAY_INTERNAL
#include "asdf/core/ndarray.h" // IWYU pragma: export


typedef struct {
    asdf_block_t *block;
    asdf_file_t *file;
    /**
     * Optional compressor name to set on the block for the ndarray when
     * writing a new ndarray
     */
    const char *write_compression;
    /* User-provided data array for new ndarrays not written to a file */
    void *data;
    /* Cloned YAML sequence for inline ndarrays; non-NULL iff this is an
     * inline ndarray whose data has not yet been parsed into a C array */
    asdf_sequence_t *inline_data;
    /* True iff data was malloc'd during lazy inline parsing (not mmap'd) */
    bool data_is_inline;
} asdf_ndarray_internal_t;


/** Internal definition of the asdf_ndarray_t type with extended internal fields */
typedef struct asdf_ndarray {
    size_t source;
    uint32_t ndim;
    uint64_t *shape;
    asdf_datatype_t datatype;
    asdf_byteorder_t byteorder;
    uint64_t offset;
    int64_t *strides;

    // Internal fields
    asdf_ndarray_internal_t *internal;
} asdf_ndarray_t;
