// Тесты таблицы сессии: игроки и их пределы, выдача net_id, поиск, владение,
// порядок записей и путь таблицы от хоста к клиенту через сообщение VESSELS.

#include <string>
#include <vector>

#include "check.h"
#include "session_table.h"

namespace {

void testPlayerLimits() {
    CHECK(olc::SessionTable(1).maxPlayers() == 2);
    CHECK(olc::SessionTable(0).maxPlayers() == 2);
    CHECK(olc::SessionTable(100).maxPlayers() == olc::kMaxPlayersLimit);
    CHECK(olc::SessionTable(5).maxPlayers() == 5);

    // По умолчанию 4 игрока: хост и три клиента.
    olc::SessionTable table;
    CHECK(table.maxPlayers() == 4);
    CHECK(table.playerCount() == 1);
    uint8_t id = 0;
    CHECK(table.addPlayer(id) && id == 1);
    CHECK(table.addPlayer(id) && id == 2);
    CHECK(table.addPlayer(id) && id == 3);
    CHECK(table.playerCount() == 4);
    id = 99;
    CHECK(!table.addPlayer(id));
    CHECK(id == 99);  // при отказе номер не меняется

    // Минимальная сессия: хост и один клиент.
    olc::SessionTable small(1);
    CHECK(small.addPlayer(id) && id == 1);
    CHECK(!small.addPlayer(id));
}

void testPlayerIds() {
    olc::SessionTable table;
    uint8_t id = 0;
    table.addPlayer(id);
    table.addPlayer(id);
    table.addPlayer(id);

    // Освободившийся номер выдаётся снова, и это наименьший свободный.
    CHECK(table.removePlayer(2));
    CHECK(table.playerCount() == 3);
    CHECK(table.addPlayer(id) && id == 2);

    CHECK(table.removePlayer(1));
    CHECK(table.removePlayer(3));
    CHECK(table.addPlayer(id) && id == 1);

    // Несуществующих клиентов удалить нельзя.
    CHECK(!table.removePlayer(9));
    CHECK(!table.removePlayer(olc::kHostOwner));
    CHECK(!table.removePlayer(olc::kNoOwner));
    CHECK(!table.removePlayer(100));
}

void testRemovePlayerReleasesVessels() {
    olc::SessionTable table;
    uint8_t first = 0, second = 0;
    table.addPlayer(first);
    table.addPlayer(second);

    uint16_t a = 0, b = 0, c = 0, d = 0;
    table.addVessel("A", "DeltaGlider", a);
    table.addVessel("B", "DeltaGlider", b);
    table.addVessel("C", "ShuttleA", c);
    table.addVessel("D", "ShuttleA", d);
    table.setOwner(a, first);
    table.setOwner(b, second);
    table.setOwner(c, first);
    table.setOwner(d, olc::kHostOwner);

    CHECK(table.removePlayer(first));
    CHECK(table.findById(a)->owner == olc::kNoOwner);
    CHECK(table.findById(c)->owner == olc::kNoOwner);
    CHECK(table.findById(b)->owner == second);
    CHECK(table.findById(d)->owner == olc::kHostOwner);
}

void testAddVessel() {
    olc::SessionTable table;
    uint16_t id = 99;
    CHECK(table.addVessel("GL-01", "DeltaGlider", id) && id == 0);
    CHECK(table.addVessel("GL-02", "DeltaGlider", id) && id == 1);
    CHECK(table.addVessel("ISS", "", id) && id == 2);  // класс может быть пустым
    CHECK(table.findById(0)->name == "GL-01");
    CHECK(table.findById(0)->className == "DeltaGlider");
    CHECK(table.findById(0)->owner == olc::kNoOwner);

    // Отказы не расходуют номер.
    id = 77;
    CHECK(!table.addVessel("GL-01", "ShuttleA", id));  // имя занято
    CHECK(!table.addVessel("", "ShuttleA", id));       // пустое имя
    CHECK(!table.addVessel(std::string(256, 'x'), "ShuttleA", id));
    CHECK(!table.addVessel("Long", std::string(256, 'x'), id));
    CHECK(id == 77);
    CHECK(table.addVessel(std::string(255, 'x'), std::string(255, 'y'), id) && id == 3);

    // Номера не переиспользуются после удаления.
    CHECK(table.removeVessel(1));
    CHECK(table.addVessel("GL-03", "DeltaGlider", id) && id == 4);
    CHECK(table.addVessel("GL-02", "DeltaGlider", id) && id == 5);  // имя свободно
    CHECK(table.findById(1) == nullptr);
}

void testNetIdExhaustion() {
    olc::SessionTable table;
    uint16_t id = 0;
    // Каждый корабль сразу удаляем: таблица остаётся маленькой и тест идёт быстро,
    // а заодно видно, что удаление номер не возвращает.
    int wrongIds = 0;
    for (int i = 0; i < 65536; i++) {
        if (!table.addVessel("v", "", id) || id != static_cast<uint16_t>(i) ||
            !table.removeVessel(id)) {
            wrongIds++;
        }
    }
    CHECK(wrongIds == 0);
    CHECK(!table.addVessel("one-more", "", id));
}

void testAddVesselWithId() {
    olc::SessionTable table;
    CHECK(table.addVesselWithId(5, "E", "ShuttleA"));
    CHECK(table.addVesselWithId(1, "B", "DeltaGlider"));
    CHECK(table.addVesselWithId(9, "I", ""));
    CHECK(table.addVesselWithId(3, "C", "DeltaGlider"));

    std::vector<olc::VesselEntry> entries = table.vesselEntries();
    CHECK(entries.size() == 4);
    CHECK(entries.size() == 4 && entries[0].netId == 1 && entries[0].name == "B");
    CHECK(entries.size() == 4 && entries[1].netId == 3 && entries[1].name == "C");
    CHECK(entries.size() == 4 && entries[2].netId == 5 && entries[2].className == "ShuttleA");
    CHECK(entries.size() == 4 && entries[3].netId == 9);

    CHECK(!table.addVesselWithId(5, "Other", "X"));  // номер занят
    CHECK(!table.addVesselWithId(7, "B", "X"));      // имя занято
    CHECK(!table.addVesselWithId(7, "", "X"));
    CHECK(!table.addVesselWithId(7, std::string(256, 'x'), "X"));
    CHECK(!table.addVesselWithId(7, "G", std::string(256, 'x')));
    CHECK(table.vesselEntries().size() == 4);

    // Номер 65535 тоже допустим.
    CHECK(table.addVesselWithId(65535, "Last", ""));
    CHECK(table.vesselEntries().back().netId == 65535);
}

void testFind() {
    olc::SessionTable table;
    uint16_t id = 0;
    table.addVessel("A", "ClassA", id);
    table.addVessel("B", "ClassB", id);

    CHECK(table.findById(1) != nullptr && table.findById(1)->name == "B");
    CHECK(table.findByName("A") != nullptr && table.findByName("A")->netId == 0);
    CHECK(table.findById(2) == nullptr);
    CHECK(table.findByName("Z") == nullptr);

    CHECK(table.removeVessel(0));
    CHECK(!table.removeVessel(0));
    CHECK(table.findById(0) == nullptr);
    CHECK(table.findByName("A") == nullptr);
    CHECK(table.findByName("B") != nullptr);
}

void testOwnership() {
    olc::SessionTable table;
    uint8_t client = 0;
    table.addPlayer(client);
    uint16_t a = 0, b = 0;
    table.addVessel("A", "", a);
    table.addVessel("B", "", b);

    CHECK(!table.setOwner(50, client));  // корабля нет
    CHECK(!table.isControlledBy(50, client));
    CHECK(!table.isControlledBy(a, client));  // пока никто не владеет

    CHECK(table.setOwner(a, client));
    CHECK(table.isControlledBy(a, client));
    CHECK(!table.isControlledBy(b, client));
    CHECK(!table.isControlledBy(a, 2));  // чужой клиент

    // Хост и "никто" не клиенты: false даже если они владельцы.
    CHECK(table.setOwner(a, olc::kHostOwner));
    CHECK(!table.isControlledBy(a, olc::kHostOwner));
    CHECK(table.setOwner(a, olc::kNoOwner));
    CHECK(!table.isControlledBy(a, olc::kNoOwner));
    CHECK(!table.isControlledBy(b, olc::kNoOwner));

    // Корабль удалён - управлять нечем.
    table.setOwner(a, client);
    table.removeVessel(a);
    CHECK(!table.isControlledBy(a, client));
}

void testOwnerEntries() {
    olc::SessionTable table;
    table.addVesselWithId(8, "H", "");
    table.addVesselWithId(2, "B", "");
    table.addVesselWithId(4, "D", "");
    table.setOwner(2, 3);
    table.setOwner(8, olc::kHostOwner);

    std::vector<olc::OwnerEntry> owners = table.ownerEntries();
    CHECK(owners.size() == 3);
    CHECK(owners.size() == 3 && owners[0].netId == 2 && owners[0].owner == 3);
    CHECK(owners.size() == 3 && owners[1].netId == 4 && owners[1].owner == olc::kNoOwner);
    CHECK(owners.size() == 3 && owners[2].netId == 8 && owners[2].owner == olc::kHostOwner);
}

// Хост → VESSELS → клиент: у клиента те же записи.
void testHostToClient() {
    olc::SessionTable host;
    uint16_t id = 0;
    host.addVessel("GL-01", "DeltaGlider", id);
    host.addVessel("ISS", "ISS", id);
    host.addVessel("Shuttle", "ShuttleA", id);
    host.removeVessel(1);

    olc::Writer writer;
    CHECK(olc::writeVessels(writer, host.vesselEntries()));

    olc::Reader reader(writer.bytes().data(), writer.bytes().size());
    std::vector<olc::VesselEntry> received;
    CHECK(olc::readVessels(reader, received));

    olc::SessionTable client;
    for (const olc::VesselEntry& entry : received) {
        CHECK(client.addVesselWithId(entry.netId, entry.name, entry.className));
    }

    std::vector<olc::VesselEntry> expected = host.vesselEntries();
    std::vector<olc::VesselEntry> actual = client.vesselEntries();
    CHECK(actual.size() == 2);
    CHECK(actual.size() == expected.size());
    for (size_t i = 0; i < actual.size() && i < expected.size(); i++) {
        CHECK(actual[i].netId == expected[i].netId);
        CHECK(actual[i].name == expected[i].name);
        CHECK(actual[i].className == expected[i].className);
    }
}

}  // namespace

int main() {
    testPlayerLimits();
    testPlayerIds();
    testRemovePlayerReleasesVessels();
    testAddVessel();
    testNetIdExhaustion();
    testAddVesselWithId();
    testFind();
    testOwnership();
    testOwnerEntries();
    testHostToClient();
    return checkResult();
}
