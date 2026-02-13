# 轻量级嵌入式命令行工具

一款基于[Embedded CLI](https://github.com/AndreRenaud/EmbeddedCLI)的轻量级嵌入式命令行工具.
无依赖,方便移植.

## 如何运行?
1. clone 本仓库
```shell
git clone https://github.com/geekheart/xf_shell.git
```
2. 安装 xmake 
```shell
sudo add-apt-repository ppa:xmake-io/xmake
sudo apt update
sudo apt install xmake
```
3. 编译运行例程
```shell
xmake b
xmake r
```

## 如何移植?

### 所需文件
1. CLI 内核(基于 Embedded CLI 二次封装):`src/xf_cli.c`, `src/xf_cli.h`
2. 命令解析器:`src/xf_options.c`, `src/xf_options.h`
3. `src/xf_cmd_list.h`,`src/xf_console.h`,`src/xf_console.c`（已内置 Tab 补全）

## 高级功能（已集成）

- `Tab` 补全命令名
- `Tab` 补全选项（`--long-opt` 与 `-s`）
- 多候选时输出建议列表并重绘当前输入行

## 颜色输出宏

在 `src/xf_cli.h` 中可配置：

- `XF_CLI_COLORFUL`: `0/1`，是否启用 ANSI 彩色
- `XF_CLI_PROMPT_COLOR`: 提示符颜色（默认红色）
- `XF_CLI_COMMAND_COLOR`: 命令输入颜色（默认绿色）
- `XF_CLI_COLOR_RESET`: 颜色复位

### 对接输入输出

```c
/****************移植对接*******************/
static char getch(void)
{
    return getc(stdin);
}

static void putch(void *data, char ch, bool is_last)
{
    putc(ch, stdout);
}

int main()
{
    xf_console_cmd_init("POSIX> ", putch, NULL); // 命令行初始化
    while (1) {
        xf_console_cmd_handle(getch); // 调用命令行处理
    }

    return 0;
}
```

### 注册自己的命令
```c
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
////////////////////////main函数///////////////////////////
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
```
