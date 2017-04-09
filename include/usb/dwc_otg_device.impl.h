#ifndef DWC_OTG_DEVICE_IMPL_H
#define DWC_OTG_DEVICE_IMPL_H

#include <algorithm>

#include "dwc_otg_header.h"
#include "transfers.h"
#include "debug.h"


#define CORE_BASE CoreAddr
#define USB_CORE ((USB_OTG_GlobalTypeDef *)CORE_BASE)
//static USB_OTG_DeviceTypeDef * const USB_DEV = (USB_OTG_DeviceTypeDef *)((uint32_t)CORE_BASE + USB_OTG_DEVICE_BASE);
#define GHWCFG2 ((volatile uint32_t *)(CORE_BASE + 0x48))
#define GHWCFG3 ((volatile uint32_t *)(CORE_BASE + 0x4C))
#define USB_DEV ((USB_OTG_DeviceTypeDef *)(CORE_BASE + USB_OTG_DEVICE_BASE))
#define USB_INEP(i)  ((USB_OTG_INEndpointTypeDef *)(( uint32_t)CORE_BASE + USB_OTG_IN_ENDPOINT_BASE + (i)*USB_OTG_EP_REG_SIZE))
#define USB_OUTEP(i) ((USB_OTG_OUTEndpointTypeDef *)((uint32_t)CORE_BASE + USB_OTG_OUT_ENDPOINT_BASE + (i)*USB_OTG_EP_REG_SIZE))
#define USB_FIFO(i) ((volatile uint32_t *)(CORE_BASE + USB_OTG_FIFO_BASE + (i) * USB_OTG_FIFO_SIZE))


// TODO anon ns
template<typename T>
static T div_round_up(T a, T b)
{
    if (a == 0) {
        return 1;
    }

    return (a + (b - 1)) / b;
}


// TODO params: core (FS or HS), timing, Vbus sensing, interrupt priority
// TODO (not incl. EP0): F4 FS: 3, HS: 5; F7 FS: 5, HS: 8
template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::DWC_OTG_Device(
    EndpointConfig const* endpoint_config,
    DWCEndpointConfig const* dwc_endpoint_config,
    Descriptors const& descriptors,
    uint16_t rxfifo_size
    //bool vbus_sensing
):
    Device<NHandlers, NEndpoints>(endpoint_config, descriptors),
    dwc_endpoint_config(dwc_endpoint_config),
    rxfifo_size(rxfifo_size),
    // TODO F7 total_fifo_size(core == Core::FS ? USB_OTG_FS_TOTAL_FIFO_SIZE : USB_OTG_HS_TOTAL_FIFO_SIZE),
    total_fifo_size(1280),
    max_ep_n(0),
    fifo_end(0)
{
    static_assert(CoreAddr == USB_OTG_FS_PERIPH_BASE || CoreAddr == USB_OTG_HS_PERIPH_BASE, "!");
    // TODO static_assert() NEndpoints and NEndpoints too
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init()
{
    init_clocks();
    init_gpio();
    init_usb();
    init_interrupts();
    init_nvic();

    // SDIS is set by default on F7
    USB_DEV->DCTL &= ~USB_OTG_DCTL_SDIS;
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::set_address(uint16_t address)
{
    uint32_t dcfg = USB_DEV->DCFG;
    dcfg &= ~(0x7F << USB_OTG_DCFG_DAD_Pos);
    dcfg |= (address & 0x7F) << USB_OTG_DCFG_DAD_Pos;
    USB_DEV->DCFG = dcfg;

    state = State::ADDRESS;
}


// ref: RM0090 rev. 13 page 1365, "IN data transfers"
template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::submit(uint8_t ep_n, TxTransfer& transfer)
{
    // TODO check if transfer is already in progress?

    in_transfers[ep_n] = &transfer;

    // TODO data types
    uint32_t const n_packets = div_round_up(
        transfer.get_remaining(),
        (size_t)get_ep_config(ep_n, InOut::In).max_pkt_size
    );
    uint32_t const chunk_size = std::min(
        transfer.get_remaining(),
        (size_t)get_dwc_ep_config(ep_n).tx_fifo_size
    );

    d_info("start_IN_transfer(): ep %s, bytes %s, npkt %s, first chunk %s\n",
          ep_n, transfer.get_remaining(), n_packets, chunk_size);

    // program packet count and transfer size in bytes
    USB_INEP(ep_n)->DIEPTSIZ =
        (n_packets << USB_OTG_DIEPTSIZ_PKTCNT_Pos) |
        transfer.get_remaining();

    // enable endpoint
    USB_INEP(ep_n)->DIEPCTL |=
        USB_OTG_DIEPCTL_EPENA | // activate transfer
        USB_OTG_DIEPCTL_CNAK;   // clear NAK bit

    // TODO what about aliasing?
    for (uint32_t word_idx = 0; word_idx < (chunk_size + 3) / 4; ++word_idx) {
        *USB_FIFO(ep_n) = *((uint32_t *)&transfer.get_data_ptr()[word_idx * 4]);
    }

    transfer.on_transferred(chunk_size);

    // TODO race condition?
    if (transfer.get_remaining() > 0) {
        USB_DEV->DIEPEMPMSK |= 1 << ep_n;
        d_verbose("start_IN_transfer(): en intr, rem %s\n", transfer.get_remaining());
    }
}


// ref: RM0090 rev. 13 page 1356
template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::submit(uint8_t ep_n, RxTransfer& transfer)
{
    // TODO check if transfer is already in progress?
    // interrupt is already enabled (RXFLVL)

    out_transfers[ep_n] = &transfer;

    uint32_t const n_packets = div_round_up(
        transfer.get_remaining(),
        (size_t)get_ep_config(ep_n, InOut::Out).max_pkt_size
    );

    USB_OUTEP(ep_n)->DOEPTSIZ =
        (n_packets << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |
        transfer.get_remaining();  // TODO must be extended to next word boundary!

    USB_OUTEP(ep_n)->DOEPCTL |=
        USB_OTG_DOEPCTL_EPENA | // activate transfer
        USB_OTG_DOEPCTL_CNAK;   // clear NAK bit
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::transmit_zlp(uint8_t ep_n)
{
    d_info("transmit_zlp(%s)\n", ep_n);

    // packet count: 1, transfer size: 0 bytes
    USB_INEP(ep_n)->DIEPTSIZ =
        (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos) |
        0;  // size

    // enable endpoint
    USB_INEP(ep_n)->DIEPCTL |=
        USB_OTG_DIEPCTL_EPENA | // activate transfer
        USB_OTG_DIEPCTL_CNAK;   // clear NAK bit
}


// TODO support STALL for OUT EPs
// TODO support unSTALL bulk/interrupt EPs
template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::stall(uint8_t ep)
{
    // control endpoints: the core clears STALL bit when a SETUP token is received
    // bulk and interrupt endpoints: core never clears this bit
    USB_INEP(ep)->DIEPCTL = USB_OTG_DIEPCTL_STALL;
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::ep0_receive_zlp()
{
    d_info("ep0_receive_zlp()\n");

    USB_OUTEP(0)->DOEPTSIZ =
        (0 << USB_OTG_DOEPTSIZ_STUPCNT_Pos) |  // no SETUP packets
        (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |   // rx 1 packet
        // TODO ep0 transfer size, currently = max pkt size
        // TODO zero?
        get_ep_config(0, InOut::Out).max_pkt_size;

    USB_OUTEP(0)->DOEPCTL |=
        USB_OTG_DOEPCTL_EPENA  |  // enable endpoint
        USB_OTG_DOEPCTL_CNAK;     // clear NAK bit
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::ep0_init_ctrl_transfer()
{
    d_info("ep0_init_ctrl_transfer()\n");

    USB_OUTEP(0)->DOEPTSIZ =
        (3 << USB_OTG_DOEPTSIZ_STUPCNT_Pos) |  // rx 3 SETUP packets
        (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |   // rx 1 packet
        // TODO ep0 transfer size, currently = max pkt size
        // TODO depends on speed?
        get_ep_config(0, InOut::Out).max_pkt_size; // note: must be extended to next word boundary

    USB_OUTEP(0)->DOEPCTL |=
        USB_OTG_DOEPCTL_EPENA  |  // enable endpoint
        USB_OTG_DOEPCTL_CNAK;     // clear NAK bit
}


//
// private
//

template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init_clocks()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init_gpio()
{
    // USB_FS F4 and F7 pins:
    //   PA9: USB_FS_VBUS (controlled by USB_OTG_GCCFG_NOVBUSSENS)
    //   PA11: USB_FS_DM (AF)
    //   PA12: USB_FS_DP (AF)

    GPIO_InitTypeDef GPIO_InitStruct = {};
    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;  // TODO GPIO_SPEED_FREQ_VERY_HIGH
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init_nvic()
{
    NVIC_SetPriority(OTG_FS_IRQn, 2);
    NVIC_EnableIRQ(OTG_FS_IRQn);
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init_usb()
{
    d_info("init_usb()\n");

    // select PHY
    // (always 1 for FS)
    USB_CORE->GUSBCFG  |= USB_OTG_GUSBCFG_PHYSEL;

    // wait for AHB idle
    while (!(USB_CORE->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL)) ;;

    // soft reset core
    USB_CORE->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;
    while (USB_CORE->GRSTCTL & USB_OTG_GRSTCTL_CSRST);

    // wait for AHB idle
    while (!(USB_CORE->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL)) ;;

    USB_CORE->GCCFG |= USB_OTG_GCCFG_PWRDWN;  // Power down deactivated (“Transceiver active”)

    // USB configuration
    USB_CORE->GUSBCFG = (USB_CORE->GUSBCFG & ~(0xf << USB_OTG_GUSBCFG_TRDT_Pos)) |
        USB_OTG_GUSBCFG_FDMOD |            // force device mode
        (6 << USB_OTG_GUSBCFG_TRDT_Pos);  // TODO TRDT value?
        // SRP off
        // HNP off
        // TODO FS timeout calibration?

    // note: PA9/PB13 is only free to be used as GPIO when NOVBUSSENS is set

    constexpr bool vbus_sensing = false;  // TODO parameter

    if (vbus_sensing) {
#ifdef USB_OTG_GCCFG_VBDEN
        USB_CORE->GCCFG |= USB_OTG_GCCFG_VBDEN;
#else
        USB_CORE->GCCFG |= USB_OTG_GCCFG_VBUSBSEN;  // Enable Vbus sensing for “B” device
#endif
    } else {
#ifdef USB_OTG_GCCFG_VBDEN
        USB_CORE->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

        // B-session valid override
        USB_CORE->GOTGCTL |=
            USB_OTG_GOTGCTL_BVALOEN |
            USB_OTG_GOTGCTL_BVALOVAL;
#else
        USB_CORE->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;  // Disable Vbus sensing
#endif
    }

    // restart the PHY clock
    *((volatile uint32_t *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_PCGCCTL_BASE)) = 0;

    USB_DEV->DCFG |= USB_OTG_DCFG_DSPD;  // TODO ???

    // obtain max endpoint number
    max_ep_n = (*GHWCFG2 >> 10) & 0xf;
    d_info("GHWCFG2: %#10x, GHWCFG3: %#10x, max_ep_n: %d\n", *GHWCFG2, *GHWCFG3, max_ep_n);
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init_interrupts()
{
    // TODO reset any pending interrupta
    //USB_CORE->GINTSTS = 0xffffffff;  // TODO OR only necessary bits

    USB_CORE->GINTMSK =
        USB_OTG_GINTMSK_OTGINT   | // OTG interrupt TODO only when vbus_sense == true
        USB_OTG_GINTMSK_USBRST   | // USB reset
        USB_OTG_GINTMSK_ENUMDNEM | // speed enumeration done
        USB_OTG_GINTMSK_RXFLVLM  | // RX FIFO not empty
        USB_OTG_GINTMSK_IEPINT   | // IN endpoint interrupt
        USB_OTG_GINTMSK_OEPINT   | // OUT endpoint interrupts
      //USB_OTG_GINTMSK_SOFM     | // start of frame
        USB_OTG_GINTMSK_USBSUSPM | // suspend
        USB_OTG_GINTMSK_WUIM     | // resume
        USB_OTG_GINTMSK_SRQIM    | // session request TODO only when vbus_sense == true
        USB_OTG_GINTMSK_MMISM;     // mode mismatch

    USB_DEV->DIEPMSK =             // for all IN endpoints
        USB_OTG_DIEPMSK_XFRCM;     // transfer complete
    // TXFE (IN endpoint FIFO empty) is masked by DIEPEMPMSK

    USB_DEV->DOEPMSK =             // for all OUT endpoints
        USB_OTG_DOEPMSK_STUPM |    // setup stage complete
        USB_OTG_DOEPMSK_XFRCM;     // transfer complete

    // Note: PTXFELVL bit is not accessible in device mode
    USB_CORE->GAHBCFG |=
        USB_OTG_GAHBCFG_GINT |    // global interrupt enable
        USB_OTG_GAHBCFG_TXFELVL;  // TX interrupt when IN EP TxFIFO: 1=empty, 0=half-empty
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init_ep0()
{
    // allocate RxFIFO (for all endpoints)
    // value in words, min 16, max 256
    USB_CORE->GRXFSIZ = rxfifo_size >> 2;
    fifo_end = rxfifo_size >> 2;

    // allocate txFIFO for EP0
    USB_CORE->DIEPTXF0_HNPTXFSIZ =
        ((get_dwc_ep_config(0).tx_fifo_size >> 2) << 16) |  // EP0 TxFIFO size, in 32-bit words
        fifo_end;
    fifo_end += get_dwc_ep_config(0).tx_fifo_size >> 2;

    // configure but do not activate IN

    USB_INEP(0)->DIEPCTL =
        USB_OTG_DIEPCTL_USBAEP |              // EP0 is always active
        USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
        (0 << USB_OTG_DIEPCTL_TXFNUM_Pos) |   // use TxFIFO 0
        (0 << USB_OTG_DIEPCTL_MPSIZ_Pos) |    // max packet size, 0 = 64 bytes
        USB_OTG_DIEPCTL_SNAK;

    // configure and activate OUT to prepare for SETUP packets

    USB_OUTEP(0)->DOEPTSIZ =
        (3 << USB_OTG_DOEPTSIZ_STUPCNT_Pos) |  // rx 1 SETUP packet
        (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |   // rx 1 packet
        // TODO ep0 transfer size, currently = max pkt size
        // TODO depends on speed?
        get_ep_config(0, InOut::Out).max_pkt_size;   // note: must be extended to next word boundary

    USB_OUTEP(0)->DOEPCTL =
        USB_OTG_DOEPCTL_USBAEP |              // EP0 is always active
        USB_OTG_DOEPCTL_EPENA |
        (0 << USB_OTG_DOEPCTL_MPSIZ_Pos) |    // max packet size, 0 = 64 bytes
        USB_OTG_DOEPCTL_CNAK;

    // enable interrupts for EP0 IN and OUT
    USB_DEV->DAINTMSK |=
        (1 << (0 + USB_OTG_DAINTMSK_IEPM_Pos)) |
        (1 << (0 + USB_OTG_DAINTMSK_OEPM_Pos));
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init_endpoints(uint8_t configuration)
{
    for (auto ep_conf = &endpoint_config[0]; ; ++ep_conf) {
        if (ep_conf->n > 15 ) {
            break;
        }

        if (ep_conf->n == 0) {
            continue;
        }

        if (ep_conf->in_out == InOut::In) {
            init_in_endpoint(*ep_conf);
        } else {
            init_out_endpoint(*ep_conf);
        }
    }
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::deinit_endpoints()
{
    // TODO
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init_in_endpoint(EndpointConfig const& ep_conf)
{
    // IN EP transmits data from device to host.
    // Has its own TxFIFO.

    // TODO check ep number
    // TODO check size
    // TODO check for FIFO memory overflow

    // allocate TxFIFO
    auto const tx_fifo_size_words = get_dwc_ep_config(ep_conf.n).tx_fifo_size >> 2;
    USB_CORE->DIEPTXF[ep_conf.n - 1] =
        (tx_fifo_size_words << USB_OTG_DIEPTXF_INEPTXFD_Pos) |  // TxFIFO size, in 32-bit words
        fifo_end;
    fifo_end += tx_fifo_size_words;

    USB_INEP(ep_conf.n)->DIEPCTL =
        USB_OTG_DIEPCTL_USBAEP |               // activate EP
        (static_cast<uint32_t>(ep_conf.type) << USB_OTG_DIEPCTL_EPTYP_Pos) |
        // use TxFIFO with the same number as the EP
        (ep_conf.n << USB_OTG_DIEPCTL_TXFNUM_Pos) |
        (ep_conf.max_pkt_size << USB_OTG_DIEPCTL_MPSIZ_Pos) |
        USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
        USB_OTG_DIEPCTL_SNAK;

    // enable EP-specific interrupt
    USB_DEV->DAINTMSK |=
        (1 << (ep_conf.n + USB_OTG_DAINTMSK_IEPM_Pos));

    d_info("init_in_endpoint(): ep %s, type %s, inout %s, maxpkt %s, txfifo size %s, fifo end %s\n",
        ep_conf.n,
        static_cast<uint32_t>(ep_conf.type),
        static_cast<uint32_t>(ep_conf.in_out),
        ep_conf.max_pkt_size,
        get_dwc_ep_config(ep_conf.n).tx_fifo_size,
        fifo_end * 4);
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::init_out_endpoint(EndpointConfig const& ep_conf)
{
    // TODO test transfers with EPENA=0!

    // TODO check EPType values against EPTYP
    USB_OUTEP(ep_conf.n)->DOEPCTL =
        USB_OTG_DOEPCTL_USBAEP |              // activate EP
        (static_cast<uint32_t>(ep_conf.type) << USB_OTG_DOEPCTL_EPTYP_Pos) |
        (ep_conf.max_pkt_size << USB_OTG_DIEPCTL_MPSIZ_Pos) |
        USB_OTG_DOEPCTL_CNAK;

    // enable EP-specific interrupt
    USB_DEV->DAINTMSK |=
        (1 << (ep_conf.n + USB_OTG_DAINTMSK_OEPM_Pos));

    d_info("init_out_endpoint(): ep %s\n", ep_conf.n);
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::isr()
{
    uint32_t gintsts = USB_CORE->GINTSTS;

//    d_info("\n isr() gintsts=%#10x gintmsk=%#10x masked=%#10x\n",
//          gintsts, USB_CORE->GINTMSK, gintsts & USB_CORE->GINTMSK);

    d_info("\nisr() masked=%#10x - ", gintsts & USB_CORE->GINTMSK);

    if ((gintsts & USB_CORE->GINTMSK) == 0) {
        d_info("???\n");
        return;
    }

    // must be handled before RXFLVL because the RXFLVL handler
    // affects generation of OUT EP interrupts
    if (gintsts & USB_OTG_GINTSTS_OEPINT) {
        d_info("OEPINT\n");
        isr_out_ep_interrupt();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_IEPINT) {
        d_info("IEPINT\n");
        isr_in_ep_interrupt();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_RXFLVL) {
        d_info("RXFLVL\n");
        isr_read_rxfifo();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_USBRST) {
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_USBRST;  // rc_w1
        d_info("USBRST\n");

        USB_CORE->GINTMSK |= USB_OTG_GINTMSK_USBSUSPM;  // reenable suspend interrupt
        isr_usb_reset();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_ENUMDNE) {
        d_info("ENUMDNE\n");
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_ENUMDNE;  // rc_w1
        isr_speed_complete();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_USBSUSP) {
        // no activity on the bus for 3ms
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_USBSUSP;  // rc_w1

        if (USB_DEV->DSTS & USB_OTG_DSTS_SUSPSTS) {  // if actual suspend
            d_info("USBSUSP\n");
            USB_CORE->GINTMSK &= ~USB_OTG_GINTMSK_USBSUSPM;  // disable suspend interrupt
            CALL_HANDLERS(on_suspend);
        }

        return;
    }

    if (gintsts & USB_OTG_GINTSTS_WKUINT) {
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_WKUINT;  // rc_w1
        d_info("WKUINT\n");

        USB_CORE->GINTMSK |= USB_OTG_GINTMSK_USBSUSPM;  // reenable suspend interrupt
        CALL_HANDLERS(on_resume);
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_SRQINT) {
        d_info("SRQINT\n");  // cable connect
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_SRQINT; // rc_w1
        CALL_HANDLERS(on_connect);
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_OTGINT) {
        uint32_t const gotgint = USB_CORE->GOTGINT;
        USB_CORE->GOTGINT = 0xFFFFFFFF;

        if (gotgint & USB_OTG_GOTGINT_SEDET) {
            d_info("SEDET\n");  // session end (cable disconnect)
            CALL_HANDLERS(on_disconnect);
        }

        return;
    }

    if (gintsts & USB_OTG_GINTSTS_MMIS) {
        d_critical("!!MMIS!! halt\n");
        while (1) ;;
    }

    d_critical("unhandled\n");
    while (1) ;;
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::isr_usb_reset()
{
    // this can be triggered by cable disconnect or with no cable attached!

    // https://github.com/osrf/wandrr/blob/master/firmware/foot/common/usb.c#L359-L406
    // http://cgit.jvnv.net/laks/tree/usb/USB_otg.h?id=4100075#n162

    // USB device configuration
    USB_DEV->DCFG |=
        USB_OTG_DCFG_DSPD;  // force full speed
        // TODO NZLSOHSK?

    // set NAK on all OUT EPs
//    for (uint8_t n = 0; n <= max_ep_n; ++n) {
//        USB_INEP(n)->DIEPCTL =
//            USB_OTG_DIEPCTL_EPDIS |
//            USB_OTG_DIEPCTL_SNAK;

//        USB_OUTEP(n)->DOEPCTL =
//            USB_OTG_DOEPCTL_EPDIS |
//            USB_OTG_DOEPCTL_SNAK;
//    }

    state = State::INITIALIZED;
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::isr_speed_complete()
{
    // end of reset
    // http://cgit.jvnv.net/laks/tree/usb/USB_otg.h?id=4100075#n183

    // 0 = high speed, 3 = full speed
    uint8_t speed = (USB_DEV->DSTS & USB_OTG_DSTS_ENUMSPD) >> USB_OTG_DSTS_ENUMSPD_Pos;

    state = State::SPEED;
    current_configuration = 0;
    fifo_end = 0;

    init_ep0();

    CALL_HANDLERS(on_reset);

    //d_info("DCTL %#10x DSTS %#10x speed %s\n", USB_DEV->DCTL, USB_DEV->DSTS, speed);
    //d_info("DOEPCTL0 %#10x\n", USB_OUTEP(0)->DOEPCTL);

    //d_info("GOTGCTL %#10x GOTGINT %#10x\n", USB_CORE->GOTGCTL, USB_CORE->GOTGINT);
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::isr_read_rxfifo()
{
    // http://cgit.jvnv.net/laks/tree/usb/USB_otg.h?id=4100075#n20
    // https://github.com/osrf/wandrr/blob/master/firmware/foot/common/usb.c#L417-L482

    enum class PacketStatus {
        GlobalOutNAK = 1,      // the global OUT NAK bit has taken effect
        OutPacket = 2,         // A DATA packet was received
        OutComplete = 3,       // An OUT data transfer for the specified OUT endpoint has completed.
                               // Popping this asserts TX Complete interrupt on the OUT EP and
                               // clears EPENA
        SetupComplete = 4,     // The Setup stage for the specified endpoint has completed.
                               // Popping this asserts Setup interrupt on the OUT EP
        SetupPacket = 6        // A SETUP packet was received; len=8, DPID=D0
    };

    static char const* pktsts_names[] = {
        "?", "GNAK", "OutPkt", "OutComp", "SetupComp", "?", "SetupPkt"
    };

    // read the receive status pop register
    uint32_t const status = USB_CORE->GRXSTSP;
    uint8_t  const ep_n = status & USB_OTG_GRXSTSP_EPNUM;
    uint32_t const len = (status & USB_OTG_GRXSTSP_BCNT) >> USB_OTG_GRXSTSP_BCNT_Pos;
    PacketStatus const packet_status = static_cast<PacketStatus>(
        (status & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos);

    d_info("isr_read_rxfifo() pktsts=%s ep=%s len=%s\n",
        pktsts_names[static_cast<uint32_t>(packet_status)], ep_n, len);

    if (packet_status == PacketStatus::SetupPacket) {
        uint8_t const stupcnt =
            (USB_OUTEP(0)->DOEPTSIZ >> USB_OTG_DOEPTSIZ_STUPCNT_Pos) & 3;
        //uint8_t const n_setup_pkts = 3 - stupcnt;

        if (stupcnt != 2 || len != 8) {
            d_warn("--> STUPCNT %s, len %s\n", stupcnt, len);
        }

        auto buf = ctrl_ep_dispatcher.get_setup_pkt_buffer();
        read_packet(buf, len);

        return;
    }

    if (packet_status == PacketStatus::OutPacket) {
        auto transfer = out_transfers[ep_n];
        if (transfer == nullptr) {
            d_assert("!!! EP %s is null\n"); // TODO, ep_n);
        }

        auto buffer = transfer->get_buffer(len);

        if (buffer != nullptr) {
            read_packet(buffer, len);
            transfer->on_filled(buffer, len);
        } // else what? need to discard pkt
    }

#if 0
    // TODO tempo
    bool const rxflvl = USB_CORE->GINTSTS & USB_OTG_GINTSTS_RXFLVL;
    d_critical("  RXFLVL: %s", !!rxflvl);
    if (rxflvl) {
        uint32_t const status = USB_CORE->GRXSTSR;  // peek
        uint8_t  const ep_n = status & USB_OTG_GRXSTSP_EPNUM;
        uint32_t const len = (status & USB_OTG_GRXSTSP_BCNT) >> USB_OTG_GRXSTSP_BCNT_Pos;
        PacketStatus const packet_status = static_cast<PacketStatus>(
            (status & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos);

        d_critical("; GRXSTSR: pktsts=%s ep=%s len=%s",
            pktsts_names[static_cast<uint32_t>(packet_status)], ep_n, len);
    }
    d_critical("\n");
#endif
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::isr_out_ep_interrupt()
{
    uint16_t out_ep_interrupt_flags =
        (USB_DEV->DAINT & USB_OTG_DAINT_OEPINT) >> USB_OTG_DAINT_OEPINT_Pos;
    uint8_t ep_n = 0;

    // TODO assert in_ep_flags != 0
    if (out_ep_interrupt_flags == 0) {
        return;
    }

    while ((out_ep_interrupt_flags & 1) == 0) {
        out_ep_interrupt_flags >>= 1;
        ++ep_n;
    }

    if (USB_OUTEP(ep_n)->DOEPINT & USB_OTG_DOEPINT_STUP) {
        d_info("OUT %s STUP\n", ep_n);
        USB_OUTEP(ep_n)->DOEPINT = USB_OTG_DOEPINT_STUP; // clear interrupt
        ctrl_ep_dispatcher.on_setup_stage(ep_n);
    }

    if (USB_OUTEP(ep_n)->DOEPINT & USB_OTG_DOEPINT_XFRC) {
        if (out_transfers[ep_n] == nullptr) {
            d_assert("!!! EP %s is null, halt\n"); // TODO , ep_n);
        }

        d_info("OUT %s XFRC\n", ep_n);
        USB_OUTEP(ep_n)->DOEPINT = USB_OTG_DOEPINT_XFRC; // clear interrupt
        // TODO docs say read DOEPTSIZx to determine size of payload,
        // it could be less than expected
        dispatch_out_transfer_complete(ep_n);
    }
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::isr_in_ep_interrupt()
{
    uint16_t in_ep_interrupt_flags =
        (USB_DEV->DAINT & USB_OTG_DAINT_IEPINT) >> USB_OTG_DAINT_IEPINT_Pos;
    uint8_t ep_n = 0;

    // TODO assert in_ep_flags != 0
    if (in_ep_interrupt_flags == 0) {
        return;
    }

    while ((in_ep_interrupt_flags & 1) == 0) {
        in_ep_interrupt_flags >>= 1;
        ++ep_n;
    }

    if (in_transfers[ep_n] == nullptr) {
        d_assert("!!! EP %s is null, halt\n"); // TODO , ep_n);
    }
    auto transfer = in_transfers[ep_n];

    // Tx FIFO empty
    if ((USB_INEP(ep_n)->DIEPINT & USB_OTG_DIEPINT_TXFE) &&
        (USB_DEV->DIEPEMPMSK & (1 << ep_n))
    ) {
        uint16_t const avail_words = USB_INEP(ep_n)->DTXFSTS;
        d_info("IN %s TXFE avail %s\n", ep_n, avail_words * 4); //, DIEPTSIZ %#10x USB_INEP(ep_n)->DIEPTSIZ);

        if (transfer->get_remaining() > 0) {
            uint16_t const chunk_size = std::min(
                transfer->get_remaining(), (size_t)(avail_words * 4));

            // TODO what about aliasing?
            for (uint32_t word_idx = 0; word_idx < (chunk_size + 3) / 4; word_idx++) {
                *USB_FIFO(ep_n) = *((uint32_t *)&transfer->get_data_ptr()[word_idx * 4]);
            }

            transfer->on_transferred(chunk_size);

            d_verbose("  TXFE: pushed %s, rem %s\n", chunk_size, ep->get_remaining());
        }

        if (transfer->get_remaining() == 0) {
            // disable TXFE interrupt for this EP
            USB_DEV->DIEPEMPMSK &= ~(1 << ep_n);
            d_verbose("  dis int\n");
        }
    }

    // transfer completed
    if (USB_INEP(ep_n)->DIEPINT & USB_OTG_DIEPINT_XFRC) {
        d_info("IN %s XFRC\n", ep_n);
        USB_INEP(ep_n)->DIEPINT = USB_OTG_DIEPINT_XFRC;
        dispatch_in_transfer_complete(ep_n);
    }

//    d_info("isr_in_ep_interrupt(): unknown, ep=%s DIEPINT=%#10x\n",
//        ep, USB_INEP(ep)->DIEPINT);
//    while (true) ;;
}


template <size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
void DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::read_packet(unsigned char* dest_buf, uint16_t n_bytes)
{
    uint32_t fifo_tmp;
    unsigned char const *fifo_buf = reinterpret_cast<unsigned char const*>(&fifo_tmp);

    for (int i = 0; i < n_bytes; ++i) {
        if (i % 4 == 0) {
            fifo_tmp = *USB_FIFO(0);
        }

        dest_buf[i] = fifo_buf[i % 4];
    }
}


template<size_t NHandlers, size_t NEndpoints, size_t CoreAddr>
DWCEndpointConfig const& DWC_OTG_Device<NHandlers, NEndpoints, CoreAddr>::get_dwc_ep_config(uint8_t ep_n)
{
    for (size_t i = 0; ; ++i) {
        if (dwc_endpoint_config[i].n > 15) {
            d_assert("no such endpoint in dwc_endpoint_config");  // TODO ep_n
        }

        if (dwc_endpoint_config[i].n == ep_n) {
            return dwc_endpoint_config[i];
        }
    }
}

#endif // DWC_OTG_DEVICE_IMPL_H
