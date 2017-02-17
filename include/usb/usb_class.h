#ifndef USB_CLASS_H
#define USB_CLASS_H

#include "usb/control_endpoint.h"
#include "usb/dwc_otg_device.h"


class USBClass
{
protected:
    USBClass() = default;

public:
    // TODO attach directly to device?
    void on_attached(Device* device) {
        //this->ep0 = ep0;
        this->device = device;
    }

    // global USB events
    // TODO VBUS detect?
    virtual void on_set_configuration(uint8_t configuration) = 0;
    //virtual void on_suspend() = 0;

    // control transfers
    virtual SetupResult handle_ctrl_setup_stage() = 0;
    virtual DataResult handle_ctrl_in_data_stage() = 0;
    virtual DataResult handle_ctrl_out_data_stage() = 0;
    virtual void handle_ctrl_status_stage() = 0;

    // other transfers
    virtual void handle_in_transfer(Endpoint& ep) = 0;
    virtual void handle_out_transfer(Endpoint& ep) = 0;

protected:
    Endpoint& get_ep(uint8_t n) {
        return device->get_ep(n);
    }

    ControlEndpoint& get_ep0() {
        return device->endpoint_0;
    }

    SetupPacket const& get_setup_pkt() {
        return get_ep0().setup_packet;
    }

protected:
    //ControlEndpoint* ep0;
    Device* device;
};

#endif // USB_CLASS_H
