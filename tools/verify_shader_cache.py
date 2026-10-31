"""Verify every entry with the NRO readers and seal the generated cache."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from shader_build_id import RAIZ, identidad


def verificar(salida, lista):
    salida=salida.resolve();lista=lista.resolve()
    if not salida.is_relative_to(RAIZ/"build"):
        raise ValueError("The cache must stay under build/")
    exe=RAIZ/"build/shader-tools/check_native_cache.exe"
    exe.parent.mkdir(parents=True,exist_ok=True)
    cc=Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"))
    env=os.environ.copy();env["PATH"]=str(cc.parent)+os.pathsep+env["PATH"]
    env["TMP"]=env["TEMP"]=str(exe.parent)
    orden=[str(cc),"-std=c++20","-O2","-static","-I"+str(RAIZ/"LMNX-Deko3D/include"),
        "-I"+str(RAIZ/"LMNX-Deko3D/third_party/switchwaker_translate"),str(RAIZ/"tools/shaders/check_native_cache.cpp"),"-o",str(exe)]
    r=subprocess.run(orden,cwd=RAIZ,env=env,capture_output=True)
    (exe.parent/"check_native_cache-compilacion.log").write_bytes(r.stdout+r.stderr);r.check_returncode()
    r=subprocess.run([str(exe),str(salida/"traducciones.tsv"),str(salida/"lista.tsv"),str(salida)],cwd=RAIZ,env=env,capture_output=True)
    (salida/"verificacion.log").write_bytes(r.stdout+r.stderr);r.check_returncode()
    informe=json.loads(r.stdout)
    datos=json.loads(lista.read_text(encoding="utf-8-sig"))
    if informe["etapas_verificadas"]!=len(datos["shaders"]):raise ValueError("Incomplete list coverage")
    build_id=identidad()
    if salida.name!="lmnx-f447-gxabi2-"+build_id:raise ValueError("Identidad obsoleta")
    tint=salida.parent/("lmnx-tint-"+build_id)
    for linea in (salida/"traducciones.tsv").read_text(encoding="utf-8").splitlines():
        if Path(linea.split("\t")[6]).resolve()!=tint.resolve():raise ValueError("Tint cache belongs to another generation")
    informe.update(shader_build_id=build_id,lista=str(lista.relative_to(RAIZ)),lista_sha256=hashlib.sha256(lista.read_bytes()).hexdigest(),
        pipelines_sha256=hashlib.sha256((RAIZ/"data/pipelines.bin").read_bytes()).hexdigest(),
        bytes_dksh_cache=sum(p.stat().st_size for p in salida.glob("*.lmc")),
        bytes_tint_cache=sum(p.stat().st_size for p in tint.glob("*.lmt")))
    for directorio in (salida,tint):
        (directorio/"completo.json").write_text(json.dumps(informe,indent=2,ensure_ascii=False),encoding="utf-8")
    print(json.dumps(informe,ensure_ascii=False))
    return informe


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--lista",type=Path,required=True)
    p.add_argument("--directorio",type=Path)
    a=p.parse_args()
    verificar(a.directorio or RAIZ/"build/cache-inicial"/("lmnx-f447-gxabi2-"+identidad()),a.lista)
