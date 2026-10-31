"""Generate Aurora data enums and descriptors, not a WebGPU device. Preserve Aurora pipeline serialization values. Implement resources and commands natively in aurora_gpu.cpp."""
import json
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[1]


def nombre(s):
    v = "".join(p[:1].upper() + p[1:] for p in s.split())
    return "e" + v if v[:1].isdigit() else v


def main():
    schema = RAIZ / "extern/fuentes/dawn-1155e0ed531126f33a1279afa029349651ca1c93/src/dawn/dawn.json"
    datos = json.loads(schema.read_text())
    iniciales = """address mode backend type bind group descriptor bind group entry bind group layout descriptor bind group layout entry"""
    tipos = ["address mode", "backend type", "bind group descriptor", "bind group entry", "bind group layout descriptor",
        "bind group layout entry", "blend component", "blend factor", "blend operation", "blend state", "buffer binding layout",
        "buffer binding type", "buffer descriptor", "buffer usage", "callback mode", "color", "color target state", "color write mask",
        "command encoder descriptor", "command buffer descriptor", "compare function", "component swizzle", "compute pass descriptor",
        "compute pipeline descriptor", "compute state", "cull mode", "depth stencil state", "error filter", "error type", "extent 3D",
        "feature name", "filter mode", "fragment state", "front face", "index format", "limits", "load op", "map async status",
        "map mode", "mipmap filter mode", "multisample state", "origin 3D", "pass timestamp writes", "pipeline layout descriptor",
        "pop error scope status", "present mode", "primitive state", "primitive topology", "query set descriptor", "query type",
        "render pass color attachment", "render pass depth stencil attachment", "render pass descriptor", "render pipeline descriptor",
        "sampler binding layout", "sampler binding type", "sampler descriptor", "shader module descriptor", "shader source WGSL",
        "shader stage", "store op", "surface configuration", "surface capabilities", "texel copy buffer info", "texel copy buffer layout",
        "texel copy texture info", "texture binding layout", "texture component swizzle descriptor", "texture descriptor",
        "texture dimension", "texture format", "texture sample type", "texture usage", "texture view descriptor", "texture view dimension",
        "vertex buffer layout", "vertex state", "adapter info", "wait status", "queue work done status"]
    usados = set()
    def visitar(t):
        if t in usados or t not in datos or t in ("string view", "optional bool"):
            return
        usados.add(t)
        d = datos[t]
        if d.get("category") == "structure":
            for m in d.get("members", []):
                visitar(m["type"])
    for t in tipos:
        visitar(t)
    enums = [t for t in sorted(usados) if datos[t].get("category") in ("enum", "bitmask")]
    lineas = ["// Serialized Aurora enums from Dawn's BSD-3-Clause schema; data only.", "#pragma once", "#include <cstdint>",
              "namespace lmnx::gpu {"]
    for t in enums:
        d = datos[t]; mask = d["category"] == "bitmask"
        lineas.append(f"enum class {nombre(t)} : {'uint64_t' if mask else 'uint32_t'} {{")
        for v in d["values"]:
            valor = v.get("value", 0)
            if isinstance(valor, str):
                valor = int(valor, 0)
            if v.get("tags"):
                valor |= 0x50000
            lineas.append(f"    {nombre(v['name'])} = {valor},")
        lineas.append("};")
        if mask:
            for op in ("|", "&", "^"):
                lineas.append(f"constexpr {nombre(t)} operator{op}({nombre(t)} a, {nombre(t)} b) {{ return {nombre(t)}(uint64_t(a) {op} uint64_t(b)); }}")
                lineas.append(f"constexpr {nombre(t)}& operator{op}=({nombre(t)}& a, {nombre(t)} b) {{ return a = a {op} b; }}")
            lineas.append(f"constexpr bool operator!({nombre(t)} a) {{ return uint64_t(a)==0; }}")
    lineas.append("}")
    salida = RAIZ / "LMNX-Deko3D/include/lmnx"
    (salida / "aurora_gpu_enums.hpp").write_text("\n".join(lineas) + "\n", encoding="utf-8")
    estructuras = [t for t in usados if datos[t].get("category") == "structure"]
    pendientes = set(estructuras); orden = []
    while pendientes:
        posibles = sorted(t for t in pendientes if all(m["type"] not in pendientes or "*" in m.get("annotation", "")
                                                       for m in datos[t].get("members", [])))
        if not posibles:
            raise ValueError("Ciclo de descriptores nativos: " + str(pendientes))
        orden += posibles; pendientes -= set(posibles)
    lineas = ["// Data descriptors with LMNX-owned resources; no Dawn ABI or objects.", "#pragma once", "namespace lmnx::gpu {"]
    lineas += [f"struct {nombre(t)};" for t in sorted(estructuras)]
    for t in orden:
        d = datos[t]
        lineas.append(f"struct {nombre(t)}" + (" : ChainedStruct" if d.get("chained") else "") + " {")
        if d.get("extensible"):
            lineas.append("    const ChainedStruct* nextInChain = nullptr;")
        if t == "shader source WGSL":
            lineas += ["    struct Init { StringView code; };", "    ShaderSourceWGSL() = default;", "    ShaderSourceWGSL(Init init) : code(init.code) {}"]
        for m in d.get("members", []):
            typ = m["type"]; n = nombre(m["name"]); n = n[:1].lower() + n[1:]
            if typ in ("uint16_t", "uint32_t", "uint64_t", "int32_t", "int64_t", "size_t", "float", "double", "bool", "void"):
                cpp = typ
            elif typ == "optional bool":
                cpp = "OptionalBool"
            else:
                cpp = nombre(typ)
            ann = m.get("annotation", "")
            if "*" in ann:
                cpp = ("const " if "const" in ann else "") + cpp + "*"
            default = m.get("default")
            value = "{}"
            if "*" in ann:
                value = "nullptr"
            elif typ in enums and isinstance(default, str) and any(v["name"] == default for v in datos[typ]["values"]):
                value = cpp + "::" + nombre(default)
            elif str(default) in ("true", "false") or isinstance(default, (int, float)):
                value = str(default)
            elif isinstance(default, str) and (default.startswith("0x") or default[:1].isdigit()):
                value = default
            elif default == "whole size":
                value = "UINT64_MAX"
            elif isinstance(default, str) and "undefined" in default and typ in ("uint32_t", "uint64_t"):
                value = "UINT32_MAX" if typ == "uint32_t" else "UINT64_MAX"
            lineas.append(f"    {cpp} {n} = {value};")
        lineas.append("};")
    lineas.append("}")
    (salida / "aurora_gpu_descriptors.hpp").write_text("\n".join(lineas) + "\n", encoding="utf-8")
    print(f"Native types: {len(enums)} enums and {len(estructuras)} descriptors; no generated GPU code")


if __name__ == "__main__":
    main()
