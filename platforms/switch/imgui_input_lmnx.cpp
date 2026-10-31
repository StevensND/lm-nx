// ImGui input for a Switch window: SDL events and native HID controllers.
#include <switch.h>
#include "imgui_lmnx.hpp"
#include <algorithm>
#include <cfloat>
namespace lmnx::imgui {
namespace {
uint64_t anterior=0;
// Use touch as an ImGui mouse (ImGuiMouseSource_TouchScreen) to move and resize the F1 menu and performance
// panel. Scale the touch panel's 1280x720 coordinates to ImGui's size.
bool tactilIniciado=false, tocando=false, recienSoltado=false;
void entradaTactil(ImGuiIO& io) {
    if(!tactilIniciado){hidInitializeTouchScreen();tactilIniciado=true;}
    HidTouchScreenState estado{};
    const bool hay=hidGetTouchScreenStates(&estado,1)>0 && estado.count>0;
    if(hay) {
        const float x=float(estado.touches[0].x)*io.DisplaySize.x/1280.f, y=float(estado.touches[0].y)*io.DisplaySize.y/720.f;
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);io.AddMousePosEvent(x,y);
        if(!tocando){io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);io.AddMouseButtonEvent(0,true);tocando=true;}
    } else if(tocando) {
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);io.AddMouseButtonEvent(0,false);tocando=false;recienSoltado=true;
    } else if(recienSoltado) {
        io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);recienSoltado=false;  // Without a finger, nothing stays under the mouse.
    }
}
ImGuiKey tecla(SDL_Scancode s) {
    if(s>=SDL_SCANCODE_A && s<=SDL_SCANCODE_Z)return ImGuiKey(ImGuiKey_A+s-SDL_SCANCODE_A);
    if(s>=SDL_SCANCODE_1 && s<=SDL_SCANCODE_9)return ImGuiKey(ImGuiKey_1+s-SDL_SCANCODE_1);
    if(s>=SDL_SCANCODE_F1 && s<=SDL_SCANCODE_F12)return ImGuiKey(ImGuiKey_F1+s-SDL_SCANCODE_F1);
    switch(s) {
    case SDL_SCANCODE_0:return ImGuiKey_0;case SDL_SCANCODE_RETURN:return ImGuiKey_Enter;case SDL_SCANCODE_ESCAPE:return ImGuiKey_Escape;
    case SDL_SCANCODE_TAB:return ImGuiKey_Tab;case SDL_SCANCODE_BACKSPACE:return ImGuiKey_Backspace;case SDL_SCANCODE_SPACE:return ImGuiKey_Space;
    case SDL_SCANCODE_LEFT:return ImGuiKey_LeftArrow;case SDL_SCANCODE_RIGHT:return ImGuiKey_RightArrow;case SDL_SCANCODE_UP:return ImGuiKey_UpArrow;case SDL_SCANCODE_DOWN:return ImGuiKey_DownArrow;
    default:return ImGuiKey_None;
    }
}
}
void iniciarEntrada(SDL_Window*) {
    auto& io=ImGui::GetIO();io.BackendPlatformName="LMNX-HID";io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags&=~ImGuiConfigFlags_ViewportsEnable;anterior=SDL_GetTicksNS();
}
void cerrarEntrada() {auto& io=ImGui::GetIO();io.BackendPlatformName=nullptr;io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;}
void procesarEvento(const SDL_Event* e) {
    if(!e)return;auto& io=ImGui::GetIO();
    switch(e->type) {
    case SDL_EVENT_KEY_DOWN:case SDL_EVENT_KEY_UP:{auto key=tecla(e->key.scancode);if(key!=ImGuiKey_None)io.AddKeyEvent(key,e->key.down);break;}
    case SDL_EVENT_TEXT_INPUT:if(e->text.text)io.AddInputCharactersUTF8(e->text.text);break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:io.AddFocusEvent(true);break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:io.AddFocusEvent(false);break;
    default:break;
    }
}
void nuevaEntrada() {
    auto& io=ImGui::GetIO();auto ahora=SDL_GetTicksNS();io.DeltaTime=anterior && ahora>anterior?float(double(ahora-anterior)/1e9):1.f/60.f;anterior=ahora;
    auto* pad=SDL_GetGamepadFromID(1);
    if(pad)io.BackendFlags|=ImGuiBackendFlags_HasGamepad;else io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;
    const struct {ImGuiKey key;SDL_GamepadButton button;} botones[]={
        {ImGuiKey_GamepadStart,SDL_GAMEPAD_BUTTON_START},{ImGuiKey_GamepadBack,SDL_GAMEPAD_BUTTON_BACK},
        {ImGuiKey_GamepadFaceDown,SDL_GAMEPAD_BUTTON_SOUTH},{ImGuiKey_GamepadFaceRight,SDL_GAMEPAD_BUTTON_EAST},
        {ImGuiKey_GamepadFaceLeft,SDL_GAMEPAD_BUTTON_WEST},{ImGuiKey_GamepadFaceUp,SDL_GAMEPAD_BUTTON_NORTH},
        {ImGuiKey_GamepadDpadLeft,SDL_GAMEPAD_BUTTON_DPAD_LEFT},{ImGuiKey_GamepadDpadRight,SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
        {ImGuiKey_GamepadDpadUp,SDL_GAMEPAD_BUTTON_DPAD_UP},{ImGuiKey_GamepadDpadDown,SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {ImGuiKey_GamepadL1,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},{ImGuiKey_GamepadR1,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER}};
    for(auto b:botones)io.AddKeyEvent(b.key,pad && SDL_GetGamepadButton(pad,b.button));
    auto eje=[&](SDL_GamepadAxis axis,ImGuiKey menos,ImGuiKey mas) {float v=pad?float(SDL_GetGamepadAxis(pad,axis))/32767.f:0.f;
        auto a=std::clamp((-v-0.2f)/0.8f,0.f,1.f),b=std::clamp((v-0.2f)/0.8f,0.f,1.f);io.AddKeyAnalogEvent(menos,a>0,a);io.AddKeyAnalogEvent(mas,b>0,b);};
    eje(SDL_GAMEPAD_AXIS_LEFTX,ImGuiKey_GamepadLStickLeft,ImGuiKey_GamepadLStickRight);eje(SDL_GAMEPAD_AXIS_LEFTY,ImGuiKey_GamepadLStickUp,ImGuiKey_GamepadLStickDown);
    entradaTactil(io);
}
}
