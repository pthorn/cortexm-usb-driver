#ifndef ST_DEVICE_H
#define ST_DEVICE_H

#include <cstddef>
#include <cstdint>

#include "usb/device.h"


class ST_Device: public Device
{
public:
    ST_Device();

    void init();
    void isr();

    // for use by StandardRequests
    void set_address(uint16_t address) override;
    bool set_configuration(uint8_t configuration) override;

    void start_in_transfer(Endpoint* ep) override;  // TODO &
    void start_out_transfer(Endpoint* ep) override;
    void transmit_zlp(Endpoint *ep) override;
    void stall(uint8_t ep) override;

private:
    void init_usb();
    void isr_handle_tx();
    void isr_handle_rx();
};

#endif // ST_DEVICE_H
