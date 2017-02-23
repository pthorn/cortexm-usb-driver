#ifndef USB_CLASS_H
#define USB_CLASS_H

#include "usb/control_endpoint.h"
#include "usb/dwc_otg_device.h"


class USBClass
{
protected:
    USBClass() = default;

public:
    void on_attached(Device* device) {
        //this->ep0 = ep0;
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
    virtual SetupResult handle_ctrl_setup_stage();
    virtual DataResult handle_ctrl_in_data_stage();
    virtual DataResult handle_ctrl_out_data_stage();
    virtual void handle_ctrl_status_stage();

    // other transfers
    virtual void handle_in_transfer(Endpoint& ep);
    virtual void handle_out_transfer(Endpoint& ep);

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
