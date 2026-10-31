"""Check actual command-list splitting without a device or game execution."""
from pathlib import Path
import os
import os,subprocess
raiz=Path(__file__).resolve().parents[1]
s=(raiz/'LMNX-Deko3D/third_party/plume/plume_deko3d_commands.cpp').read_text(encoding='utf-8')
a=s.index('    void Deko3DCommandList::cerrarSegmento()');b=s.index('    void Deko3DCommandList::end()',a)
codigo=s[a:b]
pre=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
using DkCmdList=uintptr_t;
struct Grabador{std::vector<unsigned> pendiente;std::vector<std::vector<unsigned>> listas;};
DkCmdList dkCmdBufFinishList(Grabador* g){if(g->pendiente.empty())return 0;g->listas.push_back(g->pendiente);g->pendiente.clear();return g->listas.size();}
struct Deko3DCommandList{
Grabador* cmd;std::vector<DkCmdList> segmentos;uint32_t trabajosSegmento=0,operacionesSegmento=0;
void flushBarriers(){};void cerrarSegmento();void limitarTrabajos();void limitarOperaciones();
};
'''
post=r'''
int main(){Grabador g;Deko3DCommandList c{&g};std::vector<unsigned> esperado;
for(unsigned i=0;i<100000;++i){
g.pendiente.push_back(i);esperado.push_back(i);
if(i%3)c.limitarTrabajos();c.limitarOperaciones();
assert(c.trabajosSegmento<64 && c.operacionesSegmento<128);
}
c.cerrarSegmento();std::vector<unsigned> recibido;
for(auto h:c.segmentos){assert(h>0 && h<=g.listas.size());auto& l=g.listas[h-1];recibido.insert(recibido.end(),l.begin(),l.end());}
assert(recibido==esperado);assert(c.segmentos.size()>100);
printf("100000 operations: order preserved, no omissions or duplicates; %zu segments\n",c.segmentos.size());}
'''
salida=raiz/'build/check-deko-segments';salida.mkdir(parents=True,exist_ok=True)
(salida/'test.cpp').write_text(pre+codigo+post,encoding='utf-8')
cc=Path(str(Path(os.environ.get("DEVKITPRO", "/opt/devkitpro")) / "msys2/mingw64/bin/g++.exe"));env=os.environ.copy();env['PATH']=str(cc.parent)+os.pathsep+env['PATH'];env['TMP']=env['TEMP']=str(salida)
subprocess.run([str(cc),'-std=c++20','-O2','-static','test.cpp','-o','test.exe'],cwd=salida,env=env,check=True)
r=subprocess.run([str(salida/'test.exe')],cwd=salida,env=env,capture_output=True,check=True)
(salida/'resultado.log').write_bytes(r.stdout);print(r.stdout.decode())
