#!/usr/bin/env python3
"""Extract and AOT-recompile the user's ISO DOL; write generated output under build/."""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess

RAIZ = Path(__file__).resolve().parents[1]
SHA1_RETAIL = "77d49225e7d28b9777916cdbe5809345c570ecbc"
SHA1_DISCOS = {"GLME01": SHA1_RETAIL, "GLMP01": "a1e2bd82813787cfe0b11afb5d138f08fdbd94f1"}

def extraer(iso, version="GLME01"):
    if version not in SHA1_DISCOS:
        raise ValueError("Unsupported disc version")
    with iso.open("rb") as archivo:
        longitud = archivo.seek(0, 2)
        archivo.seek(0)
        cabecera = archivo.read(0x440)
        if len(cabecera) != 0x440 or cabecera[:6] != version.encode("ascii") or cabecera[7] != 0 or cabecera[0x1c:0x20] != bytes.fromhex("c2339f3d"):
            raise ValueError(f"An uncompressed GameCube {version} revision 0 ISO is required")
        inicio = struct.unpack_from(">I", cabecera, 0x420)[0]
        archivo.seek(inicio)
        dh = archivo.read(0x100)
        if len(dh) != 0x100:
            raise ValueError("Incomplete DOL header")
        tamano = 0x100
        for i in range(18):
            offset = struct.unpack_from(">I", dh, 4*i)[0]
            n = struct.unpack_from(">I", dh, 0x90 + 4*i)[0]
            if n and offset < 0x100:
                raise ValueError("A DOL section overlaps the header")
            if n:
                tamano = max(tamano, offset+n)
        if tamano > 32*1024*1024 or inicio+tamano > longitud:
            raise ValueError("DOL outside ISO bounds")
        archivo.seek(inicio)
        dol = archivo.read(tamano)
    huella = hashlib.sha1(dol).hexdigest()
    if huella != SHA1_DISCOS[version]:
        raise ValueError(f"Incompatible DOL: SHA-1 {huella}; expected {SHA1_DISCOS[version]}")
    return dol

def main():
    argumentos = argparse.ArgumentParser(description=__doc__)
    argumentos.add_argument("--iso", type=Path, required=True)
    argumentos.add_argument("--version", choices=SHA1_DISCOS, default="GLME01")
    argumentos.add_argument("--recompiler", type=Path, default=RAIZ / "build/host-recompiler/lmrecompile.exe")
    argumentos.add_argument("--locales-enteros", action="store_true", help="Enable conservative optimization checked with synthetic code")
    argumentos.add_argument("--locales-todos", action="store_true", help="Local GPRs in all functions, flushed to Ctx at calls, interrupts and returns")
    argumentos.add_argument("--salida", type=Path, default=RAIZ / "build/glme01-generado")
    args = argumentos.parse_args()
    if not args.iso.is_file():
        raise SystemExit(f"User ISO does not exist: {args.iso}")
    if not args.recompiler.is_file():
        raise SystemExit(f"Build the AOT tool first: {args.recompiler}")
    dol = extraer(args.iso, args.version)
    destino = args.salida.resolve()
    if not destino.is_relative_to((RAIZ / "build").resolve()):
        raise SystemExit("Generated files may only be written under build/")
    destino.mkdir(parents=True, exist_ok=True)
    ruta_dol = destino / "main.dol"
    ruta_dol.write_bytes(dol)
    config = "GLMP01_00" if args.version == "GLMP01" else "GLME01"
    orden = [str(args.recompiler.resolve()), "--dol", str(ruta_dol), "--version", args.version,
        "--symbols", str(RAIZ / f"decomp/config/{config}/symbols.txt"),
        "--splits", str(RAIZ / f"decomp/config/{config}/splits.txt"), "--hooks", str(RAIZ / "tools/recomp/hooks.txt"),
        "--switch-integer-locals", "1" if args.locales_enteros else "0",
        "--switch-all-locals", "1" if args.locales_todos else "0", "--out", str(destino)]
    if args.version == "GLMP01":
        orden += ["--version-map", str(RAIZ / "tools/recomp/GLMP01_map.txt")]
    subprocess.run(orden, check=True)
    if args.version == "GLMP01":
        # Both modules share Ctx and helpers. Put PAL functions/tables in a separate namespace to avoid retail symbol
        # collisions.
        for nombre in ["funcs.h", "func_table.cpp", *[p.name for p in destino.glob("funcs_*.cpp")]]:
            ruta = destino / nombre
            texto = ruta.read_text(encoding="utf-8")
            nuevo = texto.replace("namespace recomp {", "namespace recomp::pal {")
            if nuevo != texto:
                ruta.write_text(nuevo, encoding="utf-8")
    (destino / "dol.sha1").write_text(SHA1_DISCOS[args.version] + "\n", encoding="ascii")
    (destino / "version.txt").write_text(args.version + "\n", encoding="ascii")
    print(f"AOT C++ prepared in {destino}. Generated sources and the resulting game NRO contain translated game code.")

if __name__ == "__main__":
    main()
