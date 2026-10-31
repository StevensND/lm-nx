"""Synthetic differential comparison of the emitter and FIFO HLE. Uses no game data and does not run the game. Keep synthetic executables and DOLs under build/check-fifo-blocks."""
import os
from pathlib import Path
import struct
import subprocess

RAIZ = Path(__file__).resolve().parents[1]
SALIDA = RAIZ / "build/check-fifo-blocks"
CC = Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"))


def ejecutar(args, env):
    subprocess.run([str(x) for x in args], cwd=RAIZ, env=env, check=True)


def d(op, r, a, imm):
    return op << 26 | r << 21 | a << 16 | (imm & 65535)


def main():
    SALIDA.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = str(CC.parent) + os.pathsep + env["PATH"]
    env["TMP"] = env["TEMP"] = str(SALIDA)
    fuente = (RAIZ / "src/recompiler/recompiler.cpp").read_text(encoding="utf-8")
    # Reference: same emitter with burst transformation disabled.
    assert fuente.count("if (count >= 2)") == 1
    base = SALIDA / "recompiler_base.cpp"
    base.write_text(fuente.replace("if (count >= 2)", "if (false && count >= 2)"), encoding="utf-8")
    herramienta = SALIDA / "base.exe"
    ejecutar([CC, "-std=c++20", "-O2", "-Isrc", base, "tools/recomp/lmrecompile.cpp", "-o", herramienta], env)
    fifo = [d(36, 3, 5, -32768), d(38, 4, 5, -32768), d(44, 6, 5, -32768), d(52, 1, 5, -32768)]
    lis = d(15, 5, 0, 0xCC01)
    blr = 0x4E800020
    casos = [
        [lis, *fifo, blr],
        [lis, *fifo, d(34, 7, 8, 0), *fifo, blr],
        [lis, *fifo, d(15, 5, 0, 0xCC02), *fifo, blr],
        # The conditional entry lands inside a write sequence.
        [lis, 0x41820008, fifo[0], *fifo, blr],
        # Merge with different bases: no common constant.
        [lis, 0x41820008, d(15, 5, 0, 0xCC02), *fifo, blr],
        # Polling loop changes the base in service_irq.
        [lis, d(14, 0, 0, 3), 0x7C0903A6, *fifo, 0x4200FFF0, blr],
        [lis, *(fifo * 20), blr],
        # mfspr replaces only its destination register.
        [lis, 0x7C0002A6, *fifo, blr],
        # A load replaces the base even if previously constant.
        [lis, d(32, 5, 8, 0), *fifo, blr],
        # Do not group a patched function.
        [lis, *fifo, blr],
        # An indirect call invalidates constants and ends the burst.
        [lis, *fifo, 0x4E800421, *fifo, blr],
    ]
    payload = bytearray()
    direcciones = []
    symbols = []
    for i, words in enumerate(casos):
        addr = 0x80001000 + len(payload)
        direcciones.append(addr)
        symbols.append(f"caso{i} = .text:0x{addr:08X}; // type:function size:0x{len(words)*4:X}")
        payload += b"".join(struct.pack(">I", w) for w in words)
    header = bytearray(256)
    for off, val in [(0, 256), (0x48, 0x80001000), (0x90, len(payload))]:
        struct.pack_into(">I", header, off, val)
    (SALIDA / "sintetico.dol").write_bytes(header + payload)
    (SALIDA / "symbols.txt").write_text("\n".join(symbols), encoding="utf-8")
    (SALIDA / "hooks.txt").write_text(f"patch 0 {direcciones[9]:08X} {lis:08X}\n", encoding="utf-8")
    for nombre, tool in [("base", herramienta), ("opt", RAIZ / "build/host-recompiler-todos/lmrecompile.exe")]:
        destino = SALIDA / nombre
        ejecutar([tool, "--dol", SALIDA / "sintetico.dol", "--symbols", SALIDA / "symbols.txt", "--hooks", SALIDA / "hooks.txt",
                  "--switch-integer-locals", "1", "--switch-all-locals", "1", "--out", destino], env)
        texto = "\n".join(p.read_text(encoding="utf-8") for p in sorted(destino.glob("funcs_*.cpp")))
        texto = texto.replace("namespace recomp {", f"namespace recomp::{nombre} {{")
        (SALIDA / f"{nombre}.cpp").write_text(texto, encoding="utf-8")
    test_source = r'''
#include "recomp/ctx.h"
#include <vector>
#include <tuple>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace recomp {
u8* g_mem; bool g_traceOn=false,g_watchOn=false; bool irq=true; bool* g_irqPending=&irq;
u8 patches[16]{}; u8* g_patches=patches;
using Event=std::tuple<u32,u32,int>; std::vector<Event> events;
void trace_enter(u32,Ctx&) {}
void service_irq(Ctx& c) { c.r[5]=0xCC020000u; }
void watch_store(u32 a,u32 v,int s) { events.emplace_back(a,v,-s); }
u32 mmio_rd(u32 a,int s) { events.emplace_back(a,0,-100-s); return 0xCC010000u; }
void mmio_wr(u32 a,u32 v,int s) { events.emplace_back(a,v,s); }
u32 rdSlow(u32 a,int s) { return mmio_rd(a,s); }
void wrSlow(u32 a,u32 v,int s) { mmio_wr(a,s==1?u8(v):s==2?u16(v):v,s); }
u32 Ctx::mfspr(u32) { return 0; }
void Ctx::call(u32) { r[5]=0xCC020000u; events.emplace_back(0,0,-200); }
void Ctx::unimplemented(u32,u32) { std::abort(); }
HleFn hleLookup(const char*) { return nullptr; }
void hleMissing(const char*,u32,Ctx&) { std::abort(); }
u32 blocks=0;
void hw::fifoWriteBlock(const FifoWrite* w,u32 n) {
    ++blocks;
    for(u32 i=0;i<n;++i) {
        if(g_watchOn) watch_store(0xCC008000u,w[i].value,w[i].size);
        wrSlow(0xCC008000u,w[i].value,w[i].size);
    }
}
@DECL@
}
int main() {
    using namespace recomp;
    struct Pair {void(*a)(Ctx&);void(*b)(Ctx&);}; Pair pairs[]={@PAIRS@};
    u32 seed=19; auto rnd=[&]{seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
    for(int t=0;t<5000;++t) for(auto p:pairs) {
        Ctx a{},b{};
        for(auto& r:a.r) r=rnd();
        for(auto& cr:a.cr) cr=rnd()&15;
        a.r[8]=0xCC004000u; a.msr=0x8000; a.f[1]=double(int(rnd()))/13;
        b=a; g_watchOn=(t&1); patches[0]=(t&2)!=0; events.clear();
        p.a(a); auto reference=events; events.clear(); p.b(b);
        if(events!=reference || std::memcmp(&a,&b,sizeof a)) return 1;
    }
    if(!blocks) return 2;
    std::printf("Emitter: 55000 identical Ctx and MMIO-event comparisons; %u blocks executed.\n",blocks);
}
'''
    decl = []
    for ns in ["base", "opt"]:
        decl.append(f"namespace {ns} {{" + "".join(f"void f_{a:08X}(Ctx&);" for a in direcciones) + "}")
    test_source = test_source.replace("@DECL@", "\n".join(decl)).replace("@PAIRS@", ",".join(f"{{base::f_{a:08X},opt::f_{a:08X}}}" for a in direcciones))
    (SALIDA / "emisor.cpp").write_text(test_source, encoding="utf-8")
    exe = SALIDA / "emisor.exe"
    ejecutar([CC, "-std=c++20", "-O2", "-Isrc", "-I"+str(SALIDA/"opt"), SALIDA/"base.cpp", SALIDA/"opt.cpp", SALIDA/"emisor.cpp", "-o", exe], env)
    ejecutar([exe], env)
    probar_hle(env)
    probar_cursores(env)


def probar_hle(env):
    fuente = (RAIZ / "src/recomp/hle_gx.cpp").read_text(encoding="utf-8")
    def extraer(firma):
        inicio = fuente.index(firma)
        fin = fuente.index("\n}", inicio) + 2
        return fuente[inicio:fin]
    funciones = "\n".join(extraer(f) for f in ["void rawFifoWrite(", "void hw::fifoWrite(", "void hw::fifoWriteBlock("])
    test_source = r'''
#include "recomp/ctx.h"
#include <vector>
#include <tuple>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace recomp;
std::vector<u8> output,snaps,vertices; std::vector<u32> cursors;
std::vector<std::tuple<u32,u32,int>> watches;
u32 waits=0,joins=0,copies=0,filterCount=0; bool capturing=false,recording=false,drop=false;
u32 capacity=100000; bool sRawCheck=true,sInBegin=false;
struct {bool active=false;std::vector<u8> bytes;std::vector<u32> writes;} sHeld;
void append(std::vector<u8>& out,u32 v,int size) {for(int b=size-1;b>=0;--b)out.push_back(u8(v>>(8*b)));}
namespace recomp {bool g_watchOn=false;
void watch_store(u32 a,u32 v,int s){watches.emplace_back(a,v,s);}
namespace hw {u32(*fifoFilter)(u32,int)=nullptr;void fifoWrite(u32,int);}
namespace frame {void gxWaitForGuestTurn(){++waits;}}
namespace interp {
void joinCapture(){++joins;} bool capturing(){return ::capturing;}
u32 mark(){return u32(output.size());}
void snapBytes(u32 v,int s){append(snaps,v,s);} void vtxBytes(u32 v,int s){append(vertices,v,s);cursors.push_back(mark());}
void vtxBytes(u32 v,int s,u32 at){append(vertices,v,s);cursors.push_back(at);}
}}
namespace aurora::gx::fifo {
namespace detail {struct Recording{bool active=false;u32 size=0,pos=0;};Recording rec;Recording& recording(){rec.active=::recording;rec.size=capacity;rec.pos=u32(output.size());return rec;}}
bool in_display_list(){return recording;}
void write_data(const void* p,u32 n){++copies;if(output.size()+n<=capacity){auto* b=(const u8*)p;output.insert(output.end(),b,b+n);}else capacity=u32(output.size());}
}
void cmd(u32 v,int s){std::vector<u8>b;append(b,v,s);aurora::gx::fifo::write_data(b.data(),s);}
void GXCmd1u8(u8 v){cmd(v,1);}void GXCmd1u16(u16 v){cmd(v,2);}void GXCmd1u32(u32 v){cmd(v,4);}
bool dropDraw(){return drop;}enum class ProjMode{Scaled};ProjMode drawMode(ProjMode m){return m;}
void prepareDraw(ProjMode,const char*){GXCmd1u32(0xABCDEF01);}
u32 filter(u32 v,int){return v^(++filterCount);}
@FUNCTIONS@
struct Result {
std::vector<u8> out,snap,vtx,held;std::vector<u32> writes;std::vector<std::tuple<u32,u32,int>> watch;
std::vector<u32> positions;bool raw;u32 filters;
bool operator==(const Result&)const=default;
};
Result run(const std::vector<hw::FifoWrite>& ws,int mode,bool block){
output.clear();snaps.clear();vertices.clear();watches.clear();cursors.clear();sHeld.bytes.clear();sHeld.writes.clear();
waits=joins=copies=filterCount=0; sRawCheck=true;sInBegin=mode&1;sHeld.active=mode&2;
capturing=mode&4;recording=mode&8;drop=mode&16;g_watchOn=mode&32;
hw::fifoFilter=mode&64?filter:nullptr;capacity=recording?17:100000;
if(block)hw::fifoWriteBlock(ws.data(),u32(ws.size()));else for(auto w:ws){
if(g_watchOn)watch_store(0xCC008000,w.value,w.size);
u32 v=w.size==1?u8(w.value):w.size==2?u16(w.value):w.value;
if(hw::fifoFilter)v=hw::fifoFilter(v,w.size);hw::fifoWrite(v,w.size);
}
return {output,snaps,vertices,sHeld.bytes,sHeld.writes,watches,cursors,sRawCheck,filterCount};
}
int main(){
u32 seed=7;auto rnd=[&]{seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
bool disabled=std::getenv("LM_FIFO_NO_BLOCKS")!=nullptr;
for(int t=0;t<1000;++t){std::vector<hw::FifoWrite> ws;
for(u32 n=1+rnd()%64;n;--n){int s[]={1,2,4};ws.push_back({rnd(),s[rnd()%3]});}
for(int mode=0;mode<128;++mode){auto ref=run(ws,mode,false);auto opt=run(ws,mode,true);
if(!(ref==opt))return 1;
if(!disabled && (mode==1 || mode==5) && (waits!=1||joins!=1||copies!=1))return 2;
}}
std::puts("HLE: 128000 identical comparisons (bytes, sizes, capture, held, filtering, watches, discard and overflowing lists).");
}
'''
    (SALIDA/"hle.cpp").write_text(test_source.replace("@FUNCTIONS@",funciones), encoding="utf-8")
    exe=SALIDA/"hle.exe"
    ejecutar([CC,"-std=c++20","-O2","-Isrc",SALIDA/"hle.cpp","-o",exe],env)
    env=env.copy();env.pop("LM_FIFO_NO_BLOCKS",None)
    ejecutar([exe],env)
    env["LM_FIFO_NO_BLOCKS"]="1"
    ejecutar([exe],env)


def probar_cursores(env):
    fuente=(RAIZ/"src/recomp/interp_vtx.cpp").read_text(encoding="utf-8")
    inicio=fuente.index("void vtxBytes(u32 v, int size) {")
    fin=fuente.index("\nvoid vtxDataAt",inicio)
    funciones=fuente[inicio:fin]
    test_source=r'''
#include "recomp/ctx.h"
#include <vector>
#include <cstdio>
using namespace recomp;
namespace test {
struct Direct {bool active=true;u32 at=~0u,bytes=0;int proj=-1;bool operator==(const Direct&)const=default;} sDirect;
struct Imm {bool active=true;int fmt=0;u32 count=2,size=16;std::vector<u8> bytes;bool operator==(const Imm&)const=default;} sImm;
std::vector<u8>sPending;u32 sPendingPos=0,pos=0,scans=0,flushes=0;
u32 mark(){return pos;}
namespace detail {int currentProjection(){return 17;}}
void scanVertices(int,const u8*,u32,u32){++scans;}
void flushPending(){++flushes;sPendingPos=u32(sPending.size());}
void vtxBytes(u32,int,u32);
@FUNCTIONS@
}
int main(){using namespace test;u32 seed=77;auto rnd=[&]{seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
for(int t=0;t<20000;++t){
Direct direct;direct.active=t&1;if(t&4)direct.at=123;
Imm imm;imm.active=t&2;std::vector<u8>pending(4000,u8(t));
std::vector<hw::FifoWrite>ws;u32 total=0;
for(int n=1+rnd()%64;n;--n){int sizes[]={1,2,4};int size=sizes[rnd()%3];ws.push_back({rnd(),size});total+=size;}
sDirect=direct;sImm=imm;sPending=pending;sPendingPos=0;pos=400;scans=flushes=0;
for(auto w:ws){pos+=w.size;vtxBytes(w.value,w.size);}
auto rd=sDirect;auto ri=sImm;auto rp=sPending;auto rs=scans,rf=flushes,rpos=sPendingPos;
sDirect=direct;sImm=imm;sPending=pending;sPendingPos=0;pos=400+total;scans=flushes=0;
u32 at=400;for(auto w:ws){at+=w.size;vtxBytes(w.value,w.size,at);}
if(!(rd==sDirect)||!(ri==sImm)||rp!=sPending||rs!=scans||rf!=flushes||rpos!=sPendingPos)return 1;
}
std::puts("Actual vtxBytes capture: 20000 identical states with per-element logical positions.");
}
'''
    (SALIDA/"cursores.cpp").write_text(test_source.replace("@FUNCTIONS@",funciones),encoding="utf-8")
    exe=SALIDA/"cursores.exe"
    ejecutar([CC,"-std=c++20","-O2","-Isrc",SALIDA/"cursores.cpp","-o",exe],env)
    ejecutar([exe],env)


if __name__ == "__main__":
    main()
