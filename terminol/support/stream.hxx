// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT_STREAM_HXX
#define SUPPORT_STREAM_HXX

#include <vector>
#include <cstring>
#include <cstddef>
#include <cstdint>

struct StreamError {};

class InStream {
public:
    virtual void readAll(void * data,
                         size_t item_size,
                         size_t num_items) throw (StreamError) = 0;

protected:
    InStream() = default;
    virtual ~InStream() = default;
};

class OutStream {
public:
    virtual void writeAll(const void * data,
                          size_t       item_size,
                          size_t       num_items) throw (StreamError) = 0;

protected:
    OutStream() = default;
    virtual ~OutStream() = default;
};

//
//
//

struct EndOfFile : StreamError {};

class InMemoryStream : public InStream {
    const std::vector<uint8_t> & _buffer;
    size_t                       _index;

public:
    InMemoryStream(const std::vector<uint8_t> & buffer) :
        _buffer(buffer), _index(0) {}

    void readAll(void * data,
                 size_t item_size,
                 size_t num_items) throw (EndOfFile) override {
        auto size = item_size * num_items;
        if (_index + size > _buffer.size()) {
            throw EndOfFile();
        }
        else {
            std::memcpy(data, &_buffer[_index], size);
            _index += size;
        }
    }
};

class OutMemoryStream : public OutStream {
    std::vector<uint8_t> & _buffer;
    bool                   _append;
    size_t                 _index;

public:
    OutMemoryStream(std::vector<uint8_t> & buffer, bool append) :
        _buffer(buffer), _append(append), _index(append ? buffer.size() : 0) {}

    void writeAll(const void * data,
                  size_t       item_size,
                  size_t       num_items) throw (EndOfFile) override {
        size_t size = item_size * num_items;

        if (_append) {
            _buffer.resize(_buffer.size() + size);
        }
        else if (_index + size > _buffer.size()) {
            throw EndOfFile();
        }

        std::memcpy(&_buffer[_index], data, size);
        _index += size;
    }
};

#endif // SUPPORT_STREAM_HXX
