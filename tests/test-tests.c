/**
 * Tests for the test framework and utilities themselves
 *
 * The unit tests use munit (https://nemequ.github.io/munit/) but with some
 * rather sneaky wrappers around it.  There are also a few test utilities.
 */

#include <sys/stat.h>

#include "munit.h"
#include "util.h"


MU_TEST(test_test_name_1) {
    assert_string_equal(fixture->suite_name, "tests");
    assert_string_equal(fixture->test_name, "test_test_name_1");
    assert_string_equal(fixture->tempfile_prefix, "tests-test_test_name_1-");
    return MUNIT_OK;
}


/** Same as ``test_test_name_1`` but ensures a different test name :) */
MU_TEST(test_test_name_2) {
    assert_string_equal(fixture->suite_name, "tests");
    assert_string_equal(fixture->test_name, "test_test_name_2");
    assert_string_equal(fixture->tempfile_prefix, "tests-test_test_name_2-");
    return MUNIT_OK;
}


MU_TEST(test_get_temp_file_path) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    assert_not_null(filename);
    /* File should exist and live under the run directory */
    struct stat st;
    assert_int(stat(filename, &st), ==, 0);
    const char *run_dir = get_run_dir();
    size_t run_dir_len = strlen(run_dir);
    size_t prefix_len = strlen(fixture->tempfile_prefix);
    /* get_temp_file_path strips the trailing '-' before a '.' suffix, so the
     * filename is run_dir + "/" + prefix_without_trailing_dash + ".asdf" */
    assert_int(strlen(filename), ==, run_dir_len + 1 + (prefix_len - 1) + 5);
    assert_memory_equal(run_dir_len, filename, run_dir);
    assert_char(filename[run_dir_len], ==, '/');
    assert_memory_equal(prefix_len - 1, filename + run_dir_len + 1, fixture->tempfile_prefix);
    assert_string_equal(filename + run_dir_len + 1 + prefix_len - 1, ".asdf");
    return MUNIT_OK;
}


MU_TEST_SUITE(
    tests,
    MU_RUN_TEST(test_test_name_1),
    MU_RUN_TEST(test_test_name_2),
    MU_RUN_TEST(test_get_temp_file_path)
);


MU_RUN_SUITE(tests);
