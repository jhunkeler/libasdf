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
#include "yaml.h"
#include "compat/endian.h"


typedef enum {
    ASDF_PARSE_ERROR = -1,
    ASDF_PARSE_CONTINUE,
    ASDF_PARSE_EVENT,
} parse_result_t;


static int check_stream(asdf_parser_t *parser) {
    asdf_stream_error_t error = asdf_stream_error(parser->stream);

    switch (error) {
    case ASDF_STREAM_OK:
        return 0;
    case ASDF_STREAM_ERR_OOM:
        asdf_parser_set_oom_error(parser);
        return 1;
    }
}


/**
 * Helper to consume bytes from the stream and immediately check the stream status
 * for any errors.
 */
#define CONSUME_AND_CHECK(parser, count) do { \
    asdf_stream_consume(parser->stream, count); \
    if (UNLIKELY(check_stream(parser) != 0)) { \
        return ASDF_PARSE_ERROR; \
    } \
} while (0);


/**
 * Like CONSUME_AND_CHECK but returns 1 in case of error instead of ASDF_PARSER_ERROR;
 */
#define CONSUME_AND_CHECK_INT(parser, count) do { \
    asdf_stream_consume(parser->stream, count); \
    if (UNLIKELY(check_stream(parser) != 0)) { \
        return 1; \
    } \
} while (0);


static int parse_version_comment(
    asdf_parser_t *parser, const char *expected, char *out_buf, size_t out_size) {
    size_t expected_len;
    size_t val_len;
    size_t line_len = 0;
    const uint8_t *r = asdf_stream_readline(parser->stream, &line_len);

    if (!r) {
        asdf_parser_set_common_error(parser, ASDF_ERR_UNEXPECTED_EOF);
        return ASDF_PARSE_ERROR;
    }

    expected_len = strlen(expected);

    if (line_len < expected_len || strncmp((const char *)r, expected, expected_len) != 0) {
        asdf_parser_set_common_error(parser, ASDF_ERR_INVALID_ASDF_HEADER);
        return 1;
    }

    size_t to_copy = line_len - expected_len - 1;

    if (to_copy >= out_size)
        to_copy = out_size - 1;

    memcpy(out_buf, r + expected_len, to_copy);
    out_buf[to_copy] = '\0';
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
    const uint8_t *r = NULL;
    size_t len = 0;

    // Peek the stream to see if we have a comment or blank line
    while ((r = asdf_stream_next(parser->stream, 1, &len))) {
        if (len == 0) {
            // EOF
            parser->state = ASDF_PARSER_STATE_END;
            return asdf_parser_parse(parser, event);
        }

        if (r[0] == '\r' || r[0] == '\n') {
            // Blank line, just continue
            CONSUME_AND_CHECK(parser, 1);
            continue;
        }

        if (r[0] == ASDF_COMMENT_CHAR) {
            // Found comment, attempt to read and consume the full line
            r = asdf_stream_readline(parser->stream, &len);

            if (!r) {
                asdf_parser_set_common_error(parser, ASDF_ERR_UNEXPECTED_EOF);
                return 1;
            }

            char *comment = strndup((const char *)r + 1, len - 1);
            comment[strcspn(comment, "\r\n")] = '\0';
            event->type = ASDF_COMMENT_EVENT;
            event->payload.comment = comment;
            // State remains searching for comment events
            return ASDF_PARSE_EVENT;
        }

        break;
    }

    parser->state = ASDF_PARSER_STATE_TREE_OR_BLOCK;
    return ASDF_PARSE_CONTINUE;
}


static parse_result_t emit_tree_start_event(asdf_parser_t *parser, asdf_event_t *event) {
    off_t offset = asdf_stream_tell(parser->stream);
    parser->tree.start = offset;
    parser->state = ASDF_PARSER_STATE_TREE;
    event->type = ASDF_TREE_START_EVENT;
    event->payload.tree = calloc(1, sizeof(asdf_tree_info_t));
    event->payload.tree->start = offset;
    return ASDF_PARSE_EVENT;
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
static parse_result_t parse_tree_or_block(asdf_parser_t *parser, asdf_event_t *event) {
    size_t len = 0;

    const uint8_t *r = asdf_stream_next(parser->stream, 0, &len);

    if (!r || !len) {
        parser->state = ASDF_PARSER_STATE_END;
        // Should immediately emit the end event, not a block event
        return ASDF_PARSE_CONTINUE;
    }

    // Likeliest case--we encounter a %YAML directive indicating beginning of the YAML tree
    if (LIKELY(is_yaml_directive((const char *)r, len))) {
        return emit_tree_start_event(parser, event);
    }

    // Or may encounter an ASDF block magic esp. in case of an exploded ASDF file
    if (is_block_magic((const char *)r, len)) {
        parser->state = ASDF_PARSER_STATE_BLOCK;
        return ASDF_PARSE_CONTINUE;
    }

    // Not as happy case: might be some padding or other garbage.  Here we might want
    // to issue some consistency warnings, depending, though technically there can be padding.
    // Scan for the beginning of the YAML tree or a block.
    const asdf_parse_token_id_t tokens[] = {
        ASDF_YAML_DIRECTIVE_TOK, ASDF_BLOCK_MAGIC_TOK, ASDF_LAST_TOK};
    asdf_parse_token_id_t match_token = ASDF_LAST_TOK;
    size_t match_offset = 0;

    while (0 == asdf_parser_scan_tokens(parser, tokens, &match_offset, &match_token)) {
        // No expected token found; may be the end of the file or just unparseable garbage
        // TODO: May also encounter a block index, though if we didn't find a valid block
        // up to this point it is either empty or invalid
        switch (match_token) {
        case ASDF_YAML_DIRECTIVE_TOK:
            // Make sure it is actually a valid YAML directive
            // TODO: Technically this should be either at the start of the file or begin
            // with a newline; should be more careful about that.
            if (LIKELY(is_yaml_directive((const char *)r, len))) {
                return emit_tree_start_event(parser, event);
            }
        case ASDF_BLOCK_MAGIC_TOK:
            parser->state = ASDF_PARSER_STATE_BLOCK;
            return ASDF_PARSE_CONTINUE;
        default:
            /* Shouldn't happen */
            asdf_parser_set_common_error(parser, ASDF_ERR_UNKNOWN_STATE);
            return 1;
        }
    }

    parser->state = ASDF_PARSER_STATE_END;
    return ASDF_PARSE_CONTINUE;
}


/**
 * Helper to clean up and set the ASDF_TREE_END_EVENT in case of finding the end of the tree
 * (or the file)
 */
parse_result_t emit_tree_end_event(asdf_parser_t *parser, asdf_event_t *event) {
    event->type = ASDF_TREE_END_EVENT;
    event->payload.tree = calloc(1, sizeof(asdf_tree_info_t));

    if (!event->payload.tree) {
        asdf_parser_set_oom_error(parser);
        return ASDF_PARSE_ERROR;
    }

    event->payload.tree->start = parser->tree.start;
    event->payload.tree->end = parser->tree.end;
    event->payload.tree->buf = (const char *)parser->tree.buf;

    return ASDF_PARSE_EVENT;
}


/**
 * "Fast" tree parsing
 *
 * Scans for either a YAML document end or an ASDF block magic and sets the offset
 * of the end of the tree (or EOF)
 */
static int parse_tree_fast(asdf_parser_t *parser) {
    const char *r = NULL;
    bool tree_end_found = false;
    off_t tree_end = 0;
    size_t len = 0;

    const asdf_parse_token_id_t tokens[] = {
        ASDF_YAML_DOCUMENT_END_TOK, ASDF_BLOCK_MAGIC_TOK, ASDF_LAST_TOK};
    asdf_parse_token_id_t match_token = ASDF_LAST_TOK;
    size_t match_offset = 0;

    while (0 == asdf_parser_scan_tokens(parser, tokens, &match_offset, &match_token)) {
        // We got a match for either a document end marker or a block magic
        switch (match_token) {
            case ASDF_YAML_DOCUMENT_END_TOK: {
                size_t len = 0;
                const uint8_t *r = asdf_stream_next(parser->stream, 0, &len);
                if (LIKELY(is_yaml_document_end_marker((const char *)r, len))) {
                    // Read and consume the full line then set the tree end
                    // First chomp the leading newline of the document end marker, then consume
                    // the line of the marker itself.
                    CONSUME_AND_CHECK_INT(parser, 1);
                    asdf_stream_readline(parser->stream, &len);
                    tree_end = asdf_stream_tell(parser->stream);
                    tree_end_found = true;
                    break;
                } else {
                    // Looked like a document end marker but unexpected trailing bytes
                    // instead of a newline; keep parsing
                    continue;
                }
                break;
            }
            case ASDF_BLOCK_MAGIC_TOK:
                // Unexpectedly found a block magic before the document end marker
                tree_end = asdf_stream_tell(parser->stream);
                tree_end_found = true;
                break;
            default:
                /* should happen */
                asdf_parser_set_common_error(parser, ASDF_ERR_UNKNOWN_STATE);
                return 1;
        }

        if (tree_end_found)
            break;
    }

    parser->tree.end = tree_end;
    parser->tree.found = true;
    return 0;
}


/**
 * Before transitioning to generating YAML events we need to set up libfyaml
 *
 * There are two possible cases:
 *
 * * We have already buffered the entire YAML tree in memory, so set up libfyaml
 *   to use that memory buffer.
 * * Otherwise set up libfyaml to use the stream.  This is only valid if the stream
 *   is seekable, logic which is enforced elsewhere.
 */
static int initialize_yaml_parser(asdf_parser_t *parser) {
    assert(parser->stream->is_seekable || (parser->tree.found && parser->tree.buf));
    bool buffer_tree = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_BUFFER_TREE);
    int ret = 0;

    if (buffer_tree) {
        ret = fy_parser_set_string(parser->yaml_parser, (const char *)parser->tree.buf,
                                   parser->tree.size);
    } else {
        ret = parser->stream->fy_parser_set_input(parser->stream, parser->yaml_parser);
    }

    if (0 != ret)
        asdf_parser_set_common_error(parser, ASDF_ERR_YAML_PARSER_INIT_FAILED);

    return ret;
}


static parse_result_t parse_tree(asdf_parser_t *parser, asdf_event_t *event) {
    bool buffer_tree = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_BUFFER_TREE);
    bool emit_yaml_events = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_EMIT_YAML_EVENTS);

    // There is an inherent limitation that if we want to generate YAML events during
    // ASDF parsing, but the input stream is not seekable, we should buffer the entire
    // YAML tree in memory.
    // This is because if we turn parsing over to libfyaml it read the stream past
    // the end of the YAML portion and make it difficult to recover previous portions
    // of the file.
    // There are in principle other more complicated ways to work around it, such as
    // adding a method to push data back onto the buffered stream.  But the
    // ASDF_PARSER_OPT_EMIT_YAML_EVENTS option is primarily just for debugging purposes
    // so for now we don't do anything too crazy.
    buffer_tree |= (emit_yaml_events && !parser->stream->is_seekable);

    if (buffer_tree) {
        // Initialize the tree buffer and set up the stream to capture to it
        parser->tree.buf = malloc(ASDF_PARSER_READ_BUFFER_INIT_SIZE);
        if (!parser->tree.buf) {
            asdf_parser_set_oom_error(parser);
            return ASDF_PARSE_ERROR;
        }
        parser->tree.size = 0;
        asdf_stream_set_capture(parser->stream, &parser->tree.buf, &parser->tree.size,
                                ASDF_PARSER_READ_BUFFER_INIT_SIZE);
    }

    if (buffer_tree || !emit_yaml_events) {
        // Go ahead and do "fast" parsing of the tree since we want to buffer it anyways.
        int res = parse_tree_fast(parser);
        // Halt stream capture if it was running
        asdf_stream_set_capture(parser->stream, NULL, NULL, 0);

        if (0 != res)
            return ASDF_PARSE_ERROR;
    }

    // Continue to generating YAML events
    if (emit_yaml_events) {
        if (0 != initialize_yaml_parser(parser))
            return ASDF_PARSE_ERROR;

        parser->state = ASDF_PARSER_STATE_YAML;
        // Even if we have buffered the full tree mark it as not found until YAML parsing completes
        parser->tree.found = false;
        return ASDF_PARSE_CONTINUE;
    }

    // Otherwise emit tree end event and move to padding/block
    parser->state = ASDF_PARSER_STATE_PADDING;
    return emit_tree_end_event(parser, event);
}


static parse_result_t parse_yaml(asdf_parser_t *parser, asdf_event_t *event) {
    bool emit_yaml_events = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_EMIT_YAML_EVENTS);
    bool buffer_tree = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_BUFFER_TREE);

    // Only allowed if the stream is seekable or the YAML is buffered.  Other logic
    // elsewhere should enforce this.
    assert(emit_yaml_events && (buffer_tree || parser->stream->is_seekable));

    if (parser->tree.found) {
        // Already found the end of the YAML tree; just emit the TREE_END event
        parser->state = ASDF_PARSER_STATE_PADDING;
        return emit_tree_end_event(parser, event);
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
            // libfyaml doesn't care about FILE* and considers the the input it's handed
            // to be at position 0 when it starts parsing, so to get the actual tree end
            // relative to the start of the file we add it to the tree start position
            parser->tree.end = parser->tree.start + mark->input_pos;
        } else {
            parser->tree.end = parser->tree.start;
        }

        size_t tree_size = parser->tree.end - parser->tree.start;
        parser->tree.found = true;
        // Rewind the stream back to the end of the tree, as libfyaml will likely have read
        // past it for its own internal buffering.
        asdf_stream_seek(parser->stream, parser->tree.end, SEEK_SET);
    }

    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_block(asdf_parser_t *parser, asdf_event_t *event) {
    size_t len = 0;
    const uint8_t *buf = asdf_stream_next(parser->stream, ASDF_BLOCK_MAGIC_SIZE, &len);
    size_t header_pos = 0;

    // Happy path, we are already pointing to the start of a block
    // Otherwise scan for the first block magic we find, if any
    if (is_block_magic((const char *)buf, len)) {
        header_pos = asdf_stream_tell(parser->stream);
    } else {
        const asdf_parse_token_id_t tokens[] = {ASDF_BLOCK_MAGIC_TOK, ASDF_LAST_TOK};
        asdf_parse_token_id_t match_token = ASDF_LAST_TOK;
        size_t match_offset = 0;

        if (0 != asdf_parser_scan_tokens(parser, tokens, &match_offset, &match_token)) {
            // No expected token found; may be the end of the file or just unparseable garbage
            // TODO: May also encounter a block index, though if we didn't find a valid block
            // up to this point it is either empty or invalid
            parser->state = ASDF_PARSER_STATE_END;
            // Should immediately emit the end event, not a block event
            return ASDF_PARSE_CONTINUE;
        }

        header_pos = match_offset;
    }

    // It's a block?
    // TODO: ASDF 2.0.0 proposes adding a checksum to the block header
    // Here we will want to check that as well.
    // In fact we should probably ignore anything that starts with a block
    // magic but then contains garbage.  But we will need some heuristics
    // for what counts as "garbage"
    // Go ahead and allocate storage for the block info
    asdf_block_info_t *block = calloc(1, sizeof(asdf_block_info_t));

    if (!block) {
        asdf_parser_set_oom_error(parser);
        return ASDF_PARSE_ERROR;
    }

    CONSUME_AND_CHECK(parser, ASDF_BLOCK_MAGIC_SIZE);

    buf = asdf_stream_next(parser->stream, FIELD_SIZEOF(asdf_block_header_t, header_size), &len);

    if (!buf) {
        asdf_parser_set_static_error(parser, "Failed to seek past block magic");
        return ASDF_PARSE_ERROR;
    }

    if (len < 2) {
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

    CONSUME_AND_CHECK(parser, FIELD_SIZEOF(asdf_block_header_t, header_size));
    buf = asdf_stream_next(parser->stream, header->header_size, &len);

    if (len < header->header_size) {
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

    block->header_pos = header_pos;

    CONSUME_AND_CHECK(parser, header->header_size);

    block->data_pos = asdf_stream_tell(parser->stream);

    // Seek to end of the block (hopefully?)
    if (parser->stream->is_seekable) {
        // TODO: When reading the file from a stream we will want the option to return a pointer
        // to the start of the block data, possibly with the option to copy it to a buffer.
        // For now it will suffice to skip past it.
        if (asdf_stream_seek(parser->stream, header->allocated_size, SEEK_CUR)) {
            asdf_parser_set_static_error(parser, "Failed to seek past block data");
            return ASDF_PARSE_ERROR;
        }
    }
    // TODO: Fix this: asdf_stream_seek doesn't mark the current buffer as consumed.
    // In fact seek doesn't really make sense with the next()/consume() semantics so it should
    // probably be deleted.
    //CONSUME_AND_CHECK(parser, header->allocated_size);
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
    assert(parser->stream);
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
        case ASDF_PARSER_STATE_TREE_OR_BLOCK:
            res = parse_tree_or_block(parser, event);
            break;
        case ASDF_PARSER_STATE_TREE:
            res = parse_tree(parser, event);
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
    parser->state = ASDF_PARSER_STATE_INITIAL;
    parser->error = NULL;
    parser->error_type = ASDF_ERROR_NONE;
    parser->found_blocks = false;
    parser->done = false;
    ZERO_MEMORY(parser->asdf_version, sizeof(parser->asdf_version));
    ZERO_MEMORY(parser->standard_version, sizeof(parser->standard_version));

    if (!(parser->yaml_parser = fy_parser_create(&DEFAULT_FY_PARSE_CFG))) {
        asdf_parser_set_common_error(parser, ASDF_ERR_YAML_PARSER_INIT_FAILED);
        return 1;
    }

    return 0;
}


int asdf_parser_set_input_file(asdf_parser_t *parser, const char *filename) {
    assert(parser);
    parser->stream = asdf_stream_from_file(filename);

    if (!parser->stream) {
        // TODO: Better error handling for file opening errors
        // For now just use this generic error
        asdf_parser_set_common_error(parser, ASDF_ERR_STREAM_INIT_FAILED);
        return 1;
    }
    parser->state = ASDF_PARSER_STATE_ASDF_VERSION;
    return 0;
}


int asdf_parser_set_input_fp(asdf_parser_t *parser, FILE *file, const char *filename) {
    assert(parser);
    parser->stream = asdf_stream_from_fp(file, filename);

    if (!parser->stream) {
        // TODO: Better error handling for file opening errors
        // For now just use this generic error
        asdf_parser_set_common_error(parser, ASDF_ERR_STREAM_INIT_FAILED);
        return 1;
    }
    parser->state = ASDF_PARSER_STATE_ASDF_VERSION;
    return 0;
}


int asdf_parser_set_input_mem(asdf_parser_t *parser, const void *buf, size_t size) {
    assert(parser);
    parser->stream = asdf_stream_from_memory(buf, size);

    if (!parser->stream) {
        // TODO: Better error handling for file opening errors
        // For now just use this generic error
        asdf_parser_set_common_error(parser, ASDF_ERR_STREAM_INIT_FAILED);
        return 1;
    }
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

    if (parser->stream)
        parser->stream->close(parser->stream);

    free(parser->tree.buf);

    // Leave no trace behind, leaving things clean for accidental
    // API calls on an already destroyed parser
    ZERO_MEMORY(parser, sizeof(asdf_parser_t));
}
