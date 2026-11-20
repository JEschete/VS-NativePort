// Hand-maintained (tools/port/owned.csv).
// Live binding for the palette pulse and appearance transitions
// (AppearanceTransition.cpp).
#include "Actors/AppearanceTransitionLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <span>

#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/Movement.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/WeaponModel.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Render/Palette.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::CVECTOR;
using VS::Core::Gte::SVECTOR;

namespace VS::Actors {
namespace {
constexpr std::size_t kEquipmentPerActor = 2;
constexpr int kGimmickClutRow = 31;
constexpr int kActorClutRowBase = 4;
constexpr int kPulseActorClutRowBase = 22;
constexpr int kEquipmentClutStride = 48;
constexpr int kCarryFirstBone = 0xF0;
constexpr int kCarrySecondBone = 0xF1;
constexpr int kReleasedGimmickHeight = 60;
constexpr std::uint8_t kReleasedGimmickMode = 0xFC;
constexpr std::uint8_t kDetachedParent = 0xFF;
constexpr int kFacingMask = 0xFFF;
constexpr int kCollisionDiagonalAngle = 0x200;
constexpr int kFixedOne = 4096;
constexpr int kDeathAnimationProbe = 0xAB;
constexpr int kFallingDeathAnimation = 0x47;
constexpr int kDeathBlendFrames = 8;
constexpr unsigned kStateMask = 0xF;
// The model color's RGB channels and the appearance delay counter.
constexpr std::size_t kColorBytes = kRgbChannels + 1;
constexpr std::size_t kDelayByte = kRgbChannels;
using ColorBytes = std::array<std::uint8_t, kColorBytes>;

void Require(bool valid, const char* message) {
  if (!valid) {
    VS::Core::Errorf("actors", "%s", message);
    std::abort();
  }
}

using Equipment = std::array<VS::Actors::WeaponModel*, kEquipmentPerActor>;

// The actor's weapon and shield models; slots past the 20-entry table (actor
// slots 10 to 16) have none (#260).
Equipment EquipmentFor(int slot) {
  Equipment models{};
  if (slot < 0) {
    return models;
  }
  const auto first = static_cast<std::size_t>(slot) * kEquipmentPerActor;
  if (first + kEquipmentPerActor <= std::size(weaponModels)) {
    std::copy_n(std::begin(weaponModels) + static_cast<std::ptrdiff_t>(first),
                kEquipmentPerActor, models.begin());
  }
  return models;
}

// An equipment palette clamped to its storage (#260).
std::span<std::uint16_t> EquipmentPalette(
    std::span<std::uint16_t> palette, const VS::Actors::WeaponModel& weapon) {
  return palette.first(
      std::min<std::size_t>(weapon.palette_color_count, palette.size()));
}

bool ApplyTransition(std::span<std::uint16_t> colors,
                     std::span<std::uint16_t> reference, PaletteTransition mode,
                     int speed) {
  Require(reference.size() >= colors.size(),
          "palette transition reference is shorter than its colors");
  return ApplyPaletteTransition(colors, reference, mode, speed);
}

void UploadClut(std::span<const std::uint16_t> colors, int row, int offset) {
  (void)VS::Render::LoadLiveClutRow(static_cast<std::uint32_t>(row),
                                    static_cast<std::uint32_t>(offset), colors);
}

std::uint8_t StateBits(int state) {
  return static_cast<std::uint8_t>(static_cast<unsigned>(state) & kStateMask);
}

AppearanceColors ColorsFrom(const ColorBytes& color, std::uint8_t fade_level,
                            std::uint8_t fade_target,
                            const VS::Render::ColorCode& tint) {
  AppearanceColors colors;
  std::copy_n(color.begin(), colors.rgb.size(), colors.rgb.begin());
  colors.delay = color[kDelayByte];
  colors.fade_level = fade_level;
  colors.fade_target = fade_target;
  colors.tint = {tint.red, tint.green, tint.blue};
  return colors;
}

void ColorsTo(const AppearanceColors& colors, ColorBytes& color,
              std::uint8_t& fade_level) {
  std::copy(colors.rgb.begin(), colors.rgb.end(), color.begin());
  color[kDelayByte] = colors.delay;
  fade_level = colors.fade_level;
}

int TransitionSpeed() { return VS::State::tickSpeed / 2; }

class LiveActorAppearance final : public ActorAppearance {
 public:
  explicit LiveActorAppearance(VS::Actors::ActorRecord& actor)
      : actor_(actor) {}

  [[nodiscard]] int TransitionState() const override {
    return actor_.instance.action_animation_state;
  }

  [[nodiscard]] AppearanceTransition LoadTransition() const override {
    const CVECTOR& color = actor_.instance.base_color;
    return {.state = actor_.instance.action_animation_state,
            .speed = TransitionSpeed(),
            .colors = ColorsFrom({color.r, color.g, color.b, color.cd},
                                 actor_.fade_red, actor_.fade_green,
                                 actor_.instance.tint)};
  }

  void StoreTransition(const AppearanceTransition& transition) override {
    ColorBytes color{};
    ColorsTo(transition.colors, color, actor_.fade_red);
    CVECTOR& base = actor_.instance.base_color;
    base.r = color[0];
    base.g = color[1];
    base.b = color[2];
    base.cd = color[kDelayByte];
    actor_.instance.action_animation_state = StateBits(transition.state);
  }

  bool ApplyPalettes(PaletteTransition mode, int speed) override {
    const std::span<std::uint16_t> palette(actor_.base_palette);
    const std::size_t count = Count();
    const std::size_t skip = Skip();
    bool changed = ApplyTransition(
        palette.subspan(kActorPaletteColors + skip, count - skip),
        palette.subspan(skip, count - skip), mode, speed);
    for (VS::Actors::WeaponModel* weapon : EquipmentFor(actor_.instance.slot)) {
      if (weapon != nullptr) {
        changed |= ApplyTransition(
            EquipmentPalette(weapon->upload_palette, *weapon),
            EquipmentPalette(weapon->working_palette, *weapon), mode, speed);
      }
    }
    return changed;
  }

  void UploadPalettes(int state) override {
    const int row = actor_.instance.slot + kActorClutRowBase;
    UploadClut(std::span<const std::uint16_t>(actor_.base_palette)
                   .subspan(kActorPaletteColors, Count()),
               row, 0);
    const Equipment equipment = EquipmentFor(actor_.instance.slot);
    for (std::size_t index = 0; index < equipment.size(); ++index) {
      VS::Actors::WeaponModel* weapon = equipment[index];
      if (weapon != nullptr) {
        weapon->owner_motion_state = StateBits(state);
        UploadClut(EquipmentPalette(weapon->upload_palette, *weapon), row,
                   kActorPaletteColors +
                       kEquipmentClutStride * static_cast<int>(index));
      }
    }
  }

  void PublishState(int state) override {
    for (VS::Actors::WeaponModel* weapon : EquipmentFor(actor_.instance.slot)) {
      if (weapon != nullptr) {
        weapon->owner_motion_state = StateBits(state);
      }
    }
  }

  // The carried gimmick drops at the actor's position between its carry
  // bones, facing with the actor.
  void ReleaseCarriedGimmick() override {
    if (!actor_.action_pose_active) {
      return;
    }
    VS::Field::GimmickInstance* object =
        VS::Field::gimmickInstances[actor_.support_gimmick];
    Require(object != nullptr, "carrying actor has no carried gimmick");
    SVECTOR first;
    SVECTOR second;
    LiveActorTaggedBonePosition(object->parent, kCarryFirstBone, &first, 1);
    LiveActorTaggedBonePosition(object->parent, kCarrySecondBone, &second, 1);
    object->position_x = actor_.instance.position.vx;
    object->position_z = actor_.instance.position.vz;
    object->position_y = VS::Core::Math::WrapSigned16(
        (first.vy + second.vy) / 2 + kReleasedGimmickHeight);
    object->mode = kReleasedGimmickMode;
    object->slide_x = object->velocity_y = 0;
    object->parent = kDetachedParent;
    object->rotation_y = static_cast<std::uint16_t>(
        (object->rotation_y +
         static_cast<std::uint16_t>(actor_.instance.facing)) &
        kFacingMask);
    actor_.action_pose_active = 0;
    VS::Actors::pushedGimmickIndex = actor_.support_gimmick;
  }

  [[nodiscard]] int MovementMode() const override {
    return actor_.instance.movement_mode;
  }
  [[nodiscard]] bool IsPlayer() const override {
    return actor_.instance.slot == 0;
  }

  void SettleDeathMotion() override {
    const int diagonal = VS::Core::Math::Sine(kCollisionDiagonalAngle) *
                         actor_.collision_radius / kFixedOne;
    VS::Field::LiveSampleActorCollisionPerimeter(&actor_, diagonal);
    VS::Field::LiveApplyActorWallRepulsion(&actor_,
                                           &actor_.instance.root_motion);
    actor_.instance.root_motion.vx /= 2;
    actor_.instance.root_motion.vz /= 2;
    UpdateLiveActorJumpAndFall(actor_.instance.slot, &actor_);
  }

  void PlayFallingDeathAnimation() override {
    if (FindLiveAnimationHeader(actor_, kDeathAnimationProbe).status ==
        AnimationHeaderStatus::kFound) {
      actor_.animation_locked = 0;
      PlayLiveActorAnimation(actor_.instance.slot, kFallingDeathAnimation, 0,
                             kDeathBlendFrames);
      actor_.animation_locked = 1;
    }
  }

 private:
  [[nodiscard]] std::size_t Count() const {
    return actor_.texture_header_bit ? kActorPaletteColors
                                     : kSmallPaletteColors;
  }
  [[nodiscard]] std::size_t Skip() const {
    return actor_.texture_header_bit ? 1 : 0;
  }

  VS::Actors::ActorRecord& actor_;
};

class LiveGimmickAppearance final : public AppearanceSubject {
 public:
  explicit LiveGimmickAppearance(VS::Field::GimmickInstance& object)
      : object_(object) {}

  bool ApplyPalettes(PaletteTransition mode, int speed) override {
    return ApplyTransition(object_.pulsed_palette, object_.base_palette, mode,
                           speed);
  }
  void UploadPalettes(int /*state*/) override {
    UploadClut(object_.pulsed_palette, kGimmickClutRow,
               object_.slot * kSmallPaletteColors);
  }
  void PublishState(int /*state*/) override {}
  void ReleaseCarriedGimmick() override {}

 private:
  VS::Field::GimmickInstance& object_;
};
}  // namespace

// func_800AB788 (BATTLE.PRG 40564.c:1651) for an actor's selected palette.
void UpdateLiveActorPalettePulse(VS::Actors::ActorRecord& actor) {
  const std::size_t count =
      actor.texture_header_bit ? kActorPaletteColors : kSmallPaletteColors;
  PalettePulse pulse{actor.instance.palette_pulse.level,
                     actor.instance.palette_pulse.flags};
  const std::span<std::uint16_t> destination =
      std::span<std::uint16_t>(actor.current_palette).first(count);
  (void)UpdatePalettePulse(
      pulse, VS::State::tickSpeed,
      std::span<const std::uint16_t>(actor.base_palette).first(count),
      destination);
  actor.instance.palette_pulse.level = pulse.level;
  actor.instance.palette_pulse.flags = pulse.flags;
  UploadClut(destination, actor.instance.slot + kPulseActorClutRowBase, 0);
}

// func_800AB788 (BATTLE.PRG 40564.c:1651) for a gimmick's 16-color palette.
void UpdateLiveGimmickPalettePulse(VS::Field::GimmickInstance& object) {
  PalettePulse pulse{object.palette_pulse.level, object.palette_pulse.flags};
  (void)UpdatePalettePulse(pulse, VS::State::tickSpeed, object.base_palette,
                           object.pulsed_palette);
  object.palette_pulse.level = pulse.level;
  object.palette_pulse.flags = pulse.flags;
  UploadClut(object.pulsed_palette, kGimmickClutRow,
             object.slot * kSmallPaletteColors);
}

// func_800AB9A4 (BATTLE.PRG 40564.c:1769). Native actors own both selected
// palette rows; current_palette is a separate pulse buffer and is not the
// transition row.
void UpdateLiveActorAppearanceTransition(VS::Actors::ActorRecord& actor) {
  LiveActorAppearance subject(actor);
  UpdateActorAppearanceTransition(subject);
}

// Gimmicks keep their own state and palettes; the death fade runs at double
// speed.
void UpdateLiveGimmickAppearanceTransition(VS::Field::GimmickInstance& object) {
  ColorBytes color{};
  static_assert(sizeof object.base_color == color.size());
  std::memcpy(color.data(), &object.base_color, color.size());
  AppearanceTransition transition{
      .state = object.motion_state,
      .speed = TransitionSpeed(),
      .colors =
          ColorsFrom(color, object.fade_red, object.fade_green, object.tint)};
  if (transition.state == kAppearanceDeathFade) {
    transition.speed *= 2;
  }
  LiveGimmickAppearance subject(object);
  AdvanceAppearanceTransition(transition, subject);
  ColorsTo(transition.colors, color, object.fade_red);
  std::memcpy(&object.base_color, color.data(), color.size());
  object.motion_state = StateBits(transition.state);
}
}  // namespace VS::Actors
