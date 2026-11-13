// Hand-maintained (tools/port/owned.csv).
#include "Battle/HudRenderingLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>

#include "Actors/ActorTableLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/HudRendering.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Hud.h"
#include "Core/Diagnostics.h"
#include "Menu/Magic.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Battle {
namespace {
constexpr int kGaugeLayer = -3;
constexpr int kNumberLayer = -4;
constexpr int kStatusLayer = -1;
constexpr int kShortcutLayer = 0;
constexpr int kSeparatorFont = 2;
constexpr std::uint32_t kGaugeLabelClut = 0x37F60000;
// vs_battle_chainAbilitiesUnlocked reads fourteen of each sixteen-byte row.
constexpr std::size_t kQueriedBattleAbilities = 14;
constexpr int kFirstWeaponBreakArt = 0xB8;
constexpr int kWeaponBreakArtCount = 4;

[[noreturn]] void Refuse(const char* message) {
  Core_Errorf("battle.hud", "%s", message);
  std::abort();
}

std::size_t GaugeIndex(HudGaugeKind kind) {
  const auto index = static_cast<std::size_t>(kind);
  if (index >= kHudGaugeCount) Refuse("Gauge kind is outside the HUD layout");
  return index;
}

u_long* Chain(Render::LiveOrderingBase base, int layer) {
  u_long* const chain = Render::LiveFrameOrderingEntry(base, layer);
  if (chain == nullptr) Refuse("HUD render target has no ordering entry");
  return chain;
}

class CheckedHudQueue final : public Render::PacketQueue {
 public:
  explicit CheckedHudQueue(u_long* chain) : queue_(chain) {}
  [[nodiscard]] std::uint32_t ChainLink() const override {
    return queue_.ChainLink();
  }
  bool Emit(std::span<const std::uint32_t> packet) override {
    if (!queue_.Emit(packet)) Refuse("HUD packet has no primitive capacity");
    return true;
  }

 private:
  Render::LivePacketQueue queue_;
};

u_long* QueueSprite(const Render::SpriteRequest& sprite, u_long* chain) {
  if (Render::ReserveLivePrimitiveWords(Render::kSpritePacketWords) ==
      nullptr) {
    Refuse("HUD sprite has no primitive capacity");
  }
  u_long* const packet = Render::QueueLiveSpriteWithTexturePage(
      sprite.brightness_transparency, sprite.packed_position,
      sprite.packed_size, chain);
  if (packet == nullptr) Refuse("HUD sprite renderer returned no packet");
  packet[Render::kSpriteDrawModeWord] = sprite.draw_mode;
  return packet;
}

class LiveHudGaugeWorld final : public HudGaugeWorld {
 public:
  std::optional<std::int32_t> GaugePosition(HudGaugeKind kind) override {
    return std::span(Battle_hudGaugePositions)[GaugeIndex(kind)];
  }
  std::int32_t SlideOffset() override { return Battle_hudSlideOffset; }

  bool DrawGauge(const Render::StatGaugeRequest& gauge) override {
    gauge_chain_ = Chain(Render::LiveOrderingBase::kScene, kGaugeLayer);
    CheckedHudQueue queue(gauge_chain_);
    std::array<std::uint32_t, kHudGaugeCount> palette{};
    if (gauge.color_index >= palette.size()) {
      Refuse("Gauge colour is outside the HUD palette");
    }
    // Only the selected palette entry is used by DrawStatGauge.
    palette.at(gauge.color_index) = static_cast<std::uint32_t>(
        std::span(Render_kStatGaugeColors)[gauge.color_index]);
    return Render::DrawStatGauge(queue, palette, gauge);
  }

  std::optional<std::int32_t> DrawNumber(
      const Render::NumberRequest& number) override {
    CheckedHudQueue queue(
        Chain(Render::LiveOrderingBase::kScene, kNumberLayer));
    const auto rendered = Render::DrawIntegerRightAligned(queue, number);
    if (rendered.truncated) {
      Refuse("HUD number does not terminate its decimal digit loop");
    }
    return rendered.next_position;
  }

  bool DrawSeparator(std::int32_t position) override {
    if (gauge_chain_ == nullptr) Refuse("Gauge separator has no gauge owner");
    CheckedHudQueue queue(gauge_chain_);
    return Render::DrawNumericGlyph(
        queue, {.font = kSeparatorFont, .position = position, .digit = 0});
  }

  std::optional<HudGaugeLabelGeometry> LabelGeometry(
      HudGaugeKind kind) override {
    const auto index = GaugeIndex(kind);
    return HudGaugeLabelGeometry{
        .position = std::span(Battle_hudGaugeLabelPositions)[index],
        .width = static_cast<std::uint8_t>(
            std::span(Battle_hudGaugeLabelWidths)[index])};
  }

  bool DrawLabel(HudGaugeKind kind,
                 const Render::SpriteRequest& sprite) override {
    if (gauge_chain_ == nullptr) Refuse("Gauge label has no gauge owner");
    u_long* const packet = QueueSprite(sprite, gauge_chain_);
    packet[Render::kSpriteTextureWord] =
        static_cast<std::uint8_t>(
            std::span(Battle_hudGaugeLabelTexU)[GaugeIndex(kind)]) |
        kGaugeLabelClut;
    return true;
  }

 private:
  // _renderStatBar holds this owner across its external number renderers.
  u_long* gauge_chain_ = nullptr;
};

class LiveHudIconWorld final : public HudStatusIconWorld {
 public:
  std::int32_t SlideOffset() override { return Battle_hudSlideOffset; }
  std::optional<std::uint16_t> StatusIconTexture(std::size_t icon) override {
    if (icon >= kHudStatusIconCount) return std::nullopt;
    return std::span(Battle_statusIconTexOffsets)[icon];
  }
  bool DrawSprite(HudSpriteLayer layer, const HudSprite& sprite) override {
    const bool status = layer == HudSpriteLayer::kStatus;
    u_long* const packet = QueueSprite(
        sprite.geometry, Chain(status ? Render::LiveOrderingBase::kScene
                                      : Render::LiveOrderingBase::kOverlay,
                               status ? kStatusLayer : kShortcutLayer));
    packet[Render::kSpriteTextureWord] = sprite.texture;
    return true;
  }
};

bool ActionUnlocked(int action) {
  if (action < 0 ||
      static_cast<std::size_t>(action) >= std::size(Battle_actionDefinitions)) {
    Refuse("Shortcut unlock references an action outside its table");
  }
  return std::span(Battle_actionDefinitions)[static_cast<std::size_t>(action)]
             .unlocked != 0;
}

std::span<const char> SpellOwner(const u_char* ids) {
  const std::array<std::span<const char>, 4> owners{
      Menu_Magic_warlockSpellIds, Menu_Magic_shamanSpellIds,
      Menu_Magic_sorcererSpellIds, Menu_Magic_enchanterSpellIds};
  const auto address = reinterpret_cast<std::uintptr_t>(ids);
  for (const auto owner : owners) {
    const auto base = reinterpret_cast<std::uintptr_t>(owner.data());
    if (address >= base && address - base < owner.size()) {
      return owner.subspan(address - base);
    }
  }
  Refuse("Used shortcut spell row has no bounded spell-table owner");
}

void CheckSpellReadiness(int spell_class) {
  const auto index = static_cast<std::size_t>(spell_class);
  if (index >= std::size(Battle_spellClassCounts)) {
    Refuse("Shortcut spell class is outside its table");
  }
  const auto count = std::span(Battle_spellClassCounts)[index];
  if (count == 0) return;
  const auto owner = SpellOwner(std::span(Battle_spellClassSpellIds)[index]);
  for (std::size_t spell = 0; spell < count; ++spell) {
    if (spell >= owner.size()) {
      Refuse("Used shortcut spell entry is outside its bounded owner");
    }
    if (ActionUnlocked(static_cast<std::uint8_t>(owner[spell]))) return;
  }
}

class LiveHudShortcutWorld final : public HudShortcutWorld {
 public:
  std::optional<std::uint32_t> ActionRestrictions() override {
    const auto* actor = Battle_characterState;
    if (actor == nullptr || actor->stats == nullptr ||
        actor->model == nullptr ||
        Actors::LiveActorRecord(0, "ShortcutIconBrightness") == nullptr) {
      Refuse(
          "Shortcut restrictions require the current actor, stats and model");
    }
    return static_cast<std::uint32_t>(
        Menu::Framework::LiveActionRestrictionFlags(0));
  }
  std::optional<bool> HasSpellClass(int spell_class) override {
    CheckSpellReadiness(spell_class);
    return LiveHasUnlockedSpellClass(spell_class) != 0;
  }
  std::optional<bool> HasBattleAbility(AbilityKind kind) override {
    const auto ids = kind == AbilityKind::kChain
                         ? std::span(Battle_chainAbilityActionIds)
                         : std::span(Battle_defenseAbilityActionIds);
    for (const char id : ids.first<kQueriedBattleAbilities>()) {
      if (ActionUnlocked(static_cast<std::uint8_t>(id))) break;
    }
    return LiveHasUnlockedBattleAbility(kind == AbilityKind::kDefense ? 1
                                                                      : 0) != 0;
  }
  std::optional<bool> HasWeaponBreakArt() override {
    const auto* actor = Battle_characterState;
    if (actor == nullptr) Refuse("Break-art shortcut has no current actor");
    const int first =
        kFirstWeaponBreakArt +
        (actor->equippedWeaponCategory - 1) * kWeaponBreakArtCount;
    for (int tier = 0; tier < kWeaponBreakArtCount; ++tier) {
      if (ActionUnlocked(first + tier)) break;
    }
    return LiveHasUnlockedWeaponBreakArt() != 0;
  }
};
}  // namespace

// decomp: F01354 _renderStatBar (src/BATTLE/BATTLE.PRG/5BF94.c:3088).
void DrawLiveHudStatGauge(int color_index, int current, int maximum) {
  if (color_index < 0 ||
      static_cast<std::size_t>(color_index) >= kHudGaugeCount) {
    Refuse("Gauge kind is outside the HUD layout");
  }
  LiveHudGaugeWorld world;
  if (!DrawHudStatGauge(world, {.kind = static_cast<HudGaugeKind>(color_index),
                                .current = current,
                                .maximum = maximum})) {
    Refuse("HUD gauge requires its used layout and rendering services");
  }
}

// decomp: F01355 func_800C9CB4 (src/BATTLE/BATTLE.PRG/5BF94.c:3112).
void DrawLiveSlidingStatusIcon(int icon, int slide, int slot) {
  LiveHudIconWorld world;
  if (!DrawSlidingStatusIcon(world,
                             {.icon = icon, .slide = slide, .slot = slot})) {
    Refuse("Status icon requires its used texture and rendering services");
  }
}

// decomp: F01358 func_800C9EB8 (src/BATTLE/BATTLE.PRG/5BF94.c:3197).
int LiveShortcutIconBrightness(int shortcut) {
  LiveHudShortcutWorld world;
  const auto tone = ShortcutIconBrightness(world, shortcut);
  if (!tone.has_value()) Refuse("Shortcut brightness service was unavailable");
  return static_cast<int>(tone.value());
}

// decomp: F01360 func_800C9F98 (src/BATTLE/BATTLE.PRG/5BF94.c:3241).
void DrawLiveShortcutButtonMarker(int position, int brightness_transparency) {
  LiveHudIconWorld world;
  if (!DrawShortcutButtonMarker(world, position, brightness_transparency)) {
    Refuse("Shortcut button marker requires its rendering services");
  }
}
}  // namespace VS::Battle
