#include "snlogger/static_logger.h"

#include "snlogger/formatter.h"

void sn_static_logger_init(snStaticLogger *logger, char *buffer, size_t buffer_size,
        snSink *sinks, size_t sink_count) {
    *logger = (snStaticLogger){
        .buffer = buffer,
        .buffer_size = buffer_size,

        .sinks = sinks,
        .sink_count = sink_count,

        .level = SN_LOG_LEVEL_TRACE,
    };

    for (size_t i = 0; i < sink_count; ++i)
        if (sinks[i].open) sinks[i].open(sinks[i].data);
}

void sn_static_logger_deinit(snStaticLogger *logger) {
    for (size_t i = 0; i < logger->sink_count; ++i) {
        if (logger->sinks[i].flush) logger->sinks[i].flush(logger->sinks[i].data);
        if (logger->sinks[i].close) logger->sinks[i].close(logger->sinks[i].data);
    }
}

void sn_static_logger_flush(snStaticLogger *logger) {
    for (size_t i = 0; i < logger->sink_count; ++i) 
        if (logger->sinks[i].flush) logger->sinks[i].flush(logger->sinks[i].data);
}

void sn_static_logger_set_level(snStaticLogger *logger, snLogLevel level) {
    logger->level = level;
}

void sn_static_logger_log(snStaticLogger *logger, snLogLevel level, const char *fmt, ...) {
    if (level < logger->level) return;

    va_list args;
    va_start(args, fmt);
    sn_static_logger_log_va(logger, level, fmt, args);
    va_end(args);
}

void sn_static_logger_log_va(snStaticLogger *logger, snLogLevel level, const char *fmt, va_list args) {
    if (level < logger->level) return;

    size_t len = format_string(logger->buffer, logger->buffer_size, fmt, args);

    if (len >= logger->buffer_size) {
        len = logger->buffer_size - 1;
        logger->truncated++;
    } else if (len == 0) {
        logger->dropped++;
        return;
    }

    for (size_t i = 0; i < logger->sink_count; ++i)
        logger->sinks[i].write(logger->buffer, len, level, logger->sinks[i].data);
}

void sn_static_logger_log_raw(snStaticLogger *logger, snLogLevel level, const char *msg, size_t len) {
    if (level < logger->level) return;

    for (size_t i = 0; i < logger->sink_count; ++i)
        logger->sinks[i].write(msg, len, level, logger->sinks[i].data);
}

