#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
namespace lmnx::plataforma {
inline constexpr char disco[] = "sdmc:/switch/LM-NX/GLME01.iso";
inline constexpr char raiz[] = "sdmc:/switch/LM-NX";
inline constexpr char guardados[] = "sdmc:/switch/LM-NX/guardados";
bool prepararDirectorios(std::string& error);
// Only uncompressed GLME01 revision 0 ISO images; no instruction interpretation.
bool extraerDol(const char* ruta, std::vector<uint8_t>& dol, std::string& error);
enum class Rol { Juego, Render, Audio, Disco, Compilador };
bool configurarHilo(Rol rol);
// Asynchronous log (log_asincrono.cpp): stdout and stderr use memory rings written to SD by a low-priority
// thread. vaciarLog immediately writes pending bytes for crash handling.
bool iniciarLogAsincrono();
void vaciarLog(bool urgencia);
// exit(): stop the log thread and write pending bytes before stdio closes files.
void detenerLog();
}
