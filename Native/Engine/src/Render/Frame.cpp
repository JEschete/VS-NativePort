// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Render/Frame.hpp"

#include <cstddef>
#include <span>

#include "Core/Log.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Render.hpp"

using VS::Legacy::DR_STP;

// decomp: vs_scratch (src/include/scratch.h:23): the frame's drawing context.
VS::Render::FrameScratch VS::Render::frameScratch{};

// decomp: D_800F1870 (D_800F1870@BATTLE/BATTLE.PRG)
void* VS::Render::legacyCurrentOrderingTable;

// decomp: D_800F1970 (D_800F1970@BATTLE/BATTLE.PRG)
DR_STP VS::Render::legacyMaskBitOffPackets[VS::Render::kMaskBitOffPacketCount];

// decomp: D_800F1988 (D_800F1988@BATTLE/BATTLE.PRG)
DR_STP VS::Render::legacyMaskBitOnPackets[VS::Render::kMaskBitOnPacketCount];

// decomp: D_80055C80 (D_80055C80@SLUS_010.40)
void* VS::Render::legacyOrderingTables[VS::Render::kFrameBufferCount];

// decomp: D_8005E0C0 (D_8005E0C0@SLUS_010.40)
void* VS::Render::legacyPrimitiveBuffers[VS::Render::kFrameBufferCount];

namespace VS::Render {
namespace {
static_assert(kFrameBufferCount * 2 == VS::Legacy::kGpuLinkRegionCount);

VS::Legacy::GpuLinkRegion LinkRegion(FrameMemory memory, std::size_t buffer) {
  const auto first = memory == FrameMemory::kPrimitives
                         ? VS::Legacy::GpuLinkRegion::kPrimitives0
                         : VS::Legacy::GpuLinkRegion::kOrderingTable0;
  return static_cast<VS::Legacy::GpuLinkRegion>(
      static_cast<std::size_t>(first) + buffer);
}

std::span<void*, kFrameBufferCount> Pointers(FrameMemory memory) {
  return memory == FrameMemory::kPrimitives
             ? std::span<void*, kFrameBufferCount>(legacyPrimitiveBuffers)
             : std::span<void*, kFrameBufferCount>(legacyOrderingTables);
}
}  // namespace

void SetLegacyFrameMemory(FrameMemory memory, std::size_t buffer,
                          void* allocation, std::size_t bytes) {
  if (buffer >= kFrameBufferCount) {
    VS::Core::LogError("render", "frame buffer {} does not exist", buffer);
    return;
  }
  Pointers(memory)[buffer] = allocation;
  const std::span<std::byte> region =
      allocation == nullptr
          ? std::span<std::byte>()
          : std::span(static_cast<std::byte*>(allocation), bytes);
  static_cast<void>(
      VS::Legacy::BindGpuLinkRegion(LinkRegion(memory, buffer), region));
}

void ClearLegacyFrameMemory() {
  for (std::size_t buffer = 0; buffer < kFrameBufferCount; ++buffer) {
    SetLegacyFrameMemory(FrameMemory::kPrimitives, buffer, nullptr, 0);
    SetLegacyFrameMemory(FrameMemory::kOrderingTable, buffer, nullptr, 0);
  }
}

void BeginLegacyFrame(std::size_t buffer) {
  VS::Legacy::BeginGpuLinkFrame(buffer);
}
}  // namespace VS::Render
