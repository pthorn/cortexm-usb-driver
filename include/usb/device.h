#ifndef DEVICE_H
#define DEVICE_H

#include <cstdint>

#define NO_OF_ENDPOINTS 6



class Endpoint;
class ControlEndpoint;
class USBClass;


class Device
{
public:
    Device(ControlEndpoint& endpoint_0);

    void add_handler(USBClass* handler);

    void add_ep(Endpoint& ep);  // TODO add in constructor?

    Endpoint& get_ep(uint8_t n) {
        return *endpoints[n];
    }

    // class API

    // for use by StandardRequests
    virtual void set_address(uint16_t address) = 0;
    virtual bool set_configuration(uint8_t configuration) = 0;
    virtual void transmit_zlp(Endpoint *ep) = 0;
    virtual void ep0_receive_zlp() = 0;
    virtual void ep0_init_ctrl_transfer() = 0;

    // for use by custom handlers
    virtual void start_in_transfer(Endpoint* ep) = 0;  // TODO &
    virtual void start_out_transfer(Endpoint* ep) = 0;
    virtual void stall(uint8_t ep) = 0;

public:
    USBClass* handlers[4];  // TODO! endpoint classes use this
    ControlEndpoint& endpoint_0;  // TODO! ControlEndpoint uses this

protected:
    //ControlEndpoint& endpoint_0;
    Endpoint* endpoints[NO_OF_ENDPOINTS];
    //USBClass* handlers[4];
};

#endif // DEVICE_H
