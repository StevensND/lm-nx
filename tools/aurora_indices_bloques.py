# prepare_idx_buffer (lib/gx/command_processor.cpp) writes a draw's index list in one block: one resize followed
# by plain stores instead of ByteBuffer::append per index/triangle. Preserve indices, order, host byte order and
# u16 count for quads with multiples of four vertices, triangles, fans and strips with at least three vertices.
# Degenerate counts, lines and points keep the original path. prepare_aurora_native.py calls aplicar(destino)
# last.


def aplicar(destino):
    textos = {}

    def cambiar(rel, viejo, nuevo):
        texto = textos.get(rel)
        if texto is None:
            texto = (destino / rel).read_text(encoding="utf-8")
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"Block indices, {rel}: {n} matches; expected one")
        textos[rel] = texto.replace(viejo, nuevo, 1)

    rel = "lib/internal.hpp"
    viejo = "  void append(const void* data, size_t size) {\n"
    cambiar(rel, viejo, """  // LMNX: reserves `size` bytes at the end and lets `fill` write them (one resize, no per-item append).
  template <class F>
  void append_generated(size_t size, F&& fill) {
    resize(m_length + size, false);
    fill(m_data + m_length);
    m_length += size;
  }

""" + viejo)

    rel = "lib/gx/command_processor.cpp"
    cambiar(rel, "#include <optional>\n", "#include <optional>\n#include <cstring>\n")
    viejo = """u16 prepare_idx_buffer(ByteBuffer& buf, GXPrimitive prim, u16 vtxStart, u16 vtxCount) noexcept {
  u16 numIndices = 0;
"""
    cambiar(rel, viejo, """u16 prepare_idx_buffer(ByteBuffer& buf, GXPrimitive prim, u16 vtxStart, u16 vtxCount) noexcept {
  // LMNX: the usual cases in one block; the same indices, order, byte order and u16 count as below.
  u32 n = 0;
  if (prim == GX_TRIANGLES && vtxCount != 0) {
    n = vtxCount;
  } else if (prim == GX_QUADS && vtxCount != 0 && (vtxCount & 3) == 0) {
    n = (u32(vtxCount) / 4) * 6;
  } else if ((prim == GX_TRIANGLEFAN || prim == GX_TRIANGLESTRIP) && vtxCount >= 3) {
    n = (u32(vtxCount) - 2) * 3;
  }
  if (n != 0) {
    buf.append_generated(size_t(n) * sizeof(u16), [&](uint8_t* dst) {
      auto emit = [&](u16 i) {
        std::memcpy(dst, &i, sizeof(i));
        dst += sizeof(i);
      };
      for (u32 v = 0; v < vtxCount; ++v) {
        const u16 i = static_cast<u16>(vtxStart + v);
        if (prim == GX_TRIANGLES) {
          emit(i);
        } else if (prim == GX_QUADS) {
          if ((v & 3) != 0) continue;
          emit(i); emit(static_cast<u16>(i + 1)); emit(static_cast<u16>(i + 2));
          emit(static_cast<u16>(i + 2)); emit(static_cast<u16>(i + 3)); emit(i);
        } else if (v < 3) {
          emit(i);
        } else {
          const u16 a = static_cast<u16>(i - 2), b = static_cast<u16>(i - 1);
          if (prim == GX_TRIANGLEFAN) { emit(vtxStart); emit(b); }
          else if ((v & 1) == 0) { emit(a); emit(b); }
          else { emit(b); emit(a); }
          emit(i);
        }
      }
    });
    return static_cast<u16>(n);
  }
  // Degenerate counts, lines and points: unchanged.
  u16 numIndices = 0;
""")
    for rel, texto in textos.items():
        (destino / rel).write_text(texto, encoding="utf-8")
