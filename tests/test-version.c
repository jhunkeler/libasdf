/** Version parsing tests */
#include "munit.h"

#include "asdf/version.h"


MU_TEST(test_asdf_version_parse_unknown) {
    asdf_version_t *version = asdf_version_parse("2026-04-15");
    assert_not_null(version);
    assert_string_equal(version->version, "2026-04-15");
    assert_int(version->major, ==, 2026);
    assert_int(version->minor, ==, 0);
    assert_int(version->patch, ==, 0);
    assert_string_equal(version->extra, "-04-15");
    asdf_version_destroy(version);
    return MUNIT_OK;
}


MU_TEST(test_asdf_version_parse_partial) {
    asdf_version_t *version = asdf_version_parse("1");
    assert_not_null(version);
    assert_string_equal(version->version, "1");
    assert_int(version->major, ==, 1);
    assert_int(version->minor, ==, 0);
    assert_int(version->patch, ==, 0);
    assert_null(version->extra);
    asdf_version_destroy(version);

    version = asdf_version_parse("1.1");
    assert_not_null(version);
    assert_string_equal(version->version, "1.1");
    assert_int(version->major, ==, 1);
    assert_int(version->minor, ==, 1);
    assert_int(version->patch, ==, 0);
    assert_null(version->extra);
    asdf_version_destroy(version);

    version = asdf_version_parse("1.1asdf");
    assert_not_null(version);
    assert_string_equal(version->version, "1.1asdf");
    assert_int(version->major, ==, 1);
    assert_int(version->minor, ==, 1);
    assert_int(version->patch, ==, 0);
    assert_string_equal(version->extra, "asdf");
    asdf_version_destroy(version);
    return MUNIT_OK;
}


MU_TEST(test_asdf_version_parse) {
    asdf_version_t *version = asdf_version_parse("1.2.3");
    assert_not_null(version);
    assert_string_equal(version->version, "1.2.3");
    assert_int(version->major, ==, 1);
    assert_int(version->minor, ==, 2);
    assert_int(version->patch, ==, 3);
    assert_null(version->extra);
    asdf_version_destroy(version);
    return MUNIT_OK;
}


MU_TEST(test_asdf_version_parse_extra) {
    asdf_version_t *version = asdf_version_parse("1.2.3asdf");
    assert_not_null(version);
    assert_string_equal(version->version, "1.2.3asdf");
    assert_int(version->major, ==, 1);
    assert_int(version->minor, ==, 2);
    assert_int(version->patch, ==, 3);
    assert_string_equal(version->extra, "asdf");
    asdf_version_destroy(version);

    version = asdf_version_parse("1.2.3.asdf");
    assert_not_null(version);
    assert_string_equal(version->version, "1.2.3.asdf");
    assert_int(version->major, ==, 1);
    assert_int(version->minor, ==, 2);
    assert_int(version->patch, ==, 3);
    assert_string_equal(version->extra, "asdf");
    asdf_version_destroy(version);

    version = asdf_version_parse("1.2.3-asdf");
    assert_not_null(version);
    assert_string_equal(version->version, "1.2.3-asdf");
    assert_int(version->major, ==, 1);
    assert_int(version->minor, ==, 2);
    assert_int(version->patch, ==, 3);
    assert_string_equal(version->extra, "asdf");
    asdf_version_destroy(version);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    version,
    MU_RUN_TEST(test_asdf_version_parse_unknown),
    MU_RUN_TEST(test_asdf_version_parse_partial),
    MU_RUN_TEST(test_asdf_version_parse),
    MU_RUN_TEST(test_asdf_version_parse_extra)
);


MU_RUN_SUITE(version);
