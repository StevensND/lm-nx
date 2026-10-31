# Write GX matrix loads (GXLoadPosMtxImm, GXLoadNrmMtxImm, GXLoadTexMtxImm) in one FIFO write instead of per
# field. Preserve 0x10, the big-endian register and big-endian floats in the same order.
# prepare_aurora_native.py calls aplicar(destino) last.


def aplicar(destino):
    rel = "lib/dolphin/gx/GXTransform.cpp"
    ruta = destino / rel
    texto = ruta.read_text(encoding="utf-8")
    ayuda = """
// LMNX: a matrix load as one FIFO write: 0x10, the XF register (big-endian), then the floats (big-endian),
// the same bytes the per-field writes produced.
static void lmnx_write_xf_floats(u32 reg, const f32* values, u32 n) {
  uint8_t buf[1 + 4 + 4 * 12];
  buf[0] = 0x10;
  const u32 r = __builtin_bswap32(reg);
  std::memcpy(buf + 1, &r, 4);
  for (u32 i = 0; i < n; i++) {
    u32 v;
    std::memcpy(&v, &values[i], 4);
    v = __builtin_bswap32(v);
    std::memcpy(buf + 5 + 4 * i, &v, 4);
  }
  aurora::gx::fifo::write_data(aurora::gx::fifo::detail::recording(), buf, 5 + 4 * n);
}

void GXLoadPosMtxImm(const void* mtx_, u32 id) {"""
    cambios = [
        ("\nvoid GXLoadPosMtxImm(const void* mtx_, u32 id) {", ayuda),
        ("""  const auto* mtx = reinterpret_cast<const f32*>(mtx_);

  auto& gxRec41 = aurora::gx::fifo::detail::recording();
  GX_WRITE_U8_REC(gxRec41, 0x10);
  GX_WRITE_U32_REC(gxRec41, (id * 4) | 0xB0000);
  for (int i = 0; i < 12; i++) {
    GX_WRITE_F32_REC(gxRec41, mtx[i]);
  }
}""",
         """  const auto* mtx = reinterpret_cast<const f32*>(mtx_);
  f32 v[12];
  for (int i = 0; i < 12; i++) {
    v[i] = mtx[i];
  }
  lmnx_write_xf_floats((id * 4) | 0xB0000, v, 12);
}"""),
        ("""  auto& gxRec41 = aurora::gx::fifo::detail::recording();
  GX_WRITE_U8_REC(gxRec41, 0x10);
  GX_WRITE_U32_REC(gxRec41, (id * 3 + 0x400) | 0x80000);
  // Write 3x3 from 3x4 matrix (skip translation column)
  GX_WRITE_F32_REC(gxRec41, mtx[0]);
  GX_WRITE_F32_REC(gxRec41, mtx[1]);
  GX_WRITE_F32_REC(gxRec41, mtx[2]);
  GX_WRITE_F32_REC(gxRec41, mtx[4]);
  GX_WRITE_F32_REC(gxRec41, mtx[5]);
  GX_WRITE_F32_REC(gxRec41, mtx[6]);
  GX_WRITE_F32_REC(gxRec41, mtx[8]);
  GX_WRITE_F32_REC(gxRec41, mtx[9]);
  GX_WRITE_F32_REC(gxRec41, mtx[10]);
}""",
         """  // Write 3x3 from 3x4 matrix (skip translation column)
  const f32 v[9] = {mtx[0], mtx[1], mtx[2], mtx[4], mtx[5], mtx[6], mtx[8], mtx[9], mtx[10]};
  lmnx_write_xf_floats((id * 3 + 0x400) | 0x80000, v, 9);
}"""),
        ("""  auto& gxRec41 = aurora::gx::fifo::detail::recording();
  GX_WRITE_U8_REC(gxRec41, 0x10);
  GX_WRITE_U32_REC(gxRec41, reg);

  const auto* mtx = reinterpret_cast<const f32*>(mtx_);
  for (u32 i = 0; i < count; i++) {
    GX_WRITE_F32_REC(gxRec41, mtx[i]);
  }
}""",
         """  const auto* mtx = reinterpret_cast<const f32*>(mtx_);
  if (count == 8) {
    f32 v[8];
    for (u32 i = 0; i < 8; i++) {
      v[i] = mtx[i];
    }
    lmnx_write_xf_floats(reg, v, 8);
  } else {
    f32 v[12];
    for (u32 i = 0; i < 12; i++) {
      v[i] = mtx[i];
    }
    lmnx_write_xf_floats(reg, v, 12);
  }
}"""),
    ]
    for viejo, nuevo in cambios:
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"GX matrices, {rel}: {n} matches; expected one: {viejo[:70]!r}")
        texto = texto.replace(viejo, nuevo, 1)
    if "#include <cstring>" not in texto:
        texto = "#include <cstring>\n" + texto
    ruta.write_text(texto, encoding="utf-8")
