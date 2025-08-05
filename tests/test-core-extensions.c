#include "munit.h"
#include "util.h"

#include <asdf/core/extension_metadata.h>
#include <asdf/core/history_entry.h>
#include <asdf/core/software.h>
#include <asdf/file.h>


/* TODO: Should have more tests for this, for now just using one test case that's already lying
 * around...
 */
MU_TEST(test_asdf_extension_metadata) {
    const char *path = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "history/extensions/0");
    assert_not_null(value);
    assert_true(asdf_value_is_extension_metadata(value));
    asdf_extension_metadata_t *metadata = NULL;
    assert_int(asdf_value_as_extension_metadata(value, &metadata), ==, ASDF_VALUE_OK);
    assert_not_null(metadata);
    assert_string_equal(metadata->extension_class, "asdf.extension._manifest.ManifestExtension");
    assert_null(metadata->package);
    assert_not_null(metadata->metadata);
    assert_true(asdf_value_is_mapping(metadata->metadata));
    
    asdf_value_t *prop = asdf_mapping_get(metadata->metadata, "extension_uri");
    assert_not_null(prop);
    const char *s = NULL;
    assert_int(asdf_value_as_string0(prop, &s), ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "asdf://asdf-format.org/core/extensions/core-1.6.0");
    asdf_value_destroy(prop);

    prop = asdf_mapping_get(metadata->metadata, "manifest_software");
    assert_not_null(prop);
    assert_true(asdf_value_is_software(prop));
    asdf_software_t *software = NULL;
    assert_int(asdf_value_as_software(prop, &software), ==, ASDF_VALUE_OK);
    assert_not_null(software);
    assert_string_equal(software->name, "asdf_standard");
    assert_string_equal(software->version, "1.1.1");
    asdf_value_destroy(prop);

    prop = asdf_mapping_get(metadata->metadata, "software");
    assert_not_null(prop);
    assert_true(asdf_value_is_software(prop));
    software = NULL;
    assert_int(asdf_value_as_software(prop, &software), ==, ASDF_VALUE_OK);
    assert_not_null(software);
    assert_string_equal(software->name, "asdf");
    assert_string_equal(software->version, "4.1.0");
    asdf_value_destroy(prop);

    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


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
    assert_int(entry->time.tv_sec, ==, 1753271775);
    assert_int(entry->time.tv_nsec, ==, 0);
    // TODO: Oops, current test case does not include software used to write the history entry
    // A little strange that Python asdf excludes it...maybe no one cares because it's the only
    // code writing asdf history entries?  Need more test cases...
    assert_null(entry->software);
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


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


MU_TEST_SUITE(
    test_asdf_core_extensions,
    MU_RUN_TEST(test_asdf_extension_metadata),
    MU_RUN_TEST(test_asdf_history_entry),
    MU_RUN_TEST(test_asdf_software)
);


MU_RUN_SUITE(test_asdf_core_extensions);
