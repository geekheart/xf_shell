/**
 * Useful links:
 *    http://www.asciitable.com
 *    https://en.wikipedia.org/wiki/ANSI_escape_code
 */

#include <stdio.h>
#include <string.h>

#include "xf_shell.h"
#include "xf_shell_cli.h"

#define CTRL_R 0x12

#define CLEAR_EOL "\x1b[0K"
#define MOVE_BOL "\x1b[1G"

static void cli_putchar(struct xf_cli *cli, char ch, bool is_last)
{
    if (cli->put_char) {
#if XF_CLI_SERIAL_XLATE
        if (ch == '\n' && !XF_SHELL_NEWLINE_IS_CRLF)
            cli->put_char(cli->cb_data, '\r', false);
#endif
        cli->put_char(cli->cb_data, ch, is_last);
    }
}

static void cli_puts(struct xf_cli *cli, const char *s)
{
    for (; *s; s++)
        cli_putchar(cli, *s, s[1] == '\0');
}

static void cli_set_command_color(struct xf_cli *cli)
{
#if XF_CLI_COLORFUL
    cli_puts(cli, XF_CLI_COMMAND_COLOR);
#else
    (void)cli;
#endif
}

static void cli_reset_color(struct xf_cli *cli)
{
#if XF_CLI_COLORFUL
    cli_puts(cli, XF_CLI_COLOR_RESET);
#else
    (void)cli;
#endif
}

static void cli_put_prompt(struct xf_cli *cli)
{
#if XF_CLI_COLORFUL
    cli_puts(cli, XF_CLI_PROMPT_COLOR);
    cli_puts(cli, cli->prompt);
    cli_reset_color(cli);
    cli_set_command_color(cli);
#else
    cli_puts(cli, cli->prompt);
#endif
}

static void xf_cli_reset_line(struct xf_cli *cli)
{
    cli->len = 0;
    cli->cursor = 0;
    cli->counter = 0;
    cli->have_csi = cli->have_escape = false;
#if XF_CLI_HISTORY_LEN
    cli->history_pos = -1;
    cli->searching = false;
#endif
}

void xf_cli_init(struct xf_cli *cli, const char *prompt,
                       void (*put_char)(void *data, char ch, bool is_last),
                       void *cb_data)
{
    memset(cli, 0, sizeof(*cli));
    cli->put_char = put_char;
    cli->cb_data = cb_data;
    if (prompt) {
        strncpy(cli->prompt, prompt, sizeof(cli->prompt));
        cli->prompt[sizeof(cli->prompt) - 1] = '\0';
    }

    xf_cli_reset_line(cli);
}

static void cli_ansi(struct xf_cli *cli, int n, char code)
{
    char buffer[5] = {'\x1b', '[', '0' + (n % 10), code, '\0'};
    cli_puts(cli, buffer);
}

static void term_cursor_back(struct xf_cli *cli, int n)
{
    while (n > 0) {
        int count = n > 9 ? 9 : n;
        cli_ansi(cli, count, 'D');
        n -= count;
    }
}

static void term_cursor_fwd(struct xf_cli *cli, int n)
{
    while (n > 0) {
        int count = n > 9 ? 9 : n;
        cli_ansi(cli, count, 'C');
        n -= count;
    }
}

#if XF_CLI_HISTORY_LEN
static void term_backspace(struct xf_cli *cli, int n)
{
    while (n--)
        cli_putchar(cli, '\b', n == 0);
}

static const char *xf_cli_get_history_search(struct xf_cli *cli)
{
    for (int i = 0;; i++) {
        const char *h = xf_cli_get_history(cli, i);
        if (!h)
            return NULL;
        if (strstr(h, cli->buffer))
            return h;
    }
    return NULL;
}
#endif

static void xf_cli_insert_default_char(struct xf_cli *cli,
                                             char ch)
{
    // If the buffer is full, there's nothing we can do
    if (cli->len >= (int)sizeof(cli->buffer) - 1)
        return;
    // Insert a gap in the buffer for the new character
    memmove(&cli->buffer[cli->cursor + 1], &cli->buffer[cli->cursor],
            cli->len - cli->cursor);
    cli->buffer[cli->cursor] = ch;
    cli->len++;
    cli->buffer[cli->len] = '\0';
    cli->cursor++;

#if XF_CLI_HISTORY_LEN
    if (cli->searching) {
        cli_puts(cli, MOVE_BOL CLEAR_EOL "search:");
        const char *h = xf_cli_get_history_search(cli);
        if (h)
            cli_puts(cli, h);

    } else
#endif
    {
        cli_puts(cli, &cli->buffer[cli->cursor - 1]);
        term_cursor_back(cli, cli->len - cli->cursor);
    }
}

const char *xf_cli_get_history(struct xf_cli *cli,
                                     int history_pos)
{
#if XF_CLI_HISTORY_LEN
    int pos = 0;

    if (history_pos < 0)
        return NULL;

    // Search back through the history buffer for `history_pos` entry
    for (int i = 0; i < history_pos; i++) {
        int len = strlen(&cli->history[pos]);
        if (len == 0)
            return NULL;
        pos += len + 1;
        if (pos == sizeof(cli->history))
            return NULL;
    }

    return &cli->history[pos];
#else
    (void)cli;
    (void)history_pos;
    return NULL;
#endif
}

#if XF_CLI_HISTORY_LEN
static void xf_cli_extend_history(struct xf_cli *cli)
{
    int len = strlen(cli->buffer);
    if (len > 0) {
        // If the new entry is the same as the most recent history entry,
        // then don't insert it
        if (strcmp(cli->buffer, cli->history) == 0)
            return;
        memmove(&cli->history[len + 1], &cli->history[0],
                sizeof(cli->history) - len + 1);
        memcpy(cli->history, cli->buffer, len + 1);
        // Make sure it's always nul terminated
        cli->history[sizeof(cli->history) - 1] = '\0';
    }
}

static void xf_cli_stop_search(struct xf_cli *cli, bool print)
{
    const char *h = xf_cli_get_history_search(cli);
    if (h) {
        strncpy(cli->buffer, h, sizeof(cli->buffer));
        cli->buffer[sizeof(cli->buffer) - 1] = '\0';
    } else
        cli->buffer[0] = '\0';
    cli->len = cli->cursor = strlen(cli->buffer);
    cli->searching = false;
    if (print) {
        cli_puts(cli, MOVE_BOL CLEAR_EOL);
        cli_put_prompt(cli);
        cli_puts(cli, cli->buffer);
    }
}
#endif

bool xf_cli_insert_char(struct xf_cli *cli, char ch)
{
    // If we're inserting a character just after a finished line, clear things
    // up
    if (cli->done) {
        cli->buffer[0] = '\0';
        cli->done = false;
    }
    if (cli->have_csi) {
        if (ch >= '0' && ch <= '9' && cli->counter < 100) {
            cli->counter = cli->counter * 10 + ch - '0';
        } else {
            if (cli->counter == 0)
                cli->counter = 1;
            switch (ch) {
            case 'A': { // up arrow
#if XF_CLI_HISTORY_LEN
                // Backspace over our current line
                term_backspace(cli, cli->done ? 0 : cli->cursor);
                const char *line =
                    xf_cli_get_history(cli, cli->history_pos + 1);
                if (line) {
                    int len = strlen(line);
                    cli->history_pos++;
                    strncpy(cli->buffer, line, sizeof(cli->buffer));
                    cli->buffer[sizeof(cli->buffer) - 1] = '\0';
                    cli->len = len;
                    cli->cursor = len;
                    cli_puts(cli, cli->buffer);
                    cli_puts(cli, CLEAR_EOL);
                } else {
                    int tmp = cli->history_pos; // We don't want to wrap this
                                                // history, so retain it
                    cli->buffer[0] = '\0';
                    xf_cli_reset_line(cli);
                    cli->history_pos = tmp;
                    cli_puts(cli, CLEAR_EOL);
                }
#endif
                break;
            }

            case 'B': { // down arrow
#if XF_CLI_HISTORY_LEN
                term_backspace(cli, cli->done ? 0 : cli->cursor);
                const char *line =
                    xf_cli_get_history(cli, cli->history_pos - 1);
                if (line) {
                    int len = strlen(line);
                    cli->history_pos--;
                    strncpy(cli->buffer, line, sizeof(cli->buffer));
                    cli->buffer[sizeof(cli->buffer) - 1] = '\0';
                    cli->len = len;
                    cli->cursor = len;
                    cli_puts(cli, cli->buffer);
                    cli_puts(cli, CLEAR_EOL);
                } else {
                    cli->buffer[0] = '\0';
                    xf_cli_reset_line(cli);
                    cli_puts(cli, CLEAR_EOL);
                }
#endif
                break;
            }

            case 'C':
                if (cli->cursor <= cli->len - cli->counter) {
                    cli->cursor += cli->counter;
                    term_cursor_fwd(cli, cli->counter);
                }
                break;
            case 'D':
                if (cli->cursor >= cli->counter) {
                    cli->cursor -= cli->counter;
                    term_cursor_back(cli, cli->counter);
                }
                break;
            case 'F':
                term_cursor_fwd(cli, cli->len - cli->cursor);
                cli->cursor = cli->len;
                break;
            case 'H':
                term_cursor_back(cli, cli->cursor);
                cli->cursor = 0;
                break;
            case '~':
                if (cli->counter == 3) { // delete key
                    if (cli->cursor < cli->len) {
                        memmove(&cli->buffer[cli->cursor],
                                &cli->buffer[cli->cursor + 1],
                                cli->len - cli->cursor);
                        cli->len--;
                        cli_puts(cli, &cli->buffer[cli->cursor]);
                        cli_puts(cli, " ");
                        term_cursor_back(cli, cli->len - cli->cursor + 1);
                    }
                }
                break;
            default:
                // TODO: Handle more escape sequences
                break;
            }
            cli->have_csi = cli->have_escape = false;
            cli->counter = 0;
        }
    } else {
        switch (ch) {
        case '\0':
            break;
        case '\x01':
            // Go to the beginning of the line
            term_cursor_back(cli, cli->cursor);
            cli->cursor = 0;
            break;
        case '\x03':
            cli_reset_color(cli);
            cli_puts(cli, "^C" XF_SHELL_NEWLINE);
            cli_put_prompt(cli);
            xf_cli_reset_line(cli);
            cli->buffer[0] = '\0';
            break;
        case '\x05': // Ctrl-E
            term_cursor_fwd(cli, cli->len - cli->cursor);
            cli->cursor = cli->len;
            break;
        case '\x0b': // Ctrl-K
            cli_puts(cli, CLEAR_EOL);
            cli->buffer[cli->cursor] = '\0';
            cli->len = cli->cursor;
            break;
        case '\x0c': // Ctrl-L
            cli_puts(cli, MOVE_BOL CLEAR_EOL);
            cli_put_prompt(cli);
            cli_puts(cli, cli->buffer);
            term_cursor_back(cli, cli->len - cli->cursor);
            break;
        case '\b': // Backspace
        case 0x7f: // backspace?
#if XF_CLI_HISTORY_LEN
            if (cli->searching)
                xf_cli_stop_search(cli, true);
#endif
            if (cli->cursor > 0) {
                memmove(&cli->buffer[cli->cursor - 1],
                        &cli->buffer[cli->cursor],
                        cli->len - cli->cursor + 1);
                cli->cursor--;
                cli->len--;
                term_cursor_back(cli, 1);
                cli_puts(cli, &cli->buffer[cli->cursor]);
                cli_puts(cli, " ");
                term_cursor_back(cli, cli->len - cli->cursor + 1);
            }
            break;
        case CTRL_R:
#if XF_CLI_HISTORY_LEN
            if (!cli->searching) {
                cli_puts(cli, XF_SHELL_NEWLINE "search:");
                cli->searching = true;
            }
#endif
            break;
        case '\x1b':
#if XF_CLI_HISTORY_LEN
            if (cli->searching)
                xf_cli_stop_search(cli, true);
#endif
            cli->have_csi = false;
            cli->have_escape = true;
            cli->counter = 0;
            break;
        case '[':
            if (cli->have_escape)
                cli->have_csi = true;
            else
                xf_cli_insert_default_char(cli, ch);
            break;
        case '\r':
#if XF_CLI_SERIAL_XLATE
            ch = '\n'; // So cli->done will exit
#endif
            // fallthrough
        case '\n':
            cli_reset_color(cli);
            cli_puts(cli, XF_SHELL_NEWLINE);
            break;
        default:
            if (ch > 0)
                xf_cli_insert_default_char(cli, ch);
        }
    }
    cli->done = (ch == '\n' || ch == '\r');

    if (cli->done) {
#if XF_CLI_HISTORY_LEN
        if (cli->searching)
            xf_cli_stop_search(cli, false);
        xf_cli_extend_history(cli);
#endif
        xf_cli_reset_line(cli);
    }
    // printf("Done with char 0x%x (done=%d)\n", ch, cli->done);
    return cli->done;
}

const char *xf_cli_get_line(const struct xf_cli *cli)
{
    if (!cli->done)
        return NULL;
    return cli->buffer;
}

static bool is_whitespace(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}

int xf_cli_argc(struct xf_cli *cli, char ***argv)
{
    int pos = 0;
    bool in_arg = false;
    bool in_escape = false;
    char in_string = '\0';
    if (!cli->done)
        return 0;
    for (size_t i = 0; i < sizeof(cli->buffer) && cli->buffer[i] != '\0';
         i++) {

        // If we're escaping this character, just absorb it regardless
        if (in_escape) {
            in_escape = false;
            continue;
        }

        if (in_string) {
            // If we're finishing a string, blank it out
            if (cli->buffer[i] == in_string) {
                memmove(&cli->buffer[i], &cli->buffer[i + 1],
                        sizeof(cli->buffer) - i - 1);
                in_string = '\0';
                i--;
            }
            continue;
        }

        // Skip over whitespace, and replace it with nul terminators so
        // each argv is nul terminated
        if (is_whitespace(cli->buffer[i])) {
            if (in_arg)
                cli->buffer[i] = '\0';
            in_arg = false;
            continue;
        }

        if (!in_arg) {
            if (pos >= XF_CLI_MAX_ARGC) {
                break;
            }
            cli->argv[pos] = &cli->buffer[i];
            pos++;
            in_arg = true;
        }

        if (cli->buffer[i] == '\\') {
            // Absorb the escape character
            memmove(&cli->buffer[i], &cli->buffer[i + 1],
                    sizeof(cli->buffer) - i - 1);
            i--;
            in_escape = true;
        }

        // If we're starting a new string, absorb the character and shuffle
        // things back
        if (cli->buffer[i] == '\'' || cli->buffer[i] == '"') {
            in_string = cli->buffer[i];
            memmove(&cli->buffer[i], &cli->buffer[i + 1],
                    sizeof(cli->buffer) - i - 1);
            i--;
        }
    }
    // Traditionally, there is a NULL entry at argv[argc].
    if (pos >= XF_CLI_MAX_ARGC) {
        pos--;
    }
    cli->argv[pos] = NULL;

    *argv = cli->argv;
    return pos;
}

void xf_cli_prompt(struct xf_cli *cli)
{
    cli_put_prompt(cli);
}
