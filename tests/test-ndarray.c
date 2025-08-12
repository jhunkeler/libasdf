#include <stdint.h>

#include <asdf/core/ndarray.h>
#include <asdf/file.h>

#include "munit.h"
#include "util.h"


/* Read contiguous 1-D "tiles" from arrays of different shapes */
MU_TEST(test_asdf_ndarray_read_1d_tile_contiguous) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;

    /* Read tile from a 1-D array */
    assert_int(asdf_get_ndarray(file, "1d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin1[] = {1};
    uint64_t shape1[] = {2};
    uint8_t expected1[] = {2, 3};
    void* tile = NULL;
    assert_int(asdf_ndarray_read_tile_ndim(ndarray, origin1, shape1, &tile), ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(2 * sizeof(uint8_t), tile, expected1);
    free(tile);
    asdf_ndarray_destroy(ndarray);

    /* Read tile from a 2-D array */
    assert_int(asdf_get_ndarray(file, "2d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin2[] = {1, 1};
    uint64_t shape2[] = {1, 2};
    uint16_t expected2[] = {22, 23};
    tile = NULL;
    assert_int(asdf_ndarray_read_tile_ndim(ndarray, origin2, shape2, &tile), ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(2 * sizeof(uint16_t), tile, expected2);
    free(tile);
    asdf_ndarray_destroy(ndarray);

    /* Read tile from a 3-D array */
    assert_int(asdf_get_ndarray(file, "3d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin3[] = {1, 1, 1};
    uint64_t shape3[] = {1, 1, 2};
    int32_t expected3[] = {222, 223};
    tile = NULL;
    assert_int(asdf_ndarray_read_tile_ndim(ndarray, origin3, shape3, &tile), ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(2 * sizeof(int32_t), tile, expected3);
    free(tile);
    asdf_ndarray_destroy(ndarray);

    asdf_close(file);
    return MUNIT_OK;
}


/* Read 2-D tiles from 2-D and 3-D arrays */
MU_TEST(test_asdf_ndarray_read_2d_tile) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;

    /* Read tile from a 2-D array */
    assert_int(asdf_get_ndarray(file, "2d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint16_t expected2[2][2] = {{22, 23}, {32, 33}};
    void* tile = NULL;
    assert_int(asdf_ndarray_read_tile_2d(ndarray, 1, 1, 2, 2, NULL, &tile), ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(4 * sizeof(uint16_t), tile, expected2);
    free(tile);
    asdf_ndarray_destroy(ndarray);

    /* Read 2-D tile from the 1th layer of a 3-D array */
    assert_int(asdf_get_ndarray(file, "3d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin3[] = {1};
    int32_t expected3[2][2] = {{222, 223}, {232, 233}};
    tile = NULL;
    assert_int(asdf_ndarray_read_tile_2d(ndarray, 1, 1, 2, 2, origin3, &tile), ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(4 * sizeof(int32_t), tile, expected3);
    free(tile);
    asdf_ndarray_destroy(ndarray);

    asdf_close(file);
    return MUNIT_OK;
}


/* Read a 3-D cube from a a 3-D array */
MU_TEST(test_asdf_ndarray_read_3d_tile) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "3d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin3[] = {1, 1, 1};
    uint64_t shape3[] = {2, 2, 2};
    int32_t expected3[2][2][2] = {{{222, 223}, {232, 233}}, {{322, 323}, {332, 333}}};
    void *tile = NULL;
    assert_int(asdf_ndarray_read_tile_ndim(ndarray, origin3, shape3, &tile), ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(8 * sizeof(int32_t), tile, expected3);
    free(tile);
    asdf_ndarray_destroy(ndarray);

    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_ndarray,
    MU_RUN_TEST(test_asdf_ndarray_read_1d_tile_contiguous),
    MU_RUN_TEST(test_asdf_ndarray_read_2d_tile),
    MU_RUN_TEST(test_asdf_ndarray_read_3d_tile)
);


MU_RUN_SUITE(test_asdf_ndarray);
