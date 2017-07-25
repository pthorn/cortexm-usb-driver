#ifndef DEFS_H
#define DEFS_H

#include <cstdint>


enum class InOut {
    In,
    Out
};

enum class EPType {
    Control = 0,
    Interrupt = 1,
    Bulk = 2,
    Isochronous = 3
};


struct SetupPacket {  // sizeof() == 8
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed));


// bmRequestType and bEndpointAddress flags
#define ENDPOINT_OUT 0x00
#define ENDPOINT_IN  0x80

// bmRequestType flags
#define REQUEST_TYPE_STANDARD (0 << 5)
#define REQUEST_TYPE_CLASS    (1 << 5)
#define REQUEST_TYPE_VENDOR   (2 << 5)
#define REQUEST_TYPE_RESERVED (3 << 5)

// bmRequestType
#define RECIPIENT_DEVICE    0
#define RECIPIENT_INTERFACE 1
#define RECIPIENT_ENDPOINT  2
#define RECIPIENT_OTHER     3

// bRequest
#define GET_STATUS         0
#define CLEAR_FEATURE      1
#define SET_FEATURE        3
#define SET_ADDRESS        5
#define GET_DESCRIPTOR     6
#define SET_DESCRIPTOR     7
#define GET_CONFIGURATION  8
#define SET_CONFIGURATION  9
#define GET_INTERFACE     10
#define SET_INTERFACE     11

// bDescriptorType
#define DESCRIPTOR_DEVICE         1
#define DESCRIPTOR_CONFIGURATION  2
#define DESCRIPTOR_STRING         3
#define DESCRIPTOR_INTERFACE      4
#define DESCRIPTOR_ENDPOINT       5


enum class SetupResult {
    UNHANDLED,
    OK,
    STALL
};

enum class DataResult {
    DONE,
    STALL  // STALL the status stage
};

#endif // DEFS_H
