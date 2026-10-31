// PC translation keeps native GLSL and reflection and adapts the ABI to SPIR-V.
#include "gpu_pc.hpp"
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <mutex>
#include <regex>
#include <stdexcept>

namespace lmnx::pc {
std::vector<uint32_t> traducirSpirv(const std::string& original,uint32_t etapa) {
    static std::once_flag inicio;std::call_once(inicio,[]{if(!glslang_initialize_process())throw std::runtime_error("Could not initialize glslang");});
    // A set per stage prevents vertex and fragment UBO/SSBO/texture aliases.
    const uint32_t set=etapa==4?1:0;
    // Tint writes UBO/SSBO blocks as "layout(binding = N, std140)\nuniform X {"; \s+ covers the newline.
    const std::regex declaracion(R"(layout\(binding = (\d+)([^)]*)\)\s+([^\n]+))");
    std::string fuente;size_t anterior=0;
    for(auto i=std::sregex_iterator(original.begin(),original.end(),declaracion);i!=std::sregex_iterator();++i) {
        const auto& m=*i;fuente+=original.substr(anterior,m.position()-anterior);
        unsigned binding=std::stoul(m[1]);std::string resto=m[3];
        std::smatch sampler;
        if(std::regex_match(resto,sampler,std::regex(R"(uniform (?:highp )?([iu]?)sampler(\w+) (\w+);)"))) {
            std::string tipo=sampler[2],nombre=sampler[3],prefijo=sampler[1];
            bool sombra=tipo.ends_with("Shadow");if(sombra)tipo.resize(tipo.size()-6);
            fuente+="layout(set="+std::to_string(set)+",binding="+std::to_string(32+binding)+") uniform "+prefijo+"texture"+tipo+" "+nombre+"_imagen;\n";
            fuente+="layout(set="+std::to_string(set)+",binding="+std::to_string(64+binding)+") uniform sampler"+(sombra?"Shadow":"")+" "+nombre+"_filtro;\n";
            fuente+="#define "+nombre+" "+prefijo+"sampler"+tipo+(sombra?"Shadow":"")+"("+nombre+"_imagen,"+nombre+"_filtro)\n";
        } else {
            if(resto.find("buffer ")!=std::string::npos)binding+=16;
            fuente+="layout(set="+std::to_string(set)+",binding="+std::to_string(binding)+m[2].str()+") "+resto;
        }
        anterior=m.position()+m.length();
    }
    fuente+=original.substr(anterior);
    fuente=std::regex_replace(fuente,std::regex(R"(\bgl_VertexID\b)"),"gl_VertexIndex");
    fuente=std::regex_replace(fuente,std::regex(R"(\bgl_InstanceID\b)"),"gl_InstanceIndex");
    glslang_input_t entrada{};entrada.language=GLSLANG_SOURCE_GLSL;
    entrada.stage=etapa==0?GLSLANG_STAGE_VERTEX:etapa==4?GLSLANG_STAGE_FRAGMENT:GLSLANG_STAGE_COMPUTE;
    entrada.client=GLSLANG_CLIENT_VULKAN;entrada.client_version=GLSLANG_TARGET_VULKAN_1_1;
    entrada.target_language=GLSLANG_TARGET_SPV;entrada.target_language_version=GLSLANG_TARGET_SPV_1_3;
    entrada.code=fuente.c_str();entrada.default_version=460;entrada.default_profile=GLSLANG_CORE_PROFILE;
    entrada.messages=glslang_messages_t(GLSLANG_MSG_SPV_RULES_BIT|GLSLANG_MSG_VULKAN_RULES_BIT);entrada.resource=glslang_default_resource();
    auto shader=glslang_shader_create(&entrada);auto programa=glslang_program_create();
    auto liberar=[&]{glslang_program_delete(programa);glslang_shader_delete(shader);};
    if(!glslang_shader_preprocess(shader,&entrada)||!glslang_shader_parse(shader,&entrada)) {
        std::string error=glslang_shader_get_info_log(shader);liberar();throw std::runtime_error("GLSL PC: "+error);
    }
    glslang_program_add_shader(programa,shader);
    if(!glslang_program_link(programa,entrada.messages)){std::string error=glslang_program_get_info_log(programa);liberar();throw std::runtime_error(error);}
    glslang_program_SPIRV_generate(programa,entrada.stage);auto n=glslang_program_SPIRV_get_size(programa);
    if(!n){liberar();throw std::runtime_error("glslang no produjo SPIR-V");}
    auto* datos=glslang_program_SPIRV_get_ptr(programa);std::vector<uint32_t> salida(datos,datos+n);liberar();return salida;
}
}
