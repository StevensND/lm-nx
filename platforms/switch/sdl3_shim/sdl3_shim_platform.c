// Actual platform services. Desktop-only utilities return explicit errors without creating windows or graphics
// contexts.
#include <SDL3/SDL.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdl3_shim.h"
bool SDL_SavePNG(SDL_Surface* surface,const char* path) {
    if(!surface || surface->format!=SDL_PIXELFORMAT_RGBA32)return SDL_SetError("PNG requiere superficie RGBA32");
    png_image image;memset(&image,0,sizeof image);image.version=PNG_IMAGE_VERSION;image.width=surface->w;image.height=surface->h;image.format=PNG_FORMAT_RGBA;
    bool ok=png_image_write_to_file(&image,path,0,surface->pixels,surface->pitch,NULL)!=0;
    if(!ok)SDL_SetError("PNG: %s",image.message);png_image_free(&image);return ok;
}
bool SDL_OpenURL(const char* url) {fprintf(stderr,"[Switch] Link: %s\n",url?url:"");return SDL_SetError("No desktop browser in LM-NX");}
bool SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags flags,const char* title,const char* message,SDL_Window* window) {
    (void)flags;(void)window;fprintf(stderr,"%s: %s\n",title?title:"",message?message:"");return SDL_SetError("Message sent to log; no desktop SDL dialog");
}
bool SDL_ShowMessageBox(const SDL_MessageBoxData* data,int* buttonid) {
    if(buttonid)*buttonid=-1;
    fprintf(stderr,"[Switch] %s: %s\n",data && data->title?data->title:"Recuperacion",data && data->message?data->message:"");
    return SDL_SetError("Recovery logged; no desktop SDL dialog on Switch");
}
bool SDL_SetWindowAlwaysOnTop(SDL_Window* w,bool on) {(void)w;(void)on;return SDL_SetError("Switch has only one window");}
bool SDL_MaximizeWindow(SDL_Window* w) {return w!=NULL;}
SDL_Keymod SDL_GetModState(void) {return SDL_KMOD_NONE;}
const char* SDL_GetScancodeName(SDL_Scancode s) {
    static const char* letras[]={"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z"};
    if(s>=SDL_SCANCODE_A && s<=SDL_SCANCODE_Z)return letras[s-SDL_SCANCODE_A];
    switch(s){case SDL_SCANCODE_RETURN:return "Return";case SDL_SCANCODE_ESCAPE:return "Escape";case SDL_SCANCODE_SPACE:return "Space";case SDL_SCANCODE_F1:return "F1";case SDL_SCANCODE_UP:return "Up";case SDL_SCANCODE_DOWN:return "Down";case SDL_SCANCODE_LEFT:return "Left";case SDL_SCANCODE_RIGHT:return "Right";default:return "";}
}
SDL_Scancode SDL_GetScancodeFromName(const char* name) {if(!name)return SDL_SCANCODE_UNKNOWN;for(int i=1;i<SDL_SCANCODE_COUNT;++i){const char* n=SDL_GetScancodeName((SDL_Scancode)i);if(*n && strcasecmp(n,name)==0)return (SDL_Scancode)i;}return SDL_SCANCODE_UNKNOWN;}
