#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <libfyaml.h>

#include <asdf/file.h>
#include <asdf/value.h>

#include "munit.h"
#include "util.h"


/* Helper for string conversion tests */
#define CHECK_STR_VALUE(key, expected_value) \
    do { \
        const char *__v = NULL; \
        size_t __len = 0; \
        asdf_value_t *__value = asdf_get(file, (key)); \
        assert_not_null(__value); \
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
        asdf_value_t *__value = asdf_get(file, (key)); \
        assert_not_null(__value); \
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
    char *s = NULL;
    asdf_value_t *value = asdf_get(file, "plain");
    assert_not_null(value); \
    asdf_value_err_t err = asdf_value_as_string0(value, &s);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "string");
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


/* Helpers for bool conversion test */
#define CHECK_BOOL_VALUE(key, expected_value) \
    do { \
        bool __v = !(expected_value); \
        asdf_value_t *__value = asdf_get(file, (key)); \
        assert_not_null(__value); \
        asdf_value_err_t __err = asdf_value_as_bool(__value, &__v); \
        assert_int(__err, ==, ASDF_VALUE_OK); \
        assert_int(__v, ==, (expected_value)); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_BOOL_MISMATCH(key) \
    do { \
        int __v = -1; \
        asdf_value_t *__value = asdf_get(file, (key)); \
        assert_not_null(__value); \
        asdf_value_err_t __err = asdf_value_as_bool(__value, (bool *)&__v); \
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
#define CHECK_NULL_VALUE(key, expected_err) \
    do { \
        asdf_value_t *__value = asdf_get(file, (key)); \
        assert_not_null(__value); \
        asdf_value_err_t __err = asdf_value_is_null(__value); \
        assert_int(__err, ==, (expected_err)); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_is_null) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_NULL_VALUE("null", ASDF_VALUE_OK);
    CHECK_NULL_VALUE("Null", ASDF_VALUE_OK);
    CHECK_NULL_VALUE("NULL", ASDF_VALUE_OK);
    CHECK_NULL_VALUE("empty", ASDF_VALUE_OK);
    CHECK_NULL_VALUE("plain", ASDF_VALUE_ERR_TYPE_MISMATCH);
    CHECK_NULL_VALUE("false0", ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_close(file);
    return MUNIT_OK;
}


/* Helpers for int conversion tests */
#define CHECK_INT_VALUE(type, key, expected_err, expected_value) \
    do { \
        type##_t __v = 0; \
        asdf_value_t *__value = asdf_get(file, (key)); \
        assert_not_null(__value); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_int(__err, ==, (expected_err)); \
        type##_t __ve = (expected_value); \
        assert_int(__v, ==, __ve); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_INT_VALUE_MISMATCH(type, key) \
    do { \
        type##_t __v = 0; \
        asdf_value_t *__value = asdf_get(file, (key)); \
        assert_not_null(__value); \
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
    CHECK_INT_VALUE(uint64, "bigint", ASDF_VALUE_ERR_OVERFLOW, 0);
    CHECK_INT_VALUE_MISMATCH(uint64, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_value,
    MU_RUN_TEST(test_asdf_value_as_string),
    MU_RUN_TEST(test_asdf_value_as_string0),
    MU_RUN_TEST(test_asdf_value_as_bool),
    MU_RUN_TEST(test_asdf_value_is_null),
    MU_RUN_TEST(test_asdf_value_as_int8),
    MU_RUN_TEST(test_asdf_value_as_int16),
    MU_RUN_TEST(test_asdf_value_as_int32),
    MU_RUN_TEST(test_asdf_value_as_int64),
    MU_RUN_TEST(test_asdf_value_as_uint8),
    MU_RUN_TEST(test_asdf_value_as_uint16),
    MU_RUN_TEST(test_asdf_value_as_uint32),
    MU_RUN_TEST(test_asdf_value_as_uint64)
);


MU_RUN_SUITE(test_asdf_value);
