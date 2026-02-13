#ifndef __XF_CLI_H__
#define __XF_CLI_H__

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
#define XF_CLI_MAX_PROMPT_LEN 10
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
 * Start up the XF CLI subsystem. This should only be called once.
 */
void xf_cli_init(struct xf_cli *, const char *prompt,
                 void (*put_char)(void *data, char ch, bool is_last),
                 void *cb_data);

/**
 * Adds a new character into the buffer. Returns true if
 * the buffer should now be processed
 * Note: This function should not be called from an interrupt handler.
 */
bool xf_cli_insert_char(struct xf_cli *cli, char ch);

/**
 * Returns the nul terminated internal buffer. This will
 * return NULL if the buffer is not yet complete
 */
const char *xf_cli_get_line(const struct xf_cli *cli);

/**
 * Parses the internal buffer and returns it as an argc/argc combo
 * @return number of values in argv (maximum of XF_CLI_MAX_ARGC)
 */
int xf_cli_argc(struct xf_cli *cli, char ***argv);

/**
 * Outputs the CLI prompt
 * This should be called after @ref xf_cli_argc or @ref
 * xf_cli_get_line has been called and the command fully processed
 */
void xf_cli_prompt(struct xf_cli *cli);

/**
 * Retrieve a history command line
 * @param history_pos 0 is the most recent command, 1 is the one before that
 * etc...
 * @return NULL if the history buffer is exceeded
 */
const char *xf_cli_get_history(struct xf_cli *cli,
                               int history_pos);

#endif /* __XF_CLI_H__ */
