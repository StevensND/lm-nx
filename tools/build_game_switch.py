"""Build the game NRO with compatible USA/PAL AOT, native Aurora and deko3d. Write generated files only under build/. This does not verify execution on a console."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
from prepare_glme01 import RAIZ

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--iso", type=Path, default=RAIZ / "orig/GLME01.iso")
    p.add_argument("--devkitpro", default=os.environ.get("DEVKITPRO", "/opt/devkitpro"))
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--pal", type=Path, help="Compatible PAL ISO for preparing the European Hidden Mansion")
    p.add_argument("--sin-locales", action="store_true", help="Disable integer leaf localization")
    p.add_argument("--sin-lto",action="store_true",help="Reference without LTO; LTO is enabled by default")
    p.add_argument("--pgo",choices=["OFF","GENERATE","USE"],default="OFF")
    p.add_argument("--pgo-dir",type=Path,default=RAIZ/"build/pgo-switch")
    args = p.parse_args()
    if not args.iso.is_file(): raise SystemExit(f"User ISO does not exist: {args.iso}. No synthetic game is generated.")
    subprocess.run([sys.executable, str(RAIZ / "tools/build_tint_direct.py"), "--switch", "--jobs", str(args.jobs)], cwd=RAIZ, check=True)
    from prepare_switch_discs import preparar
    overlay = preparar(args.iso, args.pal, exigir_pal=args.pal is not None)
    salida = RAIZ / "build/host-recompiler-directo"
    salida.mkdir(parents=True, exist_ok=True)
    binarios = (Path(args.devkitpro) / "msys2/mingw64/bin")
    env = os.environ.copy(); env["PATH"] = str(binarios) + os.pathsep + env["PATH"]
    env["TMP"] = env["TEMP"] = str(salida)
    exe = salida / "lmrecompile.exe"
    orden = [binarios / "g++.exe", "-std=c++20", "-O2", "-static", "-Isrc",
        "src/recompiler/recompiler.cpp", "tools/recomp/lmrecompile.cpp", "-o", exe]
    r = subprocess.run([str(x) for x in orden], cwd=RAIZ, env=env, capture_output=True)
    (salida / "compilacion.log").write_bytes(r.stdout + r.stderr); r.check_returncode()
    subprocess.run([sys.executable, str(RAIZ / "tools/prepare_glme01.py"), "--iso", str(args.iso.resolve()), "--recompiler", str(exe)], cwd=RAIZ, env=env, check=True)
    generados = RAIZ / "build/glme01-generado"
    if not args.sin_locales:
        generados = RAIZ / "build/glme01-locales-fase4"
        subprocess.run([sys.executable, str(RAIZ / "tools/prepare_glme01.py"), "--iso", str(args.iso.resolve()),
                        "--recompiler", str(exe), "--locales-enteros", "--salida", str(generados)], cwd=RAIZ, env=env, check=True)
        subprocess.run([sys.executable, str(RAIZ / "tools/test_codegen_retail_switch.py"),"--opt",str(generados),*(["--lto"] if not args.sin_lto else [])], cwd=RAIZ, env=env, check=True)
    extras = []
    if overlay is not None:
        pal_iso = args.pal or RAIZ / "orig/GLMP01.iso"
        for optimizar, nombre in [(False, "glmp01-generado"), (True, "glmp01-locales-fase4")]:
            if optimizar and args.sin_locales:
                continue
            pal_generados = RAIZ / "build" / nombre
            orden_pal = [sys.executable, str(RAIZ / "tools/prepare_glme01.py"), "--version", "GLMP01",
                "--iso", str(pal_iso.resolve()), "--recompiler", str(exe), "--salida", str(pal_generados)]
            if optimizar:
                orden_pal.append("--locales-enteros")
            subprocess.run(orden_pal, cwd=RAIZ, env=env, check=True)
        if not args.sin_locales:
            subprocess.run([sys.executable, str(RAIZ / "tools/test_codegen_retail_switch.py"), "--version", "GLMP01","--opt",str(pal_generados),*(["--lto"] if not args.sin_lto else [])], cwd=RAIZ, env=env, check=True)
        extras = ["--generados-pal", str(pal_generados)]
    # Both PC checks share this NRO's exact AOT/HLE sources. Compile only; do not run the game.
    for referencia in (False,True):
        subprocess.run([sys.executable,str(RAIZ/"tools/build_pc.py"),"--jobs",str(min(args.jobs,3)),"--generados",str(generados),*(["--generados-pal",str(pal_generados)] if overlay else ["--sin-pal"]),*(["--referencia-dawn"] if referencia else [])],cwd=RAIZ,env=env,check=True)
    return subprocess.run([sys.executable, str(RAIZ / "tools/build_switch_direct.py"), "--jobs", str(args.jobs),
        "--devkitpro", args.devkitpro, "--productor-nativo", "--generados", str(generados), *extras,
        *(["--sin-lto"] if args.sin_lto else []),"--pgo",args.pgo,"--pgo-dir",str(args.pgo_dir)], cwd=RAIZ, env=env).returncode

if __name__ == "__main__":
    raise SystemExit(main())
