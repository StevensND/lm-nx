"""Synthetic contract tests without running the game."""
import os
from pathlib import Path
import subprocess

RAIZ = Path(__file__).resolve().parents[1]
SALIDA = RAIZ / "build/check-hle-contracts"
CC = Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"))


def main():
    SALIDA.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = str(CC.parent) + os.pathsep + env["PATH"]
    env["TMP"] = env["TEMP"] = str(SALIDA)

    def ejecutar(*args):
        return subprocess.check_output([str(x) for x in args], cwd=RAIZ, env=env)

    fuente = (RAIZ / "src/recompiler/recompiler.cpp").read_text(encoding="utf-8")
    # Only under build/: a fake entry tests the partial branch. Enable no actual GX contracts in the shipped
    # recompiler.
    original = '{"GXLoadPosMtxImm", regs(3,4), 0, 0, 0, false}'
    assert fuente.count(original) == 1
    (SALIDA / "sintetico.cpp").write_text(fuente.replace(original,
        '{"GXLoadPosMtxImm", regs(3,4), (1u<<3)|(1u<<12), 0, 0, true}'), encoding="utf-8")
    generador = r'''
#include "sintetico.cpp"
#include <iostream>
int main() {
    using namespace lmrc;
    std::vector<u8> bytes(256); Dol dol(bytes);
    std::map<std::string,std::pair<u32,u32>> objs;
    Recompiler rc(dol, {}, objs, {}, nullptr);
    rc.funcs[0x80001000]={"GXLoadPosMtxImm",4};
    rc.hle.insert(0x80001000);
    for (int mode=0;mode<4;mode++) {
        rc.excludedContracts.clear(); rc.hooks.clear();
        if(mode==1)rc.excludedContracts.insert("*");
        if(mode==2)rc.excludedContracts.insert("GXLoadPosMtxImm");
        if(mode==3)rc.hooks.insert("GXLoadPosMtxImm");
        std::vector<std::string> body;
        for(int r=0;r<32;r++)body.push_back(fmt("    c.r[%u] += %uu;",r,r+1));
        body.push_back("    f_80001000(c);");
        body.push_back("    f_80001000_orig(c);");
        body.push_back("    f_80002000(c);");
        body.push_back("    c.call(0x1234u);");
        body.push_back("    c.poll();");
        body.push_back("    c.sraw(7, c.r[8], 2);");
        body.push_back("    return;");
        auto previous=rc.partialCalls;
        rc.localizar(body);
        if(rc.partialCalls-previous != size_t(mode==0))return 2;
        std::cout<<"void caso"<<mode<<"(Ctx& c) {\n";
        for(auto& line:body)std::cout<<line<<"\n";
        std::cout<<"}\n";
    }
}
'''
    (SALIDA / "generador.cpp").write_text(generador, encoding="utf-8")
    ejecutar(CC, "-std=c++20", "-O2", "-Isrc", SALIDA / "generador.cpp", "-o", SALIDA / "generador.exe")
    cuerpos = ejecutar(SALIDA / "generador.exe").decode()
    test_source = r'''
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <cstdio>
using u32=uint32_t;
bool g_traceOn=false; u32 irq=1; u32* g_irqPending=&irq;
std::vector<std::array<u32,32>> events;
struct Ctx {
    u32 r[32]{},msr=0x8000,lr=73;
    void full(){ std::array<u32,32> e; std::memcpy(e.data(),r,sizeof r); events.push_back(e);
        for(int i=0;i<32;i++)r[i]^=0x1020304u+i;lr++; }
    void call(u32){full();} void poll(){full();}
    void sraw(unsigned d,u32 v,unsigned n){r[d]=u32(int32_t(v)>>n);}
};
void f_80001000(Ctx& c){
    if(g_traceOn){std::array<u32,32> e;std::memcpy(e.data(),c.r,sizeof c.r);events.push_back(e);}
    std::array<u32,32> args{};args[3]=c.r[3];args[4]=c.r[4];events.push_back(args);
    c.r[3]+=c.r[4];c.r[12]=c.r[4]^0xabcd1234u;c.lr++;
}
void f_80001000_orig(Ctx& c){c.full();}
void f_80002000(Ctx& c){c.full();}
'''
    test_source += cuerpos + r'''
int main(){
    uint32_t seed=12345;
    for(int trial=0;trial<20000;trial++){
        Ctx a;for(auto& v:a.r){seed=seed*1664525u+1013904223u;v=seed;}
        a.msr=trial&1?0x8000:0;irq=trial&2;g_traceOn=trial&4;
        Ctx b=a,c=a,d=a;
        events.clear();caso0(a);auto expected=events;
        events.clear();caso1(b);if(events!=expected||std::memcmp(&a,&b,sizeof a))return 3;
        events.clear();caso2(c);if(events!=expected||std::memcmp(&a,&c,sizeof a))return 4;
        events.clear();caso3(d);if(events!=expected||std::memcmp(&a,&d,sizeof a))return 5;
    }
    std::puts("20000 comparisons: final Ctx, observers, tracing, IRQs, indirect calls, hooks and exclusions checked");
}
'''
    (SALIDA / "test.cpp").write_text(test_source, encoding="utf-8")
    ejecutar(CC, "-std=c++20", "-O2", SALIDA / "test.cpp", "-o", SALIDA / "test.exe")
    print(ejecutar(SALIDA / "test.exe").decode().strip())


if __name__ == "__main__":
    main()
