#include <stddef.h>
#include <stdint.h>

#include <asdf/core/asdf.h>
#define ASDF_CORE_NDARRAY_INTERNAL
#include <asdf/core/ndarray.h>
#undef ASDF_CORE_NDARRAY_INTERNAL
#include <asdf/extension.h>

#include "../file.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"


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
        return ASDF_DATATYPE_UNKNOWN;
    }

    if (strcmp(s, "bool8") == 0)
        return ASDF_DATATYPE_BOOL8;

unknown:
#ifdef ASDF_LOG_ENABLED
    const char *path = asdf_value_path(value);
    ASDF_LOG(value->file, ASDF_LOG_WARN, "unknown datatype for ndarray at %s: %s", path, s);
#endif
    return ASDF_DATATYPE_UNKNOWN;
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

invalid:
#ifdef ASDF_LOG_ENABLED
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "invalid byteorder for ndarray at %s; "
        "defaulting to \"little\"",
        path);
#endif
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
