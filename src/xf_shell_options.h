/**
 * @file xf_shell_options.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

#ifndef __XF_SHELL_OPTIONS_H__
#define __XF_SHELL_OPTIONS_H__

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
    uint32_t type:6; // xf_opt_type_t
    uint32_t require:1;
    uint32_t has_default:1; // Whether a default value is configured
    uint32_t provided:1; // Whether value was provided by user in current parse
    uint32_t reserved:15;
    union {
        int32_t integer; // Pointers to where the option values should be stored
        bool boolean;
        float floating;
        const char *string;
    };
} xf_options_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 在参数列表中查找并解析指定选项。
 *
 * @details
 * 支持以下格式：
 * - 长选项：`--name value` / `--name=value`
 * - 短选项：`-n value` / `-n=value`
 *
 * 若找到目标选项，会完成类型转换并写入 `options` 的值字段。
 *
 * @param[in,out] options 选项描述与结果存储对象。
 * @param[in] argc 参数个数。
 * @param[in] argv 参数数组。
 *
 * @return `XF_OPTION_OK` 或对应错误码（如未找到、类型错误等）。
 */
int xf_opt_parse(xf_options_t *options, int argc, const char **argv);

/**
 * @brief 将单个字符串按选项类型解析并写入结果。
 *
 * @details
 * 本函数只做“值解析”，不参与 `argv` 中的参数匹配流程，
 * 适用于位置参数等场景的类型转换复用。
 *
 * @param[in,out] options 选项对象（其 `type` 决定解析方式）。
 * @param[in] val 待解析的字符串值。
 *
 * @return `XF_OPTION_OK` 或对应错误码。
 */
int xf_opt_parse_value(xf_options_t *options, const char *val);

/**
 * @brief 生成单个选项的帮助文本。
 *
 * @details
 * 会根据选项定义输出短/长选项名、描述、默认值、必填标记等信息。
 *
 * @param[in] options 选项对象。
 * @param[out] msg 输出缓冲区（需由调用方保证容量足够）。
 *
 * @return 写入字符数（不含字符串结尾 `\0`）。
 */
size_t xf_opt_parse_usage(xf_options_t *options, char* msg);

/**
 * @brief 将一行命令文本按空白字符拆分为参数数组。
 *
 * @details
 * 该函数会直接在输入字符串上写入 `\0` 分隔符，因此 `line` 内容会被修改。
 *
 * @param[in,out] line 待拆分的命令行缓冲区。
 * @param[out] output 输出参数指针数组。
 * @param[in] max_items `output` 的最大可写项数。
 *
 * @return 实际拆分出的参数数量。
 */
int xf_opt_parse_split_string(char *line, const char **output, int max_items);
/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __XF_SHELL_OPTIONS_H__
