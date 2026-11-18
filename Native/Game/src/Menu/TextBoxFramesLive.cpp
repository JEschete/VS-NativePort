#include "Menu/TextBoxFramesLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <new>
#include <span>

#include "Content/ModuleData.h"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Event/ScriptVm.h"
#include "Event/ScriptVmLive.hpp"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Menu/TextBoxFrames.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Sprites.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.h"

namespace VS::Menu {
namespace {

using FrameBytes = std::span<const std::uint8_t>;
constexpr std::size_t kBoxOrderingStride = 4;
std::uint64_t refused_frames = 0;

void RefuseFrame(int index, int reason) {
  ++refused_frames;
  if (std::has_single_bit(refused_frames)) {
    VS::Core::Warnf("menu",
                    "Text-box frame refused: box=%d reason=%d count=%" PRIu64,
                    index, reason, refused_frames);
  }
}

std::uint32_t PackHalfwords(std::int32_t low, std::int32_t high) {
  constexpr int kHalfwordBits = 16;
  return static_cast<std::uint16_t>(low) |
         (static_cast<std::uint32_t>(high) << kHalfwordBits);
}

class LiveInformationBoxWorld final : public InformationBoxWorld {
 public:
  explicit LiveInformationBoxWorld(std::size_t index)
      : index_(index),
        backdrop_(Menu_activeTextBoxOrderingTable == nullptr
                      ? nullptr
                      : Menu_activeTextBoxOrderingTable +
                            index * kBoxOrderingStride + 1),
        headers_(
            static_cast<u_long*>(Render_frameScratch.overlayOrderingTable)) {}

  TextBox ReadBox() const override { return ReadLiveTextBox(index_); }
  bool AlternateFramebuffer() const override { return Render_frameBuf != 0; }

  bool DrawHeader(std::size_t kind, std::int32_t x, std::int32_t y) override {
    const Render::SpriteRequest request{
        Render::kDefaultSpriteDrawMode, Render::kDefaultSpriteBrightness,
        std::bit_cast<std::int32_t>(PackHalfwords(x, y)),
        Menu_informationHeaderPositions[kind]};
    auto packet = Render::BuildSpritePacket(request, headers_.ChainLink());
    packet[Render::kSpriteTextureWord] =
        static_cast<std::uint32_t>(Menu_kInformationHeaderSpriteWords[kind]);
    return headers_.Emit(packet);
  }

  bool DrawFramebufferRow(std::int32_t x, std::int32_t y, std::int32_t width,
                          std::int32_t source_x,
                          std::uint8_t brightness) override {
    constexpr std::uint32_t kFramebufferMode = 0xE1000100;
    constexpr int kTexturePageShift = 6;
    constexpr int kTextureYShift = 8;
    const Render::SpriteRequest request{
        kFramebufferMode |
            (static_cast<std::uint32_t>(source_x) >> kTexturePageShift),
        brightness, std::bit_cast<std::int32_t>(PackHalfwords(x, y)),
        std::bit_cast<std::int32_t>(PackHalfwords(width, 1))};
    auto packet = Render::BuildSpritePacket(request, backdrop_.ChainLink());
    packet[Render::kSpriteTextureWord] = static_cast<std::uint32_t>(y)
                                         << kTextureYShift;
    return backdrop_.Emit(packet);
  }

  bool DrawBlackFill(std::int32_t x, std::int32_t y, std::int32_t width,
                     std::int32_t height) override {
    constexpr std::uint32_t kBlackTile = 0x60000000;
    return Render::InsertThreeWordPrimitive(
        backdrop_,
        {kBlackTile, PackHalfwords(x, y), PackHalfwords(width, height)});
  }

 private:
  std::size_t index_;
  Render::LivePacketQueue backdrop_;
  Render::LivePacketQueue headers_;
};

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

void DrawLiveInformationBoxHeader(std::int32_t y, std::uint32_t kind) {
  LiveInformationBoxWorld world(0);
  if (!DrawInformationBoxHeader(world, y, kind)) {
    RefuseFrame(-1, static_cast<int>(FrameDecodeError::kCapacity));
  }
}

void DrawLiveInformationBoxBackdrop(std::int32_t index) {
  if (index < 0 ||
      static_cast<std::size_t>(index) >= std::size(Menu_textBoxes)) {
    RefuseFrame(index, -1);
    return;
  }
  LiveInformationBoxWorld world(static_cast<std::size_t>(index));
  if (!DrawInformationBoxBackdrop(world)) {
    RefuseFrame(index, static_cast<int>(FrameDecodeError::kCapacity));
  }
}

std::int32_t FlipLiveTextBoxShapePoint(std::int32_t packed_shape_point,
                                       std::int32_t flip_axes) {
  return std::bit_cast<std::int32_t>(
      FlipTextBoxShapePoint(static_cast<std::uint32_t>(packed_shape_point),
                            static_cast<std::uint32_t>(flip_axes)));
}

// decomp: BATTLE.PRG func_800CD3E4 (src/BATTLE/BATTLE.PRG/5BF94.c:4715).
void DrawLiveTextBoxFrame(std::int32_t index) {
  try {
    DrawFrame(index);
  } catch (const std::bad_alloc&) {
    RefuseFrame(index, -2);
  }
}

}  // namespace VS::Menu
