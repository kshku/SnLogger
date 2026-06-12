#pragma once

#include "snlogger/api.h"
#include "snlogger/log_level.h"
#include "snlogger/sink.h"

#include <sncore/defines.h>
#include <sncore/types.h>
#include <snmemory/ring_buffer.h>
#include <stdarg.h>

/**
 * @struct SnLogRecordHeader
 * @brief Header stored before each log record in the async logger buffer.
 *
 * This header is immediately followed in memory by the log message payload
 * of @ref len bytes.
 *
 * The message payload is not required to be null-terminated.
 */
typedef struct SnLogRecordHeader {
    SnLogLevel level; /**< Log level of the record */
    uint64_t timestamp; /**< Timestamp associated with the record */
    size_t len; /**< Length of the message payload in bytes */
} SnLogRecordHeader;

/**
 * @struct SnLogRecordHeapNode
 * @brief Node to store log record in heap.
 */
typedef struct SnLogRecordHeapNode {
    struct SnLogRecordHeapNode *next; /**< Pointer to next node */
    SnLogRecordHeader *record; /**< Pointer to log record header */
} SnLogRecordHeapNode;

/**
 * @struct SnAsyncLogger async_logger.h <snlogger/async_logger.h>
 * @brief Asynchronous logger using a fixed-size ring buffer.
 *
 * The async logger enqueues log records into a ring buffer and processes
 * them later when explicitly requested by the caller.
 *
 * The logger:
 * - Does not create threads
 * - Does not block internally
 * - Does not perform I/O during enqueue
 *
 * Thread safety:
 * - Not thread-safe by default
 * - Thread-safe only when lock hooks are installed
 * - Lock hooks must protect both producers and consumers
 */
typedef struct SnAsyncLogger {
    SnLogLevel level; /**< Global log level */

    SnSink *sinks; /**< List of sinks */
    size_t sink_count; /**< Number of sinks */

    SnRingBufferAllocator ring_buffer; /**< Ring buffer */

    SnLogRecordHeapNode *heap_head; /**< Overflow heap list head */
    SnLogRecordHeapNode *heap_tail; /**< Overflow heap list tail */

    uint64_t timestamp; /**< Monotonic record counter */
    uint64_t processed_timestamp; /**< Last processed record */

    SnLockFn lock; /**< Optional lock function */
    SnUnlockFn unlock; /**< Optional unlock function */
    void *lock_data; /**< User data passed to lock functions */

    SnMemoryAllocator *allocator; /**< Optional memory allocator to use (realloc is not used) */

    size_t dropped; /**< Number of logs dropped */
} SnAsyncLogger;

/**
 * @brief Initialize an async logger.
 *
 * @param logger Pointer to the async logger context.
 * @param buffer Pointer to the ring buffer storage.
 * @param buffer_size Size of the ring buffer in bytes.
 * @param sinks Array of sinks used for output.
 * @param sink_count Number of sinks in the array.
 *
 * @note The buffer must remain valid for the lifetime of the logger.
 * @note This function does not allocate memory or start any threads.
 */
SN_LOGGER_API void sn_async_logger_init(
    SnAsyncLogger *logger, void *buffer, size_t buffer_size, SnSink *sinks, size_t sink_count);

/**
 * @brief Deinitialize the async logger.
 *
 * Processes any remaining queued log records, flushes all sinks,
 * and closes them.
 *
 * @param logger Pointer to the async logger context.
 */
SN_LOGGER_API void sn_async_logger_deinit(SnAsyncLogger *logger);

/**
 * @brief Set memory allocator for the async logger.
 *
 * @param logger Pointer to the async logger context.
 * @param alloc Memory allocation function.
 * @param free Memory free function.
 * @param data User-provided memory context.
 *
 * @note Optional. The async logger functions without memory allocator.
 */
SN_FORCE_INLINE void sn_async_logger_set_memory_allocator(SnAsyncLogger *logger, SnMemoryAllocator *allocator) {
    logger->allocator = allocator;
}

/**
 * @brief Set lock hooks for the async logger.
 *
 * When lock hooks are installed, the async logger uses them to serialize
 * access to its internal state, enabling thread-safe usage.
 *
 * If no lock hooks are provided, the async logger performs no synchronization
 * and concurrent access results in undefined behavior.
 *
 * @param logger Pointer to the async logger context.
 * @param lock Lock function.
 * @param unlock Unlock function.
 * @param data User-provided lock context.
 *
 * @note Optional. Lock hooks must protect both producer and consumer calls.
 * @note Lock functions must not call the logger directly or indirectly.
 */
SN_FORCE_INLINE void
    sn_async_logger_set_lock_hooks(SnAsyncLogger *logger, SnLockFn lock, SnUnlockFn unlock, void *data) {
    logger->lock = lock;
    logger->unlock = unlock;
    logger->lock_data = data;
}

/**
 * @brief Set the global log level.
 *
 * @param logger Pointer to the async logger context.
 * @param level New log level.
 */
SN_FORCE_INLINE void sn_async_logger_set_level(SnAsyncLogger *logger, SnLogLevel level) {
    logger->level = level;
}

/**
 * @brief Enqueue a formatted log message using a va_list.
 *
 * @param logger Pointer to the async logger context.
 * @param level Log level of the message.
 * @param fmt Format string.
 * @param args Argument list.
 *
 * @note This function only enqueues the message. It does not write to sinks.
 * @note This function is not thread-safe unless lock hooks are installed
 *       or external synchronization is provided by the caller.
 */
SN_LOGGER_API void
    sn_async_logger_log_va(SnAsyncLogger *logger, SnLogLevel level, const char *fmt, va_list args);

/**
 * @brief Enqueue a formatted log message.
 *
 * Formats the message and stores it in the internal buffers.
 *
 * @param logger Pointer to the async logger context.
 * @param level Log level of the message.
 * @param fmt Format string.
 * @param ... Format arguments.
 *
 * @note This function only enqueues the message. It does not write to sinks.
 * @note This function is not thread-safe unless lock hooks are installed
 *       or external synchronization is provided by the caller.
 * @note Records are processed only when sn_async_logger_process*() is called.
 */
SN_INLINE void sn_async_logger_log(SnAsyncLogger *logger, SnLogLevel level, const char *fmt, ...) {
    if (level < logger->level) return;

    va_list args;
    va_start(args, fmt);
    sn_async_logger_log_va(logger, level, fmt, args);
    va_end(args);
}

/**
 * @brief Enqueue a raw log message without formatting.
 *
 * @param logger Pointer to the async logger context.
 * @param level Log level of the message.
 * @param msg Pointer to the message data.
 * @param len Length of the message in bytes.
 *
 * @note The message is copied into the async logger buffer.
 * @note This function is not thread-safe unless lock hooks are installed
 *       or external synchronization is provided by the caller.
 */
SN_LOGGER_API void
    sn_async_logger_log_raw(SnAsyncLogger *logger, SnLogLevel level, const char *msg, size_t len);

/**
 * @brief Process at max n queued log records.
 *
 * Writes queued records to all sinks in order.
 *
 * @param logger Pointer to the async logger context.
 * @param n Number of records to process at max.
 *
 * @return Number of log records processed.
 *
 * @note Intended to be called by a user-managed consumer thread or loop.
 * @note This function is not thread-safe unless lock hooks are installed
 *       or external synchronization is provided by the caller.
 */
SN_LOGGER_API size_t sn_async_logger_process_n(SnAsyncLogger *logger, size_t n);

/**
 * @brief Process queued log records.
 *
 * Writes queued records to all sinks in order.
 *
 * @param logger Pointer to the async logger context.
 *
 * @return Number of log records processed.
 *
 * @note Intended to be called by a user-managed consumer thread or loop.
 * @note This function is not thread-safe unless lock hooks are installed
 *       or external synchronization is provided by the caller.
 */
SN_FORCE_INLINE size_t sn_async_logger_process(SnAsyncLogger *logger) {
    return sn_async_logger_process_n(logger, -1);
}

/**
 * @brief Process log records until the logger becomes empty.
 *
 * Records enqueued while draining MAY also be processed.
 * This function returns only when no records are available
 * at the time of checking.
 *
 * Writes queued records to all sinks in order.
 *
 * @param logger Pointer to the async logger context.
 *
 * @return Number of log records processed.
 *
 * @note Intended to be called by a user-managed consumer thread or loop.
 * @note This function is not thread-safe unless lock hooks are installed
 *       or external synchronization is provided by the caller.
 */
SN_LOGGER_API size_t sn_async_logger_drain(SnAsyncLogger *logger);

/**
 * @brief Flush all sinks.
 *
 * Calls flush on all the sinks if provided.
 *
 * @param logger Pointer to the async logger context.
 */
SN_LOGGER_API void sn_async_logger_flush(SnAsyncLogger *logger);

/**
 * @brief Process all queued log records and flush all sinks.
 *
 * Records enqueued while draining MAY also be processed.
 *
 * @param logger Pointer to the async logger context.
 *
 * @note Does not synchronize with producers unless user-provided locks
 *       guarantee it.
 */
SN_LOGGER_API void sn_async_logger_drain_and_flush(SnAsyncLogger *logger);

