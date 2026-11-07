// Запись и чтение полей протокола (docs/PROTOCOL.md, раздел 2).
//
// Каждое поле пишется отдельно, little-endian (младший байт первым), с фиксированным
// размером. Структуры C++ целиком в сеть не копируются: так формат не зависит от
// компилятора, выравнивания и порядка байтов процессора.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace olc {

// Наибольшая длина строки str: длина пишется одним байтом.
constexpr size_t kMaxStringLength = 255;

// Собирает сообщение: поля дописываются в конец.
class Writer {
public:
    void writeU8(uint8_t value);
    void writeU16(uint16_t value);
    void writeU32(uint32_t value);
    void writeF32(float value);
    void writeF64(double value);

    // Строка: байт длины и байты UTF-8 без завершающего нуля.
    // Если строка длиннее kMaxStringLength, ничего не пишет и возвращает false.
    bool writeString(const std::string& value);

    // Байты как есть, без длины: длину сообщение пишет отдельным полем.
    void writeBytes(const std::vector<uint8_t>& value);

    const std::vector<uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<uint8_t> bytes_;
};

// Читает поля из принятых байтов по порядку.
// Каждая функция возвращает false, если байтов не хватает; тогда значение не меняется,
// а все следующие чтения тоже вернут false. Поэтому можно прочитать все поля подряд
// и проверить результат один раз: битый пакет никогда не читается за своей границей.
class Reader {
public:
    // data должен жить, пока используется Reader: байты не копируются.
    Reader(const uint8_t* data, size_t size);

    bool readU8(uint8_t& value);
    bool readU16(uint16_t& value);
    bool readU32(uint32_t& value);
    bool readF32(float& value);
    bool readF64(double& value);
    bool readString(std::string& value);

    // Ровно count байтов как есть (пара к Writer::writeBytes).
    bool readBytes(std::vector<uint8_t>& value, size_t count);

    // Сколько байтов ещё не прочитано.
    size_t remaining() const { return size_ - position_; }

    // Не было ли ошибки чтения.
    bool ok() const { return ok_; }

private:
    // Проверяет, что впереди есть count байтов; при нехватке запоминает ошибку.
    bool canRead(size_t count);

    const uint8_t* data_;
    size_t size_;
    size_t position_ = 0;
    bool ok_ = true;
};

}  // namespace olc
