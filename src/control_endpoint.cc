#include "control_endpoint.h"
#include "device.h"
#include "usb_class.h"
#include "print.h"


void ControlEndpoint::on_setup_stage()
{
    print("ControlEndpoint::on_setup_stage() bmRT=0x{:x} bR=0x{:x} wI={} wV={} wL={}\n",
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
            state = CtrlState::SETUP_STAGE_COMPLETE;
        } else if (result == SetupResult::NO_DATA_STAGE) {
            state = CtrlState::DATA_STAGE_COMPLETE;
        } else if (result == SetupResult::STALL) {
            break;  // stall
        }

        current_handler = handler;
        return;
    }

    device->stall(get_number());

    // TODO what now
}


// TODO do we actually need separate callbacks for IN and OUT?
void ControlEndpoint::on_in_transfer_complete()
{
    if (current_handler == nullptr) {
        print("ControlEndpoint::on_in_transfer_complete(): current_handler == nullptr!\n");
        while (true) ;;
    }

    if (state == CtrlState::SETUP_STAGE_COMPLETE) {
        if (current_handler != nullptr) {
            auto result = current_handler->handle_ctrl_in_data_stage();
            // TODO implement stall here
        }

        // TODO reinit for status stage
        // Note: the handler will init the OUT status stage TODO do it all here!
        // but the IN status stage must be initialized here(?)
        state = CtrlState::DATA_STAGE_COMPLETE;

    } else if (state == CtrlState::DATA_STAGE_COMPLETE) {
        if (current_handler != nullptr) {
            current_handler->handle_ctrl_status_stage();
        }

        reinit();
    }
}


// TODO a lot of duplicated code
void ControlEndpoint::on_out_transfer_complete()
{
    if (current_handler == nullptr) {
        print("ControlEndpoint::on_in_transfer_complete(): current_handler == nullptr!\n");
        while (true) ;;
    }

    if (state == CtrlState::SETUP_STAGE_COMPLETE) {
        if (current_handler != nullptr) {
            auto result = current_handler->handle_ctrl_out_data_stage();
            // TODO implement stall here
        }

        // TODO reinit for status stage
        // Note: the handler will init the OUT status stage TODO do it all here!
        // but the IN status stage must be initialized here(?)
        state = CtrlState::DATA_STAGE_COMPLETE;

    } else if (state == CtrlState::DATA_STAGE_COMPLETE) {
        if (current_handler != nullptr) {
            current_handler->handle_ctrl_status_stage();
        }

        reinit();
    }
}


void ControlEndpoint::reinit()
{
    state = CtrlState::START;
    current_handler = nullptr;
    // TODO init EP0 to rx another setup pkt!
}
