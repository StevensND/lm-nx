"""Apply only Tint coordinate transformations to a build overlay."""
import os
import shutil
import hashlib
import json
from pathlib import Path
import re
import subprocess
from local_dependencies import RAIZ, fuente


def preparar():
    raiz = fuente("dawn")
    parches = [RAIZ / "patches/tint" / n for n in (
        "dawn-switch-tint-depth-zero-to-one.patch", "dawn-switch-tint-position-y-up.patch")]
    rutas = sorted({m for p in parches for m in re.findall(r"^\+\+\+ b/(.+)$", p.read_text(encoding="utf-8"), re.M)})
    for ruta in rutas:
        if not ruta.startswith("src/tint/") or ".." in Path(ruta).parts:
            raise ValueError("Invalid Tint patch path")
    clave = hashlib.sha256(b"".join(p.read_bytes() for p in parches) +
                           b"".join((raiz / r).read_bytes() for r in rutas)).hexdigest()
    salida = RAIZ / "build" / ("tint-overlay-" + clave[:12])
    sello = salida / "parches.json"
    if sello.is_file():
        return salida
    if salida.exists():
        raise RuntimeError("Incomplete Tint overlay: " + str(salida))
    for ruta in rutas:
        destino = salida / ruta
        destino.parent.mkdir(parents=True, exist_ok=True)
        destino.write_bytes((raiz / ruta).read_bytes().replace(b"\r\n", b"\n"))
    git = os.environ.get("GIT", shutil.which("git") or "git")
    with (salida / "aplicacion.log").open("wb") as log:
        for parche in parches:
            subprocess.run([git, "apply", "--check", str(parche)], cwd=salida, stdout=log, stderr=log, check=True)
            subprocess.run([git, "apply", str(parche)], cwd=salida, stdout=log, stderr=log, check=True)
    sello.write_text(json.dumps({"clave": clave, "fuentes": rutas, "parches": [p.name for p in parches]}, indent=2), encoding="utf-8")
    return salida


if __name__ == "__main__":
    print(preparar())
