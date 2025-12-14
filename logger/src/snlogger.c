#include "snlogger/snlogger.h"

#include <stdio.h>

// TODO:
static size_t format_string(char *buffer, size_t len, const char *fmt, va_list args) {
    // for now vsnprintf
    int l = vsnprintf(buffer, len, fmt, args);
    return (size_t)l;
}

static void emit_to_all_sinks(snStaticLogger *logger, size_t len, snLogLevel level) {
    for (size_t i = 0; i < logger->sink_count; ++i)
        logger->sinks[i].fn(logger->buffer, len, level, logger->sinks[i].data);
}

void sn_static_logger_init(snStaticLogger *logger, void *buffer, size_t buffer_size,
        snStaticSink *sinks, size_t sink_count) {
    *logger = (snStaticLogger){
        .buffer = buffer,
        .buffer_size = buffer_size,

        .sinks = sinks,
        .sink_count = sink_count,

        .level = SN_LOG_LEVEL_TRACE,
    };
}

void sn_static_logger_set_level(snStaticLogger *logger, snLogLevel level) {
    logger->level = level;
}

void sn_static_logger_log(snStaticLogger *logger, snLogLevel level, const char *fmt, ...) {
    if (level < logger->level) return;

    va_list args;
    va_start(args, fmt);

    size_t len = format_string(logger->buffer, logger->buffer_size, fmt, args);

    va_end(args);

    emit_to_all_sinks(logger, len, level);

}

void sn_static_logger_log_va(snStaticLogger *logger, snLogLevel level, const char *fmt, va_list args) {
    if (level < logger->level) return;

    size_t len = format_string(logger->buffer, logger->buffer_size, fmt, args);

    emit_to_all_sinks(logger, len, level);
}

void sn_static_logger_log_raw(snStaticLogger *logger, snLogLevel level, const char *msg, size_t len) {
    if (level < logger->level) return;

    for (size_t i = 0; i < logger->sink_count; ++i)
        logger->sinks[i].fn(msg, len, level, logger->sinks[i].data);
}

