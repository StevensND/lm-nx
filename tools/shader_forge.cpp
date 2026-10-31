// Shader forge: the console's shader caches, built from a pipeline list without a GPU or the game.
// For the GX shaders it runs the same steps as Aurora's precache on Switch (lib/gfx/pipeline_cache.cpp
// queue_gx_variants; lib/gx/pipeline.cpp create_pipeline) with the port's default settings: accuracy
// flags, enhanced lighting off, early-Z split and its single alpha-keep pass, destination alpha mode.
// Every stage then goes through Tint, the binding line and uam exactly as LMNX-Deko3D does on the
// console, into the two paquete.lmp files of the console's cache folder.
//
// Built for the PC (tools/build_shader_forge.py, tests) and for the installer page (WebAssembly).
#include "gx/pipeline.hpp"
#include "gx/shader_info.hpp"
#include "shader_translate.h"
#include <lmnx/cache_format.hpp>
#include <lmnx/shader_source.hpp>
#include <lmnx/translation_cache.hpp>
#include <uam_api.h>
#include <zstd.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace aurora { AuroraConfig g_config{.logLevel = LOG_WARNING}; }
namespace aurora::nativegpu { bool g_clipDistancesSupported = false; }
// The generator needs the final uniform block size, not a device (DK_UNIFORM_BUF_ALIGNMENT).
namespace aurora::gfx { uint32_t align_uniform(uint32_t v) { return (v + 255u) & ~255u; } }

using namespace aurora::gx;

namespace {

// The port's defaults on Switch (src/port/settings.h, src/recomp/hle_gx.cpp applyAccuracySettings):
// integer TEV, EFB color depth, copy filter, 6-bit blending, early-Z alpha depth, merged early-Z.
constexpr uint32_t kDefaultAccuracy = 0x9f;
// deko3d reports dual-source blending (LMNX-Deko3D aurora_gpu.cpp); scene and offscreen targets have
// one color attachment and, with enhanced lighting off, no normal buffer.
constexpr bool kDualSource = true;
constexpr uint32_t kNoNormalAttachment = UINT32_MAX;

// lib/gfx/pipeline_cache.cpp adjust_accuracy.
PipelineConfig adjust_accuracy(const PipelineConfig& config, uint32_t accuracy) {
  auto adjusted = config;
  adjusted.shaderConfig.intTev = (accuracy & GX_AURORA_ACCURACY_INT_TEV) != 0;
  if ((accuracy & GX_AURORA_ACCURACY_EFB_DEPTH) == 0) {
    adjusted.shaderConfig.efbFmt = 0;
    adjusted.shaderConfig.dither = false;
    adjusted.shaderConfig.efbBlend = false;
  } else if ((accuracy & GX_AURORA_ACCURACY_EFB_BLEND) != 0 && adjusted.shaderConfig.efbFmt == 1 &&
             adjusted.shaderConfig.efbBlend) {
    const bool replace = (adjusted.blendMode == GX_BM_BLEND && adjusted.blendFacSrc == GX_BL_ONE &&
                          adjusted.blendFacDst == GX_BL_ZERO) ||
                         (adjusted.blendMode == GX_BM_LOGIC && adjusted.blendOp == GX_LO_COPY);
    if (!replace) {
      adjusted.shaderConfig.efbFmt = 0;
      adjusted.shaderConfig.dither = false;
      adjusted.shaderConfig.efbBlend = false;
    }
  }
  return adjusted;
}

// lib/gx/command_processor.cpp alpha_compare_passes.
bool alpha_compare_passes(const AlphaCompare& ac, uint32_t a) {
  const auto cmp = [a](GXCompare c, uint32_t ref) {
    switch (c) {
    case GX_NEVER: return false;
    case GX_LESS: return a < ref;
    case GX_EQUAL: return a == ref;
    case GX_LEQUAL: return a <= ref;
    case GX_GREATER: return a > ref;
    case GX_NEQUAL: return a != ref;
    case GX_GEQUAL: return a >= ref;
    default: return true;
    }
  };
  const bool p0 = cmp(ac.comp0, ac.ref0), p1 = cmp(ac.comp1, ac.ref1);
  switch (ac.op) {
  case GX_AOP_AND: return p0 && p1;
  case GX_AOP_OR: return p0 || p1;
  case GX_AOP_XOR: return p0 != p1;
  default: return p0 == p1; // XNOR
  }
}

// lib/gx/command_processor.cpp split_variants_impl.
bool early_z_split(const PipelineConfig& pc, PipelineConfig& depthOnly, PipelineConfig& color) {
  if (!pc.depthCompare || !pc.depthUpdate || !pc.shaderConfig.alphaCompare || pc.shaderConfig.lineMode != 0 ||
      pc.shaderConfig.wireframe) {
    return false;
  }
  bool rejects = false;
  for (uint32_t a = 0; a < 256 && !rejects; ++a) {
    rejects = !alpha_compare_passes(pc.shaderConfig.alphaCompare, a);
  }
  if (!rejects) {
    return false;
  }
  depthOnly = pc;
  depthOnly.shaderConfig.alphaCompare = AlphaCompare{GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0};
  depthOnly.colorUpdate = false;
  depthOnly.alphaUpdate = false;
  depthOnly.blendMode = GX_BM_NONE;
  depthOnly.blendFacSrc = GX_BL_ONE;
  depthOnly.blendFacDst = GX_BL_ZERO;
  depthOnly.blendOp = GX_LO_COPY;
  depthOnly.dstAlpha = UINT32_MAX;
  color = pc;
  color.depthUpdate = false;
  switch (color.depthFunc) {
  case GX_LESS: color.depthFunc = GX_LEQUAL; break;
  case GX_GREATER: color.depthFunc = GX_GEQUAL; break;
  case GX_NEQUAL: color.depthFunc = GX_EQUAL; break;
  default: break;
  }
  return true;
}

struct Forge {
  std::set<std::string> modules;
  std::vector<std::string> order;
  uint32_t pipelines = 0;

  void module(const ShaderConfig& config, DstAlphaMode mode) {
    auto wgsl = build_shader_source(config, mode, kNoNormalAttachment);
    if (modules.insert(wgsl).second) {
      order.push_back(std::move(wgsl));
    }
  }

  // lib/gx/pipeline.cpp create_pipeline: the destination alpha mode of each module it builds.
  void pipeline(const PipelineConfig& config) {
    ++pipelines;
    if (config.shaderConfig.earlyZKeep) {
      const bool srcAlpha = config.blendMode == GX_BM_BLEND && config.blendFacSrc == GX_BL_SRCALPHA;
      module(config.shaderConfig, config.dstAlpha != UINT32_MAX
                                      ? (srcAlpha ? DstAlphaMode::AlphaKeepSrcAlphaDst : DstAlphaMode::AlphaKeepDst)
                                      : (srcAlpha ? DstAlphaMode::AlphaKeepSrcAlpha : DstAlphaMode::AlphaKeep));
      return;
    }
    if (config.alphaUpdate && config.dstAlpha != UINT32_MAX && config.dstAlpha != 0) {
      const auto sourceAlpha = [](GXBlendFactor f) { return f == GX_BL_SRCALPHA || f == GX_BL_INVSRCALPHA; };
      const bool needsSourceAlpha = config.colorUpdate && config.blendMode == GX_BM_BLEND &&
                                    (sourceAlpha(config.blendFacSrc) || sourceAlpha(config.blendFacDst));
      if (!needsSourceAlpha) {
        module(config.shaderConfig, DstAlphaMode::Replace);
      } else if (kDualSource) {
        module(config.shaderConfig, DstAlphaMode::DualSource);
      } else {
        module(config.shaderConfig, DstAlphaMode::Replace);
        module(config.shaderConfig, DstAlphaMode::None);
      }
      return;
    }
    module(config.shaderConfig, DstAlphaMode::None);
  }

  // lib/gfx/pipeline_cache.cpp queue_gx_variants with enhanced lighting off.
  void variants(const PipelineConfig& recorded, uint32_t accuracy) {
    PipelineConfig base = adjust_accuracy(recorded, accuracy);
    canonicalize_pipeline_config(base);
    if (base.shaderConfig.depthOnly || base.shaderConfig.lightDepth) {
      return; // enhanced lighting's own passes
    }
    PipelineConfig v = base;
    v.shaderConfig.enhClass = 0;
    const auto queue = [&](PipelineConfig c) {
      canonicalize_pipeline_config(c);
      pipeline(c);
    };
    queue(v);
    if ((accuracy & GX_AURORA_ACCURACY_EARLY_Z) != 0 && !v.shaderConfig.depthOnly && !v.shaderConfig.lightDepth) {
      PipelineConfig depthOnly, color;
      if (early_z_split(v, depthOnly, color)) {
        queue(depthOnly);
        queue(color);
        // lib/gx/command_processor.cpp early_z_split_cached: when the blend is exact, a draw takes the
        // single alpha-keep pass instead of the pair (early_z_alpha_keep: one color target, dual
        // source, no logic op / Z texture / fade). The precache does not build it; the console then
        // compiled it in play.
        const auto& sc = v.shaderConfig;
        if (early_z_blend_exact(v) && kDualSource && sc.logicOut == 0 && sc.zTexOp == 0) {
          PipelineConfig single = v;
          single.shaderConfig.earlyZKeep = 1;
          queue(single);
        }
      }
    }
  }
};


uint32_t rd32(const uint8_t* b, size_t at) {
  uint32_t v;
  std::memcpy(&v, b + at, 4);
  return v;
}

// The port's own (non-GX) shader modules: copies, conversions, post-processing, the menu (Aurora and
// port sources, as the console builds them). Generated from tools/shader_forge_builtin.json.
struct Builtin {
  const char* wgsl;
  const char* entry;
  uint32_t stage;
};
#include "shader_forge_builtin.inc"

struct Job {
  std::string wgsl;
  std::string entry;
  uint32_t stage; // DkStage: 0 vertex, 4 fragment, 5 compute
  bool gx;
};

// Every shader stage the console's precache builds from the list, plus the port's own modules.
bool generate(const uint8_t* list, size_t bytes, uint32_t accuracy, std::vector<Job>& jobs, std::string& error) {
  // "AUPC", u32 format, u32 flags, u32 rows, u64 raw size, [format 2: u32 CRC, u32 reserved]
  if (bytes < 24 || std::memcmp(list, "AUPC", 4) != 0) {
    error = "not a pipeline list";
    return false;
  }
  const uint32_t format = rd32(list, 4), flags = rd32(list, 8), rows = rd32(list, 12);
  uint64_t raw;
  std::memcpy(&raw, list + 16, 8);
  const size_t header = format == 1 ? 24 : 32;
  if ((format != 1 && format != 2) || (flags & ~1u) != 0 || bytes < header || raw > 256u * 1024 * 1024) {
    error = "unsupported pipeline list";
    return false;
  }
  // Flag 1: zstd payload (the list as data/pipelines.bin ships it).
  std::vector<uint8_t> payload(list + header, list + bytes);
  if (flags & 1) {
    std::vector<uint8_t> plain(raw);
    if (ZSTD_decompress(plain.data(), plain.size(), payload.data(), payload.size()) != raw) {
      error = "damaged pipeline list";
      return false;
    }
    payload = std::move(plain);
  }
  list = payload.data();
  bytes = payload.size();
  size_t at = 0;
  Forge forge;
  std::set<std::string> known;
  for (uint32_t i = 0; i < rows; ++i) {
    if (at + 13 > bytes) {
      error = "truncated pipeline list";
      return false;
    }
    const uint8_t type = list[at];
    const uint32_t version = rd32(list, at + 1), size = rd32(list, at + 5);
    at += 13 + (format == 2 ? 4 : 0);
    if (at + size > bytes) {
      error = "truncated pipeline list";
      return false;
    }
    if (type == 1 && version == GXPipelineConfigVersion && size == sizeof(PipelineConfig)) {
      PipelineConfig config{};
      std::memcpy(&config, list + at, size);
      PipelineConfig key = config;
      canonicalize_pipeline_config(key);
      if (known.insert(std::string(reinterpret_cast<const char*>(&key), sizeof key)).second) {
        forge.variants(config, accuracy);
      }
    }
    at += size;
  }
  jobs.clear();
  for (const auto& wgsl : forge.order) {
    jobs.push_back({wgsl, "vs_main", 0, true});
    jobs.push_back({wgsl, "fs_main", 4, true});
  }
  for (const auto& b : kBuiltins) {
    jobs.push_back({b.wgsl, b.entry, b.stage, false});
  }
  return true;
}

void put_record(std::vector<uint8_t>& out, uint64_t key, const std::vector<uint8_t>& data) {
  // lmnx/cache_paquete.hpp: "LMPK", 64-bit key, 32-bit size, data.
  const uint32_t magic = 0x4B504D4C, n = uint32_t(data.size());
  const size_t at = out.size();
  out.resize(at + 16 + n);
  std::memcpy(out.data() + at, &magic, 4);
  std::memcpy(out.data() + at + 4, &key, 8);
  std::memcpy(out.data() + at + 12, &n, 4);
  std::memcpy(out.data() + at + 16, data.data(), n);
}

// One stage, as LMNX-Deko3D does it on the console (aurora_gpu.cpp compilar, shader_cache.cpp): Tint
// translation, GLSL with the binding line, uam. Appends its translation record to `tint` and its
// DKSH record to `dksh`, both ready for the caches' paquete.lmp.
bool compile(const Job& job, std::vector<uint8_t>& tint, std::vector<uint8_t>& dksh, std::string& error) {
  static const bool ready = [] {
    swdk::translate_init();
    uam::setBanjoFlags(447);
    uam::init(true);
    return true;
  }();
  (void)ready;
  const auto scheme = job.gx ? swdk::BindingScheme::GX : swdk::BindingScheme::Sequential;
  const auto key = lmnx::claveTraduccion(job.wgsl, job.entry, job.stage, scheme);
  const auto r = swdk::wgsl_to_glsl(job.wgsl, job.entry, swdk::ShaderStage(job.stage), scheme);
  if (!r.ok) {
    error = "Tint: " + r.error;
    return false;
  }
  put_record(tint, lmnx::hashFuente(key), lmnx::empaquetarTraduccion(key, r));
  uint32_t ubo = 0, ssbo = 0;
  for (const auto& b : r.bindings) {
    if (b.kind == swdk::SlotBinding::Kind::Uniform || b.kind == swdk::SlotBinding::Kind::Immediates) ubo |= 1u << b.slot;
    if (b.kind == swdk::SlotBinding::Kind::Storage) ssbo |= 1u << b.slot;
  }
  const auto glsl = lmnx::prepararGLSL(r.glsl, ubo, ssbo);
  const auto result = uam::compile(uam::Stage(job.stage), glsl.c_str());
  if (!result.ok) {
    error = "uam: " + result.log;
    return false;
  }
  const auto entry = lmnx::empaquetarCache(job.stage, glsl, result.dksh);
  if (entry.empty()) {
    error = "uam returned an invalid DKSH";
    return false;
  }
  put_record(dksh, lmnx::hashFuente(glsl) ^ (uint64_t(job.stage) << 59), entry);
  return true;
}

} // namespace

#ifdef __EMSCRIPTEN__
// The installer page's interface (one instance per Web Worker): forge_generate once, then
// forge_compile for this worker's share of the jobs; the page appends the records to both caches.
namespace {
std::vector<Job> sJobs;
std::vector<uint8_t> sTint, sDksh;
std::string sError;
} // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE const char* forge_build_id() { return LMNX_SHADER_BUILD_ID; }
EMSCRIPTEN_KEEPALIVE int forge_generate(const uint8_t* list, uint32_t bytes, uint32_t accuracy) {
  return generate(list, bytes, accuracy ? accuracy : kDefaultAccuracy, sJobs, sError) ? int(sJobs.size()) : -1;
}
EMSCRIPTEN_KEEPALIVE int forge_compile(uint32_t job) {
  sTint.clear();
  sDksh.clear();
  if (job >= sJobs.size()) {
    sError = "no such job";
    return 0;
  }
  return compile(sJobs[job], sTint, sDksh, sError) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE const uint8_t* forge_tint_data() { return sTint.data(); }
EMSCRIPTEN_KEEPALIVE uint32_t forge_tint_size() { return uint32_t(sTint.size()); }
EMSCRIPTEN_KEEPALIVE const uint8_t* forge_dksh_data() { return sDksh.data(); }
EMSCRIPTEN_KEEPALIVE uint32_t forge_dksh_size() { return uint32_t(sDksh.size()); }
EMSCRIPTEN_KEEPALIVE const char* forge_error() { return sError.c_str(); }
}
#else
// PC: shader_forge <pipelines.bin> <out dir> [accuracy] [--wgsl]
// Writes <out>/lmnx-tint-<id>/paquete.lmp and <out>/lmnx-f447-gxabi2-<id>/paquete.lmp (as the console's
// cache folder), or with --wgsl only the GX modules (gx-<hash>.wgsl) for comparisons.
int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: shader_forge <pipelines.bin> <out dir> [accuracy] [--wgsl]\n");
    return 2;
  }
  uint32_t accuracy = kDefaultAccuracy;
  bool wgslOnly = false;
  for (int i = 3; i < argc; ++i) {
    if (std::strcmp(argv[i], "--wgsl") == 0) wgslOnly = true;
    else accuracy = uint32_t(std::strtoul(argv[i], nullptr, 0));
  }
  std::ifstream in(argv[1], std::ios::binary);
  const std::vector<uint8_t> list((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<Job> jobs;
  std::string error;
  if (!generate(list.data(), list.size(), accuracy, jobs, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 3;
  }
  const std::filesystem::path out = argv[2];
  if (wgslOnly) {
    std::filesystem::create_directories(out);
    for (const auto& job : jobs) {
      if (!job.gx || job.stage != 0) continue;
      char name[64];
      std::snprintf(name, sizeof name, "gx-%016llx.wgsl", static_cast<unsigned long long>(lmnx::hashFuente(job.wgsl)));
      std::ofstream(out / name, std::ios::binary) << job.wgsl;
    }
    std::printf("%zu stages\n", jobs.size());
    return 0;
  }
  std::vector<uint8_t> tint, dksh, t, d;
  std::set<uint64_t> seen;
  for (size_t i = 0; i < jobs.size(); ++i) {
    t.clear();
    d.clear();
    if (!compile(jobs[i], t, d, error)) {
      std::fprintf(stderr, "stage %zu (%s): %s\n", i, jobs[i].entry.c_str(), error.c_str());
      return 1;
    }
    tint.insert(tint.end(), t.begin(), t.end());
    uint64_t key;
    std::memcpy(&key, d.data() + 4, 8);
    if (seen.insert(key).second) dksh.insert(dksh.end(), d.begin(), d.end());
    if ((i + 1) % 200 == 0) std::printf("%zu / %zu\n", i + 1, jobs.size());
  }
  const std::string id = LMNX_SHADER_BUILD_ID;
  const std::pair<std::string, const std::vector<uint8_t>*> caches[] = {{"lmnx-tint-" + id, &tint},
                                                                         {"lmnx-f447-gxabi2-" + id, &dksh}};
  for (const auto& [dir, bytes] : caches) {
    std::filesystem::create_directories(out / dir);
    std::ofstream file(out / dir / "paquete.lmp", std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes->data()), std::streamsize(bytes->size()));
  }
  std::printf("%zu stages, %zu DKSH, build %s\n", jobs.size(), seen.size(), id.c_str());
  return 0;
}
#endif
