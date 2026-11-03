#include "port/dol.h"

#include <dolphin/dvd.h>
#include <dolphin/os.h>

#include <array>
#include <bit>

namespace port::dol {
namespace {

struct Section {
    uint32_t fileOffset;
    uint32_t addr;
    uint32_t size;
};

const uint8_t* sDol;
size_t sDolSize;
std::array<Section, 18> sSections;  // 7 text + 11 data
Version sVersion = Version::Unknown;

uint32_t read_be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

}  // namespace

Version load() {
    s32 size = 0;
    sDol = DVDGetDOLLocation(&size);
    sDolSize = size > 0 ? size_t(size) : 0;
    sVersion = Version::Unknown;
    if (!sDol || sDolSize < 0x100) {
        OSReport("[dol] no DOL found in disc image\n");
        return sVersion;
    }
    for (size_t i = 0; i < sSections.size(); ++i) {
        sSections[i] = {read_be32(sDol + i * 4), read_be32(sDol + 0x48 + i * 4), read_be32(sDol + 0x90 + i * 4)};
    }
    sVersion = identify(sDol, sDolSize);
    return sVersion;
}

Version version() { return sVersion; }

std::span<const uint8_t> bytes(uint32_t addr, size_t size) {
    for (const Section& s : sSections) {
        if (s.size != 0 && addr >= s.addr && addr - s.addr + size <= s.size && s.fileOffset + s.size <= sDolSize) {
            return {sDol + s.fileOffset + (addr - s.addr), size};
        }
    }
    return {};
}

uint8_t be8(uint32_t addr) {
    auto b = bytes(addr, 1);
    return b.empty() ? 0 : b[0];
}

uint16_t be16(uint32_t addr) {
    auto b = bytes(addr, 2);
    return b.empty() ? 0 : uint16_t((b[0] << 8) | b[1]);
}

uint32_t be32(uint32_t addr) {
    auto b = bytes(addr, 4);
    return b.empty() ? 0 : read_be32(b.data());
}

float bef32(uint32_t addr) { return std::bit_cast<float>(be32(addr)); }

}  // namespace port::dol
