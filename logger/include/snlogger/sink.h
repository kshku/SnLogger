#pragma once

#include "snlogger/log_level.h"

#include <stddef.h>

/**
 * @brief Sink write function for the logger.
 *
 * Writes a single log record to the sink.
 *
 * @param msg Pointer to the log message buffer
 * @param len Length of the message in bytes
 * @param level Log level of the message
 * @param data User-defined sink data
 *
 * @note The message is not guaranteed to be null-terminated.
 * @note The sink must not read beyond @p len bytes.
 *
 * @note When used with the static logger, this function must not:
 *       - allocate dynamic memory
 *       - call the logger directly or indirectly
 *
 * @note Blocking behavior is sink-defined.
 */
typedef void (*snSinkWriteFn)(const char *msg, size_t len, snLogLevel level, void *data);

/**
 * @brief Sink open callback.
 *
 * Called once during logger initialization.
 *
 * Can be used to initialize sink state, open files, or prepare ersources.
 *
 * @param data User-defined sink data
 *
 * @note Optional.
 *
 * @note When used with the static logger, this function must not:
 *       - allocate dynamic memory
 *       - call the logger directly or indirectly
 */
typedef void (*snSinkOpenFn)(void *data);

/**
 * @brief Sink close callback.
 *
 * Called once during logger deinitialization, after flushing.
 *
 * Can be used to release resources or close files.
 *
 * @param data User-defined sink data
 *
 * @note Optional.
 *
 * @note When used with the static logger, this function must not:
 *       - allocate dynamic memory
 *       - call the logger directly or indirectly
 */
typedef void (*snSinkCloseFn)(void *data);

/**
 * @brief Sink flush callback.
 *
 * Flushes any internal sink buffers.
 *
 * Called:
 * - when the user explicitly flushes the logger
 * - just before calling the sink close function
 *
 * @param data User-defined sink data
 *
 * @note Optional.
 *
 * @note When used with the static logger, this function must not:
 *       - allocate dynamic memory
 *       - call the logger directly or indirectly
 */
typedef void (*snSinkFlushFn)(void *data);

/**
 * @struct snSink sink.h <snlogger/sink.h>
 * @brief Log output sink.
 *
 * A sink represents a destination for log records.
 *
 * The logger does not impose any threading, buffering, or blocking behavior.
 * All such behavior is defined entirely by the sink implementation.
 *
 * Sink lifecycle:
 * - @c open  is called during logger initialization (if provided)
 * - @c write is called for each log record (required)
 * - @c flush may be called explicitly or before shutdown (if provided)
 * - @c close is called during logger deinitialization (if provided)
 *
 * @note The logger itself is not thread-safe unless explicitly stated.
 *       Sink implementations must handle their own synchronization if needed.
 */
typedef struct snSink {
    snSinkOpenFn  open;   /**< Optional sink initialization callback */
    snSinkWriteFn write;  /**< Required sink write callback */
    snSinkCloseFn close;  /**< Optional sink shutdown callback */
    snSinkFlushFn flush;  /**< Optional sink flush callback */
    void *data;           /**< User-defined sink data */
} snSink;

