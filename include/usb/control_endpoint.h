#ifndef CONTROL_ENDPOINT_H
#define CONTROL_ENDPOINT_H

#include <cstddef>
#include <cstdint>
#include "usb/defs.h"
#include "usb/endpoint.h"

class USBClass;

enum class CtrlState {
    START,
    //SETUP_RX,
    SETUP_STAGE_COMPLETE,
    DATA_STAGE_COMPLETE
};


// control endpoint has #0, is always active (no start_transfer),
// is unstalled automatically, has 3-stage transfers (setup, data, status)
class ControlEndpoint: public Endpoint {
public:
    ControlEndpoint(uint16_t max_pkt_size, uint16_t tx_fifo_size)
    : Endpoint(0, InOut::In, EPType::Control, max_pkt_size, tx_fifo_size)
    { }

    unsigned char* get_setup_pkt_buffer() {
        return reinterpret_cast<unsigned char*>(&setup_packet);
    }

    virtual void on_setup_stage();

    void on_in_transfer_complete();
    void on_out_transfer_complete();

private:
    void reinit();

public:  // TODO ?
    SetupPacket setup_packet;

protected:
    CtrlState state;
    USBClass* current_handler;
};

#endif // CONTROL_ENDPOINT_H
