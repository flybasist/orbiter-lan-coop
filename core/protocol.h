// Постоянные сетевого протокола OLC (docs/PROTOCOL.md) и общие правила разбора.
#pragma once

#include <cstdint>

namespace olc {

// Версия протокола: пишется в каждый UDP-пакет и в HELLO (PROTOCOL.md, разделы 4 и 9).
constexpr uint16_t kProtocolVersion = 1;

// Порт по умолчанию: хост слушает TCP и UDP на одном номере (раздел 3).
constexpr uint16_t kDefaultPort = 47800;

// Сигнатура в начале каждого UDP-пакета: байты 'O', 'L', 'C', '0' (раздел 4).
constexpr uint8_t kUdpMagic[4] = {'O', 'L', 'C', '0'};

// Наибольший размер UDP-пакета: такой пакет не дробится ни в одной обычной сети.
constexpr uint32_t kMaxUdpPacketSize = 1200;

// Наибольшая длина TCP-сообщения после поля length: 1 МБ.
constexpr uint32_t kMaxTcpMessageSize = 1024 * 1024;

// Новее ли порядковый номер UDP-пакета a, чем b.
// Номер (seq) - 32-битный счётчик, который после 4294967295 снова идёт с нуля.
// Поэтому a считается новее b, если a впереди b меньше чем на половину круга:
// например, 2 новее 4294967290 (счётчик переполнился), а 4294967290 не новее 2.
bool isSequenceNewer(uint32_t a, uint32_t b);

}  // namespace olc
