#include "audio_convert.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    int16_t out[2];
    float f[] = {1.f, -1.f, 0, 0, 0, 0, 0, 0};
    lmnx_audio_stereo(f, 2, 1, out); assert(out[0] == 32767 && out[1] == -32768);
    f[0] = NAN; f[1] = INFINITY;
    lmnx_audio_stereo(f, 2, 1, out); assert(out[0] == 0 && out[1] == 0);
    f[0] = 0.5f; lmnx_audio_stereo(f, 1, 1, out); assert(out[0] == 16384 && out[1] == 16384);
    const int16_t s[] = {1234, -2345};
    lmnx_audio_stereo(s, 2, 0, out); assert(out[0] == 1234 && out[1] == -2345);
    for (int i = 0; i < 8; ++i) f[i] = 0;
    f[2] = 1.f; lmnx_audio_stereo(f, 6, 1, out); assert(out[0] == 23170 && out[1] == 23170);
    f[2] = 0; f[6] = 0.5f;
    lmnx_audio_stereo(f, 8, 1, out); assert(out[0] == 11585 && out[1] == 0);
    puts("Audio: F32/S16, mono, center/surround, saturation and nonfinite values correct; audout not run.");
}
