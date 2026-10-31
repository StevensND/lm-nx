"""Generate DKSH from a GLSL/WGSL JSON list through host Tint and uam."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from build_uam_host import construir
from shader_build_id import identidad

RAIZ = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--lista", type=Path, required=True)
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--salida", type=Path, default=RAIZ / "build/cache-inicial", help="Output root under build/")
    args = p.parse_args()
    lista = args.lista.resolve()
    datos = json.loads(lista.read_text(encoding="utf-8-sig"))
    if datos.get("version") != 1 or not isinstance(datos.get("shaders"), list) or not datos["shaders"]:
        raise SystemExit("Requires version=1 and a nonempty shaders list")
    build_id=identidad()
    raiz_cache=args.salida.resolve()
    if not raiz_cache.is_relative_to(RAIZ / "build"):raise ValueError("Output must stay under build/")
    salida = raiz_cache / ("lmnx-f447-gxabi2-" + build_id)
    salida.mkdir(parents=True, exist_ok=True)
    for directorio in (salida,raiz_cache/("lmnx-tint-"+build_id)):
        (directorio/"completo.json").unlink(missing_ok=True)
    lineas = []
    traducciones = []
    preparados = []
    traductor = RAIZ / "build/tint-directo/wgsl_to_glsl.exe"
    if any("wgsl" in s for s in datos["shaders"]):
        subprocess.run([sys.executable, str(RAIZ / "tools/build_tint_direct.py"), "--jobs", str(args.jobs)], cwd=RAIZ, check=True)
    for indice, shader in enumerate(datos["shaders"]):
        etapa = {"vertex": 0, "fragment": 4, "compute":5}.get(shader["etapa"])
        if "wgsl" in shader:
            entrada = (lista.parent / shader["wgsl"]).resolve()
            ruta = salida / f"traducido-{indice}.{shader['etapa']}.glsl"
            metadatos = ruta.with_suffix(".json")
            campos=[str(entrada),shader["entrada"],shader["etapa"],shader.get("esquema","gx"),str(ruta),str(metadatos),str(raiz_cache / ("lmnx-tint-"+build_id))]
            if any(any(c in s for c in "\t\r\n") for s in campos):raise ValueError("Shader path is not valid for TSV")
            traducciones.append("\t".join(campos))
        else:
            ruta = (lista.parent / shader["glsl"]).resolve()
        preparados.append((shader,etapa,ruta))
    def ejecutar_log(orden, nombre):
        with (salida / nombre).open("w",encoding="utf-8") as log:
            proceso=subprocess.Popen(orden,cwd=RAIZ,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding="utf-8",errors="replace")
            for linea in proceso.stdout:
                log.write(linea);log.flush();print(linea,end="",flush=True)
            if proceso.wait():raise subprocess.CalledProcessError(proceso.returncode,orden)
    if traducciones:
        lote_tint=salida / "traducciones.tsv";lote_tint.write_text("\n".join(traducciones)+"\n",encoding="utf-8")
        ejecutar_log([str(traductor),"--lote",str(lote_tint)],"tint.log")
    for shader,etapa,ruta in preparados:
        if "wgsl" in shader:
            metadatos=ruta.with_suffix(".json")
            reflejo = json.loads(metadatos.read_text(encoding="utf-8"))
            for clave in ("ubo", "ssbo"):
                if clave in shader and shader[clave] != reflejo[clave]:
                    raise ValueError("Declared bindings differ from Tint reflection: " + clave)
                shader[clave] = reflejo[clave]
        if etapa is None or not ruta.is_file() or any(c in str(ruta) for c in "\t\r\n"):
            raise SystemExit("Invalid GLSL stage or path")
        lineas.append(f"{etapa}\t{int(shader.get('ubo', 0))}\t{int(shader.get('ssbo', 0))}\t{ruta}")
    lote = salida / "lista.tsv"
    lote.write_text("\n".join(lineas) + "\n", encoding="utf-8")
    exe = construir(args.jobs)
    ejecutar_log([str(exe), str(lote), str(salida)],"uam.log")
    if len(traducciones)==len(preparados):
        from verify_shader_cache import verificar
        verificar(salida,lista)
    print(f"Initial cache for the requested list: {salida}. Does not prove coverage of every GLME01 pipeline.")

if __name__ == "__main__":
    main()
