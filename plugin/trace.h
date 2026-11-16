// Запись положения одного корабля в CSV для сравнения хоста и клиента после прогона.
//
// Включается полем TraceVessel в Config\OrbiterLanCoop.cfg. Хост пишет настоящее
// положение, клиент - то, которое навязал. Оба пишут MJD хоста, к которому относится
// положение, поэтому строки двух файлов можно сопоставить по времени и посчитать
// расхождение (критерий этапа 1 в docs/PLAN.md).
#pragma once

#include <cstdio>
#include <string>

namespace olcplugin {

class Trace {
public:
    Trace() = default;
    ~Trace();
    Trace(const Trace&) = delete;
    Trace& operator=(const Trace&) = delete;

    // Открывает файл заново и пишет строку заголовка. false - не открылся.
    bool open(const std::string& path);
    void close();
    bool isOpen() const { return file_ != nullptr; }

    // Одна строка: MJD хоста, имя опорного тела, положение относительно него, м,
    // и признак "буфер кончился" (только у клиента; у хоста всегда 0).
    void write(double mjd, const char* refBody, double x, double y, double z, bool starved);

private:
    std::FILE* file_ = nullptr;
};

}  // namespace olcplugin
