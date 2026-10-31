// SDL3/libnx transport: S16 or F32, including LM's 32028 Hz rate. Convert mono/stereo/5.1/7.1 to audout's
// physical 48 kHz S16 stereo. The bounded queue stores converted stereo frames; byte queries express their
// duration in the input format. Format changes clear the queue. The worker plays silence while paused or out of
// samples.
#include <stdio.h>
#include <SDL3/SDL.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <math.h>

#include "sdl3_shim.h"
#include "audio_convert.h"

#define OUTPUT_RATE 48000
#define CHANNELS 2
#define BUFFER_COUNT 4
#define BUFFER_FRAMES 1024                     // 21.3 ms at 48 kHz
#define BUFFER_BYTES (BUFFER_FRAMES * CHANNELS * sizeof(s16))  // 0x1000, as audout requires
#define QUEUE_FRAMES 48000                     // one second at the highest input rate

struct SDL_AudioStream {
    Mutex lock;
    s16* queue;  // QUEUE_FRAMES interleaved stereo frames
    size_t head;
    size_t count;
    int input_rate;
    int input_channels;
    SDL_AudioFormat input_format;
    double position;  // fraction of the way from the queue's first frame to its second
    bool playing;
    bool stop;
    Thread thread;
    AudioOutBuffer buffers[BUFFER_COUNT];
    void* memory[BUFFER_COUNT];
};
static SDL_AudioStream* g_stream;
static Mutex g_device_lock;

static bool valid_input(const SDL_AudioSpec* spec) {
    return spec != NULL && (spec->format == SDL_AUDIO_S16 || spec->format == SDL_AUDIO_F32) &&
           (spec->channels == 1 || spec->channels == 2 || spec->channels == 6 || spec->channels == 8) &&
           spec->freq >= 8000 && spec->freq <= 96000;
}

static const s16* frame_at(const SDL_AudioStream* stream, size_t index) {
    return &stream->queue[((stream->head + index) % QUEUE_FRAMES) * CHANNELS];
}

// Fills one output buffer at 48 kHz from the queue. Caller holds the lock. Returns the frames filled with
// silence because the queue ran dry while playing (an audible gap: a click or crackle).
static size_t render(SDL_AudioStream* stream, s16* out) {
    const double step = (double)stream->input_rate / OUTPUT_RATE;
    size_t dry = 0;
    for (size_t frame = 0; frame < BUFFER_FRAMES; ++frame) {
        if (!stream->playing || stream->count < 2) {
            out[frame * CHANNELS] = out[frame * CHANNELS + 1] = 0;
            if (stream->playing) ++dry;
            continue;
        }
        const s16* a = frame_at(stream, 0);
        const s16* b = frame_at(stream, 1);
        const double t = stream->position;
        for (int channel = 0; channel < CHANNELS; ++channel)
            out[frame * CHANNELS + channel] = (s16)(a[channel] + (b[channel] - a[channel]) * t);
        stream->position += step;
        while (stream->position >= 1.0 && stream->count >= 2) {
            stream->position -= 1.0;
            stream->head = (stream->head + 1) % QUEUE_FRAMES;
            --stream->count;
        }
    }
    return dry;
}

// platforms/switch/perfil.cpp performance panel; weak because some test ELFs omit it.
#ifdef __cplusplus
extern "C"
#endif
__attribute__((weak)) void lmnx_perfil_nombrar_hilo(const char* nombre);

static void output_thread(void* argument) {
    if (lmnx_perfil_nombrar_hilo) lmnx_perfil_nombrar_hilo("audio out");
    u64 permitidos = 0;
    if (R_SUCCEEDED(svcGetInfo(&permitidos, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0))) {
        const u64 mascara = permitidos & 7;
        if (mascara) svcSetThreadCoreMask(CUR_THREAD_HANDLE, (mascara & 4) ? 2 : __builtin_ctzll(mascara), mascara);
    }
    SDL_AudioStream* stream = argument;
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        memset(stream->memory[i], 0, BUFFER_BYTES);
        audoutAppendAudioOutBuffer(&stream->buffers[i]);
    }
    // Gaps (queue empty while playing) over 5 s, logged only when there were any: they are what is heard as
    // crackling. The minimum queue level shows how close it came. One line per 5 s at most (asynchronous log).
    u64 inicio = armGetSystemTick();
    size_t huecos = 0, buferes_con_hueco = 0, cola_minima = (size_t)-1;
    while (!__atomic_load_n(&stream->stop, __ATOMIC_ACQUIRE)) {
        AudioOutBuffer* released = NULL;
        u32 count = 0;
        if (R_FAILED(audoutWaitPlayFinish(&released, &count, 100000000ULL)) || released == NULL)
            continue;
        mutexLock(&stream->lock);
        if (stream->playing && stream->count < cola_minima) cola_minima = stream->count;
        const size_t dry = render(stream, released->buffer);
        mutexUnlock(&stream->lock);
        audoutAppendAudioOutBuffer(released);
        huecos += dry;
        buferes_con_hueco += dry != 0;
        const u64 ahora = armGetSystemTick();
        if (armTicksToNs(ahora - inicio) >= 5000000000ULL) {
            if (huecos)
                printf("[audio] underrun: %zu samples in %zu buffers over 5 s; minimum queue %zu samples\n", huecos,
                       buferes_con_hueco, cola_minima);
            inicio = ahora;
            huecos = buferes_con_hueco = 0;
            cola_minima = (size_t)-1;
        }
    }
}

SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID devid, const SDL_AudioSpec* spec,
                                           SDL_AudioStreamCallback callback, void* userdata) {
    (void)userdata;
    if ((devid != SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK && devid != 1) || callback != NULL || !valid_input(spec)) {
        sdl3_shim_set_error("Requires S16/F32 output with 1, 2, 6 or 8 channels at 8-96 kHz");
        return NULL;
    }
    mutexLock(&g_device_lock);
    if (g_stream) {
        mutexUnlock(&g_device_lock);
        sdl3_shim_set_error("Audout ya pertenece a otro stream"); return NULL;
    }
    SDL_AudioStream* stream = calloc(1, sizeof *stream);
    if (stream == NULL) {
        mutexUnlock(&g_device_lock);
        return NULL;
    }
    stream->queue = calloc(QUEUE_FRAMES * CHANNELS, sizeof(s16));
    stream->input_rate = spec->freq;
    stream->input_channels = spec->channels;
    stream->input_format = spec->format;
    bool ready = stream->queue != NULL;
    for (int i = 0; ready && i < BUFFER_COUNT; ++i) {
        stream->memory[i] = memalign(0x1000, BUFFER_BYTES);
        ready = stream->memory[i] != NULL;
        if (ready) {
            stream->buffers[i].buffer = stream->memory[i];
            stream->buffers[i].buffer_size = BUFFER_BYTES;
            stream->buffers[i].data_size = BUFFER_BYTES;
            stream->buffers[i].data_offset = 0;
            stream->buffers[i].next = NULL;
        }
    }
    // Release every service and handle even after partial startup failure.
    bool servicio = false, salida = false, hilo = false;
    if (ready) {
        servicio = R_SUCCEEDED(audoutInitialize());
        salida = servicio && R_SUCCEEDED(audoutStartAudioOut());
        ready = salida;
        if (!ready) sdl3_shim_set_error("Could not initialize audout");
    }
    if (ready && (audoutGetSampleRate() != OUTPUT_RATE || audoutGetChannelCount() != CHANNELS)) {
        sdl3_shim_set_error("Audout lacks the 48 kHz stereo format");
        ready = false;
    }
    if (ready) {
        hilo = R_SUCCEEDED(threadCreate(&stream->thread, output_thread, stream, NULL, 0x4000, 0x2B, -2));
        ready = hilo && R_SUCCEEDED(threadStart(&stream->thread));
        if (!ready) sdl3_shim_set_error("Could not start the audio thread");
    }
    if (!ready) {
        if (hilo) threadClose(&stream->thread);
        if (salida) audoutStopAudioOut();
        if (servicio) audoutExit();
        for (int i = 0; i < BUFFER_COUNT; ++i)
            free(stream->memory[i]);
        free(stream->queue);
        free(stream);
        mutexUnlock(&g_device_lock);
        return NULL;
    }
    g_stream = stream;
    mutexUnlock(&g_device_lock);
    return stream;
}

void SDL_DestroyAudioStream(SDL_AudioStream* stream) {
    if (stream == NULL)
        return;
    mutexLock(&g_device_lock);
    if (g_stream != stream) { mutexUnlock(&g_device_lock); return; }
    __atomic_store_n(&stream->stop, true, __ATOMIC_RELEASE);
    threadWaitForExit(&stream->thread);
    threadClose(&stream->thread);
    audoutStopAudioOut();
    audoutExit();
    for (int i = 0; i < BUFFER_COUNT; ++i)
        free(stream->memory[i]);
    free(stream->queue);
    free(stream);
    g_stream = NULL;
    mutexUnlock(&g_device_lock);
}

SDL_AudioDeviceID SDL_GetAudioStreamDevice(SDL_AudioStream* stream) {
    mutexLock(&g_device_lock);
    const SDL_AudioDeviceID id = stream && stream == g_stream ? 1 : 0;
    mutexUnlock(&g_device_lock);
    return id;
}
bool SDL_GetAudioDeviceFormat(SDL_AudioDeviceID devid, SDL_AudioSpec* spec, int* sample_frames) {
    mutexLock(&g_device_lock);
    const bool valido = g_stream && devid == 1;
    if (valido) {
        if (spec) *spec = (SDL_AudioSpec){SDL_AUDIO_S16, CHANNELS, OUTPUT_RATE};
        if (sample_frames) *sample_frames = BUFFER_FRAMES;
    }
    mutexUnlock(&g_device_lock);
    return valido;
}
bool SDL_PauseAudioStreamDevice(SDL_AudioStream* stream) {
    if (!stream) return false;
    mutexLock(&stream->lock); stream->playing = false; mutexUnlock(&stream->lock);
    return true;
}
bool SDL_ClearAudioStream(SDL_AudioStream* stream) {
    if (!stream) return false;
    mutexLock(&stream->lock); stream->head = stream->count = 0; stream->position = 0; mutexUnlock(&stream->lock);
    return true;
}

bool SDL_ResumeAudioStreamDevice(SDL_AudioStream* stream) {
    if (stream == NULL)
        return false;
    mutexLock(&stream->lock);
    stream->playing = true;
    mutexUnlock(&stream->lock);
    return true;
}

// Only the input side changes (the DSP switching between 32 and 48 kHz).
bool SDL_SetAudioStreamFormat(SDL_AudioStream* stream, const SDL_AudioSpec* src_spec,
                              const SDL_AudioSpec* dst_spec) {
    if (stream == NULL || (src_spec != NULL && !valid_input(src_spec)) ||
        (dst_spec && (dst_spec->freq != OUTPUT_RATE || dst_spec->channels != CHANNELS || dst_spec->format != SDL_AUDIO_S16)))
        return false;
    if (src_spec != NULL) {
        mutexLock(&stream->lock);
        stream->input_rate = src_spec->freq;
        stream->input_channels = src_spec->channels;
        stream->input_format = src_spec->format;
        // Queued elements are already S16 stereo; a rate change must not reinterpret them at the new clock.
        stream->head = stream->count = 0; stream->position = 0;
        mutexUnlock(&stream->lock);
    }
    return true;
}

bool SDL_PutAudioStreamData(SDL_AudioStream* stream, const void* buf, int len) {
    if (stream == NULL || buf == NULL || len < 0)
        return false;
    mutexLock(&stream->lock);
    const size_t muestra = stream->input_format == SDL_AUDIO_F32 ? sizeof(float) : sizeof(s16);
    const size_t paso = stream->input_channels * muestra;
    if ((size_t)len % paso) { mutexUnlock(&stream->lock); return false; }
    size_t frames = (size_t)len / paso;
    // A full queue keeps the oldest second and drops the rest.
    if (frames > QUEUE_FRAMES - stream->count)
        frames = QUEUE_FRAMES - stream->count;
    for (size_t i = 0; i < frames; ++i) {
        s16* slot = &stream->queue[((stream->head + stream->count) % QUEUE_FRAMES) * CHANNELS];
        lmnx_audio_stereo((const char*)buf + i * paso, stream->input_channels, stream->input_format == SDL_AUDIO_F32, slot);
        ++stream->count;
    }
    mutexUnlock(&stream->lock);
    return true;
}

int SDL_GetAudioStreamQueued(SDL_AudioStream* stream) {
    if (stream == NULL)
        return -1;
    mutexLock(&stream->lock);
    const int bytes = (int)(stream->count * stream->input_channels * (stream->input_format == SDL_AUDIO_F32 ? sizeof(float) : sizeof(s16)));
    mutexUnlock(&stream->lock);
    return bytes;
}
