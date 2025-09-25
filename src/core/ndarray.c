#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <asdf/core/asdf.h>
#define ASDF_CORE_NDARRAY_INTERNAL
#include <asdf/core/ndarray.h>
#undef ASDF_CORE_NDARRAY_INTERNAL
#include <asdf/extension.h>

#include "../file.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"
#include "ndarray_convert.h"


/* Internal definition of the asdf_ndarray_t type with extended internal fields */
typedef struct asdf_ndarray {
    size_t source;
    uint32_t ndim;
    uint64_t *shape;
    asdf_datatype_t datatype;
    asdf_byteorder_t byteorder;
    uint64_t offset;
    int64_t *strides;

    // Internal fields
    asdf_block_t *block;
    asdf_file_t *file;
} asdf_ndarray_t;


/* Helper to look up required properties and log a warning if missing */
static asdf_value_t *get_required_property(asdf_value_t *mapping, const char *name) {
    asdf_value_t *prop = asdf_mapping_get(mapping, name);
#ifdef ASDF_LOG_ENABLED
    if (!prop) {
        const char *path = asdf_value_path(prop);
        ASDF_LOG(
            mapping->file,
            ASDF_LOG_WARN,
            "required property %s missing from ndarray at %s",
            name,
            path);
    }
#endif
    return prop;
}


#ifdef ASDF_LOG_ENABLED
static void warn_invalid_shape(UNUSED(asdf_value_t *value)) {
}
#else
static void warn_invalid_shape(asdf_value_t *value) {
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "invalid shape for ndarray at %s; must be an array of"
        "positive integers",
        path);
}
#endif


#ifdef ASDF_LOG_ENABLED
static void warn_invalid_strides(UNUSED(asdf_value_t *value)) {
}
#else
static void warn_invalid_strides(asdf_value_t *value) {
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "invalid strides for ndarray at %s; must be an array of"
        "non-zero integers with the same length as shape",
        path);
}
#endif


#ifdef ASDF_LOG_ENABLED
static void warn_unsupported_datatype(UNUSED(asdf_value_t *value)) {
}
#else
static void warn_unsupported_datatype(asdf_value_t *value) {
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "unsupported datatype for ndarray at %s; please note "
        "that the current version only supports basic scalar numeric (non-string) "
        "datatypes",
        path);
}
#endif


asdf_datatype_t asdf_ndarray_deserialize_datatype(asdf_value_t *value) {
    if (!value)
        return ASDF_DATATYPE_UNKNOWN;

    const char *s = NULL;

    /* Parse string datatypes partially, but we don't currently store the string length; they are
     * not fully supported.  Structured datatypes are not supported at all but are at least
     * indicated as structured.
     */
    if (asdf_value_is_sequence(value)) {
        if (asdf_sequence_size(value) == 2) {
            asdf_value_t *stringlen = asdf_sequence_get(value, 1);
            if (!stringlen || !asdf_value_is_uint64(stringlen)) {
                // Maybe it is a record array but we don't fully parse them yet
                warn_unsupported_datatype(value);
                return ASDF_DATATYPE_RECORD;
            }

            asdf_value_t *datatype = asdf_sequence_get(value, 0);

            if (ASDF_VALUE_OK != asdf_value_as_string0(datatype, &s)) {
                warn_unsupported_datatype(value);
                return ASDF_DATATYPE_UNKNOWN;
            }

            if (strcmp(s, "ascii") == 0) {
                warn_unsupported_datatype(value);
                return ASDF_DATATYPE_ASCII;
            } else if (strcmp(s, "ucs4") == 0) {
                warn_unsupported_datatype(value);
                return ASDF_DATATYPE_UCS4;
            }
        }
        warn_unsupported_datatype(value);
        return ASDF_DATATYPE_RECORD;
    } else if (asdf_value_is_mapping(value)) {
        warn_unsupported_datatype(value);
        return ASDF_DATATYPE_RECORD;
    }

    if (ASDF_VALUE_OK != asdf_value_as_string0(value, &s)) {
        warn_unsupported_datatype(value);
        return ASDF_DATATYPE_UNKNOWN;
    }

    if (strncmp(s, "int", 3) == 0) {
        const char *p = s + 3;
        if (*p && strspn(p, "123468") == strlen(p)) {
            if (strcmp(p, "8") == 0)
                return ASDF_DATATYPE_INT8;
            if (strcmp(p, "16") == 0)
                return ASDF_DATATYPE_INT16;
            if (strcmp(p, "32") == 0)
                return ASDF_DATATYPE_INT32;
            if (strcmp(p, "64") == 0)
                return ASDF_DATATYPE_INT64;
        }
        goto unknown;
    }

    if (strncmp(s, "uint", 4) == 0) {
        const char *p = s + 4;
        if (*p && strspn(p, "123468") == strlen(p)) {
            if (strcmp(p, "8") == 0)
                return ASDF_DATATYPE_UINT8;
            if (strcmp(p, "16") == 0)
                return ASDF_DATATYPE_UINT16;
            if (strcmp(p, "32") == 0)
                return ASDF_DATATYPE_UINT32;
            if (strcmp(p, "64") == 0)
                return ASDF_DATATYPE_UINT64;
        }
        goto unknown;
    }

    if (strncmp(s, "float", 5) == 0) {
        const char *p = s + 5;
        if (*p && strspn(p, "12346") == strlen(p)) {
            if (strcmp(p, "16") == 0)
                return ASDF_DATATYPE_FLOAT16;
            if (strcmp(p, "32") == 0)
                return ASDF_DATATYPE_FLOAT32;
            if (strcmp(p, "64") == 0)
                return ASDF_DATATYPE_FLOAT64;
        }
        goto unknown;
    }

    if (strncmp(s, "complex", 7) == 0) {
        const char *p = s + 7;
        if (*p && strspn(p, "12468") == strlen(p)) {
            if (strcmp(p, "64") == 0)
                return ASDF_DATATYPE_COMPLEX64;
            if (strcmp(p, "128") == 0)
                return ASDF_DATATYPE_COMPLEX128;
        }
        goto unknown;
    }

    if (strcmp(s, "bool8") == 0)
        return ASDF_DATATYPE_BOOL8;

unknown : {
#ifdef ASDF_LOG_ENABLED
    const char *path = asdf_value_path(value);
    ASDF_LOG(value->file, ASDF_LOG_WARN, "unknown datatype for ndarray at %s: %s", path, s);
#endif
}
    return ASDF_DATATYPE_UNKNOWN;
}


const char *asdf_ndarray_datatype_to_string(asdf_datatype_t datatype) {
    switch (datatype) {
    case ASDF_DATATYPE_UNKNOWN:
        return "<unknown>";
    case ASDF_DATATYPE_INT8:
        return "int8";
    case ASDF_DATATYPE_UINT8:
        return "uint8";
    case ASDF_DATATYPE_INT16:
        return "int16";
    case ASDF_DATATYPE_UINT16:
        return "uint16";
    case ASDF_DATATYPE_INT32:
        return "int32";
    case ASDF_DATATYPE_UINT32:
        return "uint32";
    case ASDF_DATATYPE_INT64:
        return "int64";
    case ASDF_DATATYPE_UINT64:
        return "uint64";
    case ASDF_DATATYPE_FLOAT16:
        return "float16";
    case ASDF_DATATYPE_FLOAT32:
        return "float32";
    case ASDF_DATATYPE_FLOAT64:
        return "float64";
    case ASDF_DATATYPE_COMPLEX64:
        return "complex64";
    case ASDF_DATATYPE_COMPLEX128:
        return "complex128";
    case ASDF_DATATYPE_BOOL8:
        return "bool8";
    // TODO: The remaining cases will be more usefully stringified
    // From an asdf_datatype_t object that contains their additional data...
    // Should probably refactor before first release to avoid ABI incompatibility
    // See issue #50
    case ASDF_DATATYPE_ASCII:
        return "ascii";
    case ASDF_DATATYPE_UCS4:
        return "ucs4";
    case ASDF_DATATYPE_RECORD:
        return "<record>";
    }
    UNREACHABLE();
}


asdf_byteorder_t asdf_ndarray_deserialize_byteorder(asdf_value_t *value) {
    if (!value) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(
            value->file,
            ASDF_LOG_WARN,
            "byteorder not specified for ndarray at %s; "
            "defaulting to \"little\"",
            path);
#endif
        return ASDF_BYTEORDER_LITTLE;
    }

    const char *s = NULL;

    if (ASDF_VALUE_OK != asdf_value_as_string0(value, &s)) {
        goto invalid;
    }

    if (s && (strcmp(s, "little") == 0))
        return ASDF_BYTEORDER_LITTLE;
    else if (s && (strcmp(s, "big") == 0))
        return ASDF_BYTEORDER_BIG;

invalid : {
#ifdef ASDF_LOG_ENABLED
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "invalid byteorder for ndarray at %s; "
        "defaulting to \"little\"",
        path);
#endif
}
    return ASDF_BYTEORDER_LITTLE;
}


static asdf_value_err_t asdf_ndarray_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    uint64_t source = 0;
    uint32_t ndim = 0; /* Will be determined from the "shape" property */
    uint64_t *shape = NULL;
    asdf_datatype_t datatype = ASDF_DATATYPE_UNKNOWN;
    asdf_byteorder_t byteorder = ASDF_BYTEORDER_LITTLE;
    uint64_t offset = 0;
    int64_t *strides = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    /* The source field is required; currently only integer sources are allowed */
    if (!(prop = get_required_property(value, "source")))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_uint64(prop, &source)) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(prop);
        const char *raw = NULL;
        asdf_value_as_scalar0(prop, &raw);
        ASDF_LOG(
            value->file,
            ASDF_LOG_WARN,
            "invalid or unsupported source for ndarray at %s: "
            "\"%s\"; only positive integers are supported",
            path,
            raw);
#endif
        goto failure;
    }

    asdf_value_destroy(prop);

    /* Parse shape */
    /* NOTE: After more careful reading of the standard, "shape" is not strictly required, nor
     * is "datatype", but this is confusing! For now require it.  See
     * https://github.com/asdf-format/asdf-standard/issues/470
     */
    if (!(prop = get_required_property(value, "shape")))
        goto failure;

    if (!asdf_value_is_sequence(prop)) {
        warn_invalid_shape(prop);
        goto failure;
    }

    int shape_size = asdf_sequence_size(prop);

    if (shape_size < 0) {
        warn_invalid_shape(prop);
        goto failure;
    }

    ndim = (uint64_t)shape_size;

    shape = malloc(ndim * sizeof(uint64_t));

    if (!shape) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *dim_val = NULL;
    size_t dim = 0;
    while ((dim_val = asdf_sequence_iter(prop, &iter))) {
        if (ASDF_VALUE_OK != asdf_value_as_uint64(dim_val, &shape[dim++])) {
            warn_invalid_shape(prop);
            goto failure;
        }
    }

    asdf_value_destroy(prop);

    /* Parse datatype */
    if (!(prop = get_required_property(value, "datatype")))
        goto failure;

    datatype = asdf_ndarray_deserialize_datatype(prop);
    asdf_value_destroy(prop);

    /* Parse byteorder */
    if ((prop = asdf_mapping_get(value, "byteorder"))) {
        byteorder = asdf_ndarray_deserialize_byteorder(prop);
        asdf_value_destroy(prop);
    }

    /* Parse offset */
    if ((prop = asdf_mapping_get(value, "offset"))) {
        if (ASDF_VALUE_OK != asdf_value_as_uint64(prop, &offset)) {
            offset = 0;
#ifdef ASDF_LOG_ENABLED
            const char *path = asdf_value_path(prop);
            ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid offset in ndarray at %s", path);
#endif
        }
        asdf_value_destroy(prop);
    }

    /* Parse strides */
    if ((prop = asdf_mapping_get(value, "strides"))) {
        if (!asdf_value_is_sequence(prop)) {
            warn_invalid_strides(prop);
            goto failure;
        }

        int strides_size = asdf_sequence_size(prop);

        if (strides_size < 0 || (uint64_t)strides_size != ndim) {
            warn_invalid_strides(prop);
            goto failure;
        }

        strides = malloc(ndim * sizeof(int64_t));

        if (!strides) {
            err = ASDF_VALUE_ERR_OOM;
            goto failure;
        }

        asdf_sequence_iter_t stride_iter = asdf_sequence_iter_init();
        asdf_value_t *stride_val = NULL;
        dim = 0;
        while ((stride_val = asdf_sequence_iter(prop, &stride_iter))) {
            if (ASDF_VALUE_OK != asdf_value_as_int64(stride_val, &strides[dim])) {
                warn_invalid_strides(prop);
                goto failure;
            }

            if (0 == strides[dim]) {
                warn_invalid_strides(prop);
                goto failure;
            }
            dim++;
        }

        asdf_value_destroy(prop);
    }

    asdf_ndarray_t *ndarray = calloc(1, sizeof(asdf_ndarray_t));

    if (!ndarray)
        return ASDF_VALUE_ERR_OOM;

    ndarray->source = source;
    ndarray->ndim = ndim;
    ndarray->shape = shape;
    ndarray->datatype = datatype;
    ndarray->byteorder = byteorder;
    ndarray->offset = offset;
    ndarray->strides = strides;
    ndarray->file = value->file;
    *out = ndarray;
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    free(shape);
    free(strides);
    return err;
}


static void asdf_ndarray_dealloc(void *value) {
    if (!value)
        return;

    asdf_ndarray_t *ndarray = value;
    asdf_block_close(ndarray->block);
    free(ndarray->shape);
    free(ndarray->strides);
    ZERO_MEMORY(ndarray, sizeof(asdf_ndarray_t));
    free(ndarray);
}


/*
 * Define the extension for the core/ndarray-1.1.0 schema
 *
 * TODO: Also support ndarray-1.0.0
 */
ASDF_REGISTER_EXTENSION(
    ndarray,
    ASDF_CORE_TAG_PREFIX "ndarray-1.1.0",
    asdf_ndarray_t,
    &libasdf_software,
    asdf_ndarray_deserialize,
    asdf_ndarray_dealloc,
    NULL);


static inline ssize_t asdf_ndarray_datatype_size(asdf_datatype_t type) {
    switch (type) {
    case ASDF_DATATYPE_INT8:
    case ASDF_DATATYPE_UINT8:
    case ASDF_DATATYPE_BOOL8:
        return 1;
    case ASDF_DATATYPE_INT16:
    case ASDF_DATATYPE_UINT16:
    case ASDF_DATATYPE_FLOAT16:
        return 2;
    case ASDF_DATATYPE_INT32:
    case ASDF_DATATYPE_UINT32:
    case ASDF_DATATYPE_FLOAT32:
        return 4;
    case ASDF_DATATYPE_INT64:
    case ASDF_DATATYPE_UINT64:
    case ASDF_DATATYPE_FLOAT64:
    case ASDF_DATATYPE_COMPLEX64:
        return 8;
    case ASDF_DATATYPE_COMPLEX128:
        return 16;

    // These need additional context to determine size, not implemented yet
    case ASDF_DATATYPE_ASCII:
    case ASDF_DATATYPE_UCS4:
    case ASDF_DATATYPE_RECORD:
    case ASDF_DATATYPE_UNKNOWN:
        return -1;
    default:
        UNREACHABLE();
    }
}


static inline asdf_byteorder_t asdf_host_byteorder() {
    uint16_t x = 1;
    return (*(uint8_t *)&x) == 1 ? ASDF_BYTEORDER_LITTLE : ASDF_BYTEORDER_BIG;
}


/* ndarray methods */
void *asdf_ndarray_data_raw(asdf_ndarray_t *ndarray, size_t *size) {
    if (!ndarray)
        return NULL;

    if (!ndarray->block) {
        asdf_block_t *block = asdf_block_open(ndarray->file, ndarray->source);

        if (!block)
            return NULL;

        ndarray->block = block;
    }

    return asdf_block_data(ndarray->block, size);
}


asdf_ndarray_err_t asdf_ndarray_read_tile_ndim(
    asdf_ndarray_t *ndarray,
    const uint64_t *origin,
    const uint64_t *shape,
    asdf_datatype_t dst_t,
    void **dst) {
    uint32_t ndim = ndarray->ndim;

    if (UNLIKELY(!dst || !ndarray || !origin || !shape))
        // Invalid argument, must be non-NULL
        return ASDF_NDARRAY_ERR_INVAL;

    asdf_datatype_t src_t = ndarray->datatype;

    if (dst_t == ASDF_DATATYPE_SOURCE)
        dst_t = src_t;

    ssize_t src_elsize = asdf_ndarray_datatype_size(src_t);
    ssize_t dst_elsize = asdf_ndarray_datatype_size(dst_t);

    // For not-yet-supported datatypes return ERR_INVAL
    if (src_elsize < 1 || dst_elsize < 1)
        return ASDF_NDARRAY_ERR_INVAL;

    // Check bounds
    // TODO: (Maybe? allow option for edge cases with fill values for out-of-bound pixels?
    uint64_t *array_shape = ndarray->shape;

    for (uint32_t idx = 0; idx < ndim; idx++) {
        if (origin[idx] + shape[idx] > array_shape[idx])
            return ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;
    }

    size_t tile_nelems = 0;

    if (ndim > 0) {
        tile_nelems = 1;

        for (uint32_t dim = 0; dim < ndim; dim++) {
            tile_nelems *= shape[dim];
        }
    }

    size_t tile_size = dst_elsize * tile_nelems;
    size_t data_size = 0;
    void *data = asdf_ndarray_data_raw(ndarray, &data_size);

    if (data_size < tile_size)
        return ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;
    //
    // If the function is passed a null pointer, allocate memory for the tile ourselves
    // User is responsible for freeing it.
    void *tile = *dst;
    void *new_buf = NULL;

    if (!tile) {
        tile = malloc(tile_size);

        if (!tile)
            return ASDF_NDARRAY_ERR_OOM;

        new_buf = tile;
    }

    // Special case, if size of the array is 0 just return now.  We do still malloc though even if
    // it's a bit pointless, just to ensure that the returned pointer can be freed successfully
    if (UNLIKELY(0 == ndim || 0 == tile_size)) {
        *dst = tile;
        return ASDF_NDARRAY_OK;
    }

    // Determine element strides (assume C-order for now; ndarray->strides is not used yet)
    uint32_t inner_dim = ndim - 1;
    int64_t *strides = malloc(sizeof(int64_t) * ndim);

    if (!strides) {
        free(new_buf);
        return ASDF_NDARRAY_ERR_OOM;
    }

    strides[inner_dim] = 1;

    if (ndim > 1) {
        for (uint32_t dim = inner_dim; dim > 0; dim--)
            strides[dim - 1] = strides[dim] * array_shape[dim];
    }

    // Determine the copy strategy to use; right now this just handles whether-or-not byteswap
    // is needed, may have others depending on alignment, vectorization etc.
    bool byteswap = false;

    if (src_elsize > 1) {
        asdf_byteorder_t host_byteorder = asdf_host_byteorder();

        if (host_byteorder != ndarray->byteorder)
            byteswap = true;
    }

    asdf_ndarray_convert_fn_t convert = asdf_ndarray_get_convert_fn(src_t, dst_t, byteswap);

    if (convert == NULL) {
        const char *src_datatype = asdf_ndarray_datatype_to_string(src_t);
        const char *dst_datatype = asdf_ndarray_datatype_to_string(dst_t);
        ASDF_LOG(
            ndarray->file,
            ASDF_LOG_WARN,
            "datatype conversion from \"%s\" to \"%s\" not supported for ndarray tile copy; "
            "source bytes will be copied without conversion",
            src_datatype,
            dst_datatype);
    }

    size_t offset = origin[inner_dim];
    bool is_1d = true;

    if (ndim > 1) {
        for (uint32_t dim = 0; dim < inner_dim; dim++) {
            offset += origin[dim] * strides[dim];
            if (shape[dim] != 1) {
                // If any of the outer dimensions are >1 than it's not a 1d tile
                is_1d = false;
            }
        }
        offset *= src_elsize;
    } else {
        offset = origin[0] * src_elsize;
    }

    bool overflow = false;

    // Special case if the "tile" is one-dimensional, C-contiguous
    if (is_1d) {
        const void *src = data + offset;
        // If convert() returns non-zero it means an overflow occurred
        // while copying; this does not necessarily have to be treated as an error depending
        // on the application.
        overflow = convert(tile, src, tile_nelems, dst_elsize);
        free(strides);
        *dst = tile;

        if (overflow)
            return ASDF_NDARRAY_ERR_OVERFLOW;

        return ASDF_NDARRAY_OK;
    }

    uint64_t *odometer = malloc(sizeof(uint64_t) * inner_dim);

    if (!odometer) {
        free(strides);
        free(new_buf);
        return ASDF_NDARRAY_ERR_OOM;
    }

    memcpy(odometer, origin, sizeof(uint64_t) * inner_dim);
    bool done = false;
    uint64_t inner_nelem = shape[inner_dim];
    size_t inner_size = inner_nelem * dst_elsize;
    const void *src = data + offset;
    void *dst_tmp = tile;

    while (!done) {
        overflow = convert(dst_tmp, src, inner_nelem, dst_elsize);
        dst_tmp += inner_size;

        uint32_t dim = inner_dim - 1;
        do {
            odometer[dim]++;
            src += strides[dim] * src_elsize;

            if (odometer[dim] < origin[dim] + shape[dim]) {
                break;
            } else {
                if (dim == 0) {
                    done = true;
                    break;
                }

                odometer[dim] = origin[dim];
                // Back up
                src -= shape[dim] * strides[dim] * src_elsize;
            }
        } while (dim-- > 0);
    }

    free(odometer);
    free(strides);
    *dst = tile;

    if (overflow)
        return ASDF_NDARRAY_ERR_OVERFLOW;

    return ASDF_NDARRAY_OK;
}


asdf_ndarray_err_t asdf_ndarray_read_tile_2d(
    asdf_ndarray_t *ndarray,
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    const uint64_t *plane_origin,
    asdf_datatype_t dst_t,
    void **dst) {
    uint32_t ndim = ndarray->ndim;

    if (ndim < 2)
        return ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;

    uint64_t *origin = calloc(ndim, sizeof(uint64_t));
    uint64_t *shape = calloc(ndim, sizeof(uint64_t));

    if (!origin || !shape)
        return ASDF_NDARRAY_ERR_OOM;

    uint32_t leading_ndim = ndim - 2;
    for (uint32_t dim = 0; dim < leading_ndim; dim++) {
        origin[dim] = plane_origin ? plane_origin[dim] : 0;
        shape[dim] = 1;
    }
    origin[ndim - 2] = y;
    origin[ndim - 1] = x;
    shape[ndim - 2] = height;
    shape[ndim - 1] = width;

    asdf_ndarray_err_t err = asdf_ndarray_read_tile_ndim(ndarray, origin, shape, dst_t, dst);
    free(origin);
    free(shape);
    return err;
}
