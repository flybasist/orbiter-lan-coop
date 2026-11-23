// Tests of writing and reading protocol fields: exact byte order, round trip,
// strings and broken data.

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "check.h"
#include "serialize.h"

namespace {

// Whether the bytes written by Writer match the expected ones.
bool bytesEqual(const olc::Writer& writer, const std::vector<uint8_t>& expected) {
    return writer.bytes() == expected;
}

void testByteOrder() {
    // The byte order is set by the protocol, so we check it byte by byte.
    olc::Writer w;
    w.writeU8(0xAB);
    w.writeU16(0x1234);
    w.writeU32(0x89ABCDEF);
    CHECK(bytesEqual(w, {0xAB, 0x34, 0x12, 0xEF, 0xCD, 0xAB, 0x89}));

    // 1.0 in IEEE 754: f32 = 0x3F800000, f64 = 0x3FF0000000000000.
    olc::Writer f;
    f.writeF32(1.0f);
    f.writeF64(1.0);
    CHECK(bytesEqual(f, {0x00, 0x00, 0x80, 0x3F, 0, 0, 0, 0, 0, 0, 0xF0, 0x3F}));
}

void testRoundTrip() {
    olc::Writer w;
    w.writeU8(255);
    w.writeU16(65535);
    w.writeU32(4294967295u);
    w.writeF32(-0.5f);
    w.writeF64(6.771e6);  // low orbit radius, m
    w.writeF64(-1.0e-300);
    w.writeF64(std::numeric_limits<double>::infinity());

    olc::Reader r(w.bytes().data(), w.bytes().size());
    uint8_t a = 0;
    uint16_t b = 0;
    uint32_t c = 0;
    float d = 0.0f;
    double e = 0.0, f = 0.0, g = 0.0;
    CHECK(r.readU8(a) && a == 255);
    CHECK(r.readU16(b) && b == 65535);
    CHECK(r.readU32(c) && c == 4294967295u);
    CHECK(r.readF32(d) && d == -0.5f);
    CHECK(r.readF64(e) && e == 6.771e6);
    CHECK(r.readF64(f) && f == -1.0e-300);
    CHECK(r.readF64(g) && std::isinf(g) && g > 0.0);
    CHECK(r.remaining() == 0);
    CHECK(r.ok());

    // f64 is sent without loss: millimetre precision at a 7000 km radius matters.
    olc::Writer precise;
    double position = 6771000.0012345678;
    precise.writeF64(position);
    olc::Reader pr(precise.bytes().data(), precise.bytes().size());
    double back = 0.0;
    CHECK(pr.readF64(back) && back == position);
}

void testStrings() {
    olc::Writer w;
    CHECK(w.writeString(""));
    CHECK(w.writeString("DeltaGlider"));
    CHECK(w.writeString("Пилот"));  // UTF-8 (Russian "pilot"): 5 letters, 10 bytes
    CHECK(w.writeString(std::string(255, 'x')));
    size_t sizeBefore = w.bytes().size();
    CHECK(!w.writeString(std::string(256, 'x')));  // longer than 255 - not written
    CHECK(w.bytes().size() == sizeBefore);

    olc::Reader r(w.bytes().data(), w.bytes().size());
    std::string s;
    CHECK(r.readString(s) && s.empty());
    CHECK(r.readString(s) && s == "DeltaGlider");
    CHECK(r.readString(s) && s == "Пилот" && s.size() == 10);
    CHECK(r.readString(s) && s == std::string(255, 'x'));
    CHECK(r.remaining() == 0);

    // The length byte and the string bytes themselves.
    olc::Writer ab;
    ab.writeString("ab");
    CHECK(bytesEqual(ab, {2, 'a', 'b'}));
}

void testTruncated() {
    // Fewer bytes than the field needs: the read fails, the value does not change.
    const uint8_t data[3] = {1, 2, 3};
    olc::Reader r(data, sizeof(data));
    uint32_t value = 77;
    CHECK(!r.readU32(value));
    CHECK(value == 77);
    CHECK(!r.ok());

    // After an error nothing is read, not even what there are enough bytes for.
    uint8_t small = 9;
    CHECK(!r.readU8(small));
    CHECK(small == 9);

    // Empty data.
    olc::Reader empty(nullptr, 0);
    double d = 1.5;
    CHECK(!empty.readF64(d) && d == 1.5);
    std::string s = "keep";
    olc::Reader emptyString(nullptr, 0);
    CHECK(!emptyString.readString(s) && s == "keep");
}

void testBrokenString() {
    // The string length promises 10 bytes, but there are only 3.
    const uint8_t data[4] = {10, 'a', 'b', 'c'};
    olc::Reader r(data, sizeof(data));
    std::string s = "keep";
    CHECK(!r.readString(s));
    CHECK(s == "keep");
    CHECK(!r.ok());
    CHECK(r.remaining() == 4);  // the position did not move

    // Correct fields followed by a truncated one: the first ones read, the last does not.
    const uint8_t packet[5] = {0x34, 0x12, 0x01, 0x02, 0x03};
    olc::Reader p(packet, sizeof(packet));
    uint16_t first = 0;
    uint32_t second = 0;
    CHECK(p.readU16(first) && first == 0x1234);
    CHECK(!p.readU32(second));
    CHECK(p.remaining() == 3);
}

void testBytes() {
    olc::Writer w;
    w.writeBytes({1, 2, 3});
    w.writeU8(9);
    CHECK(w.bytes() == std::vector<uint8_t>({1, 2, 3, 9}));

    olc::Reader r(w.bytes().data(), w.bytes().size());
    std::vector<uint8_t> value = {7, 7, 7, 7, 7};  // the old contents must be replaced
    CHECK(r.readBytes(value, 3));
    CHECK(value == std::vector<uint8_t>({1, 2, 3}));

    // Zero bytes can always be read.
    CHECK(r.readBytes(value, 0) && value.empty());

    // Not enough bytes: the value does not change, the error is remembered.
    value = {5};
    CHECK(!r.readBytes(value, 2));
    CHECK(value == std::vector<uint8_t>({5}));
    CHECK(!r.ok());
}

}  // namespace

int main() {
    testByteOrder();
    testRoundTrip();
    testStrings();
    testTruncated();
    testBrokenString();
    testBytes();
    return checkResult();
}
