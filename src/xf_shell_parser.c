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
#include "xf_shell_options.h"

/* ==================== [Defines] =========================================== */

#if defined(__GNUC__) || defined(__clang__)
#define XF_SHELL_PARSER_NOINLINE __attribute__((noinline))
#else
#define XF_SHELL_PARSER_NOINLINE
#endif

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
static xf_options_t* find_runtime_value_by_name(cmd_item_t* cmd, const char* name);
static int collect_positional_values(cmd_item_t* cmd, int argc, const char** argv, const char** values, int max_values);
static XF_SHELL_PARSER_NOINLINE void sync_runtime_value(xf_options_t* runtime,
                                                        xf_opt_type_t type,
                                                        bool require,
                                                        bool has_default,
                                                        int32_t default_integer,
                                                        bool default_boolean,
                                                        float default_floating,
                                                        const char* default_string);
static XF_SHELL_PARSER_NOINLINE int parser_get_value(const xf_cmd_args_t* cmd,
                                                     const char* long_opt,
                                                     void* value,
                                                     xf_opt_type_t type);
static bool is_help_token(const char* arg);
#if XF_SHELL_PARSER_HELP_ENABLE
static cmd_opt_t* find_help_target_option(cmd_item_t* cmd, int argc, const char** argv, int help_idx);
static bool handle_builtin_help(cmd_item_t* cmd, int argc, const char** argv, const parser_output_t* out);
static const char* opt_expected_desc(xf_opt_type_t type);
static void out_puts(const parser_output_t* out, const char* s);
static void print_option_usage(cmd_opt_t* opt, const parser_output_t* out);
static void print_arg_usage(cmd_arg_t* arg, const parser_output_t* out);
static void print_command_help(cmd_item_t* cmd, const parser_output_t* out);
static void print_command_args_usage(cmd_item_t* cmd, const parser_output_t* out);
static void print_command_options_usage(cmd_item_t* cmd, const parser_output_t* out);
static void print_option_error(cmd_opt_t* opt, const char* message, bool append_help, const parser_output_t* out);
static void print_arg_error(cmd_arg_t* arg, const char* message, bool append_help, const parser_output_t* out);
#else
static void out_puts(const parser_output_t* out, const char* s);
static void print_option_error(cmd_opt_t* opt, const char* message, bool append_help, const parser_output_t* out);
static void print_arg_error(cmd_arg_t* arg, const char* message, bool append_help, const parser_output_t* out);
#endif
static bool validate_known_options(cmd_item_t* cmd, int argc, const char** argv, const char** bad_token);
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
    sync_runtime_value(&opt->_opt, opt->type, opt->require, opt->has_default,
                       opt->default_integer, opt->default_boolean,
                       opt->default_floating, opt->default_string);
}

void xf_shell_parser_sync_arg_runtime(xf_arg_t* arg) {
    if (arg == NULL) {
        return;
    }

    arg->_opt.long_opt = arg->name;
    arg->_opt.description = arg->description;
    arg->_opt.short_opt = 0U;
    sync_runtime_value(&arg->_opt, arg->type, arg->require, arg->has_default,
                       arg->default_integer, arg->default_boolean,
                       arg->default_floating, arg->default_string);
}

int xf_shell_parser_run(xf_shell_cmd_t* cmd, int argc, const char** argv, xf_shell_parser_puts_t puts_fn, void* puts_ctx) {
    const char* bad_token = NULL;
    const char* positional_values[XF_CLI_MAX_ARGC];
    int positional_count = 0;
    int positional_index = 0;
    uint16_t i;
    parser_output_t out = {
        .puts_fn = puts_fn,
        .puts_ctx = puts_ctx,
    };

    if (cmd == NULL || argc <= 0 || argv == NULL || argv[0] == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

#if XF_SHELL_PARSER_HELP_ENABLE
    if (handle_builtin_help(cmd, argc, argv, &out)) {
        return XF_CMD_OK;
    }
#endif

    if (!validate_known_options(cmd, argc, argv, &bad_token)) {
        print_unknown_option_error(cmd, bad_token, XF_SHELL_PARSER_HELP_ENABLE, &out);
        return XF_CMD_NO_INVALID_ARG;
    }

    for (i = 0; i < cmd->_opt_count; ++i) {
        cmd_opt_t* it = cmd->_opts[i];
        bool should_append_help = true;
        const char* user_error = NULL;

        if (it == NULL) {
            continue;
        }

        xf_shell_parser_sync_opt_runtime(it);

        {
            int arg_ret = xf_opt_parse(&it->_opt, argc, argv);
            if (arg_ret == XF_OPTION_ERR_HELP) {
#if XF_SHELL_PARSER_HELP_ENABLE
                print_option_usage(it, &out);
#else
                print_option_error(it, "help is disabled", false, &out);
#endif
                return XF_CMD_OK;
            }
            if (arg_ret == XF_OPTION_ERR_INVALID_ARG) {
                print_option_error(it, NULL, XF_SHELL_PARSER_HELP_ENABLE, &out);
                return XF_CMD_NO_INVALID_ARG;
            }
            if (arg_ret == XF_OPTION_ERR_NO_FOUND && it->require) {
                print_option_error(it, "required option is missing", XF_SHELL_PARSER_HELP_ENABLE, &out);
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

    if (positional_count > 0 && cmd->_arg_count == 0U) {
        print_unknown_arg_error(cmd, positional_values[0], XF_SHELL_PARSER_HELP_ENABLE, &out);
        return XF_CMD_NO_INVALID_ARG;
    }

    for (i = 0; i < cmd->_arg_count; ++i) {
        cmd_arg_t* it = cmd->_args[i];
        bool should_append_help = true;
        const char* user_error = NULL;
        int parse_ret = XF_OPTION_OK;

        if (it == NULL) {
            continue;
        }

        xf_shell_parser_sync_arg_runtime(it);

        if (positional_index < positional_count) {
            parse_ret = xf_opt_parse_value(&it->_opt, positional_values[positional_index]);
            if (parse_ret != XF_OPTION_OK) {
                print_arg_error(it, NULL, XF_SHELL_PARSER_HELP_ENABLE, &out);
                return XF_CMD_NO_INVALID_ARG;
            }
            it->_opt.provided = 1U;
            positional_index++;
        } else if (it->require) {
            print_arg_error(it, "required argument is missing", XF_SHELL_PARSER_HELP_ENABLE, &out);
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
        print_unknown_arg_error(cmd, positional_values[positional_index], XF_SHELL_PARSER_HELP_ENABLE, &out);
        return XF_CMD_NO_INVALID_ARG;
    }

    return cmd->func((const xf_cmd_args_t*)cmd);
}

int xf_shell_parser_get_int(const xf_cmd_args_t* cmd, const char* long_opt, int32_t* value) {
    return parser_get_value(cmd, long_opt, value, XF_OPTION_TYPE_INT);
}

int xf_shell_parser_get_bool(const xf_cmd_args_t* cmd, const char* long_opt, bool* value) {
    return parser_get_value(cmd, long_opt, value, XF_OPTION_TYPE_BOOL);
}

int xf_shell_parser_get_float(const xf_cmd_args_t* cmd, const char* long_opt, float* value) {
    return parser_get_value(cmd, long_opt, value, XF_OPTION_TYPE_FLOAT);
}

int xf_shell_parser_get_string(const xf_cmd_args_t* cmd, const char* long_opt, const char** value) {
    return parser_get_value(cmd, long_opt, value, XF_OPTION_TYPE_STRING);
}

/* ==================== [Static Functions] ================================== */

static cmd_opt_t* find_opt_by_name(cmd_item_t* cmd, const char* name) {
    uint16_t i;

    if (cmd == NULL || name == NULL) {
        return NULL;
    }

    for (i = 0; i < cmd->_opt_count; ++i) {
        cmd_opt_t* it = cmd->_opts[i];
        if (it == NULL) {
            continue;
        }
        if (it->long_opt != NULL && strcmp(name, it->long_opt) == 0) {
            return it;
        }
    }

    return NULL;
}

static cmd_opt_t* find_opt_by_short(cmd_item_t* cmd, char short_opt) {
    uint16_t i;

    if (cmd == NULL || short_opt == '\0') {
        return NULL;
    }

    for (i = 0; i < cmd->_opt_count; ++i) {
        cmd_opt_t* it = cmd->_opts[i];
        if (it == NULL) {
            continue;
        }
        if (it->short_opt == short_opt) {
            return it;
        }
    }

    return NULL;
}

static cmd_opt_t* find_opt_by_long_name(cmd_item_t* cmd, const char* name, size_t len) {
    uint16_t i;

    if (cmd == NULL || name == NULL || len == 0U) {
        return NULL;
    }

    for (i = 0; i < cmd->_opt_count; ++i) {
        cmd_opt_t* it = cmd->_opts[i];
        if (it == NULL) {
            continue;
        }
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
    uint16_t i;

    if (cmd == NULL || name == NULL) {
        return NULL;
    }

    for (i = 0; i < cmd->_arg_count; ++i) {
        cmd_arg_t* it = cmd->_args[i];
        if (it == NULL) {
            continue;
        }
        if (it->name != NULL && strcmp(it->name, name) == 0) {
            return it;
        }
    }

    return NULL;
}

static xf_options_t* find_runtime_value_by_name(cmd_item_t* cmd, const char* name) {
    cmd_opt_t* opt;
    cmd_arg_t* arg;

    if (cmd == NULL || name == NULL) {
        return NULL;
    }

    opt = find_opt_by_name(cmd, name);
    if (opt != NULL) {
        return &opt->_opt;
    }

    arg = find_arg_by_name(cmd, name);
    if (arg != NULL) {
        return &arg->_opt;
    }

    return NULL;
}

static XF_SHELL_PARSER_NOINLINE void sync_runtime_value(xf_options_t* runtime,
                                                        xf_opt_type_t type,
                                                        bool require,
                                                        bool has_default,
                                                        int32_t default_integer,
                                                        bool default_boolean,
                                                        float default_floating,
                                                        const char* default_string) {
    if (runtime == NULL) {
        return;
    }

    runtime->require = require ? 1U : 0U;
    runtime->has_default = has_default ? 1U : 0U;
    runtime->provided = 0U;
    runtime->type = type;

    if (!has_default) {
        default_integer = 0;
        default_boolean = false;
        default_floating = 0.0f;
        default_string = NULL;
    }

    switch (type) {
        case XF_OPTION_TYPE_BOOL:
            runtime->boolean = default_boolean;
            break;
        case XF_OPTION_TYPE_INT:
            runtime->integer = default_integer;
            break;
        case XF_OPTION_TYPE_STRING:
            runtime->string = default_string;
            break;
        case XF_OPTION_TYPE_FLOAT:
            runtime->floating = default_floating;
            break;
        default:
            runtime->integer = 0;
            break;
    }
}

static XF_SHELL_PARSER_NOINLINE int parser_get_value(const xf_cmd_args_t* cmd,
                                                     const char* long_opt,
                                                     void* value,
                                                     xf_opt_type_t type) {
    xf_options_t* runtime;
    cmd_item_t* item = (cmd_item_t*)cmd;

    if (item == NULL || value == NULL || long_opt == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    runtime = find_runtime_value_by_name(item, long_opt);
    if (runtime == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    switch (type) {
        case XF_OPTION_TYPE_BOOL:
            *(bool*)value = runtime->boolean;
            break;
        case XF_OPTION_TYPE_INT:
            *(int32_t*)value = runtime->integer;
            break;
        case XF_OPTION_TYPE_STRING:
            *(const char**)value = runtime->string;
            break;
        case XF_OPTION_TYPE_FLOAT:
            *(float*)value = runtime->floating;
            break;
        default:
            return XF_CMD_NO_INVALID_ARG;
    }

    return XF_CMD_OK;
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
#if XF_SHELL_PARSER_HELP_ENABLE
    if (arg == NULL) {
        return false;
    }
    return (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0);
#else
    (void)arg;
    return false;
#endif
}

#if XF_SHELL_PARSER_HELP_ENABLE
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
#endif

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
        if (is_help_token(arg)) {
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

static void out_puts(const parser_output_t* out, const char* s) {
    if (out == NULL || out->puts_fn == NULL || s == NULL) {
        return;
    }
    out->puts_fn(out->puts_ctx, s);
}

#if XF_SHELL_PARSER_HELP_ENABLE
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
    uint16_t i;

    if (cmd == NULL) {
        return;
    }

    for (i = 0; i < cmd->_arg_count; ++i) {
        cmd_arg_t* arg = cmd->_args[i];
        if (arg == NULL) {
            continue;
        }
        print_arg_usage(arg, out);
    }
}

static void print_command_options_usage(cmd_item_t* cmd, const parser_output_t* out) {
    uint16_t i;

    if (cmd == NULL) {
        return;
    }

    for (i = 0; i < cmd->_opt_count; ++i) {
        cmd_opt_t* opt = cmd->_opts[i];
        if (opt == NULL) {
            continue;
        }
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
#else
static void print_option_error(cmd_opt_t* opt, const char* message, bool append_help, const parser_output_t* out) {
    char buffer[256];
    const char* name = (opt != NULL && opt->long_opt != NULL && opt->long_opt[0] != '\0') ? opt->long_opt : "(unknown)";
    const char* msg = message;

    (void)append_help;
    if (msg == NULL || msg[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "invalid option: --%s" XF_SHELL_NEWLINE, name);
    } else {
        snprintf(buffer, sizeof(buffer), "invalid option --%s: %s" XF_SHELL_NEWLINE, name, msg);
    }
    out_puts(out, buffer);
}

static void print_arg_error(cmd_arg_t* arg, const char* message, bool append_help, const parser_output_t* out) {
    char buffer[256];
    const char* name = (arg != NULL && arg->name != NULL && arg->name[0] != '\0') ? arg->name : "(unknown)";
    const char* msg = message;

    (void)append_help;
    if (msg == NULL || msg[0] == '\0') {
        snprintf(buffer, sizeof(buffer), "invalid argument: <%s>" XF_SHELL_NEWLINE, name);
    } else {
        snprintf(buffer, sizeof(buffer), "invalid argument <%s>: %s" XF_SHELL_NEWLINE, name, msg);
    }
    out_puts(out, buffer);
}
#endif

static void print_unknown_option_error(cmd_item_t* cmd, const char* token, bool append_help, const parser_output_t* out) {
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "unknown option: %s" XF_SHELL_NEWLINE, token ? token : "(null)");
    out_puts(out, buffer);

#if XF_SHELL_PARSER_HELP_ENABLE
    if (append_help) {
        print_command_help(cmd, out);
    }
#else
    (void)cmd;
    (void)append_help;
#endif
}

static void print_unknown_arg_error(cmd_item_t* cmd, const char* token, bool append_help, const parser_output_t* out) {
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "unexpected argument: %s" XF_SHELL_NEWLINE, token ? token : "(null)");
    out_puts(out, buffer);

#if XF_SHELL_PARSER_HELP_ENABLE
    if (append_help) {
        print_command_help(cmd, out);
    }
#else
    (void)cmd;
    (void)append_help;
#endif
}
