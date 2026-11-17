// Writes the position of one vessel to CSV to compare host and client after a run.
//
// Turned on by the TraceVessel field in Config\OrbiterLanCoop.cfg. The host writes the real
// position, the client - the one it imposed. Both write the host MJD the position
// refers to, so the lines of the two files can be matched by time and the
// error computed (stage 1 criterion in docs/PLAN.md).
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

    // Opens the file anew and writes the header line. false - it did not open.
    bool open(const std::string& path);
    void close();
    bool isOpen() const { return file_ != nullptr; }

    // One line: host MJD, reference body name, position relative to it, m,
    // and the "buffer ran out" flag (client only; always 0 on the host).
    void write(double mjd, const char* refBody, double x, double y, double z, bool starved);

private:
    std::FILE* file_ = nullptr;
};

}  // namespace olcplugin
