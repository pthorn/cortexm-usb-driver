#ifndef ENDPOINTS_H
#define ENDPOINTS_H

#include <cstddef>

#include "usb/defs.h"
//#include "print.h"

class Device;


class Endpoint {
public:
    Endpoint(
        uint8_t n, InOut in_out, EPType type,
        uint16_t max_pkt_size, uint16_t tx_fifo_size = 0
    ) :
        n(n), in_out(in_out), type(type),
        max_pkt_size(max_pkt_size), tx_fifo_size(tx_fifo_size)
    {
        // TODO
    }

    // identification

    uint8_t get_number() {
        return n;
    }

    InOut get_inout() {
        return in_out;
    }

    EPType get_type() {
        return type;
    }

    uint16_t get_max_pkt_size() {
        return max_pkt_size;
    }

    uint16_t get_tx_fifo_size() {
        return tx_fifo_size;
    }

    Device* get_device() {
        return device;
    }

    // TODO
    //virtual unsigned char* get_descriptor() = 0;

    // events

    virtual void on_attached(Device* device) {
        this->device = device;
    }

    virtual void on_initialized() { }

    // IN data transfer

    virtual unsigned char const* get_data_ptr() {
        return data;
    }

    virtual void on_transferred(size_t bytes) {
        data += bytes;
        remaining -= bytes;
    }

    virtual void on_in_transfer_complete();

    // OUT data transfer

    // TODO!
    virtual unsigned char* get_buffer(size_t bytes) {
        return const_cast<unsigned char*>(data);
    }

    // TODO?
    virtual void on_filled(unsigned char const* buffer, size_t bytes) {
        data += bytes;
        remaining -= bytes;
    }

    virtual void on_out_transfer_complete();

    // TODO IN and OUT data transfer?

    virtual size_t get_remaining() {
        return remaining;
    }

    // class driver API

    // TODO one-call method for starting xfers?
    void init_transfer(unsigned char const* buffer, size_t size) {
        data = buffer;
        remaining = size;
    }

protected:
    Device* device;
    unsigned char const* data;
    size_t remaining;

private:
    // TODO template args to save RAM?
    uint8_t const n;
    InOut const in_out;
    EPType const type;
    uint16_t const max_pkt_size;
    uint16_t const tx_fifo_size;
};

#endif // ENDPOINTS_H
