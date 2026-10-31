"""LTO/PGO options shared by direct compilation, linkage and auditing."""
import hashlib
import json
from pathlib import Path
import subprocess
from local_dependencies import RAIZ

# Include Aurora's GX FIFO thread and the deko3d render thread in LTO and PGO.
GRUPOS={"lmnx_aot_runtime_core","lmnx_game_aot","lmnx_game_aot_pal","lmnx_switch_runtime","entrada_juego",
        "lmnx_aurora_gx","lmnx_aurora_core","lmnx_aurora_gpu","lmnx_deko3d"}

def preparar(args,salida,grupos):
    lto=not args.sin_lto
    perfil=args.pgo_dir.resolve()
    if not perfil.is_relative_to(RAIZ/"build"):raise ValueError("Host profile files must stay under build/")
    # Serial LTRANS avoids depending on GNU make/MSYS on Windows hosts.
    compilar=["-flto","-fno-fat-lto-objects"] if lto else []
    enlazar=["-O2","-fwrapv","-flto","--param=lto-partitions=64","-flto-report-wpa"] if lto else []
    h=hashlib.sha256()
    for nombre in sorted(GRUPOS & grupos.keys()):
        for archivo in grupos[nombre][0]:h.update(archivo.relative_to(RAIZ).as_posix().encode()+b"\0"+archivo.read_bytes())
        h.update(json.dumps(grupos[nombre][1:]).encode())
    h.update(Path(__file__).read_bytes())
    for carpeta in (RAIZ/"src",RAIZ/"platforms/switch"):
        for archivo in sorted(carpeta.rglob("*")):
            if archivo.is_file() and archivo.suffix in (".h",".hpp"):h.update(archivo.relative_to(RAIZ).as_posix().encode()+archivo.read_bytes())
    cc=Path(args.devkitpro)/"devkitA64/bin/aarch64-none-elf-g++.exe"
    version=subprocess.run([str(cc),"-dumpfullversion"],capture_output=True,check=True).stdout.decode().strip()
    identidad={"arquitectura":"aarch64-horizon","gcc":version,"inputs_sha256":h.hexdigest(),"lto":lto,"optimizacion":"O2; ffp-contract=off; no-fast-math; no-profile-values"}
    if args.pgo!="OFF":
        perfil.mkdir(parents=True,exist_ok=True)
        comun=["-fprofile-prefix-path="+str(salida.resolve()),"-fno-profile-values"]
        if args.pgo=="GENERATE":
            # update=single, not atomic. On ARMv8.0 every atomic counter is an LDXR/STXR loop; the instrumented
            # build ran at ~10 FPS in the lab and crashed loading the mansion (null J3DMaterial). Lost increments
            # between threads are tolerated by -fprofile-correction in the USE build.
            (perfil/"identidad.json").write_text(json.dumps(identidad,indent=2),encoding="utf-8")
            compilar += ["-fprofile-generate="+str(perfil),"-fprofile-update=single",*comun,
                '-DLMNX_PGO_GENERATE=1','-DLMNX_PGO_HOST_DIR="'+perfil.as_posix()+'"']
            enlazar += ["-fprofile-generate","-Wl,--wrap=fopen","-Wl,-u,__wrap_fopen"]
        else:
            anterior=getattr(args,"pgo_perfil_anterior",False)
            if not (perfil/"identidad.json").is_file():raise ValueError("Profile has no identidad.json")
            guardada=json.loads((perfil/"identidad.json").read_text())
            # --pgo-perfil-anterior: only the sources may differ; compiler, architecture, LTO and flags must match.
            iguales=guardada==identidad if not anterior else {k:v for k,v in guardada.items() if k!="inputs_sha256"}=={k:v for k,v in identidad.items() if k!="inputs_sha256"}
            if not iguales:raise ValueError("Profile does not match this code, compiler, architecture and LTO")
            if not list(perfil.rglob("*.gcda")):raise ValueError("No actual Switch .gcda profiles; PC training is not accepted")
            compilar += ["-fprofile-use="+str(perfil),"-fprofile-partial-training","-fprofile-correction",
                "-Wno-error=coverage-mismatch" if anterior else "-Werror=coverage-mismatch",*comun]
            enlazar += ["-fprofile-use="+str(perfil),"-fprofile-correction"]
    datos={"lto_runtime_aot":lto,"pgo":args.pgo,"perfil_dir":str(perfil),"identidad":identidad,"compilar":compilar,"enlazar":enlazar}
    (salida/"opciones-optimizacion.json").write_text(json.dumps(datos,indent=2,ensure_ascii=False),encoding="utf-8")
    return datos
