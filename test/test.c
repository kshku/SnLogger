#include <snlogger/snlogger.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#define MAX_LOGS 500
#define MAX_LEN 256
#define BUFFER_SIZE 4096
#define TEST_COUNT 100
#define TEST_STRING "msg-%d"
char sbuffer[BUFFER_SIZE];
char abuffer[BUFFER_SIZE];

typedef struct {
    char logs[MAX_LOGS][MAX_LEN];
    size_t count;
} TestSink;

void test_sink_write(const char *msg, size_t len, snLogLevel level, void *data) {
    TestSink *sink = data;

    if (sink->count >= MAX_LOGS) return;

    size_t copy = len < MAX_LEN - 1 ? len : MAX_LEN - 1;
    
    memcpy(sink->logs[sink->count], msg, copy);
    sink->logs[sink->count][copy] = 0;
    sink->count++;
}

void console_log_sink(const char *msg, size_t len, snLogLevel level, void *data) {
    printf("%.*s\n", len, msg);
}

void *malloc_wrapper(size_t size, void *data) {
    (void)(data);
    void *ptr = malloc(size);
    if (!ptr) exit(EXIT_FAILURE);
    return ptr;
}

void free_wrapper(void *ptr, void *data) {
    (void)(data);
    free(ptr);
}

int main(void) {
    snStaticLogger sl;
    snAsyncLogger al;

    TestSink tss = {0};
    TestSink tsa = {0};

    snSink ssinks[] = {
        {.write = test_sink_write, .data = &tss},
        // {.write = console_log_sink}
    };
    snSink asinks[] = {
        {.write = test_sink_write, .data = &tsa},
        // {.write = console_log_sink}
    };

    sn_static_logger_init(&sl, sbuffer, BUFFER_SIZE, ssinks, (sizeof(ssinks)/sizeof(ssinks[0])));
    sn_async_logger_init(&al, abuffer, BUFFER_SIZE, asinks, (sizeof(asinks)/sizeof(asinks[0])));

    sn_async_logger_set_memory_hooks(&al, malloc_wrapper, free_wrapper, NULL);

    for (int i = 0; i < TEST_COUNT; ++i) {
        char msg[32];
        snprintf(msg, sizeof(msg), TEST_STRING, i);
        sn_static_logger_log(&sl, SN_LOG_LEVEL_INFO, "%s", msg);
        sn_async_logger_log(&al, SN_LOG_LEVEL_INFO, "%s", msg);
        if (i % 5 == 0) sn_async_logger_process(&al);
    }

    sn_static_logger_deinit(&sl);
    sn_async_logger_deinit(&al);

    assert(tss.count == TEST_COUNT);
    assert(tsa.count == TEST_COUNT);

    for (int i = 0; i < TEST_COUNT; ++i) {
        char expected[32];
        snprintf(expected, sizeof(expected), TEST_STRING, i);
        assert(strcmp(tss.logs[i], expected) == 0);
        assert(strcmp(tsa.logs[i], expected) == 0);
    }

    printf("Ordering test passed\n");
}
