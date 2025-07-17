/*
 * Structures and methods pertaining to ASDF tree values
 */
#ifndef ASDF_VALUE_H
#define ASDF_VALUE_H


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


#endif /* ASDF_VALUE_H */
