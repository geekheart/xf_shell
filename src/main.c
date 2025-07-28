#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "xf_console.h"
#include "xf_options.h"

/****************移植对接*******************/
static char getch(void)
{
    return getc(stdin);
}

static void putch(void *data, char ch, bool is_last)
{
    putc(ch, stdout);
}

/****************设置指令回调*******************/
static int test(const xf_cmd_args_t *cmd)
{
    int res = XF_CMD_OK;
    const char *file = NULL;
    int32_t number = 0;
    res = xf_console_cmd_get_string(cmd, "file", &file);
    res = xf_console_cmd_get_int(cmd, "number", &number);

    printf("file: %s\n", file);
    printf("num: %d\n", number);
    return 0;
}

int main(void)
{
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

    while (1) {
        xf_console_cmd_handle(getch);
    }

    return 0;
}