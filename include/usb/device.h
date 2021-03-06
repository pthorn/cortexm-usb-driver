#ifndef DEVICE_H
#define DEVICE_H

#include "defs.h"
#include "descriptors.h"
#include "idevice.h"
#include "dispatchers.h"
#include "debug.h"

class IRxTransfer;
class ITxTransfer;
class Handler;
class EndpointConfig;


template<size_t NHandlers, size_t NEndpoints>
class Device: public IDevice
{
    friend class EPDispatcher<Device<NHandlers, NEndpoints>>;
    friend class CtrlEPDispatcher<Device<NHandlers, NEndpoints>>;

public:
    Device(EndpointConfig const* endpoint_config, Descriptors const& descriptors);
    void add_handler(Handler* handler);

    uint8_t get_configuration() override;
    bool set_configuration(uint8_t configuration) override;

    EndpointConfig const& get_ep_config(uint8_t ep_n, InOut in_out);
    SetupPacket const& get_setup_pkt() override;

protected:
    virtual void init_in_endpoint(EndpointConfig const& ep_conf) = 0;
    virtual void init_out_endpoint(EndpointConfig const& ep_conf) = 0;

    void init_endpoints(uint8_t configuration);
    void deinit_endpoints();
    void on_connect();
    void on_disconnect();
    void on_suspend();
    void on_resume();
    void on_reset();
    void dispatch_in_transfer_complete(uint8_t ep_n);
    void dispatch_out_transfer_complete(uint8_t ep_n);

    CtrlEPDispatcher<Device<NHandlers, NEndpoints>> ctrl_ep_dispatcher;  // dispatch control xfers to handlers
    EPDispatcher<Device<NHandlers, NEndpoints>> ep_dispatcher;           // dispatch non-ctrl xfers to handlers
    EndpointConfig const* endpoint_config;   // config data, can be put into flash
    Handler* handlers[NHandlers];            // pointers to handler objects
    ITxTransfer* in_transfers[NEndpoints];   // pointers to IN transfers
    IRxTransfer* out_transfers[NEndpoints];  // pointers to OUT transfers
    State state;
    uint8_t current_configuration;
};

#include "device.impl.h"

#endif // DEVICE_H
