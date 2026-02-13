# 轻量级嵌入式命令行工具

一款的轻量级嵌入式命令行工具.
无依赖,方便移植.

## 功能特色

1. 纯C语言，符合C99语法
2. 无外部库依赖，移植只需要对接方便
3. 自带解析功能，支持int bool string格式的。参数或者选项
4. 内置help命令和history命令，会收集所有的命令规则并做到提示
5. 支持tab补全，支持↑键回顾历史命令（`getc` 必须按“逐字节”返回输入字符，不能是整行缓冲）
6. 支持自定义参数合法值回调，方便更合理的限制用户输入
7. 全静态数据，不用担心内存泄漏
8. 支持内部直接调用命令
9. 可配置的彩色颜色输出宏



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
1. CLI 内核:`src/xf_shell_cli.c`, `src/xf_shell_cli.h`
2. 命令解析器:`src/xf_shell_options.c`, `src/xf_shell_options.h`
3. 命令框架核心:`src/xf_shell_cmd_list.h`, `src/xf_shell.h`, `src/xf_shell.c`
4. 补全与解析子模块:`src/xf_shell_completion.c/.h`, `src/xf_shell_parser.c/.h`


### 对接输入输出

要支持 `↑/↓` 历史浏览、`Tab` 补全等交互，`getc` 必须按“逐字节”返回输入字符（不能是整行缓冲）。

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
    xf_shell_cmd_init("XF_SHELL > ", putch, NULL); // 命令行初始化（自动注册 help/history）
    while (1) {
        xf_shell_cmd_handle(getch); // 调用命令行处理
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
    res = xf_shell_cmd_get_string(cmd, "file", &file);
    res = xf_shell_cmd_get_int(cmd, "number", &number);

    printf("file: %s\n", file);
    printf("num: %d\n", number);
    return 0;
}

static bool validate_number_range(const xf_opt_arg_t *opt, const char **error_msg,
                                  bool *append_help)
{
    if (opt->_opt.integer < 1 || opt->_opt.integer > 100) {
        if (error_msg) {
            *error_msg = "must be in range [1, 100]";
        }
        if (append_help) {
            *append_help = true;
        }
        return false;
    }
    return true;
}

/****************用户静态对象*******************/
static xf_shell_cmd_t cmd = {
    .command = "test",
    .func = test,
    .help = "测试命令",
};

static xf_opt_arg_t opt1 = {
    .long_opt = "file",
    .short_opt = 'f',
    .description = "File to load",
    .type = XF_OPTION_TYPE_STRING,
    .require = false,
};

static xf_opt_arg_t opt2 = {
    .long_opt = "number",
    .short_opt = 'n',
    .description = "Number",
    .type = XF_OPTION_TYPE_INT,
    .require = false,
    .has_default = true,
    .validator = validate_number_range,
    .default_integer = 10,
};
////////////////////////main函数///////////////////////////
/****************注册命令*******************/
xf_shell_cmd_register(&cmd);

/****************注册参数*******************/
xf_shell_cmd_set_opt(&cmd, &opt1);
xf_shell_cmd_set_opt(&cmd, &opt2);
```

> 本文client部分参考了：
> [Embedded CLI](https://github.com/AndreRenaud/EmbeddedCLI)
> 
