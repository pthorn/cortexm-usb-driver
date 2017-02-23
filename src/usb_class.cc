#include "usb/usb_class.h"


void USBClass::on_connect() { }
void USBClass::on_disconnect() { }

void USBClass::on_reset() { }
void USBClass::on_set_configuration(uint8_t configuration) { }
void USBClass::on_suspend() { }
void USBClass::on_resume() { }

SetupResult USBClass::handle_ctrl_setup_stage() {
    return SetupResult::UNHANDLED;
}

DataResult USBClass::handle_ctrl_in_data_stage() {
    return DataResult::STALL;  // TODO msg
}

DataResult USBClass::handle_ctrl_out_data_stage() {
    return DataResult::STALL;  // TODO msg
}

void USBClass::handle_ctrl_status_stage() { }

void USBClass::handle_in_transfer(Endpoint& ep) { }
void USBClass::handle_out_transfer(Endpoint& ep) { }
