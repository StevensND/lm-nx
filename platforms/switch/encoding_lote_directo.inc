#if defined(__SWITCH__)
  // As in SwitchWaker's dk_encode, submit GX draws to deko3d in one batch with state caching (lmnx/directo.hpp),
  // avoiding one command per call. Non-GX operations end the batch and use the existing path in the same order.
  {
    std::vector<lmnx::gpu::directo::Orden> lote;
    lote.reserve(passInfo.commands.size());
    wgpu::BindGroup texturas = gx::g_emptyTextureBindGroup;
    const auto vaciar = [&] {
      if (lote.empty()) {
        return;
      }
      lmnx::gpu::directo::grabarLote(pass, resources().staticBindGroup, resources().uniformBindGroup,
                                     resources().indexBuffer, std::move(lote));
      lote.clear();
    };
    const auto& destino = passInfo.colorAttachments[SceneColorAttachmentIndex].size;
    for (auto& cmd : passInfo.commands) {
      switch (cmd.type) {
      case CommandType::SetViewport: {
        const auto& vp = cmd.data.setViewport;
        lmnx::gpu::directo::Orden o;
        o.tipo = lmnx::gpu::directo::Orden::Tipo::Viewport;
        o.viewport[0] = vp.left;
        o.viewport[1] = vp.top;
        o.viewport[2] = vp.width;
        o.viewport[3] = vp.height;
        o.viewport[4] = gx::UseReversedZ ? 1.f - vp.zfar : vp.znear;
        o.viewport[5] = gx::UseReversedZ ? 1.f - vp.znear : vp.zfar;
        lote.push_back(std::move(o));
        currentViewport = vp;
        hasViewport = true;
      } break;
      case CommandType::SetScissor: {
        const auto& sc = cmd.data.setScissor;
        lmnx::gpu::directo::Orden o;
        o.tipo = lmnx::gpu::directo::Orden::Tipo::Scissor;
        const auto x = std::clamp(static_cast<uint32_t>(sc.x), 0u, destino.width);
        const auto y = std::clamp(static_cast<uint32_t>(sc.y), 0u, destino.height);
        o.scissor[0] = x;
        o.scissor[1] = y;
        o.scissor[2] = std::clamp(static_cast<uint32_t>(sc.width), 0u, destino.width - x);
        o.scissor[3] = std::clamp(static_cast<uint32_t>(sc.height), 0u, destino.height - y);
        lote.push_back(std::move(o));
        currentScissor = sc;
        hasScissor = true;
      } break;
      case CommandType::Draw: {
        auto& draw = cmd.data.draw;
        if (const gx::DrawData* d = gx_draw_payload(draw)) {
          gfx::CompiledPipeline cp;
          if (!gfx::get_pipeline(d->pipeline, cp) || !cp.main) {
            break;  // As in render_seq: skip the draw if the pipeline is not ready.
          }
          if (d->indexCount != 0 && (d->idxRange.offset % sizeof(uint16_t)) != 0) {
            vaciar();  // Unaligned indices use the existing path; Aurora's packing does not produce these.
            gx::render(*d, pass);
            break;
          }
          if (d->bindGroups.textureBindGroup) {
            texturas = gfx::find_bind_group(d->bindGroups.textureBindGroup);
          }
          lmnx::gpu::directo::Orden o;
          o.principal = cp.main;
          o.previo = cp.prepass;
          o.texturas = texturas;
          o.offsetUniformes = d->uniformRange.offset;
          o.indices = d->indexCount;
          o.primerIndice = d->indexCount != 0 ? d->idxRange.offset / static_cast<uint32_t>(sizeof(uint16_t)) : 0;
          o.vertices = d->vtxCount;
          o.instancias = d->instanceCount;
          o.dstAlpha = d->dstAlpha;
          static_assert(sizeof(d->immediateData) <= 64);
          std::memcpy(o.inmediatos.data(), &d->immediateData, sizeof(d->immediateData));
          lote.push_back(std::move(o));
        } else if (draw.encoder != nullptr) {
          vaciar();
          draw.encoder(draw.payload.data(), pass, passInfo);
          texturas = gx::g_emptyTextureBindGroup;
        }
      } break;
      case CommandType::CustomDraw: {
        vaciar();
        render_custom_draw(cmd.data.customDraw, pass, passInfo);
        g_currentPipeline = nullptr;
        pass.SetBindGroup(0, resources().staticBindGroup);
        pass.SetBindGroup(2, gx::g_emptyTextureBindGroup);
        texturas = gx::g_emptyTextureBindGroup;
        if (hasViewport) {
          apply_viewport(pass, currentViewport);
        }
        if (hasScissor) {
          apply_scissor(pass, currentScissor, destino);
        }
      } break;
      default:
        break;
      }
    }
    vaciar();
    (void)seq;
    (void)seqOn;
    return;
  }
#endif
