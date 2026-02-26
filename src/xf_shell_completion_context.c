/**
 * @file xf_shell_completion_context.c
 * @brief Internal parser for completion cursor context.
 */

/* ==================== [Includes] ========================================== */
#include "xf_shell_completion.h"
#include "xf_shell_completion_context.h"
#include <string.h>

/* ==================== [Typedefs] ========================================== */

#if XF_SHELL_COMPLETION_ENABLE && XF_SHELL_COMPLETION_ENABLE_VALUE

typedef xf_shell_cmd_t cmd_item_t;
typedef xf_opt_arg_t cmd_opt_t;
typedef xf_arg_t cmd_arg_t;

typedef struct {
    int start;
    int end;
} token_span_t;

/* ==================== [Static Prototypes] ================================= */

static bool is_whitespace(char ch);
static cmd_opt_t* find_option_by_long_name(cmd_item_t* cmd, const char* name, int len);
static cmd_opt_t* find_option_by_short_name(cmd_item_t* cmd, char short_opt);
static cmd_arg_t* find_argument_by_index(cmd_item_t* cmd, int index);
static int tokenize_line(const struct xf_cli* cli, token_span_t tokens[], int max_tokens);
static int locate_cursor_token(const token_span_t tokens[],
                               int token_count,
                               int cursor,
                               bool* has_current_token);
static void analyze_tokens_before_cursor(const struct xf_cli* cli,
                                         cmd_item_t* cmd,
                                         const token_span_t tokens[],
                                         int token_count,
                                         int cursor_token_index,
                                         cmd_opt_t** pending_opt,
                                         int* positional_count);
static cmd_opt_t* resolve_inline_value_option(const struct xf_cli* cli,
                                              cmd_item_t* cmd,
                                              token_span_t token,
                                              int cursor,
                                              int* value_start);

/* ==================== [Global Functions] ================================== */

void xf_shell_completion_resolve_context(const struct xf_cli* cli,
                                         xf_shell_cmd_t* cmd,
                                         int cursor,
                                         xf_shell_completion_context_t* out) {
    token_span_t tokens[XF_CLI_MAX_ARGC];
    int token_count;
    bool has_current_token = false;
    int cursor_token_index;
    cmd_opt_t* pending_opt = NULL;
    int positional_count = 0;

    if (out == NULL) {
        return;
    }

    out->inline_opt = NULL;
    out->inline_value_start = 0;
    out->pending_opt = NULL;
    out->positional_arg = NULL;

    if (cli == NULL || cmd == NULL) {
        return;
    }

    if (cursor < 0) {
        cursor = 0;
    }
    if (cursor > cli->len) {
        cursor = cli->len;
    }

    token_count = tokenize_line(cli, tokens, XF_CLI_MAX_ARGC);
    cursor_token_index = locate_cursor_token(tokens, token_count, cursor,
                                             &has_current_token);

    if (has_current_token && cursor_token_index > 0 && cursor_token_index < token_count) {
        out->inline_opt = resolve_inline_value_option(cli, cmd,
                                                      tokens[cursor_token_index],
                                                      cursor,
                                                      &out->inline_value_start);
    }

    analyze_tokens_before_cursor(cli, cmd, tokens, token_count,
                                 cursor_token_index, &pending_opt,
                                 &positional_count);

    out->pending_opt = pending_opt;
    out->positional_arg = find_argument_by_index(cmd, positional_count);
}

/* ==================== [Static Functions] ================================== */

static bool is_whitespace(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}

static cmd_opt_t* find_option_by_long_name(cmd_item_t* cmd, const char* name, int len) {
    uint16_t i;

    if (cmd == NULL || name == NULL || len <= 0) {
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
        if ((int)strlen(it->long_opt) != len) {
            continue;
        }
        if (strncmp(it->long_opt, name, (size_t)len) == 0) {
            return it;
        }
    }

    return NULL;
}

static cmd_opt_t* find_option_by_short_name(cmd_item_t* cmd, char short_opt) {
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

static cmd_arg_t* find_argument_by_index(cmd_item_t* cmd, int index) {
    if (cmd == NULL || index < 0) {
        return NULL;
    }

    if ((uint16_t)index >= cmd->_arg_count) {
        return NULL;
    }

    return cmd->_args[index];
}

static int tokenize_line(const struct xf_cli* cli, token_span_t tokens[], int max_tokens) {
    int i = 0;
    int count = 0;

    if (cli == NULL || tokens == NULL || max_tokens <= 0) {
        return 0;
    }

    while (i < cli->len && count < max_tokens) {
        while (i < cli->len && is_whitespace(cli->buffer[i])) {
            i++;
        }
        if (i >= cli->len) {
            break;
        }

        tokens[count].start = i;
        while (i < cli->len && !is_whitespace(cli->buffer[i])) {
            i++;
        }
        tokens[count].end = i;
        count++;
    }

    return count;
}

static int locate_cursor_token(const token_span_t tokens[],
                               int token_count,
                               int cursor,
                               bool* has_current_token) {
    int i;

    if (has_current_token != NULL) {
        *has_current_token = false;
    }

    for (i = 0; i < token_count; ++i) {
        if (cursor < tokens[i].start) {
            return i;
        }
        if (cursor <= tokens[i].end) {
            if (has_current_token != NULL) {
                *has_current_token = true;
            }
            return i;
        }
    }

    return token_count;
}

static void analyze_tokens_before_cursor(const struct xf_cli* cli,
                                         cmd_item_t* cmd,
                                         const token_span_t tokens[],
                                         int token_count,
                                         int cursor_token_index,
                                         cmd_opt_t** pending_opt,
                                         int* positional_count) {
    bool force_positional = false;
    int i;

    if (pending_opt != NULL) {
        *pending_opt = NULL;
    }
    if (positional_count != NULL) {
        *positional_count = 0;
    }

    if (cli == NULL || cmd == NULL || tokens == NULL ||
        pending_opt == NULL || positional_count == NULL) {
        return;
    }

    if (cursor_token_index > token_count) {
        cursor_token_index = token_count;
    }
    if (cursor_token_index < 1) {
        return;
    }

    for (i = 1; i < cursor_token_index; ++i) {
        int start = tokens[i].start;
        int end = tokens[i].end;
        int len = end - start;

        if (len <= 0) {
            continue;
        }

        if (*pending_opt != NULL) {
            *pending_opt = NULL;
            continue;
        }

        if (force_positional) {
            (*positional_count)++;
            continue;
        }

        if (len == 2 && cli->buffer[start] == '-' && cli->buffer[start + 1] == '-') {
            force_positional = true;
            continue;
        }

        if (cli->buffer[start] == '-' && len > 1) {
            if (cli->buffer[start + 1] == '-') {
                int name_start = start + 2;
                int eq = -1;

                for (int j = name_start; j < end; ++j) {
                    if (cli->buffer[j] == '=') {
                        eq = j;
                        break;
                    }
                }

                if (eq >= 0) {
                    continue;
                }

                if (name_start < end) {
                    cmd_opt_t* opt = find_option_by_long_name(cmd,
                                                              &cli->buffer[name_start],
                                                              end - name_start);
                    if (opt != NULL) {
                        *pending_opt = opt;
                    }
                }
                continue;
            }

            if (len == 2) {
                cmd_opt_t* opt = find_option_by_short_name(cmd, cli->buffer[start + 1]);
                if (opt != NULL) {
                    *pending_opt = opt;
                }
                continue;
            }

            if (len >= 3 && cli->buffer[start + 2] == '=') {
                continue;
            }

            continue;
        }

        (*positional_count)++;
    }
}

static cmd_opt_t* resolve_inline_value_option(const struct xf_cli* cli,
                                              cmd_item_t* cmd,
                                              token_span_t token,
                                              int cursor,
                                              int* value_start) {
    int eq = -1;

    if (cli == NULL || cmd == NULL || value_start == NULL) {
        return NULL;
    }
    if (token.start < 0 || token.end <= token.start || token.end > cli->len) {
        return NULL;
    }
    if (cli->buffer[token.start] != '-') {
        return NULL;
    }

    for (int i = token.start; i < token.end; ++i) {
        if (cli->buffer[i] == '=') {
            eq = i;
            break;
        }
    }

    if (eq < 0 || cursor <= eq) {
        return NULL;
    }

    if (token.start + 1 >= token.end) {
        return NULL;
    }

    if (cli->buffer[token.start + 1] == '-') {
        int name_start = token.start + 2;
        int name_len = eq - name_start;
        cmd_opt_t* opt;

        if (name_len <= 0) {
            return NULL;
        }

        opt = find_option_by_long_name(cmd, &cli->buffer[name_start], name_len);
        if (opt == NULL) {
            return NULL;
        }

        *value_start = eq + 1;
        return opt;
    }

    if (eq != token.start + 2) {
        return NULL;
    }

    {
        cmd_opt_t* opt = find_option_by_short_name(cmd, cli->buffer[token.start + 1]);
        if (opt == NULL) {
            return NULL;
        }
        *value_start = eq + 1;
        return opt;
    }
}

#endif
