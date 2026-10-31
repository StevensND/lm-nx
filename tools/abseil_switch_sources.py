"""Resolve the Abseil libraries used by Aurora from local CMakeLists. Add no tests, benchmarks, generators or network dependencies. Actual compilers check the resulting source set; no implementations are replaced."""
import re
from local_dependencies import fuente


def fuentes_abseil():
    raiz = fuente("abseil-cpp")
    objetivos = {}
    for cmake in sorted((raiz / "absl").rglob("CMakeLists.txt")):
        texto = re.sub(r"#[^\n]*", "", cmake.read_text(encoding="utf-8"))
        for bloque in re.findall(r"absl_cc_library\((.*?)\n\)", texto, re.S):
            campos = {}
            clave = None
            for token in re.findall(r'"[^"]*"|[^\s]+', bloque):
                token = token.strip('"')
                if token in ("NAME", "HDRS", "SRCS", "COPTS", "DEFINES", "LINKOPTS", "DEPS", "PUBLIC", "TESTONLY"):
                    clave = token
                    campos.setdefault(clave, [])
                elif clave:
                    campos[clave].append(token)
            nombre = campos.get("NAME", [None])[0]
            if nombre and "TESTONLY" not in campos:
                objetivos[nombre] = (cmake.parent, campos)
    visitados = set()
    resultado = set()
    def visitar(nombre):
        if nombre in visitados:
            return
        if nombre not in objetivos:
            raise ValueError("Undeclared Abseil dependency: " + nombre)
        visitados.add(nombre)
        carpeta, campos = objetivos[nombre]
        for src in campos.get("SRCS", []):
            if src.endswith(".cc"):
                ruta = carpeta / src
                if not ruta.is_file():
                    raise ValueError("Unresolved Abseil source: " + str(ruta))
                resultado.add(ruta)
        for dep in campos.get("DEPS", []):
            if dep.startswith("absl::"):
                visitar(dep[6:])
    for nombre in ("flat_hash_map", "flat_hash_set", "btree"):
        visitar(nombre)
    return sorted(resultado)
