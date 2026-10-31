// Internal interfaces between the parts of the Switch SDL 3 shim.
#pragma once

#include <SDL3/SDL.h>

// The screen size, chosen once at start-up (sdl3_shim_video.c): 1920x1080 when the console starts docked,
// 1280x720 in handheld mode. LMNX-Deko3D sizes its swapchain from the same choice.
#ifdef __cplusplus
extern "C"
#endif
void lmnx_pantalla_tam(int* w, int* h);
#define SDL3_SHIM_WIDTH (sdl3_shim_ancho())
#define SDL3_SHIM_HEIGHT (sdl3_shim_alto())
int sdl3_shim_ancho(void);
int sdl3_shim_alto(void);

void sdl3_shim_set_error(const char* format, ...);

// Gamepad: configures libnx input once; pump samples it and queues
// SDL_EVENT_GAMEPAD_ADDED/REMOVED when the controller appears or goes.
void sdl3_shim_gamepad_init(void);
void sdl3_shim_gamepad_pump(void);

// Queues an event for SDL_PollEvent after running the event watches.
void sdl3_shim_queue_event(const SDL_Event* event);
