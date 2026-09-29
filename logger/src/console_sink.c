#define _POSIX_C_SOURCE 200809L

#include "snlogger/console_sink.h"

#include <sncore/platform.h>
#include <string.h>

#if defined(SN_OS_WINDOWS)
    #include <windows.h>

static bool console_enable_vt(FILE *stream);
#else
    #include <unistd.h>
#endif

static void console_sink_open(void *data);
static void console_sink_write(const char *msg, size_t len, SnLogLevel level, void *data);
static void console_sink_close(void *data);
static void console_sink_flush(void *data);

static SnConsoleColor level_color(SnLogLevel level);
static SnConsoleSink *console_of(void *data);
static void write_prefix(FILE *stream, const char *sgr, int mode);
static void write_formatted(SnConsoleSink *console, const char *sgr, int mode, const char *fmt, va_list args);
static void append_sgr(char *buf, size_t size, const char *fmt, ...);
static bool console_stream_is_terminal(FILE *stream);
static bool color_effective(const SnConsoleSink *console);

/* The SGR code for each SnConsoleMode bit, in bit order. These are not
 * bit + 1: 6 is unused, and the last three sit at 7, 8 and 9. */
static const int MODE_SGR[SN_CONSOLE_MODE_COUNT] = {1, 2, 3, 4, 5, 7, 8, 9};

/* The longest color parameter list is "48;2;255;255;255;38;2;255;255;255". */
#define SGR_BUFFER_SIZE 48
#define RESET "\x1b[0m"

SnConsoleSink sn_console_std_sink = {
    .sink =
        {
            .open = console_sink_open,
            .write = console_sink_write,
            .close = console_sink_close,
            .flush = console_sink_flush,
        },
    .stream = NULL,
    .color_mode = SN_CONSOLE_COLOR_AUTO,
    .level_color = true,
};

void sn_console_sink_init(SnConsoleSink *console, FILE *stream) {
    if (!console) return;

    console->sink.open = console_sink_open;
    console->sink.write = console_sink_write;
    console->sink.close = console_sink_close;
    console->sink.flush = console_sink_flush;
    console->sink.data = console;

    console->stream = stream;
    console->color_mode = SN_CONSOLE_COLOR_AUTO;
    console->level_color = true;
}

void sn_console_sink_set_color(SnConsoleSink *console, SnConsoleColorMode mode) {
    if (!console) return;
    console->color_mode = mode;
}

void sn_console_sink_set_level_color(SnConsoleSink *console, bool enable) {
    if (console) console->level_color = enable;
}

void sn_console_write_va(
    SnConsoleSink *console, SnConsoleColor fg, SnConsoleColor bg, int mode, const char *fmt, va_list args) {
    if (!console || !fmt) return;

    /* zeroed, because append_sgr appends to whatever is already there */
    char sgr[SGR_BUFFER_SIZE] = {0};
    append_sgr(sgr, sizeof(sgr), "%d;%d", (int)bg + 10, (int)fg);

    write_formatted(console, sgr, mode, fmt, args);
}

void sn_console_write(
    SnConsoleSink *console, SnConsoleColor fg, SnConsoleColor bg, int mode, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    sn_console_write_va(console, fg, bg, mode, fmt, args);
    va_end(args);
}

void sn_console_write_rgb_va(
    SnConsoleSink *console, SnRgbColor fg, SnRgbColor bg, int mode, const char *fmt, va_list args) {
    if (!console || !fmt) return;

    char sgr[SGR_BUFFER_SIZE] = {0};

    if (bg.set) {
        append_sgr(sgr, sizeof(sgr), "48;2;%u;%u;%u;", bg.r, bg.g, bg.b);
    } else {
        append_sgr(sgr, sizeof(sgr), "49;");
    }

    if (fg.set) {
        append_sgr(sgr, sizeof(sgr), "38;2;%u;%u;%u", fg.r, fg.g, fg.b);
    } else {
        append_sgr(sgr, sizeof(sgr), "39");
    }

    write_formatted(console, sgr, mode, fmt, args);
}

void sn_console_write_rgb(SnConsoleSink *console, SnRgbColor fg, SnRgbColor bg, int mode, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    sn_console_write_rgb_va(console, fg, bg, mode, fmt, args);
    va_end(args);
}

void sn_console_flush(SnConsoleSink *console) {
    if (!console) return;
    fflush(console->stream ? console->stream : stdout);
}

/* The one place a record is actually emitted. The caller has already built the
 * color parameter list, so the palette, the rgb and the va_list forms all share
 * this and cannot drift apart. */
static void write_formatted(SnConsoleSink *console, const char *sgr, int mode, const char *fmt, va_list args) {
    FILE *stream = console->stream ? console->stream : stdout;

    if (color_effective(console)) {
        write_prefix(stream, sgr, mode);
        vfprintf(stream, fmt, args);
        fputs(RESET, stream);
    } else {
        vfprintf(stream, fmt, args);
    }
}

/* Emit the opening escape: any graphics modes, then the color parameters, then
 * the 'm' that ends the sequence. Every colored write goes through here so the
 * palette and rgb forms cannot drift apart. */
static void write_prefix(FILE *stream, const char *sgr, int mode) {
    fputs("\x1b[", stream);

    if (mode != SN_CONSOLE_MODE_DEFAULT) {
        for (int bit = 0; bit < SN_CONSOLE_MODE_COUNT; ++bit) {
            if (mode & SN_BIT_FLAG(bit)) fprintf(stream, "%d;", MODE_SGR[bit]);
        }
    }

    if (sgr && *sgr) fputs(sgr, stream);
    fputs("m", stream);
}

/* Append to a color parameter buffer, clamping rather than trusting the length
 * snprintf reports, which is the length it wanted, not the length it wrote. */
static void append_sgr(char *buf, size_t size, const char *fmt, ...) {
    size_t len = strlen(buf);
    if (len + 1 >= size) return;

    va_list args;
    va_start(args, fmt);
    int wrote = vsnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    if (wrote < 0) return;
    if ((size_t)wrote >= size - len) buf[size - 1] = 0;
}

/* The global's sink.data cannot point at itself, a self reference is not a
 * constant expression, so a NULL data means the global. That is what lets
 * sn_console_std_sink.sink be handed to a logger with no setup at all. */
static SnConsoleSink *console_of(void *data) {
    return data ? (SnConsoleSink *)data : &sn_console_std_sink;
}

static void console_sink_open(void *data) {
    /* Deliberately empty. A terminal can come and go between writes, for
     * instance when a process is daemonized, so the color mode is resolved on
     * every write rather than cached here. */
    (void)data;
}

static void console_sink_write(const char *msg, size_t len, SnLogLevel level, void *data) {
    SnConsoleSink *console = console_of(data);
    if (!msg) return;

    FILE *stream = console->stream ? console->stream : stdout;

    if (!console->level_color || !color_effective(console)) {
        fwrite(msg, sizeof(char), len, stream);
        return;
    }

    char sgr[SGR_BUFFER_SIZE] = {0};
    snprintf(sgr, sizeof(sgr), "%d", (int)level_color(level));

    write_prefix(stream, sgr, SN_CONSOLE_MODE_DEFAULT);
    fwrite(msg, sizeof(char), len, stream);
    fputs(RESET, stream);
}

static void console_sink_close(void *data) {
    /* Deliberately does not close the stream. The sink borrows it, either from
     * the caller or as stdout, and the logger flushes immediately before
     * calling this, so there is nothing left to do. Closing here would fclose a
     * stream the caller still owns, and a later flush would then be undefined
     * behaviour. A sink that opened a file of its own would close it here. */
    (void)data;
}

static void console_sink_flush(void *data) {
    SnConsoleSink *console = console_of(data);
    fflush(console->stream ? console->stream : stdout);
}

static SnConsoleColor level_color(SnLogLevel level) {
    switch (level) {
        case SN_LOG_LEVEL_TRACE:
        case SN_LOG_LEVEL_DEBUG:
            return SN_CONSOLE_COLOR_CYAN;
        case SN_LOG_LEVEL_INFO:
            return SN_CONSOLE_COLOR_DEFAULT;
        case SN_LOG_LEVEL_WARN:
            return SN_CONSOLE_COLOR_YELLOW;
        case SN_LOG_LEVEL_ERROR:
            return SN_CONSOLE_COLOR_RED;
        case SN_LOG_LEVEL_FATAL:
            return SN_CONSOLE_COLOR_MAGENTA;
    }
    return SN_CONSOLE_COLOR_DEFAULT;
}

#if defined(SN_OS_WINDOWS)
/* Turn on virtual terminal processing, without which the escape codes would
 * print as literal garbage. GetConsoleMode succeeding is also how we tell a
 * real console from a redirected file, since the redirection is what makes it
 * fail.
 *
 * The console mode is a one time, process wide, idempotent change, so it is
 * done once and remembered. Doing it on every write would put a GetConsoleMode
 * and a SetConsoleMode pair in the logging path. This is not the same as
 * caching the color decision, which is resolved fresh every write. */
static bool console_enable_vt(FILE *stream) {
    /* tracked per handle, so enabling one does not vouch for the other */
    static bool done[2] = {false, false};

    int which = stream == stderr ? 1 : 0;
    if (done[which]) return true;

    HANDLE handle = GetStdHandle(which ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    if (handle == NULL || handle == INVALID_HANDLE_VALUE) return false;

    DWORD modes = 0;
    if (!GetConsoleMode(handle, &modes)) return false;

    modes |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;

    if (!SetConsoleMode(handle, modes)) return false;

    done[which] = true;
    return true;
}
#endif

static bool color_effective(const SnConsoleSink *console) {
    if (console->color_mode == SN_CONSOLE_COLOR_ON) return true;
    if (console->color_mode == SN_CONSOLE_COLOR_OFF) return false;

    FILE *stream = console->stream ? console->stream : stdout;

    return console_stream_is_terminal(stream);
}

static bool console_stream_is_terminal(FILE *stream) {
#if defined(SN_OS_WINDOWS)
    return console_enable_vt(stream);
#else
    return isatty(fileno(stream));
#endif
}
