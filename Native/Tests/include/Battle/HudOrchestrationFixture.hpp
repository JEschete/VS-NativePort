#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "Battle/HudOrchestration.hpp"
#include "Render/Sprites.hpp"

namespace hud_orchestration_fixture {
namespace b = VS::Battle;
using Event = std::array<std::int64_t, 6>;

struct Sprite final : b::HudSpriteConsumer {
  VS::Render::SpriteRequest request{};
  std::uint32_t color = 0;
  std::uint32_t texture = 0;
  void SetColor(std::uint32_t value) override { color = value; }
  void SetTexture(std::uint32_t value) override { texture = value; }
  void ModulateColor(std::int32_t delta) override {
    color += static_cast<std::uint32_t>(delta);
  }
};

class World final : public b::HudOrchestrationWorld {
 public:
  b::HudPresentationState state{};
  std::array<std::uint8_t, 12> closing{};
  std::array<std::uint8_t, 20> displacement{};
  std::array<std::uint32_t, 8> tones{};
  std::array<std::array<b::HudPoint, 4>, 2> markers{};
  std::array<std::array<b::HudElementGeometry, 4>, 2> icons{};
  std::array<std::array<std::uint16_t, 4>, 2> textures{};
  std::array<std::int32_t, 4> digits{};
  std::array<b::HudElementGeometry, 10> limbs{};
  std::array<std::uint32_t, 10> limb_textures{};
  std::array<std::uint32_t, 5> limb_colors{};
  std::array<b::HudLimbCondition, 5> conditions{};
  std::array<std::int16_t, 4> vitals{12, 24, 6, 12};
  std::uint16_t risk = 1;
  std::uint32_t status_flags = 0;
  std::int16_t box_state = 0;
  std::uint8_t box_duration = 0;
  std::int32_t ticks = 2;
  std::int32_t sine = 0;
  std::int32_t scene_owner = 100;
  std::int32_t overlay_owner = 200;
  std::int32_t captured_scene = 100;
  std::array<Sprite, 64> sprites{};
  std::size_t sprite_count = 0;
  std::vector<Event> events;
  std::function<void(World&, const Event&)> callback;
  int refuse = 0;

  World() {
    for (std::size_t index = 0; index < closing.size(); ++index) {
      closing[index] = static_cast<std::uint8_t>(index + 1);
    }
    closing[10] = 10;
    for (std::size_t index = 0; index < displacement.size(); ++index) {
      displacement[index] = static_cast<std::uint8_t>(index);
    }
    tones.fill(static_cast<std::uint32_t>(b::HudShortcutTone::kEnabled));
    state.carousel = {255, 255, 255, 0};
    conditions.fill(b::HudLimbCondition::kExcellent);
  }

  void Emit(Event event) {
    events.push_back(event);
    if (callback) callback(*this, event);
  }
  b::HudPresentationState Presentation() override { return state; }
  void SetShortcutRequest(std::uint8_t value) override {
    state.commands.shortcut = value;
  }
  void SetShortcutStep(std::uint8_t value) override {
    state.shortcut_step = value;
  }
  void SetIndicatorStep(std::uint8_t value) override {
    state.indicator_step = value;
  }
  void SetIndicatorPhase(std::uint16_t value) override {
    state.indicator_phase = value;
  }
  void SetSlide(std::int32_t value) override { state.slide = value; }
  void SetCarousel(std::size_t slot, std::uint8_t value) override {
    state.carousel.at(slot) = value;
  }
  std::optional<std::uint8_t> ClosingShortcutStep(std::uint8_t step) override {
    if (refuse == 1 || step >= closing.size()) return std::nullopt;
    return closing.at(step);
  }
  std::optional<std::uint8_t> ShortcutDisplacement(std::uint8_t step) override {
    if (refuse == 2 || step >= displacement.size()) return std::nullopt;
    return displacement.at(step);
  }
  std::optional<b::HudShortcutTone> ShortcutTone(int shortcut) override {
    Emit({1, shortcut, 0, 0, 0, 0});
    if (refuse == 3) return std::nullopt;
    return static_cast<b::HudShortcutTone>(tones.at(shortcut));
  }
  std::optional<b::HudPoint> ShortcutMarkerPosition(b::HudShortcutRow row,
                                                    std::size_t slot) override {
    if (refuse == 4) return std::nullopt;
    return markers.at(static_cast<std::size_t>(row)).at(slot);
  }
  std::optional<b::HudElementGeometry> ShortcutIconGeometry(
      b::HudShortcutRow row, std::size_t slot) override {
    if (refuse == 5) return std::nullopt;
    return icons.at(static_cast<std::size_t>(row)).at(slot);
  }
  std::optional<std::uint16_t> ShortcutIconTexture(b::HudShortcutRow row,
                                                   std::size_t slot) override {
    if (refuse == 6) return std::nullopt;
    return textures.at(static_cast<std::size_t>(row)).at(slot);
  }
  std::optional<std::int32_t> ShortcutDigitPosition(
      std::size_t digit) override {
    if (refuse == 7) return std::nullopt;
    return digits.at(digit);
  }
  bool DrawShortcutMarker(std::int32_t position,
                          b::HudShortcutTone tone) override {
    Emit({2, position, static_cast<int>(tone), 0, 0, 0});
    return refuse != 8;
  }
  b::HudSpriteConsumer* QueueSprite(
      b::HudPresentationLayer layer,
      const VS::Render::SpriteRequest& request) override {
    if (refuse == 9) return nullptr;
    auto& sprite = sprites.at(sprite_count++);
    sprite.request = request;
    sprite.color =
        VS::Render::BuildSpritePacket(request, 0)[VS::Render::kSpriteColorWord];
    sprite.texture = 0;
    const bool page =
        layer != b::HudPresentationLayer::kIndicator && request.draw_mode != 0;
    const auto chain =
        layer == b::HudPresentationLayer::kShortcut
            ? overlay_owner
            : (layer == b::HudPresentationLayer::kIndicator
                   ? scene_owner - 12
                   : captured_scene -
                         (layer == b::HudPresentationLayer::kLimbs ? 4 : 8));
    Emit({3, page ? 1 : 0, request.brightness_transparency,
          static_cast<std::uint32_t>(request.packed_position),
          static_cast<std::uint32_t>(request.packed_size), chain});
    return &sprite;
  }
  void DrawShortcutPanel() override {
    captured_scene = scene_owner;
    Emit({4, 0, 0, 0, 0, 0});
  }
  std::int32_t Sine(std::uint16_t phase) override {
    Emit({5, phase, 0, 0, 0, 0});
    return sine;
  }
  std::int32_t TickSpeed() override { return ticks; }
  bool LimbDisplayAllowed() override {
    return !state.commands.SuppressesLimbDisplay() &&
           (box_state == 0 || box_state != box_duration + 1);
  }
  std::optional<b::HudElementGeometry> LimbGeometry(
      std::size_t layout) override {
    if (refuse == 10 || layout >= limbs.size()) return std::nullopt;
    return limbs.at(layout);
  }
  std::optional<b::HudLimbCondition> CurrentLimbCondition(
      std::size_t limb) override {
    Emit({9, static_cast<std::int64_t>(limb), 0, 0, 0, 0});
    if (refuse == 11) return std::nullopt;
    return conditions.at(limb);
  }
  std::optional<std::uint32_t> LimbColor(
      b::HudLimbCondition condition) override {
    const auto index = static_cast<std::size_t>(condition);
    if (refuse == 12 || index >= limb_colors.size()) return std::nullopt;
    return limb_colors.at(index);
  }
  std::optional<std::uint32_t> LimbTexture(std::size_t layout) override {
    if (refuse == 13 || layout >= limb_textures.size()) return std::nullopt;
    return limb_textures.at(layout);
  }
  bool DrawActorGauge(b::HudGaugeKind kind) override {
    const auto index = static_cast<std::size_t>(kind) * 2;
    Emit({6, static_cast<int>(kind), vitals.at(index), vitals.at(index + 1), 0,
          0});
    return refuse != 14;
  }
  std::uint16_t ActorRisk() override { return risk; }
  bool DrawRiskGauge(std::uint16_t value) override {
    Emit({6, 2, value, 100, 0, 0});
    return refuse != 15;
  }
  bool DrawZeroRisk(std::int32_t position) override {
    Emit({7, position, scene_owner - 12, 0, 0, 0});
    return refuse != 16;
  }
  std::optional<std::uint32_t> ActorStatusFlags() override {
    Emit({8, 0, 0, 0, 0, 0});
    if (refuse == 17) return std::nullopt;
    return status_flags;
  }
  bool DrawStatusIcon(const b::SlidingHudStatusIcon& icon) override {
    Emit({10, icon.icon, icon.slide, icon.slot, 0, 0});
    return refuse != 18;
  }
};
}  // namespace hud_orchestration_fixture
