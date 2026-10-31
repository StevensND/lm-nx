#pragma once
// CPU and GX turns. GX opens within the Aurora frame; solapar_cpu.flag lets the CPU advance while the previous
// frame closes.
#include "recomp/ctx.h"

#include <atomic>

namespace recomp::frame {

using Entry = void (*)();

void start(Entry boot);       // boot runs on a new host thread once the first frame opens
bool runUntilPresent();       // false once the guest has exited
// One host frame (between aurora_begin_frame and aurora_end_frame): runs the
// guest until it presents, or with frame interpolation on (settings), replays
// the guest's recorded frames instead while the guest runs on its own.
bool hostFrame();             // false once the guest has exited
void pace();                  // after aurora_end_frame (lockstep only; interpolated frames: pacing.h)
bool interpolating();         // host: frames are replayed tapes (decoupled guest)
u32 guestFrames();             // frames the game has finished (VISetNextFrameBuffer), fast-forwarded ones included
// Presentation target (F1 > Visuals > Frame rate): the slider, or the refresh rate of the
// display the window is on. 30 = the game's own rate without interpolation.
int targetFps();
float displayRefreshHz();     // 0 if unknown; host (main) thread
void stop();
bool gamePaused();
void setHostPaused(bool paused);  // host: window paused (no frames)

void present(Ctx& c, u32 xfb, bool black);  // guest side
// Guest side, before any GX work: while the presenting thread waits for the host to finish
// the frame, other guest threads keep running (present() gives up the guest CPU); one that
// reaches GX waits here until the guest's turn, since the host owns the Aurora frame.
namespace detail {
extern std::atomic<bool> hostOwnsFrame LM_HIDDEN;  // the host has the Aurora frame (a guest thread waits in present)
extern std::atomic<bool> gxBusy LM_HIDDEN;         // gxBlockingWait(): a guest thread waits for GX work
void gxWaitForGuestTurnSlow();
}  // namespace detail
// Inline fast path, called before every GX call (about 18,000 draws per frame). Keep only the rare wait
// out of line; forcing the wait and overlap barriers inline increased measured frame time.
inline void gxWaitForGuestTurn() {
    if (!detail::hostOwnsFrame.load(std::memory_order_acquire) && !detail::gxBusy.load(std::memory_order_acquire)) return;
    detail::gxWaitForGuestTurnSlow();
}
// Prepare the menu and display policy and drain the FIFO before releasing the CPU just before aurora_end_frame.
// GX waits for the next hostFrame. Enabled with solapar_cpu.flag, or LM_OVERLAP_CPU=1 on PC; otherwise keep the
// previous order.
namespace detail {
extern bool g_overlapCpu;  // solapar_cpu.flag, read once by start() before the game runs
}
// inline and a plain load: overlapGraphicsBarrier() runs on hot paths (34 places, FIFO writes included);
// as an out-of-line call with a function-local static it took ~8.6 % of the game thread with the flag off.
inline bool overlapGuestCpu() { return detail::g_overlapCpu; }
void resumeGuestCpu();
// Before the next iteration's events and menu, wait for the released thread to park at a graphics entry point;
// GX stays closed.
void finishGuestCpuOverlap();
// The extra entry points keep their previous behavior without the flag.
inline void overlapGraphicsBarrier() {
    if (overlapGuestCpu()) gxWaitForGuestTurn();
}
// Guest side: runs `wait` (host work that blocks on GX, e.g. draining the FIFO for
// GXDrawDone) with the guest CPU given up, like the console's GXDrawDone sleeping on its
// interrupt; other threads' GX calls wait until it is done.
void gxBlockingWait(void (*wait)());

// The US game hands each finished frame to VISetNextFrameBuffer. The PAL release
// re-submits the framebuffer from its post-retrace callback every retrace and paces its
// frames with an alarm instead, so there a frame is finished at GXCopyDisp.
bool presentOnCopy();
bool viBlack();  // last VISetBlack

}  // namespace recomp::frame
