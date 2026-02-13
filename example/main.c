#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "xf_shell.h"
#include "xf_shell_options.h"

/****************移植对接*******************/
static bool s_termios_enabled = false;
static struct termios s_termios_old;
static volatile sig_atomic_t s_should_exit = 0;

static void handle_exit_signal(int signo) {
    (void)signo;
    s_should_exit = 1;
}

static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_exit_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);
}

static void restore_terminal_mode(void) {
    if (s_termios_enabled) {
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_termios_old);
        s_termios_enabled = false;
    }
}

static void enable_terminal_char_mode(void) {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) {
        return;
    }
    if (tcgetattr(STDIN_FILENO, &s_termios_old) != 0) {
        return;
    }

    raw = s_termios_old;
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
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

static char getch(void) {
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

static void putch(void* data, char ch, bool is_last) {
    (void)data;
    putc(ch, stdout);
    if (is_last) {
        fflush(stdout);
    }
}

/****************设置指令回调*******************/
static int test(const xf_cmd_args_t* cmd) {
    int res = XF_CMD_OK;
    const char* input = NULL;
    const char* file = NULL;
    int32_t number = 0;
    bool bool_val = false;
    res = xf_shell_cmd_get_string(cmd, "input", &input);
    if (res != XF_CMD_OK) {
        input = NULL;
    }
    res = xf_shell_cmd_get_string(cmd, "file", &file);
    res = xf_shell_cmd_get_int(cmd, "number", &number);
    res = xf_shell_cmd_get_bool(cmd, "bool", &bool_val);

    printf("input: %s\n\r", input);
    printf("file: %s\n\r", file);
    printf("num: %d\n\r", number);
    printf("bool: %d\n\r", bool_val);
    return 0;
}

static bool validate_number_range(const xf_opt_arg_t* opt, const char** error_msg,
                                  bool* append_help)
{
    if (opt->_opt.integer < 1 || opt->_opt.integer > 100) {
        if (error_msg != NULL) {
            *error_msg = "must be in range [1, 100]";
        }
        if (append_help != NULL) {
            *append_help = true;
        }
        return false;
    }
    return true;
}

static xf_shell_cmd_t s_test_cmd = {
    .command = "test",
    .func = test,
    .help = "测试命令",
};

static xf_opt_arg_t s_opt_file = {
    .long_opt = "file",
    .short_opt = 'f',
    .description = "File to load",
    .type = XF_OPTION_TYPE_STRING,
    .require = false,
};

static xf_arg_t s_arg_input = {
    .name = "input",
    .description = "Input positional string",
    .type = XF_OPTION_TYPE_STRING,
    .require = false,
};

static xf_opt_arg_t s_opt_number = {
    .long_opt = "number",
    .short_opt = 'n',
    .description = "Number",
    .type = XF_OPTION_TYPE_INT,
    .require = false,
    .has_default = true,
    .validator = validate_number_range,
    .default_integer = 10,
};

static xf_opt_arg_t s_opt_bool = {
    .long_opt = "bool",
    .short_opt = 'b',
    .description = "Boolean flag",
    .type = XF_OPTION_TYPE_BOOL,
    .require = false,
    .has_default = true,
    .default_boolean= 0,
};

int main(void) {
    setup_signal_handlers();
    enable_terminal_char_mode();

    /****************注册命令*******************/
    xf_shell_cmd_register(&s_test_cmd);

    /****************注册参数*******************/
    xf_shell_cmd_set_arg(&s_test_cmd, &s_arg_input);
    xf_shell_cmd_set_opt(&s_test_cmd, &s_opt_file);
    xf_shell_cmd_set_opt(&s_test_cmd, &s_opt_number);
    xf_shell_cmd_set_opt(&s_test_cmd, &s_opt_bool);

    /****************初始化及调用*******************/

    xf_shell_cmd_init("XF_SHELL > ", putch, NULL);

    while (!s_should_exit) {
        xf_shell_cmd_handle(getch);
    }

    return 130;
}
