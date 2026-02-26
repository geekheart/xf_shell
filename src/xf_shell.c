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
#include "xf_shell_cli.h"
#include "xf_shell_completion.h"
#include "xf_shell_parser.h"
#include <stdio.h>
#include <string.h>

/* ==================== [Typedefs] ========================================== */

typedef xf_shell_cmd_t cmd_item_t;
typedef xf_opt_arg_t cmd_opt_t;
typedef xf_arg_t cmd_arg_t;

/* ==================== [Static Prototypes] ================================= */

static void xf_shell_register_help_cmd(void);
#if XF_CLI_HISTORY_LEN
static void xf_shell_register_history_cmd(void);
#endif
static int append_command_to_registry(cmd_item_t *cmd);
static bool is_valid_command_descriptor(const cmd_item_t *cmd);
static int find_command_index_by_name(const char *name);
static cmd_item_t *find_command_by_name(const char *name);
static int import_static_command_table(void);
static bool is_whitespace_char(char ch);
static bool are_completion_candidates_valid(const char *const *candidates, uint16_t count);
static int help_command(const xf_cmd_args_t *cmd);
static void cli_puts_adapter(void *ctx, const char *s);
static void cli_putchar(struct xf_cli *cli, char ch, bool is_last);
static void cli_puts(struct xf_cli *cli, const char *s);
#if XF_CLI_HISTORY_LEN
static int history_command(const xf_cmd_args_t *cmd);
#endif

/* ==================== [Static Variables] ================================== */

static cmd_item_t *s_cmd_table[XF_SHELL_MAX_COMMANDS];
static uint16_t s_cmd_count = 0;
static struct xf_cli s_cli;
static cmd_item_t *const *s_user_cmd_table = NULL;
static uint16_t s_user_cmd_count = 0;
#if XF_SHELL_COMPLETION_ENABLE
static char s_matches[XF_SHELL_MAX_MATCHES][XF_CLI_MAX_LINE];
#endif

/* ==================== [Global Functions] ================================== */

void xf_shell_cmd_init(const char *prompt, xf_putc_t putc, void *user_data)
{
    xf_cli_init(&s_cli, prompt, putc, user_data);

    s_cmd_count = 0;
    xf_shell_register_help_cmd();
#if XF_CLI_HISTORY_LEN
    xf_shell_register_history_cmd();
#endif
    (void)import_static_command_table();

    xf_cli_prompt(&s_cli);
}

void xf_shell_cmd_handle(xf_getc_t getc)
{
    int cli_argc;
    char **cli_argv;
    char ch = getc();

    if (ch == '\t') {
#if XF_SHELL_COMPLETION_ENABLE
        if (xf_shell_completion_handle_tab(&s_cli, s_cmd_table, (int)s_cmd_count,
                                           s_matches, XF_SHELL_MAX_MATCHES)) {
            return;
        }
#else
        return;
#endif
    }

    if (xf_cli_insert_char(&s_cli, ch)) {
        cli_argc = xf_cli_argc(&s_cli, &cli_argv);
        if (xf_shell_cmd_run(cli_argc, (const char **)cli_argv)
            == XF_CMD_NOT_SUPPORTED && cli_argv[0] != NULL) {
            cli_puts(&s_cli, "command not found: ");
            cli_puts(&s_cli, cli_argv[0]);
            cli_puts(&s_cli, XF_SHELL_NEWLINE);
        }

        xf_cli_prompt(&s_cli);
    }
}

int xf_shell_cmd_set_table(xf_shell_cmd_t *const *cmd_table, uint16_t cmd_count)
{
    if (cmd_count > 0U && cmd_table == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    s_user_cmd_table = cmd_table;
    s_user_cmd_count = cmd_count;
    return XF_CMD_OK;
}


int xf_shell_cmd_set_opt_candidates(xf_opt_arg_t *opt,
                                    const char *const *candidates,
                                    uint16_t count)
{
    if (opt == NULL || !are_completion_candidates_valid(candidates, count)) {
        return XF_CMD_NO_INVALID_ARG;
    }

    opt->completion.items = candidates;
    opt->completion.count = count;
    return XF_CMD_OK;
}

int xf_shell_cmd_set_arg_candidates(xf_arg_t *arg,
                                    const char *const *candidates,
                                    uint16_t count)
{
    if (arg == NULL || !are_completion_candidates_valid(candidates, count)) {
        return XF_CMD_NO_INVALID_ARG;
    }

    arg->completion.items = candidates;
    arg->completion.count = count;
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

    (void)append_command_to_registry(&s_help_cmd);
}

#if XF_CLI_HISTORY_LEN
static void xf_shell_register_history_cmd(void)
{
    static xf_shell_cmd_t s_history_cmd = {
        .command = "history",
        .help = "Print command history",
        .func = history_command,
    };

    (void)append_command_to_registry(&s_history_cmd);
}
#endif

static int import_static_command_table(void)
{
    uint16_t i;

    if (s_user_cmd_count == 0U) {
        return XF_CMD_OK;
    }

    if (s_user_cmd_table == NULL) {
        return XF_CMD_NO_INVALID_ARG;
    }

    for (i = 0; i < s_user_cmd_count; ++i) {
        int ret = append_command_to_registry(s_user_cmd_table[i]);
        if (ret != XF_CMD_OK && ret != XF_CMD_INITED) {
            return ret;
        }
    }

    return XF_CMD_OK;
}

static int append_command_to_registry(cmd_item_t *cmd)
{
    if (!is_valid_command_descriptor(cmd)) {
        return XF_CMD_NO_INVALID_ARG;
    }

    if (find_command_by_name(cmd->command) != NULL) {
        return XF_CMD_INITED;
    }

    if (s_cmd_count >= XF_SHELL_MAX_COMMANDS) {
        return XF_CMD_NO_MEM;
    }

    s_cmd_table[s_cmd_count++] = cmd;
    return XF_CMD_OK;
}

static bool is_valid_command_descriptor(const cmd_item_t *cmd)
{
    if (cmd == NULL || cmd->command == NULL || cmd->func == NULL) {
        return false;
    }
    if (strchr(cmd->command, ' ') != NULL) {
        return false;
    }
    if (cmd->_opt_count > 0U && cmd->_opts == NULL) {
        return false;
    }
    if (cmd->_arg_count > 0U && cmd->_args == NULL) {
        return false;
    }
    return true;
}

static int find_command_index_by_name(const char *name)
{
    uint16_t i;

    if (name == NULL) {
        return -1;
    }

    for (i = 0; i < s_cmd_count; ++i) {
        cmd_item_t *it = s_cmd_table[i];
        if (it != NULL && strcmp(name, it->command) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static cmd_item_t *find_command_by_name(const char *name)
{
    int index = find_command_index_by_name(name);
    if (index < 0) {
        return NULL;
    }

    return s_cmd_table[index];
}

static bool is_whitespace_char(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}

static bool are_completion_candidates_valid(const char *const *candidates, uint16_t count)
{
    uint16_t i;

    if (count == 0U) {
        return true;
    }
    if (candidates == NULL) {
        return false;
    }

    for (i = 0; i < count; ++i) {
        const char *it = candidates[i];
        if (it == NULL || it[0] == '\0') {
            return false;
        }
        for (; *it != '\0'; ++it) {
            if (is_whitespace_char(*it)) {
                return false;
            }
        }
    }

    return true;
}

static int help_command(const xf_cmd_args_t *cmd)
{
    uint16_t i;

    (void)cmd;
    cli_puts(&s_cli, ">>>>>>>>>>> help <<<<<<<<<<<<" XF_SHELL_NEWLINE);

    for (i = 0; i < s_cmd_count; ++i) {
        cmd_item_t *it = s_cmd_table[i];
        if (it == NULL || it->help == NULL) {
            continue;
        }

        cli_puts(&s_cli, "\t");
        cli_puts(&s_cli, it->command);
        cli_puts(&s_cli, ":\t");
        cli_puts(&s_cli, it->help);
        cli_puts(&s_cli, XF_SHELL_NEWLINE);
    }

    return XF_CMD_OK;
}

#if XF_CLI_HISTORY_LEN
static int history_command(const xf_cmd_args_t *cmd)
{
    int i;
    const char *line;
    char buffer[32];

    (void)cmd;
    for (i = 0;; ++i) {
        line = xf_cli_get_history(&s_cli, i);
        if (line == NULL || line[0] == '\0') {
            break;
        }
        snprintf(buffer, sizeof(buffer), "\t[%d] ", i);
        cli_puts(&s_cli, buffer);
        cli_puts(&s_cli, line);
        cli_puts(&s_cli, XF_SHELL_NEWLINE);
    }
    if (i == 0) {
        cli_puts(&s_cli, "history is empty" XF_SHELL_NEWLINE);
    }
    return XF_CMD_OK;
}
#endif

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
    if (ch == '\n' && !XF_SHELL_NEWLINE_IS_CRLF) {
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
