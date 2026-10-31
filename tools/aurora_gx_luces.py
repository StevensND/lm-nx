# Write GXLoadLightObjImm in one FIFO write instead of separate header, register, zeros, color and float writes
# with repeated capacity checks. Preserve exactly the same bytes and order. prepare_aurora_native.py applies
# this after aurora_gx_matrices.


def aplicar(destino):
    rel = "lib/dolphin/gx/GXLighting.cpp"
    ruta = destino / rel
    texto = ruta.read_text(encoding="utf-8")
    viejo = """  auto& gxRec41 = aurora::gx::fifo::detail::recording();
  GX_WRITE_U8_REC(gxRec41, 0x10);
  GX_WRITE_U32_REC(gxRec41, reg);
  // Padding (3 u32s)
  GX_WRITE_U32_REC(gxRec41, 0);
  GX_WRITE_U32_REC(gxRec41, 0);
  GX_WRITE_U32_REC(gxRec41, 0);
  // Color
  GX_WRITE_U32_REC(gxRec41, colorPacked);
  // Cosine attenuation (a0, a1, a2)
  GX_WRITE_F32_REC(gxRec41, light->a0);
  GX_WRITE_F32_REC(gxRec41, light->a1);
  GX_WRITE_F32_REC(gxRec41, light->a2);
  // Distance attenuation (k0, k1, k2)
  GX_WRITE_F32_REC(gxRec41, light->k0);
  GX_WRITE_F32_REC(gxRec41, light->k1);
  GX_WRITE_F32_REC(gxRec41, light->k2);
  // Position (px, py, pz)
  GX_WRITE_F32_REC(gxRec41, light->px);
  GX_WRITE_F32_REC(gxRec41, light->py);
  GX_WRITE_F32_REC(gxRec41, light->pz);
  // Direction (nx, ny, nz)
  GX_WRITE_F32_REC(gxRec41, light->nx);
  GX_WRITE_F32_REC(gxRec41, light->ny);
  GX_WRITE_F32_REC(gxRec41, light->nz);
}"""
    nuevo = """  // LMNX: one FIFO write with the same bytes: 0x10, reg, three zero words, the packed colour, then a0 a1 a2
  // k0 k1 k2 px py pz nx ny nz (big-endian), as the per-field writes produced.
  const f32 values[12] = {light->a0, light->a1, light->a2, light->k0, light->k1, light->k2,
                          light->px, light->py, light->pz, light->nx, light->ny, light->nz};
  uint8_t buf[1 + 4 + 4 * 16];
  buf[0] = 0x10;
  const u32 words[5] = {__builtin_bswap32(reg), 0, 0, 0, __builtin_bswap32(colorPacked)};
  std::memcpy(buf + 1, words, sizeof(words));
  for (int i = 0; i < 12; i++) {
    u32 v;
    std::memcpy(&v, &values[i], 4);
    v = __builtin_bswap32(v);
    std::memcpy(buf + 21 + 4 * i, &v, 4);
  }
  aurora::gx::fifo::write_data(aurora::gx::fifo::detail::recording(), buf, sizeof(buf));
}"""
    n = texto.count(viejo)
    if n != 1:
        raise ValueError(f"GX lights, {rel}: {n} matches; expected one")
    texto = texto.replace(viejo, nuevo, 1)
    if "#include <cstring>" not in texto:
        texto = "#include <cstring>\n" + texto
    ruta.write_text(texto, encoding="utf-8")
