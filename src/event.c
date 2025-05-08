/**
 * Functions for handling parser events
 */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <libfyaml.h>

#include "event.h"
#include "parse.h"


int asdf_event_iterate(asdf_parser_t *parser, asdf_event_t *event) {
    assert(parser);
    assert(event);

    // Clear previous event contents
    asdf_event_destroy(parser, event);
    memset(event, 0, sizeof(asdf_event_t));
    return asdf_parser_parse(parser, event);
}


const char* asdf_event_type_name(asdf_event_type_t event_type)
{
    if (event_type >= 0 && event_type < ASDF_EVENT_TYPE_COUNT)
        return asdf_event_type_names[event_type];
    else
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

        case ASDF_YAML_EVENT: {
            struct fy_event *yaml = event->payload.yaml;
            enum fy_event_type event_type = yaml->type;
            fprintf(file, "  Type: %s\n", fy_event_type_get_text(event_type));
            // Safe to call even on events that don't produce tags; just returns NULL
            struct fy_token *token = fy_event_get_tag_token(yaml);
            size_t token_len;
            const char *token_text;

            // Print the tag if it exists
            if (token) {
                token_text = fy_token_get_text(token, &token_len);
                if (token_text)
                    fprintf(file, "  Tag: %.*s\n", (int)token_len, token_text);
            }

            if (event_type == FYET_SCALAR && (token = yaml->scalar.value)) {
                token_text = fy_token_get_text(token, &token_len);
                if (token_text) {
                    fprintf(file, "  Value: %.*s\n", (int)token_len, token_text);
                }
            }
            break;
        }

        case ASDF_BLOCK_EVENT: {
            const asdf_block_info_t *block = event->payload.block;
            const asdf_block_header_t header = block->header;
            fprintf(file, "  Header position: %" PRId64 "\n", (int64_t)block->header_pos);
            fprintf(file, "  Data position: %" PRId64 "\n", (int64_t)block->data_pos);
            fprintf(file, "  Allocated size: %" PRIu64 "\n", header.allocated_size);
            fprintf(file, "  Used size: %" PRIu64 "\n", header.used_size);
            fprintf(file, "  Data size: %" PRIu64 "\n", header.data_size);

            if (header.compression[0] != '\0')
                fprintf(file, "  Compression: %.4s\n", header.compression);
            break;
        }
    }
}


void asdf_event_destroy(asdf_parser_t *parser, asdf_event_t *event) {
    assert(event);
    switch (event->type) {
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
        default:
            break;
    }
    memset(event, 0, sizeof(asdf_event_t));
}
