/**
 * @file xf_options.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

#ifndef __XF_OPTIONS_H__
#define __XF_OPTIONS_H__

/* ==================== [Includes] ========================================== */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */
typedef enum _xf_opt_type_t{
	XF_OPTION_TYPE_NONE     = 0,
	XF_OPTION_TYPE_BOOL     = 1, // Takes boolean parameter
	XF_OPTION_TYPE_INT      = 2, // Takes integer parameter
	XF_OPTION_TYPE_STRING   = 3, // Takes string parameter
    XF_OPTION_TYPE_FLOAT    = 4, // Takes a float parameter
}xf_opt_type_t;

enum {
    XF_OPTION_OK,
    XF_OPTION_ERR_NO_MEM,
    XF_OPTION_ERR_INVALID_ARG,
    XF_OPTION_ERR_HELP,
    XF_OPTION_ERR_NO_FOUND,
    XF_OPTION_ERR_FLAG,
};


typedef struct _xf_options_t {
    const char *long_opt; // Name of the option when used in the long format
    const char *description; // Description for the usage/help message
    uint32_t short_opt:8; // Single character short option
    uint32_t type:23; // Flags (see OPTION_FLAG_xxx)
    uint32_t require:1;
    union {
        int32_t integer; // Pointers to where the option values should be stored
        bool boolean;
        float floating;
        const char *string;
    };
} xf_options_t;

/* ==================== [Global Prototypes] ================================= */
int xf_opt_parse(xf_options_t *options, int argc, const char **argv);
size_t xf_opt_parse_usage(xf_options_t *options, char* msg);
int xf_opt_parse_split_string(char *line, const char **output, int max_items);
/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __XF_OPTIONS_H__
