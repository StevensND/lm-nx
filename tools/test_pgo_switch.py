"""Compile/link checks for actual Horizon PGO counters. Does not run NROs, create training profiles or alter the game build."""
from pathlib import Path
import json
import os
import subprocess
from local_dependencies import RAIZ

def main():
    destino=RAIZ/"build/check-pgo-switch";destino.mkdir(parents=True,exist_ok=True)
    dkp=Path(os.environ.get("DEVKITPRO", "/opt/devkitpro"));cc=dkp/"devkitA64/bin/aarch64-none-elf-g++.exe"
    env=os.environ.copy();env["TMP"]=env["TEMP"]=str(destino);env["DEVKITPRO"]=dkp.as_posix()
    fuente=destino/"contador.cpp"
    fuente.write_text('''#include <cstdio>
extern "C" void lmnx_pgo_volcar();
int main(int argc,char**){volatile int total=0;for(int i=0;i<argc;++i)total=total+i;lmnx_pgo_volcar();return total;}
''',encoding="utf-8")
    perfil=destino/"perfil";perfil.mkdir(exist_ok=True)
    comun=[str(cc),"-O2","-std=c++20","-flto","-fno-profile-values","-fprofile-prefix-path="+str(destino),"-march=armv8-a+crc+crypto","-mtune=cortex-a57","-fPIE","-mtp=soft","-ftls-model=local-exec"]
    ordenes=[[*comun,"-fprofile-generate="+str(perfil),"-fprofile-update=atomic","-c",str(fuente),"-o","contador.o"],
      [*comun,"-DLMNX_PGO_GENERATE=1",'-DLMNX_PGO_HOST_DIR="'+perfil.as_posix()+'"',"-c",str(RAIZ/"platforms/switch/pgo_switch.cpp"),"-o",str(destino/"rutas.o")],
      [*comun,"-fprofile-generate","-specs="+str(dkp/"libnx/switch.specs"),"-L"+str(dkp/"libnx/lib"),str(destino/"contador.o"),str(destino/"rutas.o"),"-Wl,--wrap=fopen","-Wl,-u,__wrap_fopen","-lnx","-o",str(destino/"test.elf")]]
    for i,orden in enumerate(ordenes):
        r=subprocess.run(orden,cwd=destino,env=env,capture_output=True);(destino/f"paso-{i}.log").write_bytes(r.stdout+r.stderr);r.check_returncode()
    r=subprocess.run([str(dkp/"devkitA64/bin/aarch64-none-elf-nm.exe"),str(destino/"test.elf")],capture_output=True,check=True)
    simbolos=r.stdout.decode();assert "__gcov_dump" in simbolos and "__wrap_fopen" in simbolos
    incrustado=(destino/"test.elf").read_bytes()
    assert (str(perfil)+"\\contador.gcda").encode() in incrustado, "gcda must have a flat name rather than a nested Windows path"
    informe={"enlace_aarch64":True,"lto":True,"contadores_atomicos":True,"perfil_valores":False,"gcov_real":True,"redireccion_fopen":True,"ejecutado":False,"perfil_entrenado":False}
    (destino/"informe.json").write_text(json.dumps(informe,indent=2,ensure_ascii=False),encoding="utf-8");print(json.dumps(informe,ensure_ascii=False))
if __name__=="__main__":main()
