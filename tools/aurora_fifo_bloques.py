# Two out-of-line Aurora FIFO functions for hw::fifoWriteBlock (src/recomp/hle_gx.cpp), which cannot include
# internal lib/gx/fifo.hpp dependencies such as fmt/nativegpu. lmnx_write_bytes(data, size) sends write_data to
# the active recording or FIFO. lmnx_recording_room(size) reports no active recording or enough remaining
# capacity. prepare_aurora_native.py calls aplicar(destino) last.


def aplicar(destino):
    rel = "lib/gx/fifo.cpp"
    ruta = destino / rel
    texto = ruta.read_text(encoding="utf-8")
    viejo = "bool in_display_list() { return detail::recording().active; }\n"
    nuevo = (viejo +
             "\n// LMNX: for hw::fifoWriteBlock (src/recomp/hle_gx.cpp), which cannot include this header.\n"
             "void lmnx_write_bytes(const void* data, uint32_t size) { write_data(data, size); }\n"
             "bool lmnx_recording_room(uint32_t size) {\n"
             "  const auto& rec = detail::recording();\n"
             "  return !rec.active || size <= rec.size - rec.pos;\n"
             "}\n")
    n = texto.count(viejo)
    if n != 1:
        raise ValueError(f"FIFO blocks, {rel}: {n} matches; expected one")
    ruta.write_text(texto.replace(viejo, nuevo, 1), encoding="utf-8")
