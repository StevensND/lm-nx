"""Check actual barrier selection without running a GPU."""
from pathlib import Path
import os
import os,subprocess
raiz=Path(__file__).resolve().parents[1]
s=(raiz/'LMNX-Deko3D/third_party/plume/plume_deko3d_commands.cpp').read_text(encoding='utf-8')
a=s.index('    void Deko3DCommandList::barriers(');b=s.index('    void Deko3DCommandList::emitBarrier(',a)
pre=r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
using RenderBarrierStages=uint32_t;
namespace RenderBarrierStage{constexpr unsigned COPY=1,COMPUTE=2,ALL=3;}
enum DkBarrier{DkBarrier_None,DkBarrier_Primitives,DkBarrier_Fragments,DkBarrier_Full};
constexpr uint32_t DkInvalidateFlags_Image=1,DkInvalidateFlags_Shader=2;
enum class RenderTextureLayout{UNKNOWN,SHADER_READ,DEPTH_READ,GENERAL,COPY_SOURCE,COPY_DEST,RESOLVE_SOURCE,RESOLVE_DEST,PRESENT,COLOR_WRITE,DEPTH_WRITE};
struct RenderTexture{};struct RenderTextureBarrier{RenderTexture* texture;RenderTextureLayout layout;};struct RenderBufferBarrier{};
struct Estadisticas{std::atomic<uint64_t> lecturasOmitidas{0};} estadisticas;
auto& GetDeko3DStats(){return estadisticas;}
struct Opciones{bool mergeBarriers=true,leanBarriers=true;} opciones;
auto& Deko3DGetOptions(){return opciones;}
struct Deko3DCommandList{
std::unordered_map<const RenderTexture*,RenderTextureLayout> estadosTexturas;
bool pendingBarrier=false;DkBarrier pendingBarrierMode=DkBarrier_None;uint32_t pendingBarrierInvalidate=0,emitidas=0;
void emitBarrier(DkBarrier,uint32_t){++emitidas;}
void barriers(RenderBarrierStages,const RenderBufferBarrier*,uint32_t,const RenderTextureBarrier*,uint32_t);
};
'''
post=r'''
int main(){for(bool fusion:{false,true}){
opciones.mergeBarriers=fusion;Deko3DCommandList c;RenderTexture t;
auto paso=[&](RenderTextureLayout layout,bool debeEmitir){
c.pendingBarrier=false;auto antes=c.emitidas;RenderTextureBarrier b{&t,layout};
c.barriers(RenderBarrierStage::ALL,nullptr,0,&b,1);
assert((c.pendingBarrier || c.emitidas!=antes)==debeEmitir);
};
paso(RenderTextureLayout::SHADER_READ,true);paso(RenderTextureLayout::SHADER_READ,false);
for(auto escritura:{RenderTextureLayout::COLOR_WRITE,RenderTextureLayout::DEPTH_WRITE,RenderTextureLayout::COPY_DEST,RenderTextureLayout::GENERAL}){
paso(escritura,true);paso(RenderTextureLayout::SHADER_READ,true);paso(RenderTextureLayout::SHADER_READ,false);
}
c.estadosTexturas.clear();paso(RenderTextureLayout::SHADER_READ,true);
RenderTextureBarrier b{&t,RenderTextureLayout::SHADER_READ};RenderBufferBarrier buffer;
c.pendingBarrier=false;auto antes=c.emitidas;c.barriers(RenderBarrierStage::ALL,&buffer,1,&b,1);
assert(c.pendingBarrier || c.emitidas!=antes);
}printf("Actual barriers: redundant read skipped; write/copy/general and buffer preserved, with and without merging\n");}
'''
salida=raiz/'build/check-deko-barriers';salida.mkdir(parents=True,exist_ok=True)
(salida/'test.cpp').write_text(pre+s[a:b]+post,encoding='utf-8')
cc=Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"));env=os.environ.copy();env['PATH']=str(cc.parent)+os.pathsep+env['PATH'];env['TMP']=env['TEMP']=str(salida)
subprocess.run([str(cc),'-std=c++20','-O2','-static','test.cpp','-o','test.exe'],cwd=salida,env=env,check=True)
r=subprocess.run([str(salida/'test.exe')],cwd=salida,env=env,capture_output=True,check=True)
(salida/'resultado.log').write_bytes(r.stdout);print(r.stdout.decode())
