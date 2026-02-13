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
#include "xf_cli.h"
#include "xf_options.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ==================== [Defines] =========================================== */

#define CLEAR_EOL "\x1b[0K"
#define MOVE_BOL  "\x1b[1G"

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
static void free_command_opts(cmd_item_t *cmd);
static bool xf_console_handle_tab(struct xf_cli *cli);
static int help_command(const xf_cmd_args_t *cmd);
static bool is_whitespace(char ch);
static bool starts_with(const char *str, const char *prefix);
static int count_registered_commands(void);
static int count_command_option_candidates(cmd_item_t *cmd);
static bool add_match(char matches[][XF_CLI_MAX_LINE], int *match_count,
                      int max_matches, const char *value);
static int collect_command_matches(const char *prefix,
                                   char matches[][XF_CLI_MAX_LINE],
                                   int max_matches);
static int collect_option_matches(cmd_item_t *cmd,
                                  const char *prefix,
                                  char matches[][XF_CLI_MAX_LINE],
                                  int max_matches);
static cmd_item_t *resolve_current_command(const struct xf_cli *cli);
static int common_prefix_len(char matches[][XF_CLI_MAX_LINE],
                             int match_count);
static bool replace_prefix(struct xf_cli *cli, int start, int cursor,
                           const char *replacement);
static bool insert_space_if_needed(struct xf_cli *cli);
static void cli_putchar(struct xf_cli *cli, char ch, bool is_last);
static void cli_puts(struct xf_cli *cli, const char *s);
static void cli_ansi(struct xf_cli *cli, int n, char code);
static void term_cursor_back(struct xf_cli *cli, int n);
static void redraw_line(struct xf_cli *cli);
static void print_suggestions(struct xf_cli *cli,
                              char matches[][XF_CLI_MAX_LINE],
                              int match_count);

/* ==================== [Static Variables] ================================== */

static xf_cmd_list_t s_cmd_list = {&s_cmd_list, &s_cmd_list};
static struct xf_cli s_cli;
static char help_msg[1024] = {0};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

void xf_console_cmd_init(const char *prompt, xf_putc_t putc, void *user_data)
{
    xf_cli_init(&s_cli, prompt, putc, user_data);
    xf_cli_prompt(&s_cli);
}

void xf_console_cmd_handle(xf_getc_t getc)
{
    int cli_argc;
    char **cli_argv;
    char ch = getc();

    if (ch == '\t' && xf_console_handle_tab(&s_cli)) {
        return;
    }

    if (xf_cli_insert_char(&s_cli, ch)) {
        cli_argc = xf_cli_argc(&s_cli, &cli_argv);
        if (xf_console_cmd_run(cli_argc, (const char **)cli_argv)
                == XF_CMD_NOT_SUPPORTED && cli_argv[0] != NULL) {
            cli_puts(&s_cli, "command not found: ");
            cli_puts(&s_cli, cli_argv[0]);
            cli_puts(&s_cli, "\n");
        }

        xf_cli_prompt(&s_cli);
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

    free_command_opts(item);
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
    if (opt == NULL) {
        return XF_CMD_NO_MEM;
    }

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

static void free_command_opts(cmd_item_t *cmd)
{
    xf_cmd_list_t *next = xf_cmd_list_get_next(&cmd->opt_list);
    while (next != &cmd->opt_list) {
        cmd_opt_t *opt = GET_PARENT_ADDR(next, cmd_opt_t, node);
        next = xf_cmd_list_get_next(next);
        xf_cmd_list_detach(&opt->node);
        free(opt);
    }
}

static bool xf_console_handle_tab(struct xf_cli *cli)
{
    int token_start;
    int prefix_len;
    bool first_token = true;
    int token_end;
    bool at_token_end;
    char prefix[XF_CLI_MAX_LINE];
    char (*matches)[XF_CLI_MAX_LINE] = NULL;
    int max_matches = 0;
    int match_count = 0;

    if (cli == NULL) {
        return false;
    }

    if (cli->len < 0) {
        cli->len = 0;
    }
    if (cli->cursor < 0) {
        cli->cursor = 0;
    }
    if (cli->cursor > cli->len) {
        cli->cursor = cli->len;
    }
    cli->buffer[cli->len] = '\0';

    token_start = cli->cursor;
    while (token_start > 0 && !is_whitespace(cli->buffer[token_start - 1])) {
        token_start--;
    }

    for (int i = 0; i < token_start; ++i) {
        if (!is_whitespace(cli->buffer[i])) {
            first_token = false;
            break;
        }
    }

    prefix_len = cli->cursor - token_start;
    if (prefix_len < 0) {
        prefix_len = 0;
    }
    if (prefix_len >= (int)sizeof(prefix)) {
        prefix_len = (int)sizeof(prefix) - 1;
    }
    memcpy(prefix, &cli->buffer[token_start], (size_t)prefix_len);
    prefix[prefix_len] = '\0';

    token_end = cli->cursor;
    while (token_end < cli->len && !is_whitespace(cli->buffer[token_end])) {
        token_end++;
    }
    at_token_end = (token_end == cli->cursor);

    if (first_token) {
        max_matches = count_registered_commands();
    } else {
        cmd_item_t *cmd = resolve_current_command(cli);
        if (cmd != NULL && (prefix[0] == '-' || prefix[0] == '\0')) {
            max_matches = count_command_option_candidates(cmd);
        }
    }

    if (max_matches <= 0) {
        cli_putchar(cli, '\a', true);
        return true;
    }

    matches = (char (*)[XF_CLI_MAX_LINE])calloc((size_t)max_matches,
                                                 sizeof(*matches));
    if (matches == NULL) {
        cli_putchar(cli, '\a', true);
        return true;
    }

    if (first_token) {
        match_count = collect_command_matches(prefix, matches, max_matches);
    } else {
        cmd_item_t *cmd = resolve_current_command(cli);
        if (cmd != NULL && (prefix[0] == '-' || prefix[0] == '\0')) {
            match_count = collect_option_matches(cmd, prefix, matches, max_matches);
        }
    }

    if (match_count <= 0) {
        cli_putchar(cli, '\a', true);
        free(matches);
        return true;
    }

    if (match_count == 1) {
        bool replaced = replace_prefix(cli, token_start, cli->cursor, matches[0]);
        if (!replaced) {
            cli_putchar(cli, '\a', true);
            free(matches);
            return true;
        }
        if (at_token_end) {
            (void)insert_space_if_needed(cli);
        }
        redraw_line(cli);
        free(matches);
        return true;
    }

    int lcp_len = common_prefix_len(matches, match_count);
    if (lcp_len > prefix_len) {
        char lcp[XF_CLI_MAX_LINE];
        memcpy(lcp, matches[0], (size_t)lcp_len);
        lcp[lcp_len] = '\0';
        if (!replace_prefix(cli, token_start, cli->cursor, lcp)) {
            cli_putchar(cli, '\a', true);
            free(matches);
            return true;
        }
        redraw_line(cli);
        free(matches);
        return true;
    }

    print_suggestions(cli, matches, match_count);
    redraw_line(cli);
    free(matches);
    return true;
}

static int help_command(const xf_cmd_args_t *cmd)
{
    (void)cmd;
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
    return XF_CMD_OK;
}

static bool is_whitespace(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}

static bool starts_with(const char *str, const char *prefix)
{
    while (*prefix != '\0') {
        if (*str != *prefix) {
            return false;
        }
        str++;
        prefix++;
    }
    return true;
}

static int count_registered_commands(void)
{
    int count = 0;
    xf_cmd_list_t *next;
    for (next = xf_cmd_list_get_next(&s_cmd_list); next != &s_cmd_list;
            next = xf_cmd_list_get_next(next)) {
        count++;
    }
    return count;
}

static int count_command_option_candidates(cmd_item_t *cmd)
{
    int count = 0;
    xf_cmd_list_t *next;

    for (next = xf_cmd_list_get_next(&cmd->opt_list); next != &cmd->opt_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_opt_t *it = GET_PARENT_ADDR(next, cmd_opt_t, node);
        if (it->opt.long_opt != NULL && it->opt.long_opt[0] != '\0') {
            count++;
        }
        if (it->opt.short_opt != '\0') {
            count++;
        }
    }

    return count;
}

static bool add_match(char matches[][XF_CLI_MAX_LINE], int *match_count,
                      int max_matches, const char *value)
{
    int i;
    if (value == NULL || match_count == NULL) {
        return false;
    }

    for (i = 0; i < *match_count; ++i) {
        if (strcmp(matches[i], value) == 0) {
            return false;
        }
    }

    if (*match_count >= max_matches) {
        return false;
    }
    strncpy(matches[*match_count], value, XF_CLI_MAX_LINE);
    matches[*match_count][XF_CLI_MAX_LINE - 1] = '\0';
    (*match_count)++;
    return true;
}

static int collect_command_matches(const char *prefix,
                                   char matches[][XF_CLI_MAX_LINE],
                                   int max_matches)
{
    int match_count = 0;
    xf_cmd_list_t *next;
    for (next = xf_cmd_list_get_next(&s_cmd_list); next != &s_cmd_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_item_t *it = GET_PARENT_ADDR(next, cmd_item_t, node);
        if (starts_with(it->cmd.command, prefix)) {
            (void)add_match(matches, &match_count, max_matches, it->cmd.command);
        }
    }
    return match_count;
}

static int collect_option_matches(cmd_item_t *cmd,
                                  const char *prefix,
                                  char matches[][XF_CLI_MAX_LINE],
                                  int max_matches)
{
    int match_count = 0;
    xf_cmd_list_t *next;
    char candidate[XF_CLI_MAX_LINE];

    if (cmd == NULL) {
        return 0;
    }

    for (next = xf_cmd_list_get_next(&cmd->opt_list); next != &cmd->opt_list;
            next = xf_cmd_list_get_next(next)) {
        cmd_opt_t *it = GET_PARENT_ADDR(next, cmd_opt_t, node);

        if (it->opt.long_opt != NULL && it->opt.long_opt[0] != '\0') {
            snprintf(candidate, sizeof(candidate), "--%s", it->opt.long_opt);
            if (starts_with(candidate, prefix)) {
                (void)add_match(matches, &match_count, max_matches, candidate);
            }
        }
        if (it->opt.short_opt != '\0') {
            candidate[0] = '-';
            candidate[1] = it->opt.short_opt;
            candidate[2] = '\0';
            if (starts_with(candidate, prefix)) {
                (void)add_match(matches, &match_count, max_matches, candidate);
            }
        }
    }

    return match_count;
}

static cmd_item_t *resolve_current_command(const struct xf_cli *cli)
{
    int start = 0;
    int end = 0;
    int cmd_len;
    char command[XF_CLI_MAX_LINE];

    while (start < cli->len && is_whitespace(cli->buffer[start])) {
        start++;
    }
    end = start;
    while (end < cli->len && !is_whitespace(cli->buffer[end])) {
        end++;
    }

    cmd_len = end - start;
    if (cmd_len <= 0) {
        return NULL;
    }
    if (cmd_len >= (int)sizeof(command)) {
        cmd_len = (int)sizeof(command) - 1;
    }

    memcpy(command, &cli->buffer[start], (size_t)cmd_len);
    command[cmd_len] = '\0';
    return find_command_by_name(command);
}

static int common_prefix_len(char matches[][XF_CLI_MAX_LINE],
                             int match_count)
{
    int common_len;
    int i;

    if (match_count <= 0) {
        return 0;
    }

    common_len = (int)strlen(matches[0]);
    for (i = 1; i < match_count && common_len > 0; ++i) {
        int j = 0;
        while (j < common_len && matches[i][j] != '\0' &&
                matches[i][j] == matches[0][j]) {
            j++;
        }
        common_len = j;
    }
    return common_len;
}

static bool replace_prefix(struct xf_cli *cli, int start, int cursor,
                           const char *replacement)
{
    int old_len = cursor - start;
    int rep_len = (int)strlen(replacement);
    int new_len = cli->len - old_len + rep_len;

    if (start < 0 || cursor < start || cursor > cli->len) {
        return false;
    }
    if (new_len >= (int)sizeof(cli->buffer)) {
        return false;
    }

    memmove(&cli->buffer[start + rep_len], &cli->buffer[cursor],
            (size_t)(cli->len - cursor + 1));
    memcpy(&cli->buffer[start], replacement, (size_t)rep_len);
    cli->len = new_len;
    cli->cursor = start + rep_len;
    cli->buffer[cli->len] = '\0';
    return true;
}

static bool insert_space_if_needed(struct xf_cli *cli)
{
    if (cli->len >= (int)sizeof(cli->buffer) - 1) {
        return false;
    }
    if (cli->cursor < cli->len && is_whitespace(cli->buffer[cli->cursor])) {
        return true;
    }

    memmove(&cli->buffer[cli->cursor + 1], &cli->buffer[cli->cursor],
            (size_t)(cli->len - cli->cursor + 1));
    cli->buffer[cli->cursor] = ' ';
    cli->cursor++;
    cli->len++;
    cli->buffer[cli->len] = '\0';
    return true;
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

static void cli_ansi(struct xf_cli *cli, int n, char code)
{
    char buffer[16];
    int len;

    if (n <= 0) {
        return;
    }
    len = snprintf(buffer, sizeof(buffer), "\x1b[%d%c", n, code);
    if (len <= 0) {
        return;
    }
    for (int i = 0; i < len && buffer[i] != '\0'; ++i) {
        cli_putchar(cli, buffer[i], (i == len - 1));
    }
}

static void term_cursor_back(struct xf_cli *cli, int n)
{
    cli_ansi(cli, n, 'D');
}

static void redraw_line(struct xf_cli *cli)
{
    cli_puts(cli, MOVE_BOL CLEAR_EOL);
#if XF_CLI_COLORFUL
    cli_puts(cli, XF_CLI_PROMPT_COLOR);
    cli_puts(cli, cli->prompt);
    cli_puts(cli, XF_CLI_COLOR_RESET);
    cli_puts(cli, XF_CLI_COMMAND_COLOR);
#else
    cli_puts(cli, cli->prompt);
#endif
    cli_puts(cli, cli->buffer);
    if (cli->cursor < cli->len) {
        term_cursor_back(cli, cli->len - cli->cursor);
    }
}

static void print_suggestions(struct xf_cli *cli,
                              char matches[][XF_CLI_MAX_LINE],
                              int match_count)
{
    int i;

#if XF_CLI_COLORFUL
    cli_puts(cli, XF_CLI_COLOR_RESET);
#endif
    cli_puts(cli, "\n");
    for (i = 0; i < match_count; ++i) {
        cli_puts(cli, "  ");
        cli_puts(cli, matches[i]);
        cli_puts(cli, "\n");
    }
}
