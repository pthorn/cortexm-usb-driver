API available in callbacks:
  - get_ep(n)
    - ep->init_transfer(TODO); (TODO or stream per EP?)
  - get_setup_pkt()
  - return DATA_STAGE / NO_DATA_STAGE / STALL / UNHANDLED;

Endpoint has multiple functions:
    - carry config data (#, type, in/out, max pkt size, fifo size)
    - provide stream
    - for EP0, implement state machine

TODO:
  - avoid having to call both transmit_zlp() AND start_*_transfer() here
    by making lower level do it based on return code?
  + make device frontend superclass and a dwc_otg subclass
  + make all handlers available to all EPs by moving the handler array into device
  + remember handler that handled the status stage, only call it for data and status?
  + make all EPs available to each handler (device->get_ep(i))
  - do not allow IN xfers on OUT EPs and vice versa!
  - implement transfers of unknown size (rx already works if given large # of bytes?
    see docs about adding zlp to tx)
  - why device doesn't enumerate on PC boot?
  - test behavior when EPENA=0 and other flag combinations
  - test OUT packets 60...64 bytes long (assuming max pkt size = 64)
  + add string descriptors
  - WCID descriptors for auto WinUSB installation
    http://stackoverflow.com/questions/17666006/support-winusb-in-device-firmware
    https://github.com/pbatard/libwdi/wiki/WCID-Devices
  - proper configs for both FS and HS
  - reset STUPCNT after each control transfer?
  + what is Vbus sensing? is it present on 407?
  + implement callbacks for user (class drivers)
  + move part of init stuff to usb reset handler as per docs?
  - deinit on USBSUSP?
  - interrupt driven transmit (>1 packets at once)
  - more intelligent FIFO allocation
  - what usb core is in 072 - same as F1 but FIFO is 16-bit
  - NXP USB core
  - Kinetis USB core?
  - research host / OTG
  - research SRP, HNP
  - parametrize RxFIFO size, time params
  - packet telemetry


*
ControlEndpoint {
  ControlEndpoint(Driver&);
  //SetupPacket& get_setup_buf();
  //void on_setup_stage(SetupPacket& packet);
 *
  uint8_t get_max_pkt_size();
  accept_setup_packet();
  accept_data_packet();
 *
  void on_out_tx();  // setup and data available, need to send status
  void on_out_tx_early();  // only setup stage is complete
  void on_in_tx();   // setup available, need to send data TODO handle status
 *
   SetupPacket setup_packet;
  enum class Stage { Control, Data, Status };
}
 *
InEndpoint {
  void on_in_tx();
}
 *
pnputil -e > list.txt; search by vid/pid; pnputil -d oemXX.inf


* bulk and control: can be multipacket
* control pkt size: 8, 16, 32 or 64 in FS, 512 in HS?
* bulk pkt size: 64 in FS, 512 in HS?
* interrupt: single pkt only, any pkt size up to ...?
*
* A bulk transfer is considered complete when it has transferred the exact amount of data requested,
* transferred a packet less than the maximum endpoint size, or transferred a zero-length packet.
*
*   - proper handling of DxEPCTLx and DxEPSIZx:
*       EPENA - schedule transfer, do not touch DIEPTSIZx while it's active
*               set for OUT EPs when initializing but not for IN EPs
*       SNAK/CNAK, PKTCNT, XFRSIZ
*   - DTXFSTSx: # of words available in TxFIFO



* enumeration sequence (windows 8.1, winusb)
*   - reset
*   - get device descriptor, wLength=64
*   - reset
*   - set address
*   - get device descriptor, wLength=sizeof(descriptor)
*   - get config descriptor, wLength=255
*   - get string descriptor, index=3 (serial), langid=0x0409(!), wLength=255
*   - get string descriptor, index=0 (langid), langid=0, wLength=255 <--- ?
*   - get string descriptor, index=2 (product), langid=0x0409, wLength=255
*   - get descriptor type=0x6 (wValue=0x600), stall
*   - reset
*   - get device descriptor, wLength=sizeof(descriptor)
*   - reset
*   - set address
*   - get device descriptor, wLength=sizeof(descriptor)
*   - get device descriptor, wLength=sizeof(descriptor)
*   - get config descriptor, wLength=9 (config only)
*   - get config descriptor, wLength=32 (config and all subordinates)
*   - get status
*   - set configuration


enumeration sequence, osx 10.12.2
  - usb reset
  - set address 5
  - get descriptor: device, wLength=8
  - get descriptor: device, wLength=sizeof()
  - get descriptor: string, index 2, langid 0x0409, wLength=2
  - get descriptor: string, index 2, langid 0x0409, wLength=sizeof()
  - get descriptor: string, index 1, langid 0x0409, wLength=2
  - get descriptor: string, index 1, langid 0x0409, wLength=sizeof()
  - get descriptor: string, index 3, langid 0x0409, wLength=2
  - get descriptor: string, index 3, langid 0x0409, wLength=sizeof()
  - get descriptor: config, wLength=9
  - get descriptor: config, wLength=32
  - get descriptor: string, index 0, langid 0, wLength=255
  - get descriptor: string, index 0, langid 0x0409, wLength=255
  - get descriptor: string, index 0, langid 0, wLength=255
(no set configuration until device is opened by a host app)
