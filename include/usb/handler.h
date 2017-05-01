#ifndef HANDLER_H
#define HANDLER_H

#include "defs.h"
#include "idevice.h"

class IRxTransfer;
class ITxTransfer;


class Handler
{
protected:
    Handler() = default;

public:
    void on_attached(IDevice* device) {
        this->device = device;
    }

    // VBUS detect
    virtual void on_connect();
    virtual void on_disconnect();

    // USB events
    virtual void on_reset();
    virtual void on_set_configuration(uint8_t configuration);
    virtual void on_suspend();
    virtual void on_resume();

    // control transfers
    virtual SetupResult on_ctrl_setup_stage();
    virtual void handle_ctrl_status_stage();

protected:
    SetupPacket const& get_setup_pkt() {
        return device->get_setup_pkt();
    }

    void submit(uint8_t ep_n, IRxTransfer& transfer) {
        return device->submit(ep_n, transfer);
    }

    void submit(uint8_t ep_n, ITxTransfer& transfer) {
        return device->submit(ep_n, transfer);
    }

    void stall() {
        // TODO
    }

protected:
    IDevice* device;
};

#endif // HANDLER_H
