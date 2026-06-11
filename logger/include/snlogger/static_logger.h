#pragma once

#include "snlogger/api.h"
#include "snlogger/log_level.h"
#include "snlogger/sink.h"

#include <sncore/defines.h>
#include <stdarg.h>

/**
 * @struct SnStaticLogger static_logger.h <snlogger/static_logger.h>
 * @brief Simple synchronous logger using a user-provided static buffer.
 *
 * SnStaticLogger formats log messages into a shared buffer and immediately
 * emits them to all configured sinks.
 *
 * Characteristics:
 * - Synchronous execution
 * - No internal locking
 * - Not thread-safe
 * - Uses a single shared formatting buffer
 *
 * The logger performs no dynamic allocation and does not retain log records
 * after emission.
 */
typedef struct SnStaticLogger {
    char *buffer; /**< The buffer used by logger */
    size_t buffer_size; /**< The size of the buffer */

    SnSink *sinks; /**< Array of sinks */
    size_t sink_count; /**< Number of sinks */

    SnLogLevel level; /**< The global log level threashold */

    size_t dropped; /**< Number of logs dropped */
    size_t truncated; /**< Number of logs truncated */
} SnStaticLogger;

/**
 * @brief Initialize the static logger.
 *
 * This function:
 * - Stores references to the provided buffer and sinks
 * - Sets the initial log level to SN_LOG_LEVEL_TRACE
 * - Calls the `open` callback on each sink (if provided)
 *
 * @param logger Pointer to the logger context
 * @param buffer User-provided formatting buffer
 * @param buffer_size Size of the buffer in bytes
 * @param sinks Array of sinks
 * @param sink_count Number of sinks
 *
 * @note The buffer and sinks must remain valid for the lifetime of the logger.
 * @note This function does not allocate memory.
 */
SN_LOGGER_API void sn_static_logger_init(
    SnStaticLogger *logger, char *buffer, size_t buffer_size, SnSink *sinks, size_t sink_count);

/**
 * @brief Deinitialize the static logger.
 *
 * This function:
 * - Calls `flush` on each sink (if provided)
 * - Calls `close` on each sink (if provided)
 *
 * @param logger Pointer to the logger context
 */
SN_LOGGER_API void sn_static_logger_deinit(SnStaticLogger *logger);

/**
 * @brief Flush all sinks.
 *
 * Calls the `flush` callback on each sink if it exists.
 *
 * @param logger Pointer to the logger context
 */
SN_LOGGER_API void sn_static_logger_flush(SnStaticLogger *logger);

/**
 * @brief Set the global log level.
 *
 * Messages with a level lower than this value will be ignored.
 *
 * @param logger Pointer to the logger context
 * @param level New log level threshold
 */
SN_FORCE_INLINE void sn_static_logger_set_level(SnStaticLogger *logger, SnLogLevel level) {
    logger->level = level;
}

/**
 * @brief Log a formatted message using a va_list.
 *
 * Equivalent to sn_static_logger_log, but accepts a va_list.
 *
 * @param logger Pointer to the logger context
 * @param level Log level of the message
 * @param fmt printf-style format string
 * @param args Format arguments as va_list
 *
 * @note The va_list is consumed by this function.
 */
SN_LOGGER_API void
    sn_static_logger_log_va(SnStaticLogger *logger, SnLogLevel level, const char *fmt, va_list args);

/**
 * @brief Log a formatted message.
 *
 * Formats the message into the shared buffer and emits it to all sinks.
 *
 * @param logger Pointer to the logger context
 * @param level Log level of the message
 * @param fmt printf-style format string
 * @param ... Format arguments
 *
 * @note Messages below the current log level are ignored.
 * @note Not thread-safe.
 * @note Messages may be truncated if teh buffer is too small.
 */
SN_INLINE void sn_static_logger_log(SnStaticLogger *logger, SnLogLevel level, const char *fmt, ...) {
    if (level < logger->level) return;

    va_list args;
    va_start(args, fmt);
    sn_static_logger_log_va(logger, level, fmt, args);
    va_end(args);
}

/**
 * @brief Log a raw message without formatting.
 *
 * Emits the provided message directly to all sinks.
 *
 * @param logger Pointer to the logger context
 * @param level Log level of the message
 * @param msg Pointer to the message data
 * @param len Length of the message in bytes
 *
 * @note No formatting or null-termination is assumed.
 */
SN_LOGGER_API void
    sn_static_logger_log_raw(SnStaticLogger *logger, SnLogLevel level, const char *msg, size_t len);

