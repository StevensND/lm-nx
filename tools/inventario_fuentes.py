#!/usr/bin/env python3
"""Record source provenance and reference documentation; write only under docs/.

LMNX_PC_SOURCE, LMNX_BANJO_NX, LMNX_SWITCHWAKER and LMNX_TP_ALEKS select
upstream source trees. Defaults are named directories under extern/fuentes/.
"""
import hashlib
import json
import os
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[1]
PC = Path(os.environ.get("LMNX_PC_SOURCE", str(RAIZ / "extern/fuentes/luigis-mansion-recompiled")))
BANJO = Path(os.environ.get("LMNX_BANJO_NX", str(RAIZ / "extern/fuentes/banjo-nx")))

def sha(ruta):
    return hashlib.sha256(ruta.read_bytes()).hexdigest()

def main():
    procedencia = []
    pares = [(RAIZ / "LMNX-Deko3D/third_party/uam", BANJO / "app/thirdparty/uam"),
             (RAIZ / "LMNX-Deko3D/third_party/plume", BANJO / "app/thirdparty/plume"),
             (RAIZ / "platforms/switch/sdl3_shim", (Path(os.environ.get("LMNX_SWITCHWAKER", str(RAIZ / "extern/fuentes/SwitchWaker"))) / "switch/aurora/sdl3_shim")),
             (RAIZ / "platforms/switch/sdl3_headers/SDL3", BANJO / "sdk/thirdparty/sdl3/include"),
             (RAIZ, PC)]
    vistos = set()
    for destino, origen in pares:
        for ruta in destino.rglob("*"):
            if not ruta.is_file() or "build" in ruta.relative_to(RAIZ).parts or "__pycache__" in ruta.parts:
                continue
            relativo = ruta.relative_to(RAIZ).as_posix()
            fuente = origen / ruta.relative_to(destino)
            if relativo in vistos or not fuente.is_file():
                continue
            vistos.add(relativo)
            a,b = sha(fuente),sha(ruta)
            procedencia.append({"destino": relativo, "origen": fuente.as_posix(), "sha256_origen": a,
                               "sha256_destino": b, "estado": "igual" if a == b else "adaptado"})
    documentos = []
    for carpeta in [BANJO / "docs", (Path(os.environ.get("LMNX_TP_ALEKS", str(RAIZ / "extern/fuentes/TP-Aleks"))) / "docs"), (Path(os.environ.get("LMNX_SWITCHWAKER", str(RAIZ / "extern/fuentes/SwitchWaker"))) / "docs")]:
        for ruta in sorted(carpeta.glob("*.md")):
            if carpeta.parent.name == "TP-Aleks" and not (ruta.name.startswith("switch-h") or ruta.name == "building-switch.md"):
                continue
            if carpeta.parent.name == "SwitchWaker" and ruta.name not in ["DEKO3D_MIGRATION_PLAN.md", "SWITCH_BUILD.md", "SWITCH_PERF_STUDY.md", "FPS60_PLAN.md"]:
                continue
            texto = ruta.read_text(encoding="utf-8-sig", errors="replace")
            encabezados = [linea.strip() for linea in texto.splitlines() if linea.startswith("#")]
            documentos.append({"ruta": ruta.as_posix(), "bytes": ruta.stat().st_size,
                               "sha256": sha(ruta), "encabezados": encabezados})
    informe = {"fecha": "", "nota": "Content and provenance inventory; does not certify integration or console testing.",
               "fuentes": procedencia, "documentos_referencia": documentos}
    (RAIZ / "docs/PROCEDENCIA.json").write_text(json.dumps(informe, indent=2, ensure_ascii=False), encoding="utf-8")
    lineas = ["# Reference documentation inventory", "", "Read-only source paths. Hashes and all headings are in `PROCEDENCIA.json`.", "",
              "This index records the inventory; it does not validate individual proposals or their integration.", ""]
    for documento in documentos:
        lineas += [f"## {Path(documento['ruta']).name}", "", f"Source: `{documento['ruta']}`; {documento['bytes']} bytes.", ""]
        lineas.extend("- " + h.lstrip("# ") for h in documento["encabezados"])
        lineas.append("")
    (RAIZ / "docs/INVENTARIO_REFERENCIAS.md").write_text("\n".join(lineas), encoding="utf-8")
    print(f"Recorded {len(procedencia)} sources and {len(documentos)} reference documents.")

if __name__ == "__main__":
    main()
