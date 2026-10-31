#pragma once
#include <atomic>
#include <cstdint>
namespace lmnx::diagnostico {
// Only the host thread increments; rendering receives a snapshot per packet.
inline std::atomic<uint64_t> marcaJuego{UINT64_MAX};
}
