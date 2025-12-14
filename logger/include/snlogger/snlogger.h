#pragma once

#include <stdarg.h>
#include <stddef.h>

/**
 * @enum
 * @brief The log levels.
 */
typedef enum snLogLevel {
    SN_LOG_LEVEL_TRACE,
    SN_LOG_LEVEL_DEBUG,
    SN_LOG_LEVEL_INFO,
    SN_LOG_LELEL_WARN,
    SN_LOG_LEVEL_ERROR,
    SN_LOG_LEVEL_FATAL
} snLogLevel;

/**
 * @brief The sink function for the static logger.
 *
 * @note must not allocate memory, must not call logger, should not block for long.
 *
 * @param msg The message
 * @parma len Lenght of the message
 * @param level The log level
 * @param data The user data
 */
typedef void (*snStaticSinkFn)(const char *msg, size_t len, snLogLevel level, void *data);

/**
 * @struct snStaticSink snlogger.h "snlogger/snlogger.h"
 * @brief The static sink.
 */
typedef struct snStaticSink {
    snStaticSinkFn fn;
    void *data;
} snStaticSink;

/**
 * @struct snStaticLogger snlogger.h "snlogger/snlogger.h"
 * @brief Static logger context.
 */
typedef struct snStaticLogger {
    char *buffer; /**< The buffer used by logger */
    size_t buffer_size; /**< The size of the buffer */

    snStaticSink *sinks;
    size_t sink_count;

    snLogLevel level; /**< The global log level */

    size_t dropped;
    size_t truncated;
} snStaticLogger;

/**
 * @brief Initialize the static logger.
 *
 * @param logger Pointer to the context
 * @param buffer The buffer for logger
 * @param buffer_size The buffer size
 * @param sinks List of sinks
 * @param sink_count The number of sinks
 */
void sn_static_logger_init(snStaticLogger *logger, void *buffer, size_t buffer_size,
        snStaticSink *sinks, size_t sink_count);

/**
 * @brief Set the log level.
 *
 * @param logger Pointer to the context
 * @param level The log level
 */
void sn_static_logger_set_level(snStaticLogger *logger, snLogLevel level);

/**
 * @brief Log a message using static logger.
 *
 * @param logger Pointer to the static logger
 * @param level Log level
 * @param fmt The format string
 * @param ... The args for format string
 */
void sn_static_logger_log(snStaticLogger *logger, snLogLevel level, const char *fmt, ...);

/**
 * @brief Log a message using static logger.
 *
 * @param logger Pointer to the static logger
 * @param level Log level
 * @param fmt The format string
 * @param args The va_list of args for format string
 */
void sn_static_logger_log_va(snStaticLogger *logger, snLogLevel level, const char *fmt, va_list args);

/**
 * @brief Log a raw message (no formatting).
 *
 * @param logger Pointer to the static logger
 * @param level Log level
 * @param msg The msessage
 * @param len The length of the message
 */
void sn_static_logger_log_raw(snStaticLogger *logger, snLogLevel level, const char *msg, size_t len);

