#include "Menu/TextBoxFramesLive.h"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <new>
#include <span>

#include "Content/ModuleData.h"
#include "Core/Memory/HeapLive.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Menu/TextBoxFrames.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Event/ScriptVm.h"
#include "Menu/TextBox.h"
}

namespace {

using FrameBytes = std::span<const std::uint8_t>;
constexpr std::size_t kBoxOrderingStride = 4;
std::uint64_t refused_frames = 0;

void RefuseFrame(int index, int reason) {
  ++refused_frames;
  if (std::has_single_bit(refused_frames)) {
    Core_Warnf("menu",
               "Text-box frame refused: box=%d reason=%d count=%" PRIu64, index,
               reason, refused_frames);
  }
}

FrameBytes SourceTail(FrameBytes source, const void* pointer) {
  const auto base = reinterpret_cast<std::uintptr_t>(source.data());
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  if (base == 0 || address < base || address - base >= source.size()) {
    return {};
  }
  return source.subspan(address - base);
}

FrameBytes CompatibilityStyleBytes(const void* pointer) {
  const auto allocation = VS::Core::Memory::LivePayloadContaining(pointer);
  if (allocation) {
    return SourceTail(
        {reinterpret_cast<const std::uint8_t*>(allocation->data()),
         allocation->size()},
        pointer);
  }
  FrameBytes result;
  for (const auto& symbol :
       std::span(Content_moduleSymbols, Content_moduleSymbolCount)) {
    if (symbol.nativeSize == 0 ||
        symbol.sourceElementSize != symbol.nativeElementSize) {
      continue;
    }
    const auto candidate = SourceTail(
        {static_cast<const std::uint8_t*>(symbol.target), symbol.nativeSize},
        pointer);
    if (!candidate.empty() &&
        (result.empty() || candidate.size() < result.size())) {
      result = candidate;
    }
  }
  return result;
}

void DrawFrame(int index) {
  if (index < 0 ||
      static_cast<std::size_t>(index) >= std::size(Menu_textBoxes) ||
      Menu_activeTextBoxOrderingTable == nullptr) {
    RefuseFrame(index, -1);
    return;
  }
  const auto& box = Menu_textBoxes[index];
  const auto lease =
      VS::Event::LeaseLiveTextBoxFrame(static_cast<int>(box.flags.frameShape));
  FrameBytes bytes;
  if (lease) {
    const auto view = lease->Bytes();
    if (view) {
      bytes = *view;
    }
  } else if (lease.error() == VS::Event::ScriptSourceError::kMissingOwner) {
    const void* pointer = Event_textBoxFrameStyles[box.flags.frameShape];
    if (pointer == nullptr) {
      return;
    }
    bytes = CompatibilityStyleBytes(pointer);
  }
  const VS::Menu::FrameRequest request{
      box.frameX,
      box.frameY,
      box.frameWidth,
      box.frameHeight,
      static_cast<std::uint8_t>(box.flags.alignment),
      box.brightness,
      box.flags.framePart};
  auto* ordering = Menu_activeTextBoxOrderingTable +
                   static_cast<std::size_t>(index) * kBoxOrderingStride;
  VS::Render::LivePacketQueue foreground(ordering + 2);
  VS::Render::LivePacketQueue shadow(ordering + 1);
  const auto result =
      VS::Menu::DrawTextBoxFrame(foreground, shadow, bytes, request);
  if (!result) {
    RefuseFrame(index, static_cast<int>(result.error()));
  }
}

}  // namespace

// decomp: BATTLE.PRG func_800CD3E4 (src/BATTLE/BATTLE.PRG/5BF94.c:4715).
void Menu_DrawTextBoxFrame(int index) {
  try {
    DrawFrame(index);
  } catch (const std::bad_alloc&) {
    RefuseFrame(index, -2);
  }
}
