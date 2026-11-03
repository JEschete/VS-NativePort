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

#include "Core/Math/ScalarMath.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/WeaponModel.h"
#include "Core/Diagnostics.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Render/Palette.h"
#include "State/Timers.h"
}

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
constexpr u_char kReleasedGimmickMode = 0xFC;
constexpr u_char kDetachedParent = 0xFF;
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
    Core_Errorf("actors", "%s", message);
    std::abort();
  }
}

using Equipment = std::array<Actors_WeaponModel*, kEquipmentPerActor>;

// The actor's weapon and shield models; slots past the 20-entry table (actor
// slots 10 to 16) have none (#260).
Equipment EquipmentFor(int slot) {
  Equipment models{};
  if (slot < 0) {
    return models;
  }
  const auto first = static_cast<std::size_t>(slot) * kEquipmentPerActor;
  if (first + kEquipmentPerActor <= std::size(Actors_weaponModels)) {
    std::copy_n(
        std::begin(Actors_weaponModels) + static_cast<std::ptrdiff_t>(first),
        kEquipmentPerActor, models.begin());
  }
  return models;
}

// An equipment palette clamped to its storage (#260).
std::span<u_short> EquipmentPalette(std::span<u_short> palette,
                                    const Actors_WeaponModel& weapon) {
  return palette.first(
      std::min<std::size_t>(weapon.paletteColorCount, palette.size()));
}

bool ApplyTransition(std::span<u_short> colors, std::span<u_short> reference,
                     PaletteTransition mode, int speed) {
  Require(reference.size() >= colors.size(),
          "palette transition reference is shorter than its colors");
  return Render_ApplyPaletteTransition(
             colors.data(), static_cast<int>(colors.size()), speed,
             static_cast<int>(mode), reference.data()) != 0;
}

void UploadClut(std::span<const u_short> colors, int row, int offset) {
  Render_LoadClutColors(colors.data(), static_cast<u_int>(row),
                        static_cast<u_int>(offset),
                        static_cast<u_int>(colors.size()));
}

u_char StateBits(int state) {
  return static_cast<u_char>(static_cast<unsigned>(state) & kStateMask);
}

AppearanceColors ColorsFrom(const ColorBytes& color, u_char fade_level,
                            u_char fade_target, const Render_ColorCode& tint) {
  AppearanceColors colors;
  std::copy_n(color.begin(), colors.rgb.size(), colors.rgb.begin());
  colors.delay = color[kDelayByte];
  colors.fade_level = fade_level;
  colors.fade_target = fade_target;
  colors.tint = {tint.red, tint.green, tint.blue};
  return colors;
}

void ColorsTo(const AppearanceColors& colors, ColorBytes& color,
              u_char& fade_level) {
  std::copy(colors.rgb.begin(), colors.rgb.end(), color.begin());
  color[kDelayByte] = colors.delay;
  fade_level = colors.fade_level;
}

int TransitionSpeed() { return State_tickSpeed / 2; }

class LiveActorAppearance final : public ActorAppearance {
 public:
  explicit LiveActorAppearance(Actors_ActorRecord& actor) : actor_(actor) {}

  [[nodiscard]] int TransitionState() const override {
    return actor_.instance.actionAnimationState;
  }

  [[nodiscard]] AppearanceTransition LoadTransition() const override {
    const CVECTOR& color = actor_.instance.baseColor;
    return {.state = actor_.instance.actionAnimationState,
            .speed = TransitionSpeed(),
            .colors = ColorsFrom({color.r, color.g, color.b, color.cd},
                                 actor_.fadeRed, actor_.fadeGreen,
                                 actor_.instance.tint)};
  }

  void StoreTransition(const AppearanceTransition& transition) override {
    ColorBytes color{};
    ColorsTo(transition.colors, color, actor_.fadeRed);
    CVECTOR& base = actor_.instance.baseColor;
    base.r = color[0];
    base.g = color[1];
    base.b = color[2];
    base.cd = color[kDelayByte];
    actor_.instance.actionAnimationState = StateBits(transition.state);
  }

  bool ApplyPalettes(PaletteTransition mode, int speed) override {
    const std::span<u_short> palette(actor_.basePalette);
    const std::size_t count = Count();
    const std::size_t skip = Skip();
    bool changed = ApplyTransition(
        palette.subspan(kActorPaletteColors + skip, count - skip),
        palette.subspan(skip, count - skip), mode, speed);
    for (Actors_WeaponModel* weapon : EquipmentFor(actor_.instance.slot)) {
      if (weapon != nullptr) {
        changed |= ApplyTransition(
            EquipmentPalette(weapon->uploadPalette, *weapon),
            EquipmentPalette(weapon->workingPalette, *weapon), mode, speed);
      }
    }
    return changed;
  }

  void UploadPalettes(int state) override {
    const int row = actor_.instance.slot + kActorClutRowBase;
    UploadClut(std::span<const u_short>(actor_.basePalette)
                   .subspan(kActorPaletteColors, Count()),
               row, 0);
    const Equipment equipment = EquipmentFor(actor_.instance.slot);
    for (std::size_t index = 0; index < equipment.size(); ++index) {
      Actors_WeaponModel* weapon = equipment[index];
      if (weapon != nullptr) {
        weapon->ownerMotionState = StateBits(state);
        UploadClut(EquipmentPalette(weapon->uploadPalette, *weapon), row,
                   kActorPaletteColors +
                       kEquipmentClutStride * static_cast<int>(index));
      }
    }
  }

  void PublishState(int state) override {
    for (Actors_WeaponModel* weapon : EquipmentFor(actor_.instance.slot)) {
      if (weapon != nullptr) {
        weapon->ownerMotionState = StateBits(state);
      }
    }
  }

  // The carried gimmick drops at the actor's position between its carry
  // bones, facing with the actor.
  void ReleaseCarriedGimmick() override {
    if (!actor_.actionPoseActive) {
      return;
    }
    Field_GimmickInstance* object =
        Field_gimmickInstances[actor_.supportGimmick];
    Require(object != nullptr, "carrying actor has no carried gimmick");
    SVECTOR first;
    SVECTOR second;
    Actors_ActorTaggedBonePosition(object->parent, kCarryFirstBone, &first, 1);
    Actors_ActorTaggedBonePosition(object->parent, kCarrySecondBone, &second,
                                   1);
    object->positionX = actor_.instance.position.vx;
    object->positionZ = actor_.instance.position.vz;
    object->positionY = VS::Core::Math::WrapSigned16(
        (first.vy + second.vy) / 2 + kReleasedGimmickHeight);
    object->mode = kReleasedGimmickMode;
    object->slideX = object->velocityY = 0;
    object->parent = kDetachedParent;
    object->rotationY = static_cast<u_short>(
        (object->rotationY + static_cast<u_short>(actor_.instance.facing)) &
        kFacingMask);
    actor_.actionPoseActive = 0;
    Actors_pushedGimmickIndex = actor_.supportGimmick;
  }

  [[nodiscard]] int MovementMode() const override {
    return actor_.instance.movementMode;
  }
  [[nodiscard]] bool IsPlayer() const override {
    return actor_.instance.slot == 0;
  }

  void SettleDeathMotion() override {
    const int diagonal = VS::Core::Math::Sine(kCollisionDiagonalAngle) *
                         actor_.collisionRadius / kFixedOne;
    Field_SampleActorCollisionPerimeter(&actor_, diagonal);
    Field_ApplyActorWallRepulsion(&actor_, &actor_.instance.rootMotion);
    actor_.instance.rootMotion.vx /= 2;
    actor_.instance.rootMotion.vz /= 2;
    Actors_UpdateActorJumpAndFall(actor_.instance.slot, &actor_);
  }

  void PlayFallingDeathAnimation() override {
    u_short* header = nullptr;
    Actors_FindActorAnimationHeader(&actor_, kDeathAnimationProbe, &header);
    if (header != nullptr) {
      actor_.animationLocked = 0;
      Actors_PlayActorAnimation(actor_.instance.slot, kFallingDeathAnimation, 0,
                                kDeathBlendFrames);
      actor_.animationLocked = 1;
    }
  }

 private:
  [[nodiscard]] std::size_t Count() const {
    return actor_.textureHeaderBit ? kActorPaletteColors : kSmallPaletteColors;
  }
  [[nodiscard]] std::size_t Skip() const {
    return actor_.textureHeaderBit ? 1 : 0;
  }

  Actors_ActorRecord& actor_;
};

class LiveGimmickAppearance final : public AppearanceSubject {
 public:
  explicit LiveGimmickAppearance(Field_GimmickInstance& object)
      : object_(object) {}

  bool ApplyPalettes(PaletteTransition mode, int speed) override {
    return ApplyTransition(object_.pulsedPalette, object_.basePalette, mode,
                           speed);
  }
  void UploadPalettes(int /*state*/) override {
    UploadClut(object_.pulsedPalette, kGimmickClutRow,
               object_.slot * kSmallPaletteColors);
  }
  void PublishState(int /*state*/) override {}
  void ReleaseCarriedGimmick() override {}

 private:
  Field_GimmickInstance& object_;
};
}  // namespace

// func_800AB788 (BATTLE.PRG 40564.c:1651) for an actor's selected palette.
void UpdateLiveActorPalettePulse(Actors_ActorRecord& actor) {
  const std::size_t count =
      actor.textureHeaderBit ? kActorPaletteColors : kSmallPaletteColors;
  PalettePulse pulse{actor.instance.palettePulse.level,
                     actor.instance.palettePulse.flags};
  const std::span<u_short> destination =
      std::span<u_short>(actor.currentPalette).first(count);
  (void)UpdatePalettePulse(
      pulse, State_tickSpeed,
      std::span<const u_short>(actor.basePalette).first(count), destination);
  actor.instance.palettePulse.level = pulse.level;
  actor.instance.palettePulse.flags = pulse.flags;
  UploadClut(destination, actor.instance.slot + kPulseActorClutRowBase, 0);
}

// func_800AB788 (BATTLE.PRG 40564.c:1651) for a gimmick's 16-color palette.
void UpdateLiveGimmickPalettePulse(Field_GimmickInstance& object) {
  PalettePulse pulse{object.palettePulse.level, object.palettePulse.flags};
  (void)UpdatePalettePulse(pulse, State_tickSpeed, object.basePalette,
                           object.pulsedPalette);
  object.palettePulse.level = pulse.level;
  object.palettePulse.flags = pulse.flags;
  UploadClut(object.pulsedPalette, kGimmickClutRow,
             object.slot * kSmallPaletteColors);
}

// func_800AB9A4 (BATTLE.PRG 40564.c:1769). Native actors own both selected
// palette rows; currentPalette is a separate pulse buffer and is not the
// transition row.
void UpdateLiveActorAppearanceTransition(Actors_ActorRecord& actor) {
  LiveActorAppearance subject(actor);
  UpdateActorAppearanceTransition(subject);
}

// Gimmicks keep their own state and palettes; the death fade runs at double
// speed.
void UpdateLiveGimmickAppearanceTransition(Field_GimmickInstance& object) {
  ColorBytes color{};
  static_assert(sizeof object.baseColor == color.size());
  std::memcpy(color.data(), &object.baseColor, color.size());
  AppearanceTransition transition{
      .state = object.motionState,
      .speed = TransitionSpeed(),
      .colors =
          ColorsFrom(color, object.fadeRed, object.fadeGreen, object.tint)};
  if (transition.state == kAppearanceDeathFade) {
    transition.speed *= 2;
  }
  LiveGimmickAppearance subject(object);
  AdvanceAppearanceTransition(transition, subject);
  ColorsTo(transition.colors, color, object.fadeRed);
  std::memcpy(&object.baseColor, color.data(), color.size());
  object.motionState = StateBits(transition.state);
}
}  // namespace VS::Actors
