#ifndef DISPATCHER_IMPL_H
#define DISPATCHER_IMPL_H

#include "dispatchers.h"
#include "device.h"
#include "handler.h"
#include "transfers.h"
#include "debug.h"


template <typename Device>
void EPDispatcher<Device>::on_in_transfer_complete(uint8_t ep_n) {
    auto transfer = device.in_transfers[ep_n];
    if (transfer == nullptr) {
        d_assert("transfer == nullptr");
    }

    device.in_transfers[ep_n] = nullptr;
    transfer->on_complete();
}


template <typename Device>
void EPDispatcher<Device>::on_out_transfer_complete(uint8_t ep_n) {
    auto transfer = device.out_transfers[ep_n];
    if (transfer == nullptr) {
        d_assert("transfer == nullptr");
    }

    device.out_transfers[ep_n] = nullptr;
    transfer->on_complete();
}


namespace {
    ZeroLengthRxTransfer zl_rx_transfer;
    ZeroLengthTxTransfer zl_tx_transfer;
}


template <typename Device>
void CtrlEPDispatcher<Device>::on_setup_stage(uint8_t ep_n)
{
    d_info("CtrlEPDispatcher::on_setup_stage() bmRT=%#x bR=%#x wI=%s wV=%s wL=%s\n",
        setup_packet.bmRequestType, setup_packet.bRequest,
        setup_packet.wIndex, setup_packet.wValue,
        setup_packet.wLength);

    // TODO possible irl
    if (device.in_transfers[ep_n] != nullptr || device.in_transfers[ep_n] != nullptr) {
        d_warn("device.in_transfers[ep_n] != nullptr || device.in_transfers[ep_n] != nullptr, CtrlEPDispatcher::on_setup_stage()\n");
    }
    device.in_transfers[ep_n] = nullptr;
    device.out_transfers[ep_n] = nullptr;

    // find a handler that recognizes the request

    SetupResult result = SetupResult::UNHANDLED;
    for (auto handler: device.handlers) {
        if (handler == nullptr) {
            continue;
        }

        result = handler->on_ctrl_setup_stage();

        if (result != SetupResult::UNHANDLED) {
            break;
        }
    }

    // TODO only STALLs IN transfers, not OUT transfers! (applies to data transfers only?)
    // TODO test stalls w/ data and no-data transfers
    // TODO does not reinitialize STUPCNT
    if (result == SetupResult::UNHANDLED || result == SetupResult::STALL) {
        device.stall(ep_n);  // TODO stall IN or OUT?
        return;
    }

    bool const has_data_stage = setup_packet.wLength > 0;
    InOut const in_out = setup_packet.bmRequestType & ENDPOINT_IN ? InOut::In : InOut::Out;

    if (has_data_stage) {
        if (in_out == InOut::In && device.in_transfers[ep_n] == nullptr) {
            d_assert("handler did not submit an IN transfer for data stage\n");
        }
        if (in_out == InOut::Out && device.out_transfers[ep_n] == nullptr) {
            d_assert("handler did not submit an OUT transfer for data stage\n");
        }

        // data stage transfer has been initialized by a handler
        d_info("CE: setup stage compl, data stage\n");
        state = CtrlState::DATA_STAGE;
        return;
    }

    // no data stage: initialize status stage (zero length transfer)

    if (in_out == InOut::In) {
        d_info("CE: IN setup stage compl, no data stage, receiving (OUT) ZLP\n");
        device.submit(0, zl_rx_transfer);
    } else {
        d_info("CE: OUT setup stage compl, no data stage, sending (IN) ZLP\n");
        device.submit(0, zl_tx_transfer);
    }

    state = CtrlState::STATUS_STAGE;
}


template <typename Device>
void CtrlEPDispatcher<Device>::on_in_transfer_complete(uint8_t ep_n)
{
    auto transfer = device.in_transfers[ep_n];
    if (transfer == nullptr) {
        d_assert("CtrlEPDispatcher::on_out_transfer_complete(): current_handler == nullptr!\n");
    }

    device.in_transfers[ep_n] = nullptr;
    transfer->on_complete();  // may install another transfer

    if (state == CtrlState::DATA_STAGE) {
        // data stage complete, init status stage
        d_info("CE: IN data stage compl, receiving (OUT) ZLP\n");
        device.submit(0, zl_rx_transfer);

//        if (result == DataResult::DONE) {
//        } else {
//            d_info("CE: IN data stage compl, STALL\n");
//            device.stall(0);
//        }

        state = CtrlState::STATUS_STAGE;
        return;
    }

    if (state == CtrlState::STATUS_STAGE) {
        d_info("CE: IN status stage compl, reinit\n");
        reinit();
    }
}


template <typename Device>
void CtrlEPDispatcher<Device>::on_out_transfer_complete(uint8_t ep_n)
{
    auto transfer = device.out_transfers[ep_n];
    if (transfer == nullptr) {
        d_assert("CtrlEPDispatcher::on_out_transfer_complete(): current_handler == nullptr!\n");
    }

    device.out_transfers[ep_n] = nullptr;
    transfer->on_complete();  // may install another transfer

    if (state == CtrlState::DATA_STAGE) {
        // data stage complete, init status stage
        d_info("CE: OUT data stage compl, sending (IN) ZLP\n");
        device.submit(0, zl_tx_transfer);

//        if (result == DataResult::DONE) {
//        } else {
//            d_info("CE: OUT data stage compl, STALL\n");
//            device.stall(0);
//        }

        state = CtrlState::STATUS_STAGE;
        return;
    }

    if (state == CtrlState::STATUS_STAGE) {
        d_info("CE: OUT status stage compl, reinit\n");
        reinit();
    }
}


template <typename Device>
void CtrlEPDispatcher<Device>::reinit()
{
    device.ep0_init_ctrl_transfer();
    state = CtrlState::START;
}


#endif // DISPATCHER_IMPL_H
