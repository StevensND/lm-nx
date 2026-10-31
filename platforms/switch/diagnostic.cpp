#include "runtime.hpp"
#include <lmnx/backend.hpp>
#include <SDL3/SDL.h>
#include <switch.h>
#include <cstdio>
int main() {
    consoleInit(nullptr);
    lmnx::plataforma::configurarHilo(lmnx::plataforma::Rol::Juego);
    std::puts("LM-NX: platform diagnostic. Does not run the game.");
    std::string error;
    if (!lmnx::plataforma::prepararDirectorios(error)) std::puts(error.c_str());
    std::vector<uint8_t> dol;
    if (lmnx::plataforma::extraerDol(lmnx::plataforma::disco, dol, error))
        std::printf("ISO GLME01 revision 0: DOL read (%zu bytes). Retail SHA-1 verification pending.\n", dol.size());
    else std::puts(error.c_str());
    // Force linkage of the whole backend and uam without creating a GPU device alongside consoleInit's framebuffer.
    // GPU activation uses a separate presentation test.
    auto backend = lmnx::crearBackend();
    std::printf("Native interface: %s\n", backend ? "created; GPU device not started yet" : "failed");
    SDL_Init(SDL_INIT_GAMEPAD);
    std::puts("Press + to exit. Minus is reserved for the game's F1 menu.");
    PadState mando;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&mando);
    while (appletMainLoop()) {
        padUpdate(&mando);
        if (padGetButtonsDown(&mando) & HidNpadButton_Plus) break;
        consoleUpdate(nullptr);
    }
    backend.reset();
    SDL_Quit();
    consoleExit(nullptr);
    return 0;
}
