"""Compare AOT C++ ELF imports with actual runtime exports. Does not link, create stubs or treat a synthetic module as the game. Match mangled ABI names rather than demangled text."""
import os
import argparse
import json
from pathlib import Path
import re
import subprocess

RAIZ = Path(__file__).resolve().parents[1]


def simbolos(nm, biblioteca):
    r = subprocess.run([str(nm), "-g", "-P", str(biblioteca)],
                       capture_output=True, check=True)
    definidos, requeridos = set(), set()
    for linea in r.stdout.decode(errors="replace").splitlines():
        partes = linea.split()
        if len(partes) < 2 or len(partes[1]) != 1:
            continue  # Archive member header.
        nombre, tipo = partes[:2]
        if tipo == "U":
            requeridos.add(nombre)
        elif tipo not in ("w", "v", "?"):
            definidos.add(nombre)
    return definidos, requeridos


def auditar(biblioteca, runtime, devkitpro=os.environ.get("DEVKITPRO", "/opt/devkitpro"), sintetico=False, pal=False):
    nm = Path(devkitpro) / "devkitA64/bin/aarch64-none-elf-gcc-nm.exe"
    propios, requeridos = simbolos(nm, biblioteca)
    imports = requeridos - propios
    exports, proveedores = set(), {}
    for ruta in runtime:
        definidos, _ = simbolos(nm, ruta)
        exports.update(definidos)
        for nombre in sorted(definidos & imports):
            proveedores.setdefault(nombre, []).append(ruta.name)
    # libc/libm/libgcc may satisfy other imports at final link. Recompiler helpers must exist in this runtime
    # without DLLs.
    helpers = {n for n in imports if n.startswith(("_ZN6recomp", "_ZNK6recomp"))}
    pendientes = sorted(helpers - exports)
    directas = sorted(n for n in imports if re.match(r"_ZN6recomp(?:3pal)?\d+f_[0-9A-Fa-f]{8}(?:_orig)?E", n))
    modulo = ("_ZN6recomp3pal11kGameModuleE" if pal else "_ZN6recomp11kGameModuleE") in propios
    return {
        "nota": "Object ABI contract; does not verify full linkage or execution",
        "sintetico": sintetico,
        "version": "GLMP01" if pal else "GLME01",
        "biblioteca": str(biblioteca),
        "runtime": [str(p) for p in runtime],
        "exporta_kGameModule": modulo,
        "imports": sorted(imports),
        "proveedores_runtime": proveedores,
        "helpers_runtime_pendientes": pendientes,
        "llamadas_aot_sin_definicion": directas,
        "imports_de_otras_bibliotecas": sorted(imports - helpers - exports),
        "contrato_runtime_satisfecho": modulo and not pendientes and not directas,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--biblioteca", type=Path, required=True)
    p.add_argument("--runtime", type=Path, nargs="+", required=True)
    p.add_argument("--devkitpro", default=os.environ.get("DEVKITPRO", "/opt/devkitpro"))
    p.add_argument("--sintetico", action="store_true")
    p.add_argument("--informe", type=Path, default=RAIZ / "build/direct-switch/contrato-aot.json")
    args = p.parse_args()
    informe = auditar(args.biblioteca, args.runtime, args.devkitpro, args.sintetico)
    args.informe.parent.mkdir(parents=True, exist_ok=True)
    args.informe.write_text(json.dumps(informe, ensure_ascii=False, indent=2), encoding="utf-8")
    print("AOT contract " + ("synthetic" if args.sintetico else "specified") + ": " +
          ("satisfied" if informe["contrato_runtime_satisfecho"] else "INCOMPLETE"))
    return 0 if informe["contrato_runtime_satisfecho"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
