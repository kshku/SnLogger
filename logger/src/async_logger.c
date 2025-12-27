#include "snlogger/async_logger.h"

#include "snlogger/formatter.h"

#include <string.h>

#define async_logger_lock(logger) if (logger->lock) logger->lock(logger->lock_data)

#define async_logger_unlock(logger) if (logger->unlock) logger->unlock(logger->lock_data)

#define GET_ALIGNED(x, align) (((size_t)(x) + (align) - 1) & ~((align) - 1))
#define PTR_BYTE_DIFF(x, y) (((size_t)x) - ((size_t)y))

static size_t ring_buffer_free_size(snAsyncLogger *logger) {
    if (logger->write_offset >= logger->read_offset)
        return logger->buffer_size - (logger->write_offset - logger->read_offset);

    return logger->read_offset - logger->write_offset;
}

static snLogRecordHeader *ring_buffer_allocate(snAsyncLogger *logger, size_t size) {
    size += alignof(snLogRecordHeader);

    size_t free = ring_buffer_free_size(logger);

    if (free < size) return NULL;

    if ((logger->write_offset >= logger->read_offset && logger->write_offset + size <= logger->buffer_size) || 
            (logger->write_offset < logger->read_offset && logger->write_offset + size < logger->read_offset)) {
        void *p = ((char *)logger->buffer) + logger->write_offset;
        void *aligned = (void *)GET_ALIGNED(p, alignof(snLogRecordHeader));
        logger->write_offset += size - alignof(snLogRecordHeader) + PTR_BYTE_DIFF(aligned, p);
        return (snLogRecordHeader *)aligned;
    }

    // Wrapping logic
    if (logger->write_offset >= logger->read_offset && logger->read_offset > size) {
        // Write the wrap mark
        if (logger->write_offset < logger->buffer_size) {
            void *ptr = ((char *)logger->buffer) + logger->write_offset;
            snLogRecordHeader *wrap_mark = (snLogRecordHeader *)GET_ALIGNED(ptr, alignof(snLogRecordHeader));
            logger->write_offset += PTR_BYTE_DIFF(wrap_mark, ptr);
            if (logger->write_offset < logger->buffer_size && logger->buffer_size - logger->write_offset >= sizeof(snLogRecordHeader))
                wrap_mark->level = SN_LOG_LEVEL_FATAL + 1;
        }
        void *aligned = (void *)GET_ALIGNED(logger->buffer, alignof(snLogRecordHeader));
        logger->write_offset = size - alignof(snLogRecordHeader) + PTR_BYTE_DIFF(aligned, logger->buffer);
        return (snLogRecordHeader *)aligned;
    }

    return NULL;
}

static snLogRecordHeapNode *try_heap_allocation(snAsyncLogger *logger, size_t len) {
    if (!logger->alloc) return NULL;

    size_t alloc_size = sizeof(snLogRecordHeapNode) + sizeof(snLogRecordHeader) + len;
    snLogRecordHeapNode *node = logger->alloc(alloc_size, alignof(snLogRecordHeader), logger->mem_data);

    if (!node) return NULL;

    node->next = NULL;

    node->record = (snLogRecordHeader *)(node + 1);

    if (logger->heap_tail)
        logger->heap_tail->next = node;
    else
        logger->heap_head = node;
    logger->heap_tail = node;

    return node;
}

void sn_async_logger_init(snAsyncLogger *logger, void *buffer, size_t buffer_size, snSink *sinks, size_t sink_count) {
    *logger = (snAsyncLogger){
        .level = SN_LOG_LEVEL_TRACE,

        .sinks = sinks,
        .sink_count = sink_count,

        .buffer = buffer,
        .buffer_size = buffer_size,
        .write_offset = 0,
        .read_offset = 0,

        .timestamp = 1,
        .processed_timestamp = 0,
    };

    for (size_t i = 0; i < sink_count; ++i)
        if (sinks[i].open) sinks[i].open(sinks[i].data);
}

void sn_async_logger_deinit(snAsyncLogger *logger) {
    while (sn_async_logger_process(logger));

    for (size_t i = 0; i < logger->sink_count; ++i) {
        if (logger->sinks[i].flush) logger->sinks[i].flush(logger->sinks[i].data);
        if (logger->sinks[i].close) logger->sinks[i].close(logger->sinks[i].data);
    }

    *logger = (snAsyncLogger){0};
}

void sn_async_logger_log_va(snAsyncLogger *logger, snLogLevel level, const char *fmt, va_list args) {
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
    size_t record_size = sizeof(snLogRecordHeader) + len;
    snLogRecordHeader *record = ring_buffer_allocate(logger, record_size);

    if (record) {
        record->level = level;
        record->len = len;
        record->timestamp = logger->timestamp++;
        format_string((char *)(record + 1), len, fmt, args);

        async_logger_unlock(logger);
        return;
    }

    snLogRecordHeapNode *node = try_heap_allocation(logger, len);
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

void sn_async_logger_log_raw(snAsyncLogger *logger, snLogLevel level, const char *msg, size_t len) {
    if (level < logger->level) return;

    async_logger_lock(logger);

    size_t record_size = sizeof(snLogRecordHeader) + len;
    snLogRecordHeader *record = ring_buffer_allocate(logger, record_size);

    if (record) {
        record->level = level;
        record->len = len;
        record->timestamp = logger->timestamp++;
        memcpy((void *)(record + 1), msg, len * sizeof(char));

        async_logger_unlock(logger);
        return;
    }

    snLogRecordHeapNode *node = try_heap_allocation(logger, len);
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

size_t sn_async_logger_process_n(snAsyncLogger *logger, size_t n) {
    size_t count = 0;

    async_logger_lock(logger);

    while (logger->read_offset != logger->write_offset && count < n) {
        void *read_ptr = ((char *)logger->buffer) + logger->read_offset;
        snLogRecordHeader *record = (snLogRecordHeader *)GET_ALIGNED(read_ptr, alignof(snLogRecordHeader));
        logger->read_offset += PTR_BYTE_DIFF(record, read_ptr);

        if (logger->read_offset >= logger->buffer_size || logger->buffer_size - logger->read_offset < sizeof(snLogRecordHeader)) {
            // Next record should start from 0 itself
            logger->read_offset = 0;
            continue;
        }

        // Check for wrap mark
        if (record->level == SN_LOG_LEVEL_FATAL + 1) {
            logger->read_offset = 0;
            continue;
        }

        // maintain the order
        if (logger->processed_timestamp + 1 != record->timestamp) break;
        logger->processed_timestamp = record->timestamp; // or just logger->processed_timestamp++;

        async_logger_unlock(logger);

        for (size_t i = 0; i < logger->sink_count; ++i)
            logger->sinks[i].write((const char *)(record + 1), record->len, record->level, logger->sinks[i].data);

        ++count;

        async_logger_lock(logger);

        logger->read_offset += sizeof(snLogRecordHeader) + record->len;
    }

    while (logger->heap_head && count < n) {
        snLogRecordHeapNode *node = logger->heap_head;
        // maintain the order
        if (logger->processed_timestamp + 1 != node->record->timestamp) break;
        logger->processed_timestamp = node->record->timestamp; // or just logger->processed_timestamp++;

        logger->heap_head = node->next;

        if (!logger->heap_head) logger->heap_tail = NULL;

        async_logger_unlock(logger);

        for (size_t i = 0; i < logger->sink_count; ++i)
            logger->sinks[i].write((const char *)(node->record + 1), node->record->len, node->record->level, logger->sinks[i].data);

        if (logger->free) logger->free(node, logger->mem_data);
        ++count;

        async_logger_lock(logger);
    }

    async_logger_unlock(logger);

    return count;
}

size_t sn_async_logger_drain(snAsyncLogger *logger) {
    size_t total = 0;
    size_t count;
    do {
        count = sn_async_logger_process(logger);
        total += count;
    } while (count);

    return total;
}

void sn_async_logger_flush(snAsyncLogger *logger) {
    for (size_t i = 0; i < logger->sink_count; ++i)
        if (logger->sinks[i].flush) logger->sinks[i].flush(logger->sinks[i].data);
}

void sn_async_logger_drain_and_flush(snAsyncLogger *logger) {
    while (sn_async_logger_process(logger));

    for (size_t i = 0; i < logger->sink_count; ++i)
        if (logger->sinks[i].flush) logger->sinks[i].flush(logger->sinks[i].data);
}

