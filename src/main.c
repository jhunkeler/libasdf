#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "info.h"

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;


static char doc[] = "asdf — Commandline utilities for managing ASDF files.\n\n";

static char args_doc[] = "COMMAND [ARGS...]";

static struct argp_option global_options[] = {
    { 0 }
};


typedef enum {
    ASDF_SUBCMD_NONE = 0,
    ASDF_SUBCMD_INFO
} asdf_subcmd_t;


struct global_args {
    asdf_subcmd_t subcmd;
    char **subcmd_argv;
    int subcmd_argc;
};


static error_t parse_global_opt(int key, char *arg, struct argp_state *state) {
    struct global_args *args = state->input;

    switch (key) {
        case ARGP_KEY_ARG:
            if (!args->subcmd) {
                if (strcmp(arg, "info") == 0) {
                    args->subcmd = ASDF_SUBCMD_INFO;
                } else {
                    argp_state_help(state, stdout, ARGP_HELP_STD_ERR);
                }
                args->subcmd_argv = &state->argv[state->next];
                args->subcmd_argc = state->argc - state->next;
                state->next = state->argc;
            } else {
                argp_error(state, "Only one subcommand is allowed.");
            }
            break;

        case ARGP_KEY_NO_ARGS:
            argp_state_help(state, stdout, ARGP_HELP_STD_USAGE);
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


static char info_doc[] = "Print a rendering of an ASDF tree.";
static char info_args_doc[] = "FILENAME";


static struct argp_option info_options[] = {
    { 0 }
};


struct info_args {
    const char *filename;
};


static error_t parse_info_opt(int key, char *arg, struct argp_state *state) {
    struct info_args *args = state->input;

    switch (key) {
      case ARGP_KEY_ARG:
        if (!args->filename)
          args->filename = arg;
        else
          argp_error(state, "Too many arguments for 'info'. Only FILENAME is expected.");
        break;

      case ARGP_KEY_NO_ARGS:
        argp_error(state, "'info' requires a FILENAME argument.");
        break;

      default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


static struct argp info_argp = {
    info_options,
    parse_info_opt,
    info_args_doc,
    info_doc
};


int main(int argc, char *argv[]) {
    struct global_args global_args = {0};
    struct argp global_argp = {global_options, parse_global_opt, args_doc, doc};

    argp_parse(&global_argp, argc, argv, 0, 0, &global_args);

    if (global_args.subcmd == ASDF_SUBCMD_INFO) {
        struct info_args info_args = {0};
        argp_parse(
            &info_argp,
            global_args.subcmd_argc + 1,
            global_args.subcmd_argv - 1,
            0,
            0,
            &info_args
        );

        FILE *file = fopen(info_args.filename, "r");

        if (!file) {
            perror("error");
            return EXIT_FAILURE;
        }

        asdf_info(file, stdout);
    } else {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
