#ifndef EP_CONFIG_H
#define EP_CONFIG_H

#include "defs.h"


// configuration is supposed to be placed into flash
struct EndpointConfig {
    uint8_t n;
    InOut in_out;
    EPType type;
    uint16_t max_pkt_size;
    //uint16_t tx_fifo_size = 0;  //  TODO dwc_otg specific, IN only
};


#endif // EP_CONFIG_H
