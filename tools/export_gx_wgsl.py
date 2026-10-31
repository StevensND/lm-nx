"""Read CRC-checked pipelines.bin and run Aurora's exact GX generator on PC."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zlib
from local_dependencies import RAIZ, includes_locales, fuente
from prepare_aurora_native import preparar_nativo


def main():
    salida=RAIZ / "build/fase4/gx-export"
    salida.mkdir(parents=True,exist_ok=True)
    original=(RAIZ / "data/pipelines.bin").read_bytes()
    magic,version,flags,count,tamano=struct.unpack_from("<4IQ",original)
    if magic!=0x43505541 or version not in (1,2) or tamano>64*1024*1024 or flags&~1:raise ValueError("Unknown pipeline header")
    cabecera=32 if version==2 else 24
    payload=original[cabecera:]
    if flags&1:
        dll=ctypes.CDLL(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/libzstd.dll"))
        dll.ZSTD_decompress.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t];dll.ZSTD_decompress.restype=ctypes.c_size_t
        destino=ctypes.create_string_buffer(tamano)
        n=dll.ZSTD_decompress(destino,tamano,payload,len(payload))
        if n!=tamano:raise ValueError("Error al descomprimir la lista")
        payload=destino.raw
    if len(payload)!=tamano or (version==2 and zlib.crc32(payload)!=struct.unpack_from("<I",original,24)[0]):raise ValueError("CRC de lista incorrecto")
    offset=0;gx=[];otros=[]
    for i in range(count):
        tipo,revision,n,primer_frame=struct.unpack_from("<BIII",payload,offset);offset+=13
        crc=None
        if version==2:crc=struct.unpack_from("<I",payload,offset)[0];offset+=4
        config=payload[offset:offset+n];offset+=n
        if len(config)!=n or (crc is not None and crc!=zlib.crc32(config)):raise ValueError("CRC de fila incorrecto")
        if tipo==1:
            if revision!=15:raise ValueError("Unsupported GX version: "+str(revision))
            gx.append(config)
        else:otros.append({"tipo":tipo,"version":revision,"bytes":n})
    if offset!=len(payload):raise ValueError("Bytes sobrantes en la lista")
    (salida/"configuraciones.bin").write_bytes(struct.pack("<I",len(gx))+b"".join(struct.pack("<I",len(c))+c for c in gx))
    aurora=preparar_nativo();binarios=Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin"))
    env=os.environ.copy();env["PATH"]=str(binarios)+os.pathsep+env["PATH"];env["TMP"]=env["TEMP"]=str(salida)
    fuentes=[aurora / "lib" / f for f in ("gx/shader.cpp","gx/shader_info.cpp","gx/gx.cpp","gx/attr_fmt.cpp","logging.cpp")]+[RAIZ/"tools/export_gx_wgsl.cpp"]
    fuentes += [fuente("abseil-cpp") / "absl/container/internal/raw_hash_set.cc"]
    includes=[aurora/"include",aurora/"include/dolphin",aurora/"lib",RAIZ/"LMNX-Deko3D/include",RAIZ/"LMNX-Deko3D/third_party/xxhash",RAIZ/"platforms/switch/sdl3_headers/SDL3",*includes_locales()]
    objetos=[]
    for i,src in enumerate(fuentes):
        obj=salida/(str(i)+".o");objetos.append(obj)
        orden=[str(binarios/"g++.exe"),"-std=c++20","-O2","-flto","-DAURORA=1","-DTARGET_PC=1","-DFMT_HEADER_ONLY=1","-DXXH_INLINE_ALL=1","-ffunction-sections","-fdata-sections",*["-I"+str(p) for p in includes],"-c",str(src),"-o",str(obj)]
        sello=obj.with_suffix(".sha256");huella=hashlib.sha256((json.dumps(orden)+hashlib.sha256(src.read_bytes()).hexdigest()).encode()).hexdigest()
        if not obj.exists() or not sello.exists() or sello.read_text()!=huella:
            r=subprocess.run(orden,cwd=RAIZ,env=env,capture_output=True);(salida/(str(i)+".log")).write_bytes(r.stdout+r.stderr);r.check_returncode();sello.write_text(huella)
    exe=salida/"export_gx_wgsl.exe"
    orden=[str(binarios/"g++.exe"),"-static","-flto","-Wl,--gc-sections",*[str(o) for o in objetos],"-o",str(exe)]
    r=subprocess.run(orden,cwd=RAIZ,env=env,capture_output=True);(salida/"enlace.log").write_bytes(r.stdout+r.stderr);r.check_returncode()
    r=subprocess.run([str(exe),str(salida/"configuraciones.bin"),str(salida/"wgsl")],cwd=RAIZ,env=env,capture_output=True);(salida/"exportacion.log").write_bytes(r.stdout+r.stderr);print(r.stdout.decode(errors="replace"));r.check_returncode()
    (salida/"cobertura.json").write_text(json.dumps({"filas":count,"gx":len(gx),"otros":otros,"fuente":"patched Aurora GX generator","consola":False,"variantes":"recorded configuration, intTev 0/1, dstAlpha None/Replace/DualSource; further coverage pending"},indent=2),encoding="utf-8")


if __name__=="__main__":main()
