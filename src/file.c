#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libfyaml.h>

#include "context.h"
#include "error.h"
#include "event.h"
#include "file.h"
#include "log.h"
#include "parse.h"
#include "util.h"


/* Internal helper to allocate and set up a new asdf_file_t */
static asdf_file_t *asdf_file_create() {
    /* Try to allocate asdf_file_t object, returns NULL on memory allocation failure*/
    asdf_file_t *file = calloc(1, sizeof(asdf_file_t));

    if (UNLIKELY(!file))
        return NULL;

    /* Basic parser settings for high-level file interface: ignore individual YAML events and
     * just store the tree in memory to parse into a fy_document later */
    asdf_parser_cfg_t parser_cfg = {.flags = ASDF_PARSER_OPT_BUFFER_TREE};
    asdf_parser_t *parser = asdf_parser_create(&parser_cfg);

    if (!parser)
        return NULL;

    file->base.ctx = parser->base.ctx;
    asdf_context_retain(file->base.ctx);
    file->parser = parser;
    /* Now we can start cooking */
    return file;
}


asdf_file_t *asdf_open_file(const char *filename, const char *mode) {
    asdf_file_t *file = asdf_file_create();

    if (!file)
        return NULL;

    /* Currently only the mode string "r" is supported */
    if ((0 != strcasecmp(mode, "r"))) {
        ASDF_ERROR(file, "invalid asdf file mode: %s", mode);
        return file;
    }

    asdf_parser_set_input_file(file->parser, filename);
    return file;
}


asdf_file_t *asdf_open_fp(FILE *fp, const char *filename) {
    asdf_file_t *file = asdf_file_create();

    if (!file)
        return NULL;

    asdf_parser_set_input_fp(file->parser, fp, filename);
    return file;
}


asdf_file_t *asdf_open_mem(const void *buf, size_t size) {
    asdf_file_t *file = asdf_file_create();

    if (!file)
        return NULL;

    asdf_parser_set_input_mem(file->parser, buf, size);
    return file;
}


void asdf_close(asdf_file_t *file) {
    if (!file)
        return;

    asdf_context_release(file->base.ctx);
    asdf_parser_destroy(file->parser);
    fy_document_destroy(file->tree);
    /* Clean up */
    ZERO_MEMORY(file, sizeof(asdf_file_t));
    free(file);
}


ASDF_LOCAL struct fy_document *asdf_file_get_tree_document(asdf_file_t *file) {
    if (!file)
        return NULL;

    if (file->tree)
        /* Already exists and ready to go */
        return file->tree;

    asdf_parser_t *parser = file->parser;

    if (!parser)
        return NULL;

    if (UNLIKELY(0 == parser->tree.has_tree))
        return NULL;

    asdf_event_t *event = NULL;

    if (parser->tree.has_tree < 0) {
        /* We have to run the parser until the tree is found or we hit a block or eof (no tree) */
        while ((event = asdf_event_iterate(parser))) {
            asdf_event_type_t event_type = asdf_event_type(event);
            switch (event_type) {
            case ASDF_TREE_END_EVENT:
                goto has_tree;
            case ASDF_BLOCK_EVENT:
            case ASDF_END_EVENT:
                asdf_event_free(parser, event);
                return NULL;
            default:
                break;
            }
        }

        return NULL;
    }
has_tree:
    asdf_event_free(parser, event);

    if (parser->tree.has_tree < 1 || parser->tree.buf == NULL) {
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "logic error: there should be a YAML tree in the file at "
            "this point but it was not found (tree.has_tree = %d; tree.buf = 0x%zu)",
            parser->tree.has_tree,
            parser->tree.buf);
        return NULL;
    }

    size_t size = parser->tree.size;
    const char *buf = (const char *)parser->tree.buf;
    file->tree = fy_document_build_from_string(NULL, buf, size);
    return file->tree;
}
