"""PAL structural equivalence and FPR/PS differential checks without running the game."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

RAIZ = Path(__file__).resolve().parents[1]
SALIDA = RAIZ / "build/check-fp-locals"
PAL = RAIZ / "build/glmp01-locales-todos-57"
BASE = RAIZ / "build/glmp01-locales-todos-55"
CC = Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"))


def expandir_single(linea):
    m = re.fullmatch(r"    c.setSingle\((\d+), (.*)\);", linea)
    if not m:
        return linea
    d, expr = m.groups()
    return (f"    {{ const float lmnx_single = {expr}; c.f[{d}] = lmnx_single; "
            f"c.ps1[{d}] = lmnx_single; }}")


def main():
    SALIDA.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = str(CC.parent) + os.pathsep + env["PATH"]
    env["TMP"] = env["TEMP"] = str(SALIDA)

    def ejecutar(*args):
        subprocess.run([str(x) for x in args], cwd=RAIZ, env=env, check=True)

    # Same PAL preparation as the FIFO test; never write to the reference.
    archivos = ["funcs.h", "func_table.cpp"] + [f"funcs_{i:03}.cpp" for i in range(32)]
    for nombre in archivos:
        p = PAL / nombre
        p.write_text(p.read_text(encoding="utf-8").replace("namespace recomp {", "namespace recomp::pal {"), encoding="utf-8", newline="\n")
    for nombre in ["dol.sha1", "version.txt"]:
        shutil.copy2(BASE / nombre, PAL / nombre)

    inventario, checks = [], []
    for nombre in archivos:
        base = (BASE / nombre).read_text(encoding="utf-8").splitlines()
        apagado = (SALIDA / "desactivado" / nombre).read_text(encoding="utf-8").replace("namespace recomp {", "namespace recomp::pal {").splitlines()
        assert base == apagado, f"Option 0 differs from the baseline: {nombre}"
        opt = (PAL / nombre).read_text(encoding="utf-8").splitlines()
        b = o = 0
        funcion = ""
        while o < len(opt):
            if opt[o].startswith("void f_"):
                funcion = opt[o].split("(")[0].split()[-1]
            if opt[o] != "    { // LMNX: local FPR/PS region":
                assert opt[o] == base[b], (nombre, o+1, opt[o], base[b])
                o += 1
                b += 1
                continue
            inicio = o
            o += 1
            cargas = {}
            while (m := re.fullmatch(r"    double (lmnx_fp\d+) = (c\.(?:f|ps1)\[\d+\]);", opt[o])):
                cargas[m[1]] = m[2]
                o += 1
            assert opt[o].startswith("    bool lmnx_fp_ok = ")
            o += 1
            assert opt[o] == "    if (lmnx_fp_ok) {"
            o += 1
            cuerpo = []
            while not opt[o].startswith("    lmnx_fp_ok = "):
                cuerpo.append(opt[o])
                o += 1
            o += 1
            assert opt[o] == "    if (lmnx_fp_ok) {"
            o += 1
            volcados = []
            while (m := re.fullmatch(r"    (c\.(?:f|ps1)\[\d+\]) = (lmnx_fp\d+);", opt[o])):
                assert cargas[m[2]] == m[1]
                volcados.append(m[1])
                o += 1
            assert opt[o:o+3] == ["    }", "    }", "    if (!lmnx_fp_ok) { // LMNX: nonfinite fallback"]
            o += 3
            assert opt[o:o+len(cuerpo)] == base[b:b+len(cuerpo)]
            o += len(cuerpo)
            assert opt[o] == "    }"
            o += 1
            assert opt[o] == "    } // LMNX: end FPR/PS region"
            o += 1
            anteriores = base[b:b+len(cuerpo)]
            for viejo, nuevo in zip(anteriores, cuerpo):
                reconstruido = re.sub(r"lmnx_fp\d+", lambda m: cargas[m[0]], nuevo)
                assert reconstruido == expandir_single(viejo), (nombre, b+1, reconstruido, viejo)
            # Flush every original write; do not write read-only registers.
            expandido = "\n".join(expandir_single(x) for x in anteriores)
            destinos = set(re.findall(r"(c\.(?:f|ps1)\[\d+\]) = ", expandido))
            assert destinos == set(volcados)
            referencias = len(re.findall(r"c\.(?:f|ps1)\[\d+\]", expandido))
            inventario.append(dict(archivo=nombre, funcion=funcion, linea=inicio+1,
                                   instrucciones=len(cuerpo), referencias=referencias,
                                   transferencias=len(cargas)+len(volcados)))
            checks.append((anteriores, opt[inicio:o]))
            b += len(cuerpo)
        assert b == len(base)

    # Compile all 1,942 actual blocks with the actual Ctx. Execute only these expressions on synthetic states
    # without game memory or executable game code.
    partes = ['#include "recomp/ctx.h"\n#include <cstring>\n#include <cstdio>\n#include <cstdint>\nusing namespace recomp;\n']
    for i, (base, opt) in enumerate(checks):
        for prefijo, cuerpo in [("base", base), ("opt", opt)]:
            partes.append(f"__attribute__((noinline)) void {prefijo}{i}(Ctx& c) {{\n" + "\n".join(cuerpo) + "\n}\n")
    partes.append("using Fn=void(*)(Ctx&);\nFn bases[]={" + ",".join(f"base{i}" for i in range(len(checks))) + "};\n")
    partes.append("Fn opts[]={" + ",".join(f"opt{i}" for i in range(len(checks))) + "};\n")
    partes.append(r'''
uint64_t semilla=0x429b3177;
uint64_t azar(){semilla^=semilla<<13;semilla^=semilla>>7;semilla^=semilla<<17;return semilla;}
int main(){
    const uint64_t bordes[]={0,0x8000000000000000ull,0x7ff0000000000000ull,0xfff0000000000000ull,
      0x7ff8000000000042ull,0xfff8000000001234ull,0x7ff0000000000001ull,1,0x0010000000000000ull,
      0x3ff0000000000000ull,0xbff0000000000000ull,0x7fefffffffffffffull,0x36a0000000000000ull};
    unsigned comparaciones=0;
    for(unsigned i=0;i<sizeof(bases)/sizeof(*bases);++i)for(unsigned k=0;k<256;++k){
      Ctx a{};for(int r=0;r<32;++r){uint64_t x=k<64?bordes[(k+r)%13]:azar();
        uint64_t y=k<64?bordes[(k*3+r)%13]:azar();std::memcpy(&a.f[r],&x,8);std::memcpy(&a.ps1[r],&y,8);}
      Ctx b;std::memcpy(&b,&a,sizeof(a));bases[i](a);opts[i](b);
      if(std::memcmp(&a,&b,sizeof(a))){std::printf("Diferencia bloque %u estado %u\n",i,k);return 1;}++comparaciones;
    }std::printf("%u comparaciones exactas de Ctx; %zu regiones\n",comparaciones,sizeof(bases)/sizeof(*bases));
}
''')
    (SALIDA / "diferencial.cpp").write_text("".join(partes), encoding="utf-8")
    ejecutar(CC, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off", "-Isrc", SALIDA / "diferencial.cpp", "-o", SALIDA / "diferencial.exe")
    ejecutar(SALIDA / "diferencial.exe")
    informe = dict(funciones=len({x["funcion"] for x in inventario}), regiones=len(inventario),
                   referencias=sum(x["referencias"] for x in inventario), transferencias=sum(x["transferencias"] for x in inventario),
                   archivos_comparados=len(archivos), opcion_0_identica_55=True,
                   comparaciones_ctx=len(checks)*256, regiones_detalle=inventario,
                   sha256_generados={n:hashlib.sha256((PAL/n).read_bytes()).hexdigest() for n in archivos})
    (PAL / "fpr-locales.json").write_text(json.dumps(informe, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({k:v for k,v in informe.items() if k not in ("regiones_detalle", "sha256_generados")}, ensure_ascii=False))


if __name__ == "__main__":
    main()
