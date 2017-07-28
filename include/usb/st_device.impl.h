#ifndef ST_DEVICE_IMPL_H
#define ST_DEVICE_IMPL_H

#include <algorithm>
#include <stm32f0xx.h>


struct EP {
    volatile uint16_t EPR;
    volatile uint16_t RESERVED;
};

#define EPs ((EP*)USB_BASE)

struct BufEntry {
    volatile uint16_t ADDR_TX;
    volatile uint16_t COUNT_TX;
    volatile uint16_t ADDR_RX;
    volatile uint16_t COUNT_RX;
};

using BTableT = BufEntry[8];

#define BTable ((BufEntry*)USB_PMAADDR)
#define Buffer ((uint16_t*)USB_PMAADDR)


// TODO may lose last byte
void read_packet(uint8_t ep_n, uint8_t* buf, uint16_t len)
{
    uint16_t* p = (uint16_t*)buf;
    uint32_t base = BTable[ep_n].ADDR_RX >> 1;

    for(uint32_t i = 0; i < len; i += 2) {
        *p++ = uint16_t(  Buffer[base + (i >> 1)]  );
    }
}

void write_packet(uint8_t ep_n, uint8_t const* buf, uint16_t len)
{
    uint16_t* p = (uint16_t*)buf;
    uint32_t base = BTable[ep_n].ADDR_TX >> 1;

    for(uint32_t i = 0; i < len; i += 2) {
        Buffer[base + (i >> 1)] = *p++;
    }
}


template<size_t NHandlers, size_t NEndpoints>
ST_Device<NHandlers, NEndpoints>::ST_Device(
    EndpointConfig const* endpoint_config,
    Descriptors const& descriptors
):
    Device<NHandlers, NEndpoints>(endpoint_config, descriptors),
    buf_end(0),
    pending_addr(0)
{
}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::init()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();  // TODO ???
    __HAL_RCC_USB_CLK_ENABLE();

    USB->CNTR = USB_CNTR_PDWN | USB_CNTR_FRES;
    for (volatile int i = 0; i < 100000; ++i) ;;
    USB->CNTR = USB_CNTR_FRES;
    for (volatile int i = 0; i < 100000; ++i) ;;
    USB->CNTR = 0;  // Exit power down mode.

    USB->CNTR =
        USB_CNTR_CTRM |
        USB_CNTR_RESETM |
        0;

    USB->ISTR = 0;  // clear interrupts

    NVIC_SetPriority(USB_IRQn, 2);
    NVIC_EnableIRQ(USB_IRQn);

    USB->BCDR |= USB_BCDR_DPPU;  // pulldown
}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::isr()
{
    uint32_t const istr = USB->ISTR;

    if (istr & USB_ISTR_RESET) {
        USB->ISTR = ~USB_ISTR_RESET;
        d_info("USB RESET\n");

        buf_end = sizeof(BTableT);
        USB->DADDR = USB_DADDR_EF;

        init_ep0();
        on_reset();
        return;
    }

    if (istr & USB_ISTR_CTR) {  // transaction completed
        uint8_t const ep_n = istr & USB_ISTR_EP_ID;
        uint32_t const dir = istr & USB_ISTR_DIR;

        d_info("CTR: ep %d %s\n", ep_n, dir ? "OUT" : "IN");

        if (dir) {
            isr_handle_rx();  // TODO could be both tx and rx!
            if (EPs[ep_n].EPR & USB_EP_CTR_TX) {
                isr_handle_tx();
            }
        } else {
            isr_handle_tx();
        }
    }
}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::set_address(uint16_t address)
{
    pending_addr = address;
}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::submit(uint8_t ep_n, IRxTransfer& transfer)
{
    // TODO check if transfer is already in progress?
    out_transfers[ep_n] = &transfer;

    auto epr = EPs[ep_n].EPR;
    epr &= ~(
        USB_EP_TX_VALID |  // do not toggle STAT_TX bits
        USB_EP_DTOG_TX |
        USB_EP_DTOG_RX);
    epr ^= USB_EP_RX_VALID;
    EPs[ep_n].EPR = epr;
}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::submit(uint8_t ep_n, ITxTransfer& transfer)
{
    // TODO check if transfer is already in progress?
    in_transfers[ep_n] = &transfer;

    uint32_t const chunk_size = std::min(
        transfer.get_remaining(),
        (size_t)64 // TODO (size_t)get_dwc_ep_config(ep_n).tx_fifo_size
    );

    d_info("submit() IN: ep %s, bytes %s, first chunk %s\n",
          ep_n, transfer.get_remaining(), chunk_size);

    write_packet(ep_n, transfer.get_data_ptr(), chunk_size);

    // program endpoint to transmit
    BTable[ep_n].COUNT_TX = chunk_size;

    auto epr_old = EPs[ep_n].EPR;
    auto epr = EPs[ep_n].EPR;
    epr &= ~(
        USB_EP_RX_VALID | // do not toggle STAT_RX bits
        USB_EP_DTOG_TX |
        USB_EP_DTOG_RX);
//    epr |= (epr & (1 << 5)) ? 0 : (1 << 5);  // TODO
//    epr |= (epr & (1 << 4)) ? 0 : (1 << 4);
    epr ^= USB_EP_TX_VALID;
    EPs[ep_n].EPR = epr;

    transfer.on_transferred(chunk_size);

    print("EP0R: %02x (%02x->%02x)\n", EPs[ep_n].EPR, epr_old, epr);
}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::stall(uint8_t ep)
{

}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::ep0_init_ctrl_transfer()
{
    // TODO ?
    auto epr = EPs[0].EPR;
    auto epr_old = epr;
    epr &= ~(
        USB_EP_TX_VALID |  // do not toggle STAT_TX
        USB_EP_DTOG_TX |
        USB_EP_DTOG_RX);
    epr ^= USB_EP_RX_VALID;
    EPs[0].EPR = epr;

    print("reinit: EP0R %02x (%02x->%02x)\n", EPs[0].EPR, epr_old, epr);
}


template <size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::init_in_endpoint(EndpointConfig const& ep_conf)
{
    //
}


template <size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::init_out_endpoint(EndpointConfig const& ep_conf)
{
    //
}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::init_ep0()
{
    BTable[0].ADDR_TX = buf_end;
    buf_end += 64;  // TODO
    BTable[0].ADDR_RX = buf_end;
    buf_end += 64;  // TODO
    BTable[0].COUNT_RX =
        (1 << 15) |  // block is 32 bytes long
        (2 << 10);   // 2 blocks = 64 bytes

    // TODO cannot just set USB_EP_TX_NAK and VALID!
    EPs[0].EPR =
        (1 << 9) |   // endpoint type: control
        USB_EP_TX_NAK  |
        USB_EP_RX_VALID;

    //print("init_ep0(): EP0R: %02x CNTR: %04x\n", USB->EP0R, USB->CNTR);
}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::isr_handle_tx()
{
    uint32_t const ep_n = USB->ISTR & 0xf;

    d_info("TXBUF: ADDR: %04x, COUNT: %04x\n",
        BTable[ep_n].ADDR_TX, BTable[ep_n].COUNT_TX);

    if (pending_addr) {
        d_info("Actually changing addr to: %d\n", pending_addr);
        USB->DADDR = USB_DADDR_EF | pending_addr;
        pending_addr = 0;
    }

    auto transfer = in_transfers[ep_n];
    if (transfer == nullptr) {
        d_assert("!!! transfer is null\n"); // TODO, ep_n);
    }

    auto epr = EPs[ep_n].EPR;
    epr &= ~(
        USB_EP_CTR_TX |    // clear interrupt bit
        USB_EP_RX_VALID |  // do not toggle STAT_RX bits
        USB_EP_DTOG_TX |
        USB_EP_DTOG_RX);

    if (transfer->get_remaining() > 0) {
        uint16_t const chunk_size = std::min(
            transfer->get_remaining(), (size_t)(64));  // TODO 64!

        write_packet(ep_n, transfer->get_data_ptr(), chunk_size);

        BTable[ep_n].COUNT_TX = chunk_size;
        epr ^= USB_EP_TX_VALID;

        transfer->on_transferred(chunk_size);
    }

    EPs[ep_n].EPR = epr;

    if (transfer->get_remaining() == 0) {
        dispatch_in_transfer_complete(ep_n);
    }
}


template<size_t NHandlers, size_t NEndpoints>
void ST_Device<NHandlers, NEndpoints>::isr_handle_rx()
{
    uint8_t const ep_n = USB->ISTR & 0xf;

    d_info("RXBUF: ADDR: %04x, COUNT: %04x\n",
        BTable[ep_n].ADDR_RX, BTable[ep_n].COUNT_RX);

    uint32_t len = BTable[ep_n].COUNT_RX & 0x03ff;

    auto epr = EPs[ep_n].EPR;
    epr &= ~(
        USB_EP_CTR_RX |    // clear interrupt bit
        USB_EP_TX_VALID |  // do not toggle STAT_TX
        USB_EP_DTOG_TX |
        USB_EP_DTOG_RX);

    if(EPs[ep_n].EPR & USB_EP_SETUP) {
        d_info("SETUP packet received\n");

        auto buf = ctrl_ep_dispatcher.get_setup_pkt_buffer();
        read_packet(ep_n, buf, len);

        epr &= ~USB_EP_RX_VALID;  // do not touch STAT_RX bits
        EPs[ep_n].EPR = epr;  // reset interrupt before new transfer starts

        ctrl_ep_dispatcher.on_setup_stage(ep_n);
        return;
    }

    d_info("OUT packet received\n");

    auto transfer = out_transfers[ep_n];
    if (transfer == nullptr) {
        d_assert("!!! EP %s is null\n"); // TODO, ep_n);
    }

    auto buffer = transfer->get_buffer(len);
    if (buffer != nullptr) {
        read_packet(BTable[ep_n].ADDR_RX, buffer, len);
        transfer->on_filled(buffer, len);
    } // else what? need to discard pkt

    if (transfer->get_remaining() > 0) {
        // TODO reinit endpoint
        BTable[ep_n].COUNT_RX &= ~0x03ff;  // TODO ?
        epr ^= USB_EP_RX_VALID;
//        epr |= (epr & (1 << 13)) ? 0 : (1 << 13);  // TODO
//        epr |= (epr & (1 << 12)) ? 0 : (1 << 12);
    }

    EPs[ep_n].EPR = epr;

    // TODO
    if (transfer->get_remaining() == 0) { // TODO || short_packet_received) {
        dispatch_out_transfer_complete(ep_n);
    }

    //USB->EPR[ep] = 0x9280;
    //USB->EPR[ep] &= 0x078f;

    //EPs[ep_n].EPR = (EPs[ep_n].EPR & 0x078f) | 0x1000;
}

#endif // ST_DEVICE_IMPL_H
