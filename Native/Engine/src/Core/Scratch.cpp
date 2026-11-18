#include "Core/Scratch.hpp"

#include <array>
#include <cstddef>

namespace VS::Core {

namespace {
constexpr std::size_t kScratchBytes = 1024;
constexpr std::size_t kScratchAlignment = 16;
alignas(kScratchAlignment) std::array<std::byte, kScratchBytes> g_scratch;
}  // namespace

void* ScratchBase() noexcept { return g_scratch.data(); }

}  // namespace VS::Core