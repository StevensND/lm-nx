"""Print the retail layout constants that src/port/dol_identify.cpp embeds for one game ID.

Usage: python -I -X utf8 tools/dol_reference.py <retail main.dol>

Prints the 18-entry section table (file offset, address, size), the bss address and size,
the entry point and the SHA-1 of each of the 7 text sections. It never prints DOL bytes.
"""
import hashlib
import struct
import sys

TEXT_SECTIONS = 7
DATA_SECTIONS = 11


def main():
    with open(sys.argv[1], "rb") as f:
        dol = f.read()
    offsets = struct.unpack(">18I", dol[0x00:0x48])
    addrs = struct.unpack(">18I", dol[0x48:0x90])
    sizes = struct.unpack(">18I", dol[0x90:0xD8])
    bss_addr, bss_size, entry = struct.unpack(">III", dol[0xD8:0xE4])
    print("// Section table: {file offset, address, size}; 0-6 text, 7-17 data.")
    print("constexpr Section kSections[18] = {")
    for i in range(TEXT_SECTIONS + DATA_SECTIONS):
        name = f"text{i}" if i < TEXT_SECTIONS else f"data{i - TEXT_SECTIONS}"
        print(f"    {{0x{offsets[i]:08x}, 0x{addrs[i]:08x}, {sizes[i]}}},  // {name}")
    print("};")
    print(f"constexpr uint32_t kBss[2] = {{0x{bss_addr:08x}, {bss_size}}};")
    print(f"constexpr uint32_t kEntry = 0x{entry:08x};")
    print("// SHA-1 of each text section, in order (empty sections hash to the empty-input digest).")
    print("constexpr uint8_t kTextSha1[7][20] = {")
    for i in range(TEXT_SECTIONS):
        data = dol[offsets[i]:offsets[i] + sizes[i]]
        digest = hashlib.sha1(data).digest()
        body = ", ".join(f"0x{b:02x}" for b in digest)
        print(f"    {{{body}}},  // text{i}")
    print("};")


main()
