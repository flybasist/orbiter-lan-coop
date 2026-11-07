// Заголовки сообщений (docs/PROTOCOL.md, раздел 4): UDP-заголовок и TCP-кадры с длиной.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "serialize.h"

namespace olc {

// Размер UDP-заголовка: magic 4 + version 2 + type 1 + session 4 + seq 4 байта.
constexpr size_t kUdpHeaderSize = 15;

// Поля UDP-заголовка, которые различаются от пакета к пакету.
struct UdpHeader {
    uint8_t type = 0;      // тип сообщения (PROTOCOL.md, раздел 6)
    uint32_t session = 0;  // номер сессии из WELCOME
    uint32_t seq = 0;      // порядковый номер у отправителя
};

// Итог проверки UDP-заголовка. Всё, кроме Ok, - пакет отбрасывается
// и учитывается в статистике по причине.
enum class UdpHeaderResult {
    Ok,
    TooShort,      // меньше заголовка
    TooLong,       // больше kMaxUdpPacketSize
    WrongMagic,    // не пакет OLC
    WrongVersion,  // другая версия протокола
};

// Пишет UDP-заголовок: сигнатуру, текущую версию протокола и поля header.
void writeUdpHeader(Writer& writer, const UdpHeader& header);

// Читает и проверяет UDP-заголовок в начале пакета. reader должен стоять в начале
// пакета; при Ok он остаётся сразу за заголовком, на первом поле сообщения.
// Номер сессии и seq здесь не проверяются: это знает только тот, кто принимает.
UdpHeaderResult readUdpHeader(Reader& reader, UdpHeader& header);

// Собирает TCP-кадр: длина (u32), тип (u8) и тело сообщения.
// Длина считает тип и тело, но не саму себя.
std::vector<uint8_t> makeTcpFrame(uint8_t type, const std::vector<uint8_t>& body);

// TCP - это поток: сообщение может прийти по частям или вместе с соседними.
// TcpFrameReader копит принятые байты и выдаёт сообщения целиком.
class TcpFrameReader {
public:
    enum class Result {
        Ready,     // сообщение готово
        NeedMore,  // сообщения целиком ещё нет, ждём байтов
        Broken,    // длина 0 или больше kMaxTcpMessageSize: соединение надо закрыть
    };

    // Добавляет байты, принятые из сокета.
    void append(const uint8_t* data, size_t size);

    // Достаёт следующее целое сообщение: type - тип, body - тело без длины и типа.
    // После Broken поток уже не разобрать, и все следующие вызовы тоже вернут Broken.
    Result next(uint8_t& type, std::vector<uint8_t>& body);

private:
    std::vector<uint8_t> pending_;  // принятые, но ещё не выданные байты
    bool broken_ = false;
};

}  // namespace olc
