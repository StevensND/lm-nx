"""Check the actual span allocator without a GPU or game."""
from pathlib import Path
import os
import os, subprocess
raiz=Path(__file__).resolve().parents[1]
s=(raiz/"LMNX-Deko3D/third_party/plume/plume_deko3d.cpp").read_text(encoding='utf-8')
a=s.index("    static void chunkFree(");b=s.index("    void Deko3DDevice::retire(",a)
funciones=s[a:b].replace("ImageChunkSize","(64u<<20)")
pre=r"""
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <random>
#include <vector>
using DkMemBlock=uintptr_t;
constexpr uint32_t DK_MEMBLOCK_ALIGNMENT=4096, DK_UNIFORM_BUF_ALIGNMENT=256;
constexpr uint32_t DkMemBlockFlags_GpuCached=1,DkMemBlockFlags_Image=2;
uint32_t alignUp(uint32_t n,uint32_t a){return (n+a-1)/a*a;}
void logf(const char*,...){}
struct Deko3DImageAlloc{DkMemBlock block=0;uint32_t offset=0,size=0;int32_t chunk=-1;};
struct Deko3DDevice {
struct Chunk{uint32_t flags=0;DkMemBlock block=0;uint32_t size=0;std::vector<std::pair<uint32_t,uint32_t>> freeList;};
std::mutex heapMutex;std::vector<Chunk> chunks;
void collectRetired(){} // Test frees represent already completed fences.
DkMemBlock makeBlock(uint32_t,uint32_t,const char*){return chunks.size()+1;}
bool imageAlloc(uint32_t,uint32_t,Deko3DImageAlloc&);
bool dataAlloc(uint32_t,uint32_t,Deko3DImageAlloc&);
bool heapAlloc(uint32_t,uint32_t,uint32_t,uint32_t,Deko3DImageAlloc&);
};
"""
post=r"""
int main(){
Deko3DDevice d;std::mt19937 rng(0x1a2b);std::vector<Deko3DImageAlloc> vivos;
for(unsigned paso=0;paso<100000;++paso){
if(!vivos.empty() && (vivos.size()>500 || rng()%3==0)){
size_t i=rng()%vivos.size();auto x=vivos[i];chunkFree(d.chunks[x.chunk],x.offset,x.size);vivos.erase(vivos.begin()+i);
}else{
Deko3DImageAlloc x;uint32_t flags=4+(rng()%2)*4;
assert(d.dataAlloc(32+(rng()%16)*256,flags,x));assert(x.offset%256==0 && x.size%256==0);
assert(d.chunks[x.chunk].flags==flags && x.offset+x.size<=d.chunks[x.chunk].size);
for(auto y:vivos)if(x.block==y.block)assert(x.offset+x.size<=y.offset || y.offset+y.size<=x.offset);
vivos.push_back(x);
}}
for(auto x:vivos)chunkFree(d.chunks[x.chunk],x.offset,x.size);
for(auto& c:d.chunks)assert(c.freeList.size()==1 && c.freeList[0].first==0 && c.freeList[0].second==c.size);
assert(d.chunks.size()<=4);printf("100000 operations: no overlaps, correct alignment and reuse; %zu spans of 1 MiB\n",d.chunks.size());
}
"""
salida=raiz/"build/check-deko-heap";salida.mkdir(parents=True,exist_ok=True)
(salida/"test.cpp").write_text(pre+funciones+post,encoding="utf-8")
cc=Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"));env=os.environ.copy();env["PATH"]=str(cc.parent)+os.pathsep+env["PATH"]
env["TMP"]=env["TEMP"]=str(salida)
subprocess.run([str(cc),"-std=c++20","-O2","-static","test.cpp","-o","test.exe"],cwd=salida,env=env,check=True)
r=subprocess.run([str(salida/"test.exe")],cwd=salida,env=env,check=True,capture_output=True)
(salida/"resultado.log").write_bytes(r.stdout);print(r.stdout.decode())
