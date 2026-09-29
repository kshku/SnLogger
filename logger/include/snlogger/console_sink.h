#pragma once

#include "snlogger/api.h"
#include "snlogger/log_level.h"
#include "snlogger/sink.h"

#include <sncore/defines.h>
#include <stdarg.h>
#include <stdio.h>

// https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797#color-codes
/**
 * @enum SnConsoleColor
 * @brief ANSI foreground and background colors.
 */
typedef enum SnConsoleColor {
    SN_CONSOLE_COLOR_BLACK = 30, /**< Black */
    SN_CONSOLE_COLOR_RED, /**< Red */
    SN_CONSOLE_COLOR_GREEN, /**< Green */
    SN_CONSOLE_COLOR_YELLOW, /**< Yellow */
    SN_CONSOLE_COLOR_BLUE, /**< Blue */
    SN_CONSOLE_COLOR_MAGENTA, /**< Magenta */
    SN_CONSOLE_COLOR_CYAN, /**< Cyan */
    SN_CONSOLE_COLOR_WHITE, /**< White */

    SN_CONSOLE_COLOR_DEFAULT = 39 /**< The terminal default */
} SnConsoleColor;

// https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797#colors--graphics-mode
/**
 * @enum SnConsoleMode
 * @brief ANSI graphics modes, combinable with the bitwise or operator.
 *
 * Not every terminal supports every mode. More than one can be set at a time,
 * except for SN_CONSOLE_MODE_DEFAULT.
 */
typedef enum SnConsoleMode {
    SN_CONSOLE_MODE_DEFAULT = 0, /**< No mode */
    SN_CONSOLE_MODE_BOLD = SN_BIT_FLAG(0), /**< Bold */
    SN_CONSOLE_MODE_DIM = SN_BIT_FLAG(1), /**< Dim */
    SN_CONSOLE_MODE_ITALIC = SN_BIT_FLAG(2), /**< Italic */
    SN_CONSOLE_MODE_UNDERLINE = SN_BIT_FLAG(3), /**< Underline */
    SN_CONSOLE_MODE_BLINKING = SN_BIT_FLAG(4), /**< Blinking */
    SN_CONSOLE_MODE_INVERSE = SN_BIT_FLAG(5), /**< Inverse */
    SN_CONSOLE_MODE_HIDDEN = SN_BIT_FLAG(6), /**< Hidden */
    SN_CONSOLE_MODE_STRIKETHROUGH = SN_BIT_FLAG(7) /**< Strikethrough */
} SnConsoleMode;

/** The number of bits an SnConsoleMode uses. */
#define SN_CONSOLE_MODE_COUNT 8

/**
 * @struct SnRgbColor
 * @brief A 24 bit color, or the terminal's own color.
 */
typedef struct SnRgbColor {
    bool set; /**< False means leave the terminal default alone. */
    uint8_t r, g, b;
} SnRgbColor;

/** A color, the terminal's own foreground or background. */
#define SN_RGB_DEFAULT ((SnRgbColor){.set = false, .r = 0, .g = 0, .b = 0})

/** An 8 bit per channel color. The channels wrap, as they do in a terminal. */
#define SN_RGB(red, green, blue)                                                                  \
    ((SnRgbColor){.set = true, .r = (uint8_t)(red), .g = (uint8_t)(green), .b = (uint8_t)(blue)})

/**
 * @enum SnConsoleColorMode
 * @brief Whether a console sink emits ANSI escape codes.
 */
typedef enum SnConsoleColorMode {
    SN_CONSOLE_COLOR_AUTO = -1, /**< Emit only when the stream is a terminal */
    SN_CONSOLE_COLOR_OFF = 0, /**< Never emit escape codes */
    SN_CONSOLE_COLOR_ON = 1 /**< Always emit escape codes */
} SnConsoleColorMode;

/**
 * @struct SnConsoleSink console_sink.h <snlogger/console_sink.h>
 * @brief A sink that writes records to a terminal.
 *
 * By default color follows the destination, so records written to a terminal
 * come out colored and records written to a file or a pipe come out as plain
 * text. The escape codes are left out entirely rather than written and stripped
 * afterwards, so a redirected log stays clean either way.
 *
 * @note This does no locking. A sink used from more than one thread has to
 *       serialize its own writes.
 */
typedef struct SnConsoleSink {
    SnSink sink; /**< Hand &console->sink to a logger. */
    FILE *stream; /**< Where records are written. NULL means stdout. */
    int color_mode; /**< An SnConsoleColorMode. */
    bool level_color; /**< Whether to color records by log level. */
} SnConsoleSink;

/**
 * @brief Point a console sink at a stream and set it to sensible defaults.
 *
 * Color mode is SN_CONSOLE_COLOR_AUTO, so color follows the stream, and level
 * color is on. A sink is usable without ever calling this, which is only
 * needed to send output somewhere other than stdout.
 *
 * @param console The sink to initialize.
 * @param stream Where records are written. NULL means stdout.
 */
SN_LOGGER_API void sn_console_sink_init(SnConsoleSink *console, FILE *stream);

/**
 * @brief Set whether escape codes are emitted.
 *
 * @param console The sink.
 * @param mode An SnConsoleColorMode. ON emits escape codes even into a pipe,
 *             which is what you want when the pipe ends in a pager that
 *             understands them. OFF keeps them out even on a terminal.
 */
SN_LOGGER_API void sn_console_sink_set_color(SnConsoleSink *console, SnConsoleColorMode mode);

/**
 * @brief Set whether records are colored by their log level.
 *
 * Ignored by sn_console_write, which always uses the colors it is given.
 * Enabled by sn_console_sink_init.
 *
 * @param console The sink.
 * @param enable True to color by level, false to write records uncolored.
 */
SN_LOGGER_API void sn_console_sink_set_level_color(SnConsoleSink *console, bool enable);

/**
 * @brief Write one formatted record, wrapped in the given color and mode.
 *
 * The escape codes are left out entirely when color is off, so the result is
 * plain text on a pipe or in a CI log. The format string is used only when
 * something is written, so a plain string costs nothing extra.
 *
 * @param console The sink.
 * @param fg The foreground color.
 * @param bg The background color.
 * @param mode The graphics mode, a bitwise or of SnConsoleMode.
 * @param fmt A printf style format string.
 */
SN_LOGGER_API void sn_console_write(
    SnConsoleSink *console, SnConsoleColor fg, SnConsoleColor bg, int mode, const char *fmt, ...);

/**
 * @brief Write one formatted record from an argument list.
 *
 * The same as sn_console_write, taking a va_list rather than being variadic,
 * for a caller that has already collected its arguments and cannot forward
 * them through a plain ... .
 *
 * @param console The sink.
 * @param fg The foreground color.
 * @param bg The background color.
 * @param mode The graphics mode, a bitwise or of SnConsoleMode.
 * @param fmt A printf style format string.
 * @param args The arguments for @p fmt.
 */
SN_LOGGER_API void sn_console_write_va(
    SnConsoleSink *console, SnConsoleColor fg, SnConsoleColor bg, int mode, const char *fmt, va_list args);

/**
 * @brief Write one formatted record in 24 bit color.
 *
 * The same as sn_console_write, with the two colors given as RGB values. A
 * terminal that does not understand truecolor falls back to whatever it does
 * understand, usually a near match, and one that has color turned off gets
 * plain text as usual.
 *
 * @param console The sink.
 * @param fg The foreground color, or SN_RGB_DEFAULT.
 * @param bg The background color, or SN_RGB_DEFAULT.
 * @param mode The graphics mode, a bitwise or of SnConsoleMode.
 * @param fmt A printf style format string.
 */
SN_LOGGER_API void sn_console_write_rgb(
    SnConsoleSink *console, SnRgbColor fg, SnRgbColor bg, int mode, const char *fmt, ...);

/**
 * @brief Write one formatted 24 bit record from an argument list.
 *
 * The same as sn_console_write_rgb, taking a va_list rather than being
 * variadic.
 *
 * @param console The sink.
 * @param fg The foreground color, or SN_RGB_DEFAULT.
 * @param bg The background color, or SN_RGB_DEFAULT.
 * @param mode The graphics mode, a bitwise or of SnConsoleMode.
 * @param fmt A printf style format string.
 * @param args The arguments for @p fmt.
 */
SN_LOGGER_API void sn_console_write_rgb_va(
    SnConsoleSink *console, SnRgbColor fg, SnRgbColor bg, int mode, const char *fmt, va_list args);

/**
 * @brief Flush the console sink's stream.
 *
 * @param console The sink.
 */
SN_LOGGER_API void sn_console_flush(SnConsoleSink *console);

/**
 * @brief A ready made console sink on stdout.
 *
 * For callers that do not need a stream of their own. It needs no init call, it
 * is ready as the program starts, the same way sn_std_allocator needs none. It
 * does no locking, so it is not usable from several threads at once.
 */
extern SN_LOGGER_API SnConsoleSink sn_console_std_sink;
