#ifndef ST_DEVICE_H
#define ST_DEVICE_H

#include <cstddef>
#include <cstdint>

#include "usb/device.h"


template<size_t NHandlers, size_t NEndpoints>
class ST_Device: public Device<NHandlers, NEndpoints>
{
public:
    using typename Device<NHandlers, NEndpoints>::State;

    ST_Device(EndpointConfig const* endpoint_config, Descriptors const& descriptors);

    void init();
    void isr();

    using Device<NHandlers, NEndpoints>::get_ep_config;

    // for use by StandardRequests
    void set_address(uint16_t address) override;

    void submit(uint8_t ep_n, IRxTransfer& transfer) override;
    void submit(uint8_t ep_n, ITxTransfer& transfer) override;
    void stall(uint8_t ep) override;

    // for use by ControlEndpoint
    void ep0_init_ctrl_transfer() override;

protected:
    using Device<NHandlers, NEndpoints>::on_connect;
    using Device<NHandlers, NEndpoints>::on_disconnect;
    using Device<NHandlers, NEndpoints>::on_suspend;
    using Device<NHandlers, NEndpoints>::on_resume;
    using Device<NHandlers, NEndpoints>::on_reset;
    using Device<NHandlers, NEndpoints>::dispatch_in_transfer_complete;
    using Device<NHandlers, NEndpoints>::dispatch_out_transfer_complete;

    void init_in_endpoint(EndpointConfig const& ep_conf) override;
    void init_out_endpoint(EndpointConfig const& ep_conf) override;

private:
    void init_usb();
    void init_ep0();
    void isr_handle_tx();
    void isr_handle_rx();

    using Device<NHandlers, NEndpoints>::ctrl_ep_dispatcher;
    using Device<NHandlers, NEndpoints>::ep_dispatcher;
    using Device<NHandlers, NEndpoints>::endpoint_config;
    using Device<NHandlers, NEndpoints>::handlers;
    using Device<NHandlers, NEndpoints>::in_transfers;
    using Device<NHandlers, NEndpoints>::out_transfers;
    using Device<NHandlers, NEndpoints>::state;
    using Device<NHandlers, NEndpoints>::current_configuration;

    uint32_t buf_end;
    uint16_t pending_addr;
};

#include "st_device.impl.h"

#endif // ST_DEVICE_H
