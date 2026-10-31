"""Builds lib/codecs.wasm of the installer page: the decompressors it needs to read RVZ, WIA and GCZ disc images
(zstd 1.5.7, the LZMA SDK 26.05 decoders and bzip2 1.0.8), with tools/disc_codecs_web.c.

    py -3 tools/build_disc_codecs_web.py [--out installer/lib/codecs.wasm]
"""
import argparse
from pathlib import Path
import subprocess
import sys

from build_shader_forge import toolchain
from local_dependencies import RAIZ, fuente

EXPORTS = ["zw_alloc", "zw_free", "zw_zstd", "zw_lzma", "zw_lzma2", "zw_bzip2"]


def build(out):
    zstd = fuente("zstd") / "lib"
    lzma = fuente("lzma-sdk") / "C"
    bzip2 = fuente("bzip2")
    env, cc, _ = toolchain(True)
    sources = [RAIZ / "tools/disc_codecs_web.c"]
    sources += [zstd / f for f in (
        "common/debug.c", "common/entropy_common.c", "common/error_private.c", "common/fse_decompress.c",
        "common/xxhash.c", "common/zstd_common.c", "decompress/huf_decompress.c", "decompress/zstd_ddict.c",
        "decompress/zstd_decompress.c", "decompress/zstd_decompress_block.c")]
    sources += [lzma / f for f in ("LzmaDec.c", "Lzma2Dec.c")]
    sources += [bzip2 / f for f in ("bzlib.c", "decompress.c", "huffman.c", "crctable.c", "randtable.c",
                                    "compress.c", "blocksort.c")]
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(cc), "-O3", "-flto", "-DNDEBUG", "-DZSTD_DISABLE_ASM", "-DZSTD_MULTITHREAD=0", "-DZ7_ST", "-DBZ_NO_STDIO",
           "-I" + str(zstd), "-I" + str(zstd / "common"), "-I" + str(lzma), "-I" + str(bzip2), *map(str, sources),
           "-o", str(out), "--no-entry", "-sSTANDALONE_WASM=1", "-sALLOW_MEMORY_GROWTH=1", "-sINITIAL_MEMORY=16MB",
           "-sFILESYSTEM=0", "-sEXPORTED_FUNCTIONS=" + ",".join("_" + e for e in EXPORTS), "-Wl,--gc-sections"]
    r = subprocess.run(cmd, cwd=RAIZ, env=env, capture_output=True)
    if r.returncode:
        sys.exit((r.stdout + r.stderr).decode(errors="replace")[-4000:])
    print(f"built {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", type=Path, default=RAIZ / "build/disc-codecs-web/codecs.wasm")
    build(p.parse_args().out)
