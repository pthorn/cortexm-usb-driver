#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG_VERBOSE 0
#define DEBUG_INFO 1
#define DEBUG_WARN 2
#define DEBUG_ERROR 3
#define DEBUG_CRITICAL 4
#define DEBUG_NONE 255

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_WARN
#endif

#if DEBUG_LEVEL < DEBUG_NONE
#   include <print.h>
#endif

#if DEBUG_LEVEL <= DEBUG_VERBOSE
#   define d_verbose(...) print(__VA_ARGS__)
# else
#   define d_verbose(...)
#endif

#if DEBUG_LEVEL <= DEBUG_INFO
#   define d_info(...) print(__VA_ARGS__)
# else
#   define d_info(...)
#endif

#if DEBUG_LEVEL <= DEBUG_WARN
#   define d_warn(...) print(__VA_ARGS__)
# else
#   define d_warn(...)
#endif

#if DEBUG_LEVEL <= DEBUG_ERROR
#   define d_error(...) print(__VA_ARGS__)
# else
#   define d_error(...)
#endif

#if DEBUG_LEVEL <= DEBUG_CRITICAL
#   define d_critical(...) print(__VA_ARGS__)
# else
#   define d_critical(...)
#endif

void d_assert_impl(char const* file, int line, char const* func, char const* msg)
     __attribute__((noreturn));
#define d_assert(msg)  d_assert_impl(__FILE__, __LINE__, __PRETTY_FUNCTION__, msg)

#endif // DEBUG_H
