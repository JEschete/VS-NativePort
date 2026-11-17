#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Battle/HudState.hpp"
#include "Menu/Status/LimbControllers.hpp"
#include "Render/Sprites.hpp"

namespace status_limb_tests {
namespace status = VS::Menu::Status;
namespace render = VS::Render;
using Event = std::vector<std::int64_t>;
inline constexpr std::size_t kPaletteSize = 10;
inline constexpr std::size_t kRowOffsets = 36;
inline constexpr std::size_t kBoneSlots = 256;
inline constexpr std::size_t kNameSlots = 20;
inline constexpr std::size_t kEquipmentStats = 64;
inline constexpr std::size_t kTextBytes = 96;
inline constexpr std::uint32_t kActorStatSalt = 257;
inline constexpr std::uint32_t kPrimitiveBase = 0x80700000;
inline constexpr std::uint32_t kLinkMask = 0xFFFFFF;
enum class EventKind : std::int64_t {
  kName,
  kCondition,
  kProjection,
  kComparison,
  kSound,
  kInformation,
  kCursor,
};

struct PacketStore {
  std::array<std::uint32_t, 3> heads{0x123456, 0x234567, 0x345678};
  std::vector<std::uint32_t> words;
  bool available = true;
};
class PacketChannel final : public render::PacketQueue {
 public:
  PacketChannel(PacketStore& store, std::size_t channel)
      : store_(store), channel_(channel) {}
  std::uint32_t ChainLink() const override { return store_.heads[channel_]; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    if (!store_.available) return false;
    store_.heads[channel_] =
        (kPrimitiveBase + static_cast<std::uint32_t>(store_.words.size() *
                                                     sizeof(std::uint32_t))) &
        kLinkMask;
    store_.words.insert(store_.words.end(), packet.begin(), packet.end());
    return true;
  }

 private:
  PacketStore& store_;
  std::size_t channel_;
};

// Only the renderer, bone projection, comparison panel, audio and cursor
// services are doubled. Nested promoted controllers and HUD policies run.
class World final : public status::StatusIconHost,
                    public status::StatusEffectNavigationHost,
                    public status::LimbPanelHost,
                    public status::SelectedLimbHost {
 public:
  World() {
    for (std::size_t index = 0; index < names.size(); ++index) {
      names[index] = static_cast<std::uint8_t>(index + 1);
      hp[index] = 100;
      max_hp[index] = 100;
      bones[index] = static_cast<std::uint8_t>(index);
    }
    for (std::size_t index = 0; index < row_offsets.size(); ++index) {
      row_offsets[index] = static_cast<std::uint8_t>(index * 7);
    }
    for (std::size_t index = 0; index < name_labels.size(); ++index) {
      name_labels[index] = {'N', static_cast<std::uint8_t>('A' + index), 0};
    }
    for (std::size_t index = 0; index < condition_labels.size(); ++index) {
      condition_labels[index] = {'C', static_cast<std::uint8_t>('A' + index),
                                 0};
    }
    for (auto& text : descriptions) text = {1, render::kTextTerminator};
    panel_frame.row_offsets = row_offsets;
  }
  bool QueueStatusSprite(const status::VitalsSprite& sprite) override {
    PacketChannel queue(packets, 2);
    auto packet =
        render::BuildSpritePacket({.draw_mode = render::kDefaultSpriteDrawMode,
                                   .brightness_transparency = sprite.brightness,
                                   .packed_position = sprite.xy,
                                   .packed_size = sprite.wh},
                                  queue.ChainLink());
    packet[render::kSpriteTextureWord] = sprite.uv_clut;
    return queue.Emit(packet);
  }
  void PlayChangeSound() override {
    events.push_back({static_cast<std::int64_t>(EventKind::kSound)});
  }
  bool ShowStatusDescription(std::uint8_t effect) override {
    if (!services_available) return false;
    description = effect;
    Information(0, description_offsets[effect], descriptions[effect]);
    return true;
  }
  std::int32_t DrawCursor(std::uint8_t animation,
                          std::int32_t position) override {
    events.push_back({static_cast<std::int64_t>(EventKind::kCursor), animation,
                      static_cast<std::uint32_t>(position)});
    return cursor_return;
  }
  std::optional<status::LimbNameIndices> Names(std::uint8_t slot) override {
    if (!actor_available || slot >= status::kInspectableActorSlots) {
      return std::nullopt;
    }
    status::LimbNameIndices result{};
    for (std::size_t limb = 0; limb < result.size(); ++limb) {
      result[limb] = NameIndex(slot, limb);
    }
    return result;
  }
  bool PopulateComparisonStats(std::uint8_t slot) override {
    events.push_back({static_cast<std::int64_t>(EventKind::kComparison), slot});
    return services_available;
  }
  bool DrawIcons(std::uint8_t slot, std::int32_t slide) override {
    return status::DrawStatusEffectIcons(*this, Flags(slot), slide,
                                         texture_offsets)
        .has_value();
  }
  bool DrawName(std::uint8_t name, std::int32_t position,
                std::optional<std::uint32_t> color) override {
    if (name >= name_labels.size()) return false;
    Label(EventKind::kName, name_offsets[name], position, color,
          name_labels[name]);
    return services_available;
  }
  std::optional<std::uint8_t> Condition(std::uint8_t slot,
                                        std::size_t limb) override {
    if (limb >= hp.size()) return std::nullopt;
    return static_cast<std::uint8_t>(
        VS::Battle::LimbCondition(Health(slot, limb)));
  }
  bool DrawCondition(std::uint8_t condition, std::int32_t position,
                     std::optional<std::uint32_t> color) override {
    if (condition >= condition_labels.size()) return false;
    Label(EventKind::kCondition, condition_offsets[condition], position, color,
          condition_labels[condition]);
    return services_available;
  }
  bool DrawIndicator(const status::LimbIndicatorRequest& request) override {
    PacketChannel queue(packets, 0);
    return status::DrawLimbConditionIndicator(queue, palette, request)
        .has_value();
  }
  std::optional<std::int32_t> ProjectAnchor(std::uint8_t slot,
                                            std::size_t limb) override {
    if (!services_available || limb >= bones.size()) return std::nullopt;
    const auto bone = static_cast<std::uint8_t>(bones[limb] + slot);
    events.push_back({static_cast<std::int64_t>(EventKind::kProjection), 1,
                      bone, static_cast<std::uint32_t>(projections[bone])});
    return projections[bone];
  }
  bool DrawLeader(const status::LimbLeaderRequest& request) override {
    PacketChannel queue(packets, 1);
    return status::DrawLimbLeaderLine(queue, request).has_value();
  }
  bool ShowInformation(std::span<const std::uint8_t> text) override {
    if (!services_available) return false;
    Information(1, 0, text);
    return true;
  }
  bool SelectLimb(std::int32_t request) override {
    selection_request = request;
    return status::UpdateLimbPanel(panel, panel_frame, request, *this)
        .has_value();
  }
  [[nodiscard]] std::uint32_t Flags(std::uint8_t slot = 0) const {
    const auto flags = VS::Battle::StatusIconFlags(
        VS::Battle::HudStatusView(std::rotl(status_bits, slot)), upper_icons);
    return flags ? *flags : 0;
  }
  [[nodiscard]] std::uint8_t NameIndex(std::uint8_t slot,
                                       std::size_t limb) const {
    const auto name = names.at(limb);
    if (name == 0) return 0;
    return static_cast<std::uint8_t>(
        (name + static_cast<std::size_t>(slot) - 1) % (kNameSlots - 1) + 1);
  }
  [[nodiscard]] VS::Battle::HudLimbHealthView Health(std::uint8_t slot,
                                                     std::size_t limb) const {
    const auto current = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(hp.at(limb)) + slot);
    const auto maximum = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(max_hp.at(limb)) + slot * 2U);
    return {std::bit_cast<std::int16_t>(current),
            std::bit_cast<std::int16_t>(maximum)};
  }
  void Information(std::int64_t bank, std::int64_t offset,
                   std::span<const std::uint8_t> text) {
    Event event{static_cast<std::int64_t>(EventKind::kInformation), bank,
                offset, static_cast<std::int64_t>(text.size())};
    event.insert(event.end(), text.begin(), text.end());
    events.push_back(std::move(event));
  }

  PacketStore packets;
  std::array<std::uint32_t, kPaletteSize> palette{};
  std::array<std::uint16_t, status::kStatusEffectSlots> texture_offsets{};
  status::LimbNameIndices names{};
  std::array<std::int16_t, status::kStatusLimbSlots> hp{};
  std::array<std::int16_t, status::kStatusLimbSlots> max_hp{};
  std::array<std::uint8_t, status::kStatusLimbSlots> bones{};
  std::array<std::int32_t, kBoneSlots> projections{};
  std::array<std::uint8_t, kRowOffsets> row_offsets{};
  std::array<std::uint16_t, VS::Battle::kHudMappedStatusCount> upper_icons{};
  std::array<std::vector<std::uint8_t>, kNameSlots> name_labels;
  std::array<std::uint32_t, kNameSlots> name_offsets{};
  std::array<std::vector<std::uint8_t>, status::kLimbConditionCount>
      condition_labels;
  std::array<std::uint32_t, status::kLimbConditionCount> condition_offsets{};
  std::array<std::uint16_t, status::kStatusEffectSlots> description_offsets{};
  std::array<std::vector<std::uint8_t>, status::kStatusEffectSlots>
      descriptions;
  std::uint32_t status_bits = 0;
  std::vector<Event> events;
  status::LimbPanelState panel;
  status::LimbPanelFrame panel_frame;
  status::StatusEffectNavigationState navigation;
  status::StatusEffectNavigationFrame navigation_frame;
  std::int32_t selection_request = 0;
  std::int32_t cursor_return = 0;
  std::uint8_t description = 0;
  bool actor_available = true;
  bool services_available = true;

 private:
  void Label(EventKind kind, std::uint32_t index, std::int32_t position,
             std::optional<std::uint32_t> color,
             const std::vector<std::uint8_t>& text) {
    Event event{static_cast<std::int64_t>(kind), index,
                static_cast<std::uint32_t>(position),
                color ? static_cast<std::int64_t>(*color) : -1,
                static_cast<std::int64_t>(text.size())};
    event.insert(event.end(), text.begin(), text.end());
    events.push_back(std::move(event));
  }
};
}  // namespace status_limb_tests
