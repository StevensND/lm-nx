#pragma once
// Perfilado desactivado expresamente en Horizon. No define TRACY_ENABLE.
#ifdef TRACY_ENABLE
#error "LM-NX Switch is built without TRACY_ENABLE"
#endif
#define ZoneScoped
#define ZoneScopedN(...)
#define ZoneScopedS(...)
#define ZoneScopedC(...)
#define ZoneScopedNC(...)
#define ZoneText(...)
#define ZoneName(...)
#define FrameMark
#define FrameMarkNamed(...)
#define FrameMarkStart(...)
#define FrameMarkEnd(...)
#define TracyPlot(...)
#define TracyPlotConfig(...)
#define TracyMessage(...)
#define TracyMessageL(...)
#define TracyIsConnected false
namespace tracy {
inline void SetThreadName(const char*) noexcept {}
}
