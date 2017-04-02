#ifndef IDEVICE_H
#define IDEVICE_H

#include <cstddef>
#include <cstdint>

#include "defs.h"

class RxTransfer;
class TxTransfer;


class IDevice
{
public:
    enum class State {
        NONE,
        INITIALIZED,
        SPEED,
        ADDRESS,
        CONFIGURED,
        SUSPENDED
    };

    // for use by StandardRequests
    virtual uint8_t get_configuration() = 0;
    virtual bool set_configuration(uint8_t configuration) = 0;
    virtual void set_address(uint16_t address) = 0;
    virtual void transmit_zlp(uint8_t ep_n) = 0;
    virtual void ep0_receive_zlp() = 0;
    virtual void ep0_init_ctrl_transfer() = 0;

    // for use by all handlers
    virtual SetupPacket const& get_setup_pkt() = 0;
    virtual void submit(uint8_t ep_n, RxTransfer& transfer) = 0;
    virtual void submit(uint8_t ep_n, TxTransfer& transfer) = 0;
    virtual void stall(uint8_t ep) = 0;

protected:
    virtual void init_endpoints(uint8_t configuration) = 0;
    virtual void deinit_endpoints() = 0;
};

#endif // IDEVICE_H
