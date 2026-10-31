// The decompressors of the installer page (installer/lib/codecs.wasm): the page reads RVZ, WIA and GCZ disc
// images without converting them first. Built by tools/build_disc_codecs_web.py. zstd for RVZ; bzip2, LZMA and LZMA2
// (LZMA SDK) for RVZ and WIA made with those methods.
#include <stdlib.h>
#include <zstd.h>

#include "Lzma2Dec.h"
#include "LzmaDec.h"
#include "bzlib.h"

static ZSTD_DCtx* sCtx;

void* zw_alloc(size_t size) { return malloc(size); }

void zw_free(void* p) { free(p); }

// Each one decompresses src into dst and returns the decompressed size, or -1 on an error or when dst is too small.

long zw_zstd(void* dst, size_t capacity, const void* src, size_t size) {
    if (!sCtx && !(sCtx = ZSTD_createDCtx())) return -1;
    const size_t r = ZSTD_decompressDCtx(sCtx, dst, capacity, src, size);
    return ZSTD_isError(r) ? -1 : (long)r;
}

static void* lzmaAlloc(ISzAllocPtr p, size_t size) { (void)p; return malloc(size); }
static void lzmaFree(ISzAllocPtr p, void* address) { (void)p; free(address); }
static const ISzAlloc sLzmaAlloc = {lzmaAlloc, lzmaFree};

// A raw LZMA stream (no .lzma header); props are its 5 property bytes. It stops when dst is full.
long zw_lzma(void* dst, size_t capacity, const void* src, size_t size, const unsigned char* props) {
    SizeT out = capacity, in = size;
    ELzmaStatus status;
    const SRes r = LzmaDecode(dst, &out, src, &in, props, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &sLzmaAlloc);
    return r == SZ_OK ? (long)out : -1;
}

// A raw LZMA2 stream; prop is its dictionary-size byte.
long zw_lzma2(void* dst, size_t capacity, const void* src, size_t size, unsigned prop) {
    SizeT out = capacity, in = size;
    ELzmaStatus status;
    const SRes r = Lzma2Decode(dst, &out, src, &in, (Byte)prop, LZMA_FINISH_ANY, &status, &sLzmaAlloc);
    return r == SZ_OK ? (long)out : -1;
}

// A whole bzip2 stream.
long zw_bzip2(void* dst, size_t capacity, const void* src, size_t size) {
    unsigned int out = (unsigned int)capacity;
    const int r = BZ2_bzBuffToBuffDecompress(dst, &out, (char*)src, (unsigned int)size, 0, 0);
    return r == BZ_OK ? (long)out : -1;
}

// bzip2 built without stdio asks the program what to do on an internal error.
void bz_internal_error(int code) { (void)code; abort(); }
