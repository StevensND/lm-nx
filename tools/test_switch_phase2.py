"""Host backend tests with synthetic data, no disc or emulation."""
import json
import os
from pathlib import Path
import subprocess
import sys

RAIZ = Path(__file__).resolve().parents[1]

def main():
    salida = RAIZ / "build/check-switch-phase2"
    salida.mkdir(parents=True, exist_ok=True)
    binarios = Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin"))
    env = os.environ.copy(); env["PATH"] = str(binarios) + os.pathsep + env["PATH"]
    env["TMP"] = env["TEMP"] = str(salida)
    informe = {"consola": False, "datos": "Synthetic test data; no LM GameModule", "checks": []}
    def ejecutar(orden, nombre, esperado=0):
        r = subprocess.run([str(x) for x in orden], cwd=RAIZ, env=env, capture_output=True)
        log = r.stdout + r.stderr
        (salida / (nombre + ".log")).write_bytes(log)
        informe["checks"].append({"nombre": nombre, "codigo": r.returncode, "esperado": esperado})
        (salida / "resultados.json").write_text(json.dumps(informe, indent=2, ensure_ascii=False), encoding="utf-8")
        if r.returncode != esperado: raise RuntimeError(f"Test failed: {nombre}; see its log")
        print(log.decode(errors="replace").strip(), flush=True)
        return log
    for nombre in ("cache_format", "efb_pack", "audio_convert"):
        es_c = nombre == "audio_convert"
        fuente = RAIZ / "tools/tests" / (nombre + "_test" + (".c" if es_c else ".cpp"))
        exe = salida / (nombre + ".exe")
        ejecutar([binarios / ("gcc.exe" if es_c else "g++.exe"), "-std=c99" if es_c else "-std=c++20", "-O2", "-static",
            "-ILMNX-Deko3D/include", "-Iplatforms/switch/sdl3_shim", fuente, "-o", exe], "compilar-" + nombre)
        ejecutar([exe], nombre)
    ejecutar([sys.executable, RAIZ / "tools/generate_shader_cache.py", "--lista", RAIZ / "tools/shaders/sinteticos/lista.json", "--salida",salida/"cache-sintetica"], "uam-sinteticos")
    fuente = salida / "incorrecto.frag.glsl"
    fuente.write_text("#version 460\nESTO_NO_ES_GLSL\n", encoding="ascii")
    lote = salida / "incorrecto.tsv"
    lote.write_text(f"4\t0\t0\t{fuente}\n", encoding="utf-8")
    log = ejecutar([RAIZ / "build/shader-tools/uam_cache.exe", lote, salida / "cache-error"], "uam-error-recuperable", 1)
    if b"uam:" not in log: raise RuntimeError("The test did not reach uam")
    print("Host tests passed. Console GPU, presentation and audio were not run.")

if __name__ == "__main__":
    main()
