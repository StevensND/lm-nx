# LMNX-Deko3D

Native deko3d graphics backend for LM-NX. It adapts banjo-nx's plume backend and patched uam, with an Aurora GX resource adapter.

- `third_party/plume/`: memory, descriptors, commands, pipelines, presentation and compiler worker.
- `third_party/uam/`: GLSL-to-DKSH compiler, including imported Mesa compiler code.
- `src/backend.cpp`: backend configuration.
- `src/native_gfx.cpp` and `src/aurora_gpu.cpp`: native graphics operations and Aurora resources.
- `src/shader_cache.cpp` and `include/lmnx/`: shader cache storage, validation and backend interfaces.

The native Switch targets use deko3d for rendering. uam uses Mesa-derived frontend and code generation as a shader compiler. The separate PC comparison path uses plume/Vulkan.

See [Building for Switch](../docs/BUILDING_SWITCH.md) and [third-party notices](../THIRD_PARTY_NOTICES.md).
