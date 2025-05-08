/**
 * ASDF block definitions
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>


extern const unsigned char ASDF_BLOCK_MAGIC[];


typedef struct asdf_block_header {
    //  4 MAGIC 4 char == "\323BLK"
    //char magic[4];
    // = {0xd3, "B", "L", "K"};

    //  2 HEADER SIZE 16 bit unsigned int
    //     doesn't include magic or header size (used to optionally align
    //     blocks with filesystem blocks)
    uint16_t header_size;

    //  4 FLAGS 32 bit unsigned int (only first bit used for stream block)
    uint32_t flags;

    //  4 COMPRESSION 4 char
    char compression[4];

    //  8 ALLOCATED SIZE 64 bit unsigned int
    uint64_t allocated_size;

    //  8 USED SIZE 64 bit unsigned int
    uint64_t used_size;

    //  8 DATA SIZE 64 bit unsigned int
    uint64_t data_size;

    // 16 CHECKSUM 16 char MD5 checksum (optional)
    char checksum[16];
} asdf_block_header_t;


typedef struct asdf_block_info {
    asdf_block_header_t header;
    off_t header_pos;
    off_t data_pos;
}  asdf_block_info_t;
