#include <algorithm>

#include "usb/standard_requests.h"
#include "usb/descriptors.h"
#include "usb/transfers.h"
#include "print.h"


//BufferRxTransfer<3, MockHandler> ctrl_rx_transfer;
static BufferTxTransfer<StandardRequests> ctrl_tx_transfer;


SetupResult StandardRequests::on_ctrl_setup_stage()
{
    //
    // device requests
    //

    // get descriptor
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_IN) &&
        get_setup_pkt().bRequest == GET_DESCRIPTOR
    ) {
        print("get descriptor\n");
        return send_descriptor();
    }

    // set address
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_OUT) &&
        get_setup_pkt().bRequest == SET_ADDRESS
    ) {
        print("set address %s\n", get_setup_pkt().wValue);
        device->set_address(get_setup_pkt().wValue);
        return SetupResult::NO_DATA_STAGE;
    }

    // get configuration
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_IN) &&
        get_setup_pkt().bRequest == GET_CONFIGURATION
    ) {
        print("get configuration\n");

        static unsigned char data[] = {};
        data[0] = device->get_configuration();

        ctrl_tx_transfer.init(data, sizeof(data), this, [](StandardRequests& self, BufferTxTransfer<StandardRequests>& transfer) {
            //print("tx_transfer callback, sent %d bytes\n", transfer.get_transferred());
        });
        submit(0, ctrl_tx_transfer);

        return SetupResult::DATA_STAGE;
    }

    // set configuration
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_OUT) &&
        get_setup_pkt().bRequest == SET_CONFIGURATION
    ) {
        print("set configuration %s\n", get_setup_pkt().wValue & 0xFF);

        if (device->set_configuration(get_setup_pkt().wValue & 0xFF)) {
            return SetupResult::NO_DATA_STAGE;
        }

        return SetupResult::STALL;
    }

    // get device status
    // TODO bit 0 = self-powered, bit 1 = remote wkup enabled
    if (get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_DEVICE | ENDPOINT_IN) &&
        get_setup_pkt().bRequest == GET_STATUS
    ) {
        print("get status\n");

        static unsigned char const data[] = {0x00, 0x00};

//        get_ep0().init_transfer(data, sizeof(data));
//        device->start_in_transfer(&get_ep0());

        ctrl_tx_transfer.init(data, sizeof(data), this, [](StandardRequests& self, BufferTxTransfer<StandardRequests>& transfer) {
            //print("tx_transfer callback, sent %d bytes\n", transfer.get_transferred());
        });
        submit(0, ctrl_tx_transfer);

        return SetupResult::DATA_STAGE;
    }

    //
    // interface
    //

    // set interface
    if(get_setup_pkt().bmRequestType == (REQUEST_TYPE_STANDARD | RECIPIENT_INTERFACE | ENDPOINT_OUT) &&
       get_setup_pkt().bRequest == SET_INTERFACE
    ) {
        print("set interface\n");
        return SetupResult::NO_DATA_STAGE;
    }

    print("unknown\n");
    return SetupResult::UNHANDLED;
}


SetupResult StandardRequests::send_descriptor()
{
    uint8_t const descriptor_type = get_setup_pkt().wValue >> 8;
    uint8_t const descriptor_index = get_setup_pkt().wValue & 0xFF;
    uint16_t const descriptor_lang_id = get_setup_pkt().wIndex;

    print("send_descriptor() type=%#x index=%s langid=%#x: ",
        descriptor_type, descriptor_index, descriptor_lang_id);

    unsigned char const* descriptor_buf;
    uint16_t descriptor_size;

    switch (descriptor_type) {
    case DESCRIPTOR_DEVICE:
        print("device\n");
        descriptor_buf = reinterpret_cast<unsigned char const*>(descriptors->device);
        descriptor_size = descriptors->device->bLength;
        break;

    case DESCRIPTOR_CONFIGURATION:
        print("config\n");
        descriptor_buf = reinterpret_cast<unsigned char const*>(descriptors->config);
        descriptor_size = descriptors->config->wTotalLength;
        break;

    case DESCRIPTOR_STRING:
        print("string\n");
        if (descriptor_index == 0) {
            descriptor_buf = reinterpret_cast<unsigned char const*>(descriptors->lang_id);
            descriptor_size = descriptors->lang_id->bLength;
        } else if (descriptor_index == 0xEE && descriptors->msft_string != nullptr) {
            descriptor_buf = descriptors->msft_string;
            descriptor_size = descriptors->msft_string[0];
        } else {
            if (descriptor_index > descriptors->string_len) {
                print("index too large!\n");
                return SetupResult::STALL;
            }

            descriptor_buf = descriptors->string[descriptor_index - 1];
            descriptor_size = descriptor_buf[0];
        }
        break;

    default:
        print("unknown\n");
        return SetupResult::UNHANDLED;
    }

    // host can request more or fewer bytes than the actual descriptor size
    uint16_t length = std::min(descriptor_size, get_setup_pkt().wLength);

    ctrl_tx_transfer.init(descriptor_buf, length, this, [](StandardRequests& self, BufferTxTransfer<StandardRequests>& transfer) {
        //print("tx_transfer callback, sent %d bytes\n", transfer.get_transferred());
    });
    submit(0, ctrl_tx_transfer);

    return SetupResult::DATA_STAGE;
}
