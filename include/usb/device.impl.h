#ifndef DEVICE_IMPL_H
#define DEVICE_IMPL_H

#include "device.h"
#include "ep_config.h"
#include "handler.h"
#include "descriptors.h"
#include "standard_requests.h"
#include "debug.h"


#define CALL_HANDLERS(method, ...) \
    for (auto handler: handlers) { \
        if (handler != nullptr) { \
            handler->method(__VA_ARGS__); \
        } \
    }



extern StandardRequests standard_requests;

template<size_t NHandlers, size_t NEndpoints>
Device<NHandlers, NEndpoints>::Device(EndpointConfig const* endpoint_config, Descriptors const& descriptors):
    ctrl_ep_dispatcher(this),
    ep_dispatcher(this),
    endpoint_config(endpoint_config),
    state(State::NONE),
    current_configuration(0)
{
    standard_requests.init(descriptors);
    add_handler(&standard_requests);
}


template<size_t NHandlers, size_t NEndpoints>
void Device<NHandlers, NEndpoints>::add_handler(Handler* handler)
{
    for (auto& el: handlers) {
        if (el == nullptr) {
            el = handler;
            handler->on_attached(this);
            return;
        }
    }

    d_assert("no free slots for handlers left");
}


template <size_t NHandlers, size_t NEndpoints>
void Device<NHandlers, NEndpoints>::init_endpoints(uint8_t configuration)
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


template <size_t NHandlers, size_t NEndpoints>
void Device<NHandlers, NEndpoints>::deinit_endpoints()
{
    // TODO
}


template<size_t NHandlers, size_t NEndpoints>
inline void Device<NHandlers, NEndpoints>::on_connect()
{
    CALL_HANDLERS(on_connect);
}


template<size_t NHandlers, size_t NEndpoints>
inline void Device<NHandlers, NEndpoints>::on_disconnect()
{
    CALL_HANDLERS(on_disconnect);
}


template<size_t NHandlers, size_t NEndpoints>
inline void Device<NHandlers, NEndpoints>::on_suspend()
{
    CALL_HANDLERS(on_suspend);
}


template<size_t NHandlers, size_t NEndpoints>
inline void Device<NHandlers, NEndpoints>::on_resume()
{
    CALL_HANDLERS(on_resume);
}


template<size_t NHandlers, size_t NEndpoints>
inline void Device<NHandlers, NEndpoints>::on_reset()
{
    state = State::SPEED;
    current_configuration = 0;
    CALL_HANDLERS(on_reset);
}


template<size_t NHandlers, size_t NEndpoints>
inline uint8_t Device<NHandlers, NEndpoints>::get_configuration()
{
    return current_configuration;
}


template<size_t NHandlers, size_t NEndpoints>
bool Device<NHandlers, NEndpoints>::set_configuration(uint8_t configuration)
{
    if (current_configuration == configuration) {
        return true;
    }

    if (configuration == 0) {
        deinit_endpoints();
        current_configuration = configuration;
        state = State::ADDRESS;
        CALL_HANDLERS(on_set_configuration, configuration);  // TODO ???

        return true;
    }

    // only one configuration is supported
    // (windows doesnt't support multiple configurations)
    if (configuration == 1) {
        init_endpoints(configuration);
        current_configuration = configuration;
        state = State::CONFIGURED;
        CALL_HANDLERS(on_set_configuration, configuration);

        return true;
    }

    return false;
}


template<size_t NHandlers, size_t NEndpoints>
EndpointConfig const& Device<NHandlers, NEndpoints>::get_ep_config(uint8_t ep_n, InOut in_out) {
    for (size_t i = 0; /*i < endpoint_config_size*/; ++i) {
        if (endpoint_config[i].n > 15) {
            d_assert("no such endpoint in endpoint_config");
        }

        if (endpoint_config[i].n == ep_n && endpoint_config[i].in_out == in_out) {
            return endpoint_config[i];
        }
    }
}


template<size_t NHandlers, size_t NEndpoints>
inline SetupPacket const& Device<NHandlers, NEndpoints>::get_setup_pkt() {
    return ctrl_ep_dispatcher.setup_packet;
}


template<size_t NHandlers, size_t NEndpoints>
inline void Device<NHandlers, NEndpoints>::dispatch_in_transfer_complete(uint8_t ep_n) {
    if (ep_n == 0) {
        ctrl_ep_dispatcher.on_in_transfer_complete(ep_n);
    } else {
        ep_dispatcher.on_in_transfer_complete(ep_n);
    }
}


template<size_t NHandlers, size_t NEndpoints>
inline void Device<NHandlers, NEndpoints>::dispatch_out_transfer_complete(uint8_t ep_n) {
    if (ep_n == 0) {
        ctrl_ep_dispatcher.on_out_transfer_complete(ep_n);
    } else {
        ep_dispatcher.on_out_transfer_complete(ep_n);
    }
}

#endif // DEVICE_IMPL_H
