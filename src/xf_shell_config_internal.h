#ifndef __XF_SHELL_CONFIG_INTERNAL_H__
#define __XF_SHELL_CONFIG_INTERNAL_H__

/* Load user overrides first. */
#if defined(__has_include)
#if __has_include(<xf_shell_config.h>)
#include <xf_shell_config.h>
#elif __has_include("xf_shell_config.h")
#include "xf_shell_config.h"
#endif
#else
#include "xf_shell_config.h"
#endif

/* Lightweight profile for constrained targets. */
#ifndef XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_PROFILE_MIN_SIZE 0
#endif

#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_COMPLETION_ENABLE 0
#endif

/* Tab completion master switch. */
#ifndef XF_SHELL_COMPLETION_ENABLE
#define XF_SHELL_COMPLETION_ENABLE 1
#endif

/* Option-name completion (`-x`, `--name`). */
#ifndef XF_SHELL_COMPLETION_ENABLE_OPTION
#if !XF_SHELL_COMPLETION_ENABLE
#define XF_SHELL_COMPLETION_ENABLE_OPTION 0
#elif XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_COMPLETION_ENABLE_OPTION 0
#else
#define XF_SHELL_COMPLETION_ENABLE_OPTION 1
#endif
#endif

/* Value completion (positional/option candidates). */
#ifndef XF_SHELL_COMPLETION_ENABLE_VALUE
#if !XF_SHELL_COMPLETION_ENABLE
#define XF_SHELL_COMPLETION_ENABLE_VALUE 0
#elif XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_COMPLETION_ENABLE_VALUE 0
#else
#define XF_SHELL_COMPLETION_ENABLE_VALUE 1
#endif
#endif

/* Candidate list output for multiple matches. */
#ifndef XF_SHELL_COMPLETION_ENABLE_SUGGESTIONS
#if !XF_SHELL_COMPLETION_ENABLE
#define XF_SHELL_COMPLETION_ENABLE_SUGGESTIONS 0
#elif XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_COMPLETION_ENABLE_SUGGESTIONS 0
#else
#define XF_SHELL_COMPLETION_ENABLE_SUGGESTIONS 1
#endif
#endif

/* Parser built-in `-h/--help` and detailed usage/error text. */
#ifndef XF_SHELL_PARSER_HELP_ENABLE
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_PARSER_HELP_ENABLE 0
#else
#define XF_SHELL_PARSER_HELP_ENABLE 1
#endif
#endif

/* Maximum command count in registry. */
#ifndef XF_SHELL_MAX_COMMANDS
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_MAX_COMMANDS 12
#else
#define XF_SHELL_MAX_COMMANDS 24
#endif
#endif

/* Maximum options per command. */
#ifndef XF_SHELL_MAX_OPTS_PER_CMD
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_MAX_OPTS_PER_CMD 8
#else
#define XF_SHELL_MAX_OPTS_PER_CMD 16
#endif
#endif

/* Maximum positional arguments per command. */
#ifndef XF_SHELL_MAX_ARGS_PER_CMD
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_MAX_ARGS_PER_CMD 8
#else
#define XF_SHELL_MAX_ARGS_PER_CMD 16
#endif
#endif

/* Maximum completion candidates for value completion. */
#ifndef XF_SHELL_MAX_VALUE_MATCHES
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_SHELL_MAX_VALUE_MATCHES XF_SHELL_MAX_COMMANDS
#else
#define XF_SHELL_MAX_VALUE_MATCHES \
    (XF_SHELL_MAX_COMMANDS + (XF_SHELL_MAX_OPTS_PER_CMD * 2))
#endif
#endif

/* Global match buffer size for completion. */
#ifndef XF_SHELL_MAX_MATCHES
#if XF_SHELL_COMPLETION_ENABLE_OPTION && XF_SHELL_COMPLETION_ENABLE_VALUE
#define XF_SHELL_MAX_MATCHES \
    (((XF_SHELL_MAX_COMMANDS + (XF_SHELL_MAX_OPTS_PER_CMD * 2)) > XF_SHELL_MAX_VALUE_MATCHES) ? \
     (XF_SHELL_MAX_COMMANDS + (XF_SHELL_MAX_OPTS_PER_CMD * 2)) : XF_SHELL_MAX_VALUE_MATCHES)
#elif XF_SHELL_COMPLETION_ENABLE_OPTION
#define XF_SHELL_MAX_MATCHES (XF_SHELL_MAX_COMMANDS + (XF_SHELL_MAX_OPTS_PER_CMD * 2))
#elif XF_SHELL_COMPLETION_ENABLE_VALUE
#define XF_SHELL_MAX_MATCHES \
    ((XF_SHELL_MAX_COMMANDS > XF_SHELL_MAX_VALUE_MATCHES) ? \
     XF_SHELL_MAX_COMMANDS : XF_SHELL_MAX_VALUE_MATCHES)
#else
#define XF_SHELL_MAX_MATCHES XF_SHELL_MAX_COMMANDS
#endif
#endif

/* CLI line input buffer size. */
#ifndef XF_CLI_MAX_LINE
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_CLI_MAX_LINE 64
#else
#define XF_CLI_MAX_LINE 120
#endif
#endif

/* CLI history storage bytes, 0 to disable history. */
#ifndef XF_CLI_HISTORY_LEN
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_CLI_HISTORY_LEN 0
#else
#define XF_CLI_HISTORY_LEN 1000
#endif
#endif

/* Maximum argc parsed from a command line. */
#ifndef XF_CLI_MAX_ARGC
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_CLI_MAX_ARGC 8
#else
#define XF_CLI_MAX_ARGC 16
#endif
#endif

/* Maximum prompt string length. */
#ifndef XF_CLI_MAX_PROMPT_LEN
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_CLI_MAX_PROMPT_LEN 12
#else
#define XF_CLI_MAX_PROMPT_LEN 15
#endif
#endif

/* CR/LF translation for serial terminals. */
#ifndef XF_CLI_SERIAL_XLATE
#define XF_CLI_SERIAL_XLATE 1
#endif

/* ANSI color output for prompt/command line. */
#ifndef XF_CLI_COLORFUL
#if XF_SHELL_PROFILE_MIN_SIZE
#define XF_CLI_COLORFUL 0
#else
#define XF_CLI_COLORFUL 1
#endif
#endif

/* Prompt color sequence. */
#ifndef XF_CLI_PROMPT_COLOR
#define XF_CLI_PROMPT_COLOR "\x1b[31m"
#endif

/* Command color sequence. */
#ifndef XF_CLI_COMMAND_COLOR
#define XF_CLI_COMMAND_COLOR "\x1b[32m"
#endif

/* ANSI reset sequence. */
#ifndef XF_CLI_COLOR_RESET
#define XF_CLI_COLOR_RESET "\x1b[0m"
#endif

/* Newline sequence used by shell output. */
#ifndef XF_SHELL_NEWLINE
#if defined(_WIN32) || defined(_WIN64)
#define XF_SHELL_NEWLINE "\n"
#else
#define XF_SHELL_NEWLINE "\r\n"
#endif
#endif

/* Helper to check whether XF_SHELL_NEWLINE is "\r\n". */
#ifndef XF_SHELL_NEWLINE_IS_CRLF
#define XF_SHELL_NEWLINE_IS_CRLF \
    (XF_SHELL_NEWLINE[0] == '\r' && XF_SHELL_NEWLINE[1] == '\n' && XF_SHELL_NEWLINE[2] == '\0')
#endif

#endif  // __XF_SHELL_CONFIG_INTERNAL_H__
