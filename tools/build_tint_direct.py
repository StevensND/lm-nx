"""Direct host Tint build: WGSL/GLSL without Dawn runtime or backends."""
import argparse
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess
from local_dependencies import RAIZ, fuente
from prepare_tint_switch import preparar


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--jobs", type=int, default=6)
    p.add_argument("--configurar", action="store_true", help="Only evaluate the source manifest")
    p.add_argument("--switch", action="store_true", help="devkitA64 Tint/translator library without CLI or Dawn runtime")
    args = p.parse_args()
    salida = RAIZ / ("build/tint-switch" if args.switch else "build/tint-directo")
    salida.mkdir(parents=True, exist_ok=True)
    tint = fuente("dawn")
    overlay = preparar()
    cmake = Path(os.environ.get("CMAKE", "cmake"))
    manifiesto = salida / "fuentes.tsv"
    # -P evaluates original conditionals only. g++ checks each source; no Ninja, Dawn generators or backends are
    # run.
    subprocess.run([str(cmake), "-DTINT_ROOT=" + tint.as_posix(), "-DSALIDA=" + manifiesto.as_posix(), "-DTINT_BUILD_IS_WIN=" + ("OFF" if args.switch else "ON"),
                    "-P", str(RAIZ / "tools/tint_manifest.cmake")], cwd=tint / "src/tint", check=True)
    fuentes, dependencias, externos = (defaultdict(list) for _ in range(3))
    for linea in manifiesto.read_text(encoding="utf-8").splitlines():
        k, nombre, lista = linea.split("|")
        {"S": fuentes, "D": dependencias, "E": externos}[k][nombre] += [s for s in lista.split(";") if s]
    visitados = set()
    def recorrer(nombre):
        if nombre in visitados:
            return
        if nombre not in fuentes:
            raise ValueError("Objetivo Tint no disponible: " + nombre)
        visitados.add(nombre)
        for dep in dependencias[nombre]:
            recorrer(dep)
    for nombre in ("tint_api", "tint_lang_wgsl_reader", "tint_lang_wgsl_inspector", "tint_lang_glsl_writer"):
        recorrer(nombre)
    ext = {s for nombre in visitados for s in externos[nombre]}
    if ext - {"abseil", "src_utils"}:
        raise ValueError("New Tint dependencies are not configured: " + str(ext))
    lista = sorted({tint / "src/tint" / s for nombre in visitados for s in fuentes[nombre] if s.endswith(".cc")})
    lista = [overlay / s.relative_to(tint) if (overlay / s.relative_to(tint)).is_file() else s for s in lista]
    lista += [tint / "src/utils" / n for n in ("assert.cc", "log.cc", "placeholder.cc")]
    absl = fuente("abseil-cpp")
    lista += [absl / n for n in ("absl/strings/charconv.cc", "absl/strings/internal/charconv_bigint.cc",
              "absl/strings/internal/charconv_parse.cc", "absl/base/internal/raw_logging.cc", "absl/base/log_severity.cc",
              "absl/numeric/int128.cc", "absl/strings/internal/memutil.cc", "absl/strings/ascii.cc")]
    traductor = RAIZ / "LMNX-Deko3D/third_party/switchwaker_translate"
    lista += [traductor / "shader_translate.cpp"]
    if not args.switch:
        lista.append(RAIZ / "tools/wgsl_to_glsl.cpp")
    resumen = {"objetivos": sorted(visitados), "fuentes": [str(s.relative_to(RAIZ)) for s in lista],
               "externos": sorted(ext), "dawn_runtime": False, "ninja": False}
    (salida / "manifiesto.json").write_text(json.dumps(resumen, indent=2), encoding="utf-8")
    if args.configurar:
        return 0
    cc = Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "devkitA64/bin/aarch64-none-elf-g++.exe") if args.switch else str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"))
    env = os.environ.copy()
    env["PATH"] = str(cc.parent) + os.pathsep + env["PATH"]
    env["TMP"] = env["TEMP"] = str(salida)
    includes = [overlay, tint, tint / "include", absl, traductor]
    banderas = ["-std=c++20", "-O2", "-DNDEBUG", "-ffunction-sections", "-fdata-sections"]
    banderas += ["-I" + str(i) for i in includes]
    banderas += ["-I" + str(RAIZ / "LMNX-Deko3D/include")]
    activados = {"GLSL_WRITER", "WGSL_READER", "WGSL_WRITER"}
    if args.switch:
        banderas += ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-ftls-model=local-exec", "-fPIE", "-D__SWITCH__=1", "-ID:/devkitPro/libnx/include"]
    else:
        activados.add("IS_WIN")
    for n in ["GLSL_VALIDATOR", "GLSL_WRITER", "HLSL_WRITER", "IR_BINARY", "IS_LINUX", "IS_MAC", "IS_WIN",
              "MSL_WRITER", "SPV_READER", "SPV_WRITER", "WGSL_READER", "WGSL_WRITER", "NULL_WRITER", "TINTD"]:
        banderas.append(f"-DTINT_BUILD_{n}={int(n in activados)}")
    banderas += ["-DTINT_ENABLE_IR_DUMPING=0", "-DTINT_ENABLE_IR_VALIDATION_ASSERTS=0", "-DTINT_ENABLE_BREAK_IN_DEBUGGER=0"]
    cabecera = max(s.stat().st_mtime for i in includes for s in i.rglob("*.h"))
    def compilar(s):
        nombre = s.stem + "_" + hashlib.sha256(str(s).encode()).hexdigest()[:12]
        obj = salida / (nombre + ".o")
        orden = [str(cc), *banderas, "-c", str(s), "-o", str(obj)]
        extras = b""
        if s.name == "wgsl_to_glsl.cpp":
            extras = b"".join((RAIZ / "LMNX-Deko3D/include/lmnx" / n).read_bytes() for n in ("translation_cache.hpp","cache_format.hpp","shader_source.hpp","dksh.hpp"))
        clave = hashlib.sha256((json.dumps(orden) + hashlib.sha256(s.read_bytes()+extras).hexdigest()).encode()).hexdigest()
        sello = obj.with_suffix(".sha256")
        vigente = obj.is_file() and sello.is_file() and sello.read_text() == clave and obj.stat().st_mtime >= cabecera
        codigo = 0
        if not vigente:
            r = subprocess.run(orden, cwd=RAIZ, env=env, capture_output=True)
            obj.with_suffix(".log").write_bytes(r.stdout + r.stderr)
            codigo = r.returncode
            if not codigo:
                sello.write_text(clave)
        return {"fuente": str(s.relative_to(RAIZ)), "objeto": str(obj), "codigo": codigo, "reutilizado": vigente}
    print(f"Compiling Tint/translator: {len(lista)} units without Dawn runtime...", flush=True)
    resultados = []
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as ejecutor:
        for i, resultado in enumerate(ejecutor.map(compilar, lista), 1):
            resultados.append(resultado)
            if resultado["codigo"]:
                print("ERROR: " + resultado["fuente"], flush=True)
            elif i % 40 == 0:
                print(f"Tint {i}/{len(lista)}", flush=True)
    (salida / "resultados.json").write_text(json.dumps(resultados, indent=2), encoding="utf-8")
    if any(r["codigo"] for r in resultados):
        return 1
    if args.switch:
        biblioteca = salida / "liblmnx_tint.a"
        biblioteca.unlink(missing_ok=True)
        orden_ar = [str(cc.parent / "aarch64-none-elf-gcc-ar.exe"), "rcs", str(biblioteca), *[r["objeto"] for r in resultados]]
        respuesta_ar = salida / "archivo.rsp"
        respuesta_ar.write_text("\n".join('"' + x.replace("\\", "/") + '"' for x in orden_ar[1:]), encoding="utf-8")
        r = subprocess.run([orden_ar[0], "@" + str(respuesta_ar)], cwd=RAIZ, env=env, capture_output=True)
        (salida / "archivo.log").write_bytes(r.stdout + r.stderr)
        print("Biblioteca Tint Switch: " + str(r.returncode), flush=True)
        return r.returncode
    respuesta = salida / "enlace.rsp"
    respuesta.write_text("\n".join('"' + r["objeto"].replace("\\", "/") + '"' for r in resultados), encoding="utf-8")
    orden = [str(cc), "-static", "-Wl,--gc-sections", "@" + str(respuesta), "-o", str(salida / "wgsl_to_glsl.exe"),
             "-lpsapi", "-ldbghelp", "-lws2_32"]
    r = subprocess.run(orden, cwd=RAIZ, env=env, capture_output=True)
    (salida / "enlace.log").write_bytes(r.stdout + r.stderr)
    print("Tint translator link: " + str(r.returncode), flush=True)
    return r.returncode


if __name__ == "__main__":
    raise SystemExit(main())
