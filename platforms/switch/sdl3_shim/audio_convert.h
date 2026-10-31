#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
// Interleaved SDL input: FL, FR, C, LFE, BL, BR, SL, SR. Physical output is stereo.
static inline void lmnx_audio_stereo(const void* frame, int canales, int es_float, int16_t out[2]) {
    float v[8] = {0};
    for (int c = 0; c < canales; ++c) {
        if (es_float) memcpy(&v[c], (const char*)frame + c * sizeof(float), sizeof(float));
        else { int16_t s; memcpy(&s, (const char*)frame + c * sizeof(s), sizeof(s)); v[c] = s / 32768.f; }
        if (!isfinite(v[c])) v[c] = 0;
    }
    float l = v[0], r = canales == 1 ? v[0] : v[1];
    if (canales >= 6) {
        l += 0.70710678f * (v[2] + v[4]) + 0.5f * v[3];
        r += 0.70710678f * (v[2] + v[5]) + 0.5f * v[3];
    }
    if (canales == 8) { l += 0.70710678f * v[6]; r += 0.70710678f * v[7]; }
    out[0] = (int16_t)lrintf(fminf(32767.f, fmaxf(-32768.f, l * 32768.f)));
    out[1] = (int16_t)lrintf(fminf(32767.f, fmaxf(-32768.f, r * 32768.f)));
}
