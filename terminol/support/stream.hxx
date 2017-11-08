// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT_STREAM_HXX
#define SUPPORT_STREAM_HXX

#include "terminol/support/exception.hxx"

#include <vector>
#include <cstring>
#include <cstddef>
#include <cstdint>

class StreamError : public Exception {
protected:
    explicit StreamError(const std::string & what_arg) : Exception(what_arg) {}
};

// The data on an InStream was corrupt
class BadStreamInput final : StreamError {
public:
    explicit BadStreamInput(const std::string & what_arg) : StreamError(what_arg) {}
};

// There was a problem reading/writing the stream
class StreamAccessError : StreamError {
protected:
    explicit StreamAccessError(const std::string & what_arg) : StreamError(what_arg) {}
};

// An input stream ran out of input
class EndOfStreamInput final : StreamAccessError {
public:
    explicit EndOfStreamInput(const std::string & what_arg) : StreamAccessError(what_arg) {}
};

// An error occured accessing an in/out stream, e.g. I/O error reading from file or
// write to socket failed because other end disconnected
class OtherStreamAccessError final : StreamAccessError {
public:
    explicit OtherStreamAccessError(const std::string & what_arg) : StreamAccessError(what_arg) {}
};

// XXX InStream and OutStream look like interfaces (because the destructors are protected)
//     but they lack the 'I_' prefix

class InStream {
public:
    virtual void readAll(void * data, size_t item_size, size_t num_items) = 0;

protected:
    ~InStream() = default;
};

class OutStream {
public:
    virtual void writeAll(const void * data, size_t item_size, size_t num_items) = 0;

protected:
    ~OutStream() = default;
};

//
//
//

class InMemoryStream final : public InStream {
    const std::vector<uint8_t> & _buffer;
    size_t                       _index = 0;

public:
    explicit InMemoryStream(const std::vector<uint8_t> & buffer) : _buffer(buffer) {}

    void readAll(void * data, size_t item_size, size_t num_items) override {
        auto size = item_size * num_items;
        if (_index + size > _buffer.size()) { THROW(EndOfStreamInput("end-of-file")); }
        else {
            std::memcpy(data, &_buffer[_index], size);
            _index += size;
        }
    }
};

class OutMemoryStream final : public OutStream {
    std::vector<uint8_t> & _buffer;
    size_t                 _index;

public:
    explicit OutMemoryStream(std::vector<uint8_t> & buffer)
        : _buffer(buffer), _index(_buffer.size()) {}

    void writeAll(const void * data, size_t item_size, size_t num_items) override {
        size_t size = item_size * num_items;

        _buffer.resize(_buffer.size() + size);

        std::memcpy(&_buffer[_index], data, size);
        _index += size;
    }
};

#endif // SUPPORT_STREAM_HXX
