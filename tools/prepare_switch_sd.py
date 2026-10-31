"""Verify the game NRO and stage its installation under build/. Does not copy ISOs, write to an SD card or copy existing user saves."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
from shader_build_id import RAIZ, identidad


def verificar():
    salida=RAIZ / "build/direct-switch"
    audit=json.loads((salida/"auditoria-enlace.json").read_text(encoding="utf-8"))
    if not audit["runtime_completo"]["enlaza"] or audit["runtime_completo"]["simbolos_pendientes"]:
        raise ValueError("No validated complete link")
    if audit["binarios"]["juego"]["simbolos_prohibidos"] or audit["binarios"]["juego"]["indefinidos_fuertes"]:
        raise ValueError("ELF contains forbidden or undefined symbols")
    for nombre in ("contrato_aot","contrato_aot_pal"):
        if nombre in audit and not audit[nombre]["contrato_runtime_satisfecho"]:raise ValueError("AOT contract incorrecto")
    elf=salida/"ProjectLM-NX.elf";nro=salida/"ProjectLM-NX.nro"
    if nro.stat().st_mtime<elf.stat().st_mtime:raise ValueError("NRO predates the audited ELF; package it again")
    with elf.open("rb") as f:header=f.read(64)
    if header[:4]!=b"\x7fELF" or header[4]!=2 or struct.unpack_from("<H",header,18)[0]!=183:raise ValueError("ELF is not 64-bit AArch64")
    if identidad().encode("ascii") not in elf.read_bytes():raise ValueError("ELF lacks the current shader identity; rebuild before auditing")
    datos=nro.read_bytes()
    magic,_,size,_=struct.unpack_from("<4I",datos,16)
    if magic!=0x304f524e or size>len(datos) or size<128:raise ValueError("Invalid NRO header")
    segmentos=[]
    for i,nombre in enumerate(("text","rodata","data")):
        offset,n=struct.unpack_from("<II",datos,32+i*8)
        if offset+n>size or offset%4096 or n%4096:raise ValueError("NRO segment out of bounds")
        segmentos.append({"nombre":nombre,"offset":offset,"bytes":n})
    if datos[size:size+4]!=b"ASET":raise ValueError("Missing NACP metadata")
    offset,n=struct.unpack_from("<QQ",datos,size+24)
    if n!=0x4000 or size+offset+n>len(datos):raise ValueError("Invalid NACP")
    titulo=datos[size+offset:size+offset+0x200].split(b"\0",1)[0].decode("utf-8")
    if titulo!="ProjectLM-NX":raise ValueError("Expected the game NRO rather than a diagnostic")
    resultado={"nro":str(nro.relative_to(RAIZ)),"bytes":len(datos),"sha256":hashlib.sha256(datos).hexdigest(),"segmentos":segmentos,
        "bss_bytes":struct.unpack_from("<I",datos,56)[0],"build_id_nro":datos[64:96].hex(),"titulo":titulo,
        "aot_usa":audit["contrato_aot"]["contrato_runtime_satisfecho"],"aot_pal":"contrato_aot_pal" in audit,
        "simbolos_pendientes":0,"consola":False,"shader_build_id":identidad()}
    (salida/"artefacto-juego.json").write_text(json.dumps(resultado,indent=2,ensure_ascii=False),encoding="utf-8")
    return resultado


def main():
    informe=verificar()
    # Compilation alone does not verify rendering or stability. Console testing is needed before publishing a
    # release.
    evidencia=RAIZ/"build/validacion-pc.json"
    checks=("validado_por_usuario","imagen_comparada_dawn","sesion_30_minutos","menu_f1","guardar_cargar")
    validacion=json.loads(evidencia.read_text(encoding="utf-8")) if evidencia.is_file() else {}
    if not all(validacion.get(nombre) is True for nombre in checks):
        retencion=RAIZ/"build/direct-switch/ENTREGA_RETENIDA.txt"
        retencion.write_text("Experimental NRO held pending PC image comparison with Dawn, 30 minutes without crashes, menu and save/load checks. PC testing does not validate Switch execution.\n",encoding="utf-8")
        print("NRO audited but held; not copied to build/sd/. Visual and stability checks are pending.")
        return
    for nombre,archivo in (("pc_nativo_sha256","build/pc-nativo-nmake/ProjectLM-NX-PC.exe"),("pc_dawn_sha256","build/pc-dawn-nmake/ProjectLM-NX-PC-Dawn.exe"),("nro_sha256",informe["nro"])):
        if validacion.get(nombre)!=hashlib.sha256((RAIZ/archivo).read_bytes()).hexdigest():raise ValueError("PC validation is stale or belongs to another candidate: "+nombre)
    pc=[json.loads((RAIZ/carpeta/"artefacto-pc.json").read_text(encoding="utf-8")) for carpeta in ("build/pc-nativo-nmake","build/pc-dawn-nmake")]
    if pc[0]["aot_hle_mismos_fuentes"]!=pc[1]["aot_hle_mismos_fuentes"]:raise ValueError("The two PC builds do not use the same AOT/HLE")
    cache=RAIZ/"build/cache-inicial"
    nombres=("lmnx-tint-"+informe["shader_build_id"],"lmnx-f447-gxabi2-"+informe["shader_build_id"])
    if not all((cache/nombre/"completo.json").is_file() for nombre in nombres):raise ValueError("Regenerate and seal this candidate's shader cache first")
    destino=RAIZ/"build/sd/ProjectLM-NX";destino.mkdir(parents=True,exist_ok=True)
    shutil.copy2(RAIZ/informe["nro"],destino/"ProjectLM-NX.nro")
    shutil.copy2(RAIZ/"data/pipelines.bin",destino/"pipelines.bin")
    (destino/"orig").mkdir(exist_ok=True)
    if (RAIZ/"build/discos-switch/hidden_mansion/manifest.txt").is_file():shutil.copytree(RAIZ/"build/discos-switch/hidden_mansion",destino/"guardados/hidden_mansion",dirs_exist_ok=True)
    for nombre in nombres:shutil.copytree(cache/nombre,destino/"cache"/nombre,dirs_exist_ok=True)
    (destino/"NO_ENTREGAR.txt").unlink(missing_ok=True)
    (destino/"CANDIDATO_EXPERIMENTAL.txt").write_text("PC validation recorded for these hashes. Actual Switch execution remains unverified. Complete console checks before release. No ISOs were copied.\n",encoding="utf-8")
    informe.update({"pc_validado_por_usuario":True,"consola":False,"isos_copiadas":False})
    (destino/"verificacion.json").write_text(json.dumps(informe,indent=2,ensure_ascii=False),encoding="utf-8")
    print("Installation staged using recorded PC validation; actual Switch execution remains unverified.")



if __name__=="__main__":main()
