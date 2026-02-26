/**
 * @file xf_shell_completion_context.h
 * @brief Internal parser for completion cursor context.
 */

#ifndef __XF_SHELL_COMPLETION_CONTEXT_H__
#define __XF_SHELL_COMPLETION_CONTEXT_H__

/* ==================== [Includes] ========================================== */
#include "xf_shell_completion.h"
#include "xf_shell.h"
#include "xf_shell_cli.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Typedefs] ========================================== */

#if XF_SHELL_COMPLETION_ENABLE && XF_SHELL_COMPLETION_ENABLE_VALUE

typedef struct {
    xf_opt_arg_t* inline_opt;
    int inline_value_start;
    xf_opt_arg_t* pending_opt;
    xf_arg_t* positional_arg;
} xf_shell_completion_context_t;

/* ==================== [Global Prototypes] ================================= */

void xf_shell_completion_resolve_context(const struct xf_cli* cli,
                                         xf_shell_cmd_t* cmd,
                                         int cursor,
                                         xf_shell_completion_context_t* out);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // __XF_SHELL_COMPLETION_CONTEXT_H__
