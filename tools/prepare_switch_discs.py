"""Validate USA/PAL discs and prepare only overlay files under build/."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import struct
import subprocess
import sys

from prepare_glme01 import RAIZ, extraer


class DiscoISO:
    """Bounded GameCube FST reader; does not copy or modify the ISO."""
    def __init__(self, ruta, version):
        self.ruta = ruta.resolve()
        self.dol = extraer(self.ruta, version)
        self.archivos = {}
        with self.ruta.open("rb") as f:
            longitud = f.seek(0, 2)
            f.seek(0x424)
            inicio, tamano = struct.unpack(">II", f.read(8))
            if tamano < 12 or tamano > 16 * 1024 * 1024 or inicio + tamano > longitud:
                raise ValueError("FST outside ISO")
            f.seek(inicio)
            fst = f.read(tamano)
        if fst[0] != 1:
            raise ValueError("Invalid FST root")
        total = struct.unpack_from(">I", fst, 8)[0]
        if not total or total * 12 > len(fst):
            raise ValueError("Tabla FST truncada")
        pila = [("", total, 0)]
        for i in range(1, total):
            while pila and i >= pila[-1][1]:
                pila.pop()
            if not pila:
                raise ValueError("Invalid FST hierarchy")
            tipo_nombre, offset, size = struct.unpack_from(">III", fst, i * 12)
            tipo = tipo_nombre >> 24
            pos = total * 12 + (tipo_nombre & 0xFFFFFF)
            if pos >= len(fst):
                raise ValueError("FST name out of bounds")
            fin = fst.find(b"\0", pos)
            if fin < 0:
                raise ValueError("Unterminated FST name")
            nombre = fst[pos:fin].decode("shift_jis")
            if not nombre or nombre in (".", "..") or any(c in nombre for c in "/\\:"):
                raise ValueError("Nombre FST inseguro")
            ruta_archivo = pila[-1][0] + nombre
            if tipo == 1:
                if offset != pila[-1][2] or not i < size <= pila[-1][1]:
                    raise ValueError("Invalid FST directory")
                pila.append((ruta_archivo + "/", size, i))
            elif tipo == 0:
                if offset + size > longitud or ruta_archivo in self.archivos:
                    raise ValueError("Invalid FST file")
                self.archivos[ruta_archivo] = (offset, size)
            else:
                raise ValueError("Unknown FST type")

    def leer(self, nombre):
        offset, size = self.archivos[nombre]
        with self.ruta.open("rb") as f:
            f.seek(offset)
            datos = f.read(size)
        if len(datos) != size:
            raise ValueError("Incomplete ISO read")
        return datos


def preparar(us, pal=None, exigir_pal=False):
    destino = RAIZ / "build/discos-switch"
    destino.mkdir(parents=True, exist_ok=True)
    informe = {}
    pal = pal or RAIZ / "orig/GLMP01.iso"
    compatible_pal = False
    for version, ruta, mapa in [("GLME01", us, "map2.szp"), ("GLMP01", pal, "map2u.szp")]:
        try:
            disco = DiscoISO(ruta, version)
        except (ValueError, FileNotFoundError) as error:
            if version != "GLMP01":
                raise
            registro = {"compatible": False, "motivo": str(error)}
            if ruta.is_file():
                with ruta.open("rb") as f:
                    cabecera = f.read(0x440)
                    registro["revision"] = cabecera[7]
                    inicio = struct.unpack_from(">I", cabecera, 0x420)[0]
                    f.seek(inicio)
                    h = f.read(0x100)
                    size = max([0x100] + [struct.unpack_from(">I", h, i * 4)[0] + struct.unpack_from(">I", h, 0x90 + i * 4)[0]
                                        for i in range(18) if struct.unpack_from(">I", h, 0x90 + i * 4)[0]])
                    if size <= 32 * 1024 * 1024 and inicio + size <= ruta.stat().st_size:
                        f.seek(inicio)
                        registro["dol_sha1"] = hashlib.sha1(f.read(size)).hexdigest()
            informe[version] = registro
            print("Incompatible PAL disc; keeping USA as base: " + str(error))
            continue
        if version == "GLMP01":
            compatible_pal = True
        nombres = ["Map/" + mapa] + [f"Event/event{n}.szp" for n in ("08", "16", "73")]
        extraidos = []
        for nombre in nombres:
            datos = disco.leer(nombre)
            salida = destino / version / "files" / PurePosixPath(nombre)
            salida.parent.mkdir(parents=True, exist_ok=True)
            salida.write_bytes(datos)
            extraidos.append({"ruta": nombre, "bytes": len(datos), "sha256": hashlib.sha256(datos).hexdigest()})
        informe[version] = {"compatible": True, "dol_sha1": hashlib.sha1(disco.dol).hexdigest(), "archivos_fst": len(disco.archivos), "extraidos": extraidos}
    if not compatible_pal:
        informe["overlay"] = None
        (destino / "informe.json").write_text(json.dumps(informe, indent=2, ensure_ascii=False), encoding="utf-8")
        if exigir_pal:
            raise ValueError("Requested PAL disc is incompatible; no overlay generated")
        return None
    overlay = destino / "hidden_mansion"
    subprocess.run([sys.executable, str(RAIZ / "tools/hidden_mansion_data.py"),
                    "--us", str(destino / "GLME01"), "--pal", str(destino / "GLMP01"),
                    "--out", str(overlay)], cwd=RAIZ, check=True)
    informe["overlay"] = str(overlay.relative_to(RAIZ))
    (destino / "informe.json").write_text(json.dumps(informe, indent=2, ensure_ascii=False), encoding="utf-8")
    print("USA/PAL validated; overlay prepared in " + str(overlay))
    return overlay


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--us", type=Path, default=RAIZ / "orig/GLME01.iso")
    p.add_argument("--pal", type=Path, help="Optional compatible PAL disc; validated before generating the overlay")
    args = p.parse_args()
    preparar(args.us, args.pal, exigir_pal=args.pal is not None)
