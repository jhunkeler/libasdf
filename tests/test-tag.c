/** Tag parsing tests */
#include "munit.h"

#include "asdf/extension.h"


MU_TEST(test_asdf_tag_parse) {
    asdf_tag_t *tag = asdf_tag_parse("tag:stsci.edu:gwcs/frame-1.0.0");
    assert_not_null(tag);
    assert_string_equal(tag->name, "tag:stsci.edu:gwcs/frame");
    assert_not_null(tag->version);
    assert_string_equal(tag->version->version, "1.0.0");
    assert_int(tag->version->major, ==, 1);
    assert_int(tag->version->minor, ==, 0);
    assert_int(tag->version->patch, ==, 0);
    asdf_tag_destroy(tag);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    tag,
    MU_RUN_TEST(test_asdf_tag_parse)
);


MU_RUN_SUITE(tag);
