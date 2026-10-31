"""Paths to user-supplied sources; never download files."""
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[1]
FUENTES = RAIZ / "extern/fuentes"
VERSIONES = {
    "abseil-cpp": "abseil-cpp-20240722.0",
    "fmt": "fmt-12.1.0",
    "imgui": "imgui-1.91.9b-docking",
    "dawn": "dawn-1155e0ed531126f33a1279afa029349651ca1c93",
    "spirv-headers": "SPIRV-Headers-942fe4b988359a0750b79f0ae7ed735994d3147d",
    "spirv-tools": "SPIRV-Tools-5b5a25231a3543559b4f2cb27fe725a08b76b5d0",
    "glslang": "glslang-90afccfbd49dff0349d86a41762e9de24e1df811",
    "xxhash": "xxHash-0.8.3",
    "zlib": "zlib-ng-2.3.3",
    "libpng": "libpng-1.6.58",
    "freetype": "freetype-2.14.3",
    "zstd": "zstd-1.5.7",
    "lzma-sdk": "lzma-sdk-26.05",
    "bzip2": "bzip2-1.0.8",
    "rmlui": "RmlUi-f00a0fee38391b2f927114e11bea18dc0a7dba1e",
    "sqlite": "sqlite-amalgamation-3510300",
}


def fuente(nombre):
    ruta = FUENTES / VERSIONES[nombre]
    if not ruta.is_dir():
        raise FileNotFoundError(f"Missing local dependency {nombre}: {ruta}")
    return ruta


def includes_locales():
    includes = [fuente(n) for n in ("abseil-cpp", "fmt", "imgui", "dawn", "xxhash", "sqlite")] + [fuente("fmt") / "include"]
    includes.insert(0, RAIZ / "platforms/switch/perfilado")
    return includes


def opciones_cmake():
    """FetchContent arguments with networking disabled even in an empty tree."""
    nombres = {"libpng": "PNG", "sqlite": "SQLITE3"}
    return ["-DFETCHCONTENT_FULLY_DISCONNECTED=ON", "-DFETCHCONTENT_UPDATES_DISCONNECTED=ON"] + [
        f"-DFETCHCONTENT_SOURCE_DIR_{nombres.get(nombre, nombre.upper())}={fuente(nombre).as_posix()}"
        for nombre in VERSIONES
    ]
