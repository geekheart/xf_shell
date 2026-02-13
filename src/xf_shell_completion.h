/**
 * @file xf_shell_completion.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026, CorAL. All rights reserved.
 *
 */

#ifndef __XF_SHELL_COMPLETION_H__
#define __XF_SHELL_COMPLETION_H__

/* ==================== [Includes] ========================================== */
#include <stdbool.h>
#include "xf_shell_cli.h"
#include "xf_shell_cmd_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 处理 `Tab` 自动补全按键事件。
 *
 * @details
 * 该函数会根据当前光标位置判断补全场景：
 * - 若位于第一个 token，则补全命令名；
 * - 若位于后续 token 且前缀以 `-` 开头，则补全该命令的选项名。
 *
 * 同时函数会在需要时输出候选列表，并重绘当前输入行。
 *
 * @param[in,out] cli CLI 状态对象，会被就地修改（buffer/cursor/len）。
 * @param[in] cmd_list 已注册命令链表头节点。
 * @param[in,out] matches 候选项缓存，用于暂存补全匹配结果。
 * @param[in] max_matches `matches` 可容纳的最大候选数量。
 *
 * @return
 * - `true`：本次按键已被该函数处理（无论是否实际补全成功）；
 * - `false`：参数非法，未处理该按键。
 */
bool xf_shell_completion_handle_tab(struct xf_cli *cli,
                                      xf_cmd_list_t *cmd_list,
                                      char matches[][XF_CLI_MAX_LINE],
                                      int max_matches);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // __XF_SHELL_COMPLETION_H__
