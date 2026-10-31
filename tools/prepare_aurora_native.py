"""Adapt the patched GX producer to LMNX-owned types without Dawn. Keep extern/aurora unchanged. Prepare a build snapshot with the native resource boundary and its implementation."""
import hashlib
from pathlib import Path
import re
import shutil
from prepare_aurora_switch import preparar, RAIZ


# Reduce fixed GX call overhead: select FIFO recording once per multi-write GX command instead of each write,
# avoiding repeated TLS access through __aarch64_read_tp with -mtp=soft. Also avoid repeating publish() checks
# in finish_draw.
def transformaciones_41(destino):
    textos = {}

    def leer(rel):
        if rel not in textos:
            textos[rel] = (destino / rel).read_text(encoding="utf-8")
        return textos[rel]

    def uno(texto, viejo, nuevo, etiqueta):
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(
                f"GX call optimization, {etiqueta}: {n} matches; expected one"
            )
        return texto.replace(viejo, nuevo, 1)

    def cambiar(rel, viejo, nuevo):
        textos[rel] = uno(leer(rel), viejo, nuevo, rel)

    def funcion(rel, firma, transformar):
        texto = leer(rel)
        if texto.count(firma) != 1:
            raise ValueError(
                f"GX call optimization, {rel}: expected one signature {firma}"
            )
        a = texto.index(firma)
        b = texto.find("\n}\n", a)
        if b < 0:
            raise ValueError(
                f"GX call optimization, {rel}: missing closing brace for {firma}"
            )
        viejo = texto[a:b + 3]
        nuevo = transformar(viejo)
        textos[rel] = uno(
            texto, viejo, nuevo, rel + ": " + firma
        )

    # Recording& overloads preserve checking, writing and overflow bodies.
    h = "lib/gx/fifo.hpp"

    cambiar(
        h,
        "inline void write_data(const void* data, const uint32_t length) {\n"
        "  auto& rec = detail::recording();\n",
        "inline void write_data(detail::Recording& rec, const void* data, const uint32_t length) {\n",
    )
    cambiar(
        h,
        "inline void write_u8(const uint8_t val) {\n"
        "  auto& rec = detail::recording();\n",
        "inline void write_u8(detail::Recording& rec, const uint8_t val) {\n",
    )
    cambiar(
        h,
        "inline void write_u8(detail::Recording& rec, const uint8_t val) {\n",
        "inline void write_data(const void* data, const uint32_t length) {\n"
        "  write_data(detail::recording(), data, length);\n"
        "}\n\n"
        "inline void write_u8(detail::Recording& rec, const uint8_t val) {\n",
    )

    for nombre, tipo in (
        ("u16", "uint16_t"),
        ("u32", "uint32_t"),
        ("u64", "uint64_t"),
        ("f32", "float"),
    ):
        viejo = (
            f"inline void write_{nombre}(const {tipo} val) {{\n"
            "  const auto out = bswap(val);\n"
            "  write_data(&out, sizeof(out));\n"
            "}\n"
        )
        nuevo = (
            f"inline void write_{nombre}(detail::Recording& rec, const {tipo} val) {{\n"
            "  const auto out = bswap(val);\n"
            "  write_data(rec, &out, sizeof(out));\n"
            "}\n\n"
            f"inline void write_{nombre}(const {tipo} val) {{\n"
            f"  write_{nombre}(detail::recording(), val);\n"
            "}\n"
        )
        cambiar(h, viejo, nuevo)

    cambiar(
        h,
        "inline void write_u16(detail::Recording& rec, const uint16_t val) {\n",
        "inline void write_u8(const uint8_t val) {\n"
        "  write_u8(detail::recording(), val);\n"
        "}\n\n"
        "inline void write_u16(detail::Recording& rec, const uint16_t val) {\n",
    )

    # Explicit primitives for grouped sequences.
    gx = "lib/dolphin/gx/__gx.h"
    cambiar(
        gx,
        '#include "../../gx/fifo.hpp"\n',
        '#include "../../gx/fifo.hpp"\n\n'
        '#define GX_WRITE_DATA_REC(rec, data, length) aurora::gx::fifo::write_data((rec), (data), (length))\n'
        '#define GX_WRITE_U8_REC(rec, ub) aurora::gx::fifo::write_u8((rec), static_cast<uint8_t>(ub))\n'
        '#define GX_WRITE_U16_REC(rec, us) aurora::gx::fifo::write_u16((rec), static_cast<uint16_t>(us))\n'
        '#define GX_WRITE_U32_REC(rec, ui) aurora::gx::fifo::write_u32((rec), static_cast<uint32_t>(ui))\n'
        '#define GX_WRITE_U64_REC(rec, ui) aurora::gx::fifo::write_u64((rec), static_cast<uint64_t>(ui))\n'
        '#define GX_WRITE_F32_REC(rec, f) aurora::gx::fifo::write_f32((rec), static_cast<float>(f))\n',
    )

    # Preserve exact bodies, including ignored arguments. Anchor at the literal signature and end at the first blank
    # line.
    macros = (
        ("XF_REG", "addr, value"),
        ("XF_REG_2", "addr, value"),
        ("XF_REG_F", "addr, value"),
        ("RAS_REG", "value"),
        ("SOME_REG2", "a, b, c, addr"),
        ("SOME_REG3", "a, b, c, addr"),
        ("SOME_REG4", "a, b, c, addr"),
        ("AURORA", "cmd"),
        ("CP_REG", "addr, value"),
    )
    for nombre, argumentos in macros:
        texto = leer(gx)
        ancla = f"#define GX_WRITE_{nombre}({argumentos})"
        if texto.count(ancla) != 1:
            raise ValueError(
                f"GX call optimization, {gx}: expected one macro {ancla}"
            )
        a = texto.index(ancla)
        b = texto.find("\n\n", a)
        if b < 0:
            raise ValueError(
                f"GX call optimization, {gx}: missing end of {ancla}"
            )

        viejo = texto[a:b]
        interno = uno(
            viejo,
            ancla,
            f"#define GX_WRITE_{nombre}_REC(rec, {argumentos})",
            ancla,
        )
        for primitiva in ("U8", "U16", "U32", "U64", "F32", "DATA"):
            interno = interno.replace(
                f"GX_WRITE_{primitiva}(",
                f"GX_WRITE_{primitiva}_REC(rec, ",
            )

        nuevo = (
            interno + "\n\n" + ancla + " \\\n"
            "  do { \\\n"
            "    auto& gxRec41 = aurora::gx::fifo::detail::recording(); \\\n"
            f"    GX_WRITE_{nombre}_REC(gxRec41, {argumentos}); \\\n"
            "  } while (0)"
        )
        cambiar(gx, viejo, nuevo)

    selector = (
        "  auto& gxRec41 = aurora::gx::fifo::detail::recording();\n"
    )
    nombres = (
        "XF_REG_2", "XF_REG_F", "XF_REG", "RAS_REG",
        "SOME_REG2", "SOME_REG3", "SOME_REG4", "AURORA",
        "CP_REG", "U8", "U16", "U32", "U64", "F32", "DATA",
    )

    def agrupar(archivo, firma, punto, esperadas):
        rel = "lib/dolphin/gx/" + archivo

        def transformar(viejo):
            nuevo = uno(
                viejo,
                punto,
                punto + selector if punto == firma else selector + punto,
                firma + ": seleccion",
            )
            total = 0
            for nombre in nombres:
                ancla = f"GX_WRITE_{nombre}("
                total += viejo.count(ancla)
                nuevo = nuevo.replace(
                    ancla, f"GX_WRITE_{nombre}_REC(gxRec41, "
                )
            if total != esperadas:
                raise ValueError(
                    f"GX call optimization, {firma}: {total} escrituras; "
                    f"se esperaban {esperadas}"
                )
            return nuevo

        funcion(rel, firma, transformar)

    # Dos registros BP por llamada.
    for firma in (
        "void GXSetTevColor(GXTevRegID id, GXColor color) {\n",
        "void GXSetTevColorS10(GXTevRegID id, GXColorS10 color) {\n",
        "void GXSetTevKColor(GXTevKColorID id, GXColor color) {\n",
    ):
        agrupar(
            "GXTev.cpp", firma,
            "  GX_WRITE_RAS_REG(regRA);\n", 2,
        )

    # pre_begin stays outside the selection lifetime.
    agrupar(
        "GXVert.cpp",
        "void GXBegin(GXPrimitive primitive, GXVtxFmt vtxFmt, u16 nVerts) {\n",
        "  const u8 drawCmd = static_cast<u8>(vtxFmt) | static_cast<u8>(primitive);\n",
        5,
    )
    agrupar(
        "GXVert.cpp",
        "void GXBeginIndexed(GXVtxFmt vtxFmt, u16 nVerts, const u16* indices, u32 nIndices) {\n",
        "  sBeginAuto = false;\n",
        4,
    )
    funcion(
        "lib/dolphin/gx/GXVert.cpp",
        "void GXBeginIndexed(GXVtxFmt vtxFmt, u16 nVerts, const u16* indices, u32 nIndices) {\n",
        lambda t: uno(
            t,
            "  aurora::gx::fifo::write_data(indices, nIndices * sizeof(u16));\n",
            "  aurora::gx::fifo::write_data(gxRec41, indices, nIndices * sizeof(u16));\n",
            "GXBeginIndexed: indices",
        ),
    )

    # Short attribute sequences; retain nothing between calls.
    for firma, cuenta in (
        ("void GXPosition3f32(f32 x, f32 y, f32 z) {\n", 3),
        ("void GXNormal3f32(f32 x, f32 y, f32 z) {\n", 3),
        ("void GXColor4u8(u8 r, u8 g, u8 b, u8 a) {\n", 4),
        ("void GXTexCoord2f32(f32 s, f32 t) {\n", 2),
    ):
        agrupar("GXVert.cpp", firma, firma, cuenta)

    # XF header and data share one selection.
    for firma, cuenta in (
        (
            "void GXSetProjection(const void* mtx_, GXProjectionType type) {\n",
            9,
        ),
        ("void GXSetProjectionv(const f32* ptr) {\n", 4),
        ("void GXLoadPosMtxImm(const void* mtx_, u32 id) {\n", 3),
        ("void GXLoadNrmMtxImm(const void* mtx_, u32 id) {\n", 11),
        (
            "void GXLoadTexMtxImm(const void* mtx_, u32 id, GXTexMtxType type) {\n",
            3,
        ),
        (
            "void GXSetViewportJitter(float left, float top, float width, float height, float nearZ, float farZ, u32 field) {\n",
            8,
        ),
    ):
        agrupar(
            "GXTransform.cpp", firma,
            "  GX_WRITE_U8(0x10);\n", cuenta,
        )

    agrupar(
        "GXTransform.cpp",
        "void GXLoadPosMtxIndx(u16 mtxIndx, u32 id) {\n",
        "  GX_WRITE_U8(GX_LOAD_INDX_A);\n",
        3,
    )
    agrupar(
        "GXLighting.cpp",
        "void GXLoadLightObjImm(GXLightObj* light_, GXLightID id) {\n",
        "  GX_WRITE_U8(0x10);\n",
        18,
    )
    agrupar(
        "GXGeometry.cpp",
        "void GXSetArray(GXAttr attr, const void* data, u32 size, u8 stride, bool le) {\n",
        "  GX_WRITE_AURORA(GX_AURORA_LOAD_ARRAYBASE | cpIdx);\n",
        5,
    )
    agrupar(
        "GXManage.cpp",
        "void __GXSetVCD() {\n",
        "  GX_WRITE_SOME_REG4(8, 0x50, __gx->vcdLo, -12);\n",
        3,
    )
    agrupar(
        "GXManage.cpp",
        "void __GXSetVAT() {\n",
        "  for (u8 i = 0; i < 8; i++) {\n",
        3,
    )
    agrupar(
        "GXManage.cpp",
        "void __GXSetMatrixIndex(GXAttr matIdxAttr) {\n",
        "  if (matIdxAttr < GX_VA_TEX4MTXIDX) {\n",
        4,
    )

    # The header and initial measurement share the selection. GXEnd selects again through the no-argument wrapper.
    v = "lib/dolphin/gx/GXVert.cpp"
    cambiar(
        v,
        "u32 write_pos() {\n"
        "  return aurora::gx::fifo::in_display_list() ? aurora::gx::fifo::display_list_pos()\n"
        "                                             : aurora::gx::fifo::get_buffer_size();\n"
        "}\n",
        "u32 write_pos(aurora::gx::fifo::detail::Recording& rec) {\n"
        "  return rec.active ? rec.pos : aurora::gx::fifo::get_buffer_size();\n"
        "}\n\n"
        "u32 write_pos() {\n"
        "  return write_pos(aurora::gx::fifo::detail::recording());\n"
        "}\n",
    )
    cambiar(
        v,
        "void post_begin(u16 nVerts) {\n"
        "  sBeginNVerts = nVerts;\n"
        "  sBeginFifoSize = write_pos();\n",
        "void post_begin(aurora::gx::fifo::detail::Recording& rec, u16 nVerts) {\n"
        "  sBeginNVerts = nVerts;\n"
        "  sBeginFifoSize = write_pos(rec);\n",
    )
    for firma in (
        "void GXBegin(GXPrimitive primitive, GXVtxFmt vtxFmt, u16 nVerts) {\n",
        "void GXBeginIndexed(GXVtxFmt vtxFmt, u16 nVerts, const u16* indices, u32 nIndices) {\n",
    ):
        funcion(
            v, firma,
            lambda t: uno(
                t,
                "  post_begin(nVerts);\n",
                "  post_begin(gxRec41, nVerts);\n",
                "header: post_begin",
            ),
        )
    funcion(
        v,
        "void GXBegin(GXPrimitive primitive, GXVtxFmt vtxFmt, u16 nVerts) {\n",
        lambda t: uno(
            t,
            "!aurora::gx::fifo::in_display_list(),",
            "!gxRec41.active,",
            "GXBegin: GX_AUTO",
        ),
    )

    # Preserve target checking and synchronization.
    f = "lib/gx/fifo.cpp"
    viejo = (
        "void publish() noexcept {\n"
        "  if (!sFrameActive || kProcessingMode == ProcessingMode::Drain || detail::recording().active) {\n"
        "    return;\n"
        "  }\n\n"
        "  const uint64_t target = sStreamBase + detail::sBufferSize;\n"
    )
    nuevo = (
        "static void publish_checked() noexcept {\n"
        "  const uint64_t target = sStreamBase + detail::sBufferSize;\n"
    )
    cambiar(f, viejo, nuevo)

    cambiar(
        f,
        "DrawDoneCallback set_draw_done_callback(DrawDoneCallback callback) noexcept {\n",
        "void publish() noexcept {\n"
        "  if (!sFrameActive || kProcessingMode == ProcessingMode::Drain || detail::recording().active) {\n"
        "    return;\n"
        "  }\n"
        "  publish_checked();\n"
        "}\n\n"
        "DrawDoneCallback set_draw_done_callback(DrawDoneCallback callback) noexcept {\n",
    )
    funcion(
        f,
        "void finish_draw() noexcept {\n",
        lambda t: uno(
            t,
            "    publish();\n",
            "    publish_checked();\n",
            "finish_draw: publicacion",
        ),
    )

    return textos


def transformaciones_45(destino):
    """Alternate each single-sample EFB's physical color while preserving logical identity."""
    textos = {}

    def cambiar(rel, viejo, nuevo):
        texto = textos.get(rel)
        if texto is None:
            texto = (destino / rel).read_text(encoding="utf-8")
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"EFB alternation, {rel}: {n} matches; expected one: {viejo!r}")
        textos[rel] = texto.replace(viejo, nuevo, 1)

    h = "lib/gfx/efb_blend6.hpp"
    cambiar(h, "namespace aurora::gfx::detail {\n", """// Native boundary: returned physical views are immutable references;
// publication changes only the logical version for operations encoded later.
namespace lmnx::gpu {
Texture blend6TexturaActual(const Texture& textura);
TextureView blend6VistaActual(const TextureView& vista);
TextureView blend6Alternativa(const Device& dispositivo, const Texture& textura, const TextureView& vista);
void blend6Publicar(const Texture& textura);
}

namespace aurora::gfx::detail {
""")

    cpp = "lib/gfx/efb_blend6.cpp"
    cambiar(cpp, "#include <cstdlib>\n", "#include <cstdlib>\n#include <cstdio>\n#include <cstring>\n")
    cambiar(cpp, "Scratch g_scratch;\n", """Scratch g_scratch;

// Select once at startup to compare both paths without recompiling.
bool usar_copia() {
  static const bool copia = [] {
#ifdef __SWITCH__
    if (auto* flag = std::fopen("sdmc:/switch/ProjectLM-NX/blend6_copia.flag", "rb")) {
      std::fclose(flag);
      return true;
    }
    return false;
#else
    const auto* valor = std::getenv("LM_BLEND6_COPIA");
    return valor != nullptr && std::strcmp(valor, "1") == 0;
#endif
  }();
  return copia;
}
""")
    cambiar(cpp, """  const auto& scratch = scratch_for(color.size, color.format, samples);
  const Pipeline quantize = pipeline_for({color.format, samples, static_cast<uint8_t>((flags & 2) != 0 ? 2 : 1)});
  const Pipeline copyBack = pipeline_for({color.format, samples, 0});
""", """  const bool alternar = samples == 1 && !usar_copia() && pass.copySourceTexture &&
      pass.copySourceView.Get() == color.view.Get();
  // The alternate surface belongs to the logical EFB, never the global copy/MSAA scratch surface.
  const auto scratch = alternar ? Scratch{} : scratch_for(color.size, color.format, samples);
  const Pipeline quantize = pipeline_for({color.format, samples, static_cast<uint8_t>((flags & 2) != 0 ? 2 : 1)});
  const Pipeline copyBack = alternar ? Pipeline{} : pipeline_for({color.format, samples, 0});
""")
    cambiar(cpp, """  if (samples == 1 && pass.copySourceTexture && pass.copySourceView.Get() == color.view.Get()) {
    // Single-sampled target: a plain texture copy into the scratch, then one quantizing pass back.
""", """  if (alternar) {
    const auto alternativa = lmnx::gpu::blend6Alternativa(g_device, pass.copySourceTexture, color.view);
    const auto actual = lmnx::gpu::blend6VistaActual(color.view);
    if (timing) {
      tw[0].endOfPassWriteIndex = 1;
    }
    // Same fullscreen quantization and dithering: A -> B or B -> A without a preceding copy.
    runPass("EFB blend6 quantize", alternativa, {}, quantize, actual, timing ? &tw[0] : nullptr);
    lmnx::gpu::blend6Publicar(pass.copySourceTexture);
  } else if (samples == 1 && pass.copySourceTexture && pass.copySourceView.Get() == color.view.Get()) {
    // Single-sampled target: a plain texture copy into the scratch, then one quantizing pass back.
""")
    cambiar(cpp, "void initialize() {\n", """void initialize() {
  Log.info("[blend6] single sample: {}", usar_copia() ? "copy path" : "surface alternation");
""")
    cambiar(cpp, """// A single-sampled target is copied into a scratch texture and quantized back in one full-screen
// pass. A multisampled one is quantized per sample into the scratch, then copied back by a second
// pass that also redoes the MSAA resolve.
""", """// A single-sample EFB alternates its two owned surfaces; each full quantization
// reads the current surface, writes the alternate and publishes it. The guard restores copying to scratch.
// MSAA keeps per-sample quantization to scratch and the second resolve pass.
""")
    return textos


def transformaciones_46(destino):
    """Precompute the native-readback hash and label conversions in profiling."""
    textos = {}

    def cambiar(rel, viejo, nuevo):
        texto = textos.get(rel)
        if texto is None:
            texto = (destino / rel).read_text(encoding="utf-8")
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"Readback optimization, {rel}: {n} matches; expected one: {viejo!r}")
        textos[rel] = texto.replace(viejo, nuevo, 1)

    ram = "lib/gfx/efb_ram.cpp"
    cambiar(ram, '#include "recording.hpp"\n',
            '#include "recording.hpp"\n#include "render_worker.hpp"\n')
    cambiar(ram, "  int mapStatus = 0;          // set by the map callback: 1 mapped, 2 failed (0219)\n",
            """  int mapStatus = 0;          // set by the map callback: 1 mapped, 2 failed (0219)
  // Prepared on the render thread and published through g_landedMutex.
  const uint8_t* mappedData = nullptr;
  HashType dataHash = 0;
""")
    cambiar(ram, "std::mutex g_mutex;\n", """// Native producer only: earlier Dawn restrictions do not apply.
// Buffer::map/completarEsperas call without device locks. The callback delegates
// mapping and XXH to the render worker before publishing g_mapped; finish_maps may
// run on the game thread but only collects the prepared pointer and hash.
std::mutex g_mutex;
""")
    cambiar(ram, """            // Spontaneous: runs inside whatever Dawn call noticed the map, on any thread. No Dawn
            // call here (0219, see the lock order notes) and only the leaf lock g_landedMutex.
""", """            // The callback may arrive from another thread; delegate native preparation below.
""")
    cambiar(ram, """            std::lock_guard lock{g_landedMutex};
            g_mapped.push_back(rb);
""", """            // Native producer only: Buffer::map calls without device locks.
            // enqueue_work runs inline on the render thread or queues work from other threads.
            // Hold no EFB locks while queueing, mapping or hashing.
            render_worker::enqueue_work([rb] {
              if (rb->mapStatus == 1 && rb->buffer) {
                rb->mappedData = static_cast<const uint8_t*>(
                    rb->buffer.GetConstMappedRange(0, align4(rb->size)));
                if (rb->mappedData != nullptr) {
                  // GetConstMappedRange includes armDCacheFlush for cached Switch readbacks.
                  // Exactly size bytes without align4 padding; do not touch g_stats here.
                  rb->dataHash = xxh3_hash_s(rb->mappedData, rb->size);
                }
              }
              std::lock_guard lock{g_landedMutex};
              g_mapped.push_back(rb);
            });
""")
    cambiar(ram,
            "      const auto* p = static_cast<const uint8_t*>(rb->buffer.GetConstMappedRange(0, align4(rb->size)));\n",
            "      const auto* p = rb->mappedData; // Ya mapeado, sincronizado y hasheado en render.\n")
    cambiar(ram,
            "        const aurora::HashType h = hash_range(dest, rb->size);\n",
            """        const aurora::HashType h = rb->dataHash;
        // Keep existing accounting under g_mutex without another scan or a g_stats race.
        g_stats.hashedBytes += rb->size;
""")
    # Native preparation later applies transformaciones_44, retaining mappedData through AppliedReadback. Preserve
    # its vector, assign and Unmap anchors; this transformation is also validated independently.

    conv = "lib/gfx/tex_copy_conv.cpp"
    cambiar(conv,
            "static void execute(const wgpu::CommandEncoder& cmd, const ConvRequest& req, const wgpu::RenderPipeline& pipeline) {\n",
            """static const char* etiqueta_conversion(GXTexFmt fmt) {
  // Tables already describe the GX format; depth also needs the destination view.
  switch (fmt) {
  case GX_TF_Z8: return "TexCopyConv Z8→I8";
  case GX_TF_Z16: return "TexCopyConv Z16→IA8";
  default:
    for (const auto& conv : ConvPipelines) {
      if (conv.fmt == fmt) return conv.label;
    }
    return "TexCopyConv unknown format";
  }
}

static void execute(const wgpu::CommandEncoder& cmd, const ConvRequest& req,
                    const wgpu::RenderPipeline& pipeline, const char* etiqueta) {
""")
    cambiar(conv, '      .label = "TexCopyConv Pass",\n', '      .label = etiqueta,\n')
    # Apply this before preparar_nativo changes webgpu to nativegpu.
    cambiar(conv, '.timestampWrites = webgpu::gpu_prof::pass_writes("EFB copy convert"),',
            '.timestampWrites = webgpu::gpu_prof::pass_writes(etiqueta),')
    cambiar(conv, '      execute(cmd, req, ms->second);\n',
            '      execute(cmd, req, ms->second, etiqueta_conversion(req.fmt));\n')
    cambiar(conv, '  execute(cmd, req, it->second);\n',
            '  execute(cmd, req, it->second, etiqueta_conversion(req.fmt));\n')
    cambiar(conv,
            'void blit(const wgpu::CommandEncoder& cmd, const ConvRequest& req) { execute(cmd, req, g_blitPipeline); }\n',
            'void blit(const wgpu::CommandEncoder& cmd, const ConvRequest& req) {\n'
            '  execute(cmd, req, g_blitPipeline, "TexCopyConv escalado");\n}\n')
    return textos


def transformaciones_47(destino):
    """Early-Z with dual-source alpha rejection in one pass."""
    textos = {}

    def cambiar(rel, viejo, nuevo):
        texto = textos.get(rel)
        if texto is None:
            texto = (destino / rel).read_text(encoding="utf-8")
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"Early-Z, {rel}: {n} matches; expected one: {viejo!r}")
        textos[rel] = texto.replace(viejo, nuevo, 1)

    h = "lib/gx/gx.hpp"
    cambiar(h, "enum class DstAlphaMode : u8 { None, Replace, DualSource };",
            "enum class DstAlphaMode : u8 { None, Replace, DualSource, AlphaKeep };")
    # Reuse a reserved bit to preserve old configuration sizes and keys.
    cambiar(h, "  u8 pad0 : 1 = 0;", "  u8 earlyZKeep : 1 = 0; // Secondary alpha preserves the destination on rejection.")

    cp = "lib/gx/command_processor.cpp"
    cambiar(cp, "static bool early_z_merge() noexcept {", """static bool early_z_doble() noexcept {
  static const bool doble = [] {
#ifdef __SWITCH__
    if (auto* flag = std::fopen("sdmc:/switch/ProjectLM-NX/earlyz_doble.flag", "rb")) {
      std::fclose(flag);
      return true;
    }
    return false;
#else
    const char* valor = std::getenv("LM_EARLYZ_DOBLE");
    return valor != nullptr && valor[0] == '1' && valor[1] == '\\0';
#endif
  }();
  return doble;
}
static bool early_z_alpha_keep(const PipelineConfig& pc) noexcept {
  // Exact replacement only. Alpha blending requires checking EFB quantization.
  const auto& sc = pc.shaderConfig;
  return !early_z_doble() && nativegpu::g_dualSourceBlendingSupported &&
      gfx::get_render_target_layout().colorAttachmentCount == 1 &&
      pc.blendMode == GX_BM_NONE && pc.dstAlpha == UINT32_MAX &&
      sc.logicOut == 0 && sc.zTexOp == 0 && !sc.depthOnly && !sc.lightDepth &&
      g_gxState.fadeQ == 0;
}
static bool early_z_merge() noexcept {""")
    cambiar(cp, "  bool splitValid = false;", "  bool splitValid = false;\n  bool keepEligible = false;\n  bool alphaKeep = false;")
    cambiar(cp, "  if (cache.splitValid && cache.splitFor == cache.pipelineRef && cache.splitFlags == flags &&",
            "  const bool keep = early_z_alpha_keep(cache.config);\n"
            "  if (cache.splitValid && cache.keepEligible == keep && cache.splitFor == cache.pipelineRef && cache.splitFlags == flags &&")
    cambiar(cp, "  cache.splitValid = true;", "  cache.splitValid = true;\n  cache.keepEligible = keep;\n  cache.alphaKeep = keep;")
    cambiar(cp, "  PipelineConfig depthOnly, color;\n  split_variants_impl(cache.config, depthOnly, color);", """  if (keep) {
    auto single = cache.config;
    single.shaderConfig.earlyZKeep = 1;
    cache.splitColor = gfx::pipeline_ref(single);
    cache.split = false; // Not a pair; canMerge still extends the normal draw.
    return false;
  }
  PipelineConfig depthOnly, color;
  split_variants_impl(cache.config, depthOnly, color);""")
    cambiar(cp, "  const DrawData drawData{", "  DrawData drawData{")
    cambiar(cp, "  gfx::push_draw_command(drawData);\n}", """  if (cache.alphaKeep) {
    drawData.pipeline = cache.splitColor;
  }
  gfx::push_draw_command(drawData);
}""")
    # alphaKeep marks eligibility; keep it active only if early-Z rejection was present.
    cambiar(cp, "  if (!cache.split) {\n    return false;\n  }",
            "  if (!cache.split) {\n    cache.alphaKeep = false;\n    return false;\n  }")

    p = "lib/gx/pipeline.cpp"
    cambiar(p, "  pipeline.main = create(options, \"main\");", """  if (config.shaderConfig.earlyZKeep) {
    options.dstAlphaMode = DstAlphaMode::AlphaKeep;
  }
  pipeline.main = create(options, "main");""")
    gx = "lib/gx/gx.cpp"
    cambiar(gx, "  const auto blendState = to_blend_state(config.blendMode, config.blendFacSrc, config.blendFacDst, config.blendOp,",
            "  auto blendState = to_blend_state(config.blendMode, config.blendFacSrc, config.blendFacDst, config.blendOp,")
    cambiar(gx, "  std::array<wgpu::ColorTargetState, gfx::MaxColorAttachments> colorTargets{};", """  if (config.shaderConfig.earlyZKeep) {
    blendState.color = {
        .operation = wgpu::BlendOperation::Add,
        .srcFactor = wgpu::BlendFactor::Src1Alpha,
        .dstFactor = wgpu::BlendFactor::OneMinusSrc1Alpha,
    };
    blendState.alpha = blendState.color;
  }
  std::array<wgpu::ColorTargetState, gfx::MaxColorAttachments> colorTargets{};""")

    s = "lib/gx/shader.cpp"
    cambiar(s, "    if (discard.constant == 1) {", """    if (config.earlyZKeep) {
      if (pass.constant >= 0) {
        fragmentFn += fmt::format("\\n    let alpha_keep = {}.0;", pass.constant);
      } else {
        fragmentFn += "\\n    let alphaCompare = u32(round(clamp(prev.a, 0.0, 1.0) * 255.0));";
        fragmentFn += fmt::format("\\n    let alpha_keep = select(0.0, 1.0, {});", pass.expr);
      }
    } else if (discard.constant == 1) {""")
    cambiar(s, "  // GX_AURORA_SET_FADE (frame interpolation): drop q/255 of the pixels in a 4x4 ordered dither.\n  fragmentFn +=",
            "  // Select the no-discard variant only with fadeQ == 0.\n  if (!config.earlyZKeep) {\n  // GX_AURORA_SET_FADE (frame interpolation): drop q/255 of the pixels in a 4x4 ordered dither.\n  fragmentFn +=")
    cambiar(s, "  std::string fragmentOutput;", "  }\n\n  std::string fragmentOutput;")
    cambiar(s, "  if (dstAlphaMode == DstAlphaMode::DualSource) {\n    fragmentOutput =",
            "  if (dstAlphaMode == DstAlphaMode::DualSource || config.earlyZKeep) {\n    fragmentOutput =")
    cambiar(s, '    fragmentReturn = "\\n    return FragmentOutput(vec4f(prev.rgb, 1.0), prev);";',
            '    fragmentReturn = config.earlyZKeep\n'
            '        ? "\\n    return FragmentOutput(prev, vec4f(0.0, 0.0, 0.0, alpha_keep));"\n'
            '        : "\\n    return FragmentOutput(vec4f(prev.rgb, 1.0), prev);";')
    cambiar(s, '  if (dstAlphaMode == DstAlphaMode::DualSource) {\n    shaderSource.insert',
            '  if (dstAlphaMode == DstAlphaMode::DualSource || config.earlyZKeep) {\n    shaderSource.insert')
    c = "lib/gfx/pipeline_cache.cpp"
    cambiar(c, "  if (s.logicOut > 2 || s.pad0 != 0) {", "  if (s.logicOut > 2) {")
    cambiar(c, "  switch (s.fogType) {", """  if (s.earlyZKeep && (c.blendMode != GX_BM_NONE || c.dstAlpha != UINT32_MAX ||
      !c.depthCompare || !c.depthUpdate || !s.alphaCompare || s.lineMode != 0 ||
      s.wireframe || s.logicOut != 0 || s.zTexOp != 0 || s.depthOnly || s.lightDepth)) {
    return "early-Z alpha keep";
  }
  switch (s.fogType) {""")
    return textos


def transformaciones_48(destino):
    """RGB565: exact WGSL specialization and removal of redundant clearing."""
    textos = {}

    def leer(rel):
        if rel not in textos:
            textos[rel] = (destino / rel).read_text(encoding="utf-8")
        return textos[rel]

    def uno(texto, viejo, nuevo, etiqueta):
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"Texture copy, {etiqueta}: {n} matches; expected one: {viejo!r}")
        return texto.replace(viejo, nuevo, 1)

    def cambiar(rel, viejo, nuevo):
        textos[rel] = uno(leer(rel), viejo, nuevo, rel)

    conv = "lib/gfx/tex_copy_conv.cpp"
    original = leer(conv)

    def wgsl(nombre):
        patron = rf'static constexpr std::string_view {nombre} = R"\((.*?)\)"sv;'
        coincidencias = re.findall(patron, original, re.S)
        if len(coincidencias) != 1:
            raise ValueError(f"Texture copy: expected one WGSL block for {nombre}")
        return coincidencias[0]

    # Copy original expressions verbatim without reassociating smooth8 or its rounding.
    preambulo = wgsl("ShaderPreamble")
    inicio = "    if (ubuf.filterTaps != 0u) {\n"
    final = "\n    }\n    return vec4f(c.rgb, select(c.a, 1.0, ubuf.opaqueAlpha != 0u));"
    if preambulo.count(inicio) != 1 or preambulo.count(final) != 1:
        raise ValueError("Texture copy: vertical filter block changed")
    a = preambulo.index(inicio)
    b = preambulo.index(final, a)
    filtro = preambulo[a:b + len("\n    }")]
    declaraciones = []
    for filtrado in (0, 1):
        # With filtering, retain an unconditional scope; without it, remove both neighbors.
        cuerpo = filtro.replace(inicio, "    {\n", 1) if filtrado else ""
        fuente = uno(preambulo, filtro, cuerpo, "filtro WGSL")
        declaraciones.append(f'static constexpr std::string_view RGB565Preamble{filtrado} = R"({fuente})"sv;\n')
    fragmento = wgsl("FragRGB565")
    for q, funcion in ((0, None), (1, "bits8"), (2, "smooth8")):
        expresion = ("vec4f(c.rgb, 1.0)" if funcion is None else
                     f"vec4f({funcion}(c.r, 5u), {funcion}(c.g, 6u), {funcion}(c.b, 5u), 1.0)")
        if fragmento.count("return " + expresion + ";") != 1:
            raise ValueError(f"Texture copy: RGB565 result changed {q}")
        declaraciones.append(f'''static constexpr std::string_view RGB565Frag{q} = R"(
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {{
    let c = sample_efb(in.uv);
    return {expresion};
}}
)"sv;
''')
    cambiar(conv, "#include <absl/container/flat_hash_map.h>\n",
            "#include <absl/container/flat_hash_map.h>\n#include <cstdio>\n#include <cstdlib>\n")
    cambiar(conv, "static wgpu::RenderPipeline g_blitPipeline;", """// Index quantize * 2 + filter. The guard preserves the previous source and identity.
static std::array<wgpu::RenderPipeline, 6> g_rgb565Pipelines;
static bool texcopy_antes() noexcept {
  static const bool antes = [] {
#ifdef __SWITCH__
    if (auto* flag = std::fopen("sdmc:/switch/ProjectLM-NX/texcopy_antes.flag", "rb")) {
      std::fclose(flag);
      return true;
    }
    return false;
#else
    const char* valor = std::getenv("LM_TEXCOPY_ANTES");
    return valor != nullptr && valor[0] == '1' && valor[1] == '\\0';
#endif
  }();
  return antes;
}
static wgpu::RenderPipeline g_blitPipeline;""")
    cambiar(conv, "bool needs_conversion(const GXTexFmt fmt)",
            "// Variants generated from the original WGSL.\n" +
            "\n".join(declaraciones) + "\nbool needs_conversion(const GXTexFmt fmt)")
    cambiar(conv, "\n  // Skip depth copies in compatibility mode\n", """
  Log.info("[texcopy] RGB565: {}", texcopy_antes() ? "previous path" : "specialized without clear");
  if (!texcopy_antes()) {
    constexpr std::array preambulos{RGB565Preamble0, RGB565Preamble1};
    constexpr std::array fragmentos{RGB565Frag0, RGB565Frag1, RGB565Frag2};
    for (uint32_t q = 0; q < fragmentos.size(); ++q) {
      for (uint32_t f = 0; f < preambulos.size(); ++f) {
        g_rgb565Pipelines[q * 2 + f] = create_pipeline(
            {GX_TF_RGB565, fragmentos[q], wgpu::TextureFormat::RGBA8Unorm, "TexCopyConv RGB565"},
            preambulos[f], g_bindGroupLayout);
      }
    }
  }
  // Skip depth copies in compatibility mode
""")
    cambiar(conv, "  g_pipelines.clear();", "  g_rgb565Pipelines = {};\n  g_pipelines.clear();")
    # Only execute; snapshot_depth keeps its Clear. A fullscreen triangle without depth, blending or discard
    # overwrites every destination component before use.
    cambiar(conv, """          .view = req.dst->attachmentTextureView,
          .loadOp = wgpu::LoadOp::Clear,""", """          .view = req.dst->attachmentTextureView,
          .loadOp = req.fmt == GX_TF_RGB565 && !texcopy_antes() ? wgpu::LoadOp::Load : wgpu::LoadOp::Clear,""")
    cambiar(conv, "void run(const wgpu::CommandEncoder& cmd, const ConvRequest& req) {\n", """void run(const wgpu::CommandEncoder& cmd, const ConvRequest& req) {
  if (req.fmt == GX_TF_RGB565 && !texcopy_antes() && req.quantize <= 2) {
    const uint32_t variante = req.quantize * 2 + uint32_t(req.filtered);
    static std::array<bool, 6> anunciada{}; // Only the render worker calls run.
    if (!anunciada[variante]) {
      anunciada[variante] = true;
      Log.info("[texcopy] variant q={} filter={} samples={} destination={}x{}",
               req.quantize, req.filtered, req.sampleFilter == SampleFilter::Linear ? "lineal" : "cercano",
               req.dst->size.width, req.dst->size.height);
    }
    execute(cmd, req, g_rgb565Pipelines[variante], etiqueta_conversion(req.fmt));
    return;
  }
""")
    cambiar("lib/gfx/tex_copy_conv.hpp", "  uint32_t srcSamples = 1;", """  // Metadata from the same uniforms, captured while recording, never from live GX state.
  uint32_t quantize = 0;
  bool filtered = false;
  uint32_t srcSamples = 1;""")
    cambiar("lib/gfx/frame_packet.hpp", "  Range resolveUniformRange;",
            "  Range resolveUniformRange;\n  uint32_t resolveQuantize = 0; // Mirrors the copy uniform.")
    cambiar("lib/gfx/recording.cpp", "  prevPass.resolveFiltered = uniforms.filterTaps != 0;",
            "  prevPass.resolveQuantize = uniforms.quantize;\n  prevPass.resolveFiltered = uniforms.filterTaps != 0;")
    cambiar("lib/gfx/encoding.cpp", "        .srcSamples = isDepth ? passInfo.msaaSamples : 1u,",
            "        .quantize = passInfo.resolveQuantize,\n        .filtered = passInfo.resolveFiltered,\n"
            "        .srcSamples = isDepth ? passInfo.msaaSamples : 1u,")
    return textos


def transformaciones_49(destino):
    """Exact single pass with secondary factors and rejection diagnostics."""
    textos = {}

    def cambiar(rel, viejo, nuevo):
        texto = textos.get(rel)
        if texto is None:
            texto = (destino / rel).read_text(encoding="utf-8")
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"Early-Z patch, {rel}: {n} matches: {viejo[:100]!r}")
        textos[rel] = texto.replace(viejo, nuevo, 1)

    # The mode already belongs to all WGSL module keys. Preserve the binary format of ShaderConfig and existing
    # pipeline lists.
    cambiar("lib/gx/gx.hpp",
            "enum class DstAlphaMode : u8 { None, Replace, DualSource, AlphaKeep };",
            "enum class DstAlphaMode : u8 { None, Replace, DualSource, AlphaKeep, AlphaKeepSrcAlpha, AlphaKeepDst, AlphaKeepSrcAlphaDst };")
    cambiar("lib/gx/pipeline.hpp", "struct PipelineOptions {", """// Only combinations with exact destination behavior for both k=0 and k=1.
inline bool early_z_blend_exact(const PipelineConfig& pc) noexcept {
  return pc.blendMode == GX_BM_NONE ||
      (pc.blendMode == GX_BM_BLEND &&
       (pc.blendFacSrc == GX_BL_ONE || pc.blendFacSrc == GX_BL_SRCALPHA) &&
       (pc.blendFacDst == GX_BL_ONE ||
        (pc.blendFacDst == GX_BL_INVSRCALPHA && pc.blendFacSrc == GX_BL_SRCALPHA) ||
        (pc.blendFacDst == GX_BL_ZERO && pc.blendFacSrc == GX_BL_ONE)));
}

struct PipelineOptions {""")
    cp = "lib/gx/command_processor.cpp"
    texto = (destino / cp).read_text(encoding="utf-8")
    inicio = texto.index("static bool early_z_alpha_keep(")
    fin = texto.index("static bool early_z_merge()", inicio)
    cambiar(cp, texto[inicio:fin], """static bool early_z_alpha_keep(const PipelineConfig& pc) noexcept {
  const auto& sc = pc.shaderConfig;
  const auto count = gfx::get_render_target_layout().colorAttachmentCount;
  const bool blendExact = count == 1 ? early_z_blend_exact(pc)
      : count > 1 && pc.blendMode == GX_BM_NONE && pc.dstAlpha == UINT32_MAX && !pc.alphaUpdate;
  return !early_z_doble() && blendExact &&
      (count != 1 || nativegpu::g_dualSourceBlendingSupported) &&
      sc.logicOut == 0 && sc.zTexOp == 0 && !sc.depthOnly && !sc.lightDepth && g_gxState.fadeQ == 0;
}
""")
    inicio = texto.index("  {\n    // LMNX: how many pipelines")
    fin = texto.index("  cache.splitFor =", inicio)
    cambiar(cp, texto[inicio:fin], """  {
    // Accumulate eligible split pipeline changes; rejection reasons may overlap.
    static u64 checked = 0, kept = 0, blend = 0, dstAlpha = 0, logic = 0, ztex = 0;
    static u64 depth = 0, light = 0, fade = 0, dual = 0, doble = 0, mrt = 0, dstActive = 0;
    static u64 factors[4][8][8]{};
    if (early_z_alpha_split(cache.config)) {
      const auto& pc = cache.config;
      const auto& sc = pc.shaderConfig;
      const auto count = gfx::get_render_target_layout().colorAttachmentCount;
      ++checked;
      kept += keep;
      dstActive += pc.dstAlpha != UINT32_MAX;
      if (!keep) {
        blend += count == 1 ? !early_z_blend_exact(pc) : pc.blendMode != GX_BM_NONE;
        dstAlpha += count > 1 && pc.dstAlpha != UINT32_MAX;
        logic += sc.logicOut != 0;
        ztex += sc.zTexOp != 0;
        depth += sc.depthOnly;
        light += sc.lightDepth;
        fade += g_gxState.fadeQ != 0;
        dual += count == 1 && !nativegpu::g_dualSourceBlendingSupported;
        doble += early_z_doble();
        mrt += count == 0 || (count > 1 && pc.alphaUpdate);
        if (u32(pc.blendMode) < 4 && u32(pc.blendFacSrc) < 8 && u32(pc.blendFacDst) < 8) {
          ++factors[u32(pc.blendMode)][u32(pc.blendFacSrc)][u32(pc.blendFacDst)];
        }
      }
      if (checked % 500 == 0) {
        Log.info("[early-z 49] single={} eligible={} split={} targets={} overlapping reasons: blend={} dstAlpha={} logicOut={} zTex={} depthOnly={} lightDepth={} fade={} dual-source={} doble={} MRT/alphaUpdate={} dstAlpha_activo={}",
                 kept, checked, checked - kept, count, blend, dstAlpha, logic, ztex, depth, light, fade, dual, doble, mrt, dstActive);
        for (u32 m = 0; m < 4; ++m) for (u32 s = 0; s < 8; ++s) for (u32 d = 0; d < 8; ++d) {
          if (factors[m][s][d]) {
            Log.info("[early-z 49] split blend mode={} src={} dst={} changes={}", m, s, d, factors[m][s][d]);
          }
        }
      }
    }
  }
""")
    # Bits 24..31 were unused. Per-draw data avoids specializing for 256 constants and survives dstAlpha
    # canonicalization to 0xff.
    cambiar(cp, ".currentPnMtx = state.currentPnMtx | u32(state.fadeQ) << 8 | smoothBits};",
            ".currentPnMtx = state.currentPnMtx | u32(state.fadeQ) << 8 | smoothBits |\n"
            "                                   ((state.dstAlpha & 0xFFu) << 24)};")
    p = "lib/gx/pipeline.cpp"
    cambiar(p, "  if (config.alphaUpdate && config.dstAlpha != UINT32_MAX && config.dstAlpha != 0) {",
            "  if (!config.shaderConfig.earlyZKeep && config.alphaUpdate && config.dstAlpha != UINT32_MAX && config.dstAlpha != 0) {")
    cambiar(p, "    options.dstAlphaMode = DstAlphaMode::AlphaKeep;", """    const bool srcAlpha = config.blendMode == GX_BM_BLEND && config.blendFacSrc == GX_BL_SRCALPHA;
    options.dstAlphaMode = config.dstAlpha != UINT32_MAX
        ? (srcAlpha ? DstAlphaMode::AlphaKeepSrcAlphaDst : DstAlphaMode::AlphaKeepDst)
        : (srcAlpha ? DstAlphaMode::AlphaKeepSrcAlpha : DstAlphaMode::AlphaKeep);""")
    gx = "lib/gx/gx.cpp"
    cambiar(gx, """    blendState.alpha = blendState.color;
    if (layout.colorAttachmentCount > 1) {""", """    // RGB and alpha have independent factors when dstAlpha replaces alpha.
    const bool additive = config.blendMode == GX_BM_BLEND && config.blendFacDst == GX_BL_ONE;
    blendState.color.srcFactor = wgpu::BlendFactor::Src1;
    blendState.color.dstFactor = additive ? wgpu::BlendFactor::One : wgpu::BlendFactor::OneMinusSrc1;
    blendState.alpha = {
        .operation = wgpu::BlendOperation::Add,
        .srcFactor = wgpu::BlendFactor::Src1Alpha,
        .dstFactor = config.dstAlpha != UINT32_MAX ? wgpu::BlendFactor::OneMinusSrc1Alpha
            : additive ? wgpu::BlendFactor::One : wgpu::BlendFactor::OneMinusSrc1Alpha,
    };
    if (layout.colorAttachmentCount > 1) {""")
    s = "lib/gx/shader.cpp"
    cambiar(s, """    fragmentReturn = config.earlyZKeep
        ? "\\n    return FragmentOutput(prev, vec4f(0.0, 0.0, 0.0, alpha_keep));"
        : "\\n    return FragmentOutput(vec4f(prev.rgb, 1.0), prev);";""", """    fragmentReturn = "\\n    return FragmentOutput(vec4f(prev.rgb, 1.0), prev);";
    if (config.earlyZKeep) {
      const bool srcAlpha = dstAlphaMode == DstAlphaMode::AlphaKeepSrcAlpha ||
                            dstAlphaMode == DstAlphaMode::AlphaKeepSrcAlphaDst;
      const bool dstAlpha = dstAlphaMode == DstAlphaMode::AlphaKeepDst ||
                            dstAlphaMode == DstAlphaMode::AlphaKeepSrcAlphaDst;
      fragmentReturn = fmt::format(
          "\\n    let fc_keep = {} * alpha_keep;"
          "\\n    let fa_keep = {} * alpha_keep;"
          "\\n    return FragmentOutput(vec4f(prev.rgb, {}), vec4f(fc_keep, fc_keep, fc_keep, fa_keep));",
          srcAlpha ? "prev.a" : "1.0", dstAlpha ? "1.0" : srcAlpha ? "prev.a" : "1.0",
          dstAlpha ? "f32((imm.current_pnmtx >> 24u) & 0xFFu) / 255.0" : "prev.a");
    }""")
    c = "lib/gfx/pipeline_cache.cpp"
    cambiar(c, "if (s.earlyZKeep && (c.blendMode != GX_BM_NONE || c.dstAlpha != UINT32_MAX ||",
            "if (s.earlyZKeep && (!gx::early_z_blend_exact(c) ||")
    return textos


def preparar_nativo():
    origen = preparar()
    inicio_gpu = RAIZ / "platforms/switch/aurora_gpu_inicio.cpp.in"
    clave = hashlib.sha256((origen / "lmnx-parches.txt").read_bytes() + Path(__file__).read_bytes() + inicio_gpu.read_bytes() + (RAIZ / "platforms/switch/encoding_lote_directo.inc").read_bytes() + (Path(__file__).parent / "aurora_grabacion_global.py").read_bytes() + (Path(__file__).parent / "aurora_indices_dinamicos.py").read_bytes() + (Path(__file__).parent / "aurora_earlyz_mrt.py").read_bytes() + (Path(__file__).parent / "aurora_earlyz_fix.py").read_bytes() + (Path(__file__).parent / "aurora_fifo_aviso.py").read_bytes() + (Path(__file__).parent / "aurora_gx_matrices.py").read_bytes() + (Path(__file__).parent / "aurora_gx_luces.py").read_bytes() + (Path(__file__).parent / "aurora_gx_texobj.py").read_bytes() + (Path(__file__).parent / "aurora_fifo_bloques.py").read_bytes() + (Path(__file__).parent / "aurora_indices_bloques.py").read_bytes() + (Path(__file__).parent / "aurora_imgui_textura.py").read_bytes()).hexdigest()
    destino = RAIZ / "build" / ("aurora-nativo-" + clave[:12])
    sello = destino / "lmnx-nativo.txt"
    if sello.exists():
        return destino
    if destino.exists():
        raise ValueError("Incomplete snapshot: " + str(destino))
    shutil.copytree(origen, destino)
    for relativa, contenido in transformaciones_41(destino).items():
        (destino / relativa).write_text(contenido, encoding="utf-8")
    for relativa, contenido in transformaciones_45(destino).items():
        (destino / relativa).write_text(contenido, encoding="utf-8")
    for relativa, contenido in transformaciones_46(destino).items():
        (destino / relativa).write_text(contenido, encoding="utf-8")
    # Avoid per-write thread-recording queries in the GX FIFO (tools/aurora_grabacion_global.py).
    import aurora_grabacion_global
    aurora_grabacion_global.aplicar(destino)
    # Always inline write_data. Out of line, its variable-length memcpy calls memmove for each four-byte FIFO word
    # (GXCmd1u32 -> write_u32 -> write_data -> memmove). Inlining makes the constant-length copy a store.
    fifo_h = destino / "lib/gx/fifo.hpp"
    texto_h = fifo_h.read_text(encoding="utf-8")
    for viejo in ("inline void write_data(detail::Recording& rec, const void* data, const uint32_t length) {",
                  "inline void write_data(const void* data, const uint32_t length) {"):
        if texto_h.count(viejo) != 1:
            raise ValueError("fifo.hpp: not found: " + viejo)
        texto_h = texto_h.replace(viejo, "[[gnu::always_inline]] " + viejo)
    # Hidden visibility for FIFO globals was rejected because it did not improve measured frame time.
    fifo_h.write_text(texto_h, encoding="utf-8")
    for ruta in destino.rglob("*"):
        if not ruta.is_file() or ruta.suffix not in (".h", ".hpp", ".cpp", ".inl"):
            continue
        texto = ruta.read_text(encoding="utf-8")
        texto = texto.replace("<webgpu/webgpu_cpp.h>", "<lmnx/aurora_gpu.hpp>")
        texto = texto.replace("<dawn/webgpu_cpp.h>", "<lmnx/aurora_gpu.hpp>")
        texto = texto.replace('"../webgpu/', '"../nativegpu/').replace('"webgpu/', '"nativegpu/')
        texto = texto.replace("/webgpu/gpu.hpp", "/nativegpu/gpu.hpp")
        texto = texto.replace("namespace aurora::webgpu", "namespace aurora::nativegpu").replace("webgpu::", "nativegpu::")
        texto = texto.replace("WGPUBuffer indexBuffer", "lmnx::gpu::BufferData* indexBuffer")
        texto = texto.replace("namespace wgpu {\nclass RenderPassEncoder;\n} // namespace wgpu", "#include <lmnx/aurora_gpu.hpp>")
        if ruta.relative_to(destino).as_posix() == "include/aurora/aurora.h":
            texto = texto.replace("  BACKEND_NULL,", "  BACKEND_NULL,\n  BACKEND_DEKO3D,")
        texto = texto.replace("#include <magic_enum.hpp>", "")
        texto = re.sub(r"magic_enum::enum_name\((\w+)\)", r"static_cast<uint32_t>(\1)", texto)
        if ruta.relative_to(destino).as_posix() == "lib/gfx/resource_cache.cpp":
            texto = texto.replace("wgpu::BindGroup::Acquire(wgpuDeviceCreateBindGroup(nativegpu::g_device.Get(), &descriptor))", "nativegpu::g_device.CreateBindGroup(&descriptor)")
        if ruta.relative_to(destino).as_posix() == "lib/gfx/encoding.cpp":
            # Switch sends a pass's GX draws directly to deko3d in one batch (platforms/switch/encoding_lote_directo.inc).
            marca = "  gx::RenderSeqState seq;\n"
            if texto.count(marca) != 1:
                raise ValueError("encoding.cpp render_pass changed")
            lote = (RAIZ / "platforms/switch/encoding_lote_directo.inc").read_text(encoding="utf-8")
            texto = texto.replace(marca, marca + lote)
            texto = texto.replace("#include <tracy/Tracy.hpp>\n", "#include <tracy/Tracy.hpp>\n#if defined(__SWITCH__)\n#include <cstring>\n#include <lmnx/directo.hpp>\n#include \"resource_cache.hpp\"\n#endif\n", 1)
        if ruta.relative_to(destino).as_posix() == "lib/gx/gx.cpp":
            # Owned descriptors retain shared_ptr without a WebGPU C ABI.
            texto = re.sub(r"(textureView|sampler|layout) = ([^;\n,]+)\.Get\(\)", r"\1 = \2", texto)
        ruta.write_text(texto, encoding="utf-8")
    carpeta = destino / "lib/nativegpu"
    carpeta.mkdir()
    shutil.copyfile(destino / "lib/webgpu/gpu.hpp", carpeta / "gpu.hpp")
    for nombre in ("postfx.hpp", "postfx.cpp"):
        shutil.copyfile(destino / "lib/webgpu" / nombre, carpeta / nombre)
    (carpeta / "wgpu.hpp").write_text('#pragma once\n#include <lmnx/aurora_gpu.hpp>\n', encoding="utf-8")
    # Remove only GPU instrumentation; preserve every draw and copy.
    (carpeta / "gpu_prof.hpp").write_text('''#pragma once
#include <lmnx/aurora_gpu.hpp>
namespace aurora::nativegpu::gpu_prof {
inline void initialize() {}
inline bool text_profile_requested() { return false; }
inline void shutdown() {}
inline void frame_begin(const wgpu::CommandEncoder&) {}
inline void frame_end(const wgpu::CommandEncoder&) {}
inline void after_submit() {}
inline const wgpu::PassTimestampWrites* pass_writes(std::string_view) { return nullptr; }
struct Zone { Zone(const wgpu::CommandEncoder&, std::string_view) {} };
}
''', encoding="utf-8")
    # Horizon lacks mmap. Omit the PC recovery diagnostic journal on Switch without replacing or dropping pipelines.
    ruta_cache = destino / "lib/gfx/pipeline_cache.cpp"
    texto = '#ifdef __SWITCH__\n#include <switch.h>\n#endif\n' + ruta_cache.read_text(encoding="utf-8").replace("#include <sys/mman.h>", "")
    marca = "static void pipeline_worker(uint32_t index) {"
    assert texto.count(marca) == 1
    texto = texto.replace(marca, marca + '''
#ifdef __SWITCH__
  // Workers run below game/render/audio priority; never use core 3.
  uint64_t permitidos=0;
  const Result info=svcGetInfo(&permitidos, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0);
  const uint64_t mascara=permitidos & 7;
  const Result afinidad=R_SUCCEEDED(info) && mascara ?
      svcSetThreadCoreMask(CUR_THREAD_HANDLE, __builtin_ctzll(mascara), mascara) : info;
  const Result prioridad=svcSetThreadPriority(CUR_THREAD_HANDLE, 0x3b);
  Log.info("[precache] trabajador: mascara={}, afinidad={}, prioridad=0x3b resultado={}", mascara, afinidad, prioridad);
#endif
''')
    marca = "    g_activeWorkers = threads;"
    posicion = texto.index(marca)
    texto = texto[:posicion] + '''#ifdef __SWITCH__
    g_boostWorkers = std::clamp(g_boostWorkers, 2u, 3u);
#endif
''' + texto[posicion:]
    inicio = texto.index("void* journal_map(")
    fin = texto.index("\nvoid reset_caches_after_crash", inicio)
    texto = texto[:inicio] + "void* journal_map(const std::filesystem::path&) { return nullptr; }\n" + texto[fin:]
    texto = texto.replace("  msync(g_journal, JournalSize, MS_ASYNC);", "  // Sin mapping de diario en Horizon.")
    texto = texto.replace("PRAGMA journal_mode=WAL;", "PRAGMA journal_mode=DELETE;")
    # Actual pipelines.bin and LMT/DKSH caches do not depend on SQLite. Do not open SQLite files or create its
    # writer on Horizon.
    marca = "static bool prepare_pipeline_cache_db() {"
    assert texto.count(marca) == 1
    texto = texto.replace(marca, marca + '''
#ifdef __SWITCH__
  g_pipelineCacheBroken = true;
  return false;
#endif
''')
    # A rejected shader must not escape the worker and call terminate. Memoize an empty result so get_pipeline skips
    # it and waiting finishes.
    marca = "  auto result = pending.create();"
    assert texto.count(marca) == 1
    texto = texto.replace(marca, '''  CompiledPipeline result{};
  try { result = pending.create(); }
  catch (const std::exception& e) { Log.error("Pipeline {:016x} skipped after exception: {}", pending.hash, e.what()); }
  catch (...) { Log.error("Pipeline {:016x} skipped: unknown exception", pending.hash); }
''')
    texto = texto.replace("void check_crash_journal_once() {", '''void check_crash_journal_once() {
#ifdef __SWITCH__
  Log.info("SQLite and pipeline journal disabled on Horizon; using pipelines.bin and LMT/DKSH");
  return;
#endif
''')
    # Bound compilation waiting without canceling or freeing live work. An empty memoized result makes get_pipeline
    # skip the draw.
    for identificador in ("runtimeKey", "ref"):
        marca=f"g_pipelineReadyCv.wait(lock, [=] {{ return g_pipelines.contains({identificador}) || g_pipelineThreadEnd; }});"
        assert texto.count(marca)==1
        texto=texto.replace(marca,f'''if(!g_pipelineReadyCv.wait_for(lock, std::chrono::seconds(30), [=] {{ return g_pipelines.contains({identificador}) || g_pipelineThreadEnd; }})) {{
      Log.error("Pipeline {{:016x}} exceeded 30 s: draw skipped", {identificador});
      g_pipelines.try_emplace({identificador}, CompiledPipeline{{}});
      g_pendingPipelines.erase({identificador});
    }}''')
    marca="    auto pipelineIt = g_pipelines.find(runtimeKey);"
    assert texto.count(marca)==1
    texto=texto.replace(marca,'''    if(!g_pipelines.contains(runtimeKey) && g_pipelines.size()+g_pendingPipelines.size()>=16384) {
      static bool avisado=false;if(!avisado){avisado=true;Log.error("Cache limit: 16384 pipelines; skipping new variants");}
      return runtimeKey;
    }
'''+marca)
    marca="    auto [it, inserted] = g_knownPipelines.try_emplace"
    assert texto.count(marca)==1
    texto=texto.replace(marca,'''    if(g_knownPipelines.size()>=32768 && !g_knownPipelines.contains(cacheKey))return false;
'''+marca)
    ruta_cache.write_text(texto, encoding="utf-8")
    # DVD needs an owner too: Horizon does not support Aurora's detach when closing/reopening the disc from its own
    # thread's callback.
    dvd=destino/"lib/dolphin/dvd/dvd.cpp"
    texto=dvd.read_text(encoding="utf-8")
    texto='#include <chrono>\n#ifdef __SWITCH__\n#include <switch.h>\n#endif\n'+texto
    marca="        m_running = false;\n        m_thread.detach();"
    assert texto.count(marca)==1
    texto=texto.replace(marca,'''#ifndef __SWITCH__
        m_running = false;
        m_thread.detach();
#endif
        // On Switch keep it joinable so its owner can join it.
''')
    marca="    if (m_running) {\n      return;\n    }"
    assert texto.count(marca)==1
    texto=texto.replace(marca,'''    if (m_running) {
#ifdef __SWITCH__
      if(m_shutdown && std::this_thread::get_id()==m_thread.get_id())m_shutdown=false;
#endif
      return;
    }''')
    texto=texto.replace("    m_shutdown = false;\n    m_thread", "    m_shutdown = false;\n    m_exited = false;\n    m_thread")
    texto=texto.replace("      if (m_shutdown) {\n        return;", "      if (m_shutdown) {\n        m_exited=true; m_doneCv.notify_all();\n        return;")
    texto=texto.replace("    m_thread.join();",'''#ifdef __SWITCH__
    {
      std::unique_lock lk{m_mutex};
      if(!m_doneCv.wait_for(lk,std::chrono::seconds(30),[this]{return m_exited;})) {
        Log.error("DVD did not stop within 30 s; exiting without freeing resources still in use");
        svcExitProcess();
      }
    }
#endif
    m_thread.join();''')
    texto=texto.replace("  bool m_running = false;", "  bool m_running = false;\n  bool m_exited = true;")
    texto=texto.replace("  ~DvdWorker() { stop(); }",'''  ~DvdWorker() {
    stop();
#ifdef __SWITCH__
    if(m_thread.joinable()) {Log.error("DVD destroyed from its own worker; exiting safely");svcExitProcess();}
#endif
  }''')
    dvd.write_text(texto,encoding="utf-8")
    gpu = (destino / "lib/webgpu/gpu.cpp").read_text(encoding="utf-8")
    gpu = gpu.replace("#include <aurora/webgpu.hpp>", "#include <lmnx/backend.hpp>")
    gpu = re.sub(r"#ifdef WEBGPU_DAWN\n#include.*?\n#endif", "", gpu, flags=re.S, count=1)
    gpu = gpu.replace("static wgpu::Adapter g_adapter;", "").replace("  g_adapter = {};", "")
    gpu = gpu[:gpu.index("AuroraLogLevel wgpu_log_level")] + gpu[gpu.index("struct ResampleUniformBlock"):]
    gpu = gpu[:gpu.index("wgpu::TextureFormat to_linear")] + gpu[gpu.index("uint32_t sampler_mode"):]
    gpu = gpu[:gpu.index("wgpu::PresentMode select_present_mode")] + gpu[gpu.index("TextureWithSampler create_render_texture"):]
    gpu = gpu[:gpu.index("static wgpu::BackendType to_wgpu_backend")] + inicio_gpu.read_text(encoding="utf-8") + "\n" + gpu[gpu.index("void shutdown()"):]
    gpu = gpu[:gpu.index("void aurora_enable_vsync")] + '''void aurora_enable_vsync(bool enabled) {
  aurora::nativegpu::g_graphicsConfig.surfaceConfiguration.presentMode = enabled ? wgpu::PresentMode::Fifo : wgpu::PresentMode::Immediate;
  aurora::window::push_custom_event(aurora::window::CustomEvent::RefreshSurface);
}
'''
    gpu = gpu.replace("  cache_shutdown();", "  lmnx::cerrarCacheShaders();")
    (carpeta / "gpu.cpp").write_text(gpu, encoding="utf-8")
    header = (carpeta / "gpu.hpp").read_text(encoding="utf-8")
    inicio = header.index("size_t load_from_cache")
    fin = header.index("// 0223:", inicio)
    # CacheStats and g_cacheVerifyOnOpen are also used without Dawn. Preserve declarations; native implementations
    # are in the template.
    header = header[:inicio] + header[inicio:fin].replace("void cache_shutdown();", "") + header[fin:]
    (carpeta / "gpu.hpp").write_text(header, encoding="utf-8")
    aurora_cpp = destino / "lib/aurora.cpp"
    texto = aurora_cpp.read_text(encoding="utf-8")
    inicio = texto.index("  /* Attempt to create a window")
    fin = texto.index("\n#else", inicio)
    texto = texto[:inicio] + '''  AuroraBackend selectedBackend = BACKEND_DEKO3D;
  AURORA_ASSERT(window::create_window(selectedBackend), "Could not create the Switch window");
  AURORA_ASSERT(nativegpu::initialize(selectedBackend, false), "Could not initialize LMNX-Deko3D");
''' + texto[fin:]
    texto = texto.replace("constexpr std::array<AuroraBackend, 0> PreferredBackendOrder{};", "constexpr std::array PreferredBackendOrder{BACKEND_DEKO3D};")
    texto = re.sub(r"constexpr std::array PreferredBackendOrder\{.*?\};", "constexpr std::array PreferredBackendOrder{BACKEND_DEKO3D};", texto, flags=re.S)
    texto = texto.replace("wgpu::ConvertibleStatus status = wgpu::Status::Error;", "bool status = false;")
    aurora_cpp.write_text(texto, encoding="utf-8")
    imgui = destino / "lib/imgui.cpp"
    texto = imgui.read_text(encoding="utf-8")
    texto = texto.replace('#define IMGUI_IMPL_WEBGPU_BACKEND_DAWN', '#include "imgui_lmnx.hpp"')
    texto = texto.replace('#include "backends/imgui_impl_wgpu.h"', '').replace('#include "backends/imgui_impl_sdlrenderer3.h"','').replace('#include "backends/imgui_impl_sdl3.h"','')
    inicio = texto.index("  SDL_Renderer* renderer = window::get_sdl_renderer();", texto.index("void initialize()"))
    fin = texto.index("  g_inputLayer =", inicio)
    texto = texto[:inicio] + '''  lmnx::imgui::iniciarEntrada(window::get_sdl_window());
  lmnx::imgui::iniciar(nativegpu::g_device, nativegpu::g_graphicsConfig.surfaceConfiguration.format);
''' + texto[fin:]
    inicio = texto.index("  if (g_useSdlRenderer)", texto.index("void shutdown()"))
    fin = texto.index("  ImGui_ImplSDL3_Shutdown();", inicio)
    texto = texto[:inicio] + "  lmnx::imgui::cerrar();\n" + texto[fin:]
    inicio = texto.index("  if (g_useSdlRenderer)", texto.index("void process_event("))
    fin = texto.index("  ImGui_ImplSDL3_ProcessEvent", inicio)
    texto = texto[:inicio] + texto[fin:]
    inicio = texto.index("  if (g_useSdlRenderer)", texto.index("void new_frame("))
    fin = texto.index("  ImGui_ImplSDL3_NewFrame();", inicio)
    texto = texto[:inicio] + "  lmnx::imgui::prepararFuentes();\n" + texto[fin:]
    inicio = texto.index("  if (g_useSdlRenderer)", texto.index("void render("))
    fin = texto.index("\n}\n", inicio)
    texto = texto[:inicio] + "  lmnx::imgui::dibujar(pass, data);" + texto[fin:]
    inicio = texto.index("  if (SDL_Renderer* renderer", texto.index("ImTextureID add_texture("))
    fin = texto.index("  const wgpu::Extent3D", inicio)
    texto = texto[:inicio] + texto[fin:]
    texto = texto.replace("reinterpret_cast<ImTextureID>(textureView.MoveToCHandle())", "lmnx::imgui::registrarVista(textureView)")
    texto = texto.replace("ImGui_ImplSDL3_Shutdown()", "lmnx::imgui::cerrarEntrada()").replace("ImGui_ImplSDL3_ProcessEvent(&renderEvent)", "lmnx::imgui::procesarEvento(&renderEvent)").replace("ImGui_ImplSDL3_NewFrame()", "lmnx::imgui::nuevaEntrada()")
    imgui.write_text(texto, encoding="utf-8")
    # Fourth core (issues StevensND/nfsmw-nx#23 and ARMSX2-NX#12): Aurora threads announce their names;
    # platforms/switch/nucleos.cpp moves the selected thread to core 3 when available.
    hilo = destino / "lib/thread.cpp"
    texto = hilo.read_text(encoding="utf-8")
    marca = "  SDL_SetCurrentThreadPriority(to_sdl_priority(options.priority));\n"
    if texto.count(marca) != 1:
        raise ValueError("thread.cpp: thread priority not found")
    texto = texto.replace(marca, marca + '#ifdef __SWITCH__\n  if (lmnx_hilo_aurora) lmnx_hilo_aurora(options.name.c_str());\n#endif\n')
    # Weak: test ELFs link Aurora without the game platform.
    texto = '#ifdef __SWITCH__\nextern "C" __attribute__((weak)) void lmnx_hilo_aurora(const char* nombre);\n#endif\n' + texto
    hilo.write_text(texto, encoding="utf-8")
    # Return an existing pipeline without constructing std::function and copying its whole configuration, avoiding a
    # per-draw allocation in resolve_pipeline.
    cache = destino / "lib/gfx/pipeline_cache.cpp"
    texto = cache.read_text(encoding="utf-8")
    marca = ("static PipelineRef resolve_pipeline(ShaderType type, const gx::PipelineConfig& config, const RenderTargetLayout& layout,\n"
             "                                    PipelinePriority priority) {\n"
             "  const auto cacheKey = xxh3_hash(config, static_cast<HashType>(type));\n"
             "  const auto runtimeKey = xxh3_hash(layout.key, cacheKey);\n")
    if texto.count(marca) != 1:
        raise ValueError("pipeline_cache.cpp: GX resolve_pipeline not found")
    texto = texto.replace(marca, marca + '''#ifdef __SWITCH__
  {
    std::scoped_lock guard{g_pipelineMutex};
    if (g_pipelines.contains(runtimeKey)) {
      return runtimeKey;
    }
  }
#endif
''')
    cache.write_text(texto, encoding="utf-8")
    # Sweep texture-object caches every 32 frames instead of each frame to reduce main-thread work while the game
    # waits in present. Entries expire after 600-631 unused frames rather than exactly 600.
    textura = destino / "lib/gx/texture.cpp"
    texto = textura.read_text(encoding="utf-8")
    marca = "  apply_pending_invalidations();\n  sweep_object_caches();\n}\n"
    if texto.count(marca) != 1:
        raise ValueError("texture.cpp: end_frame sweep not found")
    texto = texto.replace(marca, "  apply_pending_invalidations();\n#ifdef __SWITCH__\n  if ((s_frameCount & 31) == 0)\n#endif\n  sweep_object_caches();\n}\n")
    textura.write_text(texto, encoding="utf-8")
    # Reading the same image for six-bit blend quantization was rejected: it removed a copy but increased GPU frame
    # time. Scan size_dynamic_arrays in one vertex pass rather than per indexed attribute, with branchless maxima.
    # Preserve maximum indices while excluding 0xFF/0xFFFF.
    cp = destino / "lib/gx/command_processor.cpp"
    texto = cp.read_text(encoding="utf-8")
    inicio = texto.index("static void size_dynamic_arrays(GXVtxFmt fmt, const u8* vtx, u16 vtxCount, u32 vtxSize) noexcept {\n")
    fin = texto.index("\n}\n", inicio) + 3
    texto = texto[:inicio] + '''static void size_dynamic_arrays(GXVtxFmt fmt, const u8* vtx, u16 vtxCount, u32 vtxSize) noexcept {
  // PORT (ProjectLM-NX): one pass over the vertices for every dynamically sized array (was one pass per array).
  const auto& vtxFmt = g_gxState.vtxFmts[fmt];
  struct Scan {
    int attr;
    u32 offset, width, idxSize;
    bool nbt3;
    u32 maxPlusOne;  // highest index seen + 1 (0: none)
  };
  Scan scans[GX_VA_TEX7 + 1];
  int count = 0;
  u32 offset = 0;
  for (int i = GX_VA_PNMTXIDX; i <= GX_VA_TEX7; ++i) {
    const auto& attrFmt = vtxFmt.attrs[i];
    const auto type = g_gxState.vtxDesc[i];
    if (type == GX_NONE) {
      continue;
    }
    if (type == GX_DIRECT) {
      const auto attr = static_cast<GXAttr>(i);
      offset += comp_type_size(attr, attrFmt.type) * comp_cnt_count(attr, attrFmt.cnt);
      continue;
    }
    const bool nbt3 = i == GX_VA_NRM && attrFmt.cnt == GX_NRM_NBT3;
    const u32 idxSize = type == GX_INDEX16 ? 2 : 1;
    const u32 width = nbt3 ? idxSize * 3 : idxSize;
    const auto& array = g_gxState.arrays[i];
    if (array.size == 0 && array.data != nullptr) {
      scans[count++] = Scan{i, offset, width, idxSize, nbt3, 0};
    }
    offset += width;
  }
  if (count == 0) {
    return;
  }
  for (u32 v = 0; v < vtxCount; ++v) {
    const u8* base = vtx + v * vtxSize;
    for (int s = 0; s < count; ++s) {
      Scan& sc = scans[s];
      const u8* p = base + sc.offset;
      u32 m = sc.maxPlusOne;
      if (sc.idxSize == 2) {
        for (u32 k = 0; k < sc.width; k += 2) {
          const u32 idx = u32(p[k]) << 8 | p[k + 1];
          const u32 c = idx == 0xFFFFu ? 0u : idx + 1;
          m = c > m ? c : m;
        }
      } else {
        for (u32 k = 0; k < sc.width; ++k) {
          const u32 idx = p[k];
          const u32 c = idx == 0xFFu ? 0u : idx + 1;
          m = c > m ? c : m;
        }
      }
      sc.maxPlusOne = m;
    }
  }
  for (int s = 0; s < count; ++s) {
    const Scan& sc = scans[s];
    auto& array = g_gxState.arrays[sc.attr];
    const bool any = sc.maxPlusOne != 0;
    const u32 maxIdx = any ? sc.maxPlusOne - 1 : 0;
    const u32 stride = array.stride != 0 ? array.stride : 1;
    const u32 needed = any ? (maxIdx + 1) * stride + (sc.nbt3 ? 2 * stride : 0) : 0;
    if (needed > array.cachedRange.size) {
      // Share uploads between arrays/draws using the same memory this frame, and
      // grow geometrically so a slowly increasing index range isn't re-uploaded per draw.
      auto& shared = sDynamicUploads[array.data];
      if (needed > shared.size) {
        u32 size = 256;
        while (size < needed) size *= 2;
        shared = gfx::push_storage(static_cast<const uint8_t*>(array.data), size);
      }
      array.cachedRange = shared;
      g_gxState.dirty |= DirtyImmediates;
    }
  }
}
''' + texto[fin:]
    # Use a flat sDynamicUploads table instead of std::unordered_map to avoid per-insert nodes, per-frame frees and
    # contention on the shared malloc lock.
    marca = "static std::unordered_map<const void*, gfx::Range> sDynamicUploads;\n"
    if texto.count(marca) != 1:
        raise ValueError("command_processor.cpp: sDynamicUploads not found")
    texto = texto.replace(marca, '''// PORT (ProjectLM-NX): pointer -> uploaded range for this frame, open addressing, cleared by a generation.
struct DynamicUploads {
  struct Entry {
    const void* key = nullptr;
    gfx::Range value{};
    u32 gen = 0;
  };
  std::vector<Entry> table;
  u32 gen = 1;
  size_t count = 0;
  static size_t hash(const void* p) { return size_t((reinterpret_cast<uintptr_t>(p) >> 4) * 0x9E3779B97F4A7C15ull >> 20); }
  void grow() {
    std::vector<Entry> old;
    old.swap(table);
    table.assign(std::max<size_t>(512, old.size() * 2), Entry{});
    const size_t mask = table.size() - 1;
    for (const Entry& e : old)
      if (e.gen == gen) {
        size_t i = hash(e.key) & mask;
        while (table[i].gen == gen) i = (i + 1) & mask;
        table[i] = e;
      }
  }
  gfx::Range& operator[](const void* k) {
    if ((count + 1) * 2 > table.size()) grow();
    const size_t mask = table.size() - 1;
    for (size_t i = hash(k) & mask;; i = (i + 1) & mask) {
      Entry& e = table[i];
      if (e.gen != gen) {
        e = Entry{k, gfx::Range{}, gen};
        ++count;
        return e.value;
      }
      if (e.key == k) return e.value;
    }
  }
  void clear() {
    count = 0;
    if (++gen == 0) {
      for (Entry& e : table) e.gen = 0;
      gen = 1;
    }
  }
};
static DynamicUploads sDynamicUploads;
''')
    # Display-list content memoization was rejected because per-draw lookup, locking and hashing cost more than the
    # scans saved. Cache size_dynamic_arrays attribute plans by format in GXState: offsets, widths, index type and
    # NBT3 depend only on VCD/VAT. Invalidate through clearVtxSizeCache(), already called by all five VCD/VAT
    # decoders and state restoration. Continue computing dynamic arrays and maxima per draw.
    cabecera_plan = destino / "lib/gx/gx.hpp"
    texto_plan = cabecera_plan.read_text(encoding="utf-8")
    viejo = "  std::array<VtxFmt, MaxVtxFmt> vtxFmts;\n"
    if texto_plan.count(viejo) != 1:
        raise ValueError("gx.hpp: vtxFmts for attribute planning not found")
    texto_plan = texto_plan.replace(viejo, '''  std::array<VtxFmt, MaxVtxFmt> vtxFmts;
  // PORT (ProjectLM-NX): per-format layout of the indexed attributes for size_dynamic_arrays (VCD/VAT only).
  struct DynamicArrayPlan {
    struct Entry {
      int attr;
      u32 offset, width, idxSize;
      bool nbt3;
    };
    std::array<Entry, GX_VA_TEX7 + 1> entries{};
    int count = 0;
    bool valid = false;
  };
  std::array<DynamicArrayPlan, MaxVtxFmt> dynamicArrayPlans{};
''', 1)
    viejo = "  void clearVtxSizeCache() { lastVtxFmt = GX_MAX_VTXFMT; }\n"
    if texto_plan.count(viejo) != 1:
        raise ValueError("gx.hpp: clearVtxSizeCache not found")
    texto_plan = texto_plan.replace(viejo, '''  void clearVtxSizeCache() {
    lastVtxFmt = GX_MAX_VTXFMT;
    for (auto& plan : dynamicArrayPlans) {
      plan.valid = false;
    }
  }
''', 1)
    cabecera_plan.write_text(texto_plan, encoding="utf-8")
    viejo = '''static void size_dynamic_arrays(GXVtxFmt fmt, const u8* vtx, u16 vtxCount, u32 vtxSize) noexcept {
  // PORT (ProjectLM-NX): one pass over the vertices for every dynamically sized array (was one pass per array).
  const auto& vtxFmt = g_gxState.vtxFmts[fmt];
  struct Scan {
    int attr;
    u32 offset, width, idxSize;
    bool nbt3;
    u32 maxPlusOne;  // highest index seen + 1 (0: none)
  };
  Scan scans[GX_VA_TEX7 + 1];
  int count = 0;
  u32 offset = 0;
  for (int i = GX_VA_PNMTXIDX; i <= GX_VA_TEX7; ++i) {
    const auto& attrFmt = vtxFmt.attrs[i];
    const auto type = g_gxState.vtxDesc[i];
    if (type == GX_NONE) {
      continue;
    }
    if (type == GX_DIRECT) {
      const auto attr = static_cast<GXAttr>(i);
      offset += comp_type_size(attr, attrFmt.type) * comp_cnt_count(attr, attrFmt.cnt);
      continue;
    }
    const bool nbt3 = i == GX_VA_NRM && attrFmt.cnt == GX_NRM_NBT3;
    const u32 idxSize = type == GX_INDEX16 ? 2 : 1;
    const u32 width = nbt3 ? idxSize * 3 : idxSize;
    const auto& array = g_gxState.arrays[i];
    if (array.size == 0 && array.data != nullptr) {
      scans[count++] = Scan{i, offset, width, idxSize, nbt3, 0};
    }
    offset += width;
  }
  if (count == 0) {
    return;
  }
'''
    if texto.count(viejo) != 1:
        raise ValueError("command_processor.cpp: size_dynamic_arrays preparation changed")
    texto = texto.replace(viejo, '''static void size_dynamic_arrays(GXVtxFmt fmt, const u8* vtx, u16 vtxCount, u32 vtxSize) noexcept {
  // PORT (ProjectLM-NX): one pass over the vertices for every dynamically sized array (was one pass per array).
  // The attribute layout depends on VCD/VAT only: built once per format (invalidated by clearVtxSizeCache()).
  auto& plan = g_gxState.dynamicArrayPlans[fmt];
  if (!plan.valid) {
    const auto& vtxFmt = g_gxState.vtxFmts[fmt];
    plan.count = 0;
    u32 offset = 0;
    for (int i = GX_VA_PNMTXIDX; i <= GX_VA_TEX7; ++i) {
      const auto& attrFmt = vtxFmt.attrs[i];
      const auto type = g_gxState.vtxDesc[i];
      if (type == GX_NONE) {
        continue;
      }
      if (type == GX_DIRECT) {
        const auto attr = static_cast<GXAttr>(i);
        offset += comp_type_size(attr, attrFmt.type) * comp_cnt_count(attr, attrFmt.cnt);
        continue;
      }
      const bool nbt3 = i == GX_VA_NRM && attrFmt.cnt == GX_NRM_NBT3;
      const u32 idxSize = type == GX_INDEX16 ? 2 : 1;
      const u32 width = nbt3 ? idxSize * 3 : idxSize;
      plan.entries[plan.count++] = GXState::DynamicArrayPlan::Entry{i, offset, width, idxSize, nbt3};
      offset += width;
    }
    plan.valid = true;
  }
  struct Scan {
    int attr;
    u32 offset, width, idxSize;
    bool nbt3;
    u32 maxPlusOne;  // highest index seen + 1 (0: none)
  };
  Scan scans[GX_VA_TEX7 + 1];
  int count = 0;
  // Which indexed arrays are dynamically sized is still decided per draw (array base/size can change alone).
  for (int s = 0; s < plan.count; ++s) {
    const auto& entry = plan.entries[s];
    const auto& array = g_gxState.arrays[entry.attr];
    if (array.size == 0 && array.data != nullptr) {
      scans[count++] = Scan{entry.attr, entry.offset, entry.width, entry.idxSize, entry.nbt3, 0};
    }
  }
  if (count == 0) {
    return;
  }
''', 1)
    cp.write_text(texto, encoding="utf-8")
    # Performance panel (LMNX-Deko3D/include/lmnx/perfil_vivo.hpp): FIFO-thread translation time and game-thread
    # wait time for completion (GXDrawDone -> drain).
    fifo = destino / "lib/gx/fifo.cpp"
    texto = fifo.read_text(encoding="utf-8")
    # worker_main wraps processing in the activity scope and thread tick.
    viejo = ("      const activity::Scope busy{activity::Thread::Fifo, \"processing GX commands\"};\n"
             "      process_to(published, std::memory_order_release);\n      thread::tick();\n      continue;\n")
    if texto.count(viejo) != 1:
        raise ValueError("fifo.cpp: worker_main processing not found")
    texto = texto.replace(viejo, '''      const activity::Scope busy{activity::Thread::Fifo, "processing GX commands"};
      // Measured only while the panel is shown (decided once per batch): wall clock and, on Switch, the CPU time the
      // thread actually ran (Horizon thread ticks, 19.2 MHz) - the difference is time it lost its core.
      const bool measure = lmnx::perfil::activo.load(std::memory_order_relaxed);
      const auto busyStart = measure ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
#ifdef __SWITCH__
      const uint64_t cpuStart = measure ? thread_cpu_ticks() : 0;
#endif
      process_to(published, std::memory_order_release);
#ifdef __SWITCH__
      const uint64_t cpuEnd = cpuStart != 0 ? thread_cpu_ticks() : 0;  // right after the work
#endif
      if (measure) {
        lmnx::perfil::fifoOcupadoNs.fetch_add(
            uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - busyStart)
                         .count()),
            std::memory_order_relaxed);
#ifdef __SWITCH__
        if (cpuEnd >= cpuStart && cpuStart != 0) {
          lmnx::perfil::fifoCpuNs.fetch_add((cpuEnd - cpuStart) * 625 / 12, std::memory_order_relaxed);
        }
#endif
      }
      thread::tick();
      continue;
''')
    viejo = '''    if (processed < target) {
      do {
        sProcessed.wait(processed, std::memory_order_acquire);
        processed = sProcessed.load(std::memory_order_acquire);
      } while (processed < target);
    }
'''
    if texto.count(viejo) != 1:
        raise ValueError("fifo.cpp: drain wait not found")
    texto = texto.replace(viejo, '''    if (processed < target) {
      const bool measure = lmnx::perfil::activo.load(std::memory_order_relaxed);
      const auto waitStart = measure ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
      do {
        sProcessed.wait(processed, std::memory_order_acquire);
        processed = sProcessed.load(std::memory_order_acquire);
      } while (processed < target);
      if (measure) {
        lmnx::perfil::esperaFifoNs.fetch_add(
            uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart)
                         .count()),
            std::memory_order_relaxed);
      }
    }
''')
    viejo = "#include <thread>\n"
    if texto.count(viejo) != 1:
        raise ValueError("fifo.cpp: #include <thread> not found")
    # Declare only the needed system call; including switch.h conflicts with Aurora types and macros.
    texto = texto.replace(viejo, '''#include <thread>

#include <lmnx/perfil_vivo.hpp>
#ifdef __SWITCH__
#include <cstdio>
// libnx: Result svcGetInfo(u64* out, u32 id0, Handle handle, u64 id1).
extern "C" uint32_t svcGetInfo(uint64_t* out, uint32_t id0, uint32_t handle, uint64_t id1);
namespace {
constexpr uint32_t kInfoThreadTickCount = 25;          // InfoType_ThreadTickCount (13.0.0+)
constexpr uint32_t kCurrentThreadHandle = 0xFFFF8000;  // CUR_THREAD_HANDLE
constexpr uint64_t kTickCountTotal = UINT64_MAX;       // TickCountInfo_Total: every core
// CPU time this thread has run, in Horizon ticks (19.2 MHz); 0 if the kernel does not report it.
uint64_t thread_cpu_ticks() noexcept {
  uint64_t ticks = 0;
  const uint32_t result = svcGetInfo(&ticks, kInfoThreadTickCount, kCurrentThreadHandle, kTickCountTotal);
  if (result != 0) {
    static bool logged = false;  // FIFO thread only
    if (!logged) {
      logged = true;
      std::fprintf(stderr, "[panel] svcGetInfo(ThreadTickCount) failed: 0x%x (FIFO CPU time unavailable)\\n", result);
    }
    return 0;
  }
  return ticks;
}
} // namespace
#endif
''')
    # Count FIFO publications and wakeups per frame only while the panel is enabled. With kDrawBatchSize = 1, each
    # draw publishes and wakes a sleeping FIFO thread through a system call.
    viejo = "    sPendingDraws = 0;\n    sPublished.store(target, std::memory_order_release);\n"
    if texto.count(viejo) != 1:
        raise ValueError("fifo.cpp: publish() publication not found")
    texto = texto.replace(viejo, viejo + '''    if (lmnx::perfil::activo.load(std::memory_order_relaxed)) {
      lmnx::perfil::publicaciones.fetch_add(1, std::memory_order_relaxed);
    }
''')
    viejo = "    sWorkerWake.wait(event, std::memory_order_acquire);\n"
    if texto.count(viejo) != 1:
        raise ValueError("fifo.cpp: worker_main wait not found")
    texto = texto.replace(viejo, viejo + '''    if (lmnx::perfil::activo.load(std::memory_order_relaxed)) {
      lmnx::perfil::despertares.fetch_add(1, std::memory_order_relaxed);
    }
''')
    fifo.write_text(texto, encoding="utf-8")
    # Performance panel: add completed frame work to atomic counters on the render thread. The menu reads averages
    # without touching unprotected g_resources.stats fields.
    frame_cpp = destino / "lib/gfx/frame.cpp"
    texto = frame_cpp.read_text(encoding="utf-8")
    viejo = "    g_resources.stats.lastTextureUploadSize = stats.lastTextureUploadSize;\n"
    if texto.count(viejo) != 1:
        raise ValueError("frame.cpp: frame statistics publication not found")
    texto = texto.replace(viejo, viejo + '''    if (lmnx::perfil::activo.load(std::memory_order_relaxed)) {
      // Fully qualified: this is inside namespace aurora::gfx, where `aurora` names the namespace.
      auto& sums = lmnx::perfil::aurora;
      sums[lmnx::perfil::AFrames].fetch_add(1, std::memory_order_relaxed);
      sums[lmnx::perfil::ADraws].fetch_add(stats.drawCallCount, std::memory_order_relaxed);
      sums[lmnx::perfil::AMerged].fetch_add(stats.mergedDrawCallCount, std::memory_order_relaxed);
      sums[lmnx::perfil::AVert].fetch_add(stats.lastVertSize, std::memory_order_relaxed);
      sums[lmnx::perfil::AIndex].fetch_add(stats.lastIndexSize, std::memory_order_relaxed);
      sums[lmnx::perfil::AUniform].fetch_add(stats.lastUniformSize, std::memory_order_relaxed);
      sums[lmnx::perfil::AStorage].fetch_add(stats.lastStorageSize, std::memory_order_relaxed);
      sums[lmnx::perfil::ATexture].fetch_add(stats.lastTextureUploadSize, std::memory_order_relaxed);
    }
''')
    viejo = "#include <tracy/Tracy.hpp>\n"
    if texto.count(viejo) != 1:
        raise ValueError("frame.cpp: #include <tracy/Tracy.hpp> not found")
    texto = texto.replace(viejo, viejo + "\n#include <lmnx/perfil_vivo.hpp>\n")
    frame_cpp.write_text(texto, encoding="utf-8")
    # Record blend6 quantization regions and reasons without changing draws, copies or shaders, to evaluate tighter
    # clipping. Sample every 60 frames and log each executed quantization. union is the bounding rectangle of marked
    # draw scissors in physical pixels, with exclusive right/bottom edges.
    def cambiar_region(relativa, viejo, nuevo):
        ruta = destino / relativa
        contenido = ruta.read_text(encoding="utf-8")
        coincidencias = contenido.count(viejo)
        if coincidencias != 1:
            raise ValueError(f"{relativa}: blend6 region anchor: {coincidencias} matches, expected one")
        ruta.write_text(contenido.replace(viejo, nuevo, 1), encoding="utf-8")

    paquete = "lib/gfx/frame_packet.hpp"
    grabacion = "lib/gfx/recording.cpp"
    blend = "lib/gfx/efb_blend6.cpp"
    cambiar_region(paquete, "#include <array>\n", "#include <algorithm>\n#include <array>\n")
    cambiar_region(paquete, "struct RenderPass {\n", """// PORT (ProjectLM-NX): diagnostic, fixed-size union of the scissors of the blend6-marked draws of a pass, in
// physical pixels; right and bottom edges exclusive.
struct Blend6Region {
  uint32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  uint64_t notas = 0, sondas = 0;
  bool hay = false;
  bool completa = false;

  void unir(const Blend6Region& otra) noexcept {
    notas += otra.notas;
    sondas += otra.sondas;
    completa = completa || otra.completa;
    if (!otra.hay) {
      return;
    }
    if (!hay) {
      x0 = otra.x0; y0 = otra.y0;
      x1 = otra.x1; y1 = otra.y1;
      hay = true;
      return;
    }
    x0 = std::min(x0, otra.x0);
    y0 = std::min(y0, otra.y0);
    x1 = std::max(x1, otra.x1);
    y1 = std::max(y1, otra.y1);
  }
};

struct RenderPass {
""")
    cambiar_region(paquete, "  uint8_t efbBlend6 = 0;\n", """  uint8_t efbBlend6 = 0;
  Blend6Region blend6Region;
  uint32_t blend6PassIndex = UINT32_MAX;
  ClipRect blend6Scissor{};
  bool blend6HasScissor = false;
""")
    # The actual recorded scissor, including after pass resume.
    cambiar_region(grabacion, """  if (type == CommandType::Draw || type == CommandType::CustomDraw) {
    renderPass.hasDraws = true;
  }
""", """  if (type == CommandType::SetScissor) {
    renderPass.blend6Scissor = data.setScissor;
    renderPass.blend6HasScissor = true;
  }
  if (type == CommandType::Draw || type == CommandType::CustomDraw) {
    renderPass.hasDraws = true;
  }
""")
    cambiar_region(grabacion, "  pass.sealed = true;\n", """  pass.blend6PassIndex = passIndex;
  pass.sealed = true;
""")
    # Record in the final receiving pass after any split caused by dithering changes.
    cambiar_region(grabacion, "  pass->efbBlend6 |= bit;\n", """  pass->efbBlend6 |= bit;
  if (pass->colorAttachmentCount > SceneColorAttachmentIndex) {
    const auto& size = pass->colorAttachments[SceneColorAttachmentIndex].size;
    const auto sc = pass->blend6Scissor;
    // Same as apply_scissor and the direct batch: convert to uint32_t first, then clamp to the target.
    const uint32_t x = pass->blend6HasScissor ? std::min(static_cast<uint32_t>(sc.x), size.width) : 0u;
    const uint32_t y = pass->blend6HasScissor ? std::min(static_cast<uint32_t>(sc.y), size.height) : 0u;
    const uint32_t w =
        pass->blend6HasScissor ? std::min(static_cast<uint32_t>(sc.width), size.width - x) : size.width;
    const uint32_t h =
        pass->blend6HasScissor ? std::min(static_cast<uint32_t>(sc.height), size.height - y) : size.height;
    Blend6Region region;
    region.notas = 1;
    region.hay = w != 0 && h != 0;
    region.completa = !pass->blend6HasScissor || (x == 0 && y == 0 && w == size.width && h == size.height);
    if (region.hay) {
      region.x0 = x; region.y0 = y;
      region.x1 = x + w; region.y1 = y + h;
    }
    pass->blend6Region.unir(region);
  }
""")
    cambiar_region(blend, """struct Pending {
  WGPUTextureView view = nullptr;
  uint8_t flags = 0;
};
""", """struct Pending {
  WGPUTextureView view = nullptr;
  uint8_t flags = 0;
  detail::Blend6Region region;
};
""")
    cambiar_region(blend, """struct Deferred {
  const detail::RenderPass* pass = nullptr;
  uint8_t flags = 0;
};
""", """struct Deferred {
  const detail::RenderPass* pass = nullptr;
  uint8_t flags = 0;
  detail::Blend6Region region;
};
""")
    firma_vieja = ("void resolve(const wgpu::CommandEncoder& cmd, detail::FramePacket& frame, "
                   "const detail::RenderPass& pass, uint8_t flags)")
    firma_nueva = ("void resolve(const wgpu::CommandEncoder& cmd, detail::FramePacket& frame, "
                   "const detail::RenderPass& pass, uint8_t flags, const detail::Blend6Region& region, "
                   "const char* motivo)")
    cambiar_region(blend, firma_vieja + ";", firma_nueva + ";")
    cambiar_region(blend, firma_vieja + " {", firma_nueva + " {")
    cambiar_region(blend, "  uint8_t flags = pass.efbBlend6;\n", """  uint8_t flags = pass.efbBlend6;
  auto region = pass.blend6Region;
""")
    cambiar_region(blend, """    flags |= g_pending.flags;
    g_pending = {};
""", """    flags |= g_pending.flags;
    region.unir(g_pending.region);
    g_pending = {};
""")
    cambiar_region(blend, "    g_pending = {color.view.Get(), flags};\n", """    ++region.sondas;
    g_pending = {color.view.Get(), flags, region};
""")
    cambiar_region(blend, "    g_deferred = {&pass, flags};\n", "    g_deferred = {&pass, flags, region};\n")
    cambiar_region(blend, "  resolve(cmd, frame, pass, flags);\n", """  const bool consumidorColor = pass.snapshotColorDst || pass.colorPostOps ||
      (pass.resolveTarget && !gx::is_depth_format(pass.resolveFormat));
  resolve(cmd, frame, pass, flags, region, consumidorColor ? "consumidor-color" : "fin-pasada");
""")
    cambiar_region(blend, "  resolve(cmd, frame, *deferred.pass, deferred.flags);\n",
                   """  resolve(cmd, frame, *deferred.pass, deferred.flags, deferred.region, "diferida-antes-operacion");
""")
    cambiar_region(blend,
                   "void flush(const wgpu::CommandEncoder& cmd, detail::FramePacket& frame) { before_op(cmd, frame, nullptr); }\n",
                   """void flush(const wgpu::CommandEncoder& cmd, detail::FramePacket& frame) {
  if (g_deferred.pass == nullptr) {
    return;
  }
  const auto deferred = std::exchange(g_deferred, Deferred{});
  resolve(cmd, frame, *deferred.pass, deferred.flags, deferred.region, "diferida-fin-fotograma");
}
""")
    # Log after emitting quantization, excluding isolated probes and deferred work discarded by a clear.
    cambiar_region(blend, """  if (timing) {
    finish_timing(cmd, frame, std::move(timing));
  }
""", """  if (timing) {
    finish_timing(cmd, frame, std::move(timing));
  }
#ifdef __SWITCH__
  if (false) {  // Diagnostics disabled; measurements showed five of six regions covered the full screen
    Log.info("[blend6 region] frame={} pass={} label='{}' destination={}x{} samples={} union=[{},{},{},{}) "
             "vacia={} completa={} notas={} tramado={} flags={} motivo={} sondas-arrastradas={} copia-color={} "
             "instantanea={} post-color={}",
             frame.frameId, pass.blend6PassIndex, pass.label, color.size.width, color.size.height, samples,
             region.x0, region.y0, region.x1, region.y1, !region.hay, region.completa, region.notas,
             (flags & 2) != 0, static_cast<unsigned>(flags), motivo, region.sondas,
             bool(pass.resolveTarget) && !gx::is_depth_format(pass.resolveFormat), bool(pass.snapshotColorDst),
             pass.colorPostOps);
  }
#endif
""")
    # Copy EFB readback directly from the mapped buffer into game RAM, removing the intermediate vector copy on the
    # game thread. Retain mapping until bytes are applied. Preserve hashes, readback selection and callback
    # behavior.
    ram = "lib/gfx/efb_ram.cpp"

    cambiar_region(
        ram,
        "  std::vector<uint8_t> data;  // filled by finish_maps\n",
        "  const uint8_t* data = nullptr;\n",
    )

    cambiar_region(
        ram,
        "        rb->data.assign(p, p + rb->size);\n",
        "        rb->data = p;\n",
    )

    cambiar_region(
        ram,
        """      rb->buffer.Unmap();
      std::lock_guard lock{g_poolMutex};
      if (g_readbackPool.size() < 16) {
        g_readbackPool.emplace_back(std::move(rb->buffer), align4(rb->size));
      }
    }
    rb->buffer = {};
""",
        """    }
""",
    )

    cambiar_region(
        ram,
        "void shutdown() {\n",
        """void release_readback(Readback& rb, bool recycle) {
  // Device calls and destruction stay outside locks.
  auto buffer = std::move(rb.buffer);
  rb.data = nullptr;
  rb.ok = false;
  if (!buffer) {
    return;
  }
  if (rb.mapStatus == 1) {
    buffer.Unmap();
    if (recycle) {
      std::lock_guard lock{g_poolMutex};
      if (g_readbackPool.size() < 16) {
        g_readbackPool.emplace_back(std::move(buffer), align4(rb.size));
      }
    }
  }
}

struct AppliedReadback {
  Readback& rb;
  ~AppliedReadback() { release_readback(rb, true); }
};

void shutdown() {
""",
    )

    cambiar_region(
        ram,
        "  g_encodePipeline = {};\n",
        """  for (auto& rb : landed) {
    release_readback(*rb, false);
  }
  for (auto& rb : mapped) {
    release_readback(*rb, false);
  }
  g_encodePipeline = {};
""",
    )

    cambiar_region(
        ram,
        """  for (auto& rb : landed) {
    bool wrote = false;
""",
        """  for (auto& rb : landed) {
    const AppliedReadback applied{*rb};
    bool wrote = false;
""",
    )

    cambiar_region(
        ram,
        "        std::memcpy(dest, rb->data.data(), rb->size);\n",
        "        std::memcpy(dest, rb->data, rb->size);\n",
    )

    cambiar_region(
        ram,
        "// No lock held across the Dawn calls (GetConstMappedRange, Unmap, buffer release).\n",
        "// Hold no lock while obtaining the mapped range.\n",
    )
    # Branchless index scanning and efb_ram hashing outside the lock (tools/aurora_indices_dinamicos.py). Apply last
    # because earlier steps insert size_dynamic_arrays loops.
    import aurora_indices_dinamicos
    aurora_indices_dinamicos.aplicar(destino)
    # Apply after index scanning and native pipeline/shader insertion.
    for relativa, contenido in transformaciones_47(destino).items():
        (destino / relativa).write_text(contenido, encoding="utf-8")
    # Anchors already contain all native adaptations and early-Z changes.
    for relativa, contenido in transformaciones_48(destino).items():
        (destino / relativa).write_text(contenido, encoding="utf-8")
    # Single-pass early-Z with the normals target (tools/aurora_earlyz_mrt.py).
    import aurora_earlyz_mrt
    aurora_earlyz_mrt.aplicar(destino)
    for relativa, contenido in transformaciones_49(destino).items():
        (destino / relativa).write_text(contenido, encoding="utf-8")
    # Do not inherit single-pass early-Z in derived variants (tools/aurora_earlyz_fix.py).
    import aurora_earlyz_fix
    aurora_earlyz_fix.aplicar(destino)
    # Wake only waiting FIFO threads (tools/aurora_fifo_aviso.py).
    import aurora_fifo_aviso
    aurora_fifo_aviso.aplicar(destino)
    # GX matrix loads in one FIFO write (tools/aurora_gx_matrices.py).
    import aurora_gx_matrices
    aurora_gx_matrices.aplicar(destino)
    # GX lights in one FIFO write (tools/aurora_gx_luces.py).
    import aurora_gx_luces
    aurora_gx_luces.aplicar(destino)
    # GXLoadTexObj in one FIFO write (tools/aurora_gx_texobj.py).
    import aurora_gx_texobj
    aurora_gx_texobj.aplicar(destino)
    # FIFO functions for game write blocks (tools/aurora_fifo_bloques.py).
    import aurora_fifo_bloques
    aurora_fifo_bloques.aplicar(destino)
    # Each draw's indices in one block (tools/aurora_indices_bloques.py).
    import aurora_indices_bloques
    aurora_indices_bloques.aplicar(destino)
    # Texturas de ImGui actualizables (tools/aurora_imgui_textura.py).
    import aurora_imgui_textura
    aurora_imgui_textura.aplicar(destino)
    sello.write_text(clave + "\n", encoding="ascii")
    return destino


if __name__ == "__main__":
    print(preparar_nativo())
