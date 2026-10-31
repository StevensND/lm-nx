"""Verifica en PC WGSL → Tint → GLSL → uam → DKSH; no ejecuta GPU."""
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib
from shader_build_id import identidad

RAIZ = Path(__file__).resolve().parents[1]


def main():
    salida = RAIZ / "build/fase4/traduccion-shaders"
    salida.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(RAIZ / "tools/generate_shader_cache.py"), "--lista",
                    str(RAIZ / "tools/shaders/wgsl-probe/lista.json"),"--salida",str(salida/"cache")], cwd=RAIZ, check=True)
    cache = salida / "cache" / ("lmnx-f447-gxabi2-" + identidad())
    vertex = json.loads((cache / "traducido-0.vertex.json").read_text())
    fragment = json.loads((cache / "traducido-1.fragment.json").read_text())
    assert (vertex["ubo"], vertex["ssbo"], vertex["inmediatos_bytes"]) == (32768, 1, 64)
    assert (fragment["ubo"], fragment["ssbo"], fragment["texturas"]) == (1, 0, 1)
    glsl = (cache / "traducido-0.vertex.glsl").read_text()
    assert re.search(r"gl_Position = vec4\((\w+)\.x, \1\.y, \1\.z, \1\.w\);", glsl), glsl
    registros = []
    for archivo in cache.glob("*.lmc"):
        datos = archivo.read_bytes()
        magic, version, etapa, ns, nd, cs, cd = struct.unpack_from("<7I", datos)
        fuente = datos[28:28 + ns]
        if b"vs_main_inner" not in fuente and b"fs_main_inner" not in fuente:
            continue
        dksh = datos[28 + ns:]
        assert magic == 0x32434D4C and version == 1 and len(datos) == 28 + ns + nd
        assert zlib.crc32(fuente) == cs and zlib.crc32(dksh) == cd and dksh[:4] == b"DKSH"
        registros.append({"etapa": etapa, "bytes_dksh": nd, "archivo": archivo.name})
    assert sorted(r["etapa"] for r in registros) == [0, 4]
    exe = RAIZ / "build/tint-directo/wgsl_to_glsl.exe"
    wgsl = RAIZ / "tools/shaders/wgsl-probe/nativo.wgsl"
    incorrecto = salida / "incorrecto.wgsl"
    incorrecto.write_text("ESTO_NO_ES_WGSL\n", encoding="ascii")
    compute = salida / "compute.wgsl"
    compute.write_text("@compute @workgroup_size(1) fn cs_main() {}\n", encoding="ascii")
    negativas = []
    for nombre, entrada, entry, etapa in [("sintaxis", incorrecto, "main", "vertex"),
            ("entrada_ausente", wgsl, "ausente", "vertex"), ("etapa", wgsl, "vs_main", "fragment"),
            ("compute_rechazado", compute, "cs_main", "fragment")]:
        r = subprocess.run([str(exe), str(entrada), entry, etapa, "gx", str(salida / (nombre + ".glsl")),
                            str(salida / (nombre + ".json"))], cwd=RAIZ, capture_output=True)
        (salida / (nombre + ".log")).write_bytes(r.stdout + r.stderr)
        assert r.returncode == 1 and r.stderr, nombre
        negativas.append(nombre)
    compute.write_text("struct Data { v: array<u32>, } @group(0) @binding(0) var<storage, read_write> d: Data; "
        "@compute @workgroup_size(8) fn cs_main(@builtin(global_invocation_id) id: vec3<u32>) { d.v[id.x] = id.x + 1u; }\n",encoding="ascii")
    cg=salida/"compute.glsl";cm=salida/"compute.json"
    subprocess.run([str(exe),str(compute),"cs_main","compute","sequential",str(cg),str(cm)],cwd=RAIZ,check=True)
    reflejo_compute=json.loads(cm.read_text())
    lote_compute=salida/"compute.tsv"
    lote_compute.write_text(f"5\t{reflejo_compute['ubo']}\t{reflejo_compute['ssbo']}\t{cg}\n",encoding="utf-8")
    cache_compute=salida/"cache-compute"
    subprocess.run([str(RAIZ/"build/shader-tools/uam_cache.exe"),str(lote_compute),str(cache_compute)],cwd=RAIZ,check=True)
    for archivo in cache_compute.glob("*.lmc"):
        datos=archivo.read_bytes();magic,version,etapa,ns,nd,cs,cd=struct.unpack_from("<7I",datos)
        assert etapa==5 and zlib.crc32(datos[28:28+ns])==cs and zlib.crc32(datos[28+ns:])==cd
        assert datos[28+ns:32+ns]==b"DKSH" and len(datos)==28+ns+nd
    assert list(cache_compute.glob("*.lmc"))
    informe = {"consola": False, "datos": "Synthetic WGSL; not GLME01 TEV", "dksh": registros,
               "vertex": vertex, "fragment": fragment, "clip_sin_inversion_y_ni_remapeo_z": True,
               "errores_rechazados": negativas,"compute_traducido_y_compilado":True,"reflejo_compute":reflejo_compute}
    (salida / "informe.json").write_text(json.dumps(informe, indent=2, ensure_ascii=False), encoding="utf-8")
    print("Native WGSL: bindings, immediate data, clip, CRC and DKSH checked; four errors rejected.")


if __name__ == "__main__":
    main()
