#include "imgui_lmnx.hpp"
#include <backends/imgui_impl_sdl3.h>
namespace lmnx::imgui {
void iniciarEntrada(SDL_Window* w){ImGui_ImplSDL3_InitForOther(w);auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard;io.ConfigFlags&=~ImGuiConfigFlags_ViewportsEnable;}
void cerrarEntrada(){ImGui_ImplSDL3_Shutdown();}
void procesarEvento(const SDL_Event* e){ImGui_ImplSDL3_ProcessEvent(e);}
void nuevaEntrada(){ImGui_ImplSDL3_NewFrame();}
}
