# Do not inherit single-pass early-Z in derived variants. pipeline_cache.cpp may request a split of an already
# single-pass configuration; split_variants_impl must not copy earlyZKeep into ALWAYS/ALWAYS alpha tests.
# Enhanced shadow depth copies must also clear it to preserve alpha clipping. aurora_earlyz_mrt.py defensively
# defines alpha_keep = 1 when alpha testing is absent. prepare_aurora_native.py calls aplicar(destino) last,
# after transformaciones_49.


def aplicar(destino):
    def cambiar(rel, pares):
        ruta = destino / rel
        texto = ruta.read_text(encoding="utf-8")
        for viejo, nuevo, veces in pares:
            n = texto.count(viejo)
            if n != veces:
                raise ValueError(f"Inherited early-Z, {rel}: {n} coincidencias; expected {veces}: {viejo[:80]!r}")
            texto = texto.replace(viejo, nuevo)
        ruta.write_text(texto, encoding="utf-8")

    cambiar("lib/gx/command_processor.cpp", [(
        "static bool split_variants_impl(const PipelineConfig& pc, PipelineConfig& depthOnly, PipelineConfig& color) noexcept {\n",
        "static bool split_variants_impl(const PipelineConfig& pc, PipelineConfig& depthOnly, PipelineConfig& color) noexcept {\n"
        "  if (pc.shaderConfig.earlyZKeep) {\n"
        "    return false;  // LMNX: already the single pass; never split it again (precache)\n"
        "  }\n",
        1)])
    cambiar("lib/gfx/enhanced.cpp", [
        ("      dc.shaderConfig.depthOnly = 1;\n",
         "      dc.shaderConfig.earlyZKeep = 0;  // LMNX: depth copies keep the alpha test (discard)\n"
         "      dc.shaderConfig.depthOnly = 1;\n", 1),
        ("    dc.shaderConfig.depthOnly = 1; // the enhanced shading of the copy would be wasted (depth only)\n",
         "    dc.shaderConfig.earlyZKeep = 0; // LMNX: depth copies keep the alpha test (discard)\n"
         "    dc.shaderConfig.depthOnly = 1; // the enhanced shading of the copy would be wasted (depth only)\n", 1),
    ])
