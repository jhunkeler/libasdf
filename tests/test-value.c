#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <libfyaml.h>

#include <asdf/file.h>
#include <asdf/value.h>

#include "munit.h"
#include "util.h"


#define CHECK_VALUE_TYPE(key, expected_type) \
    do { \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        asdf_value_type_t __type = asdf_value_get_type(__value); \
        assert_int(__type, ==, (expected_type)); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_get_type) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_VALUE_TYPE("single_quoted", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("double_quoted", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("plain", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("literal", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("folded", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("false", ASDF_VALUE_BOOL);
    CHECK_VALUE_TYPE("False", ASDF_VALUE_BOOL);
    CHECK_VALUE_TYPE("FALSE", ASDF_VALUE_BOOL);
    // Parsed as uint8 but can be read as bool
    CHECK_VALUE_TYPE("false0", ASDF_VALUE_UINT8);
    CHECK_VALUE_TYPE("true", ASDF_VALUE_BOOL);
    CHECK_VALUE_TYPE("True", ASDF_VALUE_BOOL);
    CHECK_VALUE_TYPE("TRUE", ASDF_VALUE_BOOL);
    // Parsed as uint8 but can be read as bool
    CHECK_VALUE_TYPE("true1", ASDF_VALUE_UINT8);
    CHECK_VALUE_TYPE("null", ASDF_VALUE_NULL);
    CHECK_VALUE_TYPE("Null", ASDF_VALUE_NULL);
    CHECK_VALUE_TYPE("NULL", ASDF_VALUE_NULL);
    CHECK_VALUE_TYPE("empty", ASDF_VALUE_NULL);
    CHECK_VALUE_TYPE("int8", ASDF_VALUE_UINT8);
    CHECK_VALUE_TYPE("int16", ASDF_VALUE_UINT16);
    CHECK_VALUE_TYPE("int32", ASDF_VALUE_UINT32);
    CHECK_VALUE_TYPE("int64", ASDF_VALUE_UINT64);
    CHECK_VALUE_TYPE("uint8", ASDF_VALUE_UINT8);
    CHECK_VALUE_TYPE("uint16", ASDF_VALUE_UINT16);
    CHECK_VALUE_TYPE("uint32", ASDF_VALUE_UINT32);
    CHECK_VALUE_TYPE("uint64", ASDF_VALUE_UINT64);
    CHECK_VALUE_TYPE("bigint", ASDF_VALUE_UNKNOWN);
    CHECK_VALUE_TYPE("float32", ASDF_VALUE_FLOAT);
    CHECK_VALUE_TYPE("float64", ASDF_VALUE_DOUBLE);
    CHECK_VALUE_TYPE("bigfloat", ASDF_VALUE_DOUBLE);
    asdf_close(file);
    return MUNIT_OK;
}


/* Helper for string conversion tests */
#define CHECK_STR_VALUE(key, expected_value) \
    do { \
        const char *__v = NULL; \
        size_t __len = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_true(asdf_value_is_string(__value)); \
        asdf_value_err_t __err = asdf_value_as_string(__value, &__v, &__len); \
        assert_int(__err, ==, ASDF_VALUE_OK); \
        char *__vs = strndup(__v, __len); \
        assert_not_null(__vs); \
        assert_string_equal(__vs, (expected_value)); \
        free(__vs); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_STR_VALUE_MISMATCH(key) \
    do { \
        const char *__v = NULL; \
        size_t __len = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_false(asdf_value_is_string(__value)); \
        asdf_value_err_t __err = asdf_value_as_string(__value, &__v, &__len); \
        assert_int(__err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_as_string) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_STR_VALUE("plain", "string");
    CHECK_STR_VALUE("single_quoted", "string");
    CHECK_STR_VALUE("double_quoted", "string");
    CHECK_STR_VALUE("literal", "literal\nstring\n");
    CHECK_STR_VALUE("folded", "folded string\n");
    CHECK_STR_VALUE_MISMATCH("null");
    CHECK_STR_VALUE_MISMATCH("false");
    CHECK_STR_VALUE_MISMATCH("true");
    CHECK_STR_VALUE_MISMATCH("empty");
    CHECK_STR_VALUE_MISMATCH("int8");
    CHECK_STR_VALUE_MISMATCH("float32");
    asdf_close(file);
    return MUNIT_OK;
}


/* ``_as_string0`` is not very interesting compared to ``_as_string``
 * Should work the same except for returning the string value as a null-terminated copy
 */
MU_TEST(test_asdf_value_as_string0) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    const char *s = NULL;
    asdf_value_t *value = asdf_get_value(file, "plain");
    assert_not_null(value); \
    asdf_value_err_t err = asdf_value_as_string0(value, &s);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "string");
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_scalar) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    const char *s = NULL;
    size_t len = 0;
    int8_t i = 0;
    asdf_value_t *value = asdf_get_value(file, "int8");
    assert_not_null(value); \
    assert_true(asdf_value_is_scalar(value));
    asdf_value_err_t err = asdf_value_as_int8(value, &i);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_int(i, ==, 127);
    err = asdf_value_as_scalar(value, &s, &len);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "127");
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_scalar0) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    const char *s = NULL;
    int8_t i = 0;
    asdf_value_t *value = asdf_get_value(file, "int8");
    assert_not_null(value); \
    asdf_value_err_t err = asdf_value_as_int8(value, &i);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_int(i, ==, 127);
    err = asdf_value_as_scalar0(value, &s);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "127");
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


/* Helpers for bool conversion test */
#define CHECK_BOOL_VALUE(key, expected_value) \
    do { \
        bool __v = !(expected_value); \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_true(asdf_value_is_bool(__value)); \
        asdf_value_err_t __err = asdf_value_as_bool(__value, &__v); \
        assert_int(__err, ==, ASDF_VALUE_OK); \
        assert_int(__v, ==, (expected_value)); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_BOOL_MISMATCH(key) \
    do { \
        bool __v = false; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_false(asdf_value_is_bool(__value)); \
        asdf_value_err_t __err = asdf_value_as_bool(__value, &__v); \
        assert_int(__err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_as_bool) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_BOOL_VALUE("false", false);
    CHECK_BOOL_VALUE("False", false);
    CHECK_BOOL_VALUE("FALSE", false);
    // Allow 0 to be cast to bool
    CHECK_BOOL_VALUE("false0", false);
    CHECK_BOOL_VALUE("true", true);
    CHECK_BOOL_VALUE("True", true);
    CHECK_BOOL_VALUE("TRUE", true);
    // Allow 1 to be cast to bool
    CHECK_BOOL_VALUE("true1", true);
    CHECK_BOOL_MISMATCH("int64");
    CHECK_BOOL_MISMATCH("plain");
    asdf_close(file);
    return MUNIT_OK;
}


/* Helpers for null conversion tests */
#define CHECK_NULL_VALUE(key, expected) \
    do { \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        bool __v = !(expected); \
        __v = asdf_value_is_null(__value); \
        assert_int(__v, ==, (expected)); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_is_null) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_NULL_VALUE("null", true);
    CHECK_NULL_VALUE("Null", true);
    CHECK_NULL_VALUE("NULL", true);
    CHECK_NULL_VALUE("empty", true);
    CHECK_NULL_VALUE("plain", false);
    CHECK_NULL_VALUE("false0", false);
    asdf_close(file);
    return MUNIT_OK;
}


/* Helpers for int conversion tests */
#define CHECK_INT_VALUE(type, key, expected_err, expected_value) \
    do { \
        type##_t __v = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_true(asdf_value_is_int(__value)); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_int(__err, ==, (expected_err)); \
        type##_t __ve = (expected_value); \
        assert_int(__v, ==, __ve); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_INT_VALUE_MISMATCH(type, key) \
    do { \
        type##_t __v = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_false(asdf_value_is_##type(__value)); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_int(__err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_as_int8) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(int8, "int8", ASDF_VALUE_OK, 127);
    CHECK_INT_VALUE(int8, "uint8", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int8, "int16", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int8, "uint16", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int8, "int32", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int8, "uint32", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int8, "int64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int8, "uint64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE_MISMATCH(int8, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_int16) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(int16, "int8", ASDF_VALUE_OK, 127);
    CHECK_INT_VALUE(int16, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(int16, "int16", ASDF_VALUE_OK, 32767);
    CHECK_INT_VALUE(int16, "uint16", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int16, "int32", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int16, "uint32", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int16, "int64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int16, "uint64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE_MISMATCH(int16, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_int32) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(int32, "int8", ASDF_VALUE_OK, 127);
    CHECK_INT_VALUE(int32, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(int32, "int16", ASDF_VALUE_OK, 32767);
    CHECK_INT_VALUE(int32, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(int32, "int32", ASDF_VALUE_OK, 2147483647);
    CHECK_INT_VALUE(int32, "uint32", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int32, "int64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int32, "uint64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE_MISMATCH(int32, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_int64) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(int64, "int64", ASDF_VALUE_OK, 9223372036854775807LL);
    CHECK_INT_VALUE(int64, "int32", ASDF_VALUE_OK, 2147483647);
    CHECK_INT_VALUE(int64, "int16", ASDF_VALUE_OK, 32767);
    CHECK_INT_VALUE(int64, "int8", ASDF_VALUE_OK, 127);
    CHECK_INT_VALUE(int64, "uint64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int64, "uint32", ASDF_VALUE_OK, 4294967295);
    CHECK_INT_VALUE(int64, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(int64, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE_MISMATCH(int64, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint8) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(uint8, "int8", ASDF_VALUE_OK, 127);
    CHECK_INT_VALUE(uint8, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(uint8, "int16", ASDF_VALUE_ERR_OVERFLOW, 255);
    CHECK_INT_VALUE(uint8, "uint16", ASDF_VALUE_ERR_OVERFLOW, 255);
    CHECK_INT_VALUE(uint8, "int32", ASDF_VALUE_ERR_OVERFLOW, 255);
    CHECK_INT_VALUE(uint8, "uint32", ASDF_VALUE_ERR_OVERFLOW, 255);
    CHECK_INT_VALUE(uint8, "int64", ASDF_VALUE_ERR_OVERFLOW, 255);
    CHECK_INT_VALUE(uint8, "uint64", ASDF_VALUE_ERR_OVERFLOW, 255);
    CHECK_INT_VALUE_MISMATCH(uint8, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint16) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(uint16, "int8", ASDF_VALUE_OK, 127);
    CHECK_INT_VALUE(uint16, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(uint16, "int16", ASDF_VALUE_OK, 32767);
    CHECK_INT_VALUE(uint16, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(uint16, "int32", ASDF_VALUE_ERR_OVERFLOW, 65535);
    CHECK_INT_VALUE(uint16, "uint32", ASDF_VALUE_ERR_OVERFLOW, 65535);
    CHECK_INT_VALUE(uint16, "int64", ASDF_VALUE_ERR_OVERFLOW, 65535);
    CHECK_INT_VALUE(uint16, "uint64", ASDF_VALUE_ERR_OVERFLOW, 65535);
    CHECK_INT_VALUE_MISMATCH(uint16, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint32) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(uint32, "int8", ASDF_VALUE_OK, 127);
    CHECK_INT_VALUE(uint32, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(uint32, "int16", ASDF_VALUE_OK, 32767);
    CHECK_INT_VALUE(uint32, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(uint32, "int32", ASDF_VALUE_OK, 2147483647);
    CHECK_INT_VALUE(uint32, "uint32", ASDF_VALUE_OK, 4294967295);
    CHECK_INT_VALUE(uint32, "int64", ASDF_VALUE_ERR_OVERFLOW, 4294967295);
    CHECK_INT_VALUE(uint32, "uint64", ASDF_VALUE_ERR_OVERFLOW, 4294967295);
    CHECK_INT_VALUE_MISMATCH(uint32, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint64) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(uint64, "int8", ASDF_VALUE_OK, 127);
    CHECK_INT_VALUE(uint64, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(uint64, "int16", ASDF_VALUE_OK, 32767);
    CHECK_INT_VALUE(uint64, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(uint64, "int32", ASDF_VALUE_OK, 2147483647);
    CHECK_INT_VALUE(uint64, "uint32", ASDF_VALUE_OK, 4294967295);
    CHECK_INT_VALUE(uint64, "int64", ASDF_VALUE_OK, 9223372036854775807LL);
    CHECK_INT_VALUE(uint64, "uint64", ASDF_VALUE_OK, 18446744073709551615ULL);
    CHECK_INT_VALUE_MISMATCH(uint64, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint64_on_bigint) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "bigint");
    assert_not_null(value);
    uint64_t v = 0;
    asdf_value_err_t err = asdf_value_as_uint64(value, &v);
    assert_int(err, ==, ASDF_VALUE_ERR_OVERFLOW);
    assert_false(asdf_value_is_int(value));
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


#define CHECK_FLOAT_VALUE(type, key, expected_err, expected_value) \
    do { \
        type __v = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_int(__err, ==, (expected_err)); \
        type __ve = (expected_value); \
        assert_##type(__v, ==, __ve); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_FLOAT_VALUE_MISMATCH(type, key) \
    do { \
        type __v = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_false(asdf_value_is_##type(__value)); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_##type(__err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_as_float) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_FLOAT_VALUE(float, "float32", ASDF_VALUE_OK, 0.15625);
    CHECK_FLOAT_VALUE(float, "float64", ASDF_VALUE_ERR_OVERFLOW, 1.000000059604644775390625);
    CHECK_FLOAT_VALUE_MISMATCH(float, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_double) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_FLOAT_VALUE(double, "float32", ASDF_VALUE_OK, 0.15625);
    CHECK_FLOAT_VALUE(double, "float64", ASDF_VALUE_OK, 1.000000059604644775390625);
    CHECK_FLOAT_VALUE_MISMATCH(float, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Test that scalars explicitly tagged as !!str as interpreted as strings
 */
MU_TEST(test_asdf_value_tagged_strings) {
    const char *path = get_fixture_file_path("tagged-scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_STR_VALUE("string", "string");
    CHECK_STR_VALUE("bool_string", "true");
    CHECK_STR_VALUE("null_string", "null");
    CHECK_STR_VALUE("int_string", "1");
    CHECK_STR_VALUE("float_string", "1.0");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_mapping_iter) {
    const char *path = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *mapping = NULL;
    asdf_value_err_t err = asdf_get_mapping(file, "mapping", &mapping);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_size(mapping), ==, 2);
    asdf_mapping_iter_t iter = asdf_mapping_iter_init();

    asdf_mapping_item_t *item = asdf_mapping_iter(mapping, &iter);
    assert_not_null(item);
    assert_not_null(asdf_mapping_item_key(item));
    assert_string_equal(asdf_mapping_item_key(item), "foo");
    asdf_value_t *value = asdf_mapping_item_value(item);
    assert_not_null(value);
    const char *s = NULL;
    assert_int(asdf_value_as_string0(value, &s), ==, ASDF_VALUE_OK);
    assert_string_equal(s, "foo");

    item = asdf_mapping_iter(mapping, &iter);
    assert_not_null(item);
    assert_not_null(asdf_mapping_item_key(item));
    assert_string_equal(asdf_mapping_item_key(item), "bar");
    value = asdf_mapping_item_value(item);
    assert_not_null(value);
    s = NULL;
    assert_int(asdf_value_as_string0(value, &s), ==, ASDF_VALUE_OK);
    assert_string_equal(s, "bar");

    assert_null(asdf_mapping_iter(mapping, &iter));
    assert_null((void *)iter);
    asdf_value_destroy(mapping);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_mapping_get) {
    const char *path = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *mapping = NULL;
    asdf_value_err_t err = asdf_get_mapping(file, "mapping", &mapping);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(mapping);
    asdf_value_t *foo = asdf_mapping_get(mapping, "foo");
    assert_not_null(foo);
    assert_true(asdf_value_is_string(foo));
    const char *s = NULL;
    assert_int(asdf_value_as_string0(foo, &s), ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "foo");
    asdf_value_destroy(foo);

    asdf_value_t *bar = asdf_mapping_get(mapping, "bar");
    assert_not_null(bar);
    assert_true(asdf_value_is_string(bar));
    assert_int(asdf_value_as_string0(bar, &s), ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "bar");
    asdf_value_destroy(bar);

    asdf_value_t *null = asdf_mapping_get(mapping, "does-not-exist");
    assert_null(null);

    asdf_value_destroy(mapping);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_sequence_iter) {
    const char *path = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *sequence = NULL;
    asdf_value_err_t err = asdf_get_sequence(file, "sequence", &sequence);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_size(sequence), ==, 2);
    asdf_sequence_iter_t iter = asdf_sequence_iter_init();

    asdf_value_t *item = asdf_sequence_iter(sequence, &iter);
    assert_not_null(item);
    int8_t i8 = -1;
    assert_int(asdf_value_as_int8(item, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 0);

    item = asdf_sequence_iter(sequence, &iter);
    assert_not_null(item);
    assert_int(asdf_value_as_int8(item, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 1);

    assert_null(asdf_sequence_iter(sequence, &iter));
    assert_null((void *)iter);
    asdf_value_destroy(sequence);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_sequence_get) {
    const char *path = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *sequence = NULL;
    asdf_value_err_t err = asdf_get_sequence(file, "sequence", &sequence);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(sequence);

    asdf_value_t *first = asdf_sequence_get(sequence, 0);
    assert_not_null(first);
    assert_true(asdf_value_is_int(first));
    int8_t i8 = -1;
    assert_int(asdf_value_as_int8(first, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 0);
    asdf_value_destroy(first);

    asdf_value_t *second = asdf_sequence_get(sequence, 1);
    assert_not_null(second);
    assert_true(asdf_value_is_int(second));
    assert_int(asdf_value_as_int8(second, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 1);
    asdf_value_destroy(second);

    /* TODO: When memory allocation is improved for asdf_value_t, it might be that this returns
     * a pointer to the same asdf_value_t at the same address as the previous lookup; TBD */
    asdf_value_t *last = asdf_sequence_get(sequence, -1);
    assert_not_null(last);
    assert_true(asdf_value_is_int(last));
    assert_int(asdf_value_as_int8(last, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 1);
    asdf_value_destroy(last);

    asdf_value_t *third = asdf_sequence_get(sequence, 2);
    assert_null(third);

    asdf_value_destroy(sequence);
    asdf_close(file);
    return MUNIT_OK;
}


/** Regression test for :issue:`69` */
MU_TEST(test_value_copy_with_parent_path) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "/history/extensions/0");
    assert_not_null(value);
    assert_true(asdf_value_is_mapping(value));

    asdf_value_t *ext_uri = asdf_mapping_get(value, "extension_uri");
    assert_not_null(ext_uri);

    // The test: Cloned values should still retain their original path, as should any
    // child values retrieved from them.
    asdf_value_t *value_clone = asdf_value_clone(value);
    assert_string_equal(asdf_value_path(value_clone), "/history/extensions/0");
    asdf_value_t *ext_uri_clone = asdf_mapping_get(value_clone, "extension_uri");
    assert_string_equal(asdf_value_path(ext_uri_clone), "/history/extensions/0/extension_uri");

    // Crucially, it has the full path; this is just the control case
    assert_string_equal(asdf_value_path(ext_uri), "/history/extensions/0/extension_uri");

    asdf_value_destroy(ext_uri_clone);
    asdf_value_destroy(value_clone);
    asdf_value_destroy(ext_uri);
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_value,
    MU_RUN_TEST(test_asdf_value_get_type),
    MU_RUN_TEST(test_asdf_value_as_string),
    MU_RUN_TEST(test_asdf_value_as_string0),
    MU_RUN_TEST(test_asdf_value_as_scalar),
    MU_RUN_TEST(test_asdf_value_as_scalar0),
    MU_RUN_TEST(test_asdf_value_as_bool),
    MU_RUN_TEST(test_asdf_value_is_null),
    MU_RUN_TEST(test_asdf_value_as_int8),
    MU_RUN_TEST(test_asdf_value_as_int16),
    MU_RUN_TEST(test_asdf_value_as_int32),
    MU_RUN_TEST(test_asdf_value_as_int64),
    MU_RUN_TEST(test_asdf_value_as_uint8),
    MU_RUN_TEST(test_asdf_value_as_uint16),
    MU_RUN_TEST(test_asdf_value_as_uint32),
    MU_RUN_TEST(test_asdf_value_as_uint64),
    MU_RUN_TEST(test_asdf_value_as_uint64_on_bigint),
    MU_RUN_TEST(test_asdf_value_as_float),
    MU_RUN_TEST(test_asdf_value_as_double),
    MU_RUN_TEST(test_asdf_value_tagged_strings),
    MU_RUN_TEST(test_asdf_mapping_iter),
    MU_RUN_TEST(test_asdf_mapping_get),
    MU_RUN_TEST(test_asdf_sequence_iter),
    MU_RUN_TEST(test_asdf_sequence_get),
    MU_RUN_TEST(test_value_copy_with_parent_path)
);


MU_RUN_SUITE(test_asdf_value);
