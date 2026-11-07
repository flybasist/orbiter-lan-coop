// Тесты записи и чтения полей протокола: точный порядок байтов, туда-обратно,
// строки и битые данные.

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "check.h"
#include "serialize.h"

namespace {

// Совпадают ли байты, записанные Writer, с ожидаемыми.
bool bytesEqual(const olc::Writer& writer, const std::vector<uint8_t>& expected) {
    return writer.bytes() == expected;
}

void testByteOrder() {
    // Порядок байтов задан протоколом, поэтому проверяем его побайтно.
    olc::Writer w;
    w.writeU8(0xAB);
    w.writeU16(0x1234);
    w.writeU32(0x89ABCDEF);
    CHECK(bytesEqual(w, {0xAB, 0x34, 0x12, 0xEF, 0xCD, 0xAB, 0x89}));

    // 1.0 в IEEE 754: f32 = 0x3F800000, f64 = 0x3FF0000000000000.
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
    w.writeF64(6.771e6);  // радиус низкой орбиты, м
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

    // f64 передаётся без потерь: важна точность до миллиметра на радиусе 7000 км.
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
    CHECK(w.writeString("Пилот"));  // UTF-8: 5 букв, 10 байтов
    CHECK(w.writeString(std::string(255, 'x')));
    size_t sizeBefore = w.bytes().size();
    CHECK(!w.writeString(std::string(256, 'x')));  // длиннее 255 - не пишется
    CHECK(w.bytes().size() == sizeBefore);

    olc::Reader r(w.bytes().data(), w.bytes().size());
    std::string s;
    CHECK(r.readString(s) && s.empty());
    CHECK(r.readString(s) && s == "DeltaGlider");
    CHECK(r.readString(s) && s == "Пилот" && s.size() == 10);
    CHECK(r.readString(s) && s == std::string(255, 'x'));
    CHECK(r.remaining() == 0);

    // Байт длины и сами байты строки.
    olc::Writer ab;
    ab.writeString("ab");
    CHECK(bytesEqual(ab, {2, 'a', 'b'}));
}

void testTruncated() {
    // Байтов меньше, чем нужно полю: чтение не удаётся, значение не меняется.
    const uint8_t data[3] = {1, 2, 3};
    olc::Reader r(data, sizeof(data));
    uint32_t value = 77;
    CHECK(!r.readU32(value));
    CHECK(value == 77);
    CHECK(!r.ok());

    // После ошибки не читается ничего, даже то, на что байтов хватает.
    uint8_t small = 9;
    CHECK(!r.readU8(small));
    CHECK(small == 9);

    // Пустые данные.
    olc::Reader empty(nullptr, 0);
    double d = 1.5;
    CHECK(!empty.readF64(d) && d == 1.5);
    std::string s = "keep";
    olc::Reader emptyString(nullptr, 0);
    CHECK(!emptyString.readString(s) && s == "keep");
}

void testBrokenString() {
    // Длина строки обещает 10 байтов, а есть только 3.
    const uint8_t data[4] = {10, 'a', 'b', 'c'};
    olc::Reader r(data, sizeof(data));
    std::string s = "keep";
    CHECK(!r.readString(s));
    CHECK(s == "keep");
    CHECK(!r.ok());
    CHECK(r.remaining() == 4);  // позиция не сдвинулась

    // Правильные поля, а за ними обрезанное: первые читаются, последнее - нет.
    const uint8_t packet[5] = {0x34, 0x12, 0x01, 0x02, 0x03};
    olc::Reader p(packet, sizeof(packet));
    uint16_t first = 0;
    uint32_t second = 0;
    CHECK(p.readU16(first) && first == 0x1234);
    CHECK(!p.readU32(second));
    CHECK(p.remaining() == 3);
}

}  // namespace

int main() {
    testByteOrder();
    testRoundTrip();
    testStrings();
    testTruncated();
    testBrokenString();
    return checkResult();
}
