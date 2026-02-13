#ifndef __XF_SHELL_CLI_H__
#define __XF_SHELL_CLI_H__

#include <stdbool.h>

#ifndef XF_CLI_MAX_LINE
/**
 * Maximum number of bytes to accept in a single line
 */
#define XF_CLI_MAX_LINE 120
#endif

#ifndef XF_CLI_HISTORY_LEN
/**
 * Maximum number of bytes to retain of history data
 * Define this to 0 to remove history support
 */
#define XF_CLI_HISTORY_LEN 1000
#endif

#ifndef XF_CLI_MAX_ARGC
/**
 * What is the maximum number of arguments we reserve space for
 */
#define XF_CLI_MAX_ARGC 16
#endif

#ifndef XF_CLI_MAX_PROMPT_LEN
/**
 * Maximum number of bytes in the prompt
 */
#define XF_CLI_MAX_PROMPT_LEN 15
#endif

#ifndef XF_CLI_SERIAL_XLATE
/**
 * Translate CR -> NL on input and output CR NL on output. This allows
 * "natural" processing when using a serial terminal.
 */
#define XF_CLI_SERIAL_XLATE 1
#endif

#ifndef XF_CLI_COLORFUL
/**
 * Enable ANSI colorful output for prompt/command.
 * 0: disabled, 1: enabled
 */
#define XF_CLI_COLORFUL 1
#endif

#ifndef XF_CLI_PROMPT_COLOR
/**
 * Prompt color sequence, default red.
 */
#define XF_CLI_PROMPT_COLOR "\x1b[31m"
#endif

#ifndef XF_CLI_COMMAND_COLOR
/**
 * Command input color sequence, default green.
 */
#define XF_CLI_COMMAND_COLOR "\x1b[32m"
#endif

#ifndef XF_CLI_COLOR_RESET
/**
 * ANSI reset color sequence.
 */
#define XF_CLI_COLOR_RESET "\x1b[0m"
#endif

/**
 * This is the structure which defines the current state of the CLI
 * NOTE: Although this structure is exposed here, it is not recommended
 * that it be interacted with directly. Use the accessor functions below to
 * interact with it. It is exposed here to make it easier to use as a static
 * structure, but all elements of the structure should be considered private
 */
struct xf_cli {
    /**
     * Internal buffer. This should not be accessed directly, use the
     * access functions below
     */
    char buffer[XF_CLI_MAX_LINE];

#if XF_CLI_HISTORY_LEN
    /**
     * List of history entries
     */
    char history[XF_CLI_HISTORY_LEN];

    /**
     * Are we searching through the history?
     */
    bool searching;

    /**
     * How far back in the history are we?
     */
    int history_pos;
#endif

    /**
     * Number of characters in buffer at the moment
     */
    int len;

    /**
     * Position of the cursor
     */
    int cursor;

    /**
     * Have we just parsed a full line?
     */
    bool done;

    /**
     * Callback function to output a single character to the user
     * is_last will be set to true if this is the last character in this
     * transmission - this is helpful for flushing buffers.
     */
    void (*put_char)(void *data, char ch, bool is_last);

    /**
     * Data to provide to the put_char callback
     */
    void *cb_data;

    bool have_escape;
    bool have_csi;

    /**
     * counter of the value for the CSI code
     */
    int counter;

    char *argv[XF_CLI_MAX_ARGC];

    char prompt[XF_CLI_MAX_PROMPT_LEN];
};

/**
 * @brief 初始化 CLI 核心状态对象。
 *
 * @details
 * 该函数会清空内部缓冲区、初始化光标与历史相关状态，并绑定输出回调。
 * 同一个 `struct xf_cli` 实例通常只需初始化一次。
 *
 * @param[in,out] cli CLI 状态对象。
 * @param[in] prompt 提示符字符串（会复制到内部缓存）。
 * @param[in] put_char 单字符输出回调。
 * @param[in] cb_data 回调透传上下文。
 */
void xf_cli_init(struct xf_cli *, const char *prompt,
                 void (*put_char)(void *data, char ch, bool is_last),
                 void *cb_data);

/**
 * @brief 向 CLI 输入缓冲插入一个字符并更新编辑状态。
 *
 * @details
 * 支持普通字符输入、退格、回车、方向键序列等交互。
 * 当返回 `true` 时表示当前行已完成，可触发命令解析与执行。
 *
 * @param[in,out] cli CLI 状态对象。
 * @param[in] ch 输入字符。
 *
 * @return
 * - `true`：一行命令已结束；
 * - `false`：继续接收后续字符。
 *
 * @note 不建议在中断上下文中调用。
 */
bool xf_cli_insert_char(struct xf_cli *cli, char ch);

/**
 * @brief 获取当前已完成命令行的内部字符串指针。
 *
 * @param[in] cli CLI 状态对象。
 * @return
 * - 非 `NULL`：指向以 `\0` 结尾的命令行文本；
 * - `NULL`：当前尚未形成完整命令行。
 */
const char *xf_cli_get_line(const struct xf_cli *cli);

/**
 * @brief 将内部命令行按参数拆分为 `argc/argv` 形式。
 *
 * @details
 * 拆分结果写入 `cli` 内部的 `argv` 数组，并通过 `argv` 输出指针返回。
 *
 * @param[in,out] cli CLI 状态对象。
 * @param[out] argv 输出参数数组指针。
 *
 * @return 参数个数，最大不超过 `XF_CLI_MAX_ARGC`。
 */
int xf_cli_argc(struct xf_cli *cli, char ***argv);

/**
 * @brief 输出提示符并准备下一轮输入。
 *
 * @details
 * 一般在命令处理完成后调用，使终端回到可输入状态。
 *
 * @param[in,out] cli CLI 状态对象。
 */
void xf_cli_prompt(struct xf_cli *cli);

/**
 * @brief 按序号获取历史命令文本。
 *
 * @param[in,out] cli CLI 状态对象。
 * @param[in] history_pos 历史偏移，`0` 表示最近一条，`1` 表示上一条。
 *
 * @return
 * - 非 `NULL`：历史命令字符串；
 * - `NULL`：越界或无可用历史记录。
 */
const char *xf_cli_get_history(struct xf_cli *cli,
                               int history_pos);

#endif /* __XF_SHELL_CLI_H__ */
