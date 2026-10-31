#!/usr/bin/env python3
"""Native differential test using synthetic PPC instructions without game data."""
import os
import argparse
import re
from pathlib import Path
import struct
import subprocess

RAIZ = Path(__file__).resolve().parents[1]
SALIDA = RAIZ / "build/check-codegen"
CC = Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"))
ENTORNO = os.environ.copy()
ENTORNO["PATH"] = str(CC.parent) + os.pathsep + ENTORNO["PATH"]

def ejecutar(args):
    subprocess.run([str(x) for x in args], cwd=RAIZ, env=ENTORNO, check=True)

def main():
    global SALIDA
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--lto",action="store_true");args=parser.parse_args()
    if args.lto:SALIDA=RAIZ/"build/check-codegen-lto"
    SALIDA.mkdir(parents=True, exist_ok=True)
    ENTORNO["TMP"] = ENTORNO["TEMP"] = str(SALIDA)
    herramienta = SALIDA / "lmrecompile.exe"
    ejecutar([CC, "-std=c++20", "-O2", "-Isrc", "src/recompiler/recompiler.cpp", "tools/recomp/lmrecompile.cpp", "-o", herramienta])
    # addi, ori, xoris, addis, blr. All output registers are observable.
    hoja = [0x3864FFFF, 0x60651234, 0x6CA68000, 0x3CE6FFFF, 0x4E800020]
    flags = [0x70630001, 0x4E800020]  # andi. updates CR: cannot use a local copy.
    memoria = [0x80640000, 0x4E800020]  # lwz: may access MMIO.
    funciones = [("hoja", hoja), ("con_hook", hoja), ("con_patch", hoja),
                 ("con_flags", flags), ("con_memoria", memoria)]
    candidatos=[0]
    for xo in [28,60,444,412,316,476,124,284,24,536,26,922,954,266,40,104,235,75,11]:
        rb=0 if xo in [26,922,954,104] else 3
        candidatos.append(len(funciones));funciones.append((f"entero_{xo}",[(31<<26)|(5<<21)|(4<<16)|(rb<<11)|(xo<<1),0x4e800020]))
    for op in [20,21,23]:
        candidatos.append(len(funciones));funciones.append((f"rotar_{op}",[(op<<26)|(5<<21)|(4<<16)|(3<<11)|(5<<6)|(27<<1),0x4e800020]))
    funciones += [("con_rc",[(31<<26)|(5<<21)|(4<<16)|(3<<11)|(316<<1)|1,0x4e800020]),
                  ("con_oe",[(31<<26)|(5<<21)|(4<<16)|(3<<11)|(266<<1)|0x400,0x4e800020])]
    cuerpo = bytearray()
    mapa = []
    direcciones = []
    for nombre, palabras in funciones:
        direccion = 0x80001000 + len(cuerpo)
        direcciones.append(direccion)
        mapa.append(f"{nombre} = .text:0x{direccion:08X}; // type:function size:0x{len(palabras)*4:X}")
        cuerpo.extend(b"".join(struct.pack(">I", x) for x in palabras))
    cabecera = bytearray(0x100)
    for offset, valor in [(0, 0x100), (0x48, 0x80001000), (0x90, len(cuerpo))]:
        struct.pack_into(">I", cabecera, offset, valor)
    (SALIDA / "sintetico.dol").write_bytes(cabecera + cuerpo)
    (SALIDA / "simbolos.txt").write_text("\n".join(mapa), encoding="utf-8")
    (SALIDA / "hooks.txt").write_text(f"con_hook\npatch 0 {direcciones[2]:08X} 38640001\n", encoding="utf-8")
    for nombre, activo in [("base", "0"), ("locales", "1")]:
        destino = SALIDA / nombre
        ejecutar([herramienta, "--dol", SALIDA / "sintetico.dol", "--symbols", SALIDA / "simbolos.txt",
                  "--hooks", SALIDA / "hooks.txt", "--out", destino, "--switch-integer-locals", activo])
        resumen = (destino / "summary.txt").read_text()
        if activo == "1":
            assert f"{len(candidatos)} hojas enteras" in resumen, resumen
            codigo="\n".join(p.read_text(encoding="utf-8") for p in destino.glob("funcs_*.cpp"))
            for i,direccion in enumerate(direcciones):
                m=re.search(rf"void f_{direccion:08X}\(Ctx& c\) \{{\n.*?^\}}",codigo,re.M|re.S)
                assert m and (("lmnx_r" in m[0])==(i in candidatos))
        prefijo = "b_" if activo == "0" else "o_"
        header = (destino / "funcs.h").read_text().replace("#pragma once", "").replace("f_800", prefijo + "f_800")
        textos = [header]
        for fuente in sorted(destino.glob("funcs_*.cpp")):
            textos.append(fuente.read_text().replace('#include "funcs.h"', "").replace("f_800", prefijo + "f_800"))
        (SALIDA / f"{nombre}.cpp").write_text("\n".join(textos), encoding="utf-8")
    test_source = r'''
#include "recomp/ctx.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
namespace recomp {
bool g_traceOn = false, g_watchOn = false;
u8* g_mem = nullptr;
u8 switches[16]{}; u8* g_patches = switches;
bool irq = false; bool* g_irqPending = &irq;
void trace_enter(u32, Ctx& c) {c.r[5]^=0x87654321;}
void service_irq(Ctx& c) {c.r[4]^=0x12345678;}
void watch_store(u32,u32,int) { std::abort(); }
u32 mmio_rd(u32,int) { std::abort(); }
void mmio_wr(u32,u32,int) { std::abort(); }
void Ctx::call(u32) { std::abort(); }
[[noreturn]] void Ctx::unimplemented(u32,u32) { std::abort(); }
HleFn hleLookup(const char*) { return nullptr; }
void hleMissing(const char*,u32,Ctx&) { std::abort(); }
@DECLARACIONES@
}
int main() {
    using namespace recomp;
    u32 azar = 0x43E47u;
    auto rnd = [&] { azar ^= azar << 13; azar ^= azar >> 17; azar ^= azar << 5; return azar; };
    struct Par {void(*base)(Ctx&);void(*opt)(Ctx&);};const Par pares[]={@PARES@};
    for (int i=0; i<100000; ++i) {
        Ctx original{};
        for (auto& r : original.r) r = rnd();
        for (auto& cr : original.cr) cr = rnd() & 15;
        original.lr=rnd(); original.ctr=rnd(); original.ca=rnd()&1;
        original.so=rnd()&1; original.ov=rnd()&1; original.fpscr=rnd();
        if(i<64){original.r[3]=i;original.r[4]=i;original.r[5]=~u32(i);}
        irq=(i&1)!=0;g_traceOn=(i&2)!=0;
        for(auto par:pares){
            Ctx a,b; std::memcpy(&a,&original,sizeof a); std::memcpy(&b,&original,sizeof b);
            par.base(a);par.opt(b);
            if (std::memcmp(&a,&b,sizeof a)) return 1;
        }
        const u32 v = rnd(), n = rnd();
        const u32 rotado = Ctx::rotl(v,n);
        u32 esperado = v;
        for (unsigned s=0; s<(n&31); ++s) esperado=(esperado<<1)|(esperado>>31);
        if (rotado != esperado) return 2;
    }
    std::puts("100000 estados Ctx identicos; rotaciones identicas; hooks/patches/CR/MMIO excluidos.");
}
'''
    nombres=[f"f_{direcciones[i]:08X}" for i in candidatos]
    test_source=test_source.replace("@DECLARACIONES@","\n".join(f"void b_{n}(Ctx&);void o_{n}(Ctx&);" for n in nombres)).replace("@PARES@",",".join(f"{{b_{n},o_{n}}}" for n in nombres))
    (SALIDA / "test.cpp").write_text(test_source, encoding="utf-8")
    exe = SALIDA / "test.exe"
    ejecutar([CC, "-std=c++20", "-O2", *(["-flto"] if args.lto else []), "-ffunction-sections", "-fdata-sections", "-Isrc",
              SALIDA / "base.cpp", SALIDA / "locales.cpp", SALIDA / "test.cpp", "-Wl,--gc-sections", "-o", exe])
    ejecutar([exe])

if __name__ == "__main__":
    main()
