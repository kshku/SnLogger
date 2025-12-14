#pragma once

#include "snlogger/log_level.h"
#include "snlogger/sink.h"

#include <stdarg.h>
#include <stddef.h>


/**
 * @struct snStaticLogger static_logger.h <snlogger/static_logger.h>
 * @brief Simple synchronous logger using a user-provided static buffer.
 *
 * snStaticLogger formats log messages into a shared buffer and immediately
 * emits them to all configured sinks.
 *
 * Characteristics: * - Synchronous (no background threads)
 * - Not thread-safe
 * - Uses a single shared formatting buffer
 * - Sink behavior (blocking, buffering, async) is sink-defined
 *
 * The logger does not perform any internal locking or buffering beyond
 * formatting into the provided buffer.
 */
typedef struct snStaticLogger {
    char *buffer; /**< The buffer used by logger */
    size_t buffer_size; /**< The size of the buffer */

    snSink *sinks; /**< Array of sinks */
    size_t sink_count; /**< Number of sinks */

    snLogLevel level; /**< The global log level threashold */

    // size_t dropped;
    // size_t truncated;
} snStaticLogger;

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
void sn_static_logger_init(snStaticLogger *logger, char *buffer, size_t buffer_size,
        snSink *sinks, size_t sink_count);

/**
 * @brief Deinitialize the static logger.
 *
 * This function:
 * - Calls `flush` on each sink (if provided)
 * - Calls `close` on each sink (if provided)
 *
 * @param logger Pointer to the logger context
 */
void sn_static_logger_deinit(snStaticLogger *logger);

/**
 * @brief Flush all sinks.
 *
 * Calls the `flush` callback on each sink if it exists.
 *
 * @param logger Pointer to the logger context
 */
void sn_static_logger_flush(snStaticLogger *logger);

/**
 * @brief Set the global log level.
 *
 * Messages with a level lower than this value will be ignored.
 *
 * @param logger Pointer to the logger context
 * @param level New log level threshold
 */
void sn_static_logger_set_level(snStaticLogger *logger, snLogLevel level);

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
 * @note If `level` is below the logger's current level, the call is ignored.
 * @note This function is not thread-safe.
 */void sn_static_logger_log(snStaticLogger *logger, snLogLevel level, const char *fmt, ...);

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
 */void sn_static_logger_log_va(snStaticLogger *logger, snLogLevel level, const char *fmt, va_list args);

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
void sn_static_logger_log_raw(snStaticLogger *logger, snLogLevel level, const char *msg, size_t len);

