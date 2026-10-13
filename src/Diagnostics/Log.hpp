#pragma once

#include <cstdarg>
#include <cstdio>

namespace Museum {

// Warnings and notes go to stderr so they never interleave with CNA's own
// stdout logging. Printf-style on purpose: no allocation, no formatting library.
inline void Log(const char* tag, const char* format, ...)
{
    std::fprintf(stderr, "[museum][%s] ", tag);
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
}

}  // namespace Museum
