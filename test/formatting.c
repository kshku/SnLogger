#include <snlogger/snlogger.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* ------------------ Simple layout formatter ------------------ */

typedef struct LayoutCtx {
    char buffer[256];
} LayoutCtx;

static size_t format_layout(LayoutCtx *ctx, uint64_t timestamp, snLogLevel level, const char *msg, size_t msg_len) {
    const char *level_str = "";
    switch (level) {
        case SN_LOG_LEVEL_TRACE: level_str = "TRACE"; break;
        case SN_LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case SN_LOG_LEVEL_INFO:  level_str = "INFO";  break;
        case SN_LOG_LEVEL_WARN:  level_str = "WARN";  break;
        case SN_LOG_LEVEL_ERROR: level_str = "ERROR"; break;
        case SN_LOG_LEVEL_FATAL: level_str = "FATAL"; break;
    }

    // NOTE: If VT is not enabled, colored output is not visible in windows and other characters will be displayed instead!
    static const char *colors[] = {
        "0;37", "0;34", "0;32", "0;33", "1;31", "1;41"
    };

    int n = snprintf(ctx->buffer, sizeof(ctx->buffer), "\x1b[%sm[%llu] [%s]: ", colors[level], (unsigned long long)timestamp, level_str);


    size_t off = (n > 0) ? (size_t)n : 0;

    size_t copy = msg_len;
    if (off + copy > sizeof(ctx->buffer))
        copy = sizeof(ctx->buffer) - off;

    memcpy(ctx->buffer + off, msg, copy);
    off += copy;

    return off;
}

/* ------------------ Sink that applies layout ------------------ */

static void
layout_sink_write(const char *msg, size_t len,
                  snLogLevel level, void *data)
{
    LayoutCtx *ctx = data;

    uint64_t ts = (uint64_t)time(NULL);

    size_t out_len = format_layout(ctx, ts, level, msg, len);

    fwrite(ctx->buffer, 1, out_len, stdout);
    fputs("\x1b[0m\n", stdout);
}

/* ------------------ Test ------------------ */

int main(void)
{
    char log_buf[128];
    LayoutCtx layout = {0};

    snSink sink = {
        .write = layout_sink_write,
        .data  = &layout
    };

    snStaticLogger logger;
    sn_static_logger_init(&logger,
                          log_buf, sizeof(log_buf),
                          &sink, 1);

    sn_static_logger_log(&logger, SN_LOG_LEVEL_INFO,
                         "Hello %s", "world");

    sn_static_logger_log(&logger, SN_LOG_LEVEL_FATAL,
                         "Hello %s", "FATAL");

    sn_static_logger_deinit(&logger);
    return 0;
}

