/**
 * @file xf_shell_parser.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026, CorAL. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */
#include "xf_shell_parser.h"
#include <stdio.h>
#include <string.h>
#include "xf_shell_cli.h"
#include "xf_shell_cmd_list.h"
#include "xf_shell_options.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef xf_shell_cmd_t cmd_item_t;
typedef xf_opt_arg_t cmd_opt_t;
typedef xf_arg_t cmd_arg_t;

typedef struct {
    xf_shell_parser_puts_t puts_fn;
    void* puts_ctx;
} parser_output_t;

/* ==================== [Static Prototypes] ================================= */

static cmd_opt_t* find_opt_by_name(cmd_item_t* cmd, const char* name);
static cmd_opt_t* find_opt_by_short(cmd_item_t* cmd, char short_opt);
static cmd_opt_t* find_opt_by_long_name(cmd_item_t* cmd, const char* name, size_t len);
static cmd_arg_t* find_arg_by_name(cmd_item_t* cmd, const char* name);
static int collect_positional_values(cmd_item_t* cmd, int argc, const char** argv, const char** values, int max_values);
static bool is_help_token(const char* arg);
static cmd_opt_t* find_help_target_option(cmd_item_t* cmd, int argc, const char** argv, int help_idx);
static bool handle_builtin_help(cmd_item_t* cmd, int argc, const char** argv, const parser_output_t* out);
static bool validate_known_options(cmd_item_t* cmd, int argc, const char** argv, const char** bad_token);
static const char* opt_expected_desc(xf_opt_type_t type);
static void out_puts(const parser_output_t* out, const char* s);
static void print_option_usage(cmd_opt_t* opt, const parser_output_t* out);
static void print_arg_usage(cmd_arg_t* arg, const parser_output_t* out);
static void print_command_help(cmd_item_t* cmd, const parser_output_t* out);
static void print_command_args_usage(cmd_item_t* cmd, const parser_output_t* out);
static void print_command_options_usage(cmd_item_t* cmd, const parser_output_t* out);
static void print_option_error(cmd_opt_t* opt, const char* message, bool append_help, const parser_output_t* out);
static void print_arg_error(cmd_arg_t* arg, const char* message, bool append_help, const parser_output_t* out);
static void print_unknown_option_error(cmd_item_t* cmd, const char* token, bool append_help, const parser_output_t* out);
static void print_unknown_arg_error(cmd_item_t* cmd, const char* token, bool append_help, const parser_output_t* out);

/* ==================== [Static Variables] ================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

void xf_shell_parser_sync_opt_runtime(xf_opt_arg_t* opt) {
    if (opt == NULL) {
        return;
    }

    opt->_opt.long_opt = opt->long_opt;
    opt->_opt.description = opt->description;
    opt->_opt.short_opt = (uint32_t)(uint8_t)opt->short_opt;
    opt->_opt.require = opt->require ? 1U : 0U;
    opt->_opt.has_default = opt->has_default ? 1U : 0U;
    opt->_opt.provided = 0U;
    opt->_opt.type = opt->type;

    if (opt->has_default) {
        switch (opt->type) {
            case XF_OPTION_TYPE_BOOL:
                opt->_opt.boolean = opt->default_boolean;
                break;
            case XF_OPTION_TYPE_INT:
                opt->_opt.integer = opt->default_integer;
                break;
            case XF_OPTION_TYPE_STRING:
                opt->_opt.string = opt->default_string;
                break;
            case XF_OPTION_TYPE_FLOAT:
                opt->_opt.floating = opt->default_floating;
                break;
            default:
                opt->_opt.integer = 0;
                break;
        }
    } else {
        switch (opt->type) {
            case XF_OPTION_TYPE_BOOL:
                opt->_opt.boolean = false;
                break;
            case XF_OPTION_TYPE_INT:
                opt->_opt.integer = 0;
                break;
            case XF_OPTION_TYPE_STRING:
                opt->_opt.string = NULL;
                break;
            case XF_OPTION_TYPE_FLOAT:
                opt->_opt.floating = 0.0f;
                break;
            default:
                opt->_opt.integer = 0;
                break;
        }
    }
}

void xf_shell_parser_sync_arg_runtime(xf_arg_t* arg) {
    if (arg == NULL) {
        return;
    }

    arg->_opt.long_opt = arg->name;
    arg->_opt.description = arg->description;
    arg->_opt.short_opt = 0U;
    arg->_opt.require = arg->require ? 1U : 0U;
    arg->_opt.has_default = arg->has_default ? 1U : 0U;
    arg->_opt.provided = 0U;
    arg->_opt.type = arg->type;

    if (arg->has_default) {
        switch (arg->type) {
            case XF_OPTION_TYPE_BOOL:
                arg->_opt.boolean = arg->default_boolean;
                break;
            case XF_OPTION_TYPE_INT:
                arg->_opt.integer = arg->default_integer;
                break;
            case XF_OPTION_TYPE_STRING:
                arg->_opt.string = arg->default_string;
                break;
            case XF_OPTION_TYPE_FLOAT:
                arg->_opt.floating = arg->default_floating;
                break;
            default:
                arg->_opt.integer = 0;
                break;
        }
    } else {
        switch (arg->type) {
            case XF_OPTION_TYPE_BOOL:
                arg->_opt.boolean = false;
                break;
            case XF_OPTION_TYPE_INT:
                arg->_opt.integer = 0;
                break;
            case XF_OPTION_TYPE_STRING:
                arg->_opt.string = NULL;
                break;
            case XF_OPTION_TYPE_FLOAT:
                arg->_opt.floating = 0.0f;
                break;
            default:
                arg->_opt.integer = 0;
                break;
        }
    }
}

int xf_shell_parser_run(xf_shell_cmd_t* cmd, int argc, const char** argv, xf_shell_parser_puts_t puts_fn, void* puts_ctx) {
    xf_cmd_list_t* next;
    const char* bad_token = NULL;
    const char* positional_values[XF_CLI_MAX_ARGC];
    int positional_count = 0;
    int positional_index = 0;
    parser_output_t out = {
        .puts_fn = puts_fn,
        .puts_ctx = puts_ctx,
    };

    if (cmd == NULL || argc <= 0 || argv == NULL || argv[0] == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    if (handle_builtin_help(cmd, argc, argv, &out)) {
        return XF_CMD_OK;
    }

    if (!validate_known_options(cmd, argc, argv, &bad_token)) {
        print_unknown_option_error(cmd, bad_token, true, &out);
        return XF_CMD_NO_INVALID_ARG;
    }

    for (next = xf_cmd_list_get_next(&cmd->_opt_list); next != &cmd->_opt_list; next = xf_cmd_list_get_next(next)) {
        cmd_opt_t* it = GET_PARENT_ADDR(next, cmd_opt_t, _node);
        bool should_append_help = true;
        const char* user_error = NULL;

        xf_shell_parser_sync_opt_runtime(it);

        {
            int arg_ret = xf_opt_parse(&it->_opt, argc, argv);
            if (arg_ret == XF_OPTION_ERR_HELP) {
                print_option_usage(it, &out);
                return XF_CMD_OK;
            }
            if (arg_ret == XF_OPTION_ERR_INVALID_ARG) {
                print_option_error(it, NULL, true, &out);
                return XF_CMD_NO_INVALID_ARG;
            }
            if (arg_ret == XF_OPTION_ERR_NO_FOUND && it->require) {
                print_option_error(it, "required option is missing", true, &out);
                return XF_CMD_NO_INVALID_ARG;
            }
        }

        if (it->validator != NULL && (it->_opt.provided || it->_opt.has_default)) {
            if (!it->validator(it, &user_error, &should_append_help)) {
                print_option_error(it, user_error, should_append_help, &out);
                return XF_CMD_NO_INVALID_ARG;
            }
        }
    }

    positional_count = collect_positional_values(cmd, argc, argv, positional_values, XF_CLI_MAX_ARGC);

    if (positional_count > 0 && xf_cmd_list_is_empty(&cmd->_arg_list)) {
        print_unknown_arg_error(cmd, positional_values[0], true, &out);
        return XF_CMD_NO_INVALID_ARG;
    }

    for (next = xf_cmd_list_get_next(&cmd->_arg_list); next != &cmd->_arg_list; next = xf_cmd_list_get_next(next)) {
        cmd_arg_t* it = GET_PARENT_ADDR(next, cmd_arg_t, _node);
        bool should_append_help = true;
        const char* user_error = NULL;
        int parse_ret = XF_OPTION_OK;

        xf_shell_parser_sync_arg_runtime(it);

        if (positional_index < positional_count) {
            parse_ret = xf_opt_parse_value(&it->_opt, positional_values[positional_index]);
            if (parse_ret != XF_OPTION_OK) {
                print_arg_error(it, NULL, true, &out);
                return XF_CMD_NO_INVALID_ARG;
            }
            it->_opt.provided = 1U;
            positional_index++;
        } else if (it->require) {
            print_arg_error(it, "required argument is missing", true, &out);
            return XF_CMD_NO_INVALID_ARG;
        }

        if (it->validator != NULL && (it->_opt.provided || it->_opt.has_default)) {
            if (!it->validator(it, &user_error, &should_append_help)) {
                print_arg_error(it, user_error, should_append_help, &out);
                return XF_CMD_NO_INVALID_ARG;
            }
        }
    }

    if (positional_index < positional_count) {
        print_unknown_arg_error(cmd, positional_values[positional_index], true, &out);
        return XF_CMD_NO_INVALID_ARG;
    }

    return cmd->func((const xf_cmd_args_t*)cmd);
}

int xf_shell_parser_get_int(const xf_cmd_args_t* cmd, const char* long_opt, int32_t* value) {
    cmd_item_t* item = (cmd_item_t*)cmd;
    cmd_opt_t* opt;
    cmd_arg_t* arg;

    if (item == NULL || value == NULL || long_opt == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    opt = find_opt_by_name(item, long_opt);
    if (opt != NULL) {
        *value = opt->_opt.integer;
        return XF_CMD_OK;
    }

    arg = find_arg_by_name(item, long_opt);
    if (arg != NULL) {
        *value = arg->_opt.integer;
        return XF_CMD_OK;
    }

    return XF_CMD_NOT_SUPPORTED;
}

int xf_shell_parser_get_bool(const xf_cmd_args_t* cmd, const char* long_opt, bool* value) {
    cmd_item_t* item = (cmd_item_t*)cmd;
    cmd_opt_t* opt;
    cmd_arg_t* arg;

    if (item == NULL || value == NULL || long_opt == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    opt = find_opt_by_name(item, long_opt);
    if (opt != NULL) {
        *value = opt->_opt.boolean;
        return XF_CMD_OK;
    }

    arg = find_arg_by_name(item, long_opt);
    if (arg != NULL) {
        *value = arg->_opt.boolean;
        return XF_CMD_OK;
    }

    return XF_CMD_NOT_SUPPORTED;
}

int xf_shell_parser_get_float(const xf_cmd_args_t* cmd, const char* long_opt, float* value) {
    cmd_item_t* item = (cmd_item_t*)cmd;
    cmd_opt_t* opt;
    cmd_arg_t* arg;

    if (item == NULL || value == NULL || long_opt == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    opt = find_opt_by_name(item, long_opt);
    if (opt != NULL) {
        *value = opt->_opt.floating;
        return XF_CMD_OK;
    }

    arg = find_arg_by_name(item, long_opt);
    if (arg != NULL) {
        *value = arg->_opt.floating;
        return XF_CMD_OK;
    }

    return XF_CMD_NOT_SUPPORTED;
}

int xf_shell_parser_get_string(const xf_cmd_args_t* cmd, const char* long_opt, const char** value) {
    cmd_item_t* item = (cmd_item_t*)cmd;
    cmd_opt_t* opt;
    cmd_arg_t* arg;

    if (item == NULL || value == NULL || long_opt == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    opt = find_opt_by_name(item, long_opt);
    if (opt != NULL) {
        *value = opt->_opt.string;
        return XF_CMD_OK;
    }

    arg = find_arg_by_name(item, long_opt);
    if (arg != NULL) {
        *value = arg->_opt.string;
        return XF_CMD_OK;
    }

    return XF_CMD_NOT_SUPPORTED;
}

/* ==================== [Static Functions] ================================== */

static cmd_opt_t* find_opt_by_name(cmd_item_t* cmd, const char* name) {
    xf_cmd_list_t* next;

    if (cmd == NULL || name == NULL) {
        return NULL;
    }

    for (next = xf_cmd_list_get_next(&cmd->_opt_list); next != &cmd->_opt_list; next = xf_cmd_list_get_next(next)) {
        cmd_opt_t* it = GET_PARENT_ADDR(next, cmd_opt_t, _node);
        if (it->long_opt != NULL && strcmp(name, it->long_opt) == 0) {
            return it;
        }
    }

    return NULL;
}

static cmd_opt_t* find_opt_by_short(cmd_item_t* cmd, char short_opt) {
    xf_cmd_list_t* next;

    if (cmd == NULL || short_opt == '\0') {
        return NULL;
    }

    for (next = xf_cmd_list_get_next(&cmd->_opt_list); next != &cmd->_opt_list; next = xf_cmd_list_get_next(next)) {
        cmd_opt_t* it = GET_PARENT_ADDR(next, cmd_opt_t, _node);
        if (it->short_opt == short_opt) {
            return it;
        }
    }

    return NULL;
}

static cmd_opt_t* find_opt_by_long_name(cmd_item_t* cmd, const char* name, size_t len) {
    xf_cmd_list_t* next;

    if (cmd == NULL || name == NULL || len == 0U) {
        return NULL;
    }

    for (next = xf_cmd_list_get_next(&cmd->_opt_list); next != &cmd->_opt_list; next = xf_cmd_list_get_next(next)) {
        cmd_opt_t* it = GET_PARENT_ADDR(next, cmd_opt_t, _node);
        if (it->long_opt == NULL) {
            continue;
        }
        if (strncmp(it->long_opt, name, len) == 0 && it->long_opt[len] == '\0') {
            return it;
        }
    }

    return NULL;
}

static cmd_arg_t* find_arg_by_name(cmd_item_t* cmd, const char* name) {
    xf_cmd_list_t* next;

    if (cmd == NULL || name == NULL) {
        return NULL;
    }

    for (next = xf_cmd_list_get_next(&cmd->_arg_list); next != &cmd->_arg_list; next = xf_cmd_list_get_next(next)) {
        cmd_arg_t* it = GET_PARENT_ADDR(next, cmd_arg_t, _node);
        if (it->name != NULL && strcmp(it->name, name) == 0) {
            return it;
        }
    }

    return NULL;
}

static int collect_positional_values(cmd_item_t* cmd, int argc, const char** argv, const char** values, int max_values) {
    int count = 0;
    int i;

    if (argv == NULL || argc <= 0 || values == NULL || max_values <= 0) {
        return 0;
    }

    for (i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (arg == NULL) {
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            ++i;
            for (; i < argc; ++i) {
                if (argv[i] == NULL) {
                    continue;
                }
                if (count >= max_values) {
                    return count;
                }
                values[count++] = argv[i];
            }
            break;
        }

        if (arg[0] == '-' && arg[1] != '\0') {
            if (is_help_token(arg)) {
                continue;
            }

            if (arg[1] == '-') {
                const char* name = &arg[2];
                const char* eq = strchr(name, '=');
                size_t len = eq ? (size_t)(eq - name) : strlen(name);
                cmd_opt_t* opt = find_opt_by_long_name(cmd, name, len);

                if (opt != NULL && eq == NULL && i < argc - 1) {
                    ++i;
                }
                continue;
            }

            if (arg[2] == '\0') {
                cmd_opt_t* opt = find_opt_by_short(cmd, arg[1]);
                if (opt != NULL && i < argc - 1) {
                    ++i;
                }
                continue;
            }

            if (arg[2] == '=') {
                continue;
            }
        }

        if (count >= max_values) {
            return count;
        }
        values[count++] = arg;
    }

    return count;
}

static bool is_help_token(const char* arg) {
    if (arg == NULL) {
        return false;
    }
    return (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0);
}

static cmd_opt_t* find_help_target_option(cmd_item_t* cmd, int argc, const char** argv, int help_idx) {
    int i;

    if (cmd == NULL || argv == NULL || help_idx <= 1 || help_idx >= argc) {
        return NULL;
    }

    for (i = help_idx - 1; i >= 1; --i) {
        const char* arg = argv[i];

        if (arg == NULL || arg[0] != '-' || arg[1] == '\0') {
            continue;
        }
        if (strcmp(arg, "--") == 0 || is_help_token(arg)) {
            continue;
        }

        if (arg[1] == '-') {
            const char* name = &arg[2];
            const char* eq = strchr(name, '=');
            size_t len = eq ? (size_t)(eq - name) : strlen(name);
            cmd_opt_t* opt = find_opt_by_long_name(cmd, name, len);
            if (opt != NULL) {
                return opt;
            }
            continue;
        }

        if (arg[2] != '\0' && arg[2] != '=') {
            continue;
        }

        {
            cmd_opt_t* opt = find_opt_by_short(cmd, arg[1]);
            if (opt != NULL) {
                return opt;
            }
        }
    }

    return NULL;
}

static bool handle_builtin_help(cmd_item_t* cmd, int argc, const char** argv, const parser_output_t* out) {
    int i;

    if (cmd == NULL || argv == NULL) {
        return false;
    }

    for (i = 1; i < argc; ++i) {
        if (is_help_token(argv[i])) {
            cmd_opt_t* target = find_help_target_option(cmd, argc, argv, i);
            if (target != NULL) {
                print_option_usage(target, out);
            } else {
                print_command_help(cmd, out);
            }
            return true;
        }
    }

    return false;
}

static bool validate_known_options(cmd_item_t* cmd, int argc, const char** argv, const char** bad_token) {
    int i;

    if (cmd == NULL || argv == NULL || argc <= 0) {
        return true;
    }

    for (i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (arg == NULL || arg[0] != '-' || arg[1] == '\0') {
            continue;
        }
        if (strcmp(arg, "--") == 0) {
            break;
        }
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            continue;
        }

        if (arg[1] == '-') {
            const char* name = &arg[2];
            const char* eq = strchr(name, '=');
            size_t len = eq ? (size_t)(eq - name) : strlen(name);

            if (find_opt_by_long_name(cmd, name, len) == NULL) {
                if (bad_token != NULL) {
                    *bad_token = arg;
                }
                return false;
            }
            if (eq == NULL && i < argc - 1) {
                i++;
            }
            continue;
        }

        if (arg[2] != '\0' && arg[2] != '=') {
            if (bad_token != NULL) {
                *bad_token = arg;
            }
            return false;
        }

        if (find_opt_by_short(cmd, arg[1]) == NULL) {
            if (bad_token != NULL) {
                *bad_token = arg;
            }
            return false;
        }
        if (arg[2] != '=' && i < argc - 1) {
            i++;
        }
    }

    return true;
}

static const char* opt_expected_desc(xf_opt_type_t type) {
    switch (type) {
        case XF_OPTION_TYPE_BOOL:
            return "boolean(true/false/1/0/yes/no/on/off)";
        case XF_OPTION_TYPE_INT:
            return "integer";
        case XF_OPTION_TYPE_STRING:
            return "string";
        case XF_OPTION_TYPE_FLOAT:
            return "float";
        default:
            return "value";
    }
}

static void out_puts(const parser_output_t* out, const char* s) {
    if (out == NULL || out->puts_fn == NULL || s == NULL) {
        return;
    }
    out->puts_fn(out->puts_ctx, s);
}

static void print_option_usage(cmd_opt_t* opt, const parser_output_t* out) {
    char msg[1024];

    xf_shell_parser_sync_opt_runtime(opt);
    xf_opt_parse_usage(&opt->_opt, msg);
    out_puts(out, msg);
}

static void print_arg_usage(cmd_arg_t* arg, const parser_output_t* out) {
    char buffer[256];
    int count = 0;

    if (arg == NULL) {
        return;
    }

    xf_shell_parser_sync_arg_runtime(arg);

    count += snprintf(buffer + count, sizeof(buffer) - (size_t)count, "\t<%s>\t%s", arg->name ? arg->name : "arg", arg->description ? arg->description : "");

    if (arg->has_default) {
        switch (arg->type) {
            case XF_OPTION_TYPE_INT:
                count += snprintf(buffer + count, sizeof(buffer) - (size_t)count, " [default: %d]", arg->_opt.integer);
                break;
            case XF_OPTION_TYPE_STRING:
                count += snprintf(buffer + count, sizeof(buffer) - (size_t)count, " [default: \"%s\"]", arg->_opt.string ? arg->_opt.string : "");
                break;
            case XF_OPTION_TYPE_BOOL:
                count += snprintf(buffer + count, sizeof(buffer) - (size_t)count, " [default: %s]", arg->_opt.boolean ? "true" : "false");
                break;
            case XF_OPTION_TYPE_FLOAT:
                count += snprintf(buffer + count, sizeof(buffer) - (size_t)count, " [default: %f]", arg->_opt.floating);
                break;
            default:
                break;
        }
    }

    if (arg->require) {
        count += snprintf(buffer + count, sizeof(buffer) - (size_t)count, " [required]");
    }

    (void)count;
    out_puts(out, buffer);
    out_puts(out, XF_SHELL_NEWLINE);
}

static void print_command_help(cmd_item_t* cmd, const parser_output_t* out) {
    char buffer[256];

    if (cmd == NULL) {
        return;
    }

    if (cmd->help != NULL && cmd->help[0] != '\0') {
        snprintf(buffer, sizeof(buffer), "%s: %s" XF_SHELL_NEWLINE, cmd->command, cmd->help);
    } else {
        snprintf(buffer, sizeof(buffer), "%s" XF_SHELL_NEWLINE, cmd->command);
    }
    out_puts(out, buffer);
    print_command_args_usage(cmd, out);
    print_command_options_usage(cmd, out);
}

static void print_command_args_usage(cmd_item_t* cmd, const parser_output_t* out) {
    xf_cmd_list_t* next;

    if (cmd == NULL) {
        return;
    }

    for (next = xf_cmd_list_get_next(&cmd->_arg_list); next != &cmd->_arg_list; next = xf_cmd_list_get_next(next)) {
        cmd_arg_t* arg = GET_PARENT_ADDR(next, cmd_arg_t, _node);
        print_arg_usage(arg, out);
    }
}

static void print_command_options_usage(cmd_item_t* cmd, const parser_output_t* out) {
    xf_cmd_list_t* next;

    if (cmd == NULL) {
        return;
    }

    for (next = xf_cmd_list_get_next(&cmd->_opt_list); next != &cmd->_opt_list; next = xf_cmd_list_get_next(next)) {
        cmd_opt_t* opt = GET_PARENT_ADDR(next, cmd_opt_t, _node);
        print_option_usage(opt, out);
    }
}

static void print_option_error(cmd_opt_t* opt, const char* message, bool append_help, const parser_output_t* out) {
    char buffer[256];
    const char* name = (opt->long_opt != NULL && opt->long_opt[0] != '\0') ? opt->long_opt : "(unknown)";
    const char* msg = message;

    if (msg == NULL || msg[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "invalid value for --%s, expected %s" XF_SHELL_NEWLINE, name, opt_expected_desc(opt->type));
    } else {
        snprintf(buffer, sizeof(buffer), "invalid value for --%s: %s" XF_SHELL_NEWLINE, name, msg);
    }

    out_puts(out, buffer);
    if (append_help) {
        print_option_usage(opt, out);
    }
}

static void print_arg_error(cmd_arg_t* arg, const char* message, bool append_help, const parser_output_t* out) {
    char buffer[256];
    const char* name = (arg->name != NULL && arg->name[0] != '\0') ? arg->name : "(unknown)";
    const char* msg = message;

    if (msg == NULL || msg[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "invalid value for argument <%s>, expected %s" XF_SHELL_NEWLINE, name, opt_expected_desc(arg->type));
    } else {
        snprintf(buffer, sizeof(buffer), "invalid argument <%s>: %s" XF_SHELL_NEWLINE, name, msg);
    }

    out_puts(out, buffer);
    if (append_help) {
        print_arg_usage(arg, out);
    }
}

static void print_unknown_option_error(cmd_item_t* cmd, const char* token, bool append_help, const parser_output_t* out) {
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "unknown option: %s" XF_SHELL_NEWLINE, token ? token : "(null)");
    out_puts(out, buffer);

    if (append_help) {
        print_command_help(cmd, out);
    }
}

static void print_unknown_arg_error(cmd_item_t* cmd, const char* token, bool append_help, const parser_output_t* out) {
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "unexpected argument: %s" XF_SHELL_NEWLINE, token ? token : "(null)");
    out_puts(out, buffer);

    if (append_help) {
        print_command_help(cmd, out);
    }
}
