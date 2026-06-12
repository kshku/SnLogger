#include "snlogger/async_logger.h"

#include "snlogger/formatter.h"

#include <string.h>

#define async_logger_lock(logger)                     \
    if (logger->lock) logger->lock(logger->lock_data)

#define async_logger_unlock(logger)                       \
    if (logger->unlock) logger->unlock(logger->lock_data)

static SnLogRecordHeader *ring_buffer_allocate(SnAsyncLogger *logger, size_t size) {
    size += alignof(SnLogRecordHeader);

    uint64_t free = sn_ring_buffer_allocator_free_size(&logger->ring_buffer);

    if (free < size) return NULL;

    if ((logger->ring_buffer.write_offset >= logger->ring_buffer.read_offset
         && logger->ring_buffer.write_offset + size <= logger->ring_buffer.size)
        || (logger->ring_buffer.write_offset < logger->ring_buffer.read_offset
            && logger->ring_buffer.write_offset + size < logger->ring_buffer.read_offset)) {
        void *p = (char *)logger->ring_buffer.buffer + logger->ring_buffer.write_offset;
        void *aligned = (void *)SN_GET_ALIGNED(p, alignof(SnLogRecordHeader));
        logger->ring_buffer.write_offset += size - alignof(SnLogRecordHeader) + SN_PTR_DIFF(aligned, p);
        return (SnLogRecordHeader *)aligned;
    }

    // Wrapping logic
    if (logger->ring_buffer.write_offset >= logger->ring_buffer.read_offset
        && logger->ring_buffer.read_offset > size) {
        // Write the wrap mark
        if (logger->ring_buffer.write_offset < logger->ring_buffer.size) {
            void *ptr = (char *)logger->ring_buffer.buffer + logger->ring_buffer.write_offset;
            SnLogRecordHeader *wrap_mark
                = (SnLogRecordHeader *)SN_GET_ALIGNED(ptr, alignof(SnLogRecordHeader));
            logger->ring_buffer.write_offset += SN_PTR_DIFF(wrap_mark, ptr);
            if (logger->ring_buffer.write_offset < logger->ring_buffer.size
                && logger->ring_buffer.size - logger->ring_buffer.write_offset >= sizeof(SnLogRecordHeader))
                wrap_mark->level = SN_LOG_LEVEL_FATAL + 1;
        }
        void *aligned = (void *)SN_GET_ALIGNED(logger->ring_buffer.buffer, alignof(SnLogRecordHeader));
        logger->ring_buffer.write_offset
            = size - alignof(SnLogRecordHeader) + SN_PTR_DIFF(aligned, logger->ring_buffer.buffer);
        return (SnLogRecordHeader *)aligned;
    }

    return NULL;
}

static SnLogRecordHeapNode *try_heap_allocation(SnAsyncLogger *logger, size_t len) {
    if (!logger->allocator) return NULL;

    size_t alloc_size = sizeof(SnLogRecordHeapNode) + sizeof(SnLogRecordHeader) + len;
    SnLogRecordHeapNode *node
        = logger->allocator->alloc(logger->allocator->data, alloc_size, alignof(SnLogRecordHeader));

    if (!node) return NULL;

    node->next = NULL;

    node->record = (SnLogRecordHeader *)(node + 1);

    if (logger->heap_tail) logger->heap_tail->next = node;
    else logger->heap_head = node;
    logger->heap_tail = node;

    return node;
}

void sn_async_logger_init(SnAsyncLogger *logger, void *buffer, size_t buffer_size, SnSink *sinks, size_t sink_count) {
    *logger = (SnAsyncLogger){
        .level = SN_LOG_LEVEL_TRACE,

        .sinks = sinks,
        .sink_count = sink_count,

        .timestamp = 1,
        .processed_timestamp = 0,
    };

    sn_ring_buffer_allocator_init(&logger->ring_buffer, buffer, buffer_size);

    for (size_t i = 0; i < sink_count; ++i)
        if (sinks[i].open) sinks[i].open(sinks[i].data);
}

void sn_async_logger_deinit(SnAsyncLogger *logger) {
    while (sn_async_logger_process(logger));

    for (size_t i = 0; i < logger->sink_count; ++i) {
        if (logger->sinks[i].flush) logger->sinks[i].flush(logger->sinks[i].data);
        if (logger->sinks[i].close) logger->sinks[i].close(logger->sinks[i].data);
    }

    *logger = (SnAsyncLogger){0};
}

void sn_async_logger_log_va(SnAsyncLogger *logger, SnLogLevel level, const char *fmt, va_list args) {
    if (level < logger->level) return;

    va_list args_copy;
    va_copy(args_copy, args);
    // TODO: Temporary fix: vsnprintf writes upto len bytes including null character
    size_t len = format_string(NULL, 0, fmt, args_copy) + 1;
    va_end(args_copy);
    if (len == 0) {
        logger->dropped++;
        return;
    }

    async_logger_lock(logger);
    size_t record_size = sizeof(SnLogRecordHeader) + len;
    SnLogRecordHeader *record = ring_buffer_allocate(logger, record_size);

    if (record) {
        record->level = level;
        record->len = len;
        record->timestamp = logger->timestamp++;
        format_string((char *)(record + 1), len, fmt, args);

        async_logger_unlock(logger);
        return;
    }

    SnLogRecordHeapNode *node = try_heap_allocation(logger, len);
    if (node) {
        node->record->level = level;
        node->record->len = len;
        node->record->timestamp = logger->timestamp++;
        format_string((char *)(node->record + 1), len, fmt, args);

        async_logger_unlock(logger);
        return;
    }

    logger->dropped++;
    async_logger_unlock(logger);
}

void sn_async_logger_log_raw(SnAsyncLogger *logger, SnLogLevel level, const char *msg, size_t len) {
    if (level < logger->level) return;

    async_logger_lock(logger);

    size_t record_size = sizeof(SnLogRecordHeader) + len;
    SnLogRecordHeader *record = ring_buffer_allocate(logger, record_size);

    if (record) {
        record->level = level;
        record->len = len;
        record->timestamp = logger->timestamp++;
        memcpy((void *)(record + 1), msg, len * sizeof(char));

        async_logger_unlock(logger);
        return;
    }

    SnLogRecordHeapNode *node = try_heap_allocation(logger, len);
    if (node) {
        node->record->level = level;
        node->record->len = len;
        node->record->timestamp = logger->timestamp++;
        memcpy((void *)(node->record + 1), msg, len * sizeof(char));

        async_logger_unlock(logger);
        return;
    }

    logger->dropped++;
    async_logger_unlock(logger);
}

size_t sn_async_logger_process_n(SnAsyncLogger *logger, size_t n) {
    size_t count = 0;

    async_logger_lock(logger);

    while (logger->ring_buffer.read_offset != logger->ring_buffer.write_offset && count < n) {
        void *read_ptr = sn_ring_buffer_allocator_read_ptr(&logger->ring_buffer);
        SnLogRecordHeader *record = (SnLogRecordHeader *)SN_GET_ALIGNED(read_ptr, alignof(SnLogRecordHeader));
        logger->ring_buffer.read_offset += SN_PTR_DIFF(record, read_ptr);

        if (logger->ring_buffer.read_offset >= logger->ring_buffer.size
            || logger->ring_buffer.size - logger->ring_buffer.read_offset < sizeof(SnLogRecordHeader)) {
            // Next record should start from 0 itself
            logger->ring_buffer.read_offset = 0;
            continue;
        }

        // Check for wrap mark
        if (record->level == SN_LOG_LEVEL_FATAL + 1) {
            logger->ring_buffer.read_offset = 0;
            continue;
        }

        // maintain the order
        if (logger->processed_timestamp + 1 != record->timestamp) break;
        logger->processed_timestamp = record->timestamp;

        async_logger_unlock(logger);

        for (size_t i = 0; i < logger->sink_count; ++i)
            logger->sinks[i].write(
                (const char *)(record + 1), record->len, record->level, logger->sinks[i].data);

        ++count;

        async_logger_lock(logger);

        logger->ring_buffer.read_offset += sizeof(SnLogRecordHeader) + record->len;
    }

    while (logger->heap_head && count < n) {
        SnLogRecordHeapNode *node = logger->heap_head;
        // maintain the order
        if (logger->processed_timestamp + 1 != node->record->timestamp) break;
        logger->processed_timestamp = node->record->timestamp;

        logger->heap_head = node->next;

        if (!logger->heap_head) logger->heap_tail = NULL;

        async_logger_unlock(logger);

        for (size_t i = 0; i < logger->sink_count; ++i)
            logger->sinks[i].write((const char *)(node->record + 1), node->record->len,
                                   node->record->level, logger->sinks[i].data);

        if (logger->allocator->free) logger->allocator->free(logger->allocator->data, node);
        ++count;

        async_logger_lock(logger);
    }

    async_logger_unlock(logger);

    return count;
}

size_t sn_async_logger_drain(SnAsyncLogger *logger) {
    size_t total = 0;
    size_t count;
    do {
        count = sn_async_logger_process(logger);
        total += count;
    } while (count);

    return total;
}

void sn_async_logger_flush(SnAsyncLogger *logger) {
    for (size_t i = 0; i < logger->sink_count; ++i)
        if (logger->sinks[i].flush) logger->sinks[i].flush(logger->sinks[i].data);
}

void sn_async_logger_drain_and_flush(SnAsyncLogger *logger) {
    while (sn_async_logger_process(logger));

    for (size_t i = 0; i < logger->sink_count; ++i)
        if (logger->sinks[i].flush) logger->sinks[i].flush(logger->sinks[i].data);
}
