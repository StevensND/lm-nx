# Extend single-pass early-Z (transformaciones_47) to two color targets. Scene and normal-buffer passes need
# multiple targets, where dual-source blending is unavailable. If alphaUpdate is off, use output alpha for k (1
# when alpha testing passes, 0 otherwise), blending with SRC_ALPHA / ONE_MINUS_SRC_ALPHA. This selects source or
# destination exactly. The write mask and ZERO / ONE alpha blend preserve destination alpha. Write normals for
# every fragment, matching the split depth pass. Keep dual-source blending for a single target. Log single-pass
# and split pipeline counts. prepare_aurora_native.py calls aplicar(destino) after transformaciones_47/48.


def aplicar(destino):
    def cambiar(rel, pares):
        ruta = destino / rel
        texto = ruta.read_text(encoding="utf-8")
        for viejo, nuevo in pares:
            n = texto.count(viejo)
            if n != 1:
                raise ValueError(f"Multi-target early-Z, {rel}: {n} matches; expected one: {viejo[:80]!r}")
            texto = texto.replace(viejo, nuevo, 1)
        ruta.write_text(texto, encoding="utf-8")

    cambiar("lib/gx/command_processor.cpp", [
        ("""  return !early_z_doble() && nativegpu::g_dualSourceBlendingSupported &&
      gfx::get_render_target_layout().colorAttachmentCount == 1 &&""",
         """  // LMNX: one colour target uses dual-source; with more (scene + normals) the colour's alpha output carries
  // the keep factor, which needs the draw not to write the colour's alpha.
  return !early_z_doble() &&
      (gfx::get_render_target_layout().colorAttachmentCount == 1 ? nativegpu::g_dualSourceBlendingSupported
                                                                  : !pc.alphaUpdate) &&"""),
        ("""  cache.keepEligible = keep;
  cache.alphaKeep = keep;
""",
         """  cache.keepEligible = keep;
  cache.alphaKeep = keep;
  {
    // LMNX: how many pipelines take the single pass and how many still split (log, every 500).
    static u32 sKeep = 0, sChecked = 0;
    if (early_z_alpha_split(cache.config)) {
      sKeep += keep ? 1 : 0;
      if (++sChecked % 500 == 0) {
        Log.info("[early-z 47b] {} of {} early-Z pipeline changes take the single pass (colour targets {})", sKeep,
                 sChecked, gfx::get_render_target_layout().colorAttachmentCount);
      }
    }
  }
"""),
    ])
    cambiar("lib/gx/shader.cpp", [
        # A normalized earlyZKeep configuration may have no alpha test. All fragments pass, so use factor 1 to avoid an
        # undefined alpha_keep.
        ("  if (config.alphaCompare) {\n    const auto comp0",
         "  if (config.earlyZKeep && !config.alphaCompare) {\n"
         "    // LMNX: no alpha test left in this configuration: every fragment passes.\n"
         "    fragmentFn += \"\\n    let alpha_keep = 1.0;\";\n"
         "  }\n"
         "  if (config.alphaCompare) {\n    const auto comp0"),
        ("  if (dstAlphaMode == DstAlphaMode::DualSource || config.earlyZKeep) {\n    fragmentOutput =",
         "  if (dstAlphaMode == DstAlphaMode::DualSource || (config.earlyZKeep && normalAttachment == UINT32_MAX)) {\n"
         "    fragmentOutput ="),
        ("  if (dstAlphaMode == DstAlphaMode::DualSource || config.earlyZKeep) {\n    shaderSource.insert",
         "  if (dstAlphaMode == DstAlphaMode::DualSource || (config.earlyZKeep && normalAttachment == UINT32_MAX)) {\n"
         "    shaderSource.insert"),
        ("""    fragmentReturn += "\\n    return out;";
  }
""",
         """    fragmentReturn += "\\n    return out;";
    if (config.earlyZKeep) {
      // LMNX: with the normal target there is no dual-source; the colour's alpha (not written by these
      // draws) carries the keep factor for the SRC_ALPHA / ONE_MINUS_SRC_ALPHA blend (gx.cpp).
      const auto at = fragmentReturn.find("out.color = prev;");
      fragmentReturn.replace(at, sizeof("out.color = prev;") - 1, "out.color = vec4f(prev.rgb, alpha_keep);");
    }
  }
"""),
    ])
    cambiar("lib/gx/gx.cpp", [
        ("""    blendState.alpha = blendState.color;
  }
  std::array<wgpu::ColorTargetState, gfx::MaxColorAttachments> colorTargets{};""",
         """    blendState.alpha = blendState.color;
    if (layout.colorAttachmentCount > 1) {
      // LMNX: no dual-source with the normal target: the source alpha is the keep factor; the colour's alpha
      // is kept (and not written: these draws have alphaUpdate off).
      blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
      blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
      blendState.alpha = {
          .operation = wgpu::BlendOperation::Add,
          .srcFactor = wgpu::BlendFactor::Zero,
          .dstFactor = wgpu::BlendFactor::One,
      };
    }
  }
  std::array<wgpu::ColorTargetState, gfx::MaxColorAttachments> colorTargets{};"""),
    ])
