#ifndef TRANSFERS_H
#define TRANSFERS_H

#include <cstddef>
#include <cstdint>


class RxTransfer {
public:
    virtual unsigned char* get_buffer(size_t bytes) = 0;
    virtual size_t get_remaining() = 0;
    virtual void on_filled(uint8_t* buffer, size_t bytes) = 0;
    virtual void on_complete() = 0;
};


class TxTransfer {
public:
    virtual unsigned char const* get_data_ptr() = 0;
    virtual size_t get_remaining() = 0;
    virtual void on_transferred(size_t bytes) = 0;
    virtual void on_complete() = 0;
};


class ZeroLengthRxTransfer: public RxTransfer {
public:
    unsigned char* get_buffer(size_t bytes) override {
        return nullptr;
    }

    size_t get_remaining() override {
        return 0;
    }

    void on_filled(uint8_t* buffer, size_t bytes) override { }

    void on_complete() override { }
};


class ZeroLengthTxTransfer: public TxTransfer {
    unsigned char const* get_data_ptr() override {
        return nullptr;
    }

    size_t get_remaining() override {
        return 0;
    }

    void on_transferred(size_t bytes) override {

    }

    void on_complete() override {

    }
};


template<typename Handler>
class BufferTxTransfer: public TxTransfer {
public:
    using Callback = void (*)(Handler&, BufferTxTransfer<Handler>&);

    BufferTxTransfer() { }

    // for handler

    size_t get_transferred() {
        return size - remaining_bytes;
    }

    BufferTxTransfer& init(
        unsigned char const* data,
        size_t size_,
        Handler* handler_,
        Callback callback_ = nullptr
    ) {
        data_to_tx = data;
        size = size_;
        remaining_bytes = size;
        handler = handler_;
        callback = callback_;

        return *this;
    }

    // for driver

    unsigned char const* get_data_ptr() override {
        return data_to_tx;
    }

    size_t get_remaining() override {
        return remaining_bytes;
    }

    void on_transferred(size_t bytes) override {
        data_to_tx += bytes;
        remaining_bytes -= bytes;
    }

    void on_complete() override {
        if (callback != nullptr) {
            callback(*handler, *this);
        }
    }

private:
    Handler* handler;
    Callback callback;
    unsigned char const* data_to_tx;
    size_t size;
    size_t remaining_bytes;
};


template<size_t size, typename Handler>
class BufferRxTransfer: public RxTransfer {
public:
    using Callback = void (*)(Handler&, BufferRxTransfer<size, Handler>&);

    BufferRxTransfer() :
        data_to_tx(buffer),
        remaining_bytes(size)
    {  }

    // for handler

    unsigned char* get_buffer() {
        return buffer;
    }

    size_t get_transferred() {
        return size - remaining_bytes;
    }

    BufferRxTransfer<size, Handler>& init(Handler* handler_, Callback callback_ = nullptr) {
        handler = handler_;
        callback = callback_;
        data_to_tx = buffer;
        remaining_bytes = size;

        return *this;
    }

    BufferRxTransfer<size, Handler>& reinit() {
        data_to_tx = buffer;
        remaining_bytes = size;

        return *this;
    }

    // for driver

    unsigned char* get_buffer(size_t bytes) override {
        return data_to_tx;
    }

    size_t get_remaining() override {
        return remaining_bytes;
    }

    void on_filled(unsigned char* buffer, size_t bytes) override {
        data_to_tx += bytes;
        remaining_bytes -= bytes;
    }

    void on_complete() override {
        if (callback != nullptr) {
            callback(*handler, *this);
        }
    }

private:
    Handler* handler;
    Callback callback;
    unsigned char* data_to_tx;
    size_t remaining_bytes;
    unsigned char buffer[size];
};


#endif // TRANSFERS_H
