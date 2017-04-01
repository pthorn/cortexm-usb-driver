#ifndef DISPATCHERS_H
#define DISPATCHERS_H

#include "defs.h"


template<typename Device>
class EPDispatcher {
public:
    EPDispatcher(Device* device):
        device(*device)
    { }

    void on_in_transfer_complete(uint8_t ep_n);
    void on_out_transfer_complete(uint8_t ep_n);

private:
    Device& device;
};


template<typename Device>
class CtrlEPDispatcher {
    friend Device;

public:
    enum CtrlState {
        START,
        DATA_STAGE,
        STATUS_STAGE
    };

    CtrlEPDispatcher(Device* device):
        device(*device)
    { }

    unsigned char* get_setup_pkt_buffer() {
        return reinterpret_cast<unsigned char*>(&setup_packet);
    }

    void on_setup_stage(uint8_t ep_n);
    void on_in_transfer_complete(uint8_t ep_n);
    void on_out_transfer_complete(uint8_t ep_n);

private:
    void reinit();

    Device& device;
    SetupPacket setup_packet;
    CtrlState state;
};

#include "dispatchers.impl.h"

#endif // DISPATCHERS_H
