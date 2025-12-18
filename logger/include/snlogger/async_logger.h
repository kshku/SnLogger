#pragma once

#include "snlogger/log_level.h"
#include "snlogger/sink.h"

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

/**
 * @brief Memory allocation hook used by the async logger.
 *
 * @param size Number of bytes to allocate.
 * @param data User-provided memory context.
 *
 * @return Pointer to allocated memory, or NULL on failure.
 *
 * @note Optional. The async logger does not allocate memory unless explicitly
 *       required by its implementation.
 * @note Must not call the logger directly or indirectly.
 */
typedef void *(*snMemoryAllocateFn)(size_t size, void *data);

/**
 * @brief Memory free hook used by the async logger.
 *
 * @param ptr Pointer previously returned by snMemoryAllocateFn.
 * @param data User-provided memory context.
 *
 * @note Optional.
 * @note Must not call the logger directly or indirectly.
 */
typedef void (*snMemoryFreeFn)(void *ptr, void *data);

/**
 * @brief Lock hook used to protect async logger operations.
 *
 * @param data User-provided lock context.
 *
 * @note Optional. If not provided, the async logger performs no synchronization.
 * @note Must not call the logger directly or indirectly.
 */
typedef void (*snLockFn)(void *data);

/**
 * @brief Unlock hook used to release the async logger lock.
 *
 * @param data User-provided lock context.
 *
 * @note Optional. Must correspond to snLockFn.
 * @note Must not call the logger directly or indirectly.
 */
typedef void (*snUnlockFn)(void *data);

/**
 * @struct snLogRecordHeader
 * @brief Header stored before each log record in the async logger buffer.
 *
 * This header is immediately followed in memory by the log message payload
 * of @ref len bytes.
 *
 * The message payload is not required to be null-terminated.
 */
typedef struct snLogRecordHeader {
    snLogLevel level;   /**< Log level of the record */
    uint64_t timestamp; /**< Timestamp associated with the record */
    size_t len;         /**< Length of the message payload in bytes */
} snLogRecordHeader;

/**
 * @struct snLogRecordHeapNode
 * @brief Node to store log record in heap.
 */
typedef struct snLogRecordHeapNode {
    struct snLogRecordHeapNode *next; /**< Pointer to next node */
    snLogRecordHeader *record; /**< Pointer to log record header */
} snLogRecordHeapNode;

/**
 * @struct snAsyncLogger async_logger.h <snlogger/async_logger.h>
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
typedef struct snAsyncLogger {
    snLogLevel level; /**< Global log level */

    snSink *sinks; /**< List of sinks */
    size_t sink_count; /**< Number of sinks */

    void *buffer; /**< Ring buffer storage */
    size_t buffer_size; /**< Total size of the ring buffer in bytes */
    size_t write_offset; /**< Current write position within the buffer */
    size_t read_offset; /**< Current read position within the buffer */

    snLogRecordHeapNode *heap_head; /**< Overflow heap list head */
    snLogRecordHeapNode *heap_tail; /**< Overflow heap list tail */

    uint64_t timestamp; /**< Monotonic record counter */
    uint64_t processed_timestamp; /**< Last processed record */

    snLockFn lock; /**< Optional lock function */
    snUnlockFn unlock; /**< Optional unlock function */
    void *lock_data; /**< User data passed to lock functions */

    snMemoryAllocateFn alloc; /**< Optional memory allocation hook */
    snMemoryFreeFn free; /**< Optional memory free hook */
    void *mem_data; /**< User data passed to memory hooks */

    size_t dropped; /**< Number of logs dropped */
} snAsyncLogger;

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
void sn_async_logger_init(snAsyncLogger *logger, void *buffer, size_t buffer_size, snSink *sinks, size_t sink_count);

/**
 * @brief Deinitialize the async logger.
 *
 * Processes any remaining queued log records, flushes all sinks,
 * and closes them.
 *
 * @param logger Pointer to the async logger context.
 */
void sn_async_logger_deinit(snAsyncLogger *logger);

/**
 * @brief Set memory hooks for the async logger.
 *
 * @param logger Pointer to the async logger context.
 * @param alloc Memory allocation function.
 * @param free Memory free function.
 * @param data User-provided memory context.
 *
 * @note Optional. The async logger functions without memory hooks.
 */
void sn_async_logger_set_memory_hooks(snAsyncLogger *logger, snMemoryAllocateFn alloc, snMemoryFreeFn free, void *data);

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
void sn_async_logger_set_lock_hooks(snAsyncLogger *logger, snLockFn lock, snUnlockFn unlock, void *data);

/**
 * @brief Set the global log level.
 *
 * @param logger Pointer to the async logger context.
 * @param level New log level.
 */
void sn_async_logger_set_level(snAsyncLogger *logger, snLogLevel level);

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
void sn_async_logger_log(snAsyncLogger *logger, snLogLevel level, const char *fmt, ...);

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
void sn_async_logger_log_va(snAsyncLogger *logger, snLogLevel level, const char *fmt, va_list args);

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
void sn_async_logger_log_raw(snAsyncLogger *logger, snLogLevel level, const char *msg, size_t len);

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
size_t sn_async_logger_process(snAsyncLogger *logger);

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
size_t sn_async_logger_process_n(snAsyncLogger *logger, size_t n);

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
size_t sn_async_logger_drain(snAsyncLogger *logger);

/**
 * @brief Flush all sinks.
 *
 * Calls flush on all the sinks if provided.
 *
 * @param logger Pointer to the async logger context.
 */
void sn_async_logger_flush(snAsyncLogger *logger);

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
void sn_async_logger_drain_and_flush(snAsyncLogger *logger);

