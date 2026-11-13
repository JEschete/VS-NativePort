#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "Battle/FloatingLabelRenderingLive.hpp"
#include "Content/ModuleData.h"
#include "Render/UiPackets.hpp"

extern "C" {
#include "Battle/Feedback.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
}

namespace VS::test_support::floating_label_rendering {
inline constexpr std::size_t kLabelCapacity = 32;
inline constexpr std::size_t kLabelWords = 6;
inline constexpr std::size_t kPrimitiveWords = 0x20000 / sizeof(u_long);
inline constexpr std::size_t kOrderingWords = 0x2088 / sizeof(u_long);
inline constexpr std::size_t kSceneHead = 18;
inline constexpr std::size_t kOverlayHead = 2;
inline constexpr std::size_t kQuadBucket = kSceneHead - 5;
inline constexpr std::size_t kTextBucket = kSceneHead - 4;
inline constexpr std::uint32_t kEnd = 0xFFFFFF;
inline constexpr std::uint32_t kStorageSeed = 0xA7B6C5D4;
inline constexpr std::uint32_t kLengthMask = 0xFF000000;
inline constexpr std::size_t kTimerFields = 3;
using LabelWords = std::array<std::uint32_t, kLabelWords>;
using Packet = std::vector<std::uint32_t>;

inline LabelWords Words(const Battle_FloatingLabel& label) {
  return std::bit_cast<LabelWords>(label);
}

class ProductionTables {
 public:
  ProductionTables() {
    const char* path = std::getenv("VS_FLOATING_LABEL_IMAGE");
    std::ifstream input(
        path != nullptr ? path : R"(Native\data\disc\BATTLE\BATTLE.PRG)",
        std::ios::binary);
    if (!input) {
      throw std::runtime_error(
          "Floating-label integration requires extracted BATTLE.PRG");
    }
    image_.assign(std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>());
    CaptureOwners();
    Reload();
  }
  ~ProductionTables() {
    for (const auto& backup : backups_) {
      std::memcpy(backup.entry->target, backup.bytes.data(),
                  backup.bytes.size());
    }
  }
  ProductionTables(const ProductionTables&) = delete;
  ProductionTables& operator=(const ProductionTables&) = delete;

  void Reload() const {
    for (const auto& backup : backups_) {
      if (!Content::CopyModuleDataEntry(*backup.entry, image_)) {
        throw std::runtime_error("Production BATTLE data owner refused reload");
      }
    }
  }
  [[nodiscard]] std::size_t OwnerCount() const { return backups_.size(); }

 private:
  struct Owner {
    const Content_ModuleDataEntry* entry = nullptr;
    std::vector<std::uint8_t> bytes;
  };
  struct ExpectedOwner {
    void* target = nullptr;
    std::size_t capacity = 0;
  };
  std::vector<unsigned char> image_;
  std::vector<Owner> backups_;

  void CaptureOwners() {
    const std::array expected{
        ExpectedOwner{Battle_valueCaptionRects,
                      sizeof(Battle_valueCaptionRects)},
        ExpectedOwner{Battle_lockedLabelHalfHeights,
                      sizeof(Battle_lockedLabelHalfHeights)},
        ExpectedOwner{Battle_lockedLabelPieces,
                      sizeof(Battle_lockedLabelPieces)},
        ExpectedOwner{Render_compactGlyphLeftTrim,
                      sizeof(Render_compactGlyphLeftTrim)},
        ExpectedOwner{Render_compactGlyphWidths,
                      sizeof(Render_compactGlyphWidths)},
        ExpectedOwner{Render_compactGlyphIndices,
                      sizeof(Render_compactGlyphIndices)},
        ExpectedOwner{Render_variableFontGlyphWidths,
                      sizeof(Render_variableFontGlyphWidths)}};
    for (const auto& owner : expected) {
      const auto entries =
          std::span(Content_moduleData, Content_moduleDataCount);
      const auto found = std::ranges::find(entries, owner.target,
                                           &Content_ModuleDataEntry::target);
      if (found == entries.end() || found->module == nullptr ||
          std::string_view(found->module) != "BATTLE/BATTLE.PRG" ||
          found->size > owner.capacity || found->capacity != owner.capacity) {
        throw std::runtime_error("Floating-label data registry owner mismatch");
      }
      Owner backup{&*found, std::vector<std::uint8_t>(owner.capacity)};
      std::memcpy(backup.bytes.data(), owner.target, owner.capacity);
      backups_.push_back(std::move(backup));
    }
  }
};

class LiveFrame {
 private:
  Battle_FloatingLabel storage_[1][kLabelCapacity]{};

 public:
  std::span<Battle_FloatingLabel, kLabelCapacity> labels{storage_[0]};
  std::vector<u_long> primitives =
      std::vector<u_long>(kPrimitiveWords, kStorageSeed);
  std::array<u_long, kOrderingWords> ordering{};

  LiveFrame() {
    ordering.fill(kEnd);
    Render_frameBuf = 0;
    Render_legacyOrderingTables[0] = ordering.data();
    Render_legacyPrimitiveBuffers[0] = primitives.data();
    Render_frameScratch.primitiveCursor = primitives.data();
    Render_frameScratch.orderingTable = ordering.data() + kSceneHead;
    Render_frameScratch.overlayOrderingTable = ordering.data() + kOverlayHead;
    static_assert(sizeof(storage_) == sizeof(*Battle_floatingLabels));
    Battle_floatingLabels = storage_;
    Battle_floatingLabelCount = 1;
  }
  ~LiveFrame() {
    Render_frameScratch = saved_scratch_;
    Render_frameBuf = saved_frame_;
    Render_legacyOrderingTables[0] = saved_ordering_;
    Render_legacyPrimitiveBuffers[0] = saved_primitives_;
    Battle_floatingLabels = saved_labels_;
    Battle_floatingLabelCount = saved_count_;
    Battle_abilityTimingCuePosition = saved_cue_;
    Battle_abilityTimingCueFrames = saved_timers_[0];
    Battle_statusLabelStepTimer = saved_timers_[1];
    Battle_placedLabelCount = saved_timers_[2];
  }
  LiveFrame(const LiveFrame&) = delete;
  LiveFrame& operator=(const LiveFrame&) = delete;

  [[nodiscard]] std::size_t UsedWords() const {
    return static_cast<u_long*>(Render_frameScratch.primitiveCursor) -
           primitives.data();
  }
  void ClearDraws() {
    ordering.fill(kEnd);
    Render_frameScratch.primitiveCursor = primitives.data();
  }
  [[nodiscard]] std::vector<Packet> Traversal(std::size_t bucket) const {
    if (bucket >= ordering.size()) {
      throw std::runtime_error("Requested bucket left its frame owner");
    }
    std::vector<Packet> packets;
    auto handle = ordering[bucket] & kEnd;
    std::size_t steps = 0;
    while (handle != kEnd) {
      if (++steps > primitives.size() + ordering.size()) {
        throw std::runtime_error("OT traversal exceeded its bounded owners");
      }
      const auto* primitive =
          static_cast<const u_long*>(LegacyGpu_Pointer(handle));
      const auto base = reinterpret_cast<std::uintptr_t>(primitives.data());
      const auto address = reinterpret_cast<std::uintptr_t>(primitive);
      const auto table = reinterpret_cast<std::uintptr_t>(ordering.data());
      if (address >= table &&
          address - table < ordering.size() * sizeof(u_long) &&
          (address - table) % alignof(u_long) == 0) {
        if ((*primitive & kLengthMask) != 0) {
          throw std::runtime_error("An ordering-table entry contains commands");
        }
        handle = *primitive & kEnd;
        continue;
      }
      if (primitive == nullptr || address < base ||
          address - base >= primitives.size() * sizeof(u_long)) {
        throw std::runtime_error(
            "OT traversal left the current primitive owner");
      }
      const auto offset = (address - base) / sizeof(u_long);
      const auto length = *primitive >> Render::kPacketLengthShift;
      if (length >= primitives.size() - offset ||
          packets.size() > primitives.size()) {
        throw std::runtime_error("Invalid primitive length or OT cycle");
      }
      if (length != 0) {
        Packet words(primitive, primitive + length + 1);
        words.front() &= kLengthMask;
        packets.push_back(std::move(words));
      }
      handle = *primitive & kEnd;
    }
    return packets;
  }

 private:
  Render_FrameScratch saved_scratch_ = Render_frameScratch;
  int saved_frame_ = Render_frameBuf;
  void* saved_ordering_ = Render_legacyOrderingTables[0];
  void* saved_primitives_ = Render_legacyPrimitiveBuffers[0];
  decltype(Battle_floatingLabels) saved_labels_ = Battle_floatingLabels;
  std::int16_t saved_count_ = Battle_floatingLabelCount;
  SVECTOR saved_cue_ = Battle_abilityTimingCuePosition;
  std::array<std::int16_t, kTimerFields> saved_timers_{
      Battle_abilityTimingCueFrames, Battle_statusLabelStepTimer,
      Battle_placedLabelCount};
};
}  // namespace VS::test_support::floating_label_rendering
