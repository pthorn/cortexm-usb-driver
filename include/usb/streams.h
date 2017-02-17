#ifndef STREAMS_H
#define STREAMS_H

#include <cstddef>
#include <cstdint>


class RxStream {
public:
    virtual size_t get_remaining_bytes() = 0;
    virtual unsigned char* get_buffer(size_t bytes) = 0;
    virtual void on_filled(uint8_t* buffer, size_t bytes) = 0;
    virtual void on_complete() = 0;
};


class TxStream {
public:
    virtual unsigned char const* get_data_ptr() = 0;
    virtual size_t get_remaining_bytes() = 0;
    virtual void on_transferred(size_t bytes) = 0;
    virtual void on_complete() = 0;
};


class BufferTxStream: public TxStream {
public:
    void init(unsigned char const* data, size_t bytes) {
        this->data_to_tx = data;
        this->remaining_bytes = bytes;
    }

    unsigned char const* get_data_ptr() {
        return data_to_tx;
    }

    size_t get_remaining_bytes() {
        return remaining_bytes;
    }

    void on_transferred(size_t bytes) {
        data_to_tx += bytes;
        remaining_bytes -= bytes;
    }

    void on_complete() {
        //
    }

private:
    // TODO use unsigned char* for all data buffers to comply with aliasing rules?
    unsigned char const* data_to_tx;
    size_t remaining_bytes;
};


#endif // STREAMS_H
