// Pure main.dol checks. No aurora or dolphin headers: tools/dol_identify_check.cpp compiles this file alone.
#include "port/dol.h"

#include <bit>
#include <cstring>

namespace port::dol {
namespace {

struct Section {
    uint32_t fileOffset;
    uint32_t addr;
    uint32_t size;
};

// Retail GLME01 rev 0 layout, printed by tools/dol_reference.py from the retail main.dol.
// 7 text sections, then 11 data sections. Empty sections have size 0.
constexpr Section kGLME01Sections[18] = {
    {0x00000100, 0x80003100, 9152},      // text0
    {0x000024c0, 0x80005760, 2122592},   // text1
    {0x00000000, 0x00000000, 0},         // text2
    {0x00000000, 0x00000000, 0},         // text3
    {0x00000000, 0x00000000, 0},         // text4
    {0x00000000, 0x00000000, 0},         // text5
    {0x00000000, 0x00000000, 0},         // text6
    {0x00208820, 0x800054c0, 384},       // data0
    {0x002089a0, 0x80005640, 288},       // data1
    {0x00208ac0, 0x8020bac0, 800},       // data2
    {0x00208de0, 0x8020bde0, 32},        // data3
    {0x00208e00, 0x8020be00, 49920},     // data4
    {0x00215100, 0x80218100, 1566688},   // data5 (dialogue font in a translation)
    {0x003938e0, 0x804cff60, 32896},     // data6
    {0x0039b960, 0x804d9880, 16576},     // data7
    {0x00000000, 0x00000000, 0},         // data8
    {0x00000000, 0x00000000, 0},         // data9
    {0x00000000, 0x00000000, 0},         // data10
};
constexpr uint32_t kGLME01Bss[2] = {0x803968e0, 1322893};  // address, size
constexpr uint32_t kGLME01Entry = 0x80003100;
// SHA-1 of each text section, in order (empty sections hash to the empty-input digest).
constexpr uint8_t kGLME01TextSha1[7][20] = {
    {0x46, 0xea, 0x6f, 0x5e, 0x2d, 0xad, 0xe6, 0x91, 0x53, 0xe2, 0x42, 0x1d, 0x53, 0xbb, 0x6c, 0x2d, 0x7a, 0x16, 0x60, 0x45},
    {0xdd, 0xf5, 0x1e, 0x94, 0x48, 0xeb, 0x93, 0x13, 0x12, 0x41, 0xb5, 0xcb, 0x5a, 0xc3, 0xb4, 0x3e, 0x20, 0x74, 0xf1, 0xe7},
    {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09},
    {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09},
    {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09},
    {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09},
    {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09},
};

// SHA-1 of the retail GLMP01 rev 0 main.dol (decomp/config/GLMP01_00/build.sha1). No table yet:
// a PAL main.dol is accepted only when it is byte-identical to this one.
constexpr uint8_t kShaGLMP01[20] = {0xa1, 0xe2, 0xbd, 0x82, 0x81, 0x37, 0x87, 0xcf, 0xe0, 0xb1,
                                    0x1a, 0xfb, 0x5d, 0x13, 0x8f, 0x08, 0xfd, 0xbd, 0x94, 0xf1};

uint32_t read_be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

// True when the section table, bss, entry point and text sections equal the retail GLME01 layout.
// Data sections are not compared: a translation may change them (for example the dialogue font).
bool matchesGLME01CodeAndLayout(const uint8_t* dol, size_t size) {
    for (size_t i = 0; i < 18; ++i) {
        const Section& s = kGLME01Sections[i];
        if (read_be32(dol + i * 4) != s.fileOffset || read_be32(dol + 0x48 + i * 4) != s.addr ||
            read_be32(dol + 0x90 + i * 4) != s.size) {
            return false;
        }
    }
    if (read_be32(dol + 0xD8) != kGLME01Bss[0] || read_be32(dol + 0xDC) != kGLME01Bss[1] ||
        read_be32(dol + 0xE0) != kGLME01Entry) {
        return false;
    }
    for (size_t i = 0; i < 7; ++i) {
        const Section& s = kGLME01Sections[i];
        if (uint64_t(s.fileOffset) + s.size > size) return false;
        const auto digest = sha1(dol + s.fileOffset, s.size);
        if (std::memcmp(digest.data(), kGLME01TextSha1[i], 20) != 0) return false;
    }
    return true;
}

}  // namespace

// Minimal SHA-1 (FIPS 180-1) used to identify the retail DOL and to key setup's caches.
std::array<uint8_t, 20> sha1(const uint8_t* data, size_t len) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    const uint64_t bitLen = uint64_t(len) * 8;
    const size_t padded = ((len + 8) / 64 + 1) * 64;
    auto byteAt = [&](size_t i) -> uint8_t {
        if (i < len) return data[i];
        if (i == len) return 0x80;
        if (i >= padded - 8) return uint8_t(bitLen >> (8 * (padded - 1 - i)));
        return 0;
    };
    for (size_t chunk = 0; chunk < padded; chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (uint32_t(byteAt(chunk + i * 4)) << 24) | (uint32_t(byteAt(chunk + i * 4 + 1)) << 16) |
                   (uint32_t(byteAt(chunk + i * 4 + 2)) << 8) | byteAt(chunk + i * 4 + 3);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            const uint32_t t = std::rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = std::rotl(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::array<uint8_t, 20> out{};
    for (int i = 0; i < 5; ++i) {
        out[i * 4] = uint8_t(h[i] >> 24);
        out[i * 4 + 1] = uint8_t(h[i] >> 16);
        out[i * 4 + 2] = uint8_t(h[i] >> 8);
        out[i * 4 + 3] = uint8_t(h[i]);
    }
    return out;
}

// Accepts the retail GLMP01 DOL exactly, or a GLME01 DOL whose code and layout match retail.
Version identify(const uint8_t* dol, size_t size) {
    if (!dol || size < 0x100) return Version::Unknown;
    if (matchesGLME01CodeAndLayout(dol, size)) return Version::GLME01;
    const auto digest = sha1(dol, size);
    if (std::memcmp(digest.data(), kShaGLMP01, 20) == 0) return Version::GLMP01;
    return Version::Unknown;
}

}  // namespace port::dol
