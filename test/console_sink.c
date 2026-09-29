#include <sncore/defines.h>
#include <snlogger/console_sink.h>
#include <snlogger/static_logger.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(x)                                                     \
    do {                                                                   \
        if (!(x)) {                                                        \
            fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #x); \
            abort();                                                       \
        }                                                                  \
    } while (0)

/* Read a whole stream back into a NUL terminated string, for comparison. */
static char *slurp(FILE *stream) {
    static char buf[8192];

    fflush(stream);
    rewind(stream);

    size_t len = fread(buf, 1, sizeof(buf) - 1, stream);
    buf[len] = 0;

    return buf;
}

static FILE *open_scratch(void) {
    FILE *f = tmpfile();
    TEST_ASSERT(f);
    return f;
}

static void test_write_without_color(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_OFF);

    sn_console_write(
        &console, SN_CONSOLE_COLOR_RED, SN_CONSOLE_COLOR_BLUE, SN_CONSOLE_MODE_BOLD, "%s=%d", "n", 42);
    sn_console_flush(&console);

    /* Color off must produce the bare text. Not a stripped escape sequence, the
     * escapes are never written in the first place. */
    TEST_ASSERT(strcmp(slurp(out), "n=42") == 0);

    fclose(out);
}

static void test_write_with_color(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);

    sn_console_write(&console, SN_CONSOLE_COLOR_RED, SN_CONSOLE_COLOR_DEFAULT, SN_CONSOLE_MODE_BOLD, "hi");

    char *text = slurp(out);
    fclose(out);

    /* red is SGR 31, default background is 39 which the writer offsets to 49,
     * and bold is SGR 1 */
    TEST_ASSERT(strcmp(text, "\x1b[1;49;31mhi\x1b[0m") == 0);
}

static void test_write_with_no_color_requested(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_OFF);

    sn_console_write(&console, SN_CONSOLE_COLOR_RED, SN_CONSOLE_COLOR_DEFAULT, 0, "plain");

    char *text = slurp(out);
    fclose(out);

    TEST_ASSERT(strcmp(text, "plain") == 0);
}

static void test_modes_map_to_sgr(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);

    /* underline is bit 3, strikethrough is bit 7, so SGR 4 and 9. Background
     * black is 30, offset to 40, foreground white is 37. */
    sn_console_write(&console, SN_CONSOLE_COLOR_WHITE, SN_CONSOLE_COLOR_BLACK,
                     SN_CONSOLE_MODE_UNDERLINE | SN_CONSOLE_MODE_STRIKETHROUGH, "x");

    char *text = slurp(out);
    fclose(out);

    TEST_ASSERT(strcmp(text, "\x1b[4;9;40;37mx\x1b[0m") == 0);
}

static void test_sink_write_uses_len_not_strlen(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_OFF);
    sn_console_sink_set_level_color(&console, false);

    /* the sink contract says the record is not NUL terminated, so a record with
     * an embedded NUL must come out intact rather than truncated at it */
    const char record[] = {'a', '\0', 'b'};
    console.sink.write(record, sizeof(record), SN_LOG_LEVEL_INFO, &console);
    sn_console_flush(&console);

    char *text = slurp(out);
    fclose(out);

    /* compared by length, strlen would stop at the NUL and hide the tail */
    TEST_ASSERT(memcmp(text, record, sizeof(record)) == 0);
}

static void test_level_color(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);
    sn_console_sink_set_level_color(&console, true);

    console.sink.write("bad", 3, SN_LOG_LEVEL_ERROR, &console);
    sn_console_flush(&console);

    char *text = slurp(out);
    fclose(out);

    /* error is red, and only the foreground is set, no background or mode */
    TEST_ASSERT(strcmp(text, "\x1b[31mbad\x1b[0m") == 0);
}

static void test_level_color_can_be_turned_off(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);
    sn_console_sink_set_level_color(&console, false);

    console.sink.write("bad", 3, SN_LOG_LEVEL_ERROR, &console);
    sn_console_flush(&console);

    char *text = slurp(out);
    fclose(out);

    TEST_ASSERT(strcmp(text, "bad") == 0);
}

static void test_auto_detects_a_pipe_as_not_a_terminal(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    /* no set_color call, so this is whatever the detection decided */
    sn_console_sink_init(&console, out);

    sn_console_write(&console, SN_CONSOLE_COLOR_RED, SN_CONSOLE_COLOR_DEFAULT, 0, "piped");
    sn_console_flush(&console);

    char *text = slurp(out);
    fclose(out);

    /* a file is not a terminal, so the escapes must not be there. This is what
     * keeps a redirected log free of escape noise. */
    TEST_ASSERT(strcmp(text, "piped") == 0);
}

static void test_write_rgb(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);

    /* truecolor is SGR 38;2;r;g;b for the foreground and 48;2;r;g;b for the
     * background, and the modes still come first, the same as the palette form */
    sn_console_write_rgb(&console, SN_RGB(255, 128, 0), SN_RGB(0, 0, 64), SN_CONSOLE_MODE_BOLD, "rgb");

    char *text = slurp(out);
    fclose(out);

    TEST_ASSERT(strcmp(text, "\x1b[1;48;2;0;0;64;38;2;255;128;0mrgb\x1b[0m") == 0);
}

static void test_write_rgb_defaults(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);

    /* SN_RGB_DEFAULT has to be distinguishable from true black, which is why it
     * carries a flag rather than being a sentinel triple. Default is SGR 39 for
     * the foreground and 49 for the background. */
    sn_console_write_rgb(&console, SN_RGB_DEFAULT, SN_RGB(1, 2, 3), 0, "d");

    char *text = slurp(out);
    fclose(out);

    TEST_ASSERT(strcmp(text, "\x1b[48;2;1;2;3;39md\x1b[0m") == 0);
}

static void test_write_rgb_without_color(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_OFF);

    /* turning color off must strip the truecolor codes too, not just the
     * palette ones, or a piped build would still be full of escape noise */
    sn_console_write_rgb(&console, SN_RGB(255, 0, 0), SN_RGB(0, 0, 0), SN_CONSOLE_MODE_BOLD, "plain");

    char *text = slurp(out);
    fclose(out);

    TEST_ASSERT(strcmp(text, "plain") == 0);
}

static void test_sink_works_through_a_logger(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_OFF);

    /* every slot has to be filled, the logger skips any that is NULL */
    TEST_ASSERT(console.sink.open != NULL);
    TEST_ASSERT(console.sink.write != NULL);
    TEST_ASSERT(console.sink.close != NULL);
    TEST_ASSERT(console.sink.flush != NULL);
    TEST_ASSERT(console.sink.data == &console);

    char buffer[4096];
    SnSink sinks[] = {console.sink};
    SnStaticLogger logger;

    sn_static_logger_init(&logger, buffer, sizeof(buffer), sinks, SN_ARRAY_LENGTH(sinks));
    sn_static_logger_log(&logger, SN_LOG_LEVEL_ERROR, "through the logger\n");
    sn_static_logger_deinit(&logger);

    TEST_ASSERT(strstr(slurp(out), "through the logger") != NULL);

    /* the sink borrows the stream, so deinit must not have closed it. If close
     * did fclose it, this write would be undefined behaviour. */
    fputs("still open", out);
    sn_console_flush(&console);

    TEST_ASSERT(strstr(slurp(out), "still open") != NULL);

    fclose(out);
}

/* A caller that already holds a va_list has to sit behind something variadic,
 * which is the whole reason the _va form exists. These stand in for that. */
static void forward_palette(SnConsoleSink *console, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    sn_console_write_va(console, SN_CONSOLE_COLOR_RED, SN_CONSOLE_COLOR_DEFAULT, SN_CONSOLE_MODE_BOLD, fmt, args);
    va_end(args);
}

static void forward_rgb(SnConsoleSink *console, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    sn_console_write_rgb_va(console, SN_RGB(1, 2, 3), SN_RGB_DEFAULT, SN_CONSOLE_MODE_ITALIC, fmt, args);
    va_end(args);
}

static void test_write_va_matches_write(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);

    sn_console_write(&console, SN_CONSOLE_COLOR_RED, SN_CONSOLE_COLOR_DEFAULT, SN_CONSOLE_MODE_BOLD,
                     "n=%d %s", 7, "x");

    char *variadic = slurp(out);
    fclose(out);

    out = open_scratch();
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);

    forward_palette(&console, "n=%d %s", 7, "x");

    char *from_va = slurp(out);
    fclose(out);

    /* the _va form has to produce byte for byte what the variadic form does,
     * otherwise a caller forwarding a va_list gets different output */
    TEST_ASSERT(strcmp(variadic, "\x1b[1;49;31mn=7 x\x1b[0m") == 0);
    TEST_ASSERT(strcmp(from_va, variadic) == 0);
}

static void test_write_rgb_va_matches_write_rgb(void) {
    FILE *out = open_scratch();

    SnConsoleSink console;
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);

    sn_console_write_rgb(&console, SN_RGB(1, 2, 3), SN_RGB_DEFAULT, SN_CONSOLE_MODE_ITALIC, "%d-%d", 4, 5);

    char *variadic = slurp(out);
    fclose(out);

    out = open_scratch();
    sn_console_sink_init(&console, out);
    sn_console_sink_set_color(&console, SN_CONSOLE_COLOR_ON);

    forward_rgb(&console, "%d-%d", 4, 5);

    char *from_va = slurp(out);
    fclose(out);

    TEST_ASSERT(strstr(from_va, "38;2;1;2;3") != NULL);
    TEST_ASSERT(strstr(from_va, "4-5") != NULL);
    TEST_ASSERT(strcmp(from_va, variadic) == 0);
}

static void test_std_sink_is_usable(void) {
    /* the ready made one has to work with no init call at all */
    TEST_ASSERT(sn_console_std_sink.sink.write != NULL);
    TEST_ASSERT(sn_console_std_sink.sink.open != NULL);
    TEST_ASSERT(sn_console_std_sink.sink.flush != NULL);

    /* and default to the terminal, not to colour being off */
    TEST_ASSERT(sn_console_std_sink.color_mode == SN_CONSOLE_COLOR_AUTO);
    TEST_ASSERT(sn_console_std_sink.level_color == true);

    /* writing to it must not crash. Output is left to the test runner. */
    sn_console_write(&sn_console_std_sink, SN_CONSOLE_COLOR_DEFAULT, SN_CONSOLE_COLOR_DEFAULT, 0, "");
    sn_console_write_rgb(&sn_console_std_sink, SN_RGB(1, 2, 3), SN_RGB_DEFAULT, 0, "");
}

static void test_null_sinks_are_ignored(void) {
    sn_console_sink_init(NULL, NULL);
    sn_console_sink_set_color(NULL, SN_CONSOLE_COLOR_ON);
    sn_console_sink_set_level_color(NULL, true);
    sn_console_write(NULL, SN_CONSOLE_COLOR_RED, SN_CONSOLE_COLOR_DEFAULT, 0, "dropped");
    sn_console_write_rgb(NULL, SN_RGB(1, 2, 3), SN_RGB_DEFAULT, 0, "dropped");
    {
        va_list none;
        memset(&none, 0, sizeof(none));
        sn_console_write_va(NULL, SN_CONSOLE_COLOR_RED, SN_CONSOLE_COLOR_DEFAULT, 0, "dropped", none);
        sn_console_write_rgb_va(NULL, SN_RGB(1, 2, 3), SN_RGB_DEFAULT, 0, "dropped", none);
    }
    sn_console_flush(NULL);
}

int main(void) {
    test_write_without_color();
    test_write_with_color();
    test_write_with_no_color_requested();
    test_modes_map_to_sgr();
    test_write_rgb();
    test_write_rgb_defaults();
    test_write_rgb_without_color();
    test_write_va_matches_write();
    test_write_rgb_va_matches_write_rgb();
    test_sink_write_uses_len_not_strlen();
    test_level_color();
    test_level_color_can_be_turned_off();
    test_sink_works_through_a_logger();
    test_auto_detects_a_pipe_as_not_a_terminal();
    test_std_sink_is_usable();
    test_null_sinks_are_ignored();

    printf("console_sink: all tests passed\n");
    return 0;
}
