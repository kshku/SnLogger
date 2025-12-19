#pragma once

#include "snlogger/defines.h"

#include <stdarg.h>

// TODO:
size_t format_string(char *restrict buffer, size_t len, const char *restrict fmt, va_list args);
