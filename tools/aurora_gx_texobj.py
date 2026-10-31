# Write GXLoadTexObj's 6-7 BP registers (0x61 plus a big-endian u32 each) in one FIFO write instead of 12-14.
# Preserve bytes, order, emit_loaded_texobj_metadata and __gx state. prepare_aurora_native.py applies this after
# aurora_gx_luces.


def aplicar(destino):
    rel = "lib/dolphin/gx/GXTexture.cpp"
    ruta = destino / rel
    texto = ruta.read_text(encoding="utf-8")
    viejo = """  GX_WRITE_RAS_REG(obj->mode0);
  GX_WRITE_RAS_REG(obj->mode1);
  GX_WRITE_RAS_REG(obj->image0);
  GX_WRITE_RAS_REG(image1);
  GX_WRITE_RAS_REG(image2);
  GX_WRITE_RAS_REG(obj->image3);

  if ((obj->flags & 2) == 0) {
    u32 tlut = 0;
    SET_REG_FIELD(0, tlut, 10, 0, static_cast<u32>(obj->tlut));
    SET_REG_FIELD(0, tlut, 8, 24, GXTexTlutIds[id]);
    GX_WRITE_RAS_REG(tlut);
  }
"""
    nuevo = """  // LMNX: the BP writes as one FIFO write, the same bytes (0x61 + big-endian u32 each) in the same order.
  u32 regs[7] = {obj->mode0, obj->mode1, obj->image0, image1, image2, obj->image3, 0};
  int nregs = 6;
  if ((obj->flags & 2) == 0) {
    u32 tlut = 0;
    SET_REG_FIELD(0, tlut, 10, 0, static_cast<u32>(obj->tlut));
    SET_REG_FIELD(0, tlut, 8, 24, GXTexTlutIds[id]);
    regs[nregs++] = tlut;
  }
  uint8_t buf[7 * 5];
  for (int i = 0; i < nregs; i++) {
    buf[i * 5] = 0x61;
    const u32 v = __builtin_bswap32(regs[i]);
    std::memcpy(buf + i * 5 + 1, &v, 4);
  }
  aurora::gx::fifo::write_data(aurora::gx::fifo::detail::recording(), buf, uint32_t(nregs * 5));
"""
    n = texto.count(viejo)
    if n != 1:
        raise ValueError(f"GXLoadTexObj, {rel}: {n} matches; expected one")
    texto = texto.replace(viejo, nuevo, 1)
    if "#include <cstring>" not in texto:
        texto = "#include <cstring>\n" + texto
    ruta.write_text(texto, encoding="utf-8")
