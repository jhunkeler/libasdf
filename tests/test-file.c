#include <libfyaml.h>

#include <asdf/file.h>

#include "munit.h"

#include "file.h"
#include "util.h"


/*
 * Very basic test of the `asdf_open_file` interface
 *
 * Tests opening/closing file, and reading the tree document with libfyaml.
 */
MU_TEST(test_asdf_open_file) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    struct fy_document *tree = asdf_file_get_tree_document(file);
    assert_not_null(tree);
    /* Read some key out of the tree with libfyaml */
    // TODO: (gh-28) use the asdf_value API instead of using libfyaml directly
    struct fy_node *root = fy_document_root(tree);
    struct fy_node *asdf_library_name_node = fy_node_by_path(
        root, "asdf_library/name", -1, FYNWF_PTR_DEFAULT);
    assert_not_null(asdf_library_name_node);
    assert_true(fy_node_is_scalar(asdf_library_name_node));
    assert_string_equal(fy_node_get_scalar0(asdf_library_name_node), "asdf");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_file,
    MU_RUN_TEST(test_asdf_open_file)
);


MU_RUN_SUITE(test_asdf_file);
