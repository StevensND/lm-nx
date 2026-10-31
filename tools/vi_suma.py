# Canonical little-endian XXH64 with seed zero, reading in blocks, matching XXH64_update per plane in
# platforms/switch/vi_ajuste.cpp and the project's xxhash library. Used only by tools/build_switch_direct.py to
# set lmnx_vi_esperada.
import struct

P1 = 0x9E3779B185EBCA87
P2 = 0xC2B2AE3D27D4EB4F
P3 = 0x165667B19E3779F9
P4 = 0x85EBCA77C2B2AE63
P5 = 0x27D4EB2F165667C5
M = (1 << 64) - 1


def _rol(x, r):
    return ((x << r) | (x >> (64 - r))) & M


def _round(acc, lane):
    acc = (acc + (lane * P2 & M)) & M
    acc = _rol(acc, 31)
    return acc * P1 & M


def _merge(acc, val):
    val = _round(0, val)
    acc = (acc ^ val) & M
    return (acc * P1 + P4) & M


def xxh64_por_bloques(ruta, tam_bloque):
    """XXH64 seed zero over the first N*tam_bloque bytes, where N is the number of complete blocks."""
    with open(ruta, "rb") as f:
        datos = f.read()
    total = len(datos) // tam_bloque
    datos = datos[:total * tam_bloque]
    n = len(datos)
    p = 0
    if n >= 32:
        v1 = (P1 + P2) & M
        v2 = P2
        v3 = 0
        v4 = (0 - P1) & M
        while p + 32 <= n:
            v1 = _round(v1, struct.unpack_from("<Q", datos, p)[0]); p += 8
            v2 = _round(v2, struct.unpack_from("<Q", datos, p)[0]); p += 8
            v3 = _round(v3, struct.unpack_from("<Q", datos, p)[0]); p += 8
            v4 = _round(v4, struct.unpack_from("<Q", datos, p)[0]); p += 8
        h = (_rol(v1, 1) + _rol(v2, 7) + _rol(v3, 12) + _rol(v4, 18)) & M
        h = _merge(h, v1)
        h = _merge(h, v2)
        h = _merge(h, v3)
        h = _merge(h, v4)
    else:
        h = P5
    h = (h + n) & M
    while p + 8 <= n:
        h ^= _round(0, struct.unpack_from("<Q", datos, p)[0])
        h = (_rol(h, 27) * P1 + P4) & M
        p += 8
    if p + 4 <= n:
        h ^= (struct.unpack_from("<I", datos, p)[0] * P1) & M
        h = (_rol(h, 23) * P2 + P3) & M
        p += 4
    while p < n:
        h ^= (datos[p] * P5) & M
        h = (_rol(h, 11) * P1) & M
        p += 1
    h ^= h >> 33
    h = (h * P2) & M
    h ^= h >> 29
    h = (h * P3) & M
    h ^= h >> 32
    return h
