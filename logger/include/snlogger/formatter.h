#pragma once

#include <stddef.h>
#include <stdarg.h>

// TODO:
size_t format_string(char *buffer, size_t len, const char *fmt, va_list args);
