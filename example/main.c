#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

#include "xf_console.h"
#include "xf_options.h"

/****************移植对接*******************/
static bool s_termios_enabled = false;
static struct termios s_termios_old;
static volatile sig_atomic_t s_should_exit = 0;

static void handle_exit_signal(int signo)
{
    (void)signo;
    s_should_exit = 1;
}

static void setup_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_exit_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);
}

static void restore_terminal_mode(void)
{
    if (s_termios_enabled) {
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_termios_old);
        s_termios_enabled = false;
    }
}

static void enable_terminal_char_mode(void)
{
    struct termios raw;

    if (!isatty(STDIN_FILENO)) {
        return;
    }
    if (tcgetattr(STDIN_FILENO, &s_termios_old) != 0) {
        return;
    }

    raw = s_termios_old;
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                     ICRNL | IXON);
    raw.c_oflag &= ~OPOST;
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
    raw.c_cflag &= ~(CSIZE | PARENB);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return;
    }

    s_termios_enabled = true;
    atexit(restore_terminal_mode);
}

static char getch(void)
{
    char ch = '\0';
    if (s_termios_enabled) {
        if (read(STDIN_FILENO, &ch, 1) == 1) {
            return ch;
        }
        return '\0';
    }

    int c = getc(stdin);
    if (c == EOF) {
        return '\0';
    }
    return (char)c;
}

static void putch(void *data, char ch, bool is_last)
{
    (void)data;
    putc(ch, stdout);
    if (is_last) {
        fflush(stdout);
    }
}

/****************设置指令回调*******************/
static int test(const xf_cmd_args_t *cmd)
{
    int res = XF_CMD_OK;
    const char *file = NULL;
    int32_t number = 0;
    res = xf_console_cmd_get_string(cmd, "file", &file);
    res = xf_console_cmd_get_int(cmd, "number", &number);

    printf("file: %s\n\r", file);
    printf("num: %d\n\r", number);
    return 0;
}

int main(void)
{
    setup_signal_handlers();
    enable_terminal_char_mode();

    /****************注册命令*******************/
    xf_console_cmd_t cmd;
    cmd.command = "test";
    cmd.func = test;
    cmd.help = "测试命令";

    xf_console_cmd_register(&cmd);

    /****************注册参数*******************/

    xf_opt_arg_t opt1 = {"file", 'f', "File to load", XF_OPTION_TYPE_STRING, false};
    xf_opt_arg_t opt2 = {"number", 'n', "Number", XF_OPTION_TYPE_INT, false};


    xf_console_cmd_set_opt("test", opt1);
    xf_console_cmd_set_opt("test", opt2);

    /****************注册help指令*******************/

    xf_console_register_help_cmd();

    /****************初始化及调用*******************/

    xf_console_cmd_init("POSIX> ", putch, NULL);

    while (!s_should_exit) {
        xf_console_cmd_handle(getch);
    }

    return 130;
}
