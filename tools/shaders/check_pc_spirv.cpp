// Test only the PC translator without creating a device, window or game.
#include "gpu_pc.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    try {
        std::ifstream f(argv[1]);if(!f)return 2;std::string linea;unsigned n=0;
        while(std::getline(f,linea)) {
            std::istringstream campos(linea);std::string etapa,ubo,ssbo,ruta;
            std::getline(campos,etapa,'\t');std::getline(campos,ubo,'\t');std::getline(campos,ssbo,'\t');std::getline(campos,ruta);
            std::ifstream entrada(ruta,std::ios::binary);if(!entrada)throw std::runtime_error("GLSL ausente");
            std::string glsl((std::istreambuf_iterator<char>(entrada)),{});
            auto spv=lmnx::pc::traducirSpirv(glsl,std::stoul(etapa));
            if(spv.size()<5 || spv[0]!=0x07230203)throw std::runtime_error("Invalid SPIR-V");
            ++n;
        }
        if(!n)return 2;std::cout<<"{\"etapas_spirv\":"<<n<<",\"gpu_ejecutada\":false,\"juego_ejecutado\":false}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
