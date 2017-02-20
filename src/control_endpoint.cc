#include "usb/control_endpoint.h"
#include "usb/device.h"
#include "usb/usb_class.h"
#include "print.h"


void ControlEndpoint::on_setup_stage()
{
    print("ControlEndpoint::on_setup_stage() bmRT=%#x bR=%#x wI=%s wV=%s wL=%s\n",
        setup_packet.bmRequestType, setup_packet.bRequest,
        setup_packet.wIndex, setup_packet.wValue,
        setup_packet.wLength);

    for (auto handler: device->handlers) {
        if (handler == nullptr) {
            continue;
        }

        auto result = handler->handle_ctrl_setup_stage();

        if (result == SetupResult::UNHANDLED) {
            continue;
        }

        if (result == SetupResult::DATA_STAGE) {
            // data stage transfer has been initialized by a handler
            print("CE: status stage compl, data stage\n");
            state = CtrlState::DATA_STAGE;

        } else if (result == SetupResult::NO_DATA_STAGE) {
            // initialize status stage
            if (setup_packet.bmRequestType & ENDPOINT_IN) {
                print("CE: IN status stage compl, no data stage, receiving (OUT) ZLP\n");
                device->ep0_receive_zlp();
            } else {
                print("CE: OUT status stage compl, no data stage, sending (IN) ZLP\n");
                device->transmit_zlp(&device->endpoint_0);
            }

            state = CtrlState::STATUS_STAGE;

        } else if (result == SetupResult::STALL) {
            break;  // stall
        }

        current_handler = handler;
        return;
    }

    device->stall(get_number());
}


// TODO do we actually need separate callbacks for IN and OUT?
void ControlEndpoint::on_in_transfer_complete()
{
    if (current_handler == nullptr) {
        print("ControlEndpoint::on_in_transfer_complete(): current_handler == nullptr!\n");
        return;
    }

    if (state == CtrlState::DATA_STAGE) {
        auto result = current_handler->handle_ctrl_in_data_stage();

        if (result == DataResult::DONE) {
            print("CE: IN data stage compl, receiving (OUT) ZLP\n");
            // init status stage
            device->ep0_receive_zlp();
        } else {
            print("CE: IN data stage compl, STALL\n");
            device->stall(0);
        }

        state = CtrlState::STATUS_STAGE;

    } else if (state == CtrlState::STATUS_STAGE) {
        current_handler->handle_ctrl_status_stage();
        reinit();
    }
}


// TODO a lot of duplicated code
void ControlEndpoint::on_out_transfer_complete()
{
    if (current_handler == nullptr) {
        print("ControlEndpoint::on_out_transfer_complete(): current_handler == nullptr!\n");
        return;
    }

    if (state == CtrlState::DATA_STAGE) {
        auto result = current_handler->handle_ctrl_out_data_stage();

        if (result == DataResult::DONE) {
            print("CE: OUT data stage compl, sending (IN) ZLP\n");
            // init status stage
            device->transmit_zlp(&device->endpoint_0);
        } else {
            print("CE: OUT data stage compl, STALL\n");
            device->stall(0);
        }

        state = CtrlState::STATUS_STAGE;

    } else if (state == CtrlState::STATUS_STAGE) {
        current_handler->handle_ctrl_status_stage();
        reinit();
    }
}


void ControlEndpoint::reinit()
{
    state = CtrlState::START;
    current_handler = nullptr;
    // TODO init EP0 to rx another setup pkt
}
