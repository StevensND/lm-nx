// SDL3 Switch video shim: one fullscreen window, 1920x1080 when starting docked or 1280x720 handheld. Select
// once; later mode changes keep its size and let the console scale the image. Aurora renders through LMNX-Deko3D
// and presents to NWindow. SDL only answers size/state queries. There is no SDL renderer; creation fails, which
// Aurora's ImGui path tolerates.
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "sdl3_shim.h"

#include <switch.h>

void lmnx_pantalla_tam(int* w, int* h) {
    static int ancho = 0, alto = 0;
    if (!ancho) {
        const bool dock = appletGetOperationMode() == AppletOperationMode_Console;
        ancho = dock ? 1920 : 1280;
        alto = dock ? 1080 : 720;
    }
    if (w) *w = ancho;
    if (h) *h = alto;
}
int sdl3_shim_ancho(void) { int w; lmnx_pantalla_tam(&w, NULL); return w; }
int sdl3_shim_alto(void) { int h; lmnx_pantalla_tam(NULL, &h); return h; }

struct SDL_Window {
    SDL_PropertiesID properties;
};

static struct SDL_Window g_window;
static bool g_window_open;

SDL_Window* SDL_CreateWindowWithProperties(SDL_PropertiesID props) {
    (void)props;
    if (g_window_open) {
        sdl3_shim_set_error("the Switch has one window");
        return NULL;
    }
    g_window.properties = SDL_CreateProperties();
    g_window_open = true;
    return &g_window;
}

void SDL_DestroyWindow(SDL_Window* window) {
    if (window == &g_window)
        g_window_open = false;
}

SDL_PropertiesID SDL_GetWindowProperties(SDL_Window* window) {
    return window != NULL ? window->properties : 0;
}
SDL_WindowID SDL_GetWindowID(SDL_Window* window) {return window==&g_window && g_window_open?1:0;}
SDL_Window* SDL_GetWindowFromID(SDL_WindowID id) {return id==1 && g_window_open?&g_window:NULL;}
SDL_Window* SDL_GetWindowFromEvent(const SDL_Event* e) {return e?SDL_GetWindowFromID(e->window.windowID):NULL;}
bool SDL_GetWindowPosition(SDL_Window* window,int* x,int* y) {if(x)*x=0;if(y)*y=0;return window==&g_window && g_window_open;}
SDL_DisplayID SDL_GetDisplayForWindow(SDL_Window* window) {return window==&g_window && g_window_open?1:0;}
SDL_DisplayID* SDL_GetDisplays(int* count) {SDL_DisplayID* ids=calloc(2,sizeof *ids);if(ids)ids[0]=1;if(count)*count=ids?1:0;return ids;}
float SDL_GetDisplayContentScale(SDL_DisplayID id) {return id==1?1.f:0.f;}
bool SDL_GetDisplayBounds(SDL_DisplayID id,SDL_Rect* r) {if(id!=1 || !r)return SDL_SetError("Pantalla inexistente");*r=(SDL_Rect){0,0,SDL3_SHIM_WIDTH,SDL3_SHIM_HEIGHT};return true;}
bool SDL_GetDisplayUsableBounds(SDL_DisplayID id,SDL_Rect* r) {return SDL_GetDisplayBounds(id,r);}
const SDL_DisplayMode* SDL_GetCurrentDisplayMode(SDL_DisplayID id) {
    // VI exposes no precise refresh rate here. SDL permits zero for unavailable.
    static SDL_DisplayMode modo={.displayID=1,.format=SDL_PIXELFORMAT_RGBA32,.pixel_density=1.f,.refresh_rate=0.f};
    modo.w=SDL3_SHIM_WIDTH; modo.h=SDL3_SHIM_HEIGHT;
    return id==1?&modo:NULL;
}

bool SDL_GetWindowSize(SDL_Window* window, int* w, int* h) {
    (void)window;
    if (w != NULL)
        *w = SDL3_SHIM_WIDTH;
    if (h != NULL)
        *h = SDL3_SHIM_HEIGHT;
    return true;
}

bool SDL_GetWindowSizeInPixels(SDL_Window* window, int* w, int* h) {
    return SDL_GetWindowSize(window, w, h);
}

float SDL_GetWindowDisplayScale(SDL_Window* window) {
    (void)window;
    return 1.0f;
}

SDL_WindowFlags SDL_GetWindowFlags(SDL_Window* window) {
    (void)window;
    return SDL_WINDOW_FULLSCREEN | SDL_WINDOW_INPUT_FOCUS;
}

// The window's state cannot change: these succeed without effect.
bool SDL_ShowWindow(SDL_Window* window) { (void)window; return true; }
bool SDL_RaiseWindow(SDL_Window* window) { (void)window; return true; }
bool SDL_RestoreWindow(SDL_Window* window) { (void)window; return true; }
bool SDL_SyncWindow(SDL_Window* window) { (void)window; return true; }
bool SDL_SetWindowFullscreen(SDL_Window* window, bool fullscreen) {
    (void)window;
    (void)fullscreen;
    return true;
}
bool SDL_SetWindowIcon(SDL_Window* window, SDL_Surface* icon) {
    (void)window;
    (void)icon;
    return true;
}
bool SDL_SetWindowMinimumSize(SDL_Window* window, int min_w, int min_h) {
    (void)window;
    (void)min_w;
    (void)min_h;
    return true;
}
bool SDL_SetWindowPosition(SDL_Window* window, int x, int y) {
    (void)window;
    (void)x;
    (void)y;
    return true;
}
bool SDL_SetWindowSize(SDL_Window* window, int w, int h) {
    (void)window;
    (void)w;
    (void)h;
    return true;
}
bool SDL_SetWindowTitle(SDL_Window* window, const char* title) {
    (void)window;
    (void)title;
    return true;
}
bool SDL_DisableScreenSaver(void) { return true; }
bool SDL_EnableScreenSaver(void) { return true; }

const char* SDL_GetCurrentVideoDriver(void) { return "switch"; }
SDL_DisplayID SDL_GetPrimaryDisplay(void) { return 1; }
SDL_DisplayOrientation SDL_GetCurrentDisplayOrientation(SDL_DisplayID displayID) {
    (void)displayID;
    return SDL_ORIENTATION_LANDSCAPE;
}
SDL_DisplayOrientation SDL_GetNaturalDisplayOrientation(SDL_DisplayID displayID) {
    (void)displayID;
    return SDL_ORIENTATION_LANDSCAPE;
}

// No SDL renderer on the Switch.
SDL_Renderer* SDL_CreateRendererWithProperties(SDL_PropertiesID props) {
    (void)props;
    sdl3_shim_set_error("no SDL renderer on the Switch");
    return NULL;
}
void SDL_DestroyRenderer(SDL_Renderer* renderer) { (void)renderer; }
SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, SDL_PixelFormat format,
                               SDL_TextureAccess access, int w, int h) {
    (void)renderer;
    (void)format;
    (void)access;
    (void)w;
    (void)h;
    return NULL;
}
void SDL_DestroyTexture(SDL_Texture* texture) { (void)texture; }
bool SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect, const void* pixels, int pitch) {
    (void)texture;
    (void)rect;
    (void)pixels;
    (void)pitch;
    return false;
}
bool SDL_SetTextureScaleMode(SDL_Texture* texture, SDL_ScaleMode scaleMode) {
    (void)texture;
    (void)scaleMode;
    return false;
}
bool SDL_GetRenderOutputSize(SDL_Renderer* renderer, int* w, int* h) {
    (void)renderer;
    return SDL_GetWindowSize(NULL, w, h);
}
bool SDL_GetRenderScale(SDL_Renderer* renderer, float* scaleX, float* scaleY) {
    (void)renderer;
    if (scaleX != NULL)
        *scaleX = 1.0f;
    if (scaleY != NULL)
        *scaleY = 1.0f;
    return true;
}
bool SDL_SetRenderScale(SDL_Renderer* renderer, float scaleX, float scaleY) {
    (void)renderer;
    (void)scaleX;
    (void)scaleY;
    return false;
}
bool SDL_SetRenderLogicalPresentation(SDL_Renderer* renderer, int w, int h,
                                      SDL_RendererLogicalPresentation mode) {
    (void)renderer;
    (void)w;
    (void)h;
    (void)mode;
    return false;
}
bool SDL_RenderClear(SDL_Renderer* renderer) { (void)renderer; return false; }
bool SDL_RenderPresent(SDL_Renderer* renderer) { (void)renderer; return false; }
bool SDL_ConvertEventToRenderCoordinates(SDL_Renderer* renderer, SDL_Event* event) {
    (void)renderer;
    (void)event;
    return true;
}
