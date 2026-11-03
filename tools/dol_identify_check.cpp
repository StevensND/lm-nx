// Host check for the main.dol rule in src/port/dol_identify.cpp. Reads only the DOL files named on the command line.
//
// Build: zig c++ -std=c++20 -I src tools/dol_identify_check.cpp src/port/dol_identify.cpp -o dol_identify_check
// Run:   dol_identify_check <main.dol> [<main.dol> ...]
// Prints "accept GLME01", "accept GLMP01" or "refuse" for each file. Exit code 1 if any file is refused.
#include "port/dol.h"

#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
    int refused = 0;
    for (int a = 1; a < argc; ++a) {
        std::FILE* f = std::fopen(argv[a], "rb");
        if (!f) {
            std::printf("%s: cannot open\n", argv[a]);
            return 2;
        }
        std::vector<uint8_t> dol;
        uint8_t buf[65536];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) dol.insert(dol.end(), buf, buf + n);
        std::fclose(f);
        const port::dol::Version v = port::dol::identify(dol.data(), dol.size());
        const char* result = v == port::dol::Version::GLME01   ? "accept GLME01"
                             : v == port::dol::Version::GLMP01 ? "accept GLMP01"
                                                               : "refuse";
        if (v == port::dol::Version::Unknown) ++refused;
        std::printf("%s: %s\n", argv[a], result);
    }
    return refused ? 1 : 0;
}
