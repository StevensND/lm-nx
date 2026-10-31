// Host batch compiler producing Maxwell DKSH only, without a renderer.
#include <uam_api.h>
#include <lmnx/cache_format.hpp>
#include <lmnx/shader_source.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "Uso: uam_cache lista.tsv directorio_cache\n"; return 2; }
    std::ifstream lista(argv[1]);
    if (!lista) { std::cerr << "Could not read the list\n"; return 2; }
    uam::setBanjoFlags(447); uam::init(true);
    int codigo = 0;
    try {
        std::filesystem::path destino = argv[2];
        std::filesystem::create_directories(destino);
        std::string linea;
        size_t filas=0,aciertos=0;
        while (std::getline(lista, linea)) {
            if (linea.empty()) continue;
            std::istringstream campos(linea);
            std::string etapaTexto, uboTexto, ssboTexto, ruta;
            std::getline(campos, etapaTexto, '\t'); std::getline(campos, uboTexto, '\t');
            std::getline(campos, ssboTexto, '\t'); std::getline(campos, ruta);
            const uint32_t etapa = std::stoul(etapaTexto);
            if (etapa > 5) throw std::runtime_error("Invalid stage");
            std::ifstream entrada(ruta, std::ios::binary);
            if (!entrada) throw std::runtime_error("Could not read " + ruta);
            std::string fuente((std::istreambuf_iterator<char>(entrada)), {});
            if (fuente.size() > 4 * 1024 * 1024 || fuente.find('\0') != std::string::npos)
                throw std::runtime_error("Source too large or contains NUL");
            fuente = lmnx::prepararGLSL(fuente, std::stoul(uboTexto), std::stoul(ssboTexto));
            char nombre[48];
            std::snprintf(nombre, sizeof nombre, "s%u-%016llx.lmc", etapa, static_cast<unsigned long long>(lmnx::hashFuente(fuente)));
            const auto final = destino / nombre;
            ++filas;
            std::ifstream existente(final,std::ios::binary|std::ios::ate);
            if(existente && existente.tellg()>=28 && existente.tellg()<=12*1024*1024+28) {
                std::vector<uint8_t> bytes(static_cast<size_t>(existente.tellg())),dksh;
                existente.seekg(0);existente.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
                if(existente && lmnx::desempaquetarCache(etapa,fuente,bytes,dksh)) {
                    ++aciertos;
                    if(filas%100==0)std::cout<<"uam: "<<filas<<" etapas, "<<aciertos<<" aciertos validados\n";
                    continue;
                }
            }
            existente.close();
            const auto resultado = uam::compile(uam::Stage(etapa), fuente.c_str());
            if (!resultado.ok) throw std::runtime_error("uam: " + ruta + "\n" + resultado.log);
            auto archivo = lmnx::empaquetarCache(etapa, fuente, resultado.dksh);
            if (archivo.empty()) throw std::runtime_error("uam returned invalid DKSH");
            auto temporal = final; temporal += ".tmp";
            {
                std::ofstream salida(temporal, std::ios::binary | std::ios::trunc);
                salida.write(reinterpret_cast<const char*>(archivo.data()), archivo.size()); salida.flush();
                if (!salida) throw std::runtime_error("Error writing the cache");
            }
            // Windows tool: remove the destination entry before rename. The output directory belongs to the generator, not
            // the live cache.
            std::error_code error;
            std::filesystem::remove(final, error);
            std::filesystem::rename(temporal, final);
            std::cout << nombre << ": " << resultado.dksh.size() << " bytes DKSH\n";
        }
        std::cout<<"uam: "<<filas<<" stages passed; "<<aciertos<<" reused after comparing source, stage, CRC and DKSH\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; codigo = 1; }
    uam::shutdown();
    return codigo;
}
