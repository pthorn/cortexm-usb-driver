#ifndef DEVICE_PVT_H
#define DEVICE_PVT_H

#include "usb/usb_class.h"

#define CALL_HANDLERS(method, ...) \
    for (auto handler: handlers) { \
        if (handler != nullptr) { \
            handler->method(__VA_ARGS__); \
        } \
    }

#endif // DEVICE_PVT_H
