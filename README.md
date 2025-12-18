# SnLogger

SnLogger is a C logging library.

It provides static and asynchronous loggers and focuses on log transport,
buffering, and sink dispatch rather than formatting and other things.

SnLogger does not provide a built-in layout or formatting system; message
formatting is expected to be layered on top or implemented in sinks.

The library does not create threads, or perform implicit I/O.
All processing, flushing, memory allocation, and synchronization
decisions are explicit.

## Design Overview

SnLogger follows a manual processing model (for async logger).

- Producers enqueue log records
- Records are emitted only when processing functions are called
- No background worker threads are created
- No global logger state is used

This design allows the logging behavior to be fully controlled by the application.

## Loggers

### Static Logger

The static logger formats log message to given fixed-size buffer and emits message to sinks (synchronous).

- Buffer size is defined at initialization
- No dynamic memory allocation is performed
- If buffer is not large enough, message will be truncated
- Message is directly emitted to all sinks

The static logger is intended for single-threaded or externally synchronized use.

### Asynchronous Logger

The asynchronous logger supports multiple producers concurrently enqueuing log records.

- Log records are written to a ring buffer
- Optional heap fallback if memory hooks are given
- No ordering is enforced beyond enqueue order
- Records are emitted only during explicit processing calls
- If the ring buffer is full and no heap fallback is available, log records may be dropped.


The asynchronous logger does not guarantee global timestamp ordering across
multiple producers. It only preserves the enqueue order of message.

#### Processing Model

Log records are emitted by calling processing functions.

- `process()` processes available records
- `process_n(n)` processes at most `n` records
- `drain()` processes records until none remain
- `drain_and_flush()` processes records until none remain and flushes sinks

Producers may continue to enqueue records while processing is in progress.
Processing functions operate on records available at the time of processing.

## Sinks

Sinks receive fully formatted log records.

- Multiple sinks may be attached to a logger
- Sink behavior is fully user-defined
- Flushing is explicit and never implicit

The library does not assume any I/O mechanism.

## Threading and Synchronization

SnLogger does not impose a threading model.

- Locking hooks must be provided for thread-safety in the async logger,
  or synchronization must be handled externally.
- The library does not block producers
- The messages are processed in the order they are enqueued.

## Non-goals

SnLogger does not:

- Create or manage threads
- Perform I/O implicitly during asynchronous log enqueue
- Implicitly flush or drain logs (except during deinitialization)
- Provide global loggers
- Enforce cross-thread timestamp ordering

## Building

SnLogger uses CMake.

### Build static library (default)

```sh
cmake -S . -B build
cmake --build build
```

### Build shared library
```sh
cmake -S . -B build -DSN_LOGGER_BUILD_SHARED=ON
cmake --build build
```

## Using SnLogger
SnLogger is intended to be embedded directly into projects.

You can:
- Add it as a git submodule
- Link against the static or shared library
- Include the source files directly
- No global initialization is required.

## Documentation
API documentation can be generated using Doxygen.
```sh
cmake --build build --target docs
```
Generated documentation will be available in `build/docs/output/html/index.html`

## Basic usage

### Static Logger Example
```c
#include <snlogger/snlogger.h>

#include <stdio.h>

char buffer[256];

void write_sink(const char *msg, size_t len, snLogLevel level, void *data) {
    fwrite(msg, 1, len, stdout);
    fputc('\n', stdout);
}

int main(void) {
    snSink sinks[] = {
        {.write = write_sink}
    };

    snStaticLogger logger;
    sn_static_logger_init(&logger, buffer, sizeof(buffer), sinks, 1);

    sn_static_logger_log(&logger, SN_LOG_LEVEL_INFO, "Hello, %s!", "world");

    sn_static_logger_deinit(&logger);
}
```

### Async Logger Example
```c
#include <snlogger/snlogger.h>

#include <stdio.h>

char buffer[4096];

void write_sink(const char *msg, size_t len, snLogLevel level, void *data) {
    fwrite(msg, 1, len, stdout);
    fputc('\n', stdout);
}

int main(void) {
    snSink sinks[] = {
        {.write = write_sink}
    };

    snAsyncLogger logger;
    sn_async_logger_init(&logger, buffer, sizeof(buffer), sinks, 1);

    sn_async_logger_log(&logger, SN_LOG_LEVEL_INFO, "Hello async logger");

    // Process logs explicitly
    sn_async_logger_process(&logger);

    sn_async_logger_deinit(&logger);
}
```

***Checkout `test/example_formatting.c`***

