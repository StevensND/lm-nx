# Update an existing ImGui texture for platforms/switch/vi_ajuste.cpp. Recreating one per frame would exhaust
# memory because Aurora does not free these textures. aurora_imgui_update_texture uses the same upload queue as
# add_texture, placing the copy before that frame's draw. prepare_aurora_native.py calls aplicar(destino) last.


def cambiar(destino, rel, viejo, nuevo, nombre):
    ruta = destino / rel
    texto = ruta.read_text(encoding="utf-8")
    n = texto.count(viejo)
    if n != 1:
        raise ValueError(f"{nombre}, {rel}: {n} matches; expected one")
    ruta.write_text(texto.replace(viejo, nuevo, 1), encoding="utf-8")


def aplicar(destino):
    nombre = "Textura de ImGui actualizable"
    cambiar(destino, "include/aurora/imgui.h",
            "ImTextureID aurora_imgui_add_texture(uint32_t width, uint32_t height, const void* rgba8);\n",
            "ImTextureID aurora_imgui_add_texture(uint32_t width, uint32_t height, const void* rgba8);\n"
            "/* LMNX: new pixels for a texture made by aurora_imgui_add_texture, same size. */\n"
            "void aurora_imgui_update_texture(ImTextureID id, uint32_t width, uint32_t height, const void* rgba8);\n",
            nombre)
    cambiar(destino, "lib/imgui.cpp",
            "ImTextureID add_texture(uint32_t width, uint32_t height, const uint8_t* data) noexcept {\n",
            "// LMNX: the texture behind each ImTextureID, for update_texture.\n"
            "static std::vector<std::pair<ImTextureID, wgpu::Texture>> g_lmnxTexturas;\n\n"
            "ImTextureID add_texture(uint32_t width, uint32_t height, const uint8_t* data) noexcept {\n",
            nombre)
    cambiar(destino, "lib/imgui.cpp",
            "  g_wgpuTextures.push_back(texture);\n  return lmnx::imgui::registrarVista(textureView);\n}\n",
            "  g_wgpuTextures.push_back(texture);\n"
            "  const ImTextureID id = lmnx::imgui::registrarVista(textureView);\n"
            "  g_lmnxTexturas.emplace_back(id, texture);\n"
            "  return id;\n"
            "}\n\n"
            "void update_texture(ImTextureID id, uint32_t width, uint32_t height, const uint8_t* data) noexcept {\n"
            "  for (const auto& [clave, texture] : g_lmnxTexturas) {\n"
            "    if (clave != id) continue;\n"
            "    const wgpu::Extent3D size{.width = width, .height = height, .depthOrArrayLayers = 1};\n"
            "    const wgpu::TexelCopyTextureInfo dstView{.texture = texture};\n"
            "    const uint32_t copyBytesPerRow = AURORA_ALIGN(width * 4, 256);\n"
            "    const wgpu::TexelCopyBufferLayout dataLayout{.bytesPerRow = copyBytesPerRow, .rowsPerImage = height};\n"
            "    enqueue_texture_upload(create_texture_upload_buffer(width, height, data, copyBytesPerRow), dstView, dataLayout,\n"
            "                           size);\n"
            "    return;\n"
            "  }\n"
            "}\n",
            nombre)
    cambiar(destino, "lib/imgui.cpp",
            "  return aurora::imgui::add_texture(width, height, static_cast<const uint8_t*>(rgba8));\n}\n",
            "  return aurora::imgui::add_texture(width, height, static_cast<const uint8_t*>(rgba8));\n}\n"
            "void aurora_imgui_update_texture(ImTextureID id, uint32_t width, uint32_t height, const void* rgba8) {\n"
            "  aurora::imgui::update_texture(id, width, height, static_cast<const uint8_t*>(rgba8));\n}\n",
            nombre)
