#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <libfyaml.h>

#include "error.h"
#include "log.h"
#include "util.h"
#include "value.h"
#include "value_util.h"


/* TODO: Create or return from a freelist that lives on the file */
asdf_value_t *asdf_value_create(asdf_file_t *file, struct fy_node *node) {
    asdf_value_t *value = calloc(1, sizeof(asdf_value_t));

    if (!value) {
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    value->file = file;

    enum fy_node_type node_type = fy_node_get_type(node);
    switch (node_type) {
    case FYNT_SCALAR:
        value->type = ASDF_VALUE_SCALAR;
        break;
    case FYNT_MAPPING:
        value->type = ASDF_VALUE_MAPPING;
        break;
    case FYNT_SEQUENCE:
        value->type = ASDF_VALUE_SEQUENCE;
        break;
    }
    value->err = ASDF_VALUE_ERR_UNKNOWN;
    value->node = node;
    value->path = NULL;
    return value;
}


void asdf_value_destroy(asdf_value_t *value) {
    if (!value)
        return;

    /* Not so sure about this.  According to the docs:
     * Recursively frees the given node...
     * So this will free sub-nodes as well? Seems dangerous
     * I have not yet decided the memory management strategy for asdf_value yet so need to
     * come back to this...
     */
    fy_node_free(value->node);
    free(value->path);
    ZERO_MEMORY(value, sizeof(asdf_value_t));
    free(value);
}


const char *asdf_value_path(asdf_value_t *value) {
    if (!value)
        return NULL;

    if (NULL == value->path) {
        // Get the path and memoize it; it can be freed when the value is freed
        value->path = fy_node_get_path(value->node);
    }

    return value->path;
}


/* Mapping functions */
bool asdf_value_is_mapping(asdf_value_t *value) {
    return value->type == ASDF_VALUE_MAPPING;
}


int asdf_mapping_size(asdf_value_t *mapping) {
    if (UNLIKELY(!mapping) || !asdf_value_is_mapping(mapping))
        return -1;

    return fy_node_mapping_item_count(mapping->node);
}


asdf_mapping_iter_t asdf_mapping_iter_init() {
    return NULL;
}


const char *asdf_mapping_item_key(asdf_mapping_item_t *item) {
    return item->key;
}


asdf_value_t *asdf_mapping_item_value(asdf_mapping_item_t *item) {
    return item->value;
}


asdf_mapping_item_t *asdf_mapping_iter(asdf_value_t *mapping, asdf_mapping_iter_t *iter) {
    if (mapping->type != ASDF_VALUE_MAPPING) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(mapping->file, ASDF_LOG_WARN, "%s is not a mapping", asdf_value_path(mapping));
#endif
        return NULL;
    }

    _asdf_mapping_iter_impl_t *impl = *iter;

    if (NULL == impl) {
        impl = calloc(1, sizeof(_asdf_mapping_iter_impl_t));

        if (!impl) {
            ASDF_ERROR_OOM(mapping->file);
            return false;
        }

        *iter = impl;
    }

    struct fy_node_pair *pair = fy_node_mapping_iterate(mapping->node, &impl->iter);

    if (!pair) {
        /* Cleanup and end iteration */
        goto cleanup;
    }

    struct fy_node *key_node = fy_node_pair_key(pair);

    if (UNLIKELY(fy_node_get_type(key_node) != FYNT_SCALAR)) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(
            mapping->file,
            ASDF_LOG_WARN,
            "non-scalar key found in mapping %s; non-scalar "
            "keys are not allowed in ASDF; the value will be returned but the key will "
            "be set to NULL",
            asdf_value_path(mapping));
#endif
        impl->key = NULL;
    } else {
        impl->key = fy_node_get_scalar0(key_node);
    }

    struct fy_node *value_node = fy_node_pair_value(pair);
    asdf_value_t *value = asdf_value_create(mapping->file, value_node);

    if (!value) {
        goto cleanup;
    }

    // Destroy previous value if any
    asdf_value_destroy(impl->value);
    impl->value = value;
    return impl;

cleanup:
    impl->key = NULL;
    asdf_value_destroy(impl->value);
    impl->value = NULL;
    free(impl);
    *iter = NULL;
    return NULL;
}


/* Sequence functions */
bool asdf_value_is_sequence(asdf_value_t *value) {
    return value->type == ASDF_VALUE_SEQUENCE;
}


/* Scalar functions */
static bool is_yaml_null(const char *s, size_t len) {
    return (
        !s || len == 0 ||
        (len == 4 && ((strncmp(s, "null", len) == 0) || (strncmp(s, "Null", len) == 0) ||
                      (strncmp(s, "NULL", len) == 0))) ||
        (len == 1 && s[0] == '~'));
}


static bool is_yaml_bool(const char *s, size_t len, bool *value) {
    if (!s)
        return false;

    /* Allow 0 and 1 tagged as bool */
    if (len == 1) {
        if (s[0] == '0') {
            *value = false;
            return true;
        } else if (s[0] == '1') {
            *value = true;
            return true;
        }
    }

    if (len == 5 && ((0 == strncmp(s, "false", len)) || (0 == strncmp(s, "False", len)) ||
                     (0 == strncmp(s, "FALSE", len)))) {
        *value = false;
        return true;
    } else if (
        len == 4 && ((0 == strncmp(s, "true", len)) || (0 == strncmp(s, "True", len)) ||
                     (0 == strncmp(s, "TRUE", len)))) {
        *value = true;
        return true;
    }

    return false;
}


static asdf_value_err_t is_yaml_signed_int(
    const char *s, size_t len, int64_t *value, asdf_value_type_t *type) {
    if (!s)
        return ASDF_VALUE_ERR_UNKNOWN;

    char *is = strndup(s, len);

    if (!is)
        return ASDF_VALUE_ERR_UNKNOWN;

    errno = 0;
    char *end = NULL;
    int64_t v = strtoll(is, &end, 0);

    if (errno == ERANGE) {
        free(is);
        return ASDF_VALUE_ERR_OVERFLOW;
    } else if (errno || *end) {
        free(is);
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    /* choose smallest int that fits */
    if (v >= INT8_MIN && v <= INT8_MAX)
        *type = ASDF_VALUE_INT8;
    else if (v >= INT16_MIN && v <= INT16_MAX)
        *type = ASDF_VALUE_INT16;
    else if (v >= INT32_MIN && v <= INT32_MAX)
        *type = ASDF_VALUE_INT32;
    else
        *type = ASDF_VALUE_INT64;

    *value = v;
    free(is);
    return ASDF_VALUE_OK;
}


static asdf_value_err_t is_yaml_unsigned_int(
    const char *s, size_t len, uint64_t *value, asdf_value_type_t *type) {
    if (!s)
        return ASDF_VALUE_ERR_UNKNOWN;

    char *us = strndup(s, len);

    if (!us)
        return ASDF_VALUE_ERR_UNKNOWN;

    errno = 0;
    char *end = NULL;
    uint64_t v = strtoull(us, &end, 0);

    if (errno == ERANGE) {
        free(us);
        return ASDF_VALUE_ERR_OVERFLOW;
    } else if (errno || *end) {
        free(us);
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    /* choose smallest int that fits */
    if (v <= UINT8_MAX)
        *type = ASDF_VALUE_UINT8;
    else if (v <= UINT16_MAX)
        *type = ASDF_VALUE_UINT16;
    else if (v <= UINT32_MAX)
        *type = ASDF_VALUE_UINT32;
    else
        *type = ASDF_VALUE_UINT64;

    *value = v;
    free(us);
    return ASDF_VALUE_OK;
}


static asdf_value_err_t is_yaml_float(
    const char *s, size_t len, double *value, asdf_value_type_t *type) {
    if (!s)
        return ASDF_VALUE_ERR_UNKNOWN;

    char *fs = strndup(s, len);

    if (!fs)
        return ASDF_VALUE_ERR_UNKNOWN;

    errno = 0;
    char *end = NULL;
    double v = strtod(fs, &end);

    if (errno == ERANGE) {
        free(fs);
        *type = ASDF_VALUE_DOUBLE;
        return ASDF_VALUE_ERR_OVERFLOW;
    } else if (errno || *end) {
        free(fs);
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    float fv = (float)v;

    if ((double)fv == v)
        *type = ASDF_VALUE_FLOAT;
    else
        *type = ASDF_VALUE_DOUBLE;

    *value = v;
    free(fs);
    return ASDF_VALUE_OK;
}


static asdf_value_err_t asdf_value_infer_null(asdf_value_t *value, const char *s, size_t len) {
    if (is_yaml_null(s, len)) {
        value->type = ASDF_VALUE_NULL;
        value->err = ASDF_VALUE_OK;
        return ASDF_VALUE_OK;
    }

    value->type = ASDF_VALUE_UNKNOWN;
    value->err = ASDF_VALUE_ERR_PARSE_FAILURE;
    return ASDF_VALUE_ERR_PARSE_FAILURE;
}


static asdf_value_err_t asdf_value_infer_bool(asdf_value_t *value, const char *s, size_t len) {
    bool b_val = false;
    if (is_yaml_bool(s, len, &b_val)) {
        value->type = ASDF_VALUE_BOOL;
        value->scalar.b = b_val;
        value->err = ASDF_VALUE_OK;
        return ASDF_VALUE_OK;
    }

    value->type = ASDF_VALUE_UNKNOWN;
    value->err = ASDF_VALUE_ERR_PARSE_FAILURE;
    return ASDF_VALUE_ERR_PARSE_FAILURE;
}


static asdf_value_err_t asdf_value_infer_int(asdf_value_t *value, const char *s, size_t len) {
    uint64_t u_val = 0;
    asdf_value_type_t type = ASDF_VALUE_UNKNOWN;
    asdf_value_err_t err = is_yaml_unsigned_int(s, len, &u_val, &type);

    if (ASDF_VALUE_OK == err) {
        value->type = type;
        value->scalar.u = u_val;
    } else {
        int64_t i_val = 0;
        err = is_yaml_signed_int(s, len, &i_val, &type);

        if (ASDF_VALUE_OK == err) {
            value->type = type;
            value->scalar.i = i_val;
        } else {
            value->type = ASDF_VALUE_UNKNOWN;
        }
    }
    value->err = err;
    return err;
}


static asdf_value_err_t asdf_value_infer_float(asdf_value_t *value, const char *s, size_t len) {
    double d_val = 0.0;
    asdf_value_type_t type = ASDF_VALUE_UNKNOWN;
    asdf_value_err_t err = is_yaml_float(s, len, &d_val, &type);
    if (ASDF_VALUE_OK == err || ASDF_VALUE_ERR_OVERFLOW == err) {
        value->type = type;
        value->scalar.d = d_val;
    } else {
        value->type = ASDF_VALUE_UNKNOWN;
    }
    value->err = err;
    return err;
}


static asdf_value_err_t asdf_value_infer_scalar_type(asdf_value_t *value) {
    if (!value)
        return ASDF_VALUE_ERR_UNKNOWN;

    switch (value->type) {
    case ASDF_VALUE_SEQUENCE:
    case ASDF_VALUE_MAPPING:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    default:
        break;
    }

    if (value->type != ASDF_VALUE_SCALAR) {
        /* Has already been inferred */
        return ASDF_VALUE_OK;
    } else if (!((ASDF_VALUE_ERR_UNKNOWN == value->err) || (ASDF_VALUE_OK == value->err))) {
        return value->err;
    }

    enum fy_node_style style = fy_node_get_style(value->node);

    /* Quoted / literal styles -> always string */
    switch (style) {
    case FYNS_SINGLE_QUOTED:
    case FYNS_DOUBLE_QUOTED:
    case FYNS_LITERAL:
    case FYNS_FOLDED:
        value->type = ASDF_VALUE_STRING;
        value->err = ASDF_VALUE_OK;
        return ASDF_VALUE_OK;
    default:
        break;
    }

    size_t tag_len = 0;
    const char *maybe_tag = fy_node_get_tag(value->node, &tag_len);
    char *tag_str = NULL;

    if (maybe_tag)
        tag_str = strndup(maybe_tag, tag_len);

    size_t len = 0;
    const char *s = fy_node_get_scalar(value->node, &len);

    /* If Common Schema tag explicitly present, honor it (but verify) */
    if (tag_str) {
        asdf_yaml_common_tag_t tag = asdf_common_tag_get(tag_str);
        asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;

#ifdef ASDF_LOG_ENABLED
        if (tag != ASDF_YAML_COMMON_TAG_UNKNOWN) {
            ASDF_LOG(
                value->file,
                ASDF_LOG_DEBUG,
                "inferring value type from YAML Common Schema tag: %s",
                tag_str);
        }
#endif

        switch (tag) {
        case ASDF_YAML_COMMON_TAG_UNKNOWN:
            break;
        case ASDF_YAML_COMMON_TAG_NULL:
            err = asdf_value_infer_null(value, s, len);
            break;
        case ASDF_YAML_COMMON_TAG_BOOL: {
            err = asdf_value_infer_bool(value, s, len);
            break;
        }
        case ASDF_YAML_COMMON_TAG_INT: {
            err = asdf_value_infer_int(value, s, len);
            break;
        }
        case ASDF_YAML_COMMON_TAG_FLOAT: {
            err = asdf_value_infer_float(value, s, len);
            break;
        }
        case ASDF_YAML_COMMON_TAG_STR:
            /* Explicitly tagged as str--just set the value type to str, no need to parse */
            value->type = ASDF_VALUE_STRING;
            err = ASDF_VALUE_OK;
            break;
        }
        free(tag_str);
        return err;
    }

    /* TODO: Infer extension types from tags */

    /* Untagged -> core schema heuristics (plain style only) */
    if (ASDF_VALUE_OK == asdf_value_infer_null(value, s, len)) {
        ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as null", len, s);
        return ASDF_VALUE_OK;
    }

    if (ASDF_VALUE_OK == asdf_value_infer_bool(value, s, len)) {
        ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as bool", len, s);
        return ASDF_VALUE_OK;
    }

    asdf_value_err_t err = asdf_value_infer_int(value, s, len);

    if (ASDF_VALUE_OK == err || ASDF_VALUE_ERR_OVERFLOW == err) {
#ifdef ASDF_LOG_ENABLED
        if (ASDF_VALUE_ERR_OVERFLOW == err)
            ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as int (with overflow)", len, s);
        else
            ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as int", len, s);
#endif
        return err;
    }

    err = asdf_value_infer_float(value, s, len);

    if (ASDF_VALUE_OK == err || ASDF_VALUE_ERR_OVERFLOW == err) {
#ifdef ASDF_LOG_ENABLED
        if (ASDF_VALUE_ERR_OVERFLOW == err)
            ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as float (with overflow)", len, s);
        else
            ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as float", len, s);
#endif
        return err;
    }

    /* Otherwise treat as a string */
    ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as string", len, s);
    value->type = ASDF_VALUE_STRING;
    value->err = ASDF_VALUE_OK;
    return ASDF_VALUE_OK;
}


asdf_value_type_t asdf_value_get_type(asdf_value_t *value) {
    if (!value)
        return ASDF_VALUE_UNKNOWN;

    if (value->type != ASDF_VALUE_SCALAR)
        return value->type;

    asdf_value_infer_scalar_type(value);
    return value->type;
}


/* Helper to define the asdf_value_is_(type) checkers */
#define __ASDF_VALUE_IS_SCALAR_TYPE(typ, value_type) \
    bool asdf_value_is_##typ(asdf_value_t *value) { \
        if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK) \
            return false; \
        return value->type == (value_type); \
    }


__ASDF_VALUE_IS_SCALAR_TYPE(string, ASDF_VALUE_STRING)


asdf_value_err_t asdf_value_as_string(asdf_value_t *value, const char **out, size_t *len) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    if (value->type != ASDF_VALUE_STRING)
        return ASDF_VALUE_ERR_TYPE_MISMATCH;

    *out = fy_node_get_scalar(value->node, len);
    return ASDF_VALUE_OK;
}


asdf_value_err_t asdf_value_as_string0(asdf_value_t *value, const char **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    if (value->type != ASDF_VALUE_STRING)
        return ASDF_VALUE_ERR_TYPE_MISMATCH;

    *out = fy_node_get_scalar0(value->node);
    return ASDF_VALUE_OK;
}


bool asdf_value_is_scalar(asdf_value_t *value) {
    return (asdf_value_infer_scalar_type(value) == ASDF_VALUE_OK);
}


/**
 * Like `asdf_value_as_string` but returns the unparsed text of a scalar value, ignoring type
 * inference entirely
 */
asdf_value_err_t asdf_value_as_scalar(asdf_value_t *value, const char **out, size_t *len) {
    if (!value)
        return ASDF_VALUE_ERR_UNKNOWN;

    switch (value->type) {
    case ASDF_VALUE_MAPPING:
    case ASDF_VALUE_SEQUENCE:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    default:
        break;
    }

    *out = fy_node_get_scalar(value->node, len);
    return ASDF_VALUE_OK;
}


asdf_value_err_t asdf_value_as_scalar0(asdf_value_t *value, const char **out) {
    if (!value)
        return ASDF_VALUE_ERR_UNKNOWN;

    switch (value->type) {
    case ASDF_VALUE_MAPPING:
    case ASDF_VALUE_SEQUENCE:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    default:
        break;
    }

    *out = fy_node_get_scalar0(value->node);
    return ASDF_VALUE_OK;
}


__ASDF_VALUE_IS_SCALAR_TYPE(bool, ASDF_VALUE_BOOL)


asdf_value_err_t asdf_value_as_bool(asdf_value_t *value, bool *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    /* Allow casting plain 0/1 (strictly) to bool */
    if (value->type == ASDF_VALUE_UINT8) {
        if (value->scalar.u == 0) {
            *out = value->scalar.b;
            return ASDF_VALUE_OK;
        } else if (value->scalar.u == 1) {
            *out = value->scalar.b;
            return ASDF_VALUE_OK;
        }
    } else if (value->type != ASDF_VALUE_BOOL) {
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }

    *out = value->scalar.b;
    return ASDF_VALUE_OK;
}


__ASDF_VALUE_IS_SCALAR_TYPE(null, ASDF_VALUE_NULL)


bool asdf_value_is_int(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_UINT8:
        return true;
    default:
        return false;
    }
}


bool asdf_value_is_int8(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT8:
        return true;
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64:
        return value->scalar.i >= INT8_MIN && value->scalar.u <= INT8_MAX;
    case ASDF_VALUE_UINT8:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= INT8_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_int8(asdf_value_t *value, int8_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        // Return anyways but indicate an overflow
        *out = (int8_t)value->scalar.i;

        if (value->scalar.i > INT8_MAX || value->scalar.i < INT8_MIN)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (int8_t)value->scalar.u;

        // Return anyways but indicate an overflow
        if (value->scalar.u > INT8_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_int16(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64:
        return value->scalar.i >= INT16_MIN && value->scalar.u <= INT16_MAX;
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= INT16_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_int16(asdf_value_t *value, int16_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        *out = (int16_t)value->scalar.i;

        if (value->scalar.i > INT16_MAX || value->scalar.i < INT16_MIN)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (int16_t)value->scalar.u;

        if (value->scalar.u > INT16_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_int32(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_UINT8:
    case ASDF_VALUE_UINT16:
        return true;
    case ASDF_VALUE_INT64:
        return value->scalar.i >= INT32_MIN && value->scalar.u <= INT32_MAX;
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= INT32_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_int32(asdf_value_t *value, int32_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        *out = (int32_t)value->scalar.i;

        if (value->scalar.i > INT32_MAX || value->scalar.i < INT32_MIN)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        // Return anyways but indicate an overflow
        *out = (int32_t)value->scalar.u;

        if (value->scalar.u > INT32_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_int64(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= INT64_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_int64(asdf_value_t *value, int64_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        *out = value->scalar.i;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (int64_t)value->scalar.u;

        if (value->scalar.u > INT64_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_uint8(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_UINT8:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return true;
    case ASDF_VALUE_INT8:
        return value->scalar.i >= 0;
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64:
        return value->scalar.i >= 0 && value->scalar.i <= UINT8_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_uint8(asdf_value_t *value, uint8_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_UINT8:
        *out = value->scalar.u;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
        // Return anyways but indicate an overflow
        *out = (int64_t)value->scalar.u;

        if (value->scalar.u > UINT8_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT8:
        // Allow if it doesn't underflow, otherwise still return the value but
        // indicate underflow (but we use OVERFLOW here; doesn't distinguish)
        *out = value->scalar.i;

        if (value->scalar.i < 0)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
        // Return anyways but indicate an overflow
        *out = (int64_t)value->scalar.i;

        if (value->scalar.i < 0 || value->scalar.i > UINT8_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_ERR_OVERFLOW;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_uint16(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= UINT16_MAX;
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        return value->scalar.i >= 0;
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64:
        return value->scalar.i >= 0 && value->scalar.i <= UINT16_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_uint16(asdf_value_t *value, uint16_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (uint16_t)value->scalar.u;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
        // Return anyways but indicate an overflow
        *out = (uint16_t)value->scalar.u;

        if (value->scalar.u > UINT16_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        // Allow if it doesn't underflow, otherwise still return the value but
        // indicate underflow (but we use OVERFLOW here; doesn't distinguish)
        *out = (uint16_t)value->scalar.i;

        if (value->scalar.i < 0)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
        // Return anyways but indicate an overflow
        *out = (uint16_t)value->scalar.i;

        if (value->scalar.i < 0 || value->scalar.i > UINT16_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_uint32(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= UINT32_MAX;
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        return value->scalar.i >= 0;
    case ASDF_VALUE_INT64:
        return value->scalar.i >= 0 && value->scalar.i <= UINT16_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_uint32(asdf_value_t *value, uint32_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (uint32_t)value->scalar.u;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        // Allow if it doesn't underflow, otherwise still return the value but
        // indicate underflow (but we use OVERFLOW here; doesn't distinguish)
        *out = (uint32_t)value->scalar.i;

        if (value->scalar.i < 0)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_INT64:
        // Return anyways but indicate an overflow
        *out = (uint32_t)value->scalar.u;

        if (value->scalar.u > UINT32_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_uint64(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        return value->scalar.i >= 0;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_uint64(asdf_value_t *value, uint64_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (uint64_t)value->scalar.u;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        // Allow if it doesn't underflow, otherwise still return the value but
        // indicate underflow (but we use OVERFLOW here; doesn't distinguish)
        *out = (uint64_t)value->scalar.i;

        if (value->scalar.i < 0)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_float(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_DOUBLE:
    case ASDF_VALUE_FLOAT:
        return true;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_float(asdf_value_t *value, float *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_FLOAT:
        *out = (float)value->scalar.d;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_DOUBLE:
        *out = (float)value->scalar.d;

        if ((float)value->scalar.d != value->scalar.d)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


__ASDF_VALUE_IS_SCALAR_TYPE(double, ASDF_VALUE_DOUBLE)


asdf_value_err_t asdf_value_as_double(asdf_value_t *value, double *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_DOUBLE:
    case ASDF_VALUE_FLOAT:
        *out = value->scalar.d;
        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}
