// Host tool: Tint translates WGSL, then uam compiles the resulting GLSL.
#include "shader_translate.h"
#include <lmnx/translation_cache.hpp>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <sstream>
#include <vector>

int traducir(int argc, char** argv) {
    if (argc != 7 && argc != 8) {
        std::fprintf(stderr, "Usage: wgsl_to_glsl input.wgsl entry vertex|fragment gx|sequential output.glsl metadata.json\n");
        return 2;
    }
    const std::string etapa = argv[3], esquema = argv[4];
    if ((etapa != "vertex" && etapa != "fragment" && etapa != "compute") || (esquema != "gx" && esquema != "sequential")) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 2;
    const std::string wgsl((std::istreambuf_iterator<char>(input)), {});
    const auto stage = etapa == "vertex" ? swdk::ShaderStage::Vertex : etapa == "compute" ? swdk::ShaderStage::Compute : swdk::ShaderStage::Fragment;
    auto r = swdk::wgsl_to_glsl(wgsl, argv[2], stage,
                              esquema == "gx" ? swdk::BindingScheme::GX : swdk::BindingScheme::Sequential);
    if (!r.ok) { std::fprintf(stderr, "%s\n", r.error.c_str()); return 1; }
    if(argc==8)lmnx::guardarTraduccion(argv[7],lmnx::claveTraduccion(wgsl,argv[2],uint32_t(stage),esquema=="gx"?swdk::BindingScheme::GX:swdk::BindingScheme::Sequential),r);
    uint32_t ubo = 0, ssbo = 0, texturas = 0;
    for (const auto& b : r.bindings) {
        if (b.slot >= 32) return 1;
        switch (b.kind) {
        case swdk::SlotBinding::Kind::Uniform:
        case swdk::SlotBinding::Kind::Immediates: ubo |= 1u << b.slot; break;
        case swdk::SlotBinding::Kind::Storage: ssbo |= 1u << b.slot; break;
        case swdk::SlotBinding::Kind::Sampler: texturas |= 1u << b.slot; break;
        }
    }
    std::ofstream glsl(argv[5], std::ios::binary), meta(argv[6]);
    glsl << r.glsl;
    meta << "{\"ubo\":" << ubo << ",\"ssbo\":" << ssbo << ",\"texturas\":" << texturas
         << ",\"inmediatos_bytes\":" << r.immediateSize << ",\"revision\":" << swdk::kTranslateRevision << "}\n";
    return glsl && meta ? 0 : 2;
}
int main(int argc,char** argv) {
    swdk::translate_init();
    if(argc!=3 || std::string_view(argv[1])!="--lote")return traducir(argc,argv);
    std::ifstream lista(argv[2]);if(!lista)return 2;std::string linea;size_t n=0;
    while(std::getline(lista,linea)) {
        if(linea.empty())continue;std::istringstream columnas(linea);std::vector<std::string> campos{"wgsl_to_glsl"};std::string c;
        while(std::getline(columnas,c,'\t'))campos.push_back(c);
        std::vector<char*> args;for(auto& s:campos)args.push_back(s.data());
        int rc=traducir(int(args.size()),args.data());
        if(rc){std::fprintf(stderr,"Tint failed on row %zu: %s\n",n+1,linea.c_str());return rc;}
        if(++n%100==0){std::printf("Tint: %zu stages translated\n",n);std::fflush(stdout);}
    }
    std::printf("Tint: batch complete, %zu stages\n",n);return 0;
}
