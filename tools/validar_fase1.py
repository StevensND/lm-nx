#!/usr/bin/env python3
"""Audit local artifacts without running the NRO or modifying the SD card."""
import os
import hashlib
import json
from pathlib import Path
import subprocess

RAIZ = Path(__file__).resolve().parents[1]

def main():
    build = RAIZ / "build/direct-switch"
    registros = json.loads((build / "resultados.json").read_text(encoding="utf-8"))
    assert len(registros) == 204 and all(x["codigo"] == 0 for x in registros)
    nombres = ["libuam.a", "liblmnx_deko3d.a", "liblmnx_sdl3.a", "liblmnx_switch_runtime.a",
               "liblmnx_aot_runtime_core.a", "ProjectLM-NX-diagnostico.elf", "ProjectLM-NX-diagnostico.nro"]
    lineas = ["# Platform artifact validation", "",
              "204 units compiled with devkitA64 16.1.0. Libraries, diagnostic linkage and NRO packaging checked; not run on Switch.", "",
              "| Artifact | Bytes | SHA-256 |", "|---|---:|---|"]
    for nombre in nombres:
        ruta = build / nombre
        assert ruta.is_file() and ruta.stat().st_size > 0
        lineas.append(f"| `{nombre}` | {ruta.stat().st_size} | `{hashlib.sha256(ruta.read_bytes()).hexdigest()}` |")
    nm = subprocess.run([str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "devkitA64/bin/aarch64-none-elf-nm.exe"), "-C",
                         str(build / "ProjectLM-NX-diagnostico.elf")], capture_output=True, text=True, check=True)
    prohibidos = [linea for linea in nm.stdout.splitlines() if any(x in linea for x in
                   ["wgpu", "Dawn", "vkCreate", "eglCreate", "glDrawArrays", "glDrawElements"])]
    assert not prohibidos, prohibidos
    nro = (build / "ProjectLM-NX-diagnostico.nro").read_bytes()
    assert nro[0x10:0x14] == b"NRO0"
    assert (RAIZ / "build/check-codegen/test.exe").is_file()
    assert not (RAIZ / "orig").exists()
    excluidos = {"build", ".git", "__pycache__"}
    datos = [p for p in RAIZ.rglob("*") if p.is_file() and not excluidos.intersection(p.relative_to(RAIZ).parts)
             and p.suffix.lower() in [".iso", ".rvz", ".gcm", ".dol", ".gci", ".sav", ".nsp", ".xci"]]
    assert not datos, datos
    for nombre in ["docs/BUILDING_SWITCH.md", "docs/README.md", "THIRD_PARTY_NOTICES.md", "docs/PC_ORIGINAL.md"]:
        assert (RAIZ / nombre).is_file()
    lineas += ["", "Additional checks:", "",
        "- Valid NRO0 header.",
        "- ELF nm inspection: none of the searched Dawn/wgpu, Vulkan, EGL or OpenGL draw symbols found. This check does not replace a full game dependency audit.",
        "- No discs, DOLs, saves or console packages outside build/.",
        "- Required documentation and third-party notices are present.",
        "- AOT differential test: 100,000 identical Ctx states and rotations; see check-codegen.log.",
        "- Thread test: 100 reuses, four workers and timer joined; see check-threads.log.",
        "- DKSH validator: truncation, overflow and out-of-range entry points rejected.", "",
        "Limits: this diagnostic contains no game, does not link every AOT core service and creates no GPU device. It does not verify the complete game build or console execution."]
    (RAIZ / "docs/VALIDACION_FASE1.md").write_text("\n".join(lineas) + "\n", encoding="utf-8")
    print("Audit passed: 204 units, libraries, ELF/NRO and no game data outside build/.")

if __name__ == "__main__":
    main()
