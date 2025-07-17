#include <stdint.h>

#include <libfyaml.h>

#include <asdf/file.h>
#include <asdf/value.h>

#include "munit.h"
#include "util.h"


MU_TEST(test_asdf_value_as_int64) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get(file, "int64");
    assert_not_null(value);
    int64_t int64 = 0;
    asdf_value_err_t err = asdf_value_as_int64(value, &int64);
    assert_int(err, ==, ASDF_VALUE_OK);
    int64_t expected = 9223372036854775807LL;
    assert_int(int64, ==, expected);
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_value,
    MU_RUN_TEST(test_asdf_value_as_int64)
);


MU_RUN_SUITE(test_asdf_value);
