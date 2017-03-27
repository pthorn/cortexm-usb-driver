#include "handler.h"

void Handler::on_connect() { }
void Handler::on_disconnect() { }

void Handler::on_reset() { }
void Handler::on_set_configuration(uint8_t configuration) { }
void Handler::on_suspend() { }
void Handler::on_resume() { }

SetupResult Handler::on_ctrl_setup_stage() {
    return SetupResult::UNHANDLED;
}

void Handler::handle_ctrl_status_stage() { }
