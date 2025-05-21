/**
 * Functions for handling parser events
 */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <libfyaml.h>

#include "block.h"
#include "event.h"
#include "parse.h"
#include "util.h"
#include "yaml.h"


asdf_event_type_t asdf_event_type(asdf_event_t *event) {
    if (!event)
        return ASDF_NONE_EVENT;

    return event->type;
}


const char *asdf_event_comment(const asdf_event_t *event) {
    if (!(event && event->type == ASDF_COMMENT_EVENT))
        return NULL;

    return event->payload.comment;
}


const asdf_tree_info_t *asdf_event_tree_info(const asdf_event_t *event) {
    if (!(event && (event->type == ASDF_TREE_START_EVENT || event->type == ASDF_TREE_END_EVENT)))
        return NULL;

    return event->payload.tree;
}


const asdf_block_info_t *asdf_event_block_info(const asdf_event_t *event) {
    if (!(event && event->type == ASDF_BLOCK_EVENT))
        return NULL;

    return event->payload.block;
}


int asdf_event_iterate(asdf_parser_t *parser, asdf_event_t *event) {
    assert(parser);
    assert(event);

    // Clear previous event contents
    asdf_event_destroy(parser, event);
    return asdf_parser_parse(parser, event);
}


const char *asdf_event_type_name(asdf_event_type_t event_type) {
    if (event_type >= 0 && event_type < ASDF_EVENT_TYPE_COUNT)
        return ASDF_EVENT_TYPE_NAMES[event_type];

    return "ASDF_UNKNOWN_EVENT";
}


void asdf_event_print(const asdf_event_t *event, FILE *file, bool verbose) {
    assert(file);
    assert(event);

    fprintf(file, "Event: %s\n", asdf_event_type_name(event->type));

    if (!verbose)
        return;

    switch (event->type) {
    case ASDF_ASDF_VERSION_EVENT:
        fprintf(file, "  ASDF Version: %s\n", event->payload.version->version);
        break;

    case ASDF_STANDARD_VERSION_EVENT:
        fprintf(file, "  Standard Version: %s\n", event->payload.version->version);
        break;


    case ASDF_COMMENT_EVENT:
        fprintf(file, "  Comment: %s\n", event->payload.comment);
        break;

    case ASDF_YAML_EVENT: {
        fprintf(file, "  Type: %s\n", asdf_yaml_event_type_text(event));

        size_t len = 0;
        const char *text = asdf_yaml_event_tag(event, &len);

        // Print the tag if it exists
        if (len)
            fprintf(file, "  Tag: %.*s\n", (int)len, text);

        text = asdf_yaml_event_scalar_value(event, &len);

        if (len)
            fprintf(file, "  Value: %.*s\n", (int)len, text);

        break;
    }

    case ASDF_TREE_START_EVENT:
        fprintf(file, "  Tree start position: %zu (0x%zx)\n", event->payload.tree->start,
                event->payload.tree->start);
        break;

    case ASDF_TREE_END_EVENT:
        fprintf(file, "  Tree end position: %zu (0x%zx)\n", event->payload.tree->end,
                event->payload.tree->end);
        break;

    case ASDF_BLOCK_EVENT: {
        const asdf_block_info_t *block = event->payload.block;
        const asdf_block_header_t header = block->header;
        fprintf(file, "  Header position: %" PRId64 " (0x%" PRIx64 ")\n",
                (int64_t)block->header_pos, (int64_t)block->header_pos);
        fprintf(file, "  Data position: %" PRId64 " (0x%" PRIx64 ")\n",
                (int64_t)block->data_pos, (int64_t)block->data_pos);
        fprintf(file, "  Allocated size: %" PRIu64 " (0x%" PRIx64 ")\n",
                header.allocated_size, header.allocated_size);
        fprintf(file, "  Used size: %" PRIu64 " (0x%" PRIx64 ")\n", header.used_size,
                header.used_size);
        fprintf(file, "  Data size: %" PRIu64 " (0x%" PRIx64 ")\n", header.data_size,
                header.data_size);

        if (header.compression[0] != '\0')
            fprintf(file, "  Compression: %.4s\n", header.compression);

        fprintf(file, "  Checksum: ");
        for (int idx = 0; idx < ASDF_BLOCK_CHECKSUM_FIELD_SIZE; idx++) {
            fprintf(file, "%02x", header.checksum[idx]);
        }
        fprintf(file, "\n");
        break;
    }

    default:
        break;
    }
}


void asdf_event_destroy(asdf_parser_t *parser, asdf_event_t *event) {
    assert(event);
    switch (event->type) {
    case ASDF_TREE_START_EVENT:
    case ASDF_TREE_END_EVENT:
        free(event->payload.tree);
        break;
    case ASDF_YAML_EVENT:
        fy_parser_event_free(parser->yaml_parser, event->payload.yaml);
        break;
    case ASDF_ASDF_VERSION_EVENT:
    case ASDF_STANDARD_VERSION_EVENT:
        if (event->payload.version)
            free(event->payload.version->version);

        free(event->payload.version);
        break;
    case ASDF_BLOCK_EVENT:
        free(event->payload.block);
        break;
    case ASDF_COMMENT_EVENT:
        free(event->payload.comment);
        break;
    default:
        break;
    }
    ZERO_MEMORY(event, sizeof(asdf_event_t));
}
