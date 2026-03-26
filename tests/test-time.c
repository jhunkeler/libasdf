#include <stddef.h>
#include <stdint.h>

#include "munit.h"
#include "util.h"

#include <asdf/core/asdf.h>
#include <asdf/core/time.h>
#include <asdf/file.h>
#include <asdf/value.h>


MU_TEST(test_asdf_time) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_value_t *value = NULL;

    /* buffer for formatted time string */
    char time_str[255] = {0};

    const char *fixture_keys[] = {
        "t_iso_time",
        "t_datetime",
        "t_yday",
        "t_unix",
        "t_jd",
        "t_mjd",
        "t_byear",
    };

    asdf_time_t *t = NULL;
    for (size_t idx = 0; idx < sizeof(fixture_keys) / sizeof(fixture_keys[0]); idx++) {
        const char *key = fixture_keys[idx];
        assert_true(asdf_is_time(file, key));

        value = asdf_get_value(file, key);
        assert_not_null(value);

        asdf_value_err_t err = asdf_value_as_time(value, &t);
        if (err != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed: %s\n", key);
            asdf_time_destroy(t);
            asdf_value_destroy(value);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        assert_not_null(t);
        assert_not_null(t->value);

        time_t x = t->info.ts.tv_sec;
        strftime(time_str, sizeof(time_str), "%m/%d/%Y %T %Z", gmtime(&x));
        printf("[%zu] key: %10s, value: %30s,  time: %10s\n", idx, key, t->value, time_str);

        asdf_time_destroy(t);
        t = NULL;
        memset(time_str, 0, sizeof(time_str));
        asdf_value_destroy(value);
        value = NULL;
    }

    asdf_close(file);

    return MUNIT_OK;
}


MU_TEST(test_asdf_time_serialize) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    assert_not_null(path);

    /* Write a time value to a new file */
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    char time_value[] = "2025-10-14T13:26:41.0000";
    asdf_time_t time_obj = {
        .value = time_value,
        .format = {.is_base_format = true, .type = ASDF_TIME_FORMAT_ISO_TIME},
        .scale = ASDF_TIME_SCALE_UTC,
    };

    asdf_value_err_t err = asdf_set_time(file, "t_write", &time_obj);
    assert_int(err, ==, ASDF_VALUE_OK);

    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    /* Re-open and verify the round-trip */
    file = asdf_open(path, "r");
    assert_not_null(file);

    assert_true(asdf_is_time(file, "t_write"));

    asdf_time_t *t_out = NULL;
    err = asdf_get_time(file, "t_write", &t_out);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(t_out);
    assert_not_null(t_out->value);
    assert_string_equal(t_out->value, time_value);
    assert_int(t_out->format.type, ==, ASDF_TIME_FORMAT_ISO_TIME);

    asdf_time_destroy(t_out);
    asdf_close(file);

    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_time_extension,
    MU_RUN_TEST(test_asdf_time),
    MU_RUN_TEST(test_asdf_time_serialize)
);


MU_RUN_SUITE(test_asdf_time_extension);
