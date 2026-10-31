// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Actors/SequencePlayback.h"

#include <limits.h>
#include <stdint.h>

#include "ActorAssets.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Audio/ActorSound.h"
#include "Audio/Sfx.h"
#include "Battle/ActionAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
#include "Core/Core/IntPair.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/WordCopy.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Event/ScriptVm.h"
#include "Field/Actors.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/ModelDraw.h"
#include "Render/Palette.h"
#include "State/Timers.h"

static int16_t SignedTrackByte(u_char value) {
  return value <= INT8_MAX
             ? static_cast<int16_t>(value)
             : static_cast<int16_t>(static_cast<int>(value) - (UINT8_MAX + 1));
}

static int16_t SignedTrackWord(const u_char* data) {
  return Core_Math_WrapSigned16((static_cast<uint32_t>(data[0]) << CHAR_BIT) |
                                data[1]);
}

enum { kRootPoseKey = 41 };

static void AdvanceShadowBlend(Actors_ShadowBlend* shadow) {
  enum { kShadowLevelMask = 0xFFF };
  if (shadow->levelFrames != 0) {
    const int delta = shadow->levelDelta / shadow->levelFrames;
    shadow->levelDelta =
        Core_Math_WrapSigned16(static_cast<uint32_t>(shadow->levelDelta) -
                               static_cast<uint32_t>(delta));
    shadow->level += delta;
    --shadow->levelFrames;
  }
  shadow->level &= kShadowLevelMask;
  if (shadow->radiusFrames != 0) {
    const int expand = shadow->radiusExpandDelta / shadow->radiusFrames;
    shadow->radiusExpandDelta = Core_Math_WrapSigned16(
        static_cast<uint32_t>(shadow->radiusExpandDelta) -
        static_cast<uint32_t>(expand));
    shadow->radiusExpandRate += expand;
    const int shrink = shadow->radiusShrinkDelta / shadow->radiusFrames;
    shadow->radiusShrinkDelta = Core_Math_WrapSigned16(
        static_cast<uint32_t>(shadow->radiusShrinkDelta) -
        static_cast<uint32_t>(shrink));
    shadow->radiusShrinkRate += shrink;
    --shadow->radiusFrames;
  }
}

static void ApplyPoseTracks(Actors_ActorRecord* actor,
                            Actors_AnimationPose* state, int first_frame,
                            int last_frame) {
  enum {
    kRootScaleTrack = 4,
    kRootRotationTrack = 3,
    kFirstBoneTrack = 5,
    kScaleFlagsByte = 3,
    kRootTranslationDivisor = 4
  };
  u_short* header;
  Actors_SeqAnimationBank* bank = reinterpret_cast<Actors_SeqAnimationBank*>(
      Actors_FindActorAnimationHeader(actor, actor->animationId, &header));
  if (bank == nullptr || header == nullptr) return;
  Actors_AnimationTrackContext context = {
      .data = VS_ONDISC(bank, animationData, const u_char*),
      .firstFrame = first_frame,
      .lastFrame = last_frame};
  if (header[kRootScaleTrack] != 0) {
    Actors_DecodeAnimationTrack(&context, header[kRootScaleTrack],
                                &state->scaleKeys[kRootPoseKey],
                                &state->trackStates[kRootPoseKey], 0);
    const SVECTOR* delta = &state->scaleKeys[kRootPoseKey];
    actor->instance.position.vx = Core_Math_WrapSigned16(
        static_cast<uint32_t>(actor->instance.position.vx) +
        static_cast<uint32_t>(delta->vx / kRootTranslationDivisor));
    actor->instance.position.vy = Core_Math_WrapSigned16(
        static_cast<uint32_t>(actor->instance.position.vy) +
        static_cast<uint32_t>(delta->vy / kRootTranslationDivisor));
    actor->instance.position.vz = Core_Math_WrapSigned16(
        static_cast<uint32_t>(actor->instance.position.vz) +
        static_cast<uint32_t>(delta->vz / kRootTranslationDivisor));
  }
  Actors_DecodeAnimationTrack(&context, header[kRootRotationTrack],
                              &state->boneKeys[kRootPoseKey],
                              &state->reserved2A0[kRootPoseKey], 0);
  for (int bone = 0; bone < actor->instance.boneCount; ++bone) {
    Actors_DecodeAnimationTrack(&context, header[bone + kFirstBoneTrack],
                                &state->boneKeys[bone],
                                &state->reserved2A0[bone], 1);
    if ((reinterpret_cast<u_char*>(header))[kScaleFlagsByte] & 2) {
      Actors_DecodeAnimationTrack(
          &context, header[bone + actor->instance.boneCount + kFirstBoneTrack],
          &state->scaleKeys[bone], &state->trackStates[bone], 0);
    }
  }
}

// decomp: BATTLE.PRG func_800AFA28 (src/BATTLE/BATTLE.PRG/44F14.c:1252)
// Native frame ranges are local; no pointer-bearing context is placed at the
// PS1 scratchpad's end.
void Actors_AdvanceSkeletalAnimation(Actors_ActorRecord* actor,
                                     Actors_AnimationPose* state, int mode) {
  state->reachedEnd = 0;
  int first_frame = state->frame;
  for (int step = 0; step < actor->framesPerStep; ++step) {
    AdvanceShadowBlend(&actor->shadowBlend);
    if (state->frame >= static_cast<int>((u_short)actor->animationLength)) {
      state->reachedEnd = -1;
      if (actor->queuedAnimation != 0) {
        Actors_PlayActorAnimationAtFrame(
            actor->instance.slot, actor->queuedAnimation,
            actor->queuedParameterA, actor->queuedParameterB);
        actor->queuedAnimation = 0;
        return;
      }
    }
    if (actor->instance.isFrozen && state->nextEventFrame == state->frame) {
      Actors_RunAnimationEvents(actor, state, mode);
    }
    if (state->frame >= static_cast<int>((u_short)actor->animationLength)) {
      --state->frame;
      state->holdingLastFrame = -1;
    }
    ++state->frame;
  }
  if (state->frame == 1 || state->holdingLastFrame || state->eventWait != 0) {
    return;
  }
  const int last_frame = state->frame - 1;
  if (last_frame < first_frame || first_frame == 0) first_frame = 1;
  ApplyPoseTracks(actor, state, first_frame, last_frame);
}

static const u_char* DecodeTrackCommand(const u_char* data, int* next_frame,
                                        SVECTOR* delta) {
  enum {
    kByteDeltaX = 0x80,
    kByteDeltaY = 0x40,
    kByteDeltaZ = 0x20,
    kByteDeltaAxes = kByteDeltaX | kByteDeltaY | kByteDeltaZ,
    kLongRunMask = 0x1F,
    kShortRunMask = 3,
    kPackedAxisBits = 3,
    kPackedX = 4,
    kPackedY = 2,
    kPackedZ = 1
  };
  int flags = *data;
  if (flags == 0) return nullptr;
  const int frame_mask =
      (flags & kByteDeltaAxes) != 0 ? kLongRunMask : kShortRunMask;
  const int run = flags & frame_mask;
  *next_frame += run + 1;
  if (run == frame_mask) {
    *next_frame += data[1];
    data += 2;
  } else {
    ++data;
  }
  if ((flags & kByteDeltaAxes) == 0) {
    flags <<= kPackedAxisBits;
    const int packed = SignedTrackWord(data);
    data += sizeof(int16_t);
    int value = packed >> kPackedAxisBits;
    if (packed & kPackedX) {
      delta->vx = Core_Math_WrapSigned16(static_cast<uint32_t>(value));
      flags &= kByteDeltaY | kByteDeltaZ;
      if (packed & (kPackedY | kPackedZ)) {
        value = SignedTrackWord(data);
        data += sizeof(int16_t);
      }
    }
    if (packed & kPackedY) {
      delta->vy = Core_Math_WrapSigned16(static_cast<uint32_t>(value));
      flags &= kByteDeltaX | kByteDeltaZ;
      if (packed & kPackedZ) {
        value = SignedTrackWord(data);
        data += sizeof(int16_t);
      }
    }
    if (packed & kPackedZ) {
      delta->vz = Core_Math_WrapSigned16(static_cast<uint32_t>(value));
      flags &= kByteDeltaX | kByteDeltaY;
    }
  }
  if (flags & kByteDeltaX) delta->vx = SignedTrackByte(*data++);
  if (flags & kByteDeltaY) delta->vy = SignedTrackByte(*data++);
  if (flags & kByteDeltaZ) delta->vz = SignedTrackByte(*data++);
  return data;
}

// decomp: BATTLE.PRG func_800AFDE8 (src/BATTLE/BATTLE.PRG/44F14.c:1373)
// The original delta was value + 84 SVECTORs; callers now pass the matching
// pose array explicitly.
void Actors_DecodeAnimationTrack(const Actors_AnimationTrackContext* context,
                                 int offset, SVECTOR* value, SVECTOR* delta,
                                 int rotation) {
  enum { kRotationMask = 0x1FFF };
  const u_char* start = context->data + offset;
  const u_char* data = start + (u_short)value->pad;
  int next_frame = (u_short)delta->pad;
  for (int frame = context->firstFrame; frame <= context->lastFrame; ++frame) {
    while (next_frame < frame) {
      data = DecodeTrackCommand(data, &next_frame, delta);
      if (data == nullptr) return;
    }
    value->vx = Core_Math_WrapSigned16(static_cast<uint32_t>(value->vx) +
                                       static_cast<uint32_t>(delta->vx));
    value->vy = Core_Math_WrapSigned16(static_cast<uint32_t>(value->vy) +
                                       static_cast<uint32_t>(delta->vy));
    value->vz = Core_Math_WrapSigned16(static_cast<uint32_t>(value->vz) +
                                       static_cast<uint32_t>(delta->vz));
    if (rotation) {
      value->vx = static_cast<int16_t>(static_cast<uint16_t>(value->vx) &
                                       kRotationMask);
      value->vy = static_cast<int16_t>(static_cast<uint16_t>(value->vy) &
                                       kRotationMask);
      value->vz = static_cast<int16_t>(static_cast<uint16_t>(value->vz) &
                                       kRotationMask);
    }
  }
  value->pad = Core_Math_WrapSigned16(static_cast<uint32_t>(data - start));
  delta->pad = Core_Math_WrapSigned16(static_cast<uint32_t>(next_frame));
}

static int16_t BlendComponent(int16_t from, int16_t to, int weight,
                              int rotation) {
  enum { kFixedFractionBits = 12 };
  int delta = static_cast<int>(to) - from;
  if (rotation) {
    if (delta >= ONE) {
      delta -= ONE * 2;
    } else if (delta < -ONE) {
      delta += ONE * 2;
    }
  }
  // GPF loads a signed 16-bit IR component, then shifts its product
  // arithmetically by 12.
  return static_cast<int16_t>(
      from + ((static_cast<int>(static_cast<int16_t>(delta)) * weight) >>
              kFixedFractionBits));
}

static void BlendVector(SVECTOR* dst, const SVECTOR* src, int weight,
                        int rotation) {
  dst->vx = BlendComponent(dst->vx, src->vx, weight, rotation);
  dst->vy = BlendComponent(dst->vy, src->vy, weight, rotation);
  dst->vz = BlendComponent(dst->vz, src->vz, weight, rotation);
}

// decomp: BATTLE.PRG func_800B147C (src/BATTLE/BATTLE.PRG/48C7C.s:10-184).
// Full copies use the native pose extent, not the assembly's fixed byte count.
void Actors_BlendActorPose(Actors_AnimationPose* dst, Actors_AnimationPose* src,
                           int count, int frame, int frames) {
  int i;
  int weight;
  if (frame == frames) {
    *dst = *src;
    return;
  }
  if (frame == (frames >> 1)) {
    Actors_BlendActorPoseHalfway(dst, src, count);
    return;
  }
  weight = (frame * ONE) / frames;
  BlendVector(&dst->boneKeys[kRootPoseKey], &src->boneKeys[kRootPoseKey],
              weight, 0);
  for (i = 0; i < count; ++i) {
    BlendVector(&dst->boneKeys[i], &src->boneKeys[i], weight, 1);
    BlendVector(&dst->scaleKeys[i], &src->scaleKeys[i], weight, 0);
  }
}
}  // extern "C"
