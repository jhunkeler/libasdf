#include <stdint.h>

#include <libfyaml.h>

#include <asdf/file.h>
#include <asdf/value.h>

#include "munit.h"
#include "util.h"


/* Helper for int conversion tests */
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


#define CHECK_VALUE_MISMATCH(type, key) \
    do { \
        type##_t __v = 0; \
        asdf_value_t *__value = asdf_get(file, (key)); \
        assert_not_null(__value); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_int(__err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH); \
        asdf_value_destroy(__value); \
    } while (0)


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
    CHECK_VALUE_MISMATCH(int64, "plain");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_value,
    MU_RUN_TEST(test_asdf_value_as_int64)
);


MU_RUN_SUITE(test_asdf_value);
