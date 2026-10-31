// HLE: DVD file system, on top of Aurora's disc reader. Reads complete
// synchronously; async completions are delivered as interrupts.
#include "recomp/hidden_mansion.h"
#include "recomp/room_swap.h"
#include "recomp/unused_maps.h"
#include "recomp/unused2.h"
#include "recomp/hle.h"
#include "recomp/interp.h"
#include "recomp/runtime.h"
#include "recomp/sched.h"

#include <algorithm>
#include <cstring>
#include <dolphin/dvd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace recomp {
namespace {

// Switch file reads use filesystem IPC. Horizon temporarily removes access to destination buffer pages, so
// reading an ISO directly into game RAM can crash other threads that access those pages. Read into a
// thread-owned buffer, then copy into game RAM.
}  // namespace
// wall time of the host disc reads (all threads), taken once per game frame by the hitch log (hle_gx.cpp).
std::atomic<u64> g_dvdReadNs{0}, g_dvdReadBytes{0};
namespace {
template <class F>
s32 readVia(u32 dest, s32 len, F&& read) {
    if (len > 0) interp::noteGuestWrite(dest, u32(len));  // memory a DVD read fills (vertex snapshots)
#ifdef __SWITCH__
    if (len <= 0) return read(hostp(dest));
    thread_local std::vector<u8> bounce;
    if (bounce.size() < size_t(len)) bounce.resize(size_t(len));
    const auto readStart = std::chrono::steady_clock::now();
    const s32 r = read(bounce.data());
    g_dvdReadNs.fetch_add(u64(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() -
                                                                                    readStart).count()),
                          std::memory_order_relaxed);
    g_dvdReadBytes.fetch_add(u64(len), std::memory_order_relaxed);
    if (r > 0) std::memcpy(hostp(dest), bounce.data(), size_t(std::min(r, len)));
    if (bounce.size() > (8u << 20)) std::vector<u8>().swap(bounce);  // no retener lecturas enormes
    return r;
#else
    return read(hostp(dest));
#endif
}

// The console's DVD driver splits long reads and serves other requests between the pieces, so a small read
// (an AFC stream piece, jaudio syncstream.c LoadADPCM) does not wait for a whole map load (1.9 MB when the
// mansion loads). Here host reads take turns under one lock, reads of normal priority go in pieces of kPieza
// bytes, and before each piece they let any waiting higher-priority or small read go first. (The crackle of
// issue #25 was not this but a race in the async read: see DVDReadAsyncPrio.)
constexpr s32 kPieza = 128 * 1024;
std::mutex sLecturaDisco;
std::atomic<int> sPrioritariasEsperando{0};

s32 hostReadPrio(DVDFileInfo* fi, void* dst, s32 len, s32 offset, s32 prio) {
    // Higher than normal priority (the streamer uses 1), and any small read (a stream piece is 5760 bytes, whatever
    // priority it was given): one go, ahead of the pieces of the others.
    if (prio < 2 || len <= 32 * 1024) {
        sPrioritariasEsperando.fetch_add(1, std::memory_order_acq_rel);
        std::lock_guard lk(sLecturaDisco);
        sPrioritariasEsperando.fetch_sub(1, std::memory_order_acq_rel);
        return DVDReadPrio(fi, dst, len, offset, prio);
    }
    s32 total = 0;
    while (total < len) {
        while (sPrioritariasEsperando.load(std::memory_order_acquire) > 0) std::this_thread::yield();
        const s32 n = std::min(kPieza, len - total);
        s32 got;
        {
            std::lock_guard lk(sLecturaDisco);
            got = DVDReadPrio(fi, static_cast<u8*>(dst) + total, n, offset + total, prio);
        }
        if (got <= 0) return total ? total : got;
        total += got;
        if (got < n) break;  // end of the file
    }
    return total;
}

// A destination the game may fill with file data: main memory above the OS globals (MEM1 from
// 0x80003100, the DOL's first section) or the host window above MEM1 (ctx.h). A failed game heap
// allocation (null buffer) would otherwise land in low memory: the bus clock at 0x800000F8, thread
// and arena pointers (it did in event01 up to 1.1: hle_os.cpp OSInit). Refused writes are logged.
bool destOk(u32 dest, u32 size, const char* what, u32 lr) {
    const u64 end = u64(dest) + size;
    if ((dest >= 0x80003100u && end <= 0x81800000u) || (dest >= 0x81810000u && end <= 0x80000000ull + kHostMemSize))
        return true;
    log("[dvd] ERROR: %s of %X bytes to %08X refused (lr %08X): the game's buffer allocation failed?", what, size,
        dest, lr);
    return false;
}

// Host-served files: leftover-room visits (room_swap.cpp), the rebuilt unused maps 7/8
// (unused_maps.cpp), then the PAL Hidden Mansion overlay.
s32 ovEntry(const char* path) {
    s32 e = rooms::overlayEntry(path);
    if (e < 0) e = unused2::overlayEntry(path);
    if (e < 0) e = unused_maps::overlayEntry(path);
    return e >= 0 ? e : hm::overlayEntry(path);
}
bool ovIs(s32 e) {
    return rooms::isOverlayEntry(e) || unused2::isOverlayEntry(e) || unused_maps::isOverlayEntry(e) || hm::isOverlayEntry(e);
}
u32 ovLength(s32 e) {
    if (rooms::isOverlayEntry(e)) return rooms::overlayLength(e);
    if (unused2::isOverlayEntry(e)) return unused2::overlayLength(e);
    if (unused_maps::isOverlayEntry(e)) return unused_maps::overlayLength(e);
    return hm::overlayLength(e);
}
s32 ovRead(s32 e, void* dst, s32 len, s32 off) {
    if (rooms::isOverlayEntry(e)) return rooms::overlayRead(e, dst, len, off);
    if (unused2::isOverlayEntry(e)) return unused2::overlayRead(e, dst, len, off);
    if (unused_maps::isOverlayEntry(e)) return unused_maps::overlayRead(e, dst, len, off);
    return hm::overlayRead(e, dst, len, off);
}

// Foyer left pillar: the wide blocks in room.bin in room_02.arc (heights 321-401 and 821-901) have front faces
// at z = 290, coplanar with the gold frame and overlapping it at x = -1000..-990. The right frame starts at the
// pillar's edge. This causes depth fighting visible in widescreen; in 4:3 the frame is off-screen. Move these 20
// vertices to z = 288 while reading, checking the original value 290. USA and PAL use the same file. Identify it
// by its disc entry when opened; Aurora gives every file startAddr = 0.
constexpr u32 kFoyerPillarZ[] = {0x27470, 0x27476, 0x27482, 0x2748E, 0x274A6, 0x274AC, 0x274B2,
                                 0x274BE, 0x274CA, 0x274E2, 0x28C70, 0x28C76, 0x28C82, 0x28C8E,
                                 0x28CA6, 0x28CAC, 0x28CB2, 0x28CBE, 0x28CCA, 0x28CE2};
s32 foyerRoomEntry() {
    static const s32 sEntry = DVDConvertPathToEntrynum("/Iwamoto/map2/room_02.arc");
    return sEntry;
}
std::unordered_set<u32> sFoyerFiles;  // guest DVDFileInfo abiertos sobre room_02.arc

void noteFoyerOpen(u32 guest, s32 entry, bool ok) {
    if (ok && entry >= 0 && entry == foyerRoomEntry()) sFoyerFiles.insert(guest);
    else sFoyerFiles.erase(guest);
}

// Capture `foyer` before releasing the CPU for the read; other threads may open and close files meanwhile.
bool isFoyerFile(u32 guest) { return sFoyerFiles.count(guest) != 0; }

void fixFoyerPillar(bool foyer, void* dst, s32 got, s32 off) {
    if (!foyer || got <= 0 || off < 0) return;
    auto* bytes = static_cast<u8*>(dst);
    const s64 end = s64(off) + got;
    for (const u32 at : kFoyerPillarZ) {
        // 290 -> 288 (big-endian s16): only change the low byte, even if the read splits the value.
        const s64 lo = s64(at) + 1;
        if (lo < off || lo >= end) continue;
        if (s64(at) >= off && bytes[at - u32(off)] != 0x01) continue;
        if (bytes[lo - off] == 0x22) bytes[lo - off] = 0x20;
    }
}

std::unordered_map<u32, DVDFileInfo> sFiles;  // guest DVDFileInfo -> host
std::unordered_map<u32, u32> sNames;          // FST entry -> guest copy of its name
std::unordered_map<u32, s32> sOverlay;        // guest DVDFileInfo -> hm overlay entry

// Disc latency. The game relies on reads taking time (e.g. BootScene switches the
// current heap right after queueing an async open whose callback allocates), so
// a read keeps the requesting thread blocked while other threads run. Faster
// than the console's drive, but never instantaneous.
double readLatency(u32 bytes) {
    static const double scale = [] {
        const char* env = std::getenv("LM_DVD_SPEED");  // 1 = console speed
        return env ? 1.0 / std::atof(env) : 0.1;
    }();
    return scale * (0.010 + bytes / 3.0e6);
}

DVDFileInfo* hostFile(u32 guest) {
    auto it = sFiles.find(guest);
    return it == sFiles.end() ? nullptr : &it->second;
}

void syncGuest(u32 guest, const DVDFileInfo& fi) {
    wr32(guest + 0x0C, 0);  // DVD_STATE_END
    wr32(guest + 0x30, fi.startAddr);
    wr32(guest + 0x34, fi.length);
}

// Files replaced by the PAL Hidden Mansion overlay (hidden_mansion.cpp) are served from host
// memory instead of the disc image.
bool openOverlay(u32 guest, s32 entry) {
    sFiles.erase(guest);
    sFoyerFiles.erase(guest);
    sOverlay[guest] = entry;
    wr32(guest + 0x0C, 0);
    wr32(guest + 0x30, 0);
    wr32(guest + 0x34, ovLength(entry));
    return true;
}

s32 overlayOf(u32 guest) {
    auto it = sOverlay.find(guest);
    return it == sOverlay.end() ? -1 : it->second;
}

bool openWith(u32 guest, BOOL ok, const DVDFileInfo& fi) {
    if (!ok) return false;
    sOverlay.erase(guest);
    sFiles[guest] = fi;
    syncGuest(guest, fi);
    return true;
}

}  // namespace

// The game's Yay0 decompressor for loaded files (LMDvdFileInfoBlock, run on the DVD threads):
// (Yay0 source r3, destination r4). Its caller does not check the destination allocation; a null
// one is refused here (logged) instead of decompressing over low memory.
HLE(fn_800071D0__18LMDvdFileInfoBlockFP18LMDvdFileInfoBlock) {
    const u32 src = c.r[3], dest = c.r[4];
    const u32 size = src >= 0x80000000u && src < 0x81800000u ? rd32(src + 4) : 0;
    if (!destOk(dest, size, "Yay0 decompression", c.lr)) return;
    callOriginal(c, "fn_800071D0__18LMDvdFileInfoBlockFP18LMDvdFileInfoBlock");
}

HLE(DVDInit) {}
HLE(DVDGetDriveStatus) { c.r[3] = 0; }
HLE(DVDCheckDisk) { c.r[3] = 1; }
HLE(DVDGetCommandBlockStatus) { c.r[3] = rd32(c.r[3] + 0x0C); }
HLE(DVDConvertPathToEntrynum) {
    if (const s32 e = ovEntry(gstr(c.r[3])); e >= 0) {
        c.r[3] = u32(e);
        return;
    }
    const s32 n = DVDConvertPathToEntrynum(gstr(c.r[3]));
    if (n < 0) log("[dvd] path not found: %s", gstr(c.r[3]));
    c.r[3] = u32(n);
}

HLE(DVDOpen) {
    DVDFileInfo fi{};
    const char* path = gstr(c.r[3]);
    if (const s32 e = ovEntry(path); e >= 0) {
        c.r[3] = openOverlay(c.r[4], e);
        return;
    }
    c.r[3] = openWith(c.r[4], DVDOpen(path, &fi), fi);
    noteFoyerOpen(c.r[4], DVDConvertPathToEntrynum(path), c.r[3] != 0);
    if (!c.r[3]) log("[dvd] open failed: %s", path);
}
HLE(DVDFastOpen) {
    if (ovIs(s32(c.r[3]))) {
        c.r[3] = openOverlay(c.r[4], s32(c.r[3]));
        return;
    }
    DVDFileInfo fi{};
    const s32 entry = s32(c.r[3]);
    c.r[3] = openWith(c.r[4], DVDFastOpen(entry, &fi), fi);
    noteFoyerOpen(c.r[4], entry, c.r[3] != 0);
}
HLE(DVDClose) {
    sOverlay.erase(c.r[3]);
    sFoyerFiles.erase(c.r[3]);
    if (DVDFileInfo* fi = hostFile(c.r[3])) {
        DVDClose(fi);
        sFiles.erase(c.r[3]);
    }
    c.r[3] = 1;
}

HLE(DVDReadPrio) {
    DVDFileInfo* fi = hostFile(c.r[3]);
    watchRange(c.r[4], c.r[5], "DVDReadPrio");
    const s32 ov = overlayOf(c.r[3]);
    const bool foyer = isFoyerFile(c.r[3]);
    // The host read (disc image, RVZ decompression) can take several milliseconds for a big
    // read: give up the CPU meanwhile, as the console's thread sleeps while the drive works.
    s32 result = -1;
    const bool ok = destOk(c.r[4], c.r[5], "DVDReadPrio", c.lr);
    if (ok) sched::withCpuReleased([&] {
        result = readVia(c.r[4], s32(c.r[5]), [&](void* dst) {
            if (ov >= 0) return ovRead(ov, dst, s32(c.r[5]), s32(c.r[6]));
            if (!fi) return -1;
            const s32 got = hostReadPrio(fi, dst, s32(c.r[5]), s32(c.r[6]), s32(c.r[7]));
            fixFoyerPillar(foyer, dst, got, s32(c.r[6]));
            return got;
        });
    });
    // Block this thread until the "drive" finishes.
    static std::atomic<u32> sWaitKey{0xF0000000};  // sleep-queue keys need not be real memory
    const u32 done = sWaitKey.fetch_add(8);
    std::atomic<bool>* flag = new std::atomic<bool>(false);
    sched::postIrqAfter(readLatency(c.r[5]), [done, flag](Ctx&) {
        flag->store(true);
        sched::wakeup(done);
    });
    while (!flag->load()) sched::sleepOn(done);
    delete flag;
    c.r[3] = u32(result);
}

HLE(DVDReadAsyncPrio) {
    const u32 guest = c.r[3], cb = c.r[7];
    DVDFileInfo* fi = hostFile(guest);
    const s32 ov = overlayOf(guest);
    if (!fi && ov < 0) {
        c.r[3] = 0;
        return;
    }
    watchRange(c.r[4], c.r[5], "DVDReadAsyncPrio");
    const bool foyer = isFoyerFile(guest);
    s32 result = -1;
    const bool ok = destOk(c.r[4], c.r[5], "DVDReadAsyncPrio", c.lr);
    const auto read = [&] {
        if (!ok) return;
        result = readVia(c.r[4], s32(c.r[5]), [&](void* dst) {
            if (ov >= 0) return ovRead(ov, dst, s32(c.r[5]), s32(c.r[6]));
            const s32 got = hostReadPrio(fi, dst, s32(c.r[5]), s32(c.r[6]), s32(c.r[8]));
            fixFoyerPillar(foyer, dst, got, s32(c.r[6]));
            return got;
        });
    };
    // As in DVDReadPrio, a slow host read must not hold off interrupts. On the console this call
    // returns at once, so with interrupts off (a caller's critical section) no other thread may
    // run in it: then the read just happens in place.
    // (sched::interruptsEnabled() is also false inside an interrupt handler.)
    // Small reads also happen in place (issue #25, the thunder crackle): callers rely on the call
    // returning at once. JAudio's stream loader (LoadADPCM, syncstream.c) marks its buffer busy,
    // issues the read and only then advances the file offset; with the CPU given up during a slow
    // host read (the SD busy with the mansion load) an earlier completion and the stream callback
    // ran in that gap, saw the next buffer free and read the same offset again: the first piece of
    // the stream played twice, the second never, and the ADPCM decoder carried the wrong history
    // for seconds (clicks until a block resets it). A stream piece is 5760 bytes.
    if (sched::interruptsEnabled() && s32(c.r[5]) > 32 * 1024) sched::withCpuReleased(read);
    else read();
    wr32(guest + 0x0C, 1);  // busy until the completion interrupt
    wr32(guest + 0x38, cb);
    wr32(guest + 0x20, u32(result));
    sched::postIrqAfter(readLatency(c.r[5]), [guest, cb, result](Ctx& ic) {
        wr32(guest + 0x0C, 0);
        if (cb) guestCall(ic, cb, u32(result), guest);
    });
    c.r[3] = 1;
}

HLE(DVDPrepareStreamAsync) {  // streamed audio is not emulated yet
    const u32 guest = c.r[3], cb = c.r[6];
    sched::postIrq([guest, cb](Ctx& ic) {
        if (cb) guestCall(ic, cb, 0, guest);
    });
    c.r[3] = 1;
}
HLE(DVDCancelStream) { c.r[3] = 0; }

HLE(DVDOpenDir) {
    DVDDir d{};
    const int ok = DVDOpenDir(gstr(c.r[3]), &d);
    if (ok) {
        wr32(c.r[4], d.entryNum);
        wr32(c.r[4] + 4, d.location);
        wr32(c.r[4] + 8, d.next);
    }
    c.r[3] = u32(ok);
}
HLE(DVDReadDir) {
    DVDDir d{rd32(c.r[3]), rd32(c.r[3] + 4), rd32(c.r[3] + 8)};
    DVDDirEntry e{};
    const int ok = DVDReadDir(&d, &e);
    if (ok) {
        wr32(c.r[3] + 4, d.location);
        u32& name = sNames[e.entryNum];
        if (!name) {
            const std::string s = e.name ? e.name : "";
            name = hleAlloc(u32(s.size() + 1), 4);
            std::memcpy(hostp(name), s.c_str(), s.size() + 1);
        }
        wr32(c.r[4], e.entryNum);
        wr32(c.r[4] + 4, u32(e.isDir));
        wr32(c.r[4] + 8, name);
    }
    c.r[3] = u32(ok);
}
HLE(DVDCloseDir) { c.r[3] = 1; }

}  // namespace recomp
