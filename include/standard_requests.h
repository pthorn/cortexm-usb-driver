#ifndef STANDARD_REQUESTS_H
#define STANDARD_REQUESTS_H

#include "usb_class.h"
#include "descriptors.h"


class DWC_OTG_Device;
class ControlEndpoint;


class StandardRequests: public USBClass
{
public:
    StandardRequests(
        DeviceDescriptor const& device_descriptor,
        ConfigDescriptor const& config_descriptor,
        StringLangIDDescriptor const& lang_id_descriptor,
        unsigned char const** string_descriptors,
        size_t string_descriptors_len,
        unsigned char const* msft_string_descriptor
    ): device_descriptor(device_descriptor),
       config_descriptor(config_descriptor),
       lang_id_descriptor(lang_id_descriptor),
       string_descriptors(string_descriptors),
       string_descriptors_len(string_descriptors_len),
       msft_string_descriptor(msft_string_descriptor)
    { }

    void on_set_configuration(uint8_t configuration) override;

    SetupResult handle_ctrl_setup_stage() override;
    DataResult handle_ctrl_in_data_stage() override;
    DataResult handle_ctrl_out_data_stage() override;
    void handle_ctrl_status_stage() override;

    void handle_in_transfer(Endpoint &ep) override;
    void handle_out_transfer(Endpoint &ep) override;

private:
    SetupResult send_descriptor();

    DeviceDescriptor const& device_descriptor;
    ConfigDescriptor const& config_descriptor;
    StringLangIDDescriptor const& lang_id_descriptor;
    unsigned char const** string_descriptors;
    size_t string_descriptors_len;
    unsigned char const* msft_string_descriptor;
};

#endif // STANDARD_REQUESTS_H
