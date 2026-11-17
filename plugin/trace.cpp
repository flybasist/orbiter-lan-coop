#include "trace.h"

namespace olcplugin {

Trace::~Trace() {
    close();
}

bool Trace::open(const std::string& path) {
    close();
    // fopen_s instead of fopen: MSVC considers fopen unsafe and warns.
    if (fopen_s(&file_, path.c_str(), "w") != 0) {
        file_ = nullptr;
        return false;
    }
    std::fprintf(file_, "mjd,ref_body,x,y,z,starved\n");
    return true;
}

void Trace::close() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

void Trace::write(double mjd, const char* refBody, double x, double y, double z, bool starved) {
    if (!file_) {
        return;
    }
    // MJD with 12 decimal places is a precision of about 0.1 µs.
    std::fprintf(file_, "%.12f,%s,%.3f,%.3f,%.3f,%d\n", mjd, refBody, x, y, z, starved ? 1 : 0);
}

}  // namespace olcplugin
