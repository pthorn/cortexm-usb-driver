#include <algorithm>

#include "dwc_otg_header.h"
#include "usb/dwc_otg_device.h"
#include "device_pvt.h"
#include "usb/defs.h"
#include "usb/endpoint.h"
#include "usb/control_endpoint.h"
#include "print.h"
//#include "gpio.h" TODO

// TODO try putting CORE_BASE into a template arg?
#define CORE_BASE USB_OTG_FS_PERIPH_BASE
//#define CORE_BASE base_addr
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
    return (a + (b - 1)) / b;
}


static void read_packet(unsigned char* dest_buf, uint16_t n_bytes)
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


// TODO params: core (FS or HS), timing, Vbus sensing
DWC_OTG_Device::DWC_OTG_Device(
    Core core,
    ControlEndpoint& endpoint_0,
    uint16_t rxfifo_size//,
    //bool vbus_sensing
):
    Device(endpoint_0),
    rxfifo_size(rxfifo_size),
    base_addr(core == Core::FS ? USB_OTG_FS_PERIPH_BASE : USB_OTG_HS_PERIPH_BASE),
    total_fifo_size(core == Core::FS ? USB_OTG_FS_TOTAL_FIFO_SIZE : USB_OTG_HS_TOTAL_FIFO_SIZE),
    max_ep_n(0),
    fifo_end(0)
{
    //add_ep(endpoint_0);
}


void DWC_OTG_Device::init()
{
    init_clocks();
    init_gpio();
    init_usb();
    init_interrupts();
    init_nvic();

    // SDIS is set by default on F7
    USB_DEV->DCTL &= ~USB_OTG_DCTL_SDIS;
}


void DWC_OTG_Device::set_address(uint16_t address)
{
    uint32_t dcfg = USB_DEV->DCFG;
    dcfg &= ~(0x7F << USB_OTG_DCFG_DAD_Pos);
    dcfg |= (address & 0x7F) << USB_OTG_DCFG_DAD_Pos;
    USB_DEV->DCFG = dcfg;

    state = State::ADDRESS;
}


bool DWC_OTG_Device::set_configuration(uint8_t configuration)
{
    if (current_configuration == configuration) {
        return true;
    }

    if (configuration == 0) {
        // TODO deinit EPs?
        current_configuration = configuration;
        state = State::ADDRESS;
        return true;
    } else if (configuration == 1) {
        init_endpoints(configuration);  // TODO multiple configurations? windows doesnt't support that?

        current_configuration = configuration;
        state = State::CONFIGURED;

        CALL_HANDLERS(on_set_configuration, configuration);

        //Device::set_configuration(configuration);

        return true;
    } else {
        return false;
    }
}


// to be called from InEndpoint only
// ref: RM0090 rev. 13 page 1365, "IN data transfers"
void DWC_OTG_Device::start_in_transfer(Endpoint* ep)
{
    // TODO check if transfer is already in progress?

    // TODO data types
    uint32_t const n_packets = div_round_up(
        ep->get_remaining(), (size_t)ep->get_max_pkt_size());
    uint32_t const chunk_size = std::min(
        ep->get_remaining(), (size_t)ep->get_tx_fifo_size());

    print("start_IN_transfer(): ep %s, bytes %s, npkt %s, first chunk %s\n",
          ep->get_number(), ep->get_remaining(), n_packets, chunk_size);

    // program packet count and transfer size in bytes
    USB_INEP(ep->get_number())->DIEPTSIZ =
        (n_packets << USB_OTG_DIEPTSIZ_PKTCNT_Pos) |
        ep->get_remaining();

    // enable endpoint
    USB_INEP(ep->get_number())->DIEPCTL |=
        USB_OTG_DIEPCTL_EPENA | // activate transfer
        USB_OTG_DIEPCTL_CNAK;   // clear NAK bit

    // TODO what about aliasing?
    for (uint32_t word_idx = 0; word_idx < (chunk_size + 3) / 4; ++word_idx) {
        *USB_FIFO(ep->get_number()) = *((uint32_t *)&ep->get_data_ptr()[word_idx * 4]);
    }

    ep->on_transferred(chunk_size);

    // TODO race condition?
    if (ep->get_remaining() > 0) {
        USB_DEV->DIEPEMPMSK |= 1 << ep->get_number();
        print("start_IN_transfer(): en intr, rem %s\n", ep->get_remaining());
    }
}


// to be called from OutEndpoint only
// ref: RM0090 rev. 13 page 1356
void DWC_OTG_Device::start_out_transfer(Endpoint* ep)
{
    // TODO check if transfer is already in progress?
    // interrupt is already enabled (RXFLVL)

    uint32_t const n_packets = div_round_up(ep->get_remaining(), (size_t)ep->get_max_pkt_size());

    USB_OUTEP(ep->get_number())->DOEPTSIZ =
        (n_packets << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |
        ep->get_remaining();  // TODO must be extended to next word boundary!

    USB_OUTEP(ep->get_number())->DOEPCTL |=
        USB_OTG_DOEPCTL_EPENA | // activate transfer
        USB_OTG_DOEPCTL_CNAK;   // clear NAK bit
}


void DWC_OTG_Device::transmit_zlp(Endpoint *ep)
{
    // packet count: 1, transfer size: 0 bytes
    USB_INEP(ep->get_number())->DIEPTSIZ =
        (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos) |
        0;  // size

    // enable endpoint
    USB_INEP(ep->get_number())->DIEPCTL |=
        USB_OTG_DIEPCTL_EPENA | // activate transfer
        USB_OTG_DIEPCTL_CNAK;   // clear NAK bit
}


// TODO support STALL for OUT EPs
// TODO support unSTALL bulk/interrupt EPs
void DWC_OTG_Device::stall(uint8_t ep)
{
    // control endpoints: the core clears STALL bit when a SETUP token is received
    // bulk and interrupt endpoints: core never clears this bit
    USB_INEP(ep)->DIEPCTL = USB_OTG_DIEPCTL_STALL;
}


void DWC_OTG_Device::ep0_receive_zlp()
{
    print("ep0_receive_zlp()\n");

    USB_OUTEP(0)->DOEPTSIZ =
        (0 << USB_OTG_DOEPTSIZ_STUPCNT_Pos) |  // no SETUP packets
        (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |   // rx 1 packet
        // TODO ep0 transfer size, currently = max pkt size
        // TODO zero?
        endpoint_0.get_max_pkt_size();

    USB_OUTEP(0)->DOEPCTL |=
        USB_OTG_DOEPCTL_EPENA  |  // enable endpoint
        USB_OTG_DOEPCTL_CNAK;     // clear NAK bit
}


void DWC_OTG_Device::ep0_init_ctrl_transfer()
{
    print("ep0_receive_zlp()\n");

    USB_OUTEP(0)->DOEPTSIZ =
        (3 << USB_OTG_DOEPTSIZ_STUPCNT_Pos) |  // rx 3 SETUP packets
        (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |   // rx 1 packet
        // TODO ep0 transfer size, currently = max pkt size
        // TODO depends on speed?
        endpoint_0.get_max_pkt_size();         // note: must be extended to next word boundary

    USB_OUTEP(0)->DOEPCTL |=
        USB_OTG_DOEPCTL_EPENA  |  // enable endpoint
        USB_OTG_DOEPCTL_CNAK;     // clear NAK bit
}


//
// private
//

void DWC_OTG_Device::init_clocks()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
}


void DWC_OTG_Device::init_gpio()
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


void DWC_OTG_Device::init_nvic()
{
    NVIC_SetPriority(OTG_FS_IRQn, 2);
    NVIC_EnableIRQ(OTG_FS_IRQn);
}


void DWC_OTG_Device::init_usb()
{
    print("init_usb()\n");

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
}


void DWC_OTG_Device::init_interrupts()
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


void DWC_OTG_Device::init_ep0()
{
    // allocate RxFIFO (for all endpoints)
    // value in words, min 16, max 256
    USB_CORE->GRXFSIZ = rxfifo_size >> 2;
    fifo_end = rxfifo_size >> 2;

    // allocate txFIFO for EP0
    USB_CORE->DIEPTXF0_HNPTXFSIZ =
        ((endpoint_0.get_tx_fifo_size() >> 2) << 16) |  // EP0 TxFIFO size, in 32-bit words
        fifo_end;
    fifo_end += endpoint_0.get_tx_fifo_size() >> 2;

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
        endpoint_0.get_max_pkt_size();         // note: must be extended to next word boundary

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


void DWC_OTG_Device::init_endpoints(uint8_t configuration)
{
    for (auto ep: endpoints) {
        if (ep == nullptr || ep->get_number() == 0) {
            continue;
        }

        if (ep->get_inout() == InOut::In) {
            init_in_endpoint(ep);
        } else {
            init_out_endpoint(ep);
        }

        ep->on_initialized();
    }
}


void DWC_OTG_Device::init_in_endpoint(Endpoint* ep)
{
    // IN EP transmits data from device to host.
    // Has its own TxFIFO.

    // TODO check ep number
    // TODO check size
    // TODO check for FIFO memory overflow

    // allocate TxFIFO
    auto const tx_fifo_size_words = ep->get_tx_fifo_size() >> 2;
    USB_CORE->DIEPTXF[ep->get_number() - 1] =
        (tx_fifo_size_words << USB_OTG_DIEPTXF_INEPTXFD_Pos) |  // TxFIFO size, in 32-bit words
        fifo_end;
    fifo_end += tx_fifo_size_words;

    USB_INEP(ep->get_number())->DIEPCTL =
        USB_OTG_DIEPCTL_USBAEP |               // activate EP
        (static_cast<uint32_t>(ep->get_type()) << USB_OTG_DIEPCTL_EPTYP_Pos) |
        // use TxFIFO with the same number as the EP
        (ep->get_number() << USB_OTG_DIEPCTL_TXFNUM_Pos) |
        (ep->get_max_pkt_size() << USB_OTG_DIEPCTL_MPSIZ_Pos) |
        USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
        USB_OTG_DIEPCTL_SNAK;

    // enable EP-specific interrupt
    USB_DEV->DAINTMSK |=
        (1 << (ep->get_number() + USB_OTG_DAINTMSK_IEPM_Pos));

    print("init_in_endpoint(): ep %s, type %s, inout %s, maxpkt %s, txfifo size %s, fifo end %s\n",
        ep->get_number(),
        static_cast<uint32_t>(ep->get_type()),
        static_cast<uint32_t>(ep->get_inout()),
        ep->get_max_pkt_size(),
        ep->get_tx_fifo_size(),
        fifo_end * 4);
}


void DWC_OTG_Device::init_out_endpoint(Endpoint* ep)
{
    // TODO test transfers with EPENA=0!

    // TODO check EPType values against EPTYP
    USB_OUTEP(ep->get_number())->DOEPCTL =
        USB_OTG_DOEPCTL_USBAEP |              // activate EP
        (static_cast<uint32_t>(ep->get_type()) << USB_OTG_DOEPCTL_EPTYP_Pos) |
        (ep->get_max_pkt_size() << USB_OTG_DIEPCTL_MPSIZ_Pos) |
        USB_OTG_DOEPCTL_CNAK;

    // enable EP-specific interrupt
    USB_DEV->DAINTMSK |=
        (1 << (ep->get_number() + USB_OTG_DAINTMSK_OEPM_Pos));

    print("init_out_endpoint(): ep %s\n", ep->get_number());
}


void DWC_OTG_Device::isr()
{
    uint32_t gintsts = USB_CORE->GINTSTS;

//    print("\n isr() gintsts=%#10x gintmsk=%#10x masked=%#10x\n",
//          gintsts, USB_CORE->GINTMSK, gintsts & USB_CORE->GINTMSK);

    print("\nisr() masked=%#10x - ", gintsts & USB_CORE->GINTMSK);

    if ((gintsts & USB_CORE->GINTMSK) == 0) {
        print("???\n");
        return;
    }

    // must be handled before RXFLVL because the RXFLVL handler
    // affects generation of OUT EP interrupts
    if (gintsts & USB_OTG_GINTSTS_OEPINT) {
        print("OEPINT\n");
        isr_out_ep_interrupt();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_IEPINT) {
        print("IEPINT\n");
        isr_in_ep_interrupt();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_RXFLVL) {
        print("RXFLVL\n");
        isr_read_rxfifo();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_USBRST) {
        print("USBRST\n");
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_USBRST;  // rc_w1

        USB_CORE->GINTMSK |= USB_OTG_GINTMSK_USBSUSPM;  // reenable suspend interrupt
        isr_usb_reset();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_ENUMDNE) {
        print("ENUMDNE\n");
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_ENUMDNE;  // rc_w1
        isr_speed_complete();
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_USBSUSP) {
        // no activity on the bus for 3ms
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_USBSUSP;  // rc_w1

        if (USB_DEV->DSTS & USB_OTG_DSTS_SUSPSTS) {  // if actual suspend
            print("USBSUSP\n");
            USB_CORE->GINTMSK &= ~USB_OTG_GINTMSK_USBSUSPM;  // disable suspend interrupt
            CALL_HANDLERS(on_suspend);
        }

        return;
    }

    if (gintsts & USB_OTG_GINTSTS_WKUINT) {
        print("WKUINT\n");
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_WKUINT;  // rc_w1

        USB_CORE->GINTMSK |= USB_OTG_GINTMSK_USBSUSPM;  // reenable suspend interrupt
        CALL_HANDLERS(on_resume);
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_SRQINT) {
        print("SRQINT\n");  // cable connect
        USB_CORE->GINTSTS = USB_OTG_GINTSTS_SRQINT; // rc_w1
        CALL_HANDLERS(on_connect);
        return;
    }

    if (gintsts & USB_OTG_GINTSTS_OTGINT) {
        uint32_t const gotgint = USB_CORE->GOTGINT;
        USB_CORE->GOTGINT = 0xFFFFFFFF;

        if (gotgint & USB_OTG_GOTGINT_SEDET) {
            print("SEDET\n");  // session end (cable disconnect)
            CALL_HANDLERS(on_disconnect);
        }

        return;
    }

    if (gintsts & USB_OTG_GINTSTS_MMIS) {
        print("!!MMIS!! halt\n");
        while (1) ;;
    }

    print("unhandled\n");
    while (1) ;;
}


void DWC_OTG_Device::isr_usb_reset()
{
    // this can be triggered by cable disconnect or with no cable attached!

    // https://github.com/osrf/wandrr/blob/master/firmware/foot/common/usb.c#L359-L406
    // http://cgit.jvnv.net/laks/tree/usb/USB_otg.h?id=4100075#n162

    // USB device configuration
    USB_DEV->DCFG |=
        USB_OTG_DCFG_DSPD;  // full speed
        // TODO NZLSOHSK?

    // TODO call handler->handle_reset()? or in isr_speed_complete()?

/* TODO
    otg.dev_oep_reg[0].DOEPCTL = (1 << 27);   SNAK on all OUT EPs
    otg.dev_oep_reg[1].DOEPCTL = (1 << 27);
    otg.dev_oep_reg[2].DOEPCTL = (1 << 27);
    otg.dev_oep_reg[3].DOEPCTL = (1 << 27);
    otg.dev_reg.DAINTMSK = (1 << 16) | 1;     EP0 in and out
    otg.dev_reg.DOEPMSK = (1 << 3) | 1;       STUPM and XFRCM
    otg.dev_reg.DIEPEMPMSK = (1 << 3) | 1;    EP0 and EP3?
*/
}


void DWC_OTG_Device::isr_speed_complete()
{
    // end of reset
    // http://cgit.jvnv.net/laks/tree/usb/USB_otg.h?id=4100075#n183

    // 0 = high speed, 3 = full speed
    uint8_t speed = (USB_DEV->DSTS & USB_OTG_DSTS_ENUMSPD) >> USB_OTG_DSTS_ENUMSPD_Pos;

    fifo_end = 0;

    init_ep0();

    CALL_HANDLERS(on_reset);
}


void DWC_OTG_Device::isr_read_rxfifo()
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

    auto ep = endpoints[ep_n];
    if (ep == nullptr) {
        print("!!! EP %s is null\n", ep_n);
        while (true) ;;
    }

    print("isr_read_rxfifo() pktsts=%s ep=%s len=%s\n",
        pktsts_names[static_cast<uint32_t>(packet_status)], ep_n, len);

    if (packet_status == PacketStatus::SetupPacket) {
        // TODO assert len == 8 for setup packets?

        uint8_t const stupcnt =
            (USB_OUTEP(0)->DOEPTSIZ >> USB_OTG_DOEPTSIZ_STUPCNT_Pos) & 3;

        if (stupcnt != 2) {
            print("--> STUPCNT %s\n", stupcnt);
        }

        auto buf = endpoint_0.get_setup_pkt_buffer();
        read_packet(buf, len);

        return;
    }

    if (packet_status == PacketStatus::OutPacket) {
        auto buffer = ep->get_buffer(len);

        if (buffer != nullptr) {
            read_packet(buffer, len);
            ep->on_filled(buffer, len);
        } // else what? need to discard pkt
    }
}


void DWC_OTG_Device::isr_out_ep_interrupt()
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

    auto ep = endpoints[ep_n];
    if (ep == nullptr) {
        print("!!! EP %s is null, halt\n", ep_n);
        while (true) ;;
    }

    if (USB_OUTEP(ep_n)->DOEPINT & USB_OTG_DOEPINT_STUP) {
        print("OUT %s STUP\n", ep_n);
        USB_OUTEP(ep_n)->DOEPINT = USB_OTG_DOEPINT_STUP; // clear interrupt
        endpoint_0.on_setup_stage();
    }

    if (USB_OUTEP(ep_n)->DOEPINT & USB_OTG_DOEPINT_XFRC) {
        print("OUT %s XFRC\n", ep_n);
        USB_OUTEP(ep_n)->DOEPINT = USB_OTG_DOEPINT_XFRC; // clear interrupt
        // TODO docs say read DOEPTSIZx to determine size of payload,
        // it could be less than expected
        ep->on_out_transfer_complete();
    }
}


void DWC_OTG_Device::isr_in_ep_interrupt()
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

    auto ep = endpoints[ep_n];
    if (ep == nullptr) {
        print("!!! EP %s is null, halt\n", ep_n);
        while (true) ;;
    }

    // Tx FIFO empty
    if ((USB_INEP(ep_n)->DIEPINT & USB_OTG_DIEPINT_TXFE) &&
        (USB_DEV->DIEPEMPMSK & (1 << ep_n))
    ) {
        uint16_t const avail_words = USB_INEP(ep_n)->DTXFSTS;
        print("IN %s TXFE avail %s\n", ep_n, avail_words * 4); //, DIEPTSIZ %#10x USB_INEP(ep_n)->DIEPTSIZ);

        if (ep->get_remaining() > 0) {
            uint16_t const chunk_size = std::min(
                ep->get_remaining(), (size_t)(avail_words * 4));

            // TODO what about aliasing?
            for (uint32_t word_idx = 0; word_idx < (chunk_size + 3) / 4; word_idx++) {
                *USB_FIFO(ep_n) = *((uint32_t *)&ep->get_data_ptr()[word_idx * 4]);
            }

            ep->on_transferred(chunk_size);

            print("  TXFE: pushed %s, rem %s\n", chunk_size, ep->get_remaining());
        }

        if (ep->get_remaining() == 0) {
            // disable TXFE interrupt for this EP
            USB_DEV->DIEPEMPMSK &= ~(1 << ep_n);
            print("  dis int\n");
        }
    }

    // transfer completed
    if (USB_INEP(ep_n)->DIEPINT & USB_OTG_DIEPINT_XFRC) {
        print("IN %s XFRC\n", ep_n);
        USB_INEP(ep_n)->DIEPINT = USB_OTG_DIEPINT_XFRC;
        ep->on_in_transfer_complete();
    }

//    print("isr_in_ep_interrupt(): unknown, ep=%s DIEPINT=%#10x\n",
//        ep, USB_INEP(ep)->DIEPINT);
//    while (true) ;;
}
