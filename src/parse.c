#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "block.h"
#include "event.h"
#include "parse.h"
#include "parse_util.h"
#include "util.h"
#include "compat/endian.h"


typedef enum {
    ASDF_PARSE_ERROR = -1,
    ASDF_PARSE_CONTINUE,
    ASDF_PARSE_EVENT,
} parse_result_t;


// TODO: Separate out I/O logic and make it more robust for handling pathological cases
static char *read_next_line(asdf_parser_t *parser) {
    ssize_t size = getline((char **)&parser->read_buffer, &parser->read_buffer_size, parser->file);

    if (size < 0)
        return NULL;

    return (char *)parser->read_buffer;
}


static char *read_next_chunk(asdf_parser_t *parser) {
    // TODO: Don't use fgets for this, just fread read_buffer_size
    return fgets((char *)parser->read_buffer, parser->read_buffer_size, parser->file);
}


static int parse_version_comment(
    asdf_parser_t *parser, const char *expected, char *out_buf, size_t out_size) {
    char *r = read_next_line(parser);
    size_t len;

    if (!r) {
        asdf_parser_set_common_error(parser, ASDF_ERR_UNEXPECTED_EOF);
        return ASDF_PARSE_ERROR;
    }

    len = strlen(expected);

    if (strncmp((char *)parser->read_buffer, expected, len) != 0) {
        asdf_parser_set_common_error(parser, ASDF_ERR_INVALID_ASDF_HEADER);
        return 1;
    }

    strncpy(out_buf, (char *)parser->read_buffer + len, out_size);
    out_buf[strcspn(out_buf, "\r\n")] = '\0';
    return 0;
}


// TODO: These need to be more flexible for acccepting different ASDF versions
// TODO: Depending on strictness level we can resume even if this fails
static parse_result_t parse_asdf_version(asdf_parser_t *parser, asdf_event_t *event) {
    if (parse_version_comment(
            parser, asdf_version_comment, parser->asdf_version, ASDF_ASDF_VERSION_BUFFER_SIZE) != 0)
        return ASDF_PARSE_ERROR;

    event->type = ASDF_ASDF_VERSION_EVENT;
    event->payload.version = malloc(sizeof(asdf_version_t));
    event->payload.version->version = strdup(parser->asdf_version);
    parser->state = ASDF_PARSER_STATE_STANDARD_VERSION;
    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_standard_version(asdf_parser_t *parser, asdf_event_t *event) {
    if (parse_version_comment(parser,
            asdf_standard_comment,
            parser->standard_version,
            ASDF_STANDARD_VERSION_BUFFER_SIZE) != 0)
        return ASDF_PARSE_ERROR;

    event->type = ASDF_STANDARD_VERSION_EVENT;
    event->payload.version = malloc(sizeof(asdf_version_t));
    event->payload.version->version = strdup(parser->standard_version);
    parser->state = ASDF_PARSER_STATE_COMMENT;
    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_comment(asdf_parser_t *parser, asdf_event_t *event) {
    char *r = NULL;

    while ((r = read_next_line(parser))) {
        if (r[0] == '\0')  // blank line; keep looking
            continue;

        if (r[0] == ASDF_COMMENT_CHAR) { // found comment
            event->type = ASDF_COMMENT_EVENT;
            event->payload.comment = strdup(r + 1);
            event->payload.comment[strcspn(event->payload.comment, "\r\n")] = '\0';
            // State remains searching for comment events
            return ASDF_PARSE_EVENT;
        } else {
            // Crude rewind for now, will need refinement for unseekable stream handling
            fseek(parser->file, -strlen(r), SEEK_CUR);
            parser->state = ASDF_PARSER_STATE_YAML_OR_BLOCK;
            return ASDF_PARSE_CONTINUE;
        }
    }

    parser->state = ASDF_PARSER_STATE_END;
    return ASDF_PARSE_CONTINUE;
}


static int append_to_tree_buffer(asdf_parser_t *parser, const char *buf, size_t len) {
    if (!parser->tree.buf) {
        parser->tree.buf = malloc(ASDF_PARSER_READ_BUFFER_INIT_SIZE + len + 1);
        if (!parser->tree.buf) {
            asdf_parser_set_oom_error(parser);
            return 1;
        }
        parser->tree.size = 0;
        parser->tree.cap = ASDF_PARSER_READ_BUFFER_INIT_SIZE + len + 1;
    } else if (parser->tree.size + len + 1 > parser->tree.cap) {
        // Twice the needed capacity for the new extension to avoid too-frequent reallocs
        size_t new_cap = (parser->tree.cap + len + 1) * 2;
        char *new_buf = realloc(parser->tree.buf, new_cap);

        if (!new_buf) {
            asdf_parser_set_oom_error(parser);
            return 1;
        }

        parser->tree.buf = new_buf;
        parser->tree.cap = new_cap;
    }

    memcpy(parser->tree.buf + parser->tree.size, buf, len);
    parser->tree.size += len;
    parser->tree.buf[parser->tree.size] = '\0';
    return 0;
}


static ssize_t parse_yaml_input_callback(void *user_data, void *buf, size_t count) {
    asdf_parser_t *parser = (asdf_parser_t *)user_data;
    ssize_t len = fread(buf, 1, count, parser->file);

    if (len <= 0)
        return len;

    append_to_tree_buffer(parser, buf, len);
    return len;
}


static int transition_to_yaml(asdf_parser_t *parser, asdf_event_t *event, size_t offset) {
    // Happy case, emit tree start event
    parser->state = ASDF_PARSER_STATE_YAML;
    parser->tree.start = offset;
    event->type = ASDF_TREE_START_EVENT;
    event->payload.tree = calloc(1, sizeof(asdf_tree_info_t));
    event->payload.tree->start = offset;

    // Set up libfyaml parser:
    // - In the special case of the flag combination below, we read into our own internal buffer
    //   for the YAML tree, while also feeding into libfyaml by using a custom input callback
    // - Otherwise in the normal case we let libfyaml take over the file pointer for a bit
    if (asdf_parser_has_opt(parser, ASDF_PARSER_OPT_BUFFER_TREE | ASDF_PARSER_OPT_EMIT_YAML_EVENTS))
        fy_parser_set_input_callback(parser->yaml_parser, parser, parse_yaml_input_callback);
    else if (fy_parser_set_input_fp(parser->yaml_parser, parser->filename, parser->file) != 0) {
        asdf_parser_set_common_error(parser, ASDF_ERR_YAML_PARSER_INIT_FAILED);
        return 1;
    }

    return 0;
}


/**
 * This phase of parsing expects *either* an explicit start to the YAML tree indicated
 * by the "%YAML <yaml-version>" directive or the start of a block indicated by the block
 * magic token.
 *
 * This is the the happy case for a well-formed ASDF document.
 *
 * If the happy case fails we then switch to a slower parse mode
 */
static parse_result_t parse_yaml_or_block(asdf_parser_t *parser, asdf_event_t *event) {
    off_t offset = ftello(parser->file);
    char *r = read_next_chunk(parser);

    // Easiest case--presumed start of an ASDF block
    if (is_block_magic(r, parser->read_buffer_size)) {
        fseek(parser->file, -strlen(r), SEEK_CUR);
        parser->state = ASDF_PARSER_STATE_BLOCK;
        return asdf_parser_parse(parser, event);
    }

    size_t len = strlen(r);

    if (is_yaml_directive(r, len)) {
        // Rewind to start of YAML
        fseek(parser->file, -strlen(r), SEEK_CUR);
        if (0 == transition_to_yaml(parser, event, offset))
            return ASDF_PARSE_EVENT;

        return ASDF_PARSE_ERROR;
    }

    // Pathological case--some kind of padding or malformed YAML
    // TODO: Various cases like this can be handled for sure, but come back to that.
    // For now just throw error.
    asdf_parser_set_common_error(parser, ASDF_ERR_YAML_PARSE_FAILED);
    return ASDF_PARSE_ERROR;
}


static inline bool is_yaml_document_end_marker(const char *buf, size_t len) {
    return (
        len >= 4
        && strncmp(buf, "...", 3) == 0
        && (buf[3] == '\n' || (len >= 5 && buf[3] == '\r' && buf[4] == '\n'))
    );
}


static parse_result_t parse_yaml_fast(asdf_parser_t *parser, asdf_event_t *event) {
    char *r = NULL;
    size_t len = 0;
    bool explicit_document_end = false;
    bool found_block = false;
    off_t offset = ftello(parser->file);

    while (r = read_next_chunk(parser)) {
        len = strlen(r);
        explicit_document_end = is_yaml_document_end_marker(r, len);
        found_block = is_block_magic(r, len);

        if (!found_block && asdf_parser_has_opt(parser, ASDF_PARSER_OPT_BUFFER_TREE)) {
            if (0 != append_to_tree_buffer(parser, r, len))
                return ASDF_PARSE_ERROR;
        }

        if (!(explicit_document_end || found_block)) {
            offset = ftello(parser->file);
            continue;
        }

        // Either found the document end marker (as expected) or encountered a block magic
        // TODO: (this would be unexpected, but possible in a malformed file; should warn about
        // this or error in strict mode)
        if (explicit_document_end) {
            parser->tree.end = offset + len;
        } else {
            parser->tree.end = offset;
            // Cludgy rewind for block handler
            fseeko(parser->file, offset, SEEK_SET);
        }
        parser->tree.done = true;
        break;
    }

    if (r) {
        // Still expecting more data, particularly blocks or possibly padding
        parser->state = found_block ? ASDF_PARSER_STATE_BLOCK : ASDF_PARSER_STATE_PADDING;
    } else {
        parser->state = ASDF_PARSER_STATE_END;
    }

    event->type = ASDF_TREE_END_EVENT;
    event->payload.tree = calloc(1, sizeof(asdf_tree_info_t));
    event->payload.tree->start = parser->tree.start;
    event->payload.tree->end = parser->tree.end;
    event->payload.tree->buf = parser->tree.buf;
    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_yaml(asdf_parser_t *parser, asdf_event_t *event) {
    // First the '*fast*' case--don't attempt any YAML parsing, just scan
    // until we hit a document end event or start of a block magic.
    if (!asdf_parser_has_opt(parser, ASDF_PARSER_OPT_EMIT_YAML_EVENTS))
        return parse_yaml_fast(parser, event);

    if (parser->tree.done) {
        // Already found the end of the YAML tree; just emit the TREE_END event
        // Make sure to put the file back where we left off.
        // This is obviously broken for streams, where we may need to take a more
        // careful approach to this / more sophisticated input handling.
        event->type = ASDF_TREE_END_EVENT;
        event->payload.tree = calloc(1, sizeof(asdf_tree_info_t));
        event->payload.tree->start = parser->tree.start;
        event->payload.tree->end = parser->tree.end;
        event->payload.tree->buf = parser->tree.buf;
        parser->state = ASDF_PARSER_STATE_BLOCK;
        fseek(parser->file, parser->tree.end, SEEK_SET);
        return ASDF_PARSE_EVENT;
    }

    struct fy_event *yaml = fy_parser_parse(parser->yaml_parser);

    if (fy_parser_get_stream_error(parser->yaml_parser)) {
        asdf_parser_set_common_error(parser, ASDF_ERR_YAML_PARSE_FAILED);
        return ASDF_PARSE_ERROR;
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
            parser->tree.end = parser->tree.start + mark->input_pos;
        } else {
            parser->tree.end = parser->tree.start;
        }

        // Cap off the tree buffer; normally this is done naturally by
        // append_to_tree_buffer but it might get left with some dangling bytes
        // from the libfyaml parser if we generated YAML events immediately
        if (parser->tree.buf) {
            size_t tree_buf_end = parser->tree.end - parser->tree.start;
            assert(tree_buf_end < parser->tree.size);
            parser->tree.buf[tree_buf_end] = '\0';
        }
        parser->tree.done = true;
    }

    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_block(asdf_parser_t *parser, asdf_event_t *event) {
    off_t pos = ftello(parser->file);

    if (!read_next_chunk(parser)) {
        // EOF; we are done now
        // TODO: This is wrong though, there might be a block index
        // check if we've reached it.
        parser->state = ASDF_PARSER_STATE_END;
        // Should immediately emit the end event, not a block event
        return ASDF_PARSE_CONTINUE;
    }

    if (!is_block_magic((char *)parser->read_buffer, parser->read_buffer_size)) {
        parser->state = ASDF_PARSER_STATE_PADDING;
        return ASDF_PARSE_CONTINUE;
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
        asdf_parser_set_oom_error(parser);
        return ASDF_PARSE_ERROR;
    }

    if (fseeko(parser->file, pos + 4, SEEK_SET)) {
        asdf_parser_set_static_error(parser, "Failed to seek past block magic");
        return ASDF_PARSE_ERROR;
    }

    n = fread(buf, 1, FIELD_SIZEOF(asdf_block_header_t, header_size), parser->file);
    if (n != 2) {
        asdf_parser_set_static_error(parser, "Failed to read block header size");
        return ASDF_PARSE_ERROR;
    }

    asdf_block_header_t *header = &block->header;
    // NOLINTNEXTLINE(readability-magic-numbers)
    header->header_size = (buf[0] << 8) | buf[1];
    if (header->header_size < ASDF_BLOCK_HEADER_SIZE) {
        asdf_parser_set_static_error(parser, "Invalid block header size");
        return ASDF_PARSE_ERROR;
    }

    n = fread(buf, 1, header->header_size, parser->file);
    if (n != header->header_size) {
        asdf_parser_set_static_error(parser, "Failed to read full block header");
        return ASDF_PARSE_ERROR;
    }

    // Parse block fields
    uint32_t flags =
        // NOLINTNEXTLINE(readability-magic-numbers)
        (((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3]);
    strncpy(header->compression,
        (char *)buf + ASDF_BLOCK_COMPRESSION_OFFSET,
        sizeof(header->compression));

    uint64_t allocated_size = 0;
    uint64_t used_size = 0;
    uint64_t data_size = 0;
    memcpy(&allocated_size, buf + ASDF_BLOCK_ALLOCATED_SIZE_OFFSET, sizeof(allocated_size));
    memcpy(&used_size, buf + ASDF_BLOCK_USED_SIZE_OFFSET, sizeof(used_size));
    memcpy(&data_size, buf + ASDF_BLOCK_DATA_SIZE_OFFSET, sizeof(data_size));

    header->flags = flags;
    header->allocated_size = be64toh(allocated_size);
    header->used_size = be64toh(used_size);
    header->data_size = be64toh(data_size);
    memcpy(header->checksum, buf + ASDF_BLOCK_CHECKSUM_OFFSET, sizeof(header->checksum));

    block->header_pos = pos;
    block->data_pos = ftello(parser->file);

    // Seek to end of the block (hopefully?)
    if (fseeko(parser->file, header->allocated_size, SEEK_CUR)) {
        asdf_parser_set_static_error(parser, "Failed to seek past block data");
        return ASDF_PARSE_ERROR;
    }

    parser->found_blocks += 1;
    event->type = ASDF_BLOCK_EVENT;
    event->payload.block = block;
    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_padding(asdf_parser_t *parser, asdf_event_t *event) {
    // TODO: For now, we skip padding and transition directly to block parsing
    parser->state = ASDF_PARSER_STATE_BLOCK;
    return ASDF_PARSE_CONTINUE;
}


static parse_result_t parse_block_index(asdf_parser_t *parser, asdf_event_t *event) {
    // TODO: For now, we skip block index and go to END
    parser->state = ASDF_PARSER_STATE_END;
    return ASDF_PARSE_CONTINUE;
}


int asdf_parser_parse(asdf_parser_t *parser, asdf_event_t *event) {
    assert(parser);
    assert(parser->file);
    assert(event);

    ZERO_MEMORY(event, sizeof(asdf_event_t));

    parse_result_t res = ASDF_PARSE_CONTINUE;

    while (ASDF_PARSE_CONTINUE == res) {
        switch (parser->state) {
        case ASDF_PARSER_STATE_ASDF_VERSION:
            res = parse_asdf_version(parser, event);
            break;
        case ASDF_PARSER_STATE_STANDARD_VERSION:
            res = parse_standard_version(parser, event);
            break;
        case ASDF_PARSER_STATE_COMMENT:
            res = parse_comment(parser, event);
            break;
        case ASDF_PARSER_STATE_YAML_OR_BLOCK:
            res = parse_yaml_or_block(parser, event);
            break;
        case ASDF_PARSER_STATE_YAML:
            res = parse_yaml(parser, event);
            break;
        case ASDF_PARSER_STATE_BLOCK:
            res = parse_block(parser, event);
            break;
        case ASDF_PARSER_STATE_PADDING:
            res = parse_padding(parser, event);
            break;
        case ASDF_PARSER_STATE_BLOCK_INDEX:
            res = parse_block_index(parser, event);
            break;
        case ASDF_PARSER_STATE_END:
            if (!parser->done) {
                // Produce a valid ASDF_END_EVENT and return 0 (success)
                // but on subsequent calls produce no more valid events and return 1 (error)
                parser->done = true;
                event->type = ASDF_END_EVENT;
                return 0;
            }
            event->type = ASDF_NONE_EVENT;
            return 1;
        case ASDF_PARSER_STATE_ERROR:
            return 1;
        default:
            asdf_parser_set_common_error(parser, ASDF_ERR_UNKNOWN_STATE);
            return 1;
        }
    }

    return res != ASDF_PARSE_EVENT;
}


/**
 * Default libasdf parser configuration
 */
static const asdf_parser_cfg_t DEFAULT_ASDF_PARSER_CFG = {.flags = 0};

/**
 * Default libfyaml parser configuration
 *
 * Later we will likely want some ASDF parser configuration, and this could include
 * the ability to pass through low-level YAML parser flags as well.  Could be useful.
 */
static const struct fy_parse_cfg DEFAULT_FY_PARSE_CFG = {.flags = FYPCF_QUIET | FYPCF_COLLECT_DIAG};


int asdf_parser_init(asdf_parser_t *parser, asdf_parser_cfg_t *config) {
    assert(parser);
    ZERO_MEMORY(parser, sizeof(asdf_parser_t));

    parser->config = config ? config : &DEFAULT_ASDF_PARSER_CFG;
    parser->file = NULL;
    parser->state = ASDF_PARSER_STATE_INITIAL;
    parser->error = NULL;
    parser->error_type = ASDF_ERROR_NONE;
    parser->found_blocks = false;
    parser->read_buffer = malloc(ASDF_PARSER_READ_BUFFER_INIT_SIZE);
    parser->read_buffer_size = ASDF_PARSER_READ_BUFFER_INIT_SIZE;
    parser->done = false;
    ZERO_MEMORY(parser->asdf_version, sizeof(parser->asdf_version));
    ZERO_MEMORY(parser->standard_version, sizeof(parser->standard_version));
    ZERO_MEMORY(parser->read_buffer, sizeof(parser->read_buffer));

    if (!(parser->yaml_parser = fy_parser_create(&DEFAULT_FY_PARSE_CFG))) {
        asdf_parser_set_common_error(parser, ASDF_ERR_YAML_PARSER_INIT_FAILED);
        return 1;
    }

    return 0;
}


int asdf_parser_set_input_file(asdf_parser_t *parser, FILE *file, const char *name) {
    assert(parser);
    assert(!parser->file);
    assert(file);
    parser->filename = name;
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

    free(parser->read_buffer);
    // Leave no trace behind, leaving things clean for accidental
    // API calls on an already destroyed parser
    ZERO_MEMORY(parser, sizeof(asdf_parser_t));
}
