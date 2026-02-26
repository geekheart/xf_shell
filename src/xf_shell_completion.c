/**
 * @file xf_shell_completion.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026, CorAL. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */
#if XF_SHELL_COMPLETION_ENABLE
#include "xf_shell_completion.h"
#include "xf_shell_cli.h"
#if XF_SHELL_COMPLETION_ENABLE && XF_SHELL_COMPLETION_ENABLE_VALUE
#include "xf_shell_completion_context.h"
#endif
#include <stdio.h>
#include <string.h>
#include "xf_shell.h"

/* ==================== [Defines] =========================================== */

#define CLEAR_EOL "\x1b[0K"
#define MOVE_BOL "\x1b[1G"

/* ==================== [Typedefs] ========================================== */

typedef xf_shell_cmd_t cmd_item_t;
typedef xf_opt_arg_t cmd_opt_t;

/* ==================== [Static Prototypes] ================================= */

static bool is_whitespace(char ch);
static bool starts_with(const char* str, const char* prefix);
static int copy_prefix_from_line(const struct xf_cli* cli,
                                 int start,
                                 int cursor,
                                 char prefix[],
                                 int prefix_size);
static cmd_item_t* find_command_by_name(cmd_item_t *const *cmd_table,
                                        int cmd_count,
                                        const char* name);
static bool add_match(char matches[][XF_CLI_MAX_LINE],
                      int *match_count,
                      int max_matches,
                      const char *value);
static int collect_command_matches(cmd_item_t *const *cmd_table,
                                   int cmd_count,
                                   const char *prefix,
                                   char matches[][XF_CLI_MAX_LINE],
                                   int max_matches,
                                   int* total_candidates);
#if XF_SHELL_COMPLETION_ENABLE_OPTION
static int collect_option_matches(cmd_item_t *cmd,
                                  const char *prefix,
                                  char matches[][XF_CLI_MAX_LINE],
                                  int max_matches,
                                  int* total_candidates);
#endif
#if XF_SHELL_COMPLETION_ENABLE_VALUE
static int collect_completion_matches(const xf_completion_words_t* completion,
                                      const char* prefix,
                                      char matches[][XF_CLI_MAX_LINE],
                                      int max_matches,
                                      int* total_candidates);
static void resolve_value_completion(const xf_completion_words_t* completion,
                                     const char* prefix,
                                     char matches[][XF_CLI_MAX_LINE],
                                     int max_matches,
                                     int* candidate_budget,
                                     int* match_count);
#endif
static cmd_item_t* resolve_current_command(const struct xf_cli* cli,
                                           cmd_item_t *const *cmd_table,
                                           int cmd_count);
static int common_prefix_len(char matches[][XF_CLI_MAX_LINE], int match_count);
static bool replace_prefix(struct xf_cli* cli, int start, int cursor, const char* replacement);
static bool insert_space_if_needed(struct xf_cli* cli);
static void cli_putchar(struct xf_cli* cli, char ch, bool is_last);
static void cli_puts(struct xf_cli* cli, const char* s);
static void cli_ansi(struct xf_cli* cli, int n, char code);
static void term_cursor_back(struct xf_cli* cli, int n);
static void redraw_line(struct xf_cli* cli);
#if XF_SHELL_COMPLETION_ENABLE_SUGGESTIONS
static void print_suggestions(struct xf_cli *cli,
                              char matches[][XF_CLI_MAX_LINE],
                              int match_count);
#endif

/* ==================== [Static Variables] ================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

#if XF_SHELL_COMPLETION_ENABLE

bool xf_shell_completion_handle_tab(struct xf_cli *cli,
                                    xf_shell_cmd_t *const *cmd_table,
                                    int cmd_count,
                                    char matches[][XF_CLI_MAX_LINE],
                                    int max_matches)
{
    int token_start;
    int prefix_len;
    bool first_token = true;
    int token_end;
    bool at_token_end;
    char prefix[XF_CLI_MAX_LINE];
    int candidate_budget = 0;
    int match_count = 0;
    cmd_item_t* cmd = NULL;
    int replace_start;
    int replace_cursor;

    if (cli == NULL || cmd_table == NULL || cmd_count < 0 ||
        matches == NULL || max_matches <= 0) {
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

    prefix_len = copy_prefix_from_line(cli, token_start, cli->cursor,
                                       prefix, (int)sizeof(prefix));

    token_end = cli->cursor;
    while (token_end < cli->len && !is_whitespace(cli->buffer[token_end])) {
        token_end++;
    }
    at_token_end = (token_end == cli->cursor);
    replace_start = token_start;
    replace_cursor = cli->cursor;

    if (first_token) {
        match_count = collect_command_matches(cmd_table, cmd_count,
                                              prefix, matches, max_matches,
                                              &candidate_budget);
    } else {
        cmd = resolve_current_command(cli, cmd_table, cmd_count);
        if (cmd != NULL) {
#if XF_SHELL_COMPLETION_ENABLE_VALUE
            xf_shell_completion_context_t context;

            xf_shell_completion_resolve_context(cli, cmd, cli->cursor, &context);

            if (context.inline_opt != NULL) {
                prefix_len = copy_prefix_from_line(cli, context.inline_value_start, cli->cursor,
                                                   prefix, (int)sizeof(prefix));
                replace_start = context.inline_value_start;
                replace_cursor = cli->cursor;

                resolve_value_completion(&context.inline_opt->completion, prefix, matches,
                                         max_matches, &candidate_budget, &match_count);
            } else if (context.pending_opt != NULL) {
                resolve_value_completion(&context.pending_opt->completion, prefix, matches,
                                         max_matches, &candidate_budget, &match_count);
            } else {
                if (prefix[0] != '-' && context.positional_arg != NULL) {
                    resolve_value_completion(&context.positional_arg->completion, prefix, matches,
                                             max_matches, &candidate_budget, &match_count);
                }
#if XF_SHELL_COMPLETION_ENABLE_OPTION
                if ((prefix[0] == '-') || (candidate_budget <= 0 && prefix[0] == '\0')) {
                    match_count = collect_option_matches(cmd, prefix,
                                                         matches, max_matches,
                                                         &candidate_budget);
                }
#endif
            }
#elif XF_SHELL_COMPLETION_ENABLE_OPTION
            if ((prefix[0] == '-') || (prefix[0] == '\0')) {
                match_count = collect_option_matches(cmd, prefix, matches, max_matches,
                                                     &candidate_budget);
            }
#endif
        }
    }

    if (candidate_budget <= 0) {
        cli_putchar(cli, '\a', true);
        return true;
    }

    if (match_count <= 0) {
        cli_putchar(cli, '\a', true);
        return true;
    }

    if (match_count == 1) {
        bool replaced = replace_prefix(cli, replace_start, replace_cursor, matches[0]);
        if (!replaced) {
            cli_putchar(cli, '\a', true);
            return true;
        }
        if (at_token_end) {
            (void)insert_space_if_needed(cli);
        }
        redraw_line(cli);
        return true;
    }

    {
        int lcp_len = common_prefix_len(matches, match_count);
        if (lcp_len > prefix_len) {
            char lcp[XF_CLI_MAX_LINE];
            memcpy(lcp, matches[0], (size_t)lcp_len);
            lcp[lcp_len] = '\0';
            if (!replace_prefix(cli, replace_start, replace_cursor, lcp)) {
                cli_putchar(cli, '\a', true);
                return true;
            }
            redraw_line(cli);
            return true;
        }
    }

#if XF_SHELL_COMPLETION_ENABLE_SUGGESTIONS
    print_suggestions(cli, matches, match_count);
#endif
    redraw_line(cli);
    return true;
}
#endif

/* ==================== [Static Functions] ================================== */

#if XF_SHELL_COMPLETION_ENABLE

static bool is_whitespace(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}

static bool starts_with(const char* str, const char* prefix) {
    while (*prefix != '\0') {
        if (*str != *prefix) {
            return false;
        }
        str++;
        prefix++;
    }
    return true;
}

static int copy_prefix_from_line(const struct xf_cli* cli,
                                 int start,
                                 int cursor,
                                 char prefix[],
                                 int prefix_size) {
    int len;

    if (cli == NULL || prefix == NULL || prefix_size <= 0) {
        return 0;
    }

    if (start < 0) {
        start = 0;
    }
    if (cursor < start) {
        cursor = start;
    }
    if (cursor > cli->len) {
        cursor = cli->len;
    }

    len = cursor - start;
    if (len >= prefix_size) {
        len = prefix_size - 1;
    }
    if (len > 0) {
        memcpy(prefix, &cli->buffer[start], (size_t)len);
    }
    prefix[len] = '\0';
    return len;
}

static cmd_item_t* find_command_by_name(cmd_item_t *const *cmd_table,
                                        int cmd_count,
                                        const char* name) {
    int i;

    if (cmd_table == NULL || cmd_count <= 0 || name == NULL) {
        return NULL;
    }

    for (i = 0; i < cmd_count; ++i) {
        cmd_item_t* it = cmd_table[i];
        if (it != NULL && strcmp(name, it->command) == 0) {
            return it;
        }
    }

    return NULL;
}

static bool add_match(char matches[][XF_CLI_MAX_LINE],
                      int *match_count,
                      int max_matches,
                      const char *value)
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

static int collect_command_matches(cmd_item_t *const *cmd_table,
                                   int cmd_count,
                                   const char *prefix,
                                   char matches[][XF_CLI_MAX_LINE],
                                   int max_matches,
                                   int* total_candidates)
{
    int match_count = 0;
    int i;

    if (total_candidates != NULL) {
        *total_candidates = 0;
    }

    if (cmd_table == NULL || cmd_count <= 0) {
        return 0;
    }

    for (i = 0; i < cmd_count; ++i) {
        cmd_item_t* it = cmd_table[i];
        if (it == NULL) {
            continue;
        }
        if (total_candidates != NULL) {
            (*total_candidates)++;
        }
        if (starts_with(it->command, prefix)) {
            (void)add_match(matches, &match_count, max_matches, it->command);
        }
    }

    return match_count;
}

#if XF_SHELL_COMPLETION_ENABLE_OPTION
static int collect_option_matches(cmd_item_t *cmd,
                                  const char *prefix,
                                  char matches[][XF_CLI_MAX_LINE],
                                  int max_matches,
                                  int* total_candidates)
{
    int match_count = 0;
    uint16_t i;
    char candidate[XF_CLI_MAX_LINE];

    if (total_candidates != NULL) {
        *total_candidates = 0;
    }

    if (cmd == NULL) {
        return 0;
    }

    for (i = 0; i < cmd->_opt_count; ++i) {
        cmd_opt_t* it = cmd->_opts[i];
        if (it == NULL) {
            continue;
        }

        if (it->long_opt != NULL && it->long_opt[0] != '\0') {
            if (total_candidates != NULL) {
                (*total_candidates)++;
            }
            snprintf(candidate, sizeof(candidate), "--%s", it->long_opt);
            if (starts_with(candidate, prefix)) {
                (void)add_match(matches, &match_count, max_matches, candidate);
            }
        }

        if (it->short_opt != '\0') {
            if (total_candidates != NULL) {
                (*total_candidates)++;
            }
            candidate[0] = '-';
            candidate[1] = it->short_opt;
            candidate[2] = '\0';
            if (starts_with(candidate, prefix)) {
                (void)add_match(matches, &match_count, max_matches, candidate);
            }
        }
    }

    return match_count;
}
#endif

#if XF_SHELL_COMPLETION_ENABLE_VALUE
static int collect_completion_matches(const xf_completion_words_t* completion,
                                      const char* prefix,
                                      char matches[][XF_CLI_MAX_LINE],
                                      int max_matches,
                                      int* total_candidates)
{
    int match_count = 0;
    const char* const* items = NULL;
    uint16_t count = 0U;
    uint16_t i;

    if (total_candidates != NULL) {
        *total_candidates = 0;
    }

    if (completion != NULL) {
        items = completion->items;
        count = completion->count;
    }

    if (items == NULL || count == 0U || prefix == NULL) {
        return 0;
    }

    for (i = 0; i < count; ++i) {
        const char* it = items[i];
        if (it == NULL || it[0] == '\0') {
            continue;
        }
        if (total_candidates != NULL) {
            (*total_candidates)++;
        }
        if (starts_with(it, prefix)) {
            (void)add_match(matches, &match_count, max_matches, it);
        }
    }

    return match_count;
}

static void resolve_value_completion(const xf_completion_words_t* completion,
                                     const char* prefix,
                                     char matches[][XF_CLI_MAX_LINE],
                                     int max_matches,
                                     int* candidate_budget,
                                     int* match_count)
{
    int total = 0;
    int matched = collect_completion_matches(completion, prefix, matches,
                                             max_matches, &total);

    if (candidate_budget != NULL) {
        *candidate_budget = total;
    }
    if (match_count != NULL) {
        *match_count = matched;
    }
}
#endif

static cmd_item_t* resolve_current_command(const struct xf_cli* cli,
                                           cmd_item_t *const *cmd_table,
                                           int cmd_count) {
    int start = 0;
    int end = 0;
    int cmd_len;
    char command[XF_CLI_MAX_LINE];

    if (cli == NULL || cmd_table == NULL || cmd_count <= 0) {
        return NULL;
    }

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
    return find_command_by_name(cmd_table, cmd_count, command);
}

static int common_prefix_len(char matches[][XF_CLI_MAX_LINE], int match_count)
{
    int common_len;
    int i;

    if (match_count <= 0) {
        return 0;
    }

    common_len = (int)strlen(matches[0]);
    for (i = 1; i < match_count && common_len > 0; ++i) {
        int j = 0;
        while (j < common_len && matches[i][j] != '\0' && matches[i][j] == matches[0][j]) {
            j++;
        }
        common_len = j;
    }
    return common_len;
}

static bool replace_prefix(struct xf_cli* cli, int start, int cursor, const char* replacement) {
    int old_len = cursor - start;
    int rep_len = (int)strlen(replacement);
    int new_len = cli->len - old_len + rep_len;

    if (start < 0 || cursor < start || cursor > cli->len) {
        return false;
    }
    if (new_len >= (int)sizeof(cli->buffer)) {
        return false;
    }

    memmove(&cli->buffer[start + rep_len], &cli->buffer[cursor], (size_t)(cli->len - cursor + 1));
    memcpy(&cli->buffer[start], replacement, (size_t)rep_len);
    cli->len = new_len;
    cli->cursor = start + rep_len;
    cli->buffer[cli->len] = '\0';
    return true;
}

static bool insert_space_if_needed(struct xf_cli* cli) {
    if (cli->len >= (int)sizeof(cli->buffer) - 1) {
        return false;
    }
    if (cli->cursor < cli->len && is_whitespace(cli->buffer[cli->cursor])) {
        return true;
    }

    memmove(&cli->buffer[cli->cursor + 1], &cli->buffer[cli->cursor], (size_t)(cli->len - cli->cursor + 1));
    cli->buffer[cli->cursor] = ' ';
    cli->cursor++;
    cli->len++;
    cli->buffer[cli->len] = '\0';
    return true;
}

static void cli_putchar(struct xf_cli* cli, char ch, bool is_last) {
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

static void cli_puts(struct xf_cli* cli, const char* s) {
    for (; *s != '\0'; s++) {
        cli_putchar(cli, *s, s[1] == '\0');
    }
}

static void cli_ansi(struct xf_cli* cli, int n, char code) {
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

static void term_cursor_back(struct xf_cli* cli, int n) {
    cli_ansi(cli, n, 'D');
}

static void redraw_line(struct xf_cli* cli) {
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

#if XF_SHELL_COMPLETION_ENABLE_SUGGESTIONS
static void print_suggestions(struct xf_cli *cli,
                              char matches[][XF_CLI_MAX_LINE],
                              int match_count)
{
    int i;

#if XF_CLI_COLORFUL
    cli_puts(cli, XF_CLI_COLOR_RESET);
#endif
    cli_puts(cli, XF_SHELL_NEWLINE);
    for (i = 0; i < match_count; ++i) {
        cli_puts(cli, "  ");
        cli_puts(cli, matches[i]);
        cli_puts(cli, XF_SHELL_NEWLINE);
    }
}
#endif
#endif
#endif
