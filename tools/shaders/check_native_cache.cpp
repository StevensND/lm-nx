// Host verification with the same binary readers as the NRO.
#include <lmnx/translation_cache.hpp>
#include <iostream>
#include <set>
#include <sstream>

static std::string leer(const std::filesystem::path& ruta) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) throw std::runtime_error("Could not read " + ruta.string());
    return {(std::istreambuf_iterator<char>(f)), {}};
}
static std::vector<std::string> campos(const std::string& linea) {
    std::istringstream f(linea); std::string s; std::vector<std::string> r;
    while(std::getline(f,s,'\t'))r.push_back(s);
    return r;
}
int main(int argc,char** argv) {
    if(argc!=4){std::cerr<<"Uso: check_native_cache traducciones.tsv lista.tsv cache_dksh\n";return 2;}
    try {
        std::ifstream traducciones(argv[1]),lista(argv[2]);
        if(!traducciones || !lista)throw std::runtime_error("Listas ausentes");
        std::string a,b; size_t cantidad=0; std::set<std::string> tint,dksh;
        while(std::getline(traducciones,a)) {
            if(!std::getline(lista,b))throw std::runtime_error("Listas de diferente longitud");
            auto t=campos(a),d=campos(b);
            if(t.size()!=7 || d.size()!=4)throw std::runtime_error("Fila TSV incorrecta");
            auto etapa=uint32_t(std::stoul(d[0]));
            if(t[2]!=(etapa==0?"vertex":etapa==4?"fragment":"compute"))throw std::runtime_error("Etapas diferentes");
            auto clave=lmnx::claveTraduccion(leer(t[0]),t[1],etapa,t[3]=="gx"?swdk::BindingScheme::GX:swdk::BindingScheme::Sequential);
            auto ruta=lmnx::rutaTraduccion(t[6],clave);auto bruto=leer(ruta);
            std::vector<uint8_t> bytes(bruto.begin(),bruto.end());swdk::TranslateResult r;
            if(!lmnx::desempaquetarTraduccion(clave,bytes,r) || r.glsl!=leer(t[4]))throw std::runtime_error("Invalid translation: "+ruta.string());
            uint32_t ubo=0,ssbo=0;
            for(const auto& slot:r.bindings) {
                if(slot.kind==swdk::SlotBinding::Kind::Uniform || slot.kind==swdk::SlotBinding::Kind::Immediates)ubo|=1u<<slot.slot;
                if(slot.kind==swdk::SlotBinding::Kind::Storage)ssbo|=1u<<slot.slot;
            }
            if(ubo!=std::stoul(d[1]) || ssbo!=std::stoul(d[2]))throw std::runtime_error("Masks differ from reflection used by the NRO");
            auto fuente=lmnx::prepararGLSL(r.glsl,std::stoul(d[1]),std::stoul(d[2]));
            char nombre[48];std::snprintf(nombre,sizeof nombre,"s%u-%016llx.lmc",etapa,static_cast<unsigned long long>(lmnx::hashFuente(fuente)));
            auto fichero=std::filesystem::path(argv[3])/nombre;auto bin=leer(fichero);
            std::vector<uint8_t> cache(bin.begin(),bin.end()),programa;
            if(!lmnx::desempaquetarCache(etapa,fuente,cache,programa))throw std::runtime_error("Invalid DKSH: "+fichero.string());
            if(cantidad==0) {
                bytes.back()^=1;
                if(lmnx::desempaquetarTraduccion(clave,bytes,r))throw std::runtime_error("No rechaza CRC Tint incorrecto");
                bytes.back()^=1;
                if(lmnx::desempaquetarTraduccion(clave+"x",bytes,r))throw std::runtime_error("No compara clave WGSL completa");
                cache.back()^=1;
                if(lmnx::desempaquetarCache(etapa,fuente,cache,programa))throw std::runtime_error("No rechaza CRC DKSH incorrecto");
                cache.back()^=1;
                if(lmnx::desempaquetarCache((etapa+1)%6,fuente,cache,programa))throw std::runtime_error("No rechaza etapa DKSH incorrecta");
            }
            tint.insert(ruta.string());dksh.insert(fichero.string());++cantidad;
        }
        if(!cantidad || std::getline(lista,b))throw std::runtime_error("Empty or mismatched lists");
        std::cout<<"{\"etapas_verificadas\":"<<cantidad<<",\"traducciones_unicas\":"<<tint.size()<<",\"dksh_unicos\":"<<dksh.size()<<",\"negativas_crc_clave_etapa\":4,\"consola\":false}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
