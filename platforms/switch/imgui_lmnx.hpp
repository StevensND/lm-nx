#pragma once
#include <lmnx/aurora_gpu.hpp>
#include <imgui.h>
#include <SDL3/SDL.h>
namespace lmnx::imgui {
void iniciar(const gpu::Device&,gpu::TextureFormat);
void cerrar();
void prepararFuentes();
ImTextureID registrarVista(const gpu::TextureView&);
void dibujar(const gpu::RenderPassEncoder&,ImDrawData*);
void iniciarEntrada(SDL_Window*);
void cerrarEntrada();
void procesarEvento(const SDL_Event*);
void nuevaEntrada();
}
