// SDL 3 gamepads for the Switch shim, over libnx HID, with HD rumble. The console's main
// controller (handheld Joy-Cons or player 1: a Pro Controller or paired
// Joy-Cons) is one SDL gamepad, instance 1. The event pump samples it on the
// thread that polls events; the getters read that sample, so any thread may
// call them.
//
// Buttons map by label, so the game's on-screen prompts match the controller:
// Switch A is SDL "south", which Aurora's PAD maps to GameCube A, B to B, X to
// X and Y to Y. ZL/ZR are the analog-trigger axes (GameCube L/R) and R is the
// right shoulder (GameCube Z).
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "sdl3_shim.h"

#define INSTANCE_ID 1
#define SWITCH_PAD_VENDOR_ID 0x057e
#define PRODUCT_PRO_CONTROLLER 0x2009

struct SDL_Gamepad {
    int player_index;
    SDL_PropertiesID properties;
};

static struct SDL_Gamepad g_gamepad = {.player_index = 0};
static struct SDL_Gamepad g_virtual = {.player_index = -1};
static bool g_virtual_connected;
static SDL_VirtualJoystickDesc g_virtual_desc;
static char g_virtual_name[128];
static Sint16 g_virtual_axes[SDL_GAMEPAD_AXIS_COUNT];
static bool g_virtual_buttons[SDL_GAMEPAD_BUTTON_COUNT];
#define VIRTUAL_ID 0x1000
static bool g_initialized;
static bool g_connected;
static SDL_JoystickConnectionState g_connection=SDL_JOYSTICK_CONNECTION_UNKNOWN;
static bool g_open;
static PadState g_pad;
static Mutex g_lock;
static u64 g_buttons;
static HidAnalogStickState g_sticks[2];

// Rumble through HD rumble: handheld Joy-Cons, and player 1's paired
// Joy-Cons or Pro Controller. The pump stops it when its duration ends.
#define VIBRATION_TARGETS 3
static HidVibrationDeviceHandle g_vibration[VIBRATION_TARGETS][2];
static int g_vibration_count[VIBRATION_TARGETS];
static u64 g_rumble_until;  // system tick; 0 when not rumbling

// The native port's debug server (switch/native/source/cos_debug.cpp): its presses and sticks as if
// from the controller; while it runs the controller counts as connected even with none attached
// (docked, Joy-Cons off), or the game would ignore its presses. Weak: the translated port's build
// has no debug server.
__attribute__((weak)) int cos_switch_debug_input(uint64_t* buttons, int32_t sticks[4]);

static const struct {
    SDL_GamepadButton button;
    u64 mask;
} kButtons[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, HidNpadButton_A},
    {SDL_GAMEPAD_BUTTON_EAST, HidNpadButton_B},
    {SDL_GAMEPAD_BUTTON_WEST, HidNpadButton_X},
    {SDL_GAMEPAD_BUTTON_NORTH, HidNpadButton_Y},
    {SDL_GAMEPAD_BUTTON_BACK, HidNpadButton_Minus},
    {SDL_GAMEPAD_BUTTON_START, HidNpadButton_Plus},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, HidNpadButton_StickL},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK, HidNpadButton_StickR},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, HidNpadButton_L},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, HidNpadButton_R},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, HidNpadButton_Up},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, HidNpadButton_Down},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, HidNpadButton_Left},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, HidNpadButton_Right},
};

void sdl3_shim_gamepad_init(void) {
    if (g_initialized)
        return;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);
    static const struct {
        HidNpadIdType id;
        HidNpadStyleTag style;
        int handles;
    } kTargets[VIBRATION_TARGETS] = {
        {HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld, 2},
        {HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual, 2},
        {HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey, 1},
    };
    bool rumble = false;
    for (int i = 0; i < VIBRATION_TARGETS; ++i) {
        if (R_SUCCEEDED(hidInitializeVibrationDevices(g_vibration[i], kTargets[i].handles,
                                                      kTargets[i].id, kTargets[i].style))) {
            g_vibration_count[i] = kTargets[i].handles;
            rumble = true;
        }
    }
    g_gamepad.properties = SDL_CreateProperties();
    SDL_SetBooleanProperty(g_gamepad.properties, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, rumble);
    SDL_SetBooleanProperty(g_gamepad.properties, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
    g_initialized = true;
}

static void queue_device_event(Uint32 type) {
    SDL_Event event;
    memset(&event, 0, sizeof event);
    event.type = type;
    event.gdevice.type = type;
    event.gdevice.timestamp = SDL_GetTicks() * 1000000ULL;
    event.gdevice.which = INSTANCE_ID;
    sdl3_shim_queue_event(&event);
}

// Low band carries the low-frequency motor, high band the high-frequency one,
// at the frequencies the console's own rumble defaults to.
static void send_rumble(float low, float high) {
    HidVibrationValue values[2];
    for (int i = 0; i < 2; ++i) {
        values[i].amp_low = low;
        values[i].freq_low = 160.0f;
        values[i].amp_high = high;
        values[i].freq_high = 320.0f;
    }
    for (int i = 0; i < VIBRATION_TARGETS; ++i) {
        if (g_vibration_count[i] > 0)
            hidSendVibrationValues(g_vibration[i], values, g_vibration_count[i]);
    }
}

void sdl3_shim_gamepad_pump(void) {
    // SDL may keep draining events during shutdown. Do not enter padUpdate after libnx closes HID.
    if (!g_initialized || !serviceIsActive(hidGetServiceSession()) || !SDL_WasInit(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK))
        return;
    mutexLock(&g_lock);
    if (g_rumble_until != 0 && armGetSystemTick() >= g_rumble_until) {
        g_rumble_until = 0;
        send_rumble(0.0f, 0.0f);
    }
    mutexUnlock(&g_lock);
    padUpdate(&g_pad);
    const bool attached = padIsConnected(&g_pad);
    bool connected = attached;
    mutexLock(&g_lock);
    g_buttons = attached ? padGetButtons(&g_pad) : 0;
    // Store alongside the HID sample so getters do not query HID from another thread.
    g_connection = !attached ? SDL_JOYSTICK_CONNECTION_UNKNOWN :
        (padIsHandheld(&g_pad) || (g_pad.attributes & HidNpadAttribute_IsWired)) ?
        SDL_JOYSTICK_CONNECTION_WIRED : SDL_JOYSTICK_CONNECTION_WIRELESS;
    g_sticks[0] = padGetStickPos(&g_pad, 0);
    g_sticks[1] = padGetStickPos(&g_pad, 1);
    if (cos_switch_debug_input) {
        int32_t sticks[4] = {g_sticks[0].x, g_sticks[0].y, g_sticks[1].x, g_sticks[1].y};
        if (cos_switch_debug_input(&g_buttons, sticks))
            connected = true;
        g_sticks[0].x = sticks[0], g_sticks[0].y = sticks[1];
        g_sticks[1].x = sticks[2], g_sticks[1].y = sticks[3];
    }
    mutexUnlock(&g_lock);
    static int s_attached = -1;
    // LMNX: Minus is only a controller button; the F1 menu opens with both sticks (L3 + R3, src/recomp/menu.cpp).
    if ((int)attached != s_attached) {
        s_attached = attached;
        fprintf(stderr, "[switch] controller %s (handheld or player 1)\n", attached ? "connected" : "not connected");
    }
    if (connected != g_connected) {
        g_connected = connected;
        queue_device_event(connected ? SDL_EVENT_GAMEPAD_ADDED : SDL_EVENT_GAMEPAD_REMOVED);
    }
    // Aurora input/sdl_input.cpp learns buttons and sticks from SDL events rather than state queries; send these
    // events so the detected controller responds.
    static u64 botones_previos;
    static Sint16 ejes_previos[SDL_GAMEPAD_AXIS_COUNT];
    const u64 botones = g_connected ? g_buttons : 0;
    for (size_t i = 0; i < sizeof kButtons / sizeof kButtons[0]; ++i) {
        const bool ahora = (botones & kButtons[i].mask) != 0, antes = (botones_previos & kButtons[i].mask) != 0;
        if (ahora == antes)
            continue;
        SDL_Event e;
        memset(&e, 0, sizeof e);
        e.type = ahora ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
        e.gbutton.timestamp = SDL_GetTicks() * 1000000ULL;
        e.gbutton.which = INSTANCE_ID;
        e.gbutton.button = (Uint8)kButtons[i].button;
        e.gbutton.down = ahora;
        sdl3_shim_queue_event(&e);
    }
    botones_previos = botones;
    for (int eje = 0; eje < SDL_GAMEPAD_AXIS_COUNT; ++eje) {
        const Sint16 valor = g_connected ? SDL_GetGamepadAxis(&g_gamepad, (SDL_GamepadAxis)eje) : 0;
        if (valor == ejes_previos[eje])
            continue;
        ejes_previos[eje] = valor;
        SDL_Event e;
        memset(&e, 0, sizeof e);
        e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        e.gaxis.timestamp = SDL_GetTicks() * 1000000ULL;
        e.gaxis.which = INSTANCE_ID;
        e.gaxis.axis = (Uint8)eje;
        e.gaxis.value = valor;
        sdl3_shim_queue_event(&e);
    }
}

SDL_Gamepad* SDL_OpenGamepad(SDL_JoystickID instance_id) {
    if(instance_id==VIRTUAL_ID && g_virtual_connected)return &g_virtual;
    if (instance_id != INSTANCE_ID || !g_connected) {
        sdl3_shim_set_error("no such gamepad");
        return NULL;
    }
    g_open = true;
    return &g_gamepad;
}
SDL_Gamepad* SDL_GetGamepadFromID(SDL_JoystickID id) {return id==VIRTUAL_ID && g_virtual_connected?&g_virtual:id==INSTANCE_ID && g_connected ? &g_gamepad : NULL;}
SDL_JoystickID* SDL_GetGamepads(int* count) {
    int n=(g_connected?1:0)+(g_virtual_connected?1:0);SDL_JoystickID* ids=calloc(n+1,sizeof *ids);
    if(ids){int k=0;if(g_connected)ids[k++]=INSTANCE_ID;if(g_virtual_connected)ids[k]=VIRTUAL_ID;}if(count)*count=ids?n:0;return ids;
}
SDL_JoystickID* SDL_GetJoysticks(int* count) {return SDL_GetGamepads(count);}
bool SDL_IsGamepad(SDL_JoystickID id) {return (id==INSTANCE_ID && g_connected)||(id==VIRTUAL_ID && g_virtual_connected);}
SDL_Joystick* SDL_OpenJoystick(SDL_JoystickID id) {return (SDL_Joystick*)SDL_OpenGamepad(id);}
void SDL_CloseJoystick(SDL_Joystick* j) {SDL_CloseGamepad((SDL_Gamepad*)j);}
const char* SDL_GetGamepadNameForID(SDL_JoystickID id) {return id==VIRTUAL_ID && g_virtual_connected?g_virtual_name:SDL_IsGamepad(id)?"Switch Controller":NULL;}
const char* SDL_GetJoystickNameForID(SDL_JoystickID id) {return SDL_GetGamepadNameForID(id);}
Uint16 SDL_GetJoystickVendorForID(SDL_JoystickID id) {return id==VIRTUAL_ID && g_virtual_connected?g_virtual_desc.vendor_id:SDL_IsGamepad(id)?SWITCH_PAD_VENDOR_ID:0;}
Uint16 SDL_GetJoystickProductForID(SDL_JoystickID id) {return id==VIRTUAL_ID && g_virtual_connected?g_virtual_desc.product_id:SDL_IsGamepad(id)?PRODUCT_PRO_CONTROLLER:0;}
SDL_GamepadType SDL_GetGamepadTypeForID(SDL_JoystickID id) {return id==VIRTUAL_ID && g_virtual_connected?SDL_GAMEPAD_TYPE_STANDARD:SDL_IsGamepad(id)?SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:SDL_GAMEPAD_TYPE_UNKNOWN;}
char* SDL_GetGamepadMapping(SDL_Gamepad* gamepad) {
    if(gamepad!=&g_gamepad)return NULL;
    return SDL_strdup("000000007e0500000920000000000000,Switch Controller,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");
}
SDL_GamepadButtonLabel SDL_GetGamepadButtonLabelForType(SDL_GamepadType type,SDL_GamepadButton b) {
    (void)type;
    switch(b){case SDL_GAMEPAD_BUTTON_SOUTH:return SDL_GAMEPAD_BUTTON_LABEL_A;case SDL_GAMEPAD_BUTTON_EAST:return SDL_GAMEPAD_BUTTON_LABEL_B;case SDL_GAMEPAD_BUTTON_WEST:return SDL_GAMEPAD_BUTTON_LABEL_X;case SDL_GAMEPAD_BUTTON_NORTH:return SDL_GAMEPAD_BUTTON_LABEL_Y;default:return SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;}
}

void SDL_CloseGamepad(SDL_Gamepad* gamepad) {
    if (gamepad == &g_gamepad)
        g_open = false;
}

bool SDL_GetGamepadButton(SDL_Gamepad* gamepad, SDL_GamepadButton button) {
    if(gamepad==&g_virtual && button>=0 && button<SDL_GAMEPAD_BUTTON_COUNT){mutexLock(&g_lock);bool v=g_virtual_buttons[button];mutexUnlock(&g_lock);return v;}
    if (gamepad != &g_gamepad)
        return false;
    mutexLock(&g_lock);
    const u64 buttons = g_buttons;
    mutexUnlock(&g_lock);
    for (size_t i = 0; i < sizeof kButtons / sizeof kButtons[0]; ++i) {
        if (kButtons[i].button == button)
            return (buttons & kButtons[i].mask) != 0;
    }
    return false;
}

static Sint16 stick_axis(s32 value, bool invert) {
    // libnx reports -32767..32767 with up positive; SDL has down positive.
    if (invert)
        value = -value;
    if (value > SDL_JOYSTICK_AXIS_MAX)
        value = SDL_JOYSTICK_AXIS_MAX;
    if (value < SDL_JOYSTICK_AXIS_MIN)
        value = SDL_JOYSTICK_AXIS_MIN;
    return (Sint16)value;
}

Sint16 SDL_GetGamepadAxis(SDL_Gamepad* gamepad, SDL_GamepadAxis axis) {
    if(gamepad==&g_virtual && axis>=0 && axis<SDL_GAMEPAD_AXIS_COUNT){mutexLock(&g_lock);Sint16 v=g_virtual_axes[axis];mutexUnlock(&g_lock);
        return axis==SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis==SDL_GAMEPAD_AXIS_RIGHT_TRIGGER?(Sint16)(((int)v+32768)/2):v;}
    if (gamepad != &g_gamepad)
        return 0;
    mutexLock(&g_lock);
    const u64 buttons = g_buttons;
    const HidAnalogStickState left = g_sticks[0];
    const HidAnalogStickState right = g_sticks[1];
    mutexUnlock(&g_lock);
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX: return stick_axis(left.x, false);
    case SDL_GAMEPAD_AXIS_LEFTY: return stick_axis(left.y, true);
    case SDL_GAMEPAD_AXIS_RIGHTX: return stick_axis(right.x, false);
    case SDL_GAMEPAD_AXIS_RIGHTY: return stick_axis(right.y, true);
    // ZL and ZR are digital; report them fully pressed or released.
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        return (buttons & HidNpadButton_ZL) != 0 ? SDL_JOYSTICK_AXIS_MAX : 0;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        return (buttons & HidNpadButton_ZR) != 0 ? SDL_JOYSTICK_AXIS_MAX : 0;
    default: return 0;
    }
}

SDL_JoystickID SDL_GetGamepadID(SDL_Gamepad* gamepad) {
    if(gamepad==&g_virtual)return VIRTUAL_ID;
    return gamepad == &g_gamepad ? INSTANCE_ID : 0;
}

// The gamepad is its own joystick handle.
SDL_Joystick* SDL_GetGamepadJoystick(SDL_Gamepad* gamepad) { return (SDL_Joystick*)gamepad; }

SDL_JoystickID SDL_GetJoystickID(SDL_Joystick* joystick) {
    if((void*)joystick==(void*)&g_virtual)return VIRTUAL_ID;
    return (void*)joystick == (void*)&g_gamepad ? INSTANCE_ID : 0;
}

const char* SDL_GetGamepadName(SDL_Gamepad* gamepad) {
    if(gamepad==&g_virtual)return g_virtual_name;
    return gamepad == &g_gamepad ? "Switch Controller" : NULL;
}
SDL_JoystickConnectionState SDL_GetGamepadConnectionState(SDL_Gamepad* gamepad) {
    if(gamepad==&g_virtual && g_virtual_connected)return SDL_JOYSTICK_CONNECTION_UNKNOWN;
    mutexLock(&g_lock);
    const bool valido=gamepad==&g_gamepad && g_connected;
    const SDL_JoystickConnectionState estado=g_connection;
    mutexUnlock(&g_lock);
    if(valido)return estado;
    SDL_SetError("Mando Switch no conectado");return SDL_JOYSTICK_CONNECTION_INVALID;
}
const char* SDL_GetGamepadPath(SDL_Gamepad* gamepad) {
    if(gamepad==&g_virtual && g_virtual_connected)return "libnx:virtual";
    mutexLock(&g_lock);const bool valido=gamepad==&g_gamepad && g_connected;mutexUnlock(&g_lock);
    if(valido)return "libnx:hid:npad:principal";
    SDL_SetError("Mando Switch no conectado");return NULL;
}

Uint16 SDL_GetGamepadVendor(SDL_Gamepad* gamepad) {
    if(gamepad==&g_virtual)return g_virtual_desc.vendor_id;
    return gamepad == &g_gamepad ? SWITCH_PAD_VENDOR_ID : 0;
}

Uint16 SDL_GetGamepadProduct(SDL_Gamepad* gamepad) {
    if(gamepad==&g_virtual)return g_virtual_desc.product_id;
    return gamepad == &g_gamepad ? PRODUCT_PRO_CONTROLLER : 0;
}

SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad* gamepad) {
    if(gamepad==&g_virtual)return SDL_GAMEPAD_TYPE_STANDARD;
    return gamepad == &g_gamepad ? SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO : SDL_GAMEPAD_TYPE_UNKNOWN;
}

SDL_PropertiesID SDL_GetGamepadProperties(SDL_Gamepad* gamepad) {
    if(gamepad==&g_virtual)return g_virtual.properties;
    return gamepad == &g_gamepad ? g_gamepad.properties : 0;
}

int SDL_GetGamepadPlayerIndex(SDL_Gamepad* gamepad) {
    if(gamepad==&g_virtual)return g_virtual.player_index;
    return gamepad == &g_gamepad ? g_gamepad.player_index : -1;
}

bool SDL_SetGamepadPlayerIndex(SDL_Gamepad* gamepad, int player_index) {
    if(gamepad==&g_virtual){g_virtual.player_index=player_index;if(g_virtual_desc.SetPlayerIndex)g_virtual_desc.SetPlayerIndex(g_virtual_desc.userdata,player_index);return true;}
    if (gamepad != &g_gamepad)
        return false;
    g_gamepad.player_index = player_index;
    return true;
}

const char* SDL_GetGamepadSerial(SDL_Gamepad* gamepad) {
    (void)gamepad;
    return NULL;
}

SDL_PowerState SDL_GetGamepadPowerInfo(SDL_Gamepad* gamepad, int* percent) {
    (void)gamepad;
    if (percent != NULL)
        *percent = -1;
    return SDL_POWERSTATE_UNKNOWN;
}

SDL_PowerState SDL_GetJoystickPowerInfo(SDL_Joystick* joystick, int* percent) {
    return SDL_GetGamepadPowerInfo((SDL_Gamepad*)joystick, percent);
}

// SDL's GUID layout: bus, CRC, vendor, 0, product, 0, version, driver bytes.
SDL_GUID SDL_GetGamepadGUIDForID(SDL_JoystickID instance_id) {
    SDL_GUID guid;
    memset(&guid, 0, sizeof guid);
    if (instance_id == INSTANCE_ID) {
        guid.data[4] = SWITCH_PAD_VENDOR_ID & 0xff;
        guid.data[5] = SWITCH_PAD_VENDOR_ID >> 8;
        guid.data[8] = PRODUCT_PRO_CONTROLLER & 0xff;
        guid.data[9] = PRODUCT_PRO_CONTROLLER >> 8;
    }
    return guid;
}

void SDL_GetJoystickGUIDInfo(SDL_GUID guid, Uint16* vendor, Uint16* product, Uint16* version,
                             Uint16* crc16) {
    if (vendor != NULL)
        *vendor = (Uint16)(guid.data[4] | guid.data[5] << 8);
    if (product != NULL)
        *product = (Uint16)(guid.data[8] | guid.data[9] << 8);
    if (version != NULL)
        *version = (Uint16)(guid.data[12] | guid.data[13] << 8);
    if (crc16 != NULL)
        *crc16 = (Uint16)(guid.data[2] | guid.data[3] << 8);
}

const char* SDL_GetGamepadStringForButton(SDL_GamepadButton button) {
    static const char* const kNames[SDL_GAMEPAD_BUTTON_COUNT] = {
        [SDL_GAMEPAD_BUTTON_SOUTH] = "a",
        [SDL_GAMEPAD_BUTTON_EAST] = "b",
        [SDL_GAMEPAD_BUTTON_WEST] = "x",
        [SDL_GAMEPAD_BUTTON_NORTH] = "y",
        [SDL_GAMEPAD_BUTTON_BACK] = "back",
        [SDL_GAMEPAD_BUTTON_GUIDE] = "guide",
        [SDL_GAMEPAD_BUTTON_START] = "start",
        [SDL_GAMEPAD_BUTTON_LEFT_STICK] = "leftstick",
        [SDL_GAMEPAD_BUTTON_RIGHT_STICK] = "rightstick",
        [SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = "leftshoulder",
        [SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = "rightshoulder",
        [SDL_GAMEPAD_BUTTON_DPAD_UP] = "dpup",
        [SDL_GAMEPAD_BUTTON_DPAD_DOWN] = "dpdown",
        [SDL_GAMEPAD_BUTTON_DPAD_LEFT] = "dpleft",
        [SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = "dpright",
        [SDL_GAMEPAD_BUTTON_MISC1] = "misc1",
    };
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return NULL;
    return kNames[button];
}

const char* SDL_GetGamepadStringForAxis(SDL_GamepadAxis axis) {
    static const char* const kNames[SDL_GAMEPAD_AXIS_COUNT] = {
        [SDL_GAMEPAD_AXIS_LEFTX] = "leftx",
        [SDL_GAMEPAD_AXIS_LEFTY] = "lefty",
        [SDL_GAMEPAD_AXIS_RIGHTX] = "rightx",
        [SDL_GAMEPAD_AXIS_RIGHTY] = "righty",
        [SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = "lefttrigger",
        [SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = "righttrigger",
    };
    if (axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT)
        return NULL;
    return kNames[axis];
}

bool SDL_RumbleGamepad(SDL_Gamepad* gamepad, Uint16 low_frequency_rumble,
                       Uint16 high_frequency_rumble, Uint32 duration_ms) {
    if(gamepad==&g_virtual)return g_virtual_desc.Rumble && g_virtual_desc.Rumble(g_virtual_desc.userdata,low_frequency_rumble,high_frequency_rumble);
    if (gamepad != &g_gamepad)
        return false;
    mutexLock(&g_lock);
    // As in SDL, zero intensity stops and a zero duration rumbles until the
    // next call (Aurora's PAD_MOTOR_RUMBLE asks for exactly that).
    send_rumble(low_frequency_rumble / 65535.0f, high_frequency_rumble / 65535.0f);
    const bool off = low_frequency_rumble == 0 && high_frequency_rumble == 0;
    g_rumble_until = off || duration_ms == 0
                         ? 0
                         : armGetSystemTick() + armNsToTicks((u64)duration_ms * 1000000ULL);
    mutexUnlock(&g_lock);
    return true;
}

// No LEDs or motion sensors yet.

bool SDL_SetGamepadLED(SDL_Gamepad* gamepad, Uint8 red, Uint8 green, Uint8 blue) {
    (void)gamepad;
    (void)red;
    (void)green;
    (void)blue;
    return false;
}

bool SDL_GamepadHasSensor(SDL_Gamepad* gamepad, SDL_SensorType type) {
    (void)gamepad;
    (void)type;
    return false;
}

bool SDL_SetGamepadSensorEnabled(SDL_Gamepad* gamepad, SDL_SensorType type, bool enabled) {
    (void)gamepad;
    (void)type;
    (void)enabled;
    return false;
}

bool SDL_GetGamepadSensorData(SDL_Gamepad* gamepad, SDL_SensorType type, float* data,
                              int num_values) {
    (void)gamepad;
    (void)type;
    (void)data;
    (void)num_values;
    return false;
}
static void virtual_event(Uint32 type,int index,int value) {
    SDL_Event e;memset(&e,0,sizeof e);e.type=type;e.common.timestamp=SDL_GetTicksNS();
    if(type==SDL_EVENT_GAMEPAD_AXIS_MOTION){e.gaxis.which=VIRTUAL_ID;e.gaxis.axis=index;e.gaxis.value=value;}
    else if(type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || type==SDL_EVENT_GAMEPAD_BUTTON_UP){e.gbutton.which=VIRTUAL_ID;e.gbutton.button=index;e.gbutton.down=value!=0;}
    else e.gdevice.which=VIRTUAL_ID;sdl3_shim_queue_event(&e);
}
SDL_JoystickID SDL_AttachVirtualJoystick(const SDL_VirtualJoystickDesc* desc) {
    if(!desc || desc->version!=sizeof *desc || desc->type!=SDL_JOYSTICK_TYPE_GAMEPAD || desc->naxes>SDL_GAMEPAD_AXIS_COUNT || desc->nbuttons>SDL_GAMEPAD_BUTTON_COUNT || desc->nhats || desc->nballs || desc->ntouchpads || desc->nsensors || g_virtual_connected){SDL_SetError("Descriptor de mando virtual no admitido");return 0;}
    g_virtual_desc=*desc;snprintf(g_virtual_name,sizeof g_virtual_name,"%s",desc->name?desc->name:"Virtual gamepad");g_virtual_desc.name=g_virtual_name;
    memset(g_virtual_axes,0,sizeof g_virtual_axes);memset(g_virtual_buttons,0,sizeof g_virtual_buttons);g_virtual_connected=true;
    g_virtual_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=g_virtual_axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=SDL_JOYSTICK_AXIS_MIN;
    g_virtual.properties=SDL_CreateProperties();SDL_SetBooleanProperty(g_virtual.properties,SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN,desc->Rumble!=NULL);
    virtual_event(SDL_EVENT_GAMEPAD_ADDED,0,0);return VIRTUAL_ID;
}
bool SDL_DetachVirtualJoystick(SDL_JoystickID id) {
    if(id!=VIRTUAL_ID || !g_virtual_connected)return SDL_SetError("Mando virtual no conectado");g_virtual_connected=false;
    virtual_event(SDL_EVENT_GAMEPAD_REMOVED,0,0);if(g_virtual_desc.Cleanup)g_virtual_desc.Cleanup(g_virtual_desc.userdata);SDL_DestroyProperties(g_virtual.properties);return true;
}
bool SDL_SetJoystickVirtualAxis(SDL_Joystick* j,int axis,Sint16 value) {
    if((void*)j!=(void*)&g_virtual || !g_virtual_connected || axis<0 || axis>=g_virtual_desc.naxes)return SDL_SetError("Invalid virtual axis");
    mutexLock(&g_lock);g_virtual_axes[axis]=value;mutexUnlock(&g_lock);virtual_event(SDL_EVENT_GAMEPAD_AXIS_MOTION,axis,value);return true;
}
bool SDL_SetJoystickVirtualButton(SDL_Joystick* j,int button,bool down) {
    if((void*)j!=(void*)&g_virtual || !g_virtual_connected || button<0 || button>=g_virtual_desc.nbuttons)return SDL_SetError("Invalid virtual button");
    mutexLock(&g_lock);g_virtual_buttons[button]=down;mutexUnlock(&g_lock);virtual_event(down?SDL_EVENT_GAMEPAD_BUTTON_DOWN:SDL_EVENT_GAMEPAD_BUTTON_UP,button,down);return true;
}
