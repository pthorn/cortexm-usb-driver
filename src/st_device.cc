#include "st_device.h"

#if 0
// http://cgit.jvnv.net/laks/tree/usb/l0_usb.h?h=l0_support

struct EP {
    volatile uint16_t EPR;
    volatile uint16_t RESERVED;
};

EP eps[8];


ST_Device::ST_Device()
{

}


void ST_Device::init_usb()
{
    // TODO unmask interrupts in CNTR

    USB->CNTR = 3;
    Time::sleep(10);
    USB->CNTR = 1;
    Time::sleep(10);
    // Exit power down mode.
    USB->CNTR = 0;
}


void ST_Device::isr()
{
    uint32_t const istr = USB->ISTR;

    if (istr & USB_ISTR_RESET) {  // USB_ISTR_RESET
        usb_rblog.log("USB Reset");
        USB->ISTR = ~(1 << 10);

        buf_end = 0x40;
        USB->DADDR = 0x80;

        handle_reset();

        return;
    }

    if (istr & USB_ISTR_CTR) {  // USB_ISTR_CTR
        usb_rblog.log("USB Transfer: %02x", istr & 0x1f);
        usb_rblog.log("EPR%d: %04x", ep, USB->EPR[ep]);

        uint32_t const dir = istr & USB_ISTR_DIR;

        if (dir) {
            isr_handle_rx();  // TODO could be both tx and rx!
        } else {
            isr_handle_tx();
        }
    }
}


void ST_Device::isr_handle_tx()
{
    uint32_t const ep_n = istr & 0xf;

    usb_rblog.log("TXBUF: ADDR: %04x, COUNT: %04x", usb.bufd[ep].ADDR_TX, usb.bufd[ep].COUNT_TX);

    if (pending_addr) {
        usb_rblog.log("Actually changing addr to: %d", pending_addr);
        USB->DADDR = 0x80 | pending_addr;
        pending_addr = 0;
    }

    USB->EPR[ep] &= 0x870f;
}


void ST_Device::isr_handle_rx()
{
    uint32_t const ep_n = istr & 0xf;

    usb_rblog.log("RXBUF: ADDR: %04x, COUNT: %04x", usb.bufd[ep].ADDR_RX, usb.bufd[ep].COUNT_RX);

    uint32_t len = usb.bufd[ep].COUNT_RX & 0x03ff;

    if(USB->EPR[ep] & (1 << 11)) {
        usb_rblog.log("SETUP packet received");
        read(0, setupbuf, 8);
        endpoint_0.on_setup_stage();
    } else {
        usb_rblog.log("OUT packet received");
        handle_out(ep, len);

        auto buffer = ep->get_buffer(len);

        if (buffer != nullptr) {
            read_packet(buffer, len);
            ep->on_filled(buffer, len);
        } // else what? need to discard pkt
    }

    //USB->EPR[ep] = 0x9280;
    //USB->EPR[ep] &= 0x078f;

    USB->EPR[ep] = (USB->EPR[ep] & 0x078f) | 0x1000;
}
#endif
