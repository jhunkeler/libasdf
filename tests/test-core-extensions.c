#include "munit.h"
#include "util.h"

#include <asdf/core/history_entry.h>
#include <asdf/core/software.h>
#include <asdf/file.h>


MU_TEST(test_asdf_software) {
    const char *path = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    assert_true(asdf_is_software(file, "asdf_library"));
    asdf_software_t *software = NULL;
    assert_int(asdf_get_software(file, "asdf_library", &software), ==, ASDF_VALUE_OK);
    assert_not_null(software);
    assert_string_equal(software->name, "asdf");
    assert_string_equal(software->version, "4.1.0");
    assert_string_equal(software->homepage, "http://github.com/asdf-format/asdf");
    assert_string_equal(software->author, "The ASDF Developers");
    asdf_software_destroy(software);
    asdf_close(file);
    return MUNIT_OK;
}


/* TODO: Should have more tests for this, for now just using one test case that's already lying
 * around...
 */
MU_TEST(test_asdf_history_entry) {
    const char *path = get_fixture_file_path("255.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "history/entries/0");
    assert_not_null(value);
    assert_true(asdf_value_is_history_entry(value));
    asdf_history_entry_t *entry = NULL;
    assert_int(asdf_value_as_history_entry(value, &entry), ==, ASDF_VALUE_OK);
    assert_not_null(entry);
    assert_string_equal(entry->description, "test file containing integers from 0 to 255 in the "
                        "block data, for simple tests against known data");
    assert_int(entry->time.tv_sec, ==, 1753264575);
    assert_int(entry->time.tv_nsec, ==, 0);
    // TODO: Oops, current test case does not include software used to write the history entry
    // A little strange that Python asdf excludes it...maybe no one cares because it's the only
    // code writing asdf history entries?  Need more test cases...
    assert_null(entry->software);
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_core_extensions,
    MU_RUN_TEST(test_asdf_software)
);


MU_RUN_SUITE(test_asdf_core_extensions);
