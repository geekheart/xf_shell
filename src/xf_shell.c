/**
 * @file xf_shell.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-06-17
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "xf_shell.h"
#include "xf_shell_cmd_list.h"
#include "xf_shell_cli.h"
#include "xf_shell_completion.h"
#include "xf_shell_parser.h"
#include "xf_shell_options.h"
#include <string.h>
#include <stdio.h>

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef xf_shell_cmd_t cmd_item_t;
typedef xf_opt_arg_t cmd_opt_t;
typedef xf_arg_t cmd_arg_t;

/* ==================== [Static Prototypes] ================================= */

static void xf_shell_register_help_cmd(void);
static void xf_shell_register_history_cmd(void);
static void ensure_cmd_links_inited(cmd_item_t *cmd);
static void ensure_opt_links_inited(cmd_opt_t *opt);
static void ensure_arg_links_inited(cmd_arg_t *arg);
static bool is_list_node_detached(const xf_cmd_list_t *node);
static cmd_item_t *find_command_by_name(const char *name);
static cmd_opt_t *find_opt_by_name(cmd_item_t *cmd, const char *name);
static cmd_arg_t *find_arg_by_name(cmd_item_t *cmd, const char *name);
static void detach_command_opts(cmd_item_t *cmd);
static void detach_command_args(cmd_item_t *cmd);
static int help_command(const xf_cmd_args_t *cmd);
static void cli_puts_adapter(void *ctx, const char *s);
static void cli_putchar(struct xf_cli *cli, char ch, bool is_last);
static void cli_puts(struct xf_cli *cli, const char *s);
static int history_command(const xf_cmd_args_t *cmd);

/* ==================== [Static Variables] ================================== */

static xf_cmd_list_t s_cmd_list = {&s_cmd_list, &s_cmd_list};
static struct xf_cli s_cli;
static char s_matches[XF_SHELL_MAX_MATCHES][XF_CLI_MAX_LINE];

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

void xf_shell_cmd_init(const char *prompt, xf_putc_t putc, void *user_data)
{
    xf_cli_init(&s_cli, prompt, putc, user_data);
    xf_shell_register_help_cmd();
    xf_shell_register_history_cmd();
    xf_cli_prompt(&s_cli);
}

void xf_shell_cmd_handle(xf_getc_t getc)
{
    int cli_argc;
    char **cli_argv;
    char ch = getc();

    if (ch == '\t' && xf_shell_completion_handle_tab(&s_cli, &s_cmd_list,
            s_matches, XF_SHELL_MAX_MATCHES)) {
        return;
    }

    if (xf_cli_insert_char(&s_cli, ch)) {
        cli_argc = xf_cli_argc(&s_cli, &cli_argv);
        if (xf_shell_cmd_run(cli_argc, (const char **)cli_argv)
                == XF_CMD_NOT_SUPPORTED && cli_argv[0] != NULL) {
            cli_puts(&s_cli, "command not found: ");
            cli_puts(&s_cli, cli_argv[0]);
            cli_puts(&s_cli, "\n");
        }

        xf_cli_prompt(&s_cli);
    }
}

int xf_shell_cmd_register(xf_shell_cmd_t *cmd)
{
    if (cmd == NULL || cmd->command == NULL || cmd->func == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }
    if (strchr(cmd->command, ' ') != NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    ensure_cmd_links_inited(cmd);

    if (!is_list_node_detached(&cmd->_node)) {
        return XF_CMD_INITED;
    }

    if (find_command_by_name(cmd->command) != NULL) {
        return XF_CMD_INITED;
    }

    xf_cmd_list_attach(&s_cmd_list, &cmd->_node);
    return XF_CMD_OK;
}

int xf_shell_cmd_unregister(xf_shell_cmd_t *cmd)
{
    if (cmd == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    ensure_cmd_links_inited(cmd);

    if (is_list_node_detached(&cmd->_node)) {
        return XF_CMD_NOT_SUPPORTED;
    }

    detach_command_opts(cmd);
    detach_command_args(cmd);
    xf_cmd_list_detach(&cmd->_node);

    return XF_CMD_OK;
}

int xf_shell_cmd_set_opt(xf_shell_cmd_t *cmd, xf_opt_arg_t *arg)
{
    if (cmd == NULL || arg == NULL || arg->long_opt == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }
    if (arg->require && arg->has_default) {
        return XF_CMD_NO_INVALID_ARG;
    }

    ensure_cmd_links_inited(cmd);
    if (is_list_node_detached(&cmd->_node)) {
        return XF_CMD_NOT_SUPPORTED;
    }

    if (find_opt_by_name(cmd, arg->long_opt) != NULL) {
        return XF_CMD_INITED;
    }
    if (find_arg_by_name(cmd, arg->long_opt) != NULL) {
        return XF_CMD_INITED;
    }

    ensure_opt_links_inited(arg);
    if (!is_list_node_detached(&arg->_node)) {
        return XF_CMD_INITED;
    }

    xf_shell_parser_sync_opt_runtime(arg);
    arg->_owner = cmd;

    xf_cmd_list_attach(&cmd->_opt_list, &arg->_node);
    return XF_CMD_OK;
}

int xf_shell_cmd_unset_opt(xf_shell_cmd_t *cmd, xf_opt_arg_t *arg)
{
    if (cmd == NULL || arg == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    ensure_cmd_links_inited(cmd);
    ensure_opt_links_inited(arg);

    if (is_list_node_detached(&arg->_node)) {
        return XF_CMD_NOT_SUPPORTED;
    }

    if (arg->_owner != cmd) {
        return XF_CMD_NOT_SUPPORTED;
    }

    xf_cmd_list_detach(&arg->_node);
    arg->_owner = NULL;
    return XF_CMD_OK;
}

int xf_shell_cmd_set_arg(xf_shell_cmd_t *cmd, xf_arg_t *arg)
{
    if (cmd == NULL || arg == NULL || arg->name == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }
    if (arg->require && arg->has_default) {
        return XF_CMD_NO_INVALID_ARG;
    }

    ensure_cmd_links_inited(cmd);
    if (is_list_node_detached(&cmd->_node)) {
        return XF_CMD_NOT_SUPPORTED;
    }

    if (find_arg_by_name(cmd, arg->name) != NULL) {
        return XF_CMD_INITED;
    }
    if (find_opt_by_name(cmd, arg->name) != NULL) {
        return XF_CMD_INITED;
    }

    ensure_arg_links_inited(arg);
    if (!is_list_node_detached(&arg->_node)) {
        return XF_CMD_INITED;
    }

    xf_shell_parser_sync_arg_runtime(arg);
    arg->_owner = cmd;

    xf_cmd_list_attach(&cmd->_arg_list, &arg->_node);
    return XF_CMD_OK;
}

int xf_shell_cmd_unset_arg(xf_shell_cmd_t *cmd, xf_arg_t *arg)
{
    if (cmd == NULL || arg == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    ensure_cmd_links_inited(cmd);
    ensure_arg_links_inited(arg);

    if (is_list_node_detached(&arg->_node)) {
        return XF_CMD_NOT_SUPPORTED;
    }

    if (arg->_owner != cmd) {
        return XF_CMD_NOT_SUPPORTED;
    }

    xf_cmd_list_detach(&arg->_node);
    arg->_owner = NULL;
    return XF_CMD_OK;
}

int xf_shell_cmd_run(int argc, const char **argv)
{
    cmd_item_t *item;

    if (argc <= 0 || argv == NULL || argv[0] == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    item = find_command_by_name(argv[0]);
    if (item == NULL) {
        return XF_CMD_NOT_SUPPORTED;
    }

    return xf_shell_parser_run(item, argc, argv, cli_puts_adapter, &s_cli);
}

int xf_shell_cmd_get_int(const xf_cmd_args_t *cmd, const char *long_opt,
                           int32_t *value)
{
    return xf_shell_parser_get_int(cmd, long_opt, value);
}

int xf_shell_cmd_get_bool(const xf_cmd_args_t *cmd, const char *long_opt,
                            bool *value)
{
    return xf_shell_parser_get_bool(cmd, long_opt, value);
}

int xf_shell_cmd_get_float(const xf_cmd_args_t *cmd, const char *long_opt,
                             float *value)
{
    return xf_shell_parser_get_float(cmd, long_opt, value);
}

int xf_shell_cmd_get_string(const xf_cmd_args_t *cmd,
                              const char *long_opt, const char **value)
{
    return xf_shell_parser_get_string(cmd, long_opt, value);
}

/* ==================== [Static Functions] ================================== */

static void xf_shell_register_help_cmd(void)
{
    static xf_shell_cmd_t s_help_cmd = {
        .command = "help",
        .help = "Print the list of registered commands",
        .func = help_command,
    };

    (void)xf_shell_cmd_register(&s_help_cmd);
}

static void xf_shell_register_history_cmd(void)
{
    static xf_shell_cmd_t s_history_cmd = {
        .command = "history",
        .help = "Print command history",
        .func = history_command,
    };

    (void)xf_shell_cmd_register(&s_history_cmd);
}

static void ensure_cmd_links_inited(cmd_item_t *cmd)
{
    if (cmd == NULL) {
        return;
    }

    if (cmd->_node.next == NULL || cmd->_node.prev == NULL) {
        xf_cmd_list_init(&cmd->_node);
    }

    if (cmd->_opt_list.next == NULL || cmd->_opt_list.prev == NULL) {
        xf_cmd_list_init(&cmd->_opt_list);
    }

    if (cmd->_arg_list.next == NULL || cmd->_arg_list.prev == NULL) {
        xf_cmd_list_init(&cmd->_arg_list);
    }
}

static void ensure_opt_links_inited(cmd_opt_t *opt)
{
    if (opt == NULL) {
        return;
    }

    if (opt->_node.next == NULL || opt->_node.prev == NULL) {
        xf_cmd_list_init(&opt->_node);
    }
}

static void ensure_arg_links_inited(cmd_arg_t *arg)
{
    if (arg == NULL) {
        return;
    }

    if (arg->_node.next == NULL || arg->_node.prev == NULL) {
        xf_cmd_list_init(&arg->_node);
    }
}

static bool is_list_node_detached(const xf_cmd_list_t *node)
{
    return (node != NULL && node->next == node && node->prev == node);
}

static cmd_item_t *find_command_by_name(const char *name)
{
    xf_cmd_list_t *next;

    if (name == NULL) {
        return NULL;
    }

    for (next = xf_cmd_list_get_next(&s_cmd_list); next != &s_cmd_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_item_t *it = GET_PARENT_ADDR(next, cmd_item_t, _node);
        if (strcmp(name, it->command) == 0) {
            return it;
        }
    }
    return NULL;
}

static cmd_opt_t *find_opt_by_name(cmd_item_t *cmd, const char *name)
{
    xf_cmd_list_t *next;

    if (cmd == NULL || name == NULL) {
        return NULL;
    }

    for (next = xf_cmd_list_get_next(&cmd->_opt_list); next != &cmd->_opt_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_opt_t *it = GET_PARENT_ADDR(next, cmd_opt_t, _node);
        if (it->long_opt != NULL && strcmp(name, it->long_opt) == 0) {
            return it;
        }
    }

    return NULL;
}

static cmd_arg_t *find_arg_by_name(cmd_item_t *cmd, const char *name)
{
    xf_cmd_list_t *next;

    if (cmd == NULL || name == NULL) {
        return NULL;
    }

    for (next = xf_cmd_list_get_next(&cmd->_arg_list); next != &cmd->_arg_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_arg_t *it = GET_PARENT_ADDR(next, cmd_arg_t, _node);
        if (it->name != NULL && strcmp(it->name, name) == 0) {
            return it;
        }
    }

    return NULL;
}

static void detach_command_opts(cmd_item_t *cmd)
{
    xf_cmd_list_t *next;

    if (cmd == NULL) {
        return;
    }

    next = xf_cmd_list_get_next(&cmd->_opt_list);
    while (next != &cmd->_opt_list) {
        cmd_opt_t *opt = GET_PARENT_ADDR(next, cmd_opt_t, _node);
        next = xf_cmd_list_get_next(next);
        xf_cmd_list_detach(&opt->_node);
        opt->_owner = NULL;
    }
}

static void detach_command_args(cmd_item_t *cmd)
{
    xf_cmd_list_t *next;

    if (cmd == NULL) {
        return;
    }

    next = xf_cmd_list_get_next(&cmd->_arg_list);
    while (next != &cmd->_arg_list) {
        cmd_arg_t *arg = GET_PARENT_ADDR(next, cmd_arg_t, _node);
        next = xf_cmd_list_get_next(next);
        xf_cmd_list_detach(&arg->_node);
        arg->_owner = NULL;
    }
}

static int help_command(const xf_cmd_args_t *cmd)
{
    xf_cmd_list_t *next;

    (void)cmd;
    cli_puts(&s_cli, ">>>>>>>>>>> help <<<<<<<<<<<<\n\r");
    for (next = xf_cmd_list_get_next(&s_cmd_list); next != &s_cmd_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_item_t *it = GET_PARENT_ADDR(next, cmd_item_t, _node);
        if (it->help == NULL) {
            continue;
        }
        cli_puts(&s_cli, "\t");
        cli_puts(&s_cli, it->command);
        cli_puts(&s_cli, ":\t");
        cli_puts(&s_cli, it->help);
        cli_puts(&s_cli, "\n\r");
    }
    return XF_CMD_OK;
}

static int history_command(const xf_cmd_args_t *cmd)
{
    int i;
    const char *line;
    char buffer[32];

    (void)cmd;
#if XF_CLI_HISTORY_LEN
    for (i = 0;; ++i) {
        line = xf_cli_get_history(&s_cli, i);
        if (line == NULL || line[0] == '\0') {
            break;
        }
        snprintf(buffer, sizeof(buffer), "\t[%d] ", i);
        cli_puts(&s_cli, buffer);
        cli_puts(&s_cli, line);
        cli_puts(&s_cli, "\n");
    }
    if (i == 0) {
        cli_puts(&s_cli, "history is empty\n");
    }
#else
    cli_puts(&s_cli, "history is disabled (XF_CLI_HISTORY_LEN=0)\n");
#endif
    return XF_CMD_OK;
}

static void cli_puts_adapter(void *ctx, const char *s)
{
    if (ctx == NULL || s == NULL) {
        return;
    }
    cli_puts((struct xf_cli *)ctx, s);
}

static void cli_putchar(struct xf_cli *cli, char ch, bool is_last)
{
    if (cli->put_char == NULL) {
        return;
    }
#if XF_CLI_SERIAL_XLATE
    if (ch == '\n') {
        cli->put_char(cli->cb_data, '\r', false);
    }
#endif
    cli->put_char(cli->cb_data, ch, is_last);
}

static void cli_puts(struct xf_cli *cli, const char *s)
{
    for (; *s != '\0'; s++) {
        cli_putchar(cli, *s, s[1] == '\0');
    }
}
