"""Build host uam with the same Switch patches and flags, without an MSYS2 shell."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess
from uam_sources import fuentes, opciones

RAIZ = Path(__file__).resolve().parents[1]

def construir(jobs=6):
    salida = RAIZ / "build/shader-tools"
    salida.mkdir(parents=True, exist_ok=True)
    binarios = Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin"))
    entorno = os.environ.copy()
    entorno["PATH"] = str(binarios) + os.pathsep + entorno["PATH"]
    entorno["TMP"] = entorno["TEMP"] = str(salida)
    uam = RAIZ / "LMNX-Deko3D/third_party/uam"
    banderas = [b for b in opciones(uam) if b != "-DHAVE_POSIX_MEMALIGN"] + [
        "-DHAVE_STRNDUP=1", "-O2", "-I" + str(RAIZ / "LMNX-Deko3D/include")]
    cabeceras = max(p.stat().st_mtime for p in (RAIZ / "LMNX-Deko3D").rglob("*") if p.is_file() and p.suffix in (".h", ".hpp"))
    def ejecutar(orden, log):
        r = subprocess.run([str(x) for x in orden], cwd=RAIZ, env=entorno, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        log.write_bytes(r.stdout)
        if r.returncode: raise RuntimeError(f"Compilation error: {log}")
    def compilar(fuente):
        objeto = salida / (fuente.stem + "-" + hashlib.sha256(str(fuente).encode()).hexdigest()[:12] + ".o")
        es_c = fuente.suffix == ".c"
        orden = [binarios / ("gcc.exe" if es_c else "g++.exe"), *banderas, "-std=gnu99" if es_c else "-std=c++20", "-c", fuente, "-o", objeto]
        huella = hashlib.sha256((json.dumps([str(x) for x in orden]) + hashlib.sha256(fuente.read_bytes()).hexdigest()).encode()).hexdigest()
        sello = objeto.with_suffix(".json")
        if not (objeto.exists() and sello.exists() and sello.read_text() == huella and cabeceras <= objeto.stat().st_mtime):
            ejecutar(orden, objeto.with_suffix(".log")); sello.write_text(huella)
        return objeto
    with ThreadPoolExecutor(max_workers=jobs) as cola:
        objetos = list(cola.map(compilar, fuentes(uam) + [RAIZ / "tools/shaders/uam_cache.cpp"]))
    exe = salida / "uam_cache.exe"
    respuesta = salida / "enlace.rsp"
    respuesta.write_text("\n".join('"' + str(p).replace('\\', '/') + '"' for p in objetos), encoding="utf-8")
    ejecutar([binarios / "g++.exe", "@" + str(respuesta), "-static", "-o", exe], salida / "enlace.log")
    print(f"Host uam built: {exe}", flush=True)
    return exe

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__); p.add_argument("--jobs", type=int, default=6)
    construir(p.parse_args().jobs)
