#include "snlogger/formatter.h"

#include <stdio.h>

size_t format_string(char *buffer, size_t len, const char *fmt, va_list args) {
    // for now vsnprintf
    int l = vsnprintf(buffer, len, fmt, args);
    if (l < 0) l = 0;
    if (l >= len) l = len ? (len - 1) : 0;
    return (size_t)l;
}
