// Asynchronous SD logging. stdout and stderr retain their FILE objects through freopen onto a small libnx
// device, logq:. Writes copy bytes into memory rings; a low-priority thread (Rol::Compilador, 0x3B) writes them
// to SD files. A full ring drops and counts new bytes instead of blocking the game. fatal(), the Horizon
// exception handler and exit() drain synchronously to preserve final messages. Shared state is never destroyed
// because the thread outlives static destructors. A crash drain skips state whose lock it cannot acquire. stdout
// is line buffered, limiting exception losses to a partial line, and failed SD writes are counted. This avoids
// periodic-report stalls on slow SD cards.
#include "runtime.hpp"

#include <switch.h>
#include <sys/iosupport.h>
#include <sys/stat.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr size_t kAnillo = 1u << 20;   // per file: about a minute of the busiest logging
constexpr size_t kTrozo = 64u * 1024;  // bytes written to the SD per call

struct Canal {
    const char* nombre = nullptr;   // file name under logs/
    FILE* sd = nullptr;             // the real file on the SD
    std::vector<char> anillo;
    size_t inicio = 0, usados = 0;  // under mutex
    std::atomic<unsigned long long> descartados{0}, fallidos{0}, vaciadosFallidos{0};  // bytes, bytes, fflush calls
    unsigned long long descartadosInformados = 0, fallidosInformados = 0, vaciadosInformados = 0;  // under escritura
};
// Never destroyed: the log thread keeps running while exit() runs the static destructors.
struct Estado {
    Canal canales[2] = {{"ProjectLM-NX.log"}, {"errores.log"}};
    std::mutex mutex;      // the rings
    std::mutex escritura;  // the SD files (log thread and synchronous drains)
    std::condition_variable aviso;
    std::atomic<bool> activo{false};
    std::atomic<bool> parar{false};  // exit(): wakes the thread so it can see `cerrado` and end
    bool cerrado = false;            // under escritura: detenerLog() has drained; nobody writes the files again
    bool admitir = true;             // under mutex: detenerLog() stops accepting bytes before its final drain
};
Estado& estado = *new Estado;

// Locks `m`; with `urgencia` (a crash: the thread that held it may be dead) it gives up after ~200 ms.
bool bloquear(std::mutex& m, bool urgencia) {
    if (!urgencia) {
        m.lock();
        return true;
    }
    for (int i = 0; i < 200; ++i) {
        if (m.try_lock()) return true;
        svcSleepThread(1'000'000LL);
    }
    return false;
}

// Moves up to min(kTrozo, `maximo`) pending bytes of one channel into `destino`; returns how many (0 if the
// lock was not available in a crash).
size_t tomar(Canal& c, char* destino, size_t maximo, bool urgencia) {
    if (!bloquear(estado.mutex, urgencia)) return 0;
    size_t n = c.usados < kTrozo ? c.usados : kTrozo;
    n = n < maximo ? n : maximo;
    const size_t primero = n < kAnillo - c.inicio ? n : kAnillo - c.inicio;
    std::memcpy(destino, c.anillo.data() + c.inicio, primero);
    std::memcpy(destino + primero, c.anillo.data(), n - primero);
    c.inicio = (c.inicio + n) % kAnillo;
    c.usados -= n;
    estado.mutex.unlock();
    return n;
}

// Writes everything pending to the SD files (the caller holds estado.escritura).
void vaciarCanales(char* trozo, bool urgencia) {
    for (auto& c : estado.canales) {
        if (!c.sd) continue;
        bool escrito = false;
        // Only what was pending when the pass started: busy printers cannot keep a drain (and the lock) forever.
        size_t pendiente;
        if (!bloquear(estado.mutex, urgencia)) continue;
        pendiente = c.usados;
        estado.mutex.unlock();
        for (size_t n; pendiente != 0 && (n = tomar(c, trozo, pendiente, urgencia)) != 0;) {
            pendiente -= n;
            const size_t hecho = std::fwrite(trozo, 1, n, c.sd);
            if (hecho != n) c.fallidos.fetch_add(n - hecho, std::memory_order_relaxed);
            escrito = true;
        }
        // The counters only count as reported once their line was written and flushed.
        const unsigned long long descartados = c.descartados.load(std::memory_order_relaxed);
        const unsigned long long fallidos = c.fallidos.load(std::memory_order_relaxed);
        const unsigned long long vaciados = c.vaciadosFallidos.load(std::memory_order_relaxed);
        bool aviso = true;
        if (descartados != c.descartadosInformados)
            aviso = std::fprintf(c.sd, "[log] %llu bytes dropped: SD writes too slow (asynchronous log full)\n",
                                 descartados - c.descartadosInformados) > 0 && aviso;
        if (fallidos != c.fallidosInformados)
            aviso = std::fprintf(c.sd, "[log] %llu bytes could not be written to SD\n",
                                 fallidos - c.fallidosInformados) > 0 && aviso;
        if (vaciados != c.vaciadosInformados)
            aviso = std::fprintf(c.sd, "[log] %llu vaciados a la SD fallaron\n", vaciados - c.vaciadosInformados) > 0 &&
                    aviso;
        const bool hayAviso = descartados != c.descartadosInformados || fallidos != c.fallidosInformados ||
                              vaciados != c.vaciadosInformados;
        if (!escrito && !hayAviso) continue;
        if (std::fflush(c.sd) != 0) {
            c.vaciadosFallidos.fetch_add(1, std::memory_order_relaxed);
        } else if (aviso) {
            c.descartadosInformados = descartados;
            c.fallidosInformados = fallidos;
            c.vaciadosInformados = vaciados;
        }
    }
}

void hilo() {
    lmnx::plataforma::configurarHilo(lmnx::plataforma::Rol::Compilador);
    static char trozo[kTrozo];
    while (!estado.parar.load(std::memory_order_acquire)) {
        {
            // Wakes on the first bytes after an empty ring, or every 250 ms.
            std::unique_lock lock{estado.mutex};
            estado.aviso.wait_for(lock, std::chrono::milliseconds(250), [] {
                return estado.canales[0].usados != 0 || estado.canales[1].usados != 0 ||
                       estado.parar.load(std::memory_order_acquire);
            });
        }
        if (!estado.parar.load(std::memory_order_acquire))
            svcSleepThread(100'000'000LL);  // let the burst finish: one SD write for it, not one per line
        std::lock_guard escritura{estado.escritura};
        if (estado.cerrado) return;  // detenerLog() has drained and closed: the files are no longer ours
        vaciarCanales(trozo, false);
    }
}

int abrir(struct _reent* r, void* fichero, const char* ruta, int, int) {
    const char* nombre = std::strchr(ruta, '/');
    nombre = nombre ? nombre + 1 : ruta;
    for (auto& c : estado.canales) {
        if (std::strcmp(nombre, c.nombre) != 0) continue;
        if (!c.sd) {
            const std::string destino = std::string("sdmc:/switch/LM-NX/logs/") + c.nombre;
            c.sd = std::fopen(destino.c_str(), "w");
            if (!c.sd) break;
            std::setvbuf(c.sd, nullptr, _IOFBF, kTrozo);
            c.anillo.assign(kAnillo, 0);
        }
        *static_cast<Canal**>(fichero) = &c;
        return 0;
    }
    r->_errno = ENOENT;
    return -1;
}

int cerrar(struct _reent*, void*) { return 0; }

ssize_t escribir(struct _reent*, void* fichero, const char* datos, size_t bytes) {
    Canal& c = **static_cast<Canal**>(fichero);
    bool despertar;
    {
        std::lock_guard lock{estado.mutex};
        if (!estado.admitir) return ssize_t(bytes);  // exit() is closing the log
        if (bytes > kAnillo - c.usados) {
            c.descartados.fetch_add(bytes, std::memory_order_relaxed);  // never block the printing thread
            return ssize_t(bytes);
        }
        despertar = c.usados == 0;
        const size_t fin = (c.inicio + c.usados) % kAnillo;
        const size_t primero = bytes < kAnillo - fin ? bytes : kAnillo - fin;
        std::memcpy(c.anillo.data() + fin, datos, primero);
        std::memcpy(c.anillo.data(), datos + primero, bytes - primero);
        c.usados += bytes;
    }
    if (despertar) estado.aviso.notify_one();
    return ssize_t(bytes);
}

int consultar(struct _reent*, void*, struct stat* st) {
    std::memset(st, 0, sizeof(*st));
    st->st_mode = S_IFREG;
    return 0;
}

const devoptab_t kDispositivo = {
    .name = "logq",
    .structSize = sizeof(Canal*),
    .open_r = abrir,
    .close_r = cerrar,
    .write_r = escribir,
    .fstat_r = consultar,
};
}  // namespace

namespace lmnx::plataforma {
bool iniciarLogAsincrono() {
    if (AddDevice(&kDispositivo) < 0) return false;
    if (!std::freopen("logq:/ProjectLM-NX.log", "w", stdout)) return false;
    // Line buffered: a print costs a copy into the ring, and an exception loses at most a partial line.
    std::setvbuf(stdout, nullptr, _IOLBF, 4 * 1024);
    if (!std::freopen("logq:/errores.log", "w", stderr)) {
        // Never leave stderr closed: back to the old way for both (main reopens stdout on the SD).
        if (std::freopen("sdmc:/switch/LM-NX/logs/errores.log", "w", stderr))
            std::setvbuf(stderr, nullptr, _IONBF, 0);
        return false;
    }
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    estado.activo.store(true, std::memory_order_release);
    // Persistent thread on the heap: Horizon closes the game on detach() (memory: horizon-thread-detach).
    new std::thread(hilo);
    return true;
}

// Writes the rings to the SD now. `urgencia`: a crash path, which must not block on locks a dead thread may
// hold; it gives up when it cannot take them (it also leaves the stdio FILE buffers alone: the caller flushes
// them first when that is safe).
void vaciarLog(bool urgencia) {
    if (!estado.activo.load(std::memory_order_acquire)) return;
    static char trozo[kTrozo];
    if (!bloquear(estado.escritura, urgencia)) return;
    if (!estado.cerrado) vaciarCanales(trozo, urgencia);
    estado.escritura.unlock();
}

// exit(): writes what is left and closes the log under the SD lock, before stdio's cleanup closes the files;
// the thread checks `cerrado` under the same lock before every drain, so it never touches the files again
// (it waits only for a drain in progress). Lines printed after this are lost.
void detenerLog() {
    if (!estado.activo.load(std::memory_order_acquire)) return;
    static char trozo[kTrozo];
    {
        {
            // No new bytes from here on (they would be lost after exit() anyway): the drain below always ends.
            std::lock_guard lock{estado.mutex};
            estado.admitir = false;
        }
        std::lock_guard escritura{estado.escritura};
        while (!estado.cerrado) {
            vaciarCanales(trozo, false);
            std::lock_guard lock{estado.mutex};
            if (estado.canales[0].usados == 0 && estado.canales[1].usados == 0) break;
        }
        estado.cerrado = true;
    }
    estado.activo.store(false, std::memory_order_release);
    estado.parar.store(true, std::memory_order_release);
    estado.aviso.notify_one();
}
}  // namespace lmnx::plataforma

// For src/recomp/runtime.cpp's fatal() (common to the PC build, which leaves it undefined: weak reference).
// fatal() has already flushed stdout and stderr into the rings.
extern "C" void lmnx_log_vaciar() { lmnx::plataforma::vaciarLog(true); }
