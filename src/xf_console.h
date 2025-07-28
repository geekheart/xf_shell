/**
 * @file xf_console.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-06-17
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

#ifndef __XF_CONSOLE_H__
#define __XF_CONSOLE_H__

/* ==================== [Includes] ========================================== */
#include <stdbool.h>
#include "xf_options.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef enum {
    XF_CMD_OK,
    XF_CMD_NO_MEM,
    XF_CMD_NO_INVALID_ARG,
    XF_CMD_NOT_SUPPORTED,
    XF_CMD_INITED,
} xf_cmd_return_t;

typedef void *xf_cmd_args_t;

typedef void (*xf_putc_t)(void *user_data, char ch, bool is_last);
typedef char (*xf_getc_t)(void);
typedef int (*xf_console_cmd_func_t)(const xf_cmd_args_t *arg);

typedef struct _xf_console_cmd_t {
    const char *command;
    const char *help;
    xf_console_cmd_func_t func;
} xf_console_cmd_t;

typedef struct _xf_opt_arg_t {
    const char *long_opt;
    char short_opt;
    const char *description;
    xf_opt_type_t type;
    bool require;
} xf_opt_arg_t;


/* ==================== [Global Prototypes] ================================= */

void xf_console_cmd_init(const char *prompt, xf_putc_t putc, void *user_data);
void xf_console_cmd_handle(xf_getc_t getc);
int xf_console_cmd_register(const xf_console_cmd_t *cmd);
int xf_console_cmd_unregister(const char *command);
int xf_console_cmd_set_opt(const char *command, xf_opt_arg_t arg);
int xf_console_cmd_unset_opt(const char *command, const char *long_opt);
int xf_console_cmd_get_int(const xf_cmd_args_t *cmd, const char *long_opt,
                           int32_t *value);
int xf_console_cmd_get_bool(const xf_cmd_args_t *cmd, const char *long_opt,
                            bool *value);
int xf_console_cmd_get_float(const xf_cmd_args_t *cmd, const char *long_opt,
                             float *value);
int xf_console_cmd_get_string(const xf_cmd_args_t *cmd,
                              const char *long_opt, const char **value);
int xf_console_cmd_run(int argc, const char **argv);
void xf_console_register_help_cmd(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __XF_CONSOLE_H__
