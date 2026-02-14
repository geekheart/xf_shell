/**
 * @file xf_shell_options.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */
#include "xf_shell_options.h"
#include "xf_shell.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static int set_param(xf_options_t *options, const char *val);
static bool is_whitespace(char ch);

/* ==================== [Static Variables] ================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

int xf_opt_parse(xf_options_t *options, int argc, const char **argv)
{
    int swap_pos = argc - 1;
    if (!argv || !argc) {
        return XF_OPTION_ERR_INVALID_ARG;
    }

    if (!options || options->type == XF_OPTION_TYPE_NONE) {
        return XF_OPTION_ERR_FLAG;
    }

    options->provided = 0U;

    for (int i = 1; i <= swap_pos; i++) {
        const char *arg = argv[i];
        if (!arg) {
            continue;
        }
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            return XF_OPTION_ERR_HELP;
        }
        if (arg[0] == '-' && arg[1] == '-') {
            char *end = strchr(arg, '=');
            int len = strlen(&arg[2]);
            if (end) {
                len = end - arg - 2;
            }
            if (options->long_opt == NULL) {
                continue;
            }
            if (strncmp(options->long_opt, &arg[2], len) != 0 ||
                    options->long_opt[len] != '\0') {
                continue;
            }

            if (end) {
                int ret = set_param(options, end + 1);
                if (ret == XF_OPTION_OK) {
                    options->provided = 1U;
                }
                return ret;
            } else if (i < argc - 1) {
                int ret = set_param(options, argv[++i]);
                if (ret == XF_OPTION_OK) {
                    options->provided = 1U;
                }
                return ret;
            } else {
                return XF_OPTION_ERR_INVALID_ARG;
            }
        } else if (arg[0] == '-') {
            if (options->short_opt != arg[1]) {
                continue;
            }

            if (arg[2] == '=') {
                int ret = set_param(options, &arg[3]);
                if (ret == XF_OPTION_OK) {
                    options->provided = 1U;
                }
                return ret;
            } else if (i < argc - 1) {
                int ret = set_param(options, argv[++i]);
                if (ret == XF_OPTION_OK) {
                    options->provided = 1U;
                }
                return ret;
            } else {
                return XF_OPTION_ERR_INVALID_ARG;
            }
        }
    }

    return XF_OPTION_ERR_NO_FOUND;
}

int xf_opt_parse_value(xf_options_t *options, const char *val)
{
    if (options == NULL) {
        return XF_OPTION_ERR_INVALID_ARG;
    }
    return set_param(options, val);
}

size_t xf_opt_parse_usage(xf_options_t *options, char *msg)
{
    int count = 0;
    // count = sprintf(msg, "\t-h, --help\tDisplay this help message\n");
    count += sprintf(msg + count, "\t");
    if (options->short_opt) {
        count += sprintf(msg + count, "-%c", options->short_opt);
    }
    if (options->long_opt) {
        count += sprintf(msg + count, "%s--%s", options->short_opt ? ", " : "    ",
                         options->long_opt);
    }
    count += sprintf(msg + count, "\t%s",
                     options->description ? options->description : "");

    if (options->has_default) {
        switch (options->type) {
        case XF_OPTION_TYPE_INT:
            count += sprintf(msg + count, " [default: %d]", options->integer);
            break;
        case XF_OPTION_TYPE_STRING:
            count += sprintf(msg + count, " [default: \"%s\"]",
                             options->string ? options->string : "");
            break;
        case XF_OPTION_TYPE_BOOL:
            count += sprintf(msg + count, " [default: %s]",
                             options->boolean ? "true" : "false");
            break;
        case XF_OPTION_TYPE_FLOAT:
            count += sprintf(msg + count, " [default: %f]", options->floating);
            break;
        default:
            break;
        }
    }

    if (options->require) {
        count += sprintf(msg + count, "%s[required]",
                         (options->type != XF_OPTION_TYPE_NONE
                          || options->description) ? " " : "");
    }
    count += sprintf(msg + count, XF_SHELL_NEWLINE);

    return count;
}

int xf_opt_parse_split_string(char *line, const char **output, int max_items)
{
    char *pos = line;
    int nitems = 0;

    while (pos && *pos && nitems < max_items) {
        while (is_whitespace(*pos) && *pos) {
            pos++;
        }

        if (!*pos) {
            break;
        }
        output[nitems++] = pos;
        while (!is_whitespace(*pos) && *pos) {
            pos++;
        }
        if (!*pos) {
            break;
        }
        *pos = '\0';
        pos++;
    }

    return nitems;
}

/* ==================== [Static Functions] ================================== */


static int set_param(xf_options_t *options, const char *val)
{
    char *end = NULL;

    if (val == NULL) {
        return XF_OPTION_ERR_INVALID_ARG;
    }

    switch (options->type) {
    case XF_OPTION_TYPE_BOOL:
        if (strcasecmp(val, "true") == 0 || strcasecmp(val, "t") == 0 ||
                strcasecmp(val, "yes") == 0 || strcasecmp(val, "on") == 0 ||
                strcasecmp(val, "enable") == 0 || strcmp(val, "1") == 0) {
            options->boolean = true;
            return XF_OPTION_OK;
        }
        if (strcasecmp(val, "false") == 0 || strcasecmp(val, "f") == 0 ||
                strcasecmp(val, "no") == 0 || strcasecmp(val, "off") == 0 ||
                strcasecmp(val, "disable") == 0 || strcmp(val, "0") == 0) {
            options->boolean = false;
            return XF_OPTION_OK;
        }
        return XF_OPTION_ERR_INVALID_ARG;
    case XF_OPTION_TYPE_INT:
    {
        long parsed;
        errno = 0;
        parsed = strtol(val, &end, 0);
        if (errno == ERANGE || end == val || (end != NULL && *end != '\0') ||
                parsed < INT32_MIN || parsed > INT32_MAX) {
            return XF_OPTION_ERR_INVALID_ARG;
        }
        options->integer = (int32_t)parsed;
        return XF_OPTION_OK;
    }
    case XF_OPTION_TYPE_STRING:
        options->string = val;
        return XF_OPTION_OK;
    case XF_OPTION_TYPE_FLOAT:
    {
        float parsed;
        errno = 0;
        parsed = strtof(val, &end);
        if (errno == ERANGE || end == val || (end != NULL && *end != '\0')) {
            return XF_OPTION_ERR_INVALID_ARG;
        }
        options->floating = parsed;
        return XF_OPTION_OK;
    }
    default:
        return XF_OPTION_ERR_FLAG;
    }

    return XF_OPTION_ERR_INVALID_ARG;
}

static bool is_whitespace(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}
