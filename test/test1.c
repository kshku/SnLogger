#include <snlogger/snlogger.h>
#include <stdio.h>

char buffer[256];

void write_sink(const char *msg, size_t len, SnLogLevel level, void *data) {
    fwrite(msg, 1, len, stdout);
    fputc('\n', stdout);
}

int main(void) {
    SnSink sinks[] = {{.write = write_sink}};

    SnStaticLogger logger;
    sn_static_logger_init(&logger, buffer, sizeof(buffer), sinks, 1);

    sn_static_logger_log(&logger, SN_LOG_LEVEL_INFO, "Hello, %s!", "world");

    sn_static_logger_deinit(&logger);
}

