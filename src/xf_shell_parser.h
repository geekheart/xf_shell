/**
 * @file xf_shell_parser.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026, CorAL. All rights reserved.
 *
 */

#ifndef __XF_SHELL_PARSER_H__
#define __XF_SHELL_PARSER_H__

/* ==================== [Includes] ========================================== */
#include <stdint.h>
#include "xf_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 解析模块的文本输出回调类型。
 *
 * @details
 * 解析器在输出错误信息、参数帮助信息时，会通过此回调把字符串写回调用方。
 *
 * @param[in] ctx 用户上下文指针，由调用方透传。
 * @param[in] s 以 `\0` 结尾的输出字符串。
 */
typedef void (*xf_shell_parser_puts_t)(void* ctx, const char* s);

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 同步并重置一个选项对象的运行时字段。
 *
 * @details
 * 会将注册期字段（名称、类型、默认值、必填标志等）写入 `_opt`，
 * 并清空 `provided` 状态，为新一轮解析做准备。
 *
 * @param[in,out] opt 目标选项对象。
 */
void xf_shell_parser_sync_opt_runtime(xf_opt_arg_t* opt);

/**
 * @brief 同步并重置一个位置参数对象的运行时字段。
 *
 * @details
 * 会将注册期字段（名称、类型、默认值、必填标志等）写入 `_opt`，
 * 并清空 `provided` 状态，为新一轮解析做准备。
 *
 * @param[in,out] arg 目标位置参数对象。
 */
void xf_shell_parser_sync_arg_runtime(xf_arg_t* arg);

/**
 * @brief 执行指定命令的参数解析、校验与回调调用。
 *
 * @details
 * 该函数会按顺序完成：
 * 1. 内建帮助参数处理（`-h/--help`）；
 * 2. 选项合法性检查与值解析；
 * 3. 位置参数收集、解析与校验；
 * 4. 若全部成功，调用命令回调函数。
 *
 * @param[in,out] cmd 目标命令对象。
 * @param[in] argc 参数个数（含命令名）。
 * @param[in] argv 参数数组（`argv[0]` 为命令名）。
 * @param[in] puts_fn 输出回调，可为 `NULL`（此时不输出文本）。
 * @param[in] puts_ctx 输出回调上下文指针。
 *
 * @return `xf_cmd_return_t` 对应的状态码。
 */
int xf_shell_parser_run(xf_shell_cmd_t* cmd, int argc, const char** argv, xf_shell_parser_puts_t puts_fn, void* puts_ctx);

/**
 * @brief 按名称读取 `int32_t` 类型参数值（支持选项与位置参数）。
 *
 * @param[in] cmd 命令运行时上下文。
 * @param[in] long_opt 参数名（长名/位置参数名）。
 * @param[out] value 返回读取到的整数值。
 *
 * @return `XF_CMD_OK` 表示读取成功；否则返回对应错误码。
 */
int xf_shell_parser_get_int(const xf_cmd_args_t* cmd, const char* long_opt, int32_t* value);

/**
 * @brief 按名称读取 `bool` 类型参数值（支持选项与位置参数）。
 *
 * @param[in] cmd 命令运行时上下文。
 * @param[in] long_opt 参数名（长名/位置参数名）。
 * @param[out] value 返回读取到的布尔值。
 *
 * @return `XF_CMD_OK` 表示读取成功；否则返回对应错误码。
 */
int xf_shell_parser_get_bool(const xf_cmd_args_t* cmd, const char* long_opt, bool* value);

/**
 * @brief 按名称读取 `float` 类型参数值（支持选项与位置参数）。
 *
 * @param[in] cmd 命令运行时上下文。
 * @param[in] long_opt 参数名（长名/位置参数名）。
 * @param[out] value 返回读取到的浮点值。
 *
 * @return `XF_CMD_OK` 表示读取成功；否则返回对应错误码。
 */
int xf_shell_parser_get_float(const xf_cmd_args_t* cmd, const char* long_opt, float* value);

/**
 * @brief 按名称读取字符串类型参数值（支持选项与位置参数）。
 *
 * @param[in] cmd 命令运行时上下文。
 * @param[in] long_opt 参数名（长名/位置参数名）。
 * @param[out] value 返回读取到的字符串指针（不拷贝内存）。
 *
 * @return `XF_CMD_OK` 表示读取成功；否则返回对应错误码。
 */
int xf_shell_parser_get_string(const xf_cmd_args_t* cmd, const char* long_opt, const char** value);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // __XF_SHELL_PARSER_H__
