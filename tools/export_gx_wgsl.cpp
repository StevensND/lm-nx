// Host build tool using the original GX generator without a GPU device.
#include "gx/pipeline.hpp"
#include "gx/shader_info.hpp"
#include <lmnx/shader_source.hpp>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
namespace aurora { AuroraConfig g_config{.logLevel=LOG_WARNING}; }
namespace aurora::nativegpu { bool g_clipDistancesSupported=false; }
// The generator needs the final uniform block size, not a device. Match the Switch SDK's
// DK_UNIFORM_BUF_ALIGNMENT.
namespace aurora::gfx { uint32_t align_uniform(uint32_t v) { return (v+255u)&~255u; } }
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    std::ifstream datos(argv[1],std::ios::binary);std::filesystem::path salida=argv[2];std::filesystem::create_directories(salida);
    std::ofstream lista(salida/"lista.json");lista<<"{\"version\":1,\"shaders\":[";bool primero=true;uint32_t count=0,registros=0;datos.read(reinterpret_cast<char*>(&count),4);
    std::set<std::string> unicos;
    for(uint32_t i=0;i<count;++i) {
        aurora::gx::PipelineConfig config{};uint32_t n=0;datos.read(reinterpret_cast<char*>(&n),4);
        if(n!=sizeof config)return 3;datos.read(reinterpret_cast<char*>(&config),n);if(!datos || config.version!=aurora::gx::GXPipelineConfigVersion)return 3;
        // The initial list covers recorded configurations and integer TEV arithmetic. Additional menu variants may cause
        // cache misses.
        for(bool enteros:{false,true})for(auto alpha:{aurora::gx::DstAlphaMode::None,aurora::gx::DstAlphaMode::Replace,aurora::gx::DstAlphaMode::DualSource}) {
            auto variante=config;variante.shaderConfig.intTev=enteros;aurora::gx::canonicalize_pipeline_config(variante);
            auto wgsl=aurora::gx::build_shader_source(variante.shaderConfig,alpha);
            if(!unicos.insert(wgsl).second)continue;
            char nombre[64];std::snprintf(nombre,sizeof nombre,"gx-%016llx.wgsl",static_cast<unsigned long long>(lmnx::hashFuente(wgsl)));
            std::ofstream archivo(salida/nombre,std::ios::binary);archivo<<wgsl;if(!archivo)return 4;
            for(const char* etapa:{"vertex","fragment"}) {
                if(!primero)lista<<",";primero=false;
                lista<<"{\"wgsl\":\""<<nombre<<"\",\"entrada\":\""<<(etapa[0]=='v'?"vs_main":"fs_main")<<"\",\"etapa\":\""<<etapa<<"\",\"esquema\":\"gx\"}";
                ++registros;
            }
        }
    }
    lista<<"]}\n";std::printf("%u pipelines GX -> %zu unique WGSL modules, %u stages\n",count,unicos.size(),registros);return !lista;
}
