"""Compare every optimized retail C++ leaf without running the game."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
from prepare_glme01 import RAIZ


def funciones(carpeta):
    resultado = {}
    for p in sorted(carpeta.glob("funcs_*.cpp")):
        for m in re.finditer(r"^void (f_[0-9A-F]+(?:_orig)?)\(Ctx& c\) \{\n.*?^\}", p.read_text(encoding="utf-8"), re.M | re.S):
            if m[1] in resultado:
                raise ValueError("Duplicate function: " + m[1])
            resultado[m[1]] = m[0]
    return resultado


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", choices=["GLME01", "GLMP01"], default="GLME01")
    parser.add_argument("--lto",action="store_true",help="Also check Ctx pairs under host compiler LTO")
    parser.add_argument("--opt",type=Path,help="Build directory of the variant to compare")
    args = parser.parse_args()
    prefijo_version = args.version.lower()
    base = RAIZ / f"build/{prefijo_version}-generado"
    opt = args.opt.resolve() if args.opt else RAIZ / f"build/{prefijo_version}-locales"
    if not opt.is_relative_to(RAIZ/"build"):raise ValueError("Generated AOT outside build/")
    salida = RAIZ / ("build/fase4/codegen-retail-pal" if args.version == "GLMP01" else "build/fase4/codegen-retail")
    if args.lto:salida=salida.with_name(salida.name+"-lto")
    salida.mkdir(parents=True, exist_ok=True)
    for nombre in ["funcs.h", "func_table.cpp", "dol.sha1"]:
        assert (base / nombre).read_bytes() == (opt / nombre).read_bytes(), nombre
    a, b = funciones(base), funciones(opt)
    assert a.keys() == b.keys() and len(a) > 11000
    cambios = [n for n in a if a[n] != b[n]]
    assert cambios and all("lmnx_r" in b[n] and "lmnx_r" not in a[n] for n in cambios)
    for prefijo, fs in [("base_", a), ("opt_", b)]:
        texto = '#include "recomp/ctx.h"\nnamespace recomp {\n'
        texto += "\n".join(fs[n].replace("void " + n + "(", "void " + prefijo + n + "(") for n in cambios)
        (salida / (prefijo + ".cpp")).write_text(texto + "\n}\n", encoding="utf-8")
    # Every Ctx byte is observable. Keep enter() executing; disable tracing, watches and IRQs for this integer-leaf
    # comparison.
    arnes = r'''
#include "recomp/ctx.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
namespace recomp {
bool g_traceOn=false, g_watchOn=false;
void trace_enter(u32,Ctx&) { std::abort(); }
@DECLARACIONES@
}
int main() {
 using namespace recomp;
 struct Par { const char* nombre; void(*a)(Ctx&); void(*b)(Ctx&); };
 const Par pares[] = { @PARES@ };
 u32 azar=0x43e47;
 auto rnd=[&] { azar^=azar<<13; azar^=azar>>17; azar^=azar<<5; return azar; };
 for (const auto& par:pares) for(unsigned i=0;i<1024;++i) {
  Ctx a{},b{};
  auto* bytes=reinterpret_cast<unsigned char*>(&a);
  for(size_t k=0;k<sizeof a;++k) bytes[k]=static_cast<unsigned char>(rnd());
  if(i<4) for(auto& r:a.r) r=i==0?0:i==1?0xffffffff:i==2?0x80000000:0x7fffffff;
  std::memcpy(&b,&a,sizeof a);
  par.a(a); par.b(b);
  if(std::memcmp(&a,&b,sizeof a)) { std::fprintf(stderr,"Ctx distinto: %s estado %u\n",par.nombre,i); return 1; }
 }
 std::printf("%zu hojas retail: 1024 estados Ctx identicos por hoja\n",sizeof pares/sizeof *pares);
}
'''
    arnes = arnes.replace("@DECLARACIONES@", "\n".join(f"void base_{n}(Ctx&); void opt_{n}(Ctx&);" for n in cambios))
    arnes = arnes.replace("@PARES@", ",\n".join(f'{{"{n}",base_{n},opt_{n}}}' for n in cambios))
    (salida / "test.cpp").write_text(arnes, encoding="utf-8")
    cc = Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"))
    env = os.environ.copy()
    env["PATH"] = str(cc.parent) + os.pathsep + env["PATH"]
    env["TMP"] = env["TEMP"] = str(salida)
    orden = [str(cc), "-std=c++20", "-O2", "-static", "-I" + str(RAIZ / "src"),
             *[str(salida / n) for n in ["base_.cpp", "opt_.cpp", "test.cpp"]], "-o", str(salida / "test.exe")]
    if args.lto:orden.insert(1,"-flto")
    r = subprocess.run(orden, cwd=RAIZ, env=env, capture_output=True)
    (salida / "compilacion.log").write_bytes(r.stdout + r.stderr)
    r.check_returncode()
    r = subprocess.run([str(salida / "test.exe")], cwd=RAIZ, env=env, capture_output=True)
    (salida / "test.log").write_bytes(r.stdout + r.stderr)
    informe = {"version": args.version, "lto":args.lto,"codigo": r.returncode, "hojas": len(cambios), "estados_por_hoja": 1024,
               "comparaciones": len(cambios) * 1024, "funciones_sin_cambios": len(a) - len(cambios),
               "abi_y_tablas_identicos": True, "consola": False, "ganancia_medida": False,
               "funciones": cambios}
    (salida / "informe.json").write_text(json.dumps(informe, indent=2), encoding="utf-8")
    print(r.stdout.decode(errors="replace"), end="")
    r.check_returncode()


if __name__ == "__main__":
    main()
