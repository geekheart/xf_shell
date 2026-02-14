/**
 * @file xf_shell.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-06-17
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

#ifndef __XF_SHELL_H__
#define __XF_SHELL_H__

/* ==================== [Includes] ========================================== */
#include <stdbool.h>
#include "xf_shell_cmd_list.h"
#include "xf_shell_options.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

#ifndef XF_SHELL_MAX_COMMANDS
#define XF_SHELL_MAX_COMMANDS 24
#endif

#ifndef XF_SHELL_MAX_OPTS_PER_CMD
#define XF_SHELL_MAX_OPTS_PER_CMD 16
#endif

#ifndef XF_SHELL_MAX_MATCHES
#define XF_SHELL_MAX_MATCHES (XF_SHELL_MAX_COMMANDS + (XF_SHELL_MAX_OPTS_PER_CMD * 2))
#endif

#ifndef XF_SHELL_NEWLINE
#if defined(_WIN32) || defined(_WIN64)
#define XF_SHELL_NEWLINE "\r\n"
#else
#define XF_SHELL_NEWLINE "\n"
#endif
#endif

#ifndef XF_SHELL_NEWLINE_IS_CRLF
#define XF_SHELL_NEWLINE_IS_CRLF \
    (XF_SHELL_NEWLINE[0] == '\r' && XF_SHELL_NEWLINE[1] == '\n' && XF_SHELL_NEWLINE[2] == '\0')
#endif

/* ==================== [Typedefs] ========================================== */

typedef enum {
    XF_CMD_OK,
    XF_CMD_NO_MEM,
    XF_CMD_NO_INVALID_ARG,
    XF_CMD_NOT_SUPPORTED,
    XF_CMD_INITED,
} xf_cmd_return_t;

typedef void* xf_cmd_args_t;

/**
 * @brief 控制台字符输出回调类型。
 *
 * @param[in] user_data 用户上下文指针，由初始化接口透传。
 * @param[in] ch 待输出字符。
 * @param[in] is_last 是否为本次发送中的最后一个字符，可用于触发底层 flush。
 */
typedef void (*xf_putc_t)(void* user_data, char ch, bool is_last);

/**
 * @brief 控制台字符输入回调类型。
 *
 * @return 读取到的单个字符。
 */
typedef char (*xf_getc_t)(void);

/**
 * @brief 命令执行回调类型。
 *
 * @param[in] arg 命令运行时上下文（可用于读取参数值）。
 * @return 用户自定义返回值（通常返回 `0` 表示成功）。
 */
typedef int (*xf_shell_cmd_func_t)(const xf_cmd_args_t* arg);

typedef struct _xf_shell_cmd_t xf_shell_cmd_t;
typedef struct _xf_opt_arg_t xf_opt_arg_t;
typedef struct _xf_arg_t xf_arg_t;

/**
 * @brief 选项合法性校验回调类型。
 *
 * @param[in] opt 当前选项对象。
 * @param[out] error_msg 失败时可返回错误文案（可为 `NULL`）。
 * @param[out] append_help 失败时是否追加打印该参数帮助（可为 `NULL`）。
 * @return `true` 表示校验通过；`false` 表示校验失败。
 */
typedef bool (*xf_opt_validator_t)(const xf_opt_arg_t* opt, const char** error_msg, bool* append_help);

/**
 * @brief 位置参数合法性校验回调类型。
 *
 * @param[in] arg 当前位置参数对象。
 * @param[out] error_msg 失败时可返回错误文案（可为 `NULL`）。
 * @param[out] append_help 失败时是否追加打印该参数帮助（可为 `NULL`）。
 * @return `true` 表示校验通过；`false` 表示校验失败。
 */
typedef bool (*xf_arg_validator_t)(const xf_arg_t* arg, const char** error_msg, bool* append_help);

struct _xf_shell_cmd_t {
    const char* command;
    const char* help;
    xf_shell_cmd_func_t func;
    xf_cmd_list_t _node;
    xf_cmd_list_t _opt_list;
    xf_cmd_list_t _arg_list;
};

struct _xf_opt_arg_t {
    const char* long_opt;
    char short_opt;
    const char* description;
    xf_opt_type_t type;
    bool require;
    bool has_default;
    xf_opt_validator_t validator;
    union {
        int32_t default_integer;
        bool default_boolean;
        float default_floating;
        const char* default_string;
    };
    xf_cmd_list_t _node;
    xf_shell_cmd_t* _owner;
    xf_options_t _opt;
};

struct _xf_arg_t {
    const char* name;
    const char* description;
    xf_opt_type_t type;
    bool require;
    bool has_default;
    xf_arg_validator_t validator;
    union {
        int32_t default_integer;
        bool default_boolean;
        float default_floating;
        const char* default_string;
    };
    xf_cmd_list_t _node;
    xf_shell_cmd_t* _owner;
    xf_options_t _opt;
};


/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化命令控制台子系统。
 *
 * @details
 * 会初始化 CLI 内核、注册内建 `help/history` 命令并输出首个提示符。
 *
 * @param[in] prompt 命令提示符文本。
 * @param[in] putc 字符输出回调。
 * @param[in] user_data 传递给 `putc` 的用户上下文。
 */
void xf_shell_cmd_init(const char* prompt, xf_putc_t putc, void* user_data);

/**
 * @brief 处理一次字符输入驱动流程。
 *
 * @details
 * 内部会读取 1 个字符并更新编辑状态；当检测到完整命令行时自动完成解析与执行。
 *
 * @param[in] getc 字符输入回调。
 */
void xf_shell_cmd_handle(xf_getc_t getc);

/**
 * @brief 注册命令对象到控制台。
 *
 * @param[in,out] cmd 用户提供的静态命令对象。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_register(xf_shell_cmd_t* cmd);

/**
 * @brief 从控制台注销命令对象。
 *
 * @details
 * 注销时会同时解除该命令下已挂载的选项与位置参数。
 *
 * @param[in,out] cmd 待注销命令对象。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_unregister(xf_shell_cmd_t* cmd);

/**
 * @brief 为命令挂载一个选项参数。
 *
 * @param[in,out] cmd 目标命令对象。
 * @param[in,out] arg 用户提供的静态选项对象。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_set_opt(xf_shell_cmd_t* cmd, xf_opt_arg_t* arg);

/**
 * @brief 从命令中卸载一个选项参数。
 *
 * @param[in,out] cmd 目标命令对象。
 * @param[in,out] arg 待卸载选项对象。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_unset_opt(xf_shell_cmd_t* cmd, xf_opt_arg_t* arg);

/**
 * @brief 为命令挂载一个位置参数。
 *
 * @param[in,out] cmd 目标命令对象。
 * @param[in,out] arg 用户提供的静态位置参数对象。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_set_arg(xf_shell_cmd_t* cmd, xf_arg_t* arg);

/**
 * @brief 从命令中卸载一个位置参数。
 *
 * @param[in,out] cmd 目标命令对象。
 * @param[in,out] arg 待卸载位置参数对象。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_unset_arg(xf_shell_cmd_t* cmd, xf_arg_t* arg);

/**
 * @brief 按名称读取 `int32_t` 类型参数值。
 *
 * @param[in] cmd 命令运行时上下文。
 * @param[in] long_opt 参数名（支持选项长名与位置参数名）。
 * @param[out] value 输出整数值。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_get_int(const xf_cmd_args_t* cmd, const char* long_opt, int32_t* value);

/**
 * @brief 按名称读取 `bool` 类型参数值。
 *
 * @param[in] cmd 命令运行时上下文。
 * @param[in] long_opt 参数名（支持选项长名与位置参数名）。
 * @param[out] value 输出布尔值。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_get_bool(const xf_cmd_args_t* cmd, const char* long_opt, bool* value);

/**
 * @brief 按名称读取 `float` 类型参数值。
 *
 * @param[in] cmd 命令运行时上下文。
 * @param[in] long_opt 参数名（支持选项长名与位置参数名）。
 * @param[out] value 输出浮点值。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_get_float(const xf_cmd_args_t* cmd, const char* long_opt, float* value);

/**
 * @brief 按名称读取字符串参数值。
 *
 * @param[in] cmd 命令运行时上下文。
 * @param[in] long_opt 参数名（支持选项长名与位置参数名）。
 * @param[out] value 输出字符串指针（不拷贝）。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_get_string(const xf_cmd_args_t* cmd, const char* long_opt, const char** value);

/**
 * @brief 直接执行一次命令解析与分发。
 *
 * @param[in] argc 参数数量。
 * @param[in] argv 参数数组，`argv[0]` 为命令名。
 * @return `xf_cmd_return_t` 状态码。
 */
int xf_shell_cmd_run(int argc, const char** argv);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // __XF_SHELL_H__
