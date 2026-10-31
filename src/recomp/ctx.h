#pragma once
// Guest CPU state and memory access for code produced by the recompiler (src/recompiler).
//
// Guest memory: MEM1 (24 MiB, physical 0x00000000) is reachable through the
// cached (0x8...) and uncached (0xC...) mirrors. The 16 KiB locked cache at
// 0xE0000000 lives just past MEM1. Hardware registers (0xCC......) go through
// mmio_*; the write-gather pipe (0xCC008000) feeds Aurora's GX FIFO.
//
// FREESTANDING: this header is shipped with the game and compiled into the game module
// on the user's machine by the bundled clang, with no C/C++ library headers available
// (docs/SETUP.md). Use compiler builtins only; no #include of system headers.
// Everything the generated code calls that is not inline here is provided by the runtime
// (statically in lmrecomp, through module_rt.cpp in the game module DLL).

namespace recomp {

using u8 = __UINT8_TYPE__;
using u16 = __UINT16_TYPE__;
using u32 = __UINT32_TYPE__;
using u64 = __UINT64_TYPE__;
using s8 = __INT8_TYPE__;
using s16 = __INT16_TYPE__;
using s32 = __INT32_TYPE__;
using s64 = __INT64_TYPE__;

constexpr u32 kMem1Size = 0x01800000;
constexpr u32 kLockedCacheOffset = 0x01800000;  // host offset of the 0xE0000000 locked cache
// Host window for guest addresses (masked): MEM1 (24 MiB), the locked cache, then memory that
// only host tools use (the model viewer's heap at 0x81810000+). Must stay a power of two.
constexpr u32 kHostMemSize = 0x04000000;
constexpr u32 kHostMemMask = kHostMemSize - 1;

inline u32 bswap32(u32 v) { return __builtin_bswap32(v); }
inline u16 bswap16(u16 v) { return __builtin_bswap16(v); }

// LMNX: the program is built -fPIE, so a global declared with default visibility is read through the GOT
// (its address loaded first, then the value). The console build is one static executable: these are always its own,
// and hidden visibility lets the compiler address them directly. The recompiled code tests g_watchOn on every guest
// store (16,351 places) and the interrupt flag in every poll (5,776, three dependent loads each: GOT, pointer, flag).
#ifdef __SWITCH__
#define LM_HIDDEN __attribute__((visibility("hidden")))
#else
#define LM_HIDDEN
#endif

extern u8* g_mem;
#ifdef __SWITCH__
extern bool g_irqPendingFlag LM_HIDDEN;                   // sched.cpp
inline constexpr bool* g_irqPending = &g_irqPendingFlag;  // static program: always the scheduler's flag
#else
extern bool* g_irqPending;  // set (atomically) by the runtime when an interrupt is queued
#endif
extern bool g_traceOn LM_HIDDEN;  // LM_TRACE: log/record guest function entries
extern u8* g_patches LM_HIDDEN;   // runtime-switchable instruction patches ("patch" lines in tools/recomp/hooks.txt)
extern bool g_watchOn LM_HIDDEN;  // LM_WATCH: report guest stores to an address range
void watch_store(u32 a, u32 v, int size);

namespace hw {
// Original values and independent widths, in PowerPC order.
struct FifoWrite { u32 value; int size; };
inline constexpr u32 kFifoBlockMax = 64;
void fifoWriteBlock(const FifoWrite* writes, u32 count);
}

#ifdef __SWITCH__
// LMNX: on the console guest memory is an array of the program (runtime.cpp), so its address is a link-time
// constant the compiler keeps in a register through a function. g_mem had to be reloaded after every guest store: a
// store through u8* may alias any object, g_mem itself included.
extern u8 g_memStorage[] __attribute__((visibility("hidden")));
inline u8* memBase() { return g_memStorage; }
#else
inline u8* memBase() { return g_mem; }
#endif

inline u8* hostp(u32 a) {
    return memBase() + ((a >> 28) == 0xE ? (kLockedCacheOffset | (a & 0x3FFF)) : (a & kHostMemMask));
}
inline bool is_mmio(u32 a) { return (a >> 24) == 0xCC; }

u32 mmio_rd(u32 a, int size);
void mmio_wr(u32 a, u32 v, int size);

// LMNX: the accessors below take one check for the usual addresses (cached MEM1 0x8..., physical 0x0...):
// below 0xC0000000 the address is plain RAM, neither MMIO (0xCC...) nor the locked cache (0xE...), and hostp is just
// base + (a & mask). The rest (uncached mirror 0xC..., MMIO, locked cache) goes to the out-of-line functions, which do
// exactly what the accessors did before. The same result, five instructions less per access.
inline bool plainRam(u32 a) { return (a >> 28) < 0xC; }
inline u8* ramp(u32 a) { return memBase() + (a & kHostMemMask); }
u32 rdSlow(u32 a, int size);          // runtime.cpp
void wrSlow(u32 a, u32 v, int size);  // runtime.cpp

// Bit casts and math, as builtins (the module has no C library).
inline u64 bits64(double v) { return __builtin_bit_cast(u64, v); }
inline double dbl(u64 v) { return __builtin_bit_cast(double, v); }
inline u32 bits32(float v) { return __builtin_bit_cast(u32, v); }
inline float flt(u32 v) { return __builtin_bit_cast(float, v); }
inline double fma_(double a, double b, double c) { return __builtin_fma(a, b, c); }
inline double sqrt_(double v) { return __builtin_sqrt(v); }
inline double fabs_(double v) { return __builtin_fabs(v); }
inline bool isnan_(double v) { return __builtin_isnan(v); }

inline u32 rd32(u32 a) {
    if (!plainRam(a)) [[unlikely]] return rdSlow(a, 4);
    u32 v;
    __builtin_memcpy(&v, ramp(a), 4);
    return bswap32(v);
}
inline u32 rd16(u32 a) {
    if (!plainRam(a)) [[unlikely]] return rdSlow(a, 2);
    u16 v;
    __builtin_memcpy(&v, ramp(a), 2);
    return bswap16(v);
}
inline u32 rd8(u32 a) {
    if (!plainRam(a)) [[unlikely]] return rdSlow(a, 1);
    return *ramp(a);
}
inline u32 rd32r(u32 a) {
    u32 v;
    __builtin_memcpy(&v, hostp(a), 4);
    return v;
}
inline u32 rd16r(u32 a) {
    u16 v;
    __builtin_memcpy(&v, hostp(a), 2);
    return v;
}
inline void wr32(u32 a, u32 v) {
    if (g_watchOn) [[unlikely]] watch_store(a, v, 4);
    if (!plainRam(a)) [[unlikely]] return wrSlow(a, v, 4);
    v = bswap32(v);
    __builtin_memcpy(ramp(a), &v, 4);
}
inline void wr16(u32 a, u32 v) {
    if (g_watchOn) [[unlikely]] watch_store(a, v, 2);
    if (!plainRam(a)) [[unlikely]] return wrSlow(a, v, 2);
    const u16 s = bswap16(u16(v));
    __builtin_memcpy(ramp(a), &s, 2);
}
inline void wr8(u32 a, u32 v) {
    if (g_watchOn) [[unlikely]] watch_store(a, v, 1);
    if (!plainRam(a)) [[unlikely]] return wrSlow(a, v, 1);
    *ramp(a) = u8(v);
}
inline void wr32r(u32 a, u32 v) { __builtin_memcpy(hostp(a), &v, 4); }
inline void wr16r(u32 a, u32 v) {
    const u16 s = u16(v);
    __builtin_memcpy(hostp(a), &s, 2);
}
inline float rdf32(u32 a) { return flt(rd32(a)); }
inline double rdf64(u32 a) { return dbl(u64(rd32(a)) << 32 | rd32(a + 4)); }
inline void wrf32(u32 a, float v) { wr32(a, bits32(v)); }
inline void wrf64(u32 a, double v) {
    const u64 b = bits64(v);
    wr32(a, u32(b >> 32));
    wr32(a + 4, u32(b));
}

// The recompiler emits r1-based accesses for the game stack with --switch-all-locals. The stack is never MMIO or
// watched by LM_WATCH, so these accesses skip both checks on a large part of the game's memory traffic.
inline u32 rd32p(u32 a) { u32 v; __builtin_memcpy(&v, hostp(a), 4); return bswap32(v); }
inline u32 rd16p(u32 a) { u16 v; __builtin_memcpy(&v, hostp(a), 2); return bswap16(v); }
inline u32 rd8p(u32 a) { return *hostp(a); }
inline void wr32p(u32 a, u32 v) { v = bswap32(v); __builtin_memcpy(hostp(a), &v, 4); }
inline void wr16p(u32 a, u32 v) { const u16 s = bswap16(u16(v)); __builtin_memcpy(hostp(a), &s, 2); }
inline void wr8p(u32 a, u32 v) { *hostp(a) = u8(v); }
inline float rdf32p(u32 a) { return flt(rd32p(a)); }
inline double rdf64p(u32 a) { return dbl(u64(rd32p(a)) << 32 | rd32p(a + 4)); }
inline void wrf32p(u32 a, float v) { wr32p(a, bits32(v)); }
inline void wrf64p(u32 a, double v) {
    const u64 b = bits64(v);
    wr32p(a, u32(b >> 32));
    wr32p(a + 4, u32(b));
}

struct GuestThread;
struct Ctx;
using GuestFn = void (*)(Ctx&);
struct FuncEntry {
    u32 addr;
    GuestFn fn;
};
struct SymEntry {
    const char* name;
    u32 addr;
};

// The recompiled game: tables emitted by the recompiler (func_table.cpp). Linked into
// lmrecomp directly, or exported by the game module DLL (module_abi.h).
constexpr u32 kModuleAbiVersion = 3;
constexpr u32 kMaxPatches = 64;
struct GameModule {
    u32 abiVersion;
    const char* gameId;  // "GLME01" / "GLMP01": the DOL it was recompiled from
    const FuncEntry* funcs;
    u32 funcCount;
    const SymEntry* syms;  // canonical names (hooks and HLE use US names on every version)
    u32 symCount;
    const FuncEntry* orig;  // translated bodies of hooked functions (tools/recomp/hooks.txt)
    u32 origCount;
};

// HLE (hle.h): host replacements registered by name.
using HleFn = void (*)(Ctx&);
HleFn hleLookup(const char* name);
void hleMissing(const char* name, u32 addr, Ctx& c);
// LMNX: each generated HLE/hook wrapper keeps its handler in a constant-initialized function-local pointer
// (no guard: the C++ guard of `static const HleFn s = hleLookup(...)` was an acquire load on every call, ~6 % of the
// game thread over all wrappers in the foyer). Null until the first call resolves it here; hleNone() (never called)
// marks "no handler", so the lookup by name runs once. The registry is complete before any guest code runs.
inline HleFn hleNone() { return reinterpret_cast<HleFn>(static_cast<__UINTPTR_TYPE__>(1)); }
// LMNX: a flag computed once without a C++ guard, for checks on hot paths (per GX call, draw or FIFO word):
// `static signed char s = -1; if (lmnxOnce(s, [] { ... }))`. -1 = not computed yet; a race only computes it twice.
template <typename F>
inline bool lmnxOnce(signed char& slot, F&& compute) {
    signed char v = __atomic_load_n(&slot, __ATOMIC_RELAXED);
    if (v < 0) [[unlikely]] {
        v = compute() ? 1 : 0;
        __atomic_store_n(&slot, v, __ATOMIC_RELAXED);
    }
    return v != 0;
}
[[gnu::noinline, gnu::cold]] inline HleFn hleResolve(HleFn* slot, const char* name) {
    HleFn f = hleLookup(name);
    if (!f) f = hleNone();
    __atomic_store_n(slot, f, __ATOMIC_RELAXED);
    return f;
}

void service_irq(Ctx& c);
void trace_enter(u32 addr, Ctx& c);
[[noreturn]] void fatal(const char* fmt, ...);

struct Ctx {
    u32 r[32]{};
    double f[32]{};
    double ps1[32]{};
    u8 cr[8]{};
    u32 lr = 0, ctr = 0;
    u32 ca = 0, so = 0, ov = 0;
    u32 fpscr = 0;
    u32 gqr[8]{};
    u32 msr = 0x8000;  // external interrupts enabled
    GuestThread* thread = nullptr;
    u32 lastFn = 0;  // last guest function entered (only maintained while tracing)
    u32 ring[32]{};  // recent function entries (tracing only)
    u32 ringPos = 0;

    // --- condition register ---
    bool crbit(int n) const { return (cr[n >> 2] >> (3 - (n & 3))) & 1; }
    void setCrbit(int n, bool v) {
        const u8 m = u8(1 << (3 - (n & 3)));
        cr[n >> 2] = v ? (cr[n >> 2] | m) : (cr[n >> 2] & ~m);
    }
    void cmps(int f, s32 a, s32 b) { cr[f] = u8((a < b ? 8 : a > b ? 4 : 2) | so); }
    void cmpu(int f, u32 a, u32 b) { cr[f] = u8((a < b ? 8 : a > b ? 4 : 2) | so); }
    void setCr0(s32 v) { cmps(0, v, 0); }
    void setCr1() { cr[1] = u8(fpscr >> 28); }
    void fcmp(int f, double a, double b, bool) {
        cr[f] = isnan_(a) || isnan_(b) ? 1 : a < b ? 8 : a > b ? 4 : 2;
        fpscr = (fpscr & ~0xF000u) | (u32(cr[f]) << 12);
    }
    u32 getCr() const {
        u32 v = 0;
        for (int i = 0; i < 8; i++) v |= u32(cr[i] & 0xF) << (28 - 4 * i);
        return v;
    }
    void mtcrf(u32 crm, u32 v) {
        for (int i = 0; i < 8; i++)
            if (crm & (0x80 >> i)) cr[i] = (v >> (28 - 4 * i)) & 0xF;
    }
    u32 getXer() const { return (so << 31) | (ov << 30) | (ca << 29); }
    void setXer(u32 v) {
        so = (v >> 31) & 1;
        ov = (v >> 30) & 1;
        ca = (v >> 29) & 1;
    }

    // Integer helpers: GCC/devkitA64 lacks Clang's builtin. Both shifts stay within 0..31, including when n is a
    // multiple of 32.
    static u32 rotl(u32 v, u32 n) {
#if defined(__clang__)
        return __builtin_rotateleft32(v, n & 31);
#else
        const u32 s = n & 31;
        return (v << s) | (v >> ((32 - s) & 31));
#endif
    }
    static u32 cntlzw(u32 v) { return v ? u32(__builtin_clz(v)) : 32u; }
    static u32 divw(u32 a, u32 b) {
        const s32 x = s32(a), y = s32(b);
        if (y == 0 || (x == s32(0x80000000u) && y == -1)) return x < 0 && y == 0 ? 0xFFFFFFFFu : 0;
        return u32(x / y);
    }
    static u32 divwu(u32 a, u32 b) { return b == 0 ? 0 : a / b; }
    void sraw(int dst, u32 s, u32 sh) {
        const s32 v = s32(s);
        if (sh >= 32) {
            r[dst] = v < 0 ? 0xFFFFFFFFu : 0;
            ca = v < 0;
            return;
        }
        r[dst] = u32(v >> sh);
        ca = v < 0 && sh != 0 && (u32(v) & ((1u << sh) - 1)) != 0;
    }

    // --- floating point ---
    void setSingle(int d, float v) {
        f[d] = v;
        ps1[d] = v;
    }
    static double fctiw(double v, int truncate) {
        s32 i;
        if (isnan_(v)) i = s32(0x80000000u);
        else if (v >= 2147483647.0) i = 0x7FFFFFFF;
        else if (v <= -2147483648.0) i = s32(0x80000000u);
        else i = s32(truncate ? __builtin_trunc(v) : __builtin_nearbyint(v));
        return dbl(0xFFF8000000000000ull | u32(i));
    }

    // --- paired-single quantized load/store (GQR type/scale) ---
    static double qscale(int scale) { return dbl(u64(1023 - scale) << 52); }  // 2^-scale
    static int sext6(u32 v) { return (v & 0x20) ? int(v) - 64 : int(v); }
    static double dequant(u32 ea, int type, double s, u32* size) {
        switch (type) {
        case 4: *size = 1; return double(u8(rd8(ea))) * s;
        case 5: *size = 2; return double(u16(rd16(ea))) * s;
        case 6: *size = 1; return double(s8(rd8(ea))) * s;
        case 7: *size = 2; return double(s16(rd16(ea))) * s;
        default: *size = 4; return double(rdf32(ea));
        }
    }
    static u32 saturate(double v, double lo, double hi) {
        if (isnan_(v)) return 0;
        v = v < lo ? lo : v > hi ? hi : v;
        return u32(s32(v));
    }
    static u32 quant(u32 ea, double v, int type, double s) {
        const double q = v * s;
        switch (type) {
        case 4: wr8(ea, saturate(q, 0.0, 255.0)); return 1;
        case 5: wr16(ea, saturate(q, 0.0, 65535.0)); return 2;
        case 6: wr8(ea, saturate(q, -128.0, 127.0)); return 1;
        case 7: wr16(ea, saturate(q, -32768.0, 32767.0)); return 2;
        default: wrf32(ea, float(v)); return 4;
        }
    }
    // LMNX: psq_l / psq_st with a float GQR type (0, and the reserved 1-3 that dequant/quant treat as
    // float) take a direct path: the same values as dequant/quant's float case (rdf32 / wrf32(float(v)), no scale),
    // without the type switch and the scale; both words in one load when they are plain RAM. In the foyer these two
    // helpers were ~13 % of the game thread (matrix and vector code). that direct path is inlined into the
    // callers (they were emitted out of line: ~1.7 ms of the game thread in the foyer for ~290 call sites); the
    // quantized types go to psqLoadQuant / psqStoreQuant, unchanged.
    [[gnu::always_inline]] void psqLoad(int d, u32 ea, int w, int i) {
        const u32 g = gqr[i];
        const int type = (g >> 16) & 7;
        if (type < 4) [[likely]] {
            if (w) {
                f[d] = double(rdf32(ea));
                ps1[d] = 1.0;
            } else if (plainRam(ea) && (ea & kHostMemMask) <= kHostMemMask - 7) {
                u64 raw;
                __builtin_memcpy(&raw, ramp(ea), 8);
                f[d] = double(flt(bswap32(u32(raw))));
                ps1[d] = double(flt(bswap32(u32(raw >> 32))));
            } else {
                f[d] = double(rdf32(ea));
                ps1[d] = double(rdf32(ea + 4));
            }
            return;
        }
        psqLoadQuant(d, ea, w, type, g);
    }
    [[gnu::noinline]] void psqLoadQuant(int d, u32 ea, int w, int type, u32 g) {
        const double s = qscale(sext6((g >> 24) & 0x3F));
        u32 size;
        f[d] = dequant(ea, type, s, &size);
        ps1[d] = w ? 1.0 : dequant(ea + size, type, s, &size);
    }
    [[gnu::always_inline]] void psqStore(int d, u32 ea, int w, int i) {
        const u32 g = gqr[i];
        const int type = g & 7;
        if (type < 4) [[likely]] {
            wrf32(ea, float(f[d]));
            if (!w) wrf32(ea + 4, float(ps1[d]));
            return;
        }
        psqStoreQuant(d, ea, w, type, g);
    }
    [[gnu::noinline]] void psqStoreQuant(int d, u32 ea, int w, int type, u32 g) {
        const double s = qscale(-sext6((g >> 8) & 0x3F));
        const u32 size = quant(ea, f[d], type, s);
        if (!w) quant(ea + size, ps1[d], type, s);
    }

    // --- system ---
    u32 mfspr(u32 spr);
    void mtspr(u32 spr, u32 v);
    u32 mftb(u32 spr);
    static void dcbz(u32 ea) { __builtin_memset(hostp(ea & ~31u), 0, 32); }

    void enter(u32 addr) {
        if (g_traceOn) [[unlikely]] trace_enter(addr, *this);
    }
    void poll() {
        if (__atomic_load_n(g_irqPending, __ATOMIC_RELAXED) && (msr & 0x8000)) [[unlikely]]
            service_irq(*this);
    }
    void call(u32 addr);
    [[noreturn]] void unimplemented(u32 pc, u32 insn);
};

// LMNX: the condition register as a local of a translated function (the recompiler's --switch-cr-locals),
// a u64 whose byte i is cr[i] (the same bytes as Ctx::cr on a little-endian host, so it moves to and from the Ctx with
// one 8-byte copy). The same results as the Ctx members they replace: cmps/cmpu/setCr0 set a field to LT/GT/EQ | SO,
// crbit/setCrbit address bit 3 - (n & 3) of field n >> 2, and fcmp is Ctx::fcmp (which also sets FPSCR's FPCC).
static_assert(sizeof(Ctx::cr) == 8);
inline void crPut(u64& cr, int f, u32 v) { cr = (cr & ~(u64(0xFF) << (8 * f))) | (u64(u8(v)) << (8 * f)); }
inline bool crBit(u64 cr, int n) { return (cr >> (8 * (n >> 2) + 3 - (n & 3))) & 1; }
inline void crSetBit(u64& cr, int n, bool v) {
    const u64 m = u64(1) << (8 * (n >> 2) + 3 - (n & 3));
    cr = v ? (cr | m) : (cr & ~m);
}
inline void crCmps(u64& cr, u32 so, int f, s32 a, s32 b) { crPut(cr, f, (a < b ? 8 : a > b ? 4 : 2) | so); }
inline void crCmpu(u64& cr, u32 so, int f, u32 a, u32 b) { crPut(cr, f, (a < b ? 8 : a > b ? 4 : 2) | so); }
inline void crSetCr0(u64& cr, u32 so, s32 v) { crCmps(cr, so, 0, v, 0); }
inline void crFcmp(Ctx& c, u64& cr, int f, double a, double b, bool o) {
    c.fcmp(f, a, b, o);
    crPut(cr, f, c.cr[f]);
}

}  // namespace recomp
