#include <libfyaml.h>

#include <asdf/value.h>

#include "munit.h"

#include "file.h"
#include "value.h"


/*
 * Very basic test of the `asdf_open_file` interface
 *
 * Tests opening/closing file, and reading the tree document with libfyaml.
 */
MU_TEST(test_asdf_common_tag_get) {
    asdf_yaml_common_tag_t tag = asdf_common_tag_get("tag:yaml.org,2002:str");
    assert_int(tag, ==, ASDF_YAML_COMMON_TAG_STR);
    asdf_yaml_common_tag_t unknown = asdf_common_tag_get("unknown tag");
    assert_int(unknown, ==, ASDF_YAML_COMMON_TAG_UNKNOWN);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_value,
    MU_RUN_TEST(test_asdf_common_tag_get)
);


MU_RUN_SUITE(test_asdf_value);
