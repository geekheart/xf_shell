/**
 * @file xf_console.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-06-17
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "xf_console.h"
#include "xf_cmd_list.h"
#include "embedded_cli.h"
#include "xf_options.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef struct _cmd_item_t {
    xf_cmd_list_t node;
    xf_console_cmd_t cmd;
    xf_cmd_list_t opt_list;
} cmd_item_t;

typedef struct _cmd_opt_t {
    xf_cmd_list_t node;
    xf_options_t opt;
} cmd_opt_t;



/* ==================== [Static Prototypes] ================================= */

static cmd_item_t *find_command_by_name(const char *name);
static cmd_opt_t *find_opt_by_name(cmd_item_t *cmd, const char *name);
static int help_command(const xf_cmd_args_t *cmd);
static void cli_puts(struct embedded_cli *cli, const char *s);

/* ==================== [Static Variables] ================================== */

static xf_cmd_list_t s_cmd_list = {&s_cmd_list, &s_cmd_list};
static struct embedded_cli s_cli;
static char help_msg[1024] = {0};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

void xf_console_cmd_init(const char *prompt, xf_putc_t putc, void *user_data)
{
    embedded_cli_init(&s_cli, prompt, putc, user_data);
    embedded_cli_prompt(&s_cli);
}

void xf_console_cmd_handle(xf_getc_t getc)
{
    int cli_argc;
    char **cli_argv;
    if (embedded_cli_insert_char(&s_cli, getc())) {
        cli_argc = embedded_cli_argc(&s_cli, &cli_argv);
        if (xf_console_cmd_run(cli_argc, (const char **)cli_argv)
                == XF_CMD_NOT_SUPPORTED && cli_argv[0] != NULL) {
            cli_puts(&s_cli, "command not found: ");
            cli_puts(&s_cli, cli_argv[0]);
            cli_puts(&s_cli, "\n");
        }

        embedded_cli_prompt(&s_cli);
    }
}

int xf_console_cmd_register(const xf_console_cmd_t *cmd)
{
    cmd_item_t *item = NULL;
    if (!cmd || cmd->command == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }
    if (strchr(cmd->command, ' ') != NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }
    item = find_command_by_name(cmd->command);
    if (item != NULL) {
        return XF_CMD_INITED;
    }
    // not registered before
    item = (cmd_item_t *)calloc(1, sizeof(cmd_item_t));
    if (item == NULL) {
        return XF_CMD_NO_MEM;
    }

    item->cmd.command = cmd->command;
    item->cmd.help = cmd->help;
    item->cmd.func = cmd->func;
    xf_cmd_list_init(&item->node);
    xf_cmd_list_init(&item->opt_list);
    xf_cmd_list_attach(&s_cmd_list, &item->node);

    return XF_CMD_OK;
}

int xf_console_cmd_unregister(const char *command)
{
    cmd_item_t *item = NULL;
    if (command == NULL || strchr(command, ' ') != NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }
    item = find_command_by_name(command);
    if (item == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    xf_cmd_list_detach(&item->node);
    free(item);

    return XF_CMD_OK;
}

int xf_console_cmd_set_opt(const char *command, xf_opt_arg_t arg)
{
    if (!arg.long_opt || !command) {
        return XF_CMD_NO_INVALID_ARG;
    }

    cmd_item_t *item = find_command_by_name(command);
    if (item == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    cmd_opt_t *opt = find_opt_by_name(item, arg.long_opt);

    if (opt != NULL) {
        return XF_CMD_INITED;
    }

    opt = (cmd_opt_t *)calloc(1, sizeof(cmd_opt_t));

    opt->opt.long_opt       = arg.long_opt;
    opt->opt.description    = arg.description;
    opt->opt.short_opt      = arg.short_opt;
    opt->opt.require        = arg.require;
    opt->opt.type           = arg.type;

    xf_cmd_list_init(&opt->node);

    xf_cmd_list_attach(&item->opt_list, &opt->node);

    return XF_CMD_OK;
}

int xf_console_cmd_unset_opt(const char *command, const char *long_opt)
{
    if (!long_opt || !command) {
        return XF_CMD_NO_INVALID_ARG;
    }

    cmd_item_t *item = find_command_by_name(command);
    if (item == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    cmd_opt_t *opt = find_opt_by_name(item, long_opt);

    if (opt == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    xf_cmd_list_detach(&opt->node);

    free(opt);

    return XF_CMD_OK;
}

int xf_console_cmd_run(int argc, const char **argv)
{
    cmd_item_t *item = find_command_by_name(argv[0]);

    if (item == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    // update options
    xf_cmd_list_t *next;
    for (next = xf_cmd_list_get_next(&item->opt_list); next != &item->opt_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_opt_t *it = GET_PARENT_ADDR(next, cmd_opt_t, node);
        int arg_ret = xf_opt_parse(&it->opt, argc, argv);
        if (arg_ret == XF_OPTION_ERR_HELP) {
            xf_opt_parse_usage(&it->opt, help_msg);
            cli_puts(&s_cli, help_msg);
        }

    }

    return item->cmd.func((const xf_cmd_args_t *)item);
}

int xf_console_cmd_get_int(const xf_cmd_args_t *cmd, const char *long_opt,
                           int32_t *value)
{
    cmd_item_t *item = (cmd_item_t *)cmd;

    cmd_opt_t *opt = find_opt_by_name(item, long_opt);

    if (opt == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    *value = opt->opt.integer;

    return XF_CMD_OK;
}

int xf_console_cmd_get_bool(const xf_cmd_args_t *cmd, const char *long_opt,
                            bool *value)
{
    cmd_item_t *item = (cmd_item_t *)cmd;

    cmd_opt_t *opt = find_opt_by_name(item, long_opt);

    if (opt == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    *value = opt->opt.boolean;

    return XF_CMD_OK;
}

int xf_console_cmd_get_float(const xf_cmd_args_t *cmd, const char *long_opt,
                             float *value)
{
    cmd_item_t *item = (cmd_item_t *)cmd;

    cmd_opt_t *opt = find_opt_by_name(item, long_opt);

    if (opt == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    *value = opt->opt.floating;

    return XF_CMD_OK;
}

int xf_console_cmd_get_string(const xf_cmd_args_t *cmd,
                              const char *long_opt, const char **value)
{
    cmd_item_t *item = (cmd_item_t *)cmd;

    cmd_opt_t *opt = find_opt_by_name(item, long_opt);

    if (opt == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    *value = opt->opt.string;

    return XF_CMD_OK;
}

void xf_console_register_help_cmd(void)
{
    xf_console_cmd_t cmd;
    cmd.command = "help";
    cmd.func = help_command;
    cmd.help = "Print the list of registered commands";

    xf_console_cmd_register(&cmd);
}

/* ==================== [Static Functions] ================================== */

static cmd_item_t *find_command_by_name(const char *name)
{
    if (name == NULL) {
        return NULL;
    }

    xf_cmd_list_t *next;
    for (next = xf_cmd_list_get_next(&s_cmd_list); next != &s_cmd_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_item_t *it = GET_PARENT_ADDR(next, cmd_item_t, node);
        if (strcmp(name, it->cmd.command) == 0) {
            return it;
        }
    }
    return NULL;
}

static cmd_opt_t *find_opt_by_name(cmd_item_t *cmd, const char *name)
{
    if (name == NULL) {
        return NULL;
    }

    xf_cmd_list_t *next;
    for (next = xf_cmd_list_get_next(&cmd->opt_list); next != &cmd->opt_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_opt_t *it = GET_PARENT_ADDR(next, cmd_opt_t, node);
        if (strcmp(name, it->opt.long_opt) == 0) {
            return it;
        }
    }
    return NULL;
}

static int help_command(const xf_cmd_args_t *cmd)
{
    cli_puts(&s_cli, "===========help============\n");
    xf_cmd_list_t *next;
    for (next = xf_cmd_list_get_next(&s_cmd_list); next != &s_cmd_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_item_t *it = GET_PARENT_ADDR(next, cmd_item_t, node);
        if (it->cmd.help == NULL) {
            continue;
        }
        cli_puts(&s_cli, "\t");
        cli_puts(&s_cli, it->cmd.command);
        cli_puts(&s_cli, ":\t");
        cli_puts(&s_cli, it->cmd.help);
        cli_puts(&s_cli, "\n");
    }
}

static void cli_puts(struct embedded_cli *cli, const char *s)
{
    for (; *s; s++) {
#if EMBEDDED_CLI_SERIAL_XLATE
        if (*s == '\n') {
            cli->put_char(cli->cb_data, '\r', false);
        }
#endif
        cli->put_char(cli->cb_data, *s, s[1] == '\0');
    }
}
