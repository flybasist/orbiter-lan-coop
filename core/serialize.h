// Writing and reading protocol fields (docs/PROTOCOL.md, section 2).
//
// Each field is written separately, little-endian (low byte first), with a fixed
// size. C++ structs are never copied to the network as a whole: that way the format does not
// depend on the compiler, alignment or the processor's byte order.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace olc {

// Largest length of a str string: the length is written as one byte.
constexpr size_t kMaxStringLength = 255;

// Builds a message: fields are appended at the end.
class Writer {
public:
    void writeU8(uint8_t value);
    void writeU16(uint16_t value);
    void writeU32(uint32_t value);
    void writeF32(float value);
    void writeF64(double value);

    // String: a length byte and UTF-8 bytes without a terminating zero.
    // If the string is longer than kMaxStringLength, writes nothing and returns false.
    bool writeString(const std::string& value);

    // Bytes as is, without a length: the message writes the length as a separate field.
    void writeBytes(const std::vector<uint8_t>& value);

    const std::vector<uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<uint8_t> bytes_;
};

// Reads fields from received bytes in order.
// Every function returns false if bytes are missing; then the value is unchanged,
// and all later reads return false too. So all fields can be read in a row
// and the result checked once: a broken packet is never read past its end.
class Reader {
public:
    // data must outlive the Reader: the bytes are not copied.
    Reader(const uint8_t* data, size_t size);

    bool readU8(uint8_t& value);
    bool readU16(uint16_t& value);
    bool readU32(uint32_t& value);
    bool readF32(float& value);
    bool readF64(double& value);
    bool readString(std::string& value);

    // Exactly count bytes as is (the counterpart of Writer::writeBytes).
    bool readBytes(std::vector<uint8_t>& value, size_t count);

    // How many bytes are not read yet.
    size_t remaining() const { return size_ - position_; }

    // Whether there was a read error.
    bool ok() const { return ok_; }

private:
    // Checks that count bytes are ahead; if not, remembers the error.
    bool canRead(size_t count);

    const uint8_t* data_;
    size_t size_;
    size_t position_ = 0;
    bool ok_ = true;
};

}  // namespace olc
