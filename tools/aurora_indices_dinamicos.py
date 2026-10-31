# Reduce GX FIFO size_dynamic_arrays scanning cost. Find maximum indices array-by-array rather than
# vertex-by-vertex with a generic inner loop. For the common single 16-bit or 8-bit index, use a branchless
# loop: (idx + 1) & 0xFFFF, or & 0xFF, maps the missing-vertex sentinel to zero. NBT3 keeps the generic path.
# Preserve the exact maximum. prepare_aurora_native.py applies this after other transformations.


def aplicar(destino):
    rel = "lib/gx/command_processor.cpp"
    ruta = destino / rel
    texto = ruta.read_text(encoding="utf-8")
    viejo = """  for (u32 v = 0; v < vtxCount; ++v) {
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
"""
    nuevo = """  // LMNX: array by array; one 16- or 8-bit index per vertex (the usual case) without branches:
  // (idx + 1) & 0xFFFF is idx + 1, or 0 for 0xFFFF (no vertex), the same values as before (& 0xFF for 8 bits).
  for (int s = 0; s < count; ++s) {
    Scan& sc = scans[s];
    u32 m = sc.maxPlusOne;
    if (sc.idxSize == 2 && sc.width == 2) {
      for (u32 v = 0; v < vtxCount; ++v) {
        const u8* p = vtx + v * vtxSize + sc.offset;
        const u32 c = ((u32(p[0]) << 8 | p[1]) + 1) & 0xFFFFu;
        m = c > m ? c : m;
      }
    } else if (sc.idxSize == 1 && sc.width == 1) {
      for (u32 v = 0; v < vtxCount; ++v) {
        const u8* p = vtx + v * vtxSize + sc.offset;
        const u32 c = (u32(p[0]) + 1) & 0xFFu;
        m = c > m ? c : m;
      }
    } else {
      for (u32 v = 0; v < vtxCount; ++v) {
        const u8* p = vtx + v * vtxSize + sc.offset;
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
      }
    }
    sc.maxPlusOne = m;
  }
"""
    n = texto.count(viejo)
    if n != 1:
        raise ValueError(f"Dynamic indices, {rel}: {n} matches; expected one")
    ruta.write_text(texto.replace(viejo, nuevo, 1), encoding="utf-8")

    # Compute game-RAM hashes before acquiring efb_ram g_mutex, under g_applyMutex shared by apply/guest_copy,
    # including host interpolation. This avoids blocking FIFO resolve_texture while hashing. Keep comparisons,
    # memcpy and non-atomic hashedBytes/cpuNs accounting under g_mutex. Leave FIFO-thread on_copy unchanged.
    rel = "lib/gfx/efb_ram.cpp"
    ruta = destino / rel
    texto = ruta.read_text(encoding="utf-8")
    cambios = [
        ("AuroraEfbRamStats g_stats{};\n",
         "AuroraEfbRamStats g_stats{};\n"
         "// LMNX: apply and guest_copy hash guest RAM before taking g_mutex; this keeps them apart from each\n"
         "// other (the host thread also applies writebacks, interp.cpp, while interpolating), not from the FIFO thread.\n"
         "std::mutex g_applyMutex;\n"),
        ("""  const auto* dest = static_cast<const uint8_t*>(ptr);
  std::lock_guard lock{g_mutex};
  const CpuTimer timer;
  const aurora::HashType h = hash_range(dest, size);
""",
         """  const auto* dest = static_cast<const uint8_t*>(ptr);
  // LMNX: hashed before taking g_mutex (the FIFO thread takes it for every texture it resolves), under
  // g_applyMutex so no writeback lands in between.
  std::lock_guard applyLock{g_applyMutex};
  const auto hashStart = std::chrono::steady_clock::now();
  const aurora::HashType h = aurora::xxh3_hash_s(dest, size);
  const auto hashNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - hashStart).count());
  std::lock_guard lock{g_mutex};
  const CpuTimer timer;
  g_stats.hashedBytes += size;
  g_stats.cpuNs += hashNs;
"""),
        ("""    if (rb->ok) {
      std::lock_guard lock{g_mutex};
      const CpuTimer timer;
      auto* dest = const_cast<uint8_t*>(rb->dest);
      const aurora::HashType now = hash_range(dest, rb->size);
""",
         """    if (rb->ok) {
      auto* dest = const_cast<uint8_t*>(rb->dest);
      // LMNX: the RAM is hashed before taking g_mutex (the FIFO thread takes g_mutex for every texture it
      // resolves and waited for this hash); g_applyMutex keeps another apply (host thread) from writing in between.
      std::lock_guard applyLock{g_applyMutex};
      const auto hashStart = std::chrono::steady_clock::now();
      const aurora::HashType now = aurora::xxh3_hash_s(dest, rb->size);
      const auto hashNs = static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - hashStart).count());
      std::lock_guard lock{g_mutex};
      const CpuTimer timer;
      g_stats.hashedBytes += rb->size;
      g_stats.cpuNs += hashNs;
"""),
    ]
    for viejo, nuevo in cambios:
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"Hashing outside the lock, {rel}: {n} matches; expected one")
        texto = texto.replace(viejo, nuevo, 1)
    ruta.write_text(texto, encoding="utf-8")
