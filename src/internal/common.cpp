#include "internal/common.h"
#include <cstdarg>

void FormatString(char* buf, int buf_size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VFormatString(buf, buf_size, fmt, args);
    va_end(args);
}