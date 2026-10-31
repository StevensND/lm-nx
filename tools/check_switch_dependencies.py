"""Actual compilation probes for local dependencies. Does not download dependencies or enable Dawn. Writes only under build/fase4. Header compilation alone does not prove the library links for Horizon."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from prepare_aurora_switch import RAIZ
from prepare_aurora_native import preparar_nativo
from local_dependencies import includes_locales


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--devkitpro", type=Path, default=Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")))
    p.add_argument("--include", type=Path, action="append", default=[],
                   help="Additional local root, such as standalone Tint sources")
    args = p.parse_args()
    salida = RAIZ / "build/fase4/dependencias"
    salida.mkdir(parents=True, exist_ok=True)
    aurora = preparar_nativo()
    cc = args.devkitpro / "devkitA64/bin/aarch64-none-elf-g++.exe"
    env = os.environ.copy()
    env["TMP"] = env["TEMP"] = str(salida)
    includes = [aurora / "include", aurora / "include/dolphin", aurora / "lib",
                args.devkitpro / "libnx/include", args.devkitpro / "portlibs/switch/include",
                RAIZ / "LMNX-Deko3D/third_party/xxhash", RAIZ / "platforms/switch/nod",
                RAIZ / "LMNX-Deko3D/include",
                RAIZ / "platforms/switch/sdl3_headers/SDL3", *includes_locales(), *args.include]
    sondas = {
        "fmt": "fmt/format.h",
        "abseil": "absl/container/flat_hash_map.h",
        "imgui": "imgui.h",
        "tint": "src/tint/api/tint.h",
        "tracy": "tracy/Tracy.hpp",
        "productor_aurora_nativo": "gfx/recording.hpp",
        "aurora_hilos": str(aurora / "lib/thread.cpp").replace("\\", "/"),
        "aurora_dvd": str(aurora / "lib/dolphin/dvd/dvd.cpp").replace("\\", "/"),
    }
    registros = []
    for nombre, cabecera in sondas.items():
        fuente = salida / (nombre + ".cpp")
        fuente.write_text('#include <' + cabecera + '>\n', encoding="utf-8")
        orden = [str(cc), "-std=c++20", "-D__SWITCH__=1", "-D_DEFAULT_SOURCE", "-DAURORA=1", "-DTARGET_PC=1", "-DFMT_HEADER_ONLY=1", "-fsyntax-only",
                 *["-I" + str(i) for i in includes], str(fuente)]
        r = subprocess.run(orden, env=env, cwd=RAIZ, capture_output=True)
        (salida / (nombre + ".log")).write_bytes(r.stdout + r.stderr)
        registros.append({"nombre": nombre, "cabecera": cabecera,
                          "codigo": r.returncode, "orden": orden, "log": nombre + ".log"})
        print(nombre + ": " + ("header available" if r.returncode == 0 else "FAILED"))
    informe = {
        "nota": "Header availability; does not verify native Aurora integration or linkage",
        "sondas": registros,
        "iso_predeterminado_disponible": (RAIZ / "orig/GLME01.iso").is_file(),
        "modulo_aot_real_disponible": (RAIZ / "build/glme01-generado/dol.sha1").is_file(),
    }
    (salida / "dependencias.json").write_text(json.dumps(informe, ensure_ascii=False, indent=2), encoding="utf-8")
    return int(any(s["codigo"] for s in registros))


if __name__ == "__main__":
    raise SystemExit(main())
