"""Instrumenta copias privadas PC; nunca modifica Aurora original."""
def instrumentar(raiz):
    # Diagnostic readback must also be declared in Dawn.
    for ruta in (raiz/"lib").rglob("*.cpp"):
        texto=ruta.read_text(encoding="utf-8")
        if "// LMNX_DIAGNOSTICO_COPY_SRC" in texto:continue
        if "wgpu::TextureUsage::" in texto:
            texto=texto.replace("wgpu::TextureUsage::RenderAttachment", "(wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc)")
            texto=texto.replace("wgpu::TextureUsage::TextureBinding", "(wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc)")
            ruta.write_text("// LMNX_DIAGNOSTICO_COPY_SRC\n"+texto,encoding="utf-8")
    ruta=raiz/"lib/gfx/frame_packet.hpp"
    texto=ruta.read_text(encoding="utf-8")
    if "copySourceDepthTexture" not in texto:
        texto=texto.replace("  wgpu::TextureView copySourceDepthView;", "  wgpu::TextureView copySourceDepthView;\n  wgpu::Texture copySourceDepthTexture;")
        ruta.write_text(texto,encoding="utf-8")
    ruta=raiz/"lib/gfx/recording.cpp"
    texto=ruta.read_text(encoding="utf-8")
    if "copySourceDepthTexture" not in texto:
        texto=texto.replace("  pass.copySourceDepthView =", "  pass.copySourceDepthTexture = nativegpu::g_depthBuffer.texture;\n  pass.copySourceDepthView =" if "nativegpu::" in texto else "  pass.copySourceDepthTexture = webgpu::g_depthBuffer.texture;\n  pass.copySourceDepthView =")
        texto=texto.replace(".copySourceDepthView = prevPass.copySourceDepthView,", ".copySourceDepthView = prevPass.copySourceDepthView,\n      .copySourceDepthTexture = prevPass.copySourceDepthTexture,")
        texto=texto.replace(".copySourceDepthView = g_recorder.offscreenDepth.view,", ".copySourceDepthView = g_recorder.offscreenDepth.view,\n      .copySourceDepthTexture = g_recorder.offscreenDepth.texture,")
        ruta.write_text(texto,encoding="utf-8")
    for relativo in ("lib/gfx/frame.cpp", "lib/gfx/encoding.cpp", "lib/aurora.cpp"):
        ruta=raiz/relativo
        texto=ruta.read_text(encoding="utf-8")
        if '"diagnostico_gfx.hpp"' in texto:continue
        # Aurora headers already define the wgpu alias.
        posicion=texto.index("namespace aurora")
        texto=texto[:posicion]+'#include "diagnostico_gfx.hpp"\n'+texto[posicion:]
        if relativo.endswith("frame.cpp"):
            # Capture on the host; do not read a newer marker from rendering.
            marca_cola="  render_worker::enqueue_begin_frame(frame.frameId, [frameSlot] {"
            assert texto.count(marca_cola)==1
            texto=texto.replace(marca_cola,"  const auto diagnosticoFrame=lmnx::diagnostico::marcaJuego.load(std::memory_order_relaxed);\n  render_worker::enqueue_begin_frame(frame.frameId, [frameSlot, diagnosticoFrame] {")
            marca="    constexpr wgpu::CommandEncoderDescriptor EncoderDescriptor"
            assert texto.count(marca)==1
            texto=texto.replace(marca,"    lmnx::diagnostico::iniciar(diagnosticoFrame);\n"+marca)
        elif relativo.endswith("encoding.cpp"):
            marca="  pass.End();\n"
            assert texto.count(marca)==1
            texto=texto.replace(marca,marca+'''  lmnx::diagnostico::registro(label + " comandos=" + std::to_string(passInfo.commands.size()) + " clearZ=" + std::to_string(passInfo.clearDepthValue));
  lmnx::diagnostico::textura(g_device, cmd, passInfo.copySourceTexture, label + " EFB-color");
  lmnx::diagnostico::textura(g_device, cmd, passInfo.copySourceNormalTexture, label + " EFB-normal");
  lmnx::diagnostico::textura(g_device, cmd, passInfo.copySourceDepthTexture, label + " EFB-profundidad");
''')
            texto=texto.replace("  render_pass(pass, frame, passInfo);",'''  if(lmnx::diagnostico::activo()) {
    size_t dibujos=0;for(const auto& comando:passInfo.commands)if(comando.type==CommandType::Draw || comando.type==CommandType::CustomDraw)++dibujos;
    lmnx::diagnostico::registro(label+" dibujos="+std::to_string(dibujos));
    for(size_t i=0;i<passInfo.colorAttachmentCount;++i){const auto& a=passInfo.colorAttachments[i];
      lmnx::diagnostico::registro("  color="+std::to_string(i)+" format="+std::to_string(uint32_t(a.format))+" size="+std::to_string(a.size.width)+"x"+std::to_string(a.size.height)+" load="+std::to_string(uint32_t(attachments[i].loadOp))+" clear="+std::to_string(a.clearValue.x())+","+std::to_string(a.clearValue.y())+","+std::to_string(a.clearValue.z())+","+std::to_string(a.clearValue.w()));
    }
  }
  render_pass(pass, frame, passInfo);''')
            texto=texto.replace("  CompiledPipeline pipeline;",'  lmnx::diagnostico::registro("pipeline-GX="+std::to_string(ref));\n  CompiledPipeline pipeline;')
            texto=texto.replace("cmd.CopyTextureToTexture(&src, &dst, &size);",'cmd.CopyTextureToTexture(&src, &dst, &size);\n    lmnx::diagnostico::textura(g_device,cmd,dst.texture,"copia-EFB");')
        else:
            marca="    nativegpu::gpu_prof::frame_end(encoder);" if "nativegpu::gpu_prof::frame_end(encoder);" in texto else "    webgpu::gpu_prof::frame_end(encoder);"
            espacio="nativegpu" if "nativegpu::" in marca else "webgpu"
            # Present_source is the game image before ImGui.
            texto=texto.replace(marca,f'    lmnx::diagnostico::textura(g_device,encoder,{espacio}::present_source().texture,"presentada-juego");\n    lmnx::diagnostico::textura(g_device,encoder,currentTexture,"presentada-con-ImGui");\n'+marca)
            texto=texto.replace("g_queue.Submit(1, &frameBuffer);", "g_queue.Submit(1, &frameBuffer);\n      lmnx::diagnostico::despuesDeSubmit();")
            texto=texto.replace("g_queue.Submit(1, &buffer);", "g_queue.Submit(1, &buffer);\n      lmnx::diagnostico::despuesDeSubmit();")
        ruta.write_text(texto,encoding="utf-8")
