#include "serialize.h"

#include <cstring>
#include <limits>

namespace olc {

// f32 и f64 передаются как числа IEEE 754. На всех платформах Orbiter (Windows x64)
// и на Mac/Linux float и double именно такие; проверяем это при сборке.
static_assert(std::numeric_limits<float>::is_iec559 && sizeof(float) == 4,
              "float must be IEEE 754 single precision");
static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8,
              "double must be IEEE 754 double precision");

// ---------- Writer ----------

void Writer::writeU8(uint8_t value) {
    bytes_.push_back(value);
}

void Writer::writeU16(uint16_t value) {
    bytes_.push_back(static_cast<uint8_t>(value));
    bytes_.push_back(static_cast<uint8_t>(value >> 8));
}

void Writer::writeU32(uint32_t value) {
    for (int i = 0; i < 4; i++) {
        bytes_.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
}

void Writer::writeF32(float value) {
    // Биты числа переносим в целое через memcpy (так по стандарту можно)
    // и пишем как обычное целое little-endian.
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    writeU32(bits);
}

void Writer::writeF64(double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int i = 0; i < 8; i++) {
        bytes_.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    }
}

bool Writer::writeString(const std::string& value) {
    if (value.size() > kMaxStringLength) {
        return false;
    }
    writeU8(static_cast<uint8_t>(value.size()));
    bytes_.insert(bytes_.end(), value.begin(), value.end());
    return true;
}

// ---------- Reader ----------

Reader::Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

bool Reader::canRead(size_t count) {
    if (!ok_ || count > remaining()) {
        ok_ = false;
        return false;
    }
    return true;
}

bool Reader::readU8(uint8_t& value) {
    if (!canRead(1)) {
        return false;
    }
    value = data_[position_];
    position_ += 1;
    return true;
}

bool Reader::readU16(uint16_t& value) {
    if (!canRead(2)) {
        return false;
    }
    value = static_cast<uint16_t>(data_[position_] | (data_[position_ + 1] << 8));
    position_ += 2;
    return true;
}

bool Reader::readU32(uint32_t& value) {
    if (!canRead(4)) {
        return false;
    }
    uint32_t result = 0;
    for (int i = 0; i < 4; i++) {
        result |= static_cast<uint32_t>(data_[position_ + i]) << (8 * i);
    }
    value = result;
    position_ += 4;
    return true;
}

bool Reader::readF32(float& value) {
    uint32_t bits = 0;
    if (!readU32(bits)) {
        return false;
    }
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool Reader::readF64(double& value) {
    if (!canRead(8)) {
        return false;
    }
    uint64_t bits = 0;
    for (int i = 0; i < 8; i++) {
        bits |= static_cast<uint64_t>(data_[position_ + i]) << (8 * i);
    }
    std::memcpy(&value, &bits, sizeof(value));
    position_ += 8;
    return true;
}

bool Reader::readString(std::string& value) {
    // Длину читаем сами, а не через readU8: если байтов строки не хватит,
    // позиция не должна сдвинуться наполовину.
    if (!canRead(1)) {
        return false;
    }
    size_t length = data_[position_];
    if (!canRead(1 + length)) {
        return false;
    }
    const char* start = reinterpret_cast<const char*>(data_ + position_ + 1);
    value.assign(start, length);
    position_ += 1 + length;
    return true;
}

}  // namespace olc
