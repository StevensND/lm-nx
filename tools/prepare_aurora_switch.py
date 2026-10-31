#!/usr/bin/env python3
"""Apply LM patches to a build snapshot without Git reset/clean."""
import os
import hashlib
from pathlib import Path, PurePosixPath
import shutil
import subprocess

RAIZ = Path(__file__).resolve().parents[1]

def preparar():
    parches = sorted((RAIZ / "patches/aurora").glob("*.patch"))
    # Include checkout contents in the identity so Aurora merges invalidate snapshots even when LM patches are
    # unchanged.
    identidad = hashlib.sha256(b"lmnx-native-git-lf-v4")
    for fuente in sorted((RAIZ / "extern/aurora").rglob("*")):
        if not fuente.is_file() or any(p in (".git", "build", "__pycache__") for p in fuente.relative_to(RAIZ / "extern/aurora").parts):
            continue
        datos = fuente.read_bytes()
        if b"\0" not in datos:
            datos = datos.replace(b"\r\n", b"\n")
        identidad.update(fuente.relative_to(RAIZ / "extern/aurora").as_posix().encode() + b"\0" + datos)
    identidad.update(b"".join(p.name.encode() + p.read_bytes() for p in parches))
    clave = identidad.hexdigest()
    destino = RAIZ / "build" / ("aurora-lm-" + clave[:12])
    sello = destino / "lmnx-parches.txt"
    if sello.exists() and sello.read_text().strip() == clave:
        return destino
    if destino.exists():
        raise SystemExit(f"Incomplete snapshot exists: {destino}; inspect it before retrying")
    for parche in parches:
        for linea in parche.read_text(encoding="utf-8").splitlines():
            if linea.startswith(("--- ", "+++ ")) and linea[4:] != "/dev/null":
                ruta = PurePosixPath(linea[4:])
                if ruta.parts[0] not in ("a", "b") or ".." in ruta.parts or ".git" in ruta.parts:
                    raise SystemExit(f"Patch path outside the tree: {parche.name}: {linea}")
    shutil.copytree(RAIZ / "extern/aurora", destino,
        ignore=shutil.ignore_patterns(".git", "build", "__pycache__"))
    # Normalize CRLF only in the new snapshot and only in content without NUL bytes; never modify original sources.
    for fuente in destino.rglob("*"):
        if fuente.is_file():
            datos = fuente.read_bytes()
            if b"\0" not in datos and b"\r\n" in datos:
                fuente.write_bytes(datos.replace(b"\r\n", b"\n"))
    git_nativo = Path(os.environ.get("GIT", shutil.which("git") or "git"))
    git = str(git_nativo) if git_nativo.is_file() else shutil.which("git")
    if not git:
        raise SystemExit("Native Git is required to apply patches")
    log = destino.parent / "aurora-parches.log"
    with log.open("w", encoding="utf-8") as archivo:
        for parche in parches:
            archivo.write(parche.name + "\n"); archivo.flush()
            subprocess.run([git, "-c", "core.autocrlf=false", "apply", "--check", str(parche)],
                cwd=destino, stdout=archivo, stderr=archivo, check=True)
            subprocess.run([git, "-c", "core.autocrlf=false", "apply", str(parche)],
                cwd=destino, stdout=archivo, stderr=archivo, check=True)
    sello.write_text(clave + "\n", encoding="ascii")
    return destino

if __name__ == "__main__":
    print(preparar())
