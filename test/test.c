#include <snlogger/snlogger.h>

#include <stdio.h>

#define BUFFER_LEN 1024

char buffer[BUFFER_LEN];

void print_string(const char *msg, size_t len, snLogLevel level, void *data) {
    puts(msg);
}

void print_string2(const char *msg, size_t len, snLogLevel level, void *data) {
    printf("Log level: %d, message: %s\n", level, msg);
}

int main(void) {
    snStaticLogger sl;

    snSink sink[] = {
        (snSink){.write = print_string},
        (snSink){.write = print_string2}
    };

    sn_static_logger_init(&sl, buffer, BUFFER_LEN, sink, 2);

    sn_static_logger_log(&sl, SN_LOG_LEVEL_INFO, "Hello, World!");

    sn_static_logger_log_raw(&sl, SN_LOG_LEVEL_INFO, "Hello", 5);

    sn_static_logger_deinit(&sl);
}
