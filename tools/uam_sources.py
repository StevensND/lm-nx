"""Shared uam source list for Switch and host tools."""
import re

def fuentes(uam):
    salida = [uam / x for x in ["source/compiler_iface.cpp", "source/glsl_frontend.cpp",
        "source/mini-os.c", "source/tgsi_support.cpp", "uam_api.cpp", "generated/glsl/glsl_parser.cpp",
        "generated/glsl/glsl_lexer.cpp", "generated/glsl/glcpp/glcpp-parse.c",
        "generated/glsl/glcpp/glcpp-lex.c", "mesa-imported/glsl/glcpp/pp.c"]]
    for carpeta in ["codegen", "tgsi", "util", "cso_cache", "glsl", "compiler", "program", "state_tracker", "main"]:
        texto = (uam / "mesa-imported" / carpeta / "meson.build").read_text(encoding="utf-8")
        for bloque in re.findall(r"files\(([^)]*)\)", texto):
            salida.extend(uam / "mesa-imported" / carpeta / nombre for nombre in re.findall(r"'([^']+)'", bloque))
    return salida

def opciones(uam):
    return ["-DNDEBUG", "-DDESKTOP", "-D_GNU_SOURCE", "-DHAVE_POSIX_MEMALIGN", '-DPACKAGE_STRING="uam 1.1.0"', "-w"] + [
        "-I" + str(uam / x) for x in [".", "source", "mesa-imported", "generated", "generated/glsl", "generated/glsl/glcpp"]]
