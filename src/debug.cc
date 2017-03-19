#include "debug.h"


void d_assert_impl(char const* file, int line, char const* func, char const* msg)
{
    print("ASSERT %s:%d [%s] %s\n", file, line, func, msg);
    while (true) ;;
    __builtin_unreachable();
}
