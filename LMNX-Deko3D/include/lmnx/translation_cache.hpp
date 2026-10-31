#pragma once
#include "cache_format.hpp"
#include "cache_paquete.hpp"
#include "shader_source.hpp"
#include "shader_translate.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
namespace lmnx {
inline std::string claveTraduccion(const std::string& wgsl,const std::string& entrada,uint32_t etapa,swdk::BindingScheme esquema) {
    return wgsl + std::string(1,'\0') + entrada + std::string(1,'\0') + std::to_string(etapa) + ";" + std::to_string(uint32_t(esquema));
}
inline std::filesystem::path rutaTraduccion(const std::filesystem::path& raiz,const std::string& clave) {
    char nombre[48];std::snprintf(nombre,sizeof nombre,"t-%016llx.lmt",static_cast<unsigned long long>(hashFuente(clave)));return raiz/nombre;
}
inline std::vector<uint8_t> empaquetarTraduccion(const std::string& clave,const swdk::TranslateResult& r) {
    if(!r.ok || clave.size()>4*1024*1024 || r.glsl.size()>4*1024*1024 || r.bindings.size()>256)return {};
    std::vector<uint8_t> bytes;
    auto w=[&](uint32_t v){for(int k=0;k<4;++k)bytes.push_back(uint8_t(v>>(k*8)));};
    for(auto v:{0x31544d4cu,swdk::kTranslateRevision,uint32_t(clave.size()),uint32_t(r.glsl.size()),uint32_t(r.bindings.size()),r.immediateSize,r.workgroupX,r.workgroupY,r.workgroupZ})w(v);
    bytes.insert(bytes.end(),clave.begin(),clave.end());bytes.insert(bytes.end(),r.glsl.begin(),r.glsl.end());
    for(const auto& b:r.bindings){w(uint32_t(b.kind));w(b.slot);w(b.group);w(b.binding);w(b.samplerGroup);w(b.samplerBinding);}
    auto crc=crc32(bytes.data(),bytes.size());w(crc);return bytes;
}
inline bool desempaquetarTraduccion(const std::string& clave,const std::vector<uint8_t>& bytes,swdk::TranslateResult& r) {
    if(bytes.size()<40 || bytes.size()>9*1024*1024)return false;
    auto w=[&](size_t n){return uint32_t(bytes[n])|uint32_t(bytes[n+1])<<8|uint32_t(bytes[n+2])<<16|uint32_t(bytes[n+3])<<24;};
    auto nc=w(8),ng=w(12),nb=w(16);
    if(w(0)!=0x31544d4c || w(4)!=swdk::kTranslateRevision || nc!=clave.size() || ng>4*1024*1024 || nb>256 ||
       uint64_t(40)+nc+ng+uint64_t(nb)*24!=bytes.size() || std::memcmp(bytes.data()+36,clave.data(),nc) || w(bytes.size()-4)!=crc32(bytes.data(),bytes.size()-4))return false;
    swdk::TranslateResult valor;valor.glsl.assign(reinterpret_cast<const char*>(bytes.data()+36+nc),ng);valor.immediateSize=w(20);valor.workgroupX=w(24);valor.workgroupY=w(28);valor.workgroupZ=w(32);
    if(valor.immediateSize>64 || !valor.workgroupX || !valor.workgroupY || !valor.workgroupZ)return false;
    for(uint32_t k=0;k<nb;++k){size_t p=36+nc+ng+k*24;if(w(p)>3 || w(p+4)>31)return false;valor.bindings.push_back({swdk::SlotBinding::Kind(w(p)),uint8_t(w(p+4)),w(p+8),w(p+12),w(p+16),w(p+20)});}
    valor.ok=true;r=std::move(valor);return true;
}
// Translations live in one file (cache_paquete.hpp) inside the cache directory.
inline PaqueteCache& paqueteTraduccion() { static PaqueteCache paquete; return paquete; }
inline void abrirPaqueteTraduccion(const std::filesystem::path& raiz) {
    if(raiz.empty()){paqueteTraduccion().cerrar();return;}
    std::error_code ec;std::filesystem::create_directories(raiz,ec);
    paqueteTraduccion().abrir(raiz/"paquete.lmp");
}
inline bool leerTraduccion(const std::filesystem::path& raiz,const std::string& clave,swdk::TranslateResult& resultado) {
    if(raiz.empty())return false;
    std::vector<uint8_t> bytes;const uint64_t h=hashFuente(clave);
    if(paqueteTraduccion().buscar(h,bytes) && desempaquetarTraduccion(clave,bytes,resultado))return true;
    // Read a legacy loose entry and import it into the package.
    std::ifstream archivo(rutaTraduccion(raiz,clave),std::ios::binary|std::ios::ate);if(!archivo)return false;
    auto n=archivo.tellg();if(n<40 || n>9*1024*1024)return false;bytes.resize(static_cast<size_t>(n));archivo.seekg(0);archivo.read(reinterpret_cast<char*>(bytes.data()),n);
    if(!archivo || !desempaquetarTraduccion(clave,bytes,resultado))return false;
    paqueteTraduccion().guardar(h,bytes);return true;
}
inline void guardarTraduccion(const std::filesystem::path& raiz,const std::string& clave,const swdk::TranslateResult& resultado) {
    if(raiz.empty())return;auto bytes=empaquetarTraduccion(clave,resultado);if(bytes.empty())return;
    paqueteTraduccion().guardar(hashFuente(clave),bytes);
}
}
