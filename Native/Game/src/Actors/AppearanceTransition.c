// Hand-maintained (tools/port/owned.csv).
#include "Actors/AppearanceTransition.h"

#include <stdint.h>

#include "Actors/Animation.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/WeaponModel.h"
#include "Core/Math/FixedPoint.h"
#include "Field/Collision.h"
#include "Render/Palette.h"
#include "State/Timers.h"

enum {
  kEquipmentSlotsPerActor = 2,
  kPaletteColors = 160,
  kSmallPaletteColors = 16,
  kGimmickClutRow = 31,
  kActorClutRowBase = 4,
  kEquipmentClutStride = 48,
  kRed = 0,
  kGreen = 1,
  kBlue = 2,
  kColorCounter = 3,
  kColorStep = 4,
  kReturnColorStep = 8,
  kFadeMaximum = 64,
  kMaximumColor = 255,
  kDelayFrames = 15,
  kCarryFirstBone = 0xF0,
  kCarrySecondBone = 0xF1,
  kReleasedGimmickHeight = 60,
  kReleasedGimmickMode = 0xFC,
  kDetachedParent = 0xFF,
  kFacingMask = 0xFFF,
  kCollisionDiagonalAngle = 0x200,
  kFallingMovementMode = 4,
  kDeathAnimationProbe = 0xAB,
  kFallingDeathAnimation = 0x47,
  kDeathBlendFrames = 8,
  kDeathFade = 2,
  kDeathDarken = 3,
  kDisappearance = 4,
  kAppearanceDelay = 6,
  kAppearanceDarken = 7,
  kAppearanceWhite = 9,
  kAppearanceRestore = 10,
  kAppearanceRestoreDone = 11,
  kAppearanceSlowDarken = 12,
  kAppearanceBlack = 14,
  kPaletteRemoveRedGreen = 0,
  kPaletteDarken = 1,
  kPaletteWhite = 2,
  kPaletteBrightenGreenBlue = 3,
  kPaletteRestore = 4,
  kPaletteSlowDarken = 5,
  kPaletteBlack = 6
};

static int SignedByte(u_char value) {
  return value <= INT8_MAX ? value : (int)value - (UINT8_MAX + 1);
}

typedef struct AppearanceTarget {
  Actors_ActorRecord* actor;
  u_short* colors;
  u_short* reference;
  u_char* color;
  u_char* fade_level;
  const u_char* fade_target;
  Render_ColorCode* tint;
  int state;
  int slot;
  int speed;
  int count;
  int skip;
} AppearanceTarget;

static int ApplyPalettes(AppearanceTarget* target, int mode) {
  int changed = Render_ApplyPaletteTransition(
      target->colors + target->skip, target->count - target->skip,
      target->speed, mode, target->reference + target->skip);
  if (target->actor != NULL) {
    for (int index = 0; index < kEquipmentSlotsPerActor; ++index) {
      Actors_WeaponModel* weapon =
          Actors_weaponModels[target->slot * kEquipmentSlotsPerActor + index];
      if (weapon != NULL) {
        changed |= Render_ApplyPaletteTransition(
            weapon->uploadPalette, weapon->paletteColorCount, target->speed,
            mode, weapon->workingPalette);
      }
    }
  }
  return changed;
}

static void UploadPalettes(AppearanceTarget* target) {
  if (target->actor == NULL) {
    Render_LoadClutColors(target->colors, kGimmickClutRow,
                          target->slot * kSmallPaletteColors,
                          kSmallPaletteColors);
    return;
  }
  Render_LoadClutColors(target->colors, target->slot + kActorClutRowBase, 0,
                        target->count);
  for (int index = 0; index < kEquipmentSlotsPerActor; ++index) {
    Actors_WeaponModel* weapon =
        Actors_weaponModels[target->slot * kEquipmentSlotsPerActor + index];
    if (weapon != NULL) {
      weapon->ownerMotionState = target->state;
      Render_LoadClutColors(weapon->uploadPalette,
                            target->slot + kActorClutRowBase,
                            kPaletteColors + kEquipmentClutStride * index,
                            weapon->paletteColorCount);
    }
  }
}

static void FinishPaletteStep(AppearanceTarget* target, int changed) {
  UploadPalettes(target);
  if (changed == 0) {
    ++target->state;
    if (target->actor != NULL) {
      for (int index = 0; index < kEquipmentSlotsPerActor; ++index) {
        Actors_WeaponModel* weapon =
            Actors_weaponModels[target->slot * kEquipmentSlotsPerActor + index];
        if (weapon != NULL) {
          weapon->ownerMotionState = target->state;
        }
      }
    }
  }
}

static void SetRgb(AppearanceTarget* target, int value) {
  target->color[kRed] = target->color[kGreen] = target->color[kBlue] = value;
}

static int IncreaseFade(AppearanceTarget* target) {
  int level = SignedByte(*target->fade_level) + target->speed * 2;
  if (level > kFadeMaximum) {
    level = kFadeMaximum;
  }
  *target->fade_level = level;
  return level != kFadeMaximum;
}

static void ReleaseCarriedGimmick(Actors_ActorRecord* actor) {
  if (actor == NULL || !actor->actionPoseActive) {
    return;
  }
  Field_GimmickInstance* object = Field_gimmickInstances[actor->supportGimmick];
  SVECTOR first;
  SVECTOR second;
  Actors_ActorTaggedBonePosition(object->parent, kCarryFirstBone, &first, 1);
  Actors_ActorTaggedBonePosition(object->parent, kCarrySecondBone, &second, 1);
  object->positionX = actor->instance.position.vx;
  object->positionZ = actor->instance.position.vz;
  object->positionY = (int16_t)Core_Math_WrapSigned16(
      (first.vy + second.vy) / 2 + kReleasedGimmickHeight);
  object->mode = kReleasedGimmickMode;
  object->slideX = object->velocityY = 0;
  object->parent = kDetachedParent;
  object->rotationY =
      (object->rotationY + (u_short)actor->instance.facing) & kFacingMask;
  actor->actionPoseActive = 0;
  Actors_pushedGimmickIndex = actor->supportGimmick;
}

static void UpdateDeathMotion(Actors_ActorRecord* actor) {
  if (actor->instance.actionAnimationState >= kAppearanceDelay ||
      actor->instance.movementMode == 0) {
    return;
  }
  int diagonal =
      Core_Gte_Sin(kCollisionDiagonalAngle) * actor->collisionRadius / ONE;
  Field_SampleActorCollisionPerimeter(actor, diagonal);
  Field_ApplyActorWallRepulsion(actor, &actor->instance.rootMotion);
  actor->instance.rootMotion.vx /= 2;
  actor->instance.rootMotion.vz /= 2;
  Actors_UpdateActorJumpAndFall(actor->instance.slot, actor);
  if (actor->instance.movementMode >= kFallingMovementMode &&
      actor->instance.slot != 0) {
    u_short* header;
    Actors_FindActorAnimationHeader(actor, kDeathAnimationProbe, &header);
    if (header != NULL) {
      actor->animationLocked = 0;
      Actors_PlayActorAnimation(actor->instance.slot, kFallingDeathAnimation, 0,
                                kDeathBlendFrames);
      actor->animationLocked = 1;
    }
  }
}

static void AdvanceDisappearance(AppearanceTarget* target) {
  ReleaseCarriedGimmick(target->actor);
  int value = target->color[kRed] - target->speed * kColorStep;
  if (value < 0) {
    value = 0;
  }
  SetRgb(target, value);
  int changed = value != 0;
  changed |= IncreaseFade(target);
  FinishPaletteStep(target, changed);
}

static void AdvanceReturnTint(AppearanceTarget* target) {
  int level = SignedByte(*target->fade_level) - target->speed * 2;
  int destination = SignedByte(*target->fade_target);
  if (level < destination) {
    level = destination;
  }
  *target->fade_level = level;
  int changed = level != destination;
  changed |= Render_StepRgbTowardTarget(target->color, (u_char*)target->tint,
                                        -target->speed * kReturnColorStep);
  changed |= ApplyPalettes(target, kPaletteRestore);
  FinishPaletteStep(target, changed);
}

static void AdvanceAppearance(AppearanceTarget* target) {
  int changed;
  switch (target->state) {
    case kDeathFade:
      FinishPaletteStep(target, ApplyPalettes(target, kPaletteRemoveRedGreen));
      break;
    case kDeathDarken:
      changed = ApplyPalettes(target, kPaletteDarken);
      if (changed == 0) {
        SetRgb(target, kFadeMaximum);
        target->color[kColorCounter] = 0;
        ApplyPalettes(target, kPaletteWhite);
      }
      FinishPaletteStep(target, changed);
      break;
    case kDisappearance:
      AdvanceDisappearance(target);
      break;
    case kAppearanceDelay:
      ++target->color[kColorCounter];
      changed = target->color[kColorCounter] < kDelayFrames;
      IncreaseFade(target);
      changed |= ApplyPalettes(target, kPaletteBrightenGreenBlue);
      FinishPaletteStep(target, changed);
      break;
    case kAppearanceDarken:
      IncreaseFade(target);
      FinishPaletteStep(target, ApplyPalettes(target, kPaletteDarken));
      break;
    case kAppearanceWhite:
      SetRgb(target, kMaximumColor);
      FinishPaletteStep(target, ApplyPalettes(target, kPaletteWhite));
      break;
    case kAppearanceRestore:
      AdvanceReturnTint(target);
      break;
    case kAppearanceRestoreDone:
      target->state = 0;
      break;
    case kAppearanceSlowDarken:
      FinishPaletteStep(target, ApplyPalettes(target, kPaletteSlowDarken));
      break;
    case kAppearanceBlack:
      *target->fade_level = kFadeMaximum;
      FinishPaletteStep(target, ApplyPalettes(target, kPaletteBlack));
      break;
    default:
      break;
  }
}

// decomp: BATTLE.PRG func_800AB9A4 (src/BATTLE/BATTLE.PRG/40564.c:1769).
// Native actors own both selected palette rows; currentPalette is a separate
// pulse buffer and must not be substituted for the transition row.
void Actors_UpdateActorAppearanceTransition(Actors_ActorRecord* actor) {
  UpdateDeathMotion(actor);
  AppearanceTarget target = {
      .actor = actor,
      .colors = actor->basePalette + kPaletteColors,
      .reference = actor->basePalette,
      .color = (u_char*)&actor->instance.baseColor,
      .fade_level = &actor->fadeRed,
      .fade_target = &actor->fadeGreen,
      .tint = &actor->instance.tint,
      .state = actor->instance.actionAnimationState,
      .slot = actor->instance.slot,
      .speed = State_tickSpeed / 2,
      .count = actor->textureHeaderBit ? kPaletteColors : kSmallPaletteColors,
      .skip = actor->textureHeaderBit ? 1 : 0};
  AdvanceAppearance(&target);
  actor->instance.actionAnimationState = target.state;
}

// Gimmick callers retain their existing state contract; no actor pointer or
// actor palette is resolved through the hardware-shaped gimmick view.
void Field_UpdateGimmickAppearanceTransition(Field_GimmickInstance* object) {
  AppearanceTarget target = {.colors = object->pulsedPalette,
                             .reference = object->basePalette,
                             .color = (u_char*)&object->baseColor,
                             .fade_level = &object->fadeRed,
                             .fade_target = &object->fadeGreen,
                             .tint = &object->tint,
                             .state = object->motionState,
                             .slot = object->slot,
                             .speed = State_tickSpeed / 2,
                             .count = kSmallPaletteColors};
  if (target.state == kDeathFade) {
    target.speed *= 2;
  }
  AdvanceAppearance(&target);
  object->motionState = target.state;
}
