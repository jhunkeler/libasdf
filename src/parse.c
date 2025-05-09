#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "block.h"
#include "event.h"
#include "parse.h"


const char *ASDF_STANDARD_COMMENT = "#ASDF_STANDARD ";
const char *ASDF_VERSION_COMMENT = "#ASDF ";


static const char *const parser_error_messages[] = {
    [ASDF_ERR_NONE] = NULL,
    [ASDF_ERR_UNKNOWN_STATE] = "Unknown parser state",
    [ASDF_ERR_UNEXPECTED_EOF] = "Unexpected end of file",
    [ASDF_ERR_INVALID_ASDF_HEADER] = "Invalid ASDF header",
    [ASDF_ERR_INVALID_BLOCK_HEADER] = "Invalid block header",
    [ASDF_ERR_BLOCK_MAGIC_MISMATCH] = "Block magic mismatch",
    [ASDF_ERR_YAML_PARSER_INIT_FAILED] = "YAML parser initialization failed",
    [ASDF_ERR_YAML_PARSE_FAILED] = "YAML parsing failed",
    [ASDF_ERR_OUT_OF_MEMORY] = "Out of memory",
};


#define ASDF_ERR(code) (parser_error_messages[(code)])
#define ASDF_SET_STD_ERR(parser, code) set_static_error(parser, ASDF_ERR(code))


static void set_oom_error(asdf_parser_t *parser) {
    parser->error = ASDF_ERR(ASDF_ERR_OUT_OF_MEMORY);
    parser->error_type = ASDF_ERROR_STATIC;
    return;
}


static void set_error(asdf_parser_t *parser, const char *fmt, ...) {
    va_list args;
    int size;
    assert(parser);

    if (parser->error_type == ASDF_ERROR_HEAP)
        // Heap-allocated errors can be safely cast to (void *)
        // and freed.
        free((void *)parser->error);

    parser->error = NULL;
    parser->state = ASDF_PARSER_STATE_ERROR;

    va_start(args, fmt);

    size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // Ensure space for null termination
    char *error = malloc(size + 1);

    if (!error) {
        set_oom_error(parser);
        return;
    }

    va_start(args, fmt);
    vsnprintf(error, size + 1, fmt, args);
    va_end(args);
    parser->error = error; // implicit char* -> const char*; OK
    parser->error_type = ASDF_ERROR_HEAP;
}


void set_static_error(asdf_parser_t *parser, const char *error) {
    if (parser->error_type == ASDF_ERROR_HEAP)
        free((void *)parser->error);

    parser->error = error;
    parser->error_type = ASDF_ERROR_STATIC;
    parser->state = ASDF_PARSER_STATE_ERROR;
}


static char *read_next(asdf_parser_t *parser) {
    return fgets(parser->read_buffer, ASDF_PARSER_READ_BUFFER_SIZE, parser->file);
}


static int parse_version_comment(asdf_parser_t *parser, const char *expected, char *out_buf) {
    char *r = read_next(parser);
    size_t len;

    if (!r) {
        ASDF_SET_STD_ERR(parser, ASDF_ERR_UNEXPECTED_EOF);
        return 1;
    }

    len = strlen(expected);

    if (strncmp(parser->read_buffer, expected, len) != 0) {
        ASDF_SET_STD_ERR(parser, ASDF_ERR_INVALID_ASDF_HEADER);
        return 1;
    }

    strcpy(out_buf, parser->read_buffer + len);
    out_buf[strcspn(out_buf, "\r\n")] = '\0';
    return 0;
}


// TODO: These need to be more flexible for acccepting different ASDF versions
static int parse_asdf_version(asdf_parser_t *parser, asdf_event_t *event) {
    if (parse_version_comment(parser, ASDF_VERSION_COMMENT, parser->asdf_version) != 0)
        return 1;

    event->type = ASDF_ASDF_VERSION_EVENT;
    event->payload.version = malloc(sizeof(asdf_version_t));
    event->payload.version->version = strdup(parser->asdf_version);
    parser->state = ASDF_PARSER_STATE_STANDARD_VERSION;
    return 0;
}


static int parse_standard_version(asdf_parser_t *parser, asdf_event_t *event) {
    if (parse_version_comment(parser, ASDF_STANDARD_COMMENT, parser->standard_version) != 0)
        return 1;

    event->type = ASDF_STANDARD_VERSION_EVENT;
    event->payload.version = malloc(sizeof(asdf_version_t));
    event->payload.version->version = strdup(parser->standard_version);
    parser->state = ASDF_PARSER_STATE_YAML;
    parser->yaml_start = ftello(parser->file);
    return 0;
}


static int parse_yaml(asdf_parser_t *parser, asdf_event_t *event) {
    struct fy_event *yaml = fy_parser_parse(parser->yaml_parser);

    if (fy_parser_get_stream_error(parser->yaml_parser)) {
        ASDF_SET_STD_ERR(parser, ASDF_ERR_YAML_PARSE_FAILED);
        return 1;
    }

    event->type = ASDF_YAML_EVENT;
    event->payload.yaml = yaml;

    if (!yaml || yaml->type == FYET_STREAM_END) {
        // We have reached the end of the YAML stream, maybe with an error
        // but per TODO above no error handling yet so just move to the next
        // state
        // TODO: What if there are more documents?  Currently not allowed by ASDF
        // but is being discussed for ASDF 2.0.0
        const struct fy_mark *mark = fy_event_end_mark(yaml);
        if (mark) {
            // libfyaml's input_pos starts at 0, whatever the actual offset
            // was in the file stream we handed it
            parser->yaml_end = parser->yaml_start + mark->input_pos;
        } else {
            parser->yaml_end = parser->yaml_start;
        }
        // Make sure to put the file back where we left off.
        // This is obviously broken for streams, where we may need to take a more
        // careful approach to this / more sophisticated input handling.
        fseek(parser->file, parser->yaml_end, SEEK_SET);
        parser->state = ASDF_PARSER_STATE_BLOCK;
    }

    return 0;
}


static int parse_block(asdf_parser_t *parser, asdf_event_t *event) {
    off_t pos = ftello(parser->file);

    if (!read_next(parser)) {
        // EOF; we are done now
        // TODO: This is wrong though, there might be a block index
        // check if we've reached it.
        parser->state = ASDF_PARSER_STATE_END;
        // Should immediately emit the end event, not a block event
        return asdf_parser_parse(parser, event);
    }

    if (strncmp(parser->read_buffer, ASDF_BLOCK_MAGIC, strlen(ASDF_BLOCK_MAGIC)) != 0) {
        parser->state = ASDF_PARSER_STATE_PADDING;
        return asdf_parser_parse(parser, event); // try padding parser
    }

    // It's a block?
    // TODO: ASDF 2.0.0 proposes adding a checksum to the block header
    // Here we will want to check that as well.
    // In fact we should probably ignore anything that starts with a block
    // magic but then contains garbage.  But we will need some heuristics
    // for what counts as "garbage"
    uint8_t *buf = parser->read_buffer;
    size_t n;
    // Go ahead and allocate storage for the block info
    asdf_block_info_t *block = calloc(1, sizeof(asdf_block_info_t));

    if (!block) {
        set_oom_error(parser);
        return 1;
    }

    event->type = ASDF_BLOCK_EVENT;
    event->payload.block = block;

    if (fseeko(parser->file, pos + 4, SEEK_SET)) {
        set_static_error(parser, "Failed to seek past block magic");
        return 1;
    }

    n = fread(buf, 1, 2, parser->file);
    if (n != 2) {
        set_static_error(parser, "Failed to read block header size");
        return 1;
    }

    asdf_block_header_t *header = &block->header;
    header->header_size = (buf[0] << 8) | buf[1];
    if (header->header_size < 48) {
        set_static_error(parser, "Invalid block header size");
        return 1;
    }

    n = fread(buf, 1, header->header_size, parser->file);
    if (n != header->header_size) {
        set_static_error(parser, "Failed to read full block header");
        return 1;
    }

    // Parse block fields
    uint32_t flags =
        (((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3]);
    strncpy(header->compression, (char *)buf + 4, 4);

    uint64_t allocated_size = 0, used_size = 0, data_size = 0;
    memcpy(&allocated_size, buf + 8, 8);
    memcpy(&used_size, buf + 16, 8);
    memcpy(&data_size, buf + 24, 8);

    header->flags = flags;
    header->allocated_size = be64toh(allocated_size);
    header->used_size = be64toh(used_size);
    header->data_size = be64toh(data_size);
    memcpy(header->checksum, buf + 32, 16);

    block->header_pos = pos;
    block->data_pos = ftello(parser->file);

    // Seek to end of the block (hopefully?)
    if (fseeko(parser->file, header->allocated_size, SEEK_CUR)) {
        set_static_error(parser, "Failed to seek past block data");
        return 1;
    }

    parser->found_blocks += 1;
    return 0;
}


static int parse_padding(asdf_parser_t *parser, asdf_event_t *event) {
    // TODO: For now, we skip padding and transition directly to block index
    parser->state = ASDF_PARSER_STATE_BLOCK_INDEX;
    return asdf_parser_parse(parser, event);
}


static int parse_block_index(asdf_parser_t *parser, asdf_event_t *event) {
    // TODO: For now, we skip block index and go to END
    parser->state = ASDF_PARSER_STATE_END;
    return asdf_parser_parse(parser, event);
}


int asdf_parser_parse(asdf_parser_t *parser, asdf_event_t *event) {
    assert(parser);
    assert(parser->file);
    assert(event);

    memset(event, 0, sizeof(asdf_event_t));

    switch (parser->state) {
    case ASDF_PARSER_STATE_ASDF_VERSION:
        return parse_asdf_version(parser, event);
    case ASDF_PARSER_STATE_STANDARD_VERSION:
        return parse_standard_version(parser, event);
    case ASDF_PARSER_STATE_YAML:
        return parse_yaml(parser, event);
    case ASDF_PARSER_STATE_BLOCK:
        return parse_block(parser, event);
    case ASDF_PARSER_STATE_PADDING:
        return parse_padding(parser, event);
    case ASDF_PARSER_STATE_BLOCK_INDEX:
        return parse_block_index(parser, event);
    case ASDF_PARSER_STATE_END:
        event->type = ASDF_END_EVENT;
        return 1;
    case ASDF_PARSER_STATE_ERROR:
        return 1;
    default:
        ASDF_SET_STD_ERR(parser, ASDF_ERR_UNKNOWN_STATE);
        return 1;
    }
}


/**
 * Default libfyaml parser configuration
 *
 * Later we will likely want some ASDF parser configuration, and this could include
 * the ability to pass through low-level YAML parser flags as well.  Could be useful.
 */
static const struct fy_parse_cfg default_fy_parse_cfg = {.flags = FYPCF_QUIET | FYPCF_COLLECT_DIAG};


int asdf_parser_init(asdf_parser_t *parser) {
    assert(parser);
    memset(parser, 0, sizeof(asdf_parser_t));

    parser->file = NULL;
    parser->state = ASDF_PARSER_STATE_INITIAL;
    parser->error = NULL;
    parser->error_type = ASDF_ERROR_NONE;
    parser->found_blocks = false;
    memset(parser->asdf_version, 0, sizeof(parser->asdf_version));
    memset(parser->standard_version, 0, sizeof(parser->standard_version));
    memset(parser->read_buffer, 0, sizeof(parser->read_buffer));

    if (!(parser->yaml_parser = fy_parser_create(&default_fy_parse_cfg))) {
        ASDF_SET_STD_ERR(parser, ASDF_ERR_YAML_PARSER_INIT_FAILED);
        return 1;
    }

    return 0;
}


int asdf_parser_set_input_file(asdf_parser_t *parser, FILE *file, const char *name) {
    assert(parser);
    assert(!parser->file);
    assert(file);

    if (fy_parser_set_input_fp(parser->yaml_parser, name, file) != 0) {
        ASDF_SET_STD_ERR(parser, ASDF_ERR_YAML_PARSER_INIT_FAILED);
        return 1;
    }

    parser->file = file;
    parser->found_blocks = false;
    parser->state = ASDF_PARSER_STATE_ASDF_VERSION;
    return 0;
}


bool asdf_parser_has_error(const asdf_parser_t *parser) {
    return parser->error != NULL;
}


const char *asdf_parser_get_error(const asdf_parser_t *parser) {
    return parser->error ? parser->error : "";
}


void asdf_parser_destroy(asdf_parser_t *parser) {
    if (!parser)
        return;

    fy_parser_destroy(parser->yaml_parser);

    if (parser->error_type == ASDF_ERROR_HEAP)
        free((void *)parser->error);
    // Leave no trace behind, leaving things clean for accidental
    // API calls on an already destroyed parser
    memset(parser, 0, sizeof(asdf_parser_t));
}
