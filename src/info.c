#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libfyaml.h>

#include "event.h"
#include "parse.h"
#include "yaml.h"


#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define BOLD(str) ANSI_BOLD str ANSI_RESET
#define DIM(str) ANSI_DIM str ANSI_RESET


// clang-format off
typedef enum {
    TREE_SCALAR,
    TREE_MAPPING,
    TREE_SEQUENCE
} tree_node_type_t;
// clang-format on


typedef struct tree_node {
    tree_node_type_t type;

    union {
        char *key;
        size_t index;
    } index;
    char *tag;
    char *value;

    struct tree_node *parent;
    struct tree_node *first_child;
    struct tree_node *last_child;
    struct tree_node *next_sibling;

    union {
        struct {
            char *pending_key;
        } mapping;

        struct {
            size_t index;
        } sequence;
    } state;
} tree_node_t;


typedef struct tree_node_stack {
    tree_node_t *node;
    struct tree_node_stack *next;
} tree_node_stack_t;


tree_node_t *tree_node_new(tree_node_type_t type, const char *key, size_t index) {
    tree_node_t *node = calloc(1, sizeof(tree_node_t));
    node->type = type;

    if (key) {
        node->index.key = strdup(key);
    } else {
        node->index.index = index;
    }

    switch (type) {
    case TREE_MAPPING:
        node->state.mapping.pending_key = NULL;
        break;
    case TREE_SEQUENCE:
        node->state.sequence.index = 0;
        break;
    default:
        break;
    }

    return node;
}


void tree_node_add_child(tree_node_t *parent, tree_node_t *child) {
    child->parent = parent;
    if (!parent->first_child) {
        parent->first_child = parent->last_child = child;
    } else {
        parent->last_child->next_sibling = child;
        parent->last_child = child;
    }
}


void tree_node_free(tree_node_t *node) {
    tree_node_t *child = node->first_child;
    while (child) {
        tree_node_t *next = child->next_sibling;
        tree_node_free(child);
        child = next;
    }

    if (node->parent && node->parent->type == TREE_MAPPING)
        free(node->index.key);

    free(node->tag);
    free(node->value);
    free(node);
}


static void stack_push(tree_node_stack_t **stack, tree_node_t *node) {
    tree_node_stack_t *new_item = malloc(sizeof(tree_node_stack_t));
    new_item->node = node;
    new_item->next = *stack;
    *stack = new_item;
}


static tree_node_t *stack_pop(tree_node_stack_t **stack) {
    tree_node_stack_t *top = *stack;

    if (!top)
        return NULL;

    tree_node_t *node = top->node;
    *stack = top->next;
    free(top);
    return node;
}


static tree_node_t *stack_peek(tree_node_stack_t *stack) {
    return stack ? stack->node : NULL;
}


tree_node_t *build_tree(asdf_parser_t *parser) {
    asdf_event_t event = {0};
    tree_node_t *root = NULL;
    tree_node_stack_t *stack = NULL;
    const char *tag = NULL;
    size_t tag_len = 0;

    while (asdf_event_iterate(parser, &event) == 0) {
        asdf_yaml_event_type_t type = asdf_yaml_event_type(&event);
        tree_node_t *parent = stack_peek(stack);
        tree_node_t *node = NULL;

        switch (type) {
        /* TODO: Handle anchor events */
        case ASDF_YAML_MAPPING_START_EVENT:
        case ASDF_YAML_SEQUENCE_START_EVENT: {
            tree_node_type_t node_type =
                (type == ASDF_YAML_MAPPING_START_EVENT) ? TREE_MAPPING : TREE_SEQUENCE;
            const char *key = NULL;
            size_t index = 0;

            if (parent == NULL) {
                root = tree_node_new(node_type, NULL, 0);
                node = root;
            } else {
                switch (parent->type) {
                case TREE_MAPPING:
                    key = parent->state.mapping.pending_key;
                    parent->state.mapping.pending_key = NULL;
                    break;
                case TREE_SEQUENCE:
                    index = parent->state.sequence.index++;
                default:
                    break;
                }
                node = tree_node_new(node_type, key, index);
                tree_node_add_child(parent, node);
            }

            tag = asdf_yaml_event_tag(&event, &tag_len);

            if (tag && tag_len > 0) {
                node->tag = strndup(tag, tag_len);
            }

            stack_push(&stack, node);
            break;
        }

        case ASDF_YAML_MAPPING_END_EVENT:
        case ASDF_YAML_SEQUENCE_END_EVENT:
            stack_pop(&stack);
            break;

        case ASDF_YAML_SCALAR_EVENT: {
            if (!parent)
                continue;

            const char *key = NULL;
            size_t index = 0;
            size_t val_len = 0;
            const char *value = asdf_yaml_event_scalar_value(&event, &val_len);
            tag = asdf_yaml_event_tag(&event, &tag_len);

            if (parent->type == TREE_MAPPING && !parent->state.mapping.pending_key) {
                parent->state.mapping.pending_key = strndup(value, val_len);
                continue;
            }

            switch (parent->type) {
            case TREE_MAPPING:
                key = parent->state.mapping.pending_key;
                parent->state.mapping.pending_key = NULL;
                break;
            case TREE_SEQUENCE:
                index = parent->state.sequence.index++;
                break;
            default:
                break;
            }

            node = tree_node_new(TREE_SCALAR, key, index);
            node->value = strndup(value, val_len);
            if (tag && tag_len > 0)
                node->tag = strndup(tag, tag_len);
            tree_node_add_child(parent, node);
            break;
        }

        default:
            // ignore other ASDF event types for now
            break;
        }

        asdf_event_destroy(parser, &event);
    }

    // Defensive cleanup of the stack; unlikely to be needed but possible in case of
    // an error or malformed document
    while (stack != NULL)
        stack_pop(&stack);

    return root;
}


static void print_indent(FILE *file, const tree_node_t *node) {
    if (!node)
        return;

    print_indent(file, node->parent);

    if (node->parent && node->parent->parent) {
        if (node->parent->next_sibling)
            fprintf(file, DIM("│") " ");
        else
            fprintf(file, "  ");
    }
}


void print_tree(FILE *file, const tree_node_t *node) {
    if (!node)
        return;

    print_indent(file, node);

    if (node->parent) {
        fprintf(file, node->next_sibling ? DIM("├─") : DIM("└─"));

        switch (node->parent->type) {
        case TREE_MAPPING:
            fprintf(file, BOLD("%s") " ", node->index.key);
            break;
        case TREE_SEQUENCE:
            fprintf(file, "[" BOLD("%zu") "] ", node->index.index);
            break;
        default:
            break;
        }
    } else {
        fprintf(file, BOLD("root") " ");
    }

    if (node->tag) {
        fprintf(file, "(%s)", node->tag);
    } else {
        switch (node->type) {
        case TREE_SCALAR:
            fprintf(file, "(scalar)");
            break;
        case TREE_MAPPING:
            fprintf(file, "(mapping)");
            break;
        case TREE_SEQUENCE:
            fprintf(file, "(sequence)");
            break;
        }
    }

    if (node->value)
        fprintf(file, ": %s", node->value);

    fprintf(file, "\n");

    const tree_node_t *child = node->first_child;
    while (child) {
        print_tree(file, child);
        child = child->next_sibling;
    }
}


int asdf_info(FILE *in_file, FILE *out_file, const char *filename) {
    asdf_parser_t parser = {0};

    if (asdf_parser_init(&parser) != 0)
        return 1;

    if (asdf_parser_set_input_file(&parser, in_file, filename) != 0) {
        asdf_parser_destroy(&parser);
        return 1;
    }

    tree_node_t *root = build_tree(&parser);
    print_tree(out_file, root);
    tree_node_free(root);
    asdf_parser_destroy(&parser);
    return 0;
}
