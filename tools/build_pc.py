"""Build the Windows native-producer test without running the game.

LMNX_BANJO_NX selects the banjo-nx source tree (default: extern/fuentes/banjo-nx).
LMNX_MINGW_BIN selects host GCC/binutils; otherwise use gcc from PATH.
CMAKE and LMNX_MAKE override cmake and nmake from PATH.
The Dawn reference path also needs VULKAN_SDK pointing to an installed SDK.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import struct
from local_dependencies import RAIZ,fuente
from prepare_aurora_native import preparar_nativo
from prepare_aurora_switch import preparar
from abseil_switch_sources import fuentes_abseil
from pc_diagnostico import instrumentar
from shader_build_id import identidad as identidad_shaders

def sincronizar_instrumentado(original, destino, dawn=False):
    sello=destino/"pc-instrumentacion.sha256"
    base=original/("lmnx-parches.txt" if dawn else "lmnx-nativo.txt")
    firma=hashlib.sha256(base.read_bytes()+(RAIZ/"tools/pc_diagnostico.py").read_bytes()+Path(__file__).read_bytes()).hexdigest()
    if sello.is_file() and sello.read_text()==firma:return
    procesado=RAIZ/"build/pc-fuentes"/("procesado-"+firma[:16])
    terminado=procesado/"pc-procesado-completo.txt"
    if not terminado.is_file():
        for archivo in original.rglob("*"):
            if not archivo.is_file():continue
            datos=archivo.read_bytes()
            if dawn and archivo.suffix in (".cpp",".hpp",".h"):
                s=datos.decode("utf-8").replace("#include <magic_enum.hpp>","")
                datos=re.sub(r"magic_enum::enum_name\(([\w.]+)\)",r"static_cast<uint32_t>(\1)",s).encode("utf-8")
            salida=procesado/archivo.relative_to(original);salida.parent.mkdir(parents=True,exist_ok=True);salida.write_bytes(datos)
        instrumentar(procesado)
        terminado.write_text(firma)
    # Compare final output rather than restoring and repatching headers; identical bytes must not cause a full
    # rebuild.
    for archivo in procesado.rglob("*"):
        if not archivo.is_file():continue
        salida=destino/archivo.relative_to(procesado);datos=archivo.read_bytes()
        if not salida.is_file() or salida.read_bytes()!=datos:
            salida.parent.mkdir(parents=True,exist_ok=True);salida.write_bytes(datos)
    sello.write_text(firma)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("--jobs",type=int,default=2);p.add_argument("--tracy",action="store_true");p.add_argument("--configurar",action="store_true");p.add_argument("--referencia-dawn",action="store_true")
    p.add_argument("--generados",type=Path,default=RAIZ/"build/glme01-locales-fase4")
    p.add_argument("--generados-pal",type=Path,default=RAIZ/"build/glmp01-locales-fase4")
    p.add_argument("--sin-pal",action="store_true",help="Build USA only when no compatible PAL disc is supplied")
    a=p.parse_args()
    for ruta in (a.generados,a.generados_pal):
        if not ruta.resolve().is_relative_to(RAIZ/"build"):raise ValueError("AOT must stay under build/")
    salida=RAIZ/("build/pc-dawn-nmake" if a.referencia_dawn else "build/pc-nativo-nmake");salida.mkdir(parents=True,exist_ok=True)
    copias=RAIZ/"build/pc-fuentes";copias.mkdir(exist_ok=True)
    base=Path(os.environ.get("LMNX_BANJO_NX", str(RAIZ/"extern/fuentes/banjo-nx")))
    for nombre,origen in [("SDL",base/"sdk/thirdparty/sdl3"),("plume-vulkan",base/"app/thirdparty/plume")]:
        destino=copias/nombre
        if not (destino/"lmnx-completo.json").exists():
            if nombre=="SDL":shutil.copytree(origen,destino,dirs_exist_ok=True)
            else:
                destino.mkdir(exist_ok=True)
                for n in ["plume_vulkan.cpp","plume_vulkan.h","plume_vma_names.h","plume_volk_names.h","LICENSE"]:shutil.copy2(origen/n,destino/n)
                for n in ["volk","Vulkan-Headers","VulkanMemoryAllocator"]:shutil.copytree(origen/"contrib"/n,destino/"contrib"/n,dirs_exist_ok=True)
            (destino/"lmnx-completo.json").write_text(json.dumps({"origen_solo_lectura":str(origen),"sha256":hashlib.sha256((origen/("CMakeLists.txt" if nombre=="SDL" else "plume_vulkan.cpp")).read_bytes()).hexdigest()},indent=2),encoding="utf-8")
    vk=copias/"plume-vulkan/plume_vulkan.cpp"
    texto=vk.read_text(encoding="utf-8")
    if "dynamicStates.emplace_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);" not in texto:
        marca="dynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);"
        if texto.count(marca)!=1:raise ValueError("plume/Vulkan pipeline creation changed")
        texto=texto.replace(marca,marca+"\n        // LMNX PC: SetBlendConstant changes per draw, as in deko3d.\n        dynamicStates.emplace_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);")
        vk.write_text(texto,encoding="utf-8")
    # Switch memory is CPU-visible and Aurora uses mappable UPLOAD buffers as CopyBufferToBuffer destinations, so
    # the PC path also needs TRANSFER_DST.
    if "LMNX PC: copy destination" not in texto:
        marca="case RenderHeapType::UPLOAD:\n            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;"
        if texto.count(marca)!=1:raise ValueError("plume/Vulkan buffer creation changed")
        texto=texto.replace(marca,marca+"\n            // LMNX PC: copy destination, like deko3d unified memory.\n            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;")
        vk.write_text(texto,encoding="utf-8")
    if "LMNX: memory coherence" not in texto:
        marca="void VulkanBuffer::unmap(uint32_t subresource, const RenderRange *writtenRange) {"
        assert texto.count(marca)==1
        texto=texto.replace(marca,marca+'''
        // LMNX: memory coherence. Unmap without a range means the whole buffer.
        RenderRange completo{0, desc.size};
        if (writtenRange == nullptr) writtenRange = &completo;
''')
        marca="void *VulkanBuffer::map(uint32_t subresource, const RenderRange *readRange) {"
        assert texto.count(marca)==1
        texto=texto.replace(marca,marca+'''
        if (desc.heapType == RenderHeapType::READBACK)
            vmaInvalidateAllocation(device->allocator,allocation,0,desc.size);
''')
        # Aurora uses one viewport even when writing color and normals.
        texto=texto.replace("viewportState.viewportCount = renderTargetCount;", "viewportState.viewportCount = 1;")
        texto=texto.replace("viewportState.scissorCount = renderTargetCount;", "viewportState.scissorCount = 1;")
        vk.write_text(texto,encoding="utf-8")
    if a.referencia_dawn:
        # Isolate SDK Vulkan headers so old SDK SPIR-V headers do not precede Dawn DEPS's exact SPIRV-Headers/Tools
        # revisions.
        for nombre in ("vulkan","vk_video"):
            destino=copias/"vulkan-sdk/include"/nombre
            if not destino.exists():shutil.copytree((Path(os.environ["VULKAN_SDK"])/"Include")/nombre,destino)
        original=preparar();aurora=RAIZ/"build"/("aurora-pc-dawn-"+original.name)
        sincronizar_instrumentado(original,aurora,dawn=True)
    else:
        original=preparar_nativo();aurora=copias/"aurora-nativo"
        # Stable path: template changes must not alter all AOT include paths. Do not rewrite identical files or modify
        # sealed snapshots.
        sincronizar_instrumentado(original,aurora)
    runtime=json.loads((RAIZ/"platforms/switch/runtime_sources.json").read_text(encoding="utf-8"))
    servicios=json.loads((RAIZ/"platforms/switch/aurora_sources.json").read_text(encoding="utf-8"))["servicios"]
    gx=re.findall(r"lib/[\w/]+\.cpp",(aurora/"cmake/aurora_gx.cmake").read_text().split("add_library(aurora::gx")[0])
    core=["lib/"+n+".cpp" for n in ["aurora","window","device","gamepad","time","system_info","input/binding","input/capture","input/router","input/sdl_input","input/source_state","dolphin/pad/pad","dolphin/pad/adapter","dolphin/si/si","dolphin/vi/vi","nativegpu/gpu","nativegpu/postfx","imgui"]]
    if a.referencia_dawn:
        core=[n.replace("nativegpu/","webgpu/") for n in core]
        core += ["lib/webgpu/gpu_cache.cpp","lib/webgpu/gpu_prof.cpp","lib/dawn/BackendBinding.cpp","lib/dawn/TracyPlatform.cpp"]
    grupos={"runtime":[RAIZ/n for n in runtime["compilables"]]+[RAIZ/n for n in runtime["pendientes"]],
        "aurora_sources":[aurora/n for n in sorted(set(servicios+gx+core))],"abseil":fuentes_abseil(),
        "aot":[*sorted(a.generados.glob("funcs_*.cpp")),a.generados/"func_table.cpp",*([] if a.sin_pal else [*sorted(a.generados_pal.glob("funcs_*.cpp")),a.generados_pal/"func_table.cpp"])]}
    for carpeta,sha in [(a.generados,"77d49225e7d28b9777916cdbe5809345c570ecbc"),*([] if a.sin_pal else [(a.generados_pal,"a1e2bd82813787cfe0b11afb5d138f08fdbd94f1")])]:
        if (carpeta/"dol.sha1").read_text().strip()!=sha:raise ValueError("Generated AOT has not been validated")
    tint=json.loads((RAIZ/"build/tint-directo/resultados.json").read_text())
    objetos=[r["objeto"] for r in tint if Path(r["fuente"]).name!="wgsl_to_glsl.cpp"]
    cc=Path(os.environ.get("LMNX_MINGW_BIN", str(Path(shutil.which("gcc") or "gcc").resolve().parent)));env=os.environ.copy();env["PATH"]=str(cc)+os.pathsep+env["PATH"];env["TMP"]=env["TEMP"]=str(salida)
    def ejecutar(orden,nombre):
        with (salida/nombre).open("wb") as f:
            r=subprocess.run([str(x) for x in orden],cwd=RAIZ,env=env,stdout=f,stderr=subprocess.STDOUT)
        if r.returncode:raise RuntimeError(f"PC failure: {nombre}; code {r.returncode}")
    respuesta=salida/"tint.rsp";respuesta.write_text("\n".join('"'+str(x).replace('\\','/')+'"' for x in objetos),encoding="utf-8")
    (salida/"libtint_pc.a").unlink(missing_ok=True)
    ejecutar([cc/"ar.exe","rcs",salida/"libtint_pc.a","@"+str(respuesta)],"tint-archivo.log")
    def q(x):return '"'+str(x).replace('\\','/')+'"'
    manifiesto=["set(LMNX_PAL "+("OFF" if a.sin_pal else "ON")+")","set(LMNX_SHADER_ID "+identidad_shaders()+")"]
    for nombre,archivos in grupos.items():manifiesto+=["set(LMNX_"+nombre.upper()+"\n"+"\n".join(q(x) for x in archivos)+"\n)"]
    for nombre in ["abseil-cpp","fmt","imgui","sqlite","libpng","zlib-ng","zstd","glslang","tracy","dawn","spirv-headers","spirv-tools"]:
        ruta=RAIZ/"extern/fuentes/tracy-0.14.1" if nombre=="tracy" else fuente("zlib" if nombre=="zlib-ng" else nombre)
        manifiesto.append("set(DEP_"+nombre.replace('-','_').upper()+" "+q(ruta)+")")
    manifiesto+=["set(LMNX_RAIZ "+q(RAIZ)+")","set(LMNX_AURORA "+q(aurora)+")","set(LMNX_COPIAS "+q(copias)+")"]
    (salida/"fuentes.cmake").write_text("\n".join(manifiesto),encoding="utf-8")
    cmake=os.environ.get("CMAKE", shutil.which("cmake") or "cmake")
    nmake=Path(os.environ.get("LMNX_MAKE", shutil.which("nmake") or "nmake"))
    ejecutar([cmake,"-S",RAIZ/"platforms/pc","-B",salida,"-G","NMake Makefiles","-DCMAKE_MAKE_PROGRAM="+str(nmake),"-DCMAKE_C_COMPILER="+str(cc/"gcc.exe"),"-DCMAKE_CXX_COMPILER="+str(cc/"g++.exe"),"-DCMAKE_BUILD_TYPE=Release","-DLMNX_TRACY="+("ON" if a.tracy else "OFF"),"-DLMNX_DAWN_REFERENCE="+("ON" if a.referencia_dawn else "OFF")],"configuracion.log")
    if a.configurar:
        print("PC configured");return
    ejecutar([cmake,"--build",salida,"--parallel",str(a.jobs)],"compilacion.log")
    exe=salida/("ProjectLM-NX-PC-Dawn.exe" if a.referencia_dawn else "ProjectLM-NX-PC.exe")
    datos=exe.read_bytes();offset=struct.unpack_from("<I",datos,60)[0]
    if datos[:2]!=b"MZ" or datos[offset:offset+4]!=b"PE\0\0" or struct.unpack_from("<H",datos,offset+4)[0]!=0x8664:raise ValueError("Not a Windows x64 executable")
    r=subprocess.run([str(cc/"nm.exe"),"-C",str(exe)],env=env,capture_output=True,check=True)
    (salida/"simbolos.txt").write_bytes(r.stdout)
    prohibidos=[] if a.referencia_dawn else [s for s in r.stdout.decode(errors="replace").splitlines() if re.search(r"\b(?:dawn::(?:native|wire|client)::|wgpu::|wgpu[A-Z])",s)]
    if prohibidos:raise ValueError("The native test contains Dawn symbols: "+str(prohibidos[:3]))
    informe={"exe":str(exe.relative_to(RAIZ)),"sha256":hashlib.sha256(datos).hexdigest(),"bytes":len(datos),"renderer":"Dawn-referencia" if a.referencia_dawn else "LMNX/plume/Vulkan",
        "aot_hle_mismos_fuentes":{str(s.relative_to(RAIZ)):hashlib.sha256(s.read_bytes()).hexdigest() for s in grupos["runtime"]+grupos["aot"]},
        "dawn_en_renderer_nativo":bool(prohibidos),"tracy":a.tracy,"juego_ejecutado":False,"valida_switch":False}
    (salida/"artefacto-pc.json").write_text(json.dumps(informe,indent=2,ensure_ascii=False),encoding="utf-8")
    for n in ("config","guardados","cache"):(salida/"datos"/n).mkdir(parents=True,exist_ok=True)
    shutil.copy2(RAIZ/"data/pipelines.bin",salida/"pipelines.bin")
    if (RAIZ/"build/discos-switch/hidden_mansion/manifest.txt").is_file():
        shutil.copytree(RAIZ/"build/discos-switch/hidden_mansion",salida/"datos/guardados/hidden_mansion",dirs_exist_ok=True)
    print("PC build complete; the game was not run: "+str(exe))

if __name__=="__main__":main()
