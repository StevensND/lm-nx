"""Reproducible identity for the uam compiler, cache format and native backend."""
import hashlib
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[1]

# Code excluded from DKSH and translation identity: queues, submissions, barriers, memory and logging. Changing
# it must not invalidate the whole cache and recompile shaders at startup.
AJENOS_A_SHADERS = {
    "third_party/plume/plume_deko3d_commands.cpp",
    "src/native_gfx.cpp",
    "src/backend.cpp",
    "include/lmnx/directo.hpp",
    "include/lmnx/aurora_gpu.hpp",  # Aurora interface: buffers, queues and waits; shaders come from aurora_gpu.cpp. Cache storage controls where
# and how entries are saved, not compilation; validate entries when reading.
    "src/shader_cache.cpp",
    "include/lmnx/cache_paquete.hpp",
    "include/lmnx/translation_cache.hpp",
    # Performance panel: per-frame measurements unrelated to translation.
    "include/lmnx/perfil_vivo.hpp",
}

# Mixed files contribute only text between [identidad-shaders inicio] and [identidad-shaders fin].
SOLO_REGION = {"src/aurora_gpu.cpp"}

def identidad():
    base = RAIZ / "LMNX-Deko3D"
    h = hashlib.sha256(b"LMNX cache v2 flags=447 GXABI=2\0")
    for p in sorted(base.rglob("*")):
        if p.relative_to(base).as_posix() in AJENOS_A_SHADERS:
            continue
        if p.is_file() and "referencias" not in p.parts and p.suffix in (".c", ".cpp", ".h", ".hpp", ".inc"):
            datos = p.read_bytes().replace(b"\r\n", b"\n")
            if p.relative_to(base).as_posix() in SOLO_REGION:
                # Only shader translation contributes from the renderer, excluding drawing, state and measurements.
                datos = datos[datos.index(b"// [identidad-shaders inicio]"):datos.index(b"// [identidad-shaders fin]")]
            h.update(p.relative_to(base).as_posix().encode() + b"\0" + datos)
    h.update(b"Tint 1155e0ed531126f33a1279afa029349651ca1c93 PC WGSL+GLSL O2\0")
    entradas = [RAIZ / "tools" / n for n in ("wgsl_to_glsl.cpp", "tint_manifest.cmake", "build_tint_direct.py")]
    entradas += sorted((RAIZ / "patches/tint").glob("*.patch"))
    for p in entradas:
        h.update(p.relative_to(RAIZ).as_posix().encode() + b"\0" + p.read_bytes().replace(b"\r\n", b"\n"))
    return h.hexdigest()[:24]

if __name__ == "__main__":
    print(identidad())
