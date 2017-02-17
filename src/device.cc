#include "usb/device.h"
#include "usb/usb_class.h"
#include "print.h"


Device::Device(
    ControlEndpoint& endpoint_0
):
    endpoint_0(endpoint_0)
{
    add_ep(endpoint_0);
}


void Device::add_ep(Endpoint& ep) {
    // TODO check endpoint number
    if (ep.get_number() >= NO_OF_ENDPOINTS) {
        print("add_ep(): bad EP #{}\n", ep.get_number());
        return;
    }

    endpoints[ep.get_number()] = &ep;
    ep.on_attached(this);
}


void Device::add_handler(USBClass* handler)
{
    for (auto& el: handlers) {
        if (el == nullptr) {
            el = handler;
            handler->on_attached(this);
            return;
        }
    }

    while (true) ;;
}


bool Device::set_configuration(uint8_t configuration)
{
    for (auto handler: handlers) {
        if (handler == nullptr) {
            continue;
        }

        handler->on_set_configuration(configuration);
    }
}
