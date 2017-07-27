#include "usb/debug.h"


void d_assert_impl(char const* file, int line, char const* func, char const* msg)
{
#if DEBUG_LEVEL < DEBUG_NONE
    print("ASSERT %s:%d [%s] %s\n", file, line, func, msg);
#endif

    while (true) ;;
    __builtin_unreachable();
}
