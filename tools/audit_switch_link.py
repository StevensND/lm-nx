"""Audit test NRO symbols and try linking the complete actual core/HLE. Create no stubs and allow no unresolved symbols. Report full-link failure even if both independent tests link."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

RAIZ = Path(__file__).resolve().parents[1]

def auditar(devkitpro=os.environ.get("DEVKITPRO", "/opt/devkitpro"), generados=None, generados_pal=None, nativo=False):
    dkp = Path(devkitpro)
    salida = RAIZ / "build/direct-switch"
    salida.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["DEVKITPRO"] = dkp.as_posix()
    env["DEVKITA64"] = (dkp / "devkitA64").as_posix()
    env["PATH"] = str(dkp / "devkitA64/bin") + os.pathsep + env["PATH"]
    env["TMP"] = env["TEMP"] = str(salida)
    informe = {"nota": "Link test; not run on a console", "binarios": {}}
    opciones=salida/"opciones-optimizacion.json"
    optimizacion=json.loads(opciones.read_text(encoding="utf-8")) if opciones.exists() else {"enlazar":[]}
    informe["optimizacion"]=optimizacion
    manifiesto = json.loads((RAIZ / "platforms/switch/runtime_sources.json").read_text(encoding="utf-8"))
    miembros = subprocess.run([str(dkp / "devkitA64/bin/aarch64-none-elf-ar.exe"), "t",
                              str(salida / "liblmnx_aot_runtime_core.a")], env=env, capture_output=True, check=True)
    cantidad_core = len(miembros.stdout.decode().splitlines())
    informe["alcance_runtime"] = {
        "fuentes_previstas": len(manifiesto["compilables"]) + len(manifiesto["pendientes"]),
        "objetos_en_archivo_core": cantidad_core,
        "manifiesto_completo": cantidad_core == len(manifiesto["compilables"]) + len(manifiesto["pendientes"]),
    }
    if generados:
        from audit_aot_contract import auditar as contrato_aot
        informe["contrato_aot"] = contrato_aot(salida / "liblmnx_game_aot.a",
            [salida / "liblmnx_aot_runtime_core.a", salida / "liblmnx_switch_runtime.a"], devkitpro)
        (salida / "contrato-aot.json").write_text(json.dumps(informe["contrato_aot"],
            indent=2, ensure_ascii=False), encoding="utf-8")
        if generados_pal:
            informe["contrato_aot_pal"] = contrato_aot(salida / "liblmnx_game_aot_pal.a",
                [salida / "liblmnx_aot_runtime_core.a", salida / "liblmnx_switch_runtime.a"], devkitpro, pal=True)
            (salida / "contrato-aot-pal.json").write_text(json.dumps(informe["contrato_aot_pal"], indent=2, ensure_ascii=False), encoding="utf-8")
    for nombre in ("diagnostico", "graphics-probe"):
        elf = salida / f"ProjectLM-NX-{nombre}.elf"
        if not elf.is_file(): raise SystemExit(f"Build {elf} first")
        r = subprocess.run([str(dkp / "devkitA64/bin/aarch64-none-elf-nm.exe"), "-C", str(elf)], env=env, capture_output=True, check=True)
        simbolos = r.stdout.decode(errors="replace")
        (salida / f"simbolos-{nombre}.txt").write_text(simbolos, encoding="utf-8")
        # Do not reject internal names from uam's GLSL frontend.
        prohibidos = [s for s in simbolos.splitlines() if re.search(r"\b(?:dawn::|wgpu::|wgpu[A-Z]|vkCreate\w*|vkQueue\w*|glDraw\w*|egl[A-Z]\w*)", s)]
        indefinidos = [s for s in simbolos.splitlines() if re.match(r"\s+U\s", s)]
        informe["binarios"][nombre] = {"simbolos_prohibidos": prohibidos, "indefinidos_fuertes": indefinidos}
        if prohibidos or indefinidos: raise RuntimeError(f"Symbol audit failed: {nombre}")
    elf = salida / ("ProjectLM-NX.elf" if generados else "intento-runtime-completo.elf")
    # Retain static HLE registrations. Normal archive extraction would omit units anchored only by their
    # constructors.
    orden = [dkp / "devkitA64/bin/aarch64-none-elf-g++.exe", *optimizacion["enlazar"], "-march=armv8-a+crc+crypto", "-mtp=soft",
        "-ftls-model=local-exec", "-fPIE", "-specs=" + str(dkp / "libnx/switch.specs"),
        "-L" + str(dkp / "libnx/lib"), "-Wl,--gc-sections", "-Wl,--wrap=pthread_create",
        "-Wl,-Map=" + str(salida / "runtime-completo.map"), "-Wl,-u,__wrap_pthread_create", "-Wl,--wrap=threadCreate", "-Wl,--wrap=threadClose", "-o", elf,
        *sorted(salida.glob("entrada_juego_*.o")),
        "-Wl,--start-group", "-Wl,--whole-archive", salida / "liblmnx_aot_runtime_core.a"]
    if generados:
        orden.append(salida / "liblmnx_game_aot.a")
        if generados_pal:
            orden.append(salida / "liblmnx_game_aot_pal.a")
    else:
        # Without a real module, linkage must also fail on kGameModule.
        informe["sin_iso"] = True
    bibliotecas = ["lmnx_switch_runtime", "lmnx_sdl3", "lmnx_deko3d", "lmnx_xxhash", "lmnx_imgui", "lmnx_aurora_servicios", "lmnx_nod", "uam"]
    if nativo:
        bibliotecas += ["lmnx_aurora_core", "lmnx_aurora_gx", "lmnx_aurora_gpu", "lmnx_abseil", "lmnx_sqlite", "lmnx_png", "lmnx_zstd"]
    orden += ["-Wl,--no-whole-archive", *[salida / ("lib" + g + ".a") for g in bibliotecas]]
    if nativo:
        orden += [RAIZ / "build/tint-switch/liblmnx_tint.a", "-L" + str(dkp / "portlibs/switch/lib"), "-lz"]
    orden += ["-ldeko3d", "-lnx", "-lm", "-Wl,--end-group"]
    r = subprocess.run([str(x) for x in orden], env=env, cwd=RAIZ, capture_output=True)
    log = r.stdout + r.stderr
    (salida / "enlace-runtime-completo.log").write_bytes(log)
    (salida / "orden-enlace-runtime.json").write_text(json.dumps([str(x) for x in orden], indent=2), encoding="utf-8")
    texto = log.decode(errors="replace")
    pendientes = sorted(set(re.findall(r"undefined reference to [`‘'](.+?)['’]", texto)))
    informe["runtime_completo"] = {"enlaza": r.returncode == 0, "codigo": r.returncode,
        "juego_generado": bool(generados), "simbolos_pendientes": pendientes,
        "log": "enlace-runtime-completo.log", "stubs": False}
    if r.returncode == 0:
        simbolos = subprocess.run([str(dkp / "devkitA64/bin/aarch64-none-elf-nm.exe"), "-C", str(elf)], env=env, capture_output=True, check=True).stdout.decode(errors="replace")
        (salida / "simbolos-juego.txt").write_text(simbolos, encoding="utf-8")
        prohibidos = [s for s in simbolos.splitlines() if re.search(r"\b(?:dawn::(?:native|wire|client)::|wgpu::|wgpu[A-Z]|vkCreate\w*|vkQueue\w*|glDraw\w*|egl[A-Z]\w*)", s)]
        indefinidos = [s for s in simbolos.splitlines() if re.match(r"\s+U\s", s)]
        informe["binarios"]["juego"] = {"simbolos_prohibidos":prohibidos,"indefinidos_fuertes":indefinidos}
        if prohibidos or indefinidos:
            informe["runtime_completo"]["codigo"] = 1
            r.returncode=1
    (salida / "auditoria-enlace.json").write_text(json.dumps(informe, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Two test ELFs have no forbidden graphics APIs or undefined strong symbols. Complete runtime: {'LINKS' if r.returncode == 0 else 'DOES NOT LINK'}; {len(pendientes)} unresolved symbols.")
    return r.returncode or int(any(not informe[k]["contrato_runtime_satisfecho"] for k in ("contrato_aot", "contrato_aot_pal") if k in informe))

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__); p.add_argument("--devkitpro", default=os.environ.get("DEVKITPRO", "/opt/devkitpro"))
    p.add_argument("--generados", type=Path, help="Audit the retail AOT archive built by the direct script")
    p.add_argument("--generados-pal", type=Path)
    p.add_argument("--productor-nativo", action="store_true")
    args = p.parse_args()
    # The CLI reports full-link failure to the caller.
    raise SystemExit(auditar(args.devkitpro, args.generados, args.generados_pal, args.productor_nativo))
