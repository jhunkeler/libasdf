/**
 * Internal data structures for the stream interfaces; do no export
 */
#pragma once

#include <stdint.h>
#include <stdio.h>


typedef struct {
    FILE *file;
    const char *filename;
    bool should_close;
    uint8_t *buf;
    size_t buf_size;
    size_t buf_avail;
    size_t buf_pos;
    off_t offset;
} file_userdata_t;


typedef struct {
    const uint8_t *buf;
    size_t size;
    size_t pos;
} mem_userdata_t;
