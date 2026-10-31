#pragma once
#include <cstdint>

namespace recomp::gx {
// Display copy settings the game made (GXSetCopyFilter, GXSetDispCopyGamma, GXSetDispCopySrc,
// GXSetCopyClamp), applied on presentation by the display copy filter pass (crt.cpp).
struct DisplayCopy {
    uint8_t vfilter[7] = {0, 0, 21, 22, 21, 0, 0};  // vertical filter taps (sum 64)
    uint8_t gamma = 0;                              // GXGamma: 0 = 1.0, 1 = 1.7, 2 = 2.2
    uint8_t clamp = 3;                              // GXFBClamp bits (top 1, bottom 2)
    uint16_t efbHeight = 480;                       // display copy source height (EFB lines)
};
const DisplayCopy& displayCopy();
// Pushes the F1 > Visuals > Rendering accuracy options to Aurora (host thread, every frame).
void applyAccuracySettings();
// Enhanced lighting is on: "EFB color depth" (and "6-bit blending") are off whatever the
// setting says (their F1 toggles are greyed out; docs/RENDERING.md).
bool enhancedOverridesEfbDepth();
// Effective "Smooth color banding" (Aurora 0190): 0 off, 1 smooth + sub-step noise (the option),
// 2 smooth without the noise (A/B test only). The setting, forced on while enhanced lighting is
// on; LM_SMOOTH_BANDING=<0-2> overrides (test aid).
int smoothBandingLevel();
// Enhanced lighting is what forces smoothing on (for the F1 menu note).
bool smoothBandingForced();

// Re-evaluate the per-draw state (projection, enhanced-lighting draw class) before the next
// draw written straight into the FIFO by game code.
void recheckDrawState();
// Closes a draw opened by GXBegin whose vertices were written through the FIFO.
void endPendingDraw();
// Malformed FIFO data (Aurora 0235): the guest GX call sites before each FIFO stream position, for
// naming the caller when Aurora reports a bad command (docs/RENDERING.md "Malformed FIFO data").
void noteCallSite(uint32_t lr);
void installFifoErrorReport();
// Model viewer: draw with textures replaced by white (TEV colour inputs / modes).
void setUntextured(bool on);
// The guest finished a frame (VISetNextFrameBuffer): closes any open draw.
void endFrame();
// Fast-forward (skip.cpp): the current guest frame is not shown, so its draws and
// copies are dropped (state changes still reach Aurora; display lists are still recorded).
extern bool g_discardDraws;
// Current vertex layout (hle_gx shadows): bytes attribute `attr` takes in a vertex of
// format `fmt` (-1 unknown), and its GXAttrType.
int vtxAttrSize(int fmt, int attr);
unsigned char vtxDesc(int attr);
// changes whenever vtxDesc/vtxAttrSize can change (the HLEs of GXClearVtxDesc, GXSetVtxDesc(v),
// GXSetVtxAttrFmt(v)), so derived values can be cached.
unsigned vtxLayoutVersion();
// the per-draw layout queries of the vertex snapshots, answered here from the shadows instead of through one
// call per attribute. vtxSize: bytes per vertex of `fmt` (the sum of vtxAttrSize over GX_VA_PNMTXIDX..GX_VA_TEX7, 0 if
// one is unknown), kept per format until vtxLayoutVersion moves. vtxLayoutHash: FNV-1a of vtxDesc(GX_VA_PNMTXIDX..
// GX_VA_TEX7), then of u8(vtxAttrSize(f, a) + 1) over the same attributes for each format f in fmtMask (bits 0-7).
// vtxDescs: vtxDesc(attr) for every attr < GX_VA_MAX_ATTR.
unsigned vtxSize(int fmt);
unsigned long long vtxLayoutHash(unsigned fmtMask);
const unsigned char* vtxDescs();
// The VAT entry of `attr` in format `fmt` (component count, GXCompType, fraction bits).
bool vtxAttrFormat(int fmt, int attr, unsigned char& cnt, unsigned char& type, unsigned char& frac);
// An active TEV stage samples an EFB copy.
bool samplesCopy();
}  // namespace recomp::gx
