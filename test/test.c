#define _GNU_SOURCE

#include <snlogger/snlogger.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_LOGS 100000
#define MAX_LEN  16

#define STATIC_BUF_SIZE 256

typedef struct {
    char logs[MAX_LOGS][MAX_LEN];
    size_t count;
} TestSink;

typedef struct {
    TestSink base;
    int flush_count;
} FlushSink;


static void test_sink_write(const char *msg, size_t len, snLogLevel level, void *data) {
    (void)level;
    TestSink *sink = data;

    if (sink->count >= MAX_LOGS)
        return;

    size_t copy = len < MAX_LEN - 1 ? len : MAX_LEN - 1;
    memcpy(sink->logs[sink->count], msg, copy);
    sink->logs[sink->count][copy] = 0;
    sink->count++;
}

static void flush_sink_flush(void *data) {
    FlushSink *fs = data;
    fs->flush_count++;
}

static void *malloc_wrapper(size_t size, void *data) {
    (void)data;
    void *p = malloc(size);
    if (!p) abort();
    return p;
}

static void free_wrapper(void *ptr, void *data) {
    (void)data;
    free(ptr);
}

typedef struct {
    pthread_mutex_t mutex;
} MutexCtx;

static void lock_wrapper(void *data) {
    MutexCtx *ctx = data;
    pthread_mutex_lock(&ctx->mutex);
}

static void unlock_wrapper(void *data) {
    MutexCtx *ctx = data;
    pthread_mutex_unlock(&ctx->mutex);
}

static void test_static_basic(void) {
    printf("Running test_static_basic...\n");

    char buffer[STATIC_BUF_SIZE];
    TestSink sink = {0};

    snSink sinks[] = {
        {.write = test_sink_write, .data = &sink}
    };

    snStaticLogger sl;
    sn_static_logger_init(&sl, buffer, sizeof(buffer), sinks, 1);

    sn_static_logger_log(&sl, SN_LOG_LEVEL_INFO, "hello");
    sn_static_logger_log(&sl, SN_LOG_LEVEL_INFO, "world");

    sn_static_logger_deinit(&sl);

    assert(sink.count == 2);
    assert(strcmp(sink.logs[0], "hello") == 0);
    assert(strcmp(sink.logs[1], "world") == 0);

    printf("✓ passed\n");
}

static void test_static_truncation(void) {
    printf("Running test_static_truncation...\n");

    char buffer[32]; // tiny on purpose
    TestSink sink = {0};

    snSink sinks[] = {
        {.write = test_sink_write, .data = &sink}
    };

    snStaticLogger sl;
    sn_static_logger_init(&sl, buffer, sizeof(buffer), sinks, 1);

    sn_static_logger_log(&sl, SN_LOG_LEVEL_INFO,
        "this message is definitely too long to fit");

    sn_static_logger_deinit(&sl);

    assert(sink.count == 1);
    assert(strlen(sink.logs[0]) > 0); // something was printed

    printf("✓ passed\n");
}

static void test_static_log_level(void) {
    printf("Running test_static_log_level...\n");

    char buffer[STATIC_BUF_SIZE];
    TestSink sink = {0};

    snSink sinks[] = {
        {.write = test_sink_write, .data = &sink}
    };

    snStaticLogger sl;
    sn_static_logger_init(&sl, buffer, sizeof(buffer), sinks, 1);

    sn_static_logger_set_level(&sl, SN_LOG_LEVEL_WARN);

    sn_static_logger_log(&sl, SN_LOG_LEVEL_INFO, "info");
    sn_static_logger_log(&sl, SN_LOG_LEVEL_ERROR, "error");

    sn_static_logger_deinit(&sl);

    assert(sink.count == 1);
    assert(strcmp(sink.logs[0], "error") == 0);

    printf("✓ passed\n");
}

static void test_async_single_thread_ordering(void) {
    printf("Running test_async_single_thread_ordering...\n");

    char buffer[4096];
    TestSink sink = {0};

    snSink sinks[] = {
        {.write = test_sink_write, .data = &sink}
    };

    snAsyncLogger al;
    sn_async_logger_init(&al, buffer, sizeof(buffer), sinks, 1);
    sn_async_logger_set_memory_hooks(&al, malloc_wrapper, free_wrapper, NULL);

    for (int i = 0; i < 1000; ++i) {
        sn_async_logger_log(&al, SN_LOG_LEVEL_INFO, "msg-%d", i);
        if (i % 7 == 0)
            sn_async_logger_process(&al);
    }

    sn_async_logger_deinit(&al);

    assert(sink.count == 1000);

    for (int i = 0; i < 1000; ++i) {
        char expected[32];
        snprintf(expected, sizeof(expected), "msg-%d", i);
        assert(strcmp(sink.logs[i], expected) == 0);
    }

    printf("✓ passed\n");
}

static void test_async_drop_behavior(void) {
    printf("Running test_async_drop_behavior...\n");

    char buffer[256]; // intentionally tiny
    TestSink sink = {0};

    snSink sinks[] = {
        {.write = test_sink_write, .data = &sink}
    };

    snAsyncLogger al;
    sn_async_logger_init(&al, buffer, sizeof(buffer), sinks, 1);

    for (int i = 0; i < 1000; ++i) {
        sn_async_logger_log(&al, SN_LOG_LEVEL_INFO, "long-message-%d-xxxxxxxxxxxxxxxx", i);
    }

    sn_async_logger_deinit(&al);

    assert(sink.count > 0);
    assert(al.dropped > 0);

    printf("✓ passed (dropped=%zu)\n", al.dropped);
}

static atomic_uint_fast64_t global_seq = 1;

typedef struct {
    snAsyncLogger *logger;
    int thread_id;
    int count;
} ProducerArgs;

static void *producer_thread(void *arg) {
    ProducerArgs *pa = arg;

    for (int i = 0; i < pa->count; ++i) {
        uint64_t seq = atomic_fetch_add(&global_seq, 1);
        sn_async_logger_log(
            pa->logger,
            SN_LOG_LEVEL_INFO,
            "%lu t%d-%d",
            seq,
            pa->thread_id,
            i
        );
    }
    return NULL;
}

typedef struct {
    snAsyncLogger *logger;
    atomic_int *done;
} ConsumerArgs;

static void *consumer_thread(void *arg) {
    ConsumerArgs *ca = arg;

    while (!atomic_load(ca->done)) {
        sn_async_logger_process(ca->logger);
        usleep(1000); // 1ms
    }

    // Final drain
    while (sn_async_logger_process(ca->logger));
    return NULL;
}

static void test_async_multi_producer_ordering(void) {
    printf("Running test_async_multi_producer_ordering...\n");

    enum {
        PRODUCERS = 4,
        MSGS_PER_PRODUCER = 5000
    };

    char buffer[16384];
    TestSink sink = {0};

    snSink sinks[] = {
        {.write = test_sink_write, .data = &sink}
    };

    snAsyncLogger al;
    sn_async_logger_init(&al, buffer, sizeof(buffer), sinks, 1);
    sn_async_logger_set_memory_hooks(&al, malloc_wrapper, free_wrapper, NULL);

    MutexCtx mctx;
    pthread_mutex_init(&mctx.mutex, NULL);
    sn_async_logger_set_lock_hooks(&al, lock_wrapper, unlock_wrapper, &mctx);

    pthread_t prod[PRODUCERS];
    ProducerArgs pargs[PRODUCERS];

    atomic_int done = 0;
    ConsumerArgs cargs = {.logger = &al, .done = &done};
    pthread_t consumer;

    pthread_create(&consumer, NULL, consumer_thread, &cargs);

    for (int i = 0; i < PRODUCERS; ++i) {
        pargs[i] = (ProducerArgs){
            .logger = &al,
            .thread_id = i,
            .count = MSGS_PER_PRODUCER
        };
        pthread_create(&prod[i], NULL, producer_thread, &pargs[i]);
    }

    for (int i = 0; i < PRODUCERS; ++i)
        pthread_join(prod[i], NULL);

    atomic_store(&done, 1);
    pthread_join(consumer, NULL);

    sn_async_logger_deinit(&al);
    pthread_mutex_destroy(&mctx.mutex);

    size_t expected = PRODUCERS * MSGS_PER_PRODUCER;
    bool found_seq[PRODUCERS * MSGS_PER_PRODUCER] = {0};

    assert(sink.count == expected);

    for (size_t i = 0; i < sink.count; ++i) {
        uint64_t seq;
        assert(sscanf(sink.logs[i], "%lu", &seq) == 1);
        assert(!found_seq[seq]);
        found_seq[seq] = true;
    }

    printf("✓ passed\n");
}

static void test_async_process_n(void) {
    printf("Running test_async_process_n...\n");

    char buffer[4096];
    TestSink sink = {0};

    snSink sinks[] = {
        {.write = test_sink_write, .data = &sink}
    };

    snAsyncLogger al;
    sn_async_logger_init(&al, buffer, sizeof(buffer), sinks, 1);

    for (int i = 0; i < 20; ++i)
        sn_async_logger_log(&al, SN_LOG_LEVEL_INFO, "msg-%d", i);

    size_t p1 = sn_async_logger_process_n(&al, 7);
    assert(p1 == 7);
    assert(sink.count == 7);

    size_t p2 = sn_async_logger_process_n(&al, 7);
    assert(p2 == 7);
    assert(sink.count == 14);

    size_t p3 = sn_async_logger_process_n(&al, 100);
    assert(p3 == 6);
    assert(sink.count == 20);

    sn_async_logger_deinit(&al);

    printf("✓ passed\n");
}

static void test_async_drain(void) {
    printf("Running test_async_drain...\n");

    char buffer[4096];
    TestSink sink = {0};

    snSink sinks[] = {
        {.write = test_sink_write, .data = &sink}
    };

    snAsyncLogger al;
    sn_async_logger_init(&al, buffer, sizeof(buffer), sinks, 1);

    for (int i = 0; i < 50; ++i)
        sn_async_logger_log(&al, SN_LOG_LEVEL_INFO, "msg-%d", i);

    size_t drained = sn_async_logger_drain(&al);

    assert(drained == 50);
    assert(sink.count == 50);

    sn_async_logger_deinit(&al);

    printf("✓ passed\n");
}

static void test_async_flush_only(void) {
    printf("Running test_async_flush_only...\n");

    char buffer[4096];
    FlushSink sink = {0};

    snSink sinks[] = {
        {
            .write = test_sink_write,
            .flush = flush_sink_flush,
            .data = &sink
        }
    };

    snAsyncLogger al;
    sn_async_logger_init(&al, buffer, sizeof(buffer), sinks, 1);

    sn_async_logger_log(&al, SN_LOG_LEVEL_INFO, "hello");

    sn_async_logger_flush(&al);

    assert(sink.flush_count == 1);
    assert(sink.base.count == 0); // NOT processed

    sn_async_logger_deinit(&al);

    printf("✓ passed\n");
}

static void test_async_drain_and_flush(void) {
    printf("Running test_async_drain_and_flush...\n");

    char buffer[4096];
    FlushSink sink = {0};

    snSink sinks[] = {
        {
            .write = test_sink_write,
            .flush = flush_sink_flush,
            .data = &sink
        }
    };

    snAsyncLogger al;
    sn_async_logger_init(&al, buffer, sizeof(buffer), sinks, 1);

    for (int i = 0; i < 10; ++i)
        sn_async_logger_log(&al, SN_LOG_LEVEL_INFO, "msg-%d", i);

    sn_async_logger_drain_and_flush(&al);

    assert(sink.base.count == 10);
    assert(sink.flush_count == 1);

    sn_async_logger_deinit(&al);

    printf("✓ passed\n");
}

int main(void) {
    test_static_basic();
    test_static_truncation();
    test_static_log_level();

    printf("All static logger tests passed!\n\n");

    test_async_single_thread_ordering();
    test_async_multi_producer_ordering();
    test_async_drop_behavior();

    printf("All async logger tests passed!\n\n");

    test_async_process_n();
    test_async_drain();
    test_async_flush_only();
    test_async_drain_and_flush();

    printf("All tests passed\n");
    return 0;
}

