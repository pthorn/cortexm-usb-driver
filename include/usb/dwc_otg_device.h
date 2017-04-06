#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <cstddef>
#include <cstdint>

#include "usb/device.h"


// for IN endpoints only
struct DWCEndpointConfig {
    uint8_t n;
    uint16_t tx_fifo_size;
};


template<size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
class DWC_OTG_Device: public Device<NHandlers, NEndpoints>
{
public:
    using typename Device<NHandlers, NEndpoints>::State;

    DWC_OTG_Device(
        EndpointConfig const* endpoint_config,
        DWCEndpointConfig const* dwc_endpoint_config,
        Descriptors const& descriptors,
        uint16_t rxfifo_size = 256
    );

    void init();
    void isr();

    using Device<NHandlers, NEndpoints>::get_ep_config;

    // for use by StandardRequests
    void set_address(uint16_t address) override;

    void submit(uint8_t ep_n, RxTransfer& transfer) override;
    void submit(uint8_t ep_n, TxTransfer& transfer) override;
    void stall(uint8_t ep) override;

    // for use by ControlEndpoint
    void transmit_zlp(uint8_t ep_n) override;
    void ep0_receive_zlp() override;
    void ep0_init_ctrl_transfer() override;

protected:
    void init_endpoints(uint8_t configuration) override;
    void deinit_endpoints() override;

    using Device<NHandlers, NEndpoints>::dispatch_in_transfer_complete;
    using Device<NHandlers, NEndpoints>::dispatch_out_transfer_complete;

private:
    void init_clocks();
    void init_gpio();
    void init_nvic();
    void init_usb();
    void init_interrupts();
    void init_ep0();
    void init_in_endpoint(EndpointConfig const& ep_conf);
    void init_out_endpoint(EndpointConfig const& ep_conf);

    void isr_usb_reset();
    void isr_speed_complete();
    void isr_read_rxfifo();
    void isr_out_ep_interrupt();
    void isr_in_ep_interrupt();

    void read_packet(unsigned char* dest_buf, uint16_t n_bytes);
    DWCEndpointConfig const& get_dwc_ep_config(uint8_t ep_n);

    using Device<NHandlers, NEndpoints>::ctrl_ep_dispatcher;
    using Device<NHandlers, NEndpoints>::ep_dispatcher;
    using Device<NHandlers, NEndpoints>::endpoint_config;
    using Device<NHandlers, NEndpoints>::handlers;
    using Device<NHandlers, NEndpoints>::in_transfers;
    using Device<NHandlers, NEndpoints>::out_transfers;
    using Device<NHandlers, NEndpoints>::state;
    using Device<NHandlers, NEndpoints>::current_configuration;

    DWCEndpointConfig const* dwc_endpoint_config;
    uint16_t const rxfifo_size;
    uint16_t const total_fifo_size;
    uint8_t /*const*/ max_ep_n;
    uint32_t fifo_end;
};

#include "dwc_otg_device.impl.h"

#endif // USB_DEVICE_H
