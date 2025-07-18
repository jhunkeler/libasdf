/*
 * Structures and methods pertaining to ASDF tree values
 */
#ifndef ASDF_VALUE_H
#define ASDF_VALUE_H

#include <stdbool.h>
#include <stdint.h>

#include <asdf/util.h>


typedef enum {
    ASDF_VALUE_UNKNOWN,
    ASDF_VALUE_SEQUENCE,
    ASDF_VALUE_MAPPING,
    // Generic scalar, before coercion
    ASDF_VALUE_SCALAR,

    // The rest are all possible scalar types
    ASDF_VALUE_STRING,
    ASDF_VALUE_BOOL,
    ASDF_VALUE_NULL,

    // Signed integers
    ASDF_VALUE_INT8,
    ASDF_VALUE_INT16,
    ASDF_VALUE_INT32,
    ASDF_VALUE_INT64,

    // Unsigned integers
    ASDF_VALUE_UINT8,
    ASDF_VALUE_UINT16,
    ASDF_VALUE_UINT32,
    ASDF_VALUE_UINT64,

    // Floating point
    ASDF_VALUE_FLOAT32,
    ASDF_VALUE_FLOAT64,

    // Extension types (to be defined in more detail later)
    ASDF_VALUE_EXTENSION
} asdf_value_type_t;


typedef enum {
    ASDF_VALUE_ERR_UNKNOWN = -1,
    ASDF_VALUE_OK = 0,
    ASDF_VALUE_ERR_TYPE_MISMATCH,
    ASDF_VALUE_ERR_PARSE_FAILURE,
    ASDF_VALUE_ERR_OVERFLOW,
    ASDF_VALUE_ERR_TAG_MISMATCH,
} asdf_value_err_t;


typedef struct asdf_value asdf_value_t;


ASDF_EXPORT void asdf_value_destroy(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_string(asdf_value_t *value, const char **out, size_t *out_len);
ASDF_EXPORT asdf_value_err_t asdf_value_as_string0(asdf_value_t *value, char **out);

ASDF_EXPORT asdf_value_err_t asdf_value_as_scalar(asdf_value_t *value, const char **out, size_t* out_len);
ASDF_EXPORT asdf_value_err_t asdf_value_as_scalar0(asdf_value_t *value, char **out);

ASDF_EXPORT asdf_value_err_t asdf_value_as_bool(asdf_value_t *value, bool *out);
ASDF_EXPORT asdf_value_err_t asdf_value_is_null(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int8(asdf_value_t *value, int8_t *out);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int16(asdf_value_t *value, int16_t *out);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int32(asdf_value_t *value, int32_t *out);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int64(asdf_value_t *value, int64_t *out);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint8(asdf_value_t *value, uint8_t *out);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint16(asdf_value_t *value, uint16_t *out);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint32(asdf_value_t *value, uint32_t *out);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint64(asdf_value_t *value, uint64_t *out);
ASDF_EXPORT asdf_value_err_t asdf_value_as_float(asdf_value_t *value, float *out);
ASDF_EXPORT asdf_value_err_t asdf_value_as_double(asdf_value_t *value, double *out);


#endif /* ASDF_VALUE_H */
