#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <cstddef>
#include <cstdint>

#include "usb/device.h"


enum class Core {
    HS,
    FS
};

enum class State {
    NONE,
    INITIALIZED,
    SUSPENDED,
    WAKEUP,
    ADDRESS,
    CONFIGURED
};


class Endpoint;
class ControlEndpoint;


class DWC_OTG_Device: public Device
{
public:
    DWC_OTG_Device(Core core, ControlEndpoint& endpoint_0, uint16_t rxfifo_size = 256);

    void init();
    void isr();

    // for use by StandardRequests
    void set_address(uint16_t address) override;
    bool set_configuration(uint8_t configuration) override;

    void start_in_transfer(Endpoint* ep) override;  // TODO &
    void start_out_transfer(Endpoint* ep) override;
    void transmit_zlp(Endpoint *ep) override;
    void ep0_receive_zlp();
    void stall(uint8_t ep) override;

private:
    void init_clocks();
    void init_gpio();
    void init_nvic();
    void init_usb();
    void init_interrupts();
    void init_ep0();
    void init_endpoints(uint8_t configuration);
    void init_in_endpoint(Endpoint* ep);
    void init_out_endpoint(Endpoint* ep);

    void isr_usb_reset();
    void isr_speed_complete();
    void isr_read_rxfifo();
    void isr_out_ep_interrupt();
    void isr_in_ep_interrupt();

    size_t const base_addr;
    uint16_t const rxfifo_size;
    uint16_t const total_fifo_size;
    uint8_t const max_ep_n;

    State state;
    uint8_t current_configuration;
    uint32_t fifo_end;
};

#endif // USB_DEVICE_H
