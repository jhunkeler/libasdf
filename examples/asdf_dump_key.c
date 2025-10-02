// Most of the common "high-level" user APIs are included directly from `asdf.h` though
// more should be added there later.
#include <asdf.h>

// E.g. this isn't included in asdf.h yet
#include "src/util.h"

#include <asdf/core/asdf.h>
#include <asdf/core/ndarray.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

#ifdef HAVE_LZ4
#include <lz4.h>
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "src/block.h"
#include "src/file.h"
#include "src/value_util.h"
#include "stc/common.h"

#define CHUNK_SIZE 1024 * 64

// Generate a printf formatter for a ndarray datatype
static char *get_formatter(const asdf_scalar_datatype_t datatype) {
    static char fmt_s[255] = {0};
    switch (datatype) {
        case ASDF_DATATYPE_BOOL8:
        case ASDF_DATATYPE_INT8:
        case ASDF_DATATYPE_INT16:
        case ASDF_DATATYPE_INT32:
            strncpy(fmt_s, "%8d", sizeof(fmt_s) - 1);
            break;
        case ASDF_DATATYPE_INT64:
            strncpy(fmt_s, "%8zd", sizeof(fmt_s) - 1);
            break;
        case ASDF_DATATYPE_UINT8:
        case ASDF_DATATYPE_UINT16:
        case ASDF_DATATYPE_UINT32:
            strncpy(fmt_s, "%8u", sizeof(fmt_s) - 1);
            break;
        case ASDF_DATATYPE_UINT64:
            strncpy(fmt_s, "%8zu", sizeof(fmt_s) - 1);
            break;
        case ASDF_DATATYPE_FLOAT16:
        case ASDF_DATATYPE_FLOAT32:
            strncpy(fmt_s, "%12g", sizeof(fmt_s) - 1);
            break;
        case ASDF_DATATYPE_FLOAT64:
            strncpy(fmt_s, "%12lg", sizeof(fmt_s) - 1);
            break;
        case ASDF_DATATYPE_UCS4:
        case ASDF_DATATYPE_ASCII:
            strncpy(fmt_s, "'%c'", sizeof(fmt_s) - 1);
            break;
        // unhandled
        case ASDF_DATATYPE_RECORD:
        case ASDF_DATATYPE_COMPLEX64:
        case ASDF_DATATYPE_COMPLEX128:
        case ASDF_DATATYPE_UNKNOWN:
        default:
            strncpy(fmt_s, "%p", sizeof(fmt_s) - 1);
            break;
    }
    return fmt_s;
}

#define PRINT_VALUE_INDEX 0     // [  0..][  0..] = VALUE
                                // [..row][..col] = VALUE

#define PRINT_VALUE_ROW 1     // Row n
                              // VALUE (8x)

#define REPR(TYPE) \
    do { \
        const TYPE *value = data; \
        printf(repr, row, col, *value); \
    } while (0)

#define REPR_CH(TYPE) \
    do { \
        const TYPE *value = data; \
        printf(repr, value[0]); \
    } while (0)


static void print_value(const asdf_scalar_datatype_t datatype, const void *data, const size_t row, const size_t col, const int method) {
    char repr[BUFSIZ] = {0};
    static int x = 0;

    if (method == PRINT_VALUE_INDEX) {
        snprintf(repr, sizeof(repr), "[%%4zu][%%4zu] = %s\n", get_formatter(datatype));
    } else if (method == PRINT_VALUE_ROW) {
        if (col == 0) {
            snprintf(repr, sizeof(repr), "\nRow %zu\n", row);
        }
        snprintf(repr + strlen(repr), sizeof(repr), "%s ", get_formatter(datatype));
    } else {
        fprintf(stderr, "Unknown display method: %d\n", method);
        return;
    }

    switch (datatype) {
        case ASDF_DATATYPE_FLOAT16:
        case ASDF_DATATYPE_FLOAT32:
            REPR(float);
            break;
        case ASDF_DATATYPE_FLOAT64:
            REPR(double);
            break;
        case ASDF_DATATYPE_INT8:
            REPR(int8_t);
            break;
        case ASDF_DATATYPE_INT16:
            REPR(int16_t);
            break;
        case ASDF_DATATYPE_INT32:
            REPR(int32_t);
            break;
        case ASDF_DATATYPE_INT64:
            REPR(int64_t);
            break;
        case ASDF_DATATYPE_UINT8:
            REPR(uint8_t);
            break;
        case ASDF_DATATYPE_UINT16:
            REPR(uint16_t);
            break;
        case ASDF_DATATYPE_UINT32:
            REPR(uint32_t);
            break;
        case ASDF_DATATYPE_UINT64:
            REPR(uint64_t);
            break;
        case ASDF_DATATYPE_BOOL8:
            REPR(bool);
            break;
        case ASDF_DATATYPE_UCS4:
            REPR_CH(uint32_t);
            break;
        case ASDF_DATATYPE_ASCII:
            REPR_CH(uint8_t);
            break;
        default:
            break;
    }

    x++;
    if (method == PRINT_VALUE_ROW && x >= 8) {
        printf("\n");
        x = 0;
    }
}

// Dump the contents of a ndarray
static void show_ndarray(const struct asdf_ndarray *ndarray, const void *data, const int method) {
    const void *p = NULL;
    if (ndarray->ndim > 1) {
        const size_t rows = ndarray->shape[0];
        const size_t cols = ndarray->shape[1];

        for (size_t row = 0; row < rows; row++) {
            for (size_t col = 0; col < cols; col++) {
                p = data + (row * cols + col) * asdf_ndarray_scalar_datatype_size(ndarray->datatype.type);
                print_value(ndarray->datatype.type, p, row, col, method);
            }
        }
    } else {
        for (size_t i = 0; i < ndarray->shape[0]; i++) {
            p = data + i * asdf_ndarray_scalar_datatype_size(ndarray->datatype.type);
            print_value(ndarray->datatype.type, p, 0, i, method);
        }
    }
}

static int decompress_bzip2(void **dest, const size_t dest_size, const void *src, const size_t src_size) {
    int ret = 0;
    BZFILE *bzf = NULL;
    FILE *src_fp = NULL;
    FILE *dest_fp = NULL;

    src_fp = fmemopen((char *) src, src_size, "r+b");
    if (!src_fp) {
        fprintf(stderr, "fmemopen of src buffer failed: %s\n", strerror(errno));
        goto bzip2_failure;
    }

    dest_fp = fmemopen(*dest, dest_size, "w+b");
    if (!dest_fp) {
        fprintf(stderr, "fmemopen of dest buffer failed: %s\n", strerror(errno));
        goto bzip2_failure;
    }

    bzf = BZ2_bzReadOpen(&ret, src_fp, 0, 0, NULL, 0);
    if (!bzf) {
        fprintf(stderr, "BZ2_bzReadOpen failed\n");
        goto bzip2_failure;
    }

    if (ret != BZ_OK) {
        fprintf(stderr, "BZ2_bzReadOpen failed: %d\n", ret);
        goto bzip2_failure;
    }

    do {
        char buf[CHUNK_SIZE] = {0};
        // read compressed data from the source
        const size_t bytes_read = BZ2_bzRead(&ret, bzf, buf, CHUNK_SIZE);
        if (ret == BZ_OK || ret == BZ_STREAM_END) {
            // write decompressed data to the destination
            const size_t bytes_written = fwrite(buf, 1, sizeof(buf), dest_fp);
            if (bytes_written != bytes_read) {
                fprintf(stderr, "short write\n");
                goto bzip2_failure;
            }
        }
    } while (ret == BZ_OK);

    if (ret != BZ_STREAM_END) {
        fprintf(stderr, "read error: %d\n", ret);
        goto bzip2_failure;
    }

    // clean up
    BZ2_bzReadClose(&ret, bzf);
    fclose(dest_fp);
    fclose(src_fp);
    goto decompression_success;

    bzip2_failure:
    if (src_fp != NULL) {
        fclose(src_fp);
    }
    if (dest_fp != NULL) {
        fclose(dest_fp);
    }
    if (bzf != NULL) {
        BZ2_bzReadClose(&ret, bzf);
    }

    decompression_success:
    return ret;
}

static int decompress_zlib(void **dest, const size_t dest_size, const void *src, const size_t src_size) {
    uLong size_src = src_size;
    uLong size_dest = dest_size;

    const int ret = uncompress2(*dest, &size_dest, src, &size_src);
    if (ret != Z_OK) {
        switch (ret) {
            case Z_DATA_ERROR:
                fprintf(stderr, "zdata error\n");
                break;
            case Z_MEM_ERROR:
                fprintf(stderr, "zmemory error\n");
                break;
            case Z_BUF_ERROR:
                fprintf(stderr, "zbuf error\n");
                break;
            case Z_VERSION_ERROR:
                fprintf(stderr, "zversion error\n");
                break;
            default:
                fprintf(stderr, "unrecognized zlib error\n");
                break;
        }
    }
    return ret;
}

static int decompress_lz4(void **dest, const size_t dest_size, const void *src, const size_t src_size) {
    const int start = 8;
    return LZ4_decompress_safe((char *) src + start, (void *) *dest, src_size - start, dest_size);
}

static unsigned char *decompress_data(const char *compression_type, const unsigned char *data,
                                        const size_t compressed_size, const size_t src_size) {
    int ret = 0;
    unsigned char *result = calloc(src_size, sizeof(*result));
    if (!result) {
        fprintf(stderr, "unable to allocate %zu bytes for result buffer\n", src_size);
        return NULL;
    }

    if (!strcmp(compression_type, "zlib")) {
        #ifdef HAVE_ZLIB
        ret = decompress_zlib((void *) &result, src_size, data, compressed_size);
        if (ret != Z_OK) {
            goto decompression_failure;
        }
        #else
        goto no_library;
        #endif
    } else if (!strcmp(compression_type, "bzp2")) {
        #ifdef HAVE_BZIP2
        ret = decompress_bzip2((void *) &result, src_size, data, compressed_size);
        if (ret != BZ_OK) {
            goto decompression_failure;
        }
        #else
        goto no_library;
        #endif
    } else if (!strcmp(compression_type, "lz4")) {
        #ifdef HAVE_LZ4
        ret = decompress_lz4((void *) &result, src_size, data, compressed_size);
        if (ret < 0) {
            goto decompression_failure;
        }
        #else
        goto no_library;
        #endif
    } else {
        fprintf(stderr, "compression type '%s' is not implemented\n", compression_type);
        goto decompression_failure;
    }

    goto decompression_success;

    // Final clean up for any errors
    decompression_failure:
    fprintf(stderr, "decompression failed: %d\n", ret);
    free(result);
    result = NULL;

    decompression_success:
    return result;

    // Triggered only if one of the supported compression libraries wasn't available at build-time
    no_library:
    fprintf(stderr, "%s is supported but not enabled\n", compression_type);
    free(result);
    return NULL;
}

static void usage(const char *program_name) {
    fprintf(stderr, "%s {filename} {datakey} [dump_method]\n", program_name);
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "    filename       required    path to ASDF file\n");
    fprintf(stderr, "    datakey        required    YAML tree path to data\n");
    fprintf(stderr, "    dump_method    optional    index, row [default: index]\n");
}

int main(int argc, char *argv[]) {
    const char *filename = argv[1];
    const char *datakey = argv[2];
    const char *dump_method = argv[3];

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments\n");
        usage(argv[0]);
        return 1;
    }

    if (!datakey || !strlen(datakey)) {
        fprintf(stderr, "data key is required, and cannot be empty\n");
        return 1;
    }

    // set output style
    int method = PRINT_VALUE_INDEX;
    if (argc > 2) {
        if (dump_method && strcmp(dump_method, "row") == 0) {
            method = PRINT_VALUE_ROW;
        } else if (dump_method && strcmp(dump_method, "index") == 0) {
            method = PRINT_VALUE_INDEX;
        } else if (dump_method) {
            fprintf(stderr, "Unknown dump method '%s'\n", dump_method);
            return 1;
        }
    }

    printf("# File:     %s\n", filename);
    printf("# Data key: %s\n", datakey);

    // Simplest way to open a file is just asdf_open_file; there is a mode string but it's not used yet so just pass "r"
    asdf_file_t *file = asdf_open_file(filename, "r");

    // Returns NULL if there was an error
    // TODO: How to get the error code?  There is some error handling code for errors on files, but not when a file
    // isn't already open
    if (file == NULL) {
        fprintf(stderr, "error opening asdf file\n");
        return 1;
    }

    // ndarrays work no differently
    asdf_value_err_t err = 0;
    asdf_ndarray_t *ndarray = NULL;
    err = asdf_get_ndarray(file, datakey, &ndarray);
    if (err != ASDF_VALUE_OK) {
        fprintf(stderr, "error reading ndarray data from '%s': %d\n", datakey, err);
        return 1;
    }

    printf("# DATA\n");
    printf("# Dimensions: %d\n", ndarray->ndim);
    printf("# Shape: [");
    for (size_t i = 0; i < ndarray->ndim; i++ ) {
        printf("%zu", ndarray->shape[i]);
        if (i < ndarray->ndim - 1) {
            printf(", ");
        }
    }
    printf("]\n");

    // Get just a raw pointer to the ndarray data block (if uncompressed), uses mmap if possible
    // Optionally returns the size in bytes as well
    size_t size = 0;
    asdf_block_t *block = NULL;
    void *xdata = asdf_ndarray_data_raw_ex(ndarray, (void *) &block, &size);
    if (!xdata) {
        fprintf(stderr, "error reading ndarray data from '%s': %d\n", datakey, err);
    }

    const size_t compressed_size = block->info.header.used_size;
    const size_t src_size = block->info.header.data_size;

    void *data = xdata;
    const char *compressed = block->info.header.compression;
    if (compressed && strlen(compressed)) {
        printf("# Data is compressed with %s\n", compressed);
        data = decompress_data(compressed, xdata, compressed_size, src_size);
        if (!data) {
            fprintf(stderr, "error decompressing ndarray data from '%s': %d\n", datakey, err);
            return 1;
        }
        printf("# Compressed size: %zu bytes\n", size);
    } else {
        printf("# Data is not compressed\n");
    }

    printf("# Size: %zu bytes\n", src_size);
    printf("# ----\n");

    // Dump the data
    show_ndarray(ndarray, data, method);

    // If we're not using the original data (i.e. decompression took place), free the data
    if (data != xdata) {
        free(data);
    }
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);

    return 0;
}
