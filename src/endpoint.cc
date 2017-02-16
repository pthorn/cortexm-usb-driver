#include "endpoint.h"
#include "device.h"
#include "usb_class.h"


/*
void Endpoint::start_transfer(uint8_t const* data, uint32_t remaining_bytes) {
    this->data_to_tx = data;
    this->remaining_bytes = remaining_bytes;
    device->start_transfer(this);
}
*/


void Endpoint::on_in_transfer_complete()
{
    for (auto handler: device->handlers) {
        if (handler == nullptr) {
            continue;
        }

        /*auto result = */handler->handle_in_transfer(*this);
    }
}


void Endpoint::on_out_transfer_complete()
{
    for (auto handler: device->handlers) {
        if (handler == nullptr) {
            continue;
        }

        /*auto result = */handler->handle_out_transfer(*this);
    }
}
