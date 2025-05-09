#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libfyaml.h>

#include "info.h"


#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define BOLD(str) ANSI_BOLD str ANSI_RESET
#define DIM(str) ANSI_DIM str ANSI_RESET


typedef struct tree_node {
    bool is_leaf;
    struct tree_node *parent;
} tree_node_t;


/** TODO: Maybe rewrite to avoid recursion in deeply nested trees **/

static void print_indent(FILE *file, tree_node_t *tree) {
    if (!tree)
        return;

    print_indent(file, tree->parent);

    if (tree->parent) {
        if (tree->parent->is_leaf)
            fprintf(file, "  ");
        else
            fprintf(file, DIM("│") " ");
    }
}


static void print_scalar_value(FILE *file, struct fy_node *node) {
    const char *str = NULL;
    size_t len = 0;
    if ((str = fy_node_get_scalar(node, &len))) {
        fprintf(file, ": %.*s", (int)len, str);
    }
}


static const char *get_node_type_display_name(struct fy_node *node, size_t *lenp) {
    size_t tag_len = 0;
    const char *tag = fy_node_get_tag(node, &tag_len);
    if (tag && tag_len > 0) {
        *lenp = tag_len;
        return tag;
    }

    enum fy_node_type type = fy_node_get_type(node);
    const char *node_name = NULL;
    switch (type) {
    case FYNT_SCALAR:
        node_name = "scalar";
        break;
    case FYNT_SEQUENCE:
        node_name = "sequence";
        break;
    case FYNT_MAPPING:
        node_name = "mapping";
        break;
    default:
        node_name = "unknown";
    }

    *lenp = strlen(node_name);
    return node_name;
}

/* Forward declaration */
static void print_node(FILE *file,
    struct fy_node *node,
    tree_node_t *tree,
    const char *key_label,
    bool is_mapping_key);


static void print_mapping_node(FILE *file, struct fy_node *node, tree_node_t *tree) {
    struct fy_node *key = NULL;
    struct fy_node *value = NULL;
    const char *key_str = NULL;
    char *key_label = NULL;
    size_t key_len = 0;
    void *iter = NULL;
    struct fy_node_pair *curr = NULL;
    struct fy_node_pair *next = fy_node_mapping_iterate(node, &iter);

    while (next != NULL) {
        curr = next;
        next = fy_node_mapping_iterate(node, &iter);
        key = fy_node_pair_key(curr);
        value = fy_node_pair_value(curr);

        if ((key_str = fy_node_get_scalar(key, &key_len))) {
            key_label = strndup(key_str, key_len);
            tree_node_t child = {.is_leaf = (next == NULL), .parent = tree};
            print_node(file, value, &child, key_label, true);
            free(key_label);
        }
    }
}


static void print_sequence_node(FILE *file, struct fy_node *node, tree_node_t *tree) {
    int index = 0;
    char label_buf[16]; // NOLINT(readability-magic-numbers)
    void *iter = NULL;
    struct fy_node *curr = NULL;
    struct fy_node *next = fy_node_sequence_iterate(node, &iter);

    while (next != NULL) {
        curr = next;
        next = fy_node_sequence_iterate(node, &iter);
        snprintf(label_buf, sizeof(label_buf), "%d", index++);
        tree_node_t child = {.is_leaf = (next == NULL), .parent = tree};
        print_node(file, curr, &child, label_buf, false);
    }
}


static void print_node(FILE *file,
    struct fy_node *node,
    tree_node_t *tree,
    const char *key_label,
    bool is_mapping_key) {
    if (!key_label)
        return;

    print_indent(file, tree);

    if (tree) {
        if (tree->is_leaf)
            fprintf(file, DIM("└─"));
        else
            fprintf(file, DIM("├─"));
    }

    if (is_mapping_key) {
        fprintf(file, BOLD("%s") " ", key_label);
    } else {
        fprintf(file, "[" BOLD("%s") "] ", key_label);
    }

    size_t type_len;
    const char *type_str = get_node_type_display_name(node, &type_len);
    fprintf(file, "(%.*s)", (int)type_len, type_str);

    if (fy_node_get_type(node) == FYNT_SCALAR)
        print_scalar_value(file, node);

    fprintf(file, "\n");

    if (fy_node_get_type(node) == FYNT_MAPPING)
        print_mapping_node(file, node, tree);
    else if (fy_node_get_type(node) == FYNT_SEQUENCE)
        print_sequence_node(file, node, tree);
}


int asdf_info(FILE *in_file, FILE *out_file) {
    struct fy_document *fydoc = fy_document_build_from_fp(NULL, in_file);
    if (!fydoc) {
        fprintf(stderr, "Failed to parse YAML document\n");
        return 1;
    }

    struct fy_node *root = fy_document_root(fydoc);
    print_node(out_file, root, NULL, "root", true);

    fy_document_destroy(fydoc);
    return 0;
}
