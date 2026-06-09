#include <snlogger/snlogger.h>
#include <stdio.h>

char buffer[4096];

void write_sink(const char *msg, size_t len, SnLogLevel level, void *data) {
    fwrite(msg, 1, len, stdout);
    fputc('\n', stdout);
}

int main(void) {
    SnSink sinks[] = {{.write = write_sink}};

    SnAsyncLogger logger;
    sn_async_logger_init(&logger, buffer, sizeof(buffer), sinks, 1);

    sn_async_logger_log(&logger, SN_LOG_LEVEL_INFO, "Hello async logger");

    // Process logs explicitly
    sn_async_logger_process(&logger);

    sn_async_logger_deinit(&logger);
}

