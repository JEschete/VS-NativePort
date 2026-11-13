#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "Battle/HudRendering.hpp"

namespace hud_rendering_fixture {
namespace b = VS::Battle;
namespace r = VS::Render;

enum InputWord : std::size_t {
  kOperation,
  kIndex,
  kCurrent,
  kMaximum,
  kSlide,
  kSlot,
  kGaugePosition,
  kLabelPosition,
  kLabelWidth,
  kLabelTexture,
  kStatusTexture,
  kRestrictions,
  kSpellUnlock,
  kAbilityUnlock,
  kArtUnlock,
  kFirstNumberPen,
  kSecondNumberPen,
  kCallbackChanges,
  kUpdatedSlide,
  kUpdatedLabelPosition,
  kUpdatedLabelWidth,
  kUpdatedLabelTexture,
  kSceneOwner,
  kUpdatedSceneOwner,
  kOverlayOwner,
  kMarkerPosition,
  kMarkerBrightness,
  kInputWords,
};
using Input = std::array<std::uint32_t, kInputWords>;
using Event = std::array<std::uint32_t, 8>;
inline constexpr std::uint32_t kFirstPacket = 0x80600000;
inline constexpr std::uint32_t kPacketStride = 32;
inline constexpr std::uint32_t kGaugeOwnerOffset = 12;
inline constexpr std::uint32_t kStatusOwnerOffset = 4;
inline constexpr unsigned kGaugeCallback = 1;
inline constexpr unsigned kNumberCallback = 2;
inline constexpr unsigned kSpriteCallback = 4;

inline std::int32_t Signed(std::uint32_t value) {
  return std::bit_cast<std::int32_t>(value);
}

class World final : public b::HudGaugeWorld,
                    public b::HudStatusIconWorld,
                    public b::HudShortcutWorld {
 public:
  Input input{};
  std::vector<Event> events;
  bool layout_available = true;
  bool label_available = true;
  bool texture_available = true;
  bool restrictions_available = true;
  bool unlock_available = true;
  bool rendering_available = true;
  unsigned slide_reads = 0;
  unsigned texture_reads = 0;
  unsigned label_reads = 0;

  std::optional<std::int32_t> GaugePosition(b::HudGaugeKind) override {
    if (!layout_available) return std::nullopt;
    return Signed(input[kGaugePosition]);
  }
  std::int32_t SlideOffset() override {
    ++slide_reads;
    return Signed(input[kSlide]);
  }
  bool DrawGauge(const r::StatGaugeRequest& gauge) override {
    if (!rendering_available) return false;
    gauge_owner_ = input[kSceneOwner] - kGaugeOwnerOffset;
    events.push_back(
        {1, gauge.color_index, static_cast<std::uint32_t>(gauge.fill_width),
         gauge_owner_, static_cast<std::uint32_t>(gauge.position), 0, 0, 0});
    Change(kGaugeCallback);
    return true;
  }
  std::optional<std::int32_t> DrawNumber(
      const r::NumberRequest& number) override {
    if (!rendering_available) return std::nullopt;
    const auto pen =
        input[number_count_++ == 0 ? kFirstNumberPen : kSecondNumberPen];
    events.push_back({2, static_cast<std::uint32_t>(number.font),
                      static_cast<std::uint32_t>(number.position),
                      static_cast<std::uint32_t>(number.value), 0, pen, 0, 0});
    Change(kNumberCallback);
    return Signed(pen);
  }
  bool DrawSeparator(std::int32_t position) override {
    if (!rendering_available) return false;
    events.push_back(
        {3, 2, static_cast<std::uint32_t>(position), 0, gauge_owner_, 0, 0, 0});
    return true;
  }
  std::optional<b::HudGaugeLabelGeometry> LabelGeometry(
      b::HudGaugeKind) override {
    ++label_reads;
    if (!label_available) return std::nullopt;
    return b::HudGaugeLabelGeometry{
        .position = Signed(input[kLabelPosition]),
        .width = static_cast<std::uint8_t>(input[kLabelWidth])};
  }
  bool DrawLabel(b::HudGaugeKind, const r::SpriteRequest& sprite) override {
    if (!rendering_available) return false;
    auto event = SpriteEvent(sprite, gauge_owner_, 0);
    Change(kSpriteCallback);
    event[5] = (input[kLabelTexture] & 255U) | 0x37F60000;
    events.push_back(event);
    return true;
  }
  std::optional<std::uint16_t> StatusIconTexture(std::size_t) override {
    ++texture_reads;
    if (!texture_available) return std::nullopt;
    return static_cast<std::uint16_t>(input[kStatusTexture]);
  }
  bool DrawSprite(b::HudSpriteLayer layer,
                  const b::HudSprite& sprite) override {
    if (!rendering_available) return false;
    const auto chain = layer == b::HudSpriteLayer::kStatus
                           ? input[kSceneOwner] - kStatusOwnerOffset
                           : input[kOverlayOwner];
    auto event = SpriteEvent(sprite.geometry, chain, sprite.texture);
    Change(kSpriteCallback);
    events.push_back(event);
    return true;
  }
  std::optional<std::uint32_t> ActionRestrictions() override {
    if (!restrictions_available) return std::nullopt;
    events.push_back({5, 0, 0, 0, 0, input[kRestrictions], 0, 0});
    return input[kRestrictions];
  }
  std::optional<bool> HasSpellClass(int spell_class) override {
    return Unlock(6, static_cast<std::uint32_t>(spell_class), kSpellUnlock);
  }
  std::optional<bool> HasBattleAbility(b::AbilityKind kind) override {
    return Unlock(7, kind == b::AbilityKind::kDefense ? 1U : 0U,
                  kAbilityUnlock);
  }
  std::optional<bool> HasWeaponBreakArt() override {
    return Unlock(8, 0, kArtUnlock);
  }

 private:
  std::uint32_t gauge_owner_ = 0;
  unsigned number_count_ = 0;
  unsigned sprite_count_ = 0;

  void Change(unsigned callback) {
    if ((input[kCallbackChanges] & callback) == 0) return;
    input[kSlide] = input[kUpdatedSlide];
    input[kLabelPosition] = input[kUpdatedLabelPosition];
    input[kLabelWidth] = input[kUpdatedLabelWidth];
    input[kLabelTexture] = input[kUpdatedLabelTexture];
    input[kSceneOwner] = input[kUpdatedSceneOwner];
  }
  Event SpriteEvent(const r::SpriteRequest& sprite, std::uint32_t chain,
                    std::uint32_t texture) {
    return {4,
            static_cast<std::uint32_t>(sprite.brightness_transparency),
            static_cast<std::uint32_t>(sprite.packed_position),
            static_cast<std::uint32_t>(sprite.packed_size),
            chain,
            texture,
            sprite.draw_mode,
            kFirstPacket + sprite_count_++ * kPacketStride};
  }
  std::optional<bool> Unlock(std::uint32_t service, std::uint32_t argument,
                             InputWord result) {
    if (!unlock_available) return std::nullopt;
    events.push_back({service, argument, 0, 0, 0, input[result], 0, 0});
    return input[result] != 0;
  }
};

inline Input Seed() {
  Input input{};
  input[kGaugePosition] = 0x0012000A;
  input[kLabelPosition] = 0x000A000A;
  input[kLabelWidth] = 16;
  input[kLabelTexture] = 0x90;
  input[kStatusTexture] = 0x1080;
  input[kFirstNumberPen] = 0x00090002;
  input[kSecondNumberPen] = 0x12345678;
  input[kSceneOwner] = 0x80500100;
  input[kUpdatedSceneOwner] = 0x80510100;
  input[kOverlayOwner] = 0x80502084;
  input[kUpdatedLabelPosition] = 0xFFFF0001;
  input[kUpdatedSlide] = 0x80000000;
  input[kUpdatedLabelWidth] = 0xFE;
  input[kUpdatedLabelTexture] = 0xF9;
  return input;
}
}  // namespace hud_rendering_fixture
