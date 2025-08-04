#include "munit.h"
#include "util.h"

#include <asdf/core/software.h>
#include <asdf/file.h>


MU_TEST(test_asdf_software) {
    const char *path = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_true(asdf_is_software(file, "asdf_library"));
    asdf_software_t *software = NULL;
    assert_int(asdf_get_software(file, "asdf_library", &software), ==, ASDF_VALUE_OK);
    assert_not_null(software);
    assert_string_equal(software->name, "asdf");
    assert_string_equal(software->version, "4.1.0");
    assert_string_equal(software->homepage, "http://github.com/asdf-format/asdf");
    assert_string_equal(software->author, "The ASDF Developers");
    asdf_software_destroy(software);
    assert_not_null(file);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_core_extensions,
    MU_RUN_TEST(test_asdf_software)
);


MU_RUN_SUITE(test_asdf_core_extensions);
