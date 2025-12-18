#include <snlogger/snlogger.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#if defined(SN_OS_WINDOWS)
#include <windows.h>

static bool enableVTProcessing(DWORD handle_type);
#else
#include <unistd.h>
#endif

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

#define LOGGER_BUFFER_SIZE 1024

#define FORMAT "[%s]: %s:%lu in funtion %s: %s\n"
#define FORMAT_ARGS level_string, file, line, function, format_string

void log_msg(snStaticLogger *lg,
        snLogLevel level, const char *file,
        const char *function, long line,
        const char *format_string, ...) {

    if (level < lg->level) return;

    const char *level_string = NULL;
    const char *level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    level_string = level_strings[level];


    char buffer[1024] = {0};
    size_t buffer_size = ARRAY_LEN(buffer);
    size_t len = snprintf(buffer, buffer_size, FORMAT, FORMAT_ARGS);

    if (len < 0) abort();

    if (len >= buffer_size) {
        log_msg(lg, SN_LOG_LEVEL_ERROR, file, function, line, "Too long message, try increasing the buffer size or decreasing message length!");
        return;
    }

    buffer[len] = 0;

    va_list args;
    va_start(args, format_string);
    sn_static_logger_log_va(lg, level, buffer, args);
    va_end(args);
}

void loga_msg(snAsyncLogger *lg,
        snLogLevel level, const char *file,
        const char *function, long line,
        const char *format_string, ...) {
    if (level < lg->level) return;

    const char *level_string = NULL;
    const char *level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    level_string = level_strings[level];

    char buffer[1024] = {0};
    size_t buffer_size = ARRAY_LEN(buffer);
    size_t len = snprintf(buffer, buffer_size, FORMAT, FORMAT_ARGS);

    if (len < 0) abort();

    if (len >= buffer_size) {
        loga_msg(lg, SN_LOG_LEVEL_ERROR, file, function, line, "Too long message, try increasing the buffer size or decreasing message length!");
        return;
    }

    buffer[len] = 0;

    va_list args;
    va_start(args, format_string);
    sn_async_logger_log_va(lg, level, buffer, args);
    va_end(args);
}

#define log_trace(lg, msg, ...) log_msg(lg, SN_LOG_LEVEL_TRACE, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define log_debug(lg, msg, ...) log_msg(lg, SN_LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define log_info(lg, msg, ...) log_msg(lg, SN_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define log_warn(lg, msg, ...) log_msg(lg, SN_LOG_LEVEL_WARN, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define log_error(lg, msg, ...) log_msg(lg, SN_LOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define log_fatal(lg, msg, ...) log_msg(lg, SN_LOG_LEVEL_FATAL, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)

#define loga_trace(lg, msg, ...) loga_msg(lg, SN_LOG_LEVEL_TRACE, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define loga_debug(lg, msg, ...) loga_msg(lg, SN_LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define loga_info(lg, msg, ...) loga_msg(lg, SN_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define loga_warn(lg, msg, ...) loga_msg(lg, SN_LOG_LEVEL_WARN, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define loga_error(lg, msg, ...) loga_msg(lg, SN_LOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)
#define loga_fatal(lg, msg, ...) loga_msg(lg, SN_LOG_LEVEL_FATAL, __FILE__, __func__, __LINE__, msg, ##__VA_ARGS__)

typedef struct stdout_stderr_sink {
    // 0 -> stdout, 1 -> stderr
    bool colored_enabled[2];
} stdout_stderr_sink;

void stdout_stderr_sink_write(const char *msg, size_t len, snLogLevel level, void *data) {
    stdout_stderr_sink *sink = (stdout_stderr_sink *)data;

    bool error = level > SN_LOG_LEVEL_WARN;
    FILE *out_file = error ? stderr : stdout;

    // error true => 1 => stderr
    // error false => 0 => stdout
    if (sink->colored_enabled[(int)error]) {
        switch (level) {
            case SN_LOG_LEVEL_TRACE:
                fputs("\x1b[0;37m", out_file);
                break;
            case SN_LOG_LEVEL_DEBUG:
                fputs("\x1b[0;34m", out_file);
                break;
            case SN_LOG_LEVEL_INFO:
                fputs("\x1b[0;32m", out_file);
                break;
            case SN_LOG_LEVEL_WARN:
                fputs("\x1b[0;33m", out_file);
                break;
            case SN_LOG_LEVEL_ERROR:
                fputs("\x1b[1;31m", out_file);
                break;
            case SN_LOG_LEVEL_FATAL:
                fputs("\x1b[1;41m", out_file);
                break;
        }
    }

    fwrite(msg, sizeof(char), len, out_file);

    if (sink->colored_enabled[(int)error]) fputs("\x1b[0m", out_file);
}

void stdout_stderr_sink_open(void *data) {
    stdout_stderr_sink *sink = (stdout_stderr_sink *)data;
#if defined(SN_OS_WINDOWS)
    sink->colored_enabled[0] = enableVTProcessing(STD_OUTPUT_HANDLE);
    sink->colored_enabled[1] = enableVTProcessing(STD_ERROR_HANDLE);
#else
    sink->colored_enabled[0] = isatty(STDOUT_FILENO);
    sink->colored_enabled[1] = isatty(STDERR_FILENO);
#endif
}

void stdout_stderr_sink_flush(void *data) {
    (void)data;
    fflush(stdout);
    fflush(stderr);
}

int main(void) {
    snStaticLogger sl;
    snAsyncLogger al;

    // 0 -> static, 1 -> async
    char buffers[2][LOGGER_BUFFER_SIZE];
    stdout_stderr_sink sink_data[2] = {0}; 
    snSink sinks[2][1] = {
        // Static
        {
            (snSink){
                .open = stdout_stderr_sink_open,
                .flush = stdout_stderr_sink_flush,
                .write = stdout_stderr_sink_write,
                .data = &sink_data[0]
            }
        },
        // Async
        {
            (snSink){
                .open = stdout_stderr_sink_open,
                .flush = stdout_stderr_sink_flush,
                .write = stdout_stderr_sink_write,
                .data = &sink_data[1]
            }
        }
    };

    sn_static_logger_init(&sl, buffers[0], LOGGER_BUFFER_SIZE, sinks[0], ARRAY_LEN(sinks[0]));
    sn_async_logger_init(&al, buffers[1], LOGGER_BUFFER_SIZE, sinks[1], ARRAY_LEN(sinks[1]));

    log_trace(&sl, "Static trace message %.2f", 3.1415);
    log_debug(&sl, "Static debug message %.2f", 3.1415);
    log_info(&sl, "Static info message %.2f", 3.1415);
    log_warn(&sl, "Static warn message %.2f", 3.1415);
    log_error(&sl, "Static error message %.2f", 3.1415);
    log_fatal(&sl, "Static fatal message %.2f", 3.1415);

    loga_trace(&al, "Async trace message %.2f", 3.1415);
    loga_debug(&al, "Async debug message %.2f", 3.1415);
    sn_async_logger_process(&al);
    loga_info(&al, "Async info message %.2f", 3.1415);
    loga_warn(&al, "Async warn message %.2f", 3.1415);
    sn_async_logger_process(&al);
    loga_error(&al, "Async error message %.2f", 3.1415);
    loga_fatal(&al, "Async fatal message %.2f", 3.1415);

    sn_static_logger_deinit(&sl);
    sn_async_logger_deinit(&al);
}

#if defined(SN_OS_WINDOWS)
static bool enableVTProcessing(DWORD handle_type) {
    HANDLE handle = GetStdHandle(handle_type);
    if (handle == INVALID_HANDLE_VALUE) return false;

    DWORD modes = 0;
    if (!GetConsoleMode(handle, &modes)) return false;

    modes |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING
           | DISABLE_NEWLINE_AUTO_RETURN;
    if (!SetConsoleMode(handle, modes)) return false;

    return true;
}
#endif

