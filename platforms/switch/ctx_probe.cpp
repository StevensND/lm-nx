#include "recomp/ctx.h"
// Inspect REV/REV16 in actual devkitA64 assembly.
extern "C" recomp::u32 lmnx_leer_be32(recomp::u32 direccion) { return recomp::rd32(direccion); }
extern "C" recomp::u32 lmnx_leer_be16(recomp::u32 direccion) { return recomp::rd16(direccion); }
