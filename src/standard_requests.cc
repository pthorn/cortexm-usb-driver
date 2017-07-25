#include <algorithm>

#include "usb/standard_requests.h"
#include "usb/descriptors.h"
#include "usb/transfers.h"
#include "usb/debug.h"


//BufferRxTransfer<3, MockHandler> ctrl_rx_transfer;
static TxTransfer<StandardRequests> ctrl_tx_transfer;


SetupResult StandardRequests::on_ctrl_setup_stage()
{
    //
    // device requests
    //

    // get descriptor
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_IN) &&
        get_setup_pkt().bRequest == GET_DESCRIPTOR
    ) {
        d_info("get descriptor\n");
        return send_descriptor();
    }

    // set address
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_OUT) &&
        get_setup_pkt().bRequest == SET_ADDRESS
    ) {
        d_info("set address %s\n", get_setup_pkt().wValue);
        device->set_address(get_setup_pkt().wValue);
        return SetupResult::OK;
    }

    // get configuration
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_IN) &&
        get_setup_pkt().bRequest == GET_CONFIGURATION
    ) {
        d_info("get configuration\n");

        static unsigned char data[] = {0x00};
        data[0] = device->get_configuration();

        ctrl_tx_transfer.init(data, sizeof(data), this);
        submit(0, ctrl_tx_transfer);

        return SetupResult::OK;
    }

    // set configuration
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_OUT) &&
        get_setup_pkt().bRequest == SET_CONFIGURATION
    ) {
        d_info("set configuration %s\n", get_setup_pkt().wValue & 0xFF);

        if (device->set_configuration(get_setup_pkt().wValue & 0xFF)) {
            return SetupResult::OK;
        }

        return SetupResult::STALL;
    }

    // get device status
    // TODO bit 0 = self-powered, bit 1 = remote wkup enabled
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_IN) &&
        get_setup_pkt().bRequest == GET_STATUS
    ) {
        d_info("get status\n");

        static unsigned char const data[] = {0x00, 0x00};

        ctrl_tx_transfer.init(data, sizeof(data), this);
        submit(0, ctrl_tx_transfer);

        return SetupResult::OK;
    }

    //
    // interface
    //

    // set interface
    if(get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_INTERFACE | ENDPOINT_OUT) &&
       get_setup_pkt().bRequest == SET_INTERFACE
    ) {
        d_info("set interface\n");
        return SetupResult::OK;
    }

    d_info("unknown\n");
    return SetupResult::UNHANDLED;
}


SetupResult StandardRequests::send_descriptor()
{
    uint8_t const descriptor_type = get_setup_pkt().wValue >> 8;
    uint8_t const descriptor_index = get_setup_pkt().wValue & 0xFF;
    uint16_t const descriptor_lang_id = get_setup_pkt().wIndex;

    d_info("send_descriptor() type=%#x index=%s langid=%#x: ",
        descriptor_type, descriptor_index, descriptor_lang_id);

    unsigned char const* descriptor_buf;
    uint16_t descriptor_size;

    switch (descriptor_type) {
    case DESCRIPTOR_DEVICE:
        d_info("device\n");
        descriptor_buf = reinterpret_cast<unsigned char const*>(descriptors->device);
        descriptor_size = descriptors->device->bLength;
        break;

    case DESCRIPTOR_CONFIGURATION:
        d_info("config\n");
        descriptor_buf = reinterpret_cast<unsigned char const*>(descriptors->config);
        descriptor_size = descriptors->config->wTotalLength;
        break;

    case DESCRIPTOR_STRING:
        d_info("string\n");
        if (descriptor_index == 0) {
            descriptor_buf = reinterpret_cast<unsigned char const*>(descriptors->lang_id);
            descriptor_size = descriptors->lang_id->bLength;
        } else if (descriptor_index == 0xEE && descriptors->msft_string != nullptr) {
            descriptor_buf = descriptors->msft_string;
            descriptor_size = descriptors->msft_string[0];
        } else {
            if (descriptor_index > descriptors->string_len) {
                d_info("index too large!\n");
                return SetupResult::STALL;
            }

            descriptor_buf = descriptors->string[descriptor_index - 1];
            descriptor_size = descriptor_buf[0];
        }
        break;

    default:
        d_info("unknown\n");
        return SetupResult::UNHANDLED;
    }

    // host can request more or fewer bytes than the actual descriptor size
    uint16_t length = std::min(descriptor_size, get_setup_pkt().wLength);

    ctrl_tx_transfer.init(descriptor_buf, length, this);
    submit(0, ctrl_tx_transfer);

    return SetupResult::OK;
}
