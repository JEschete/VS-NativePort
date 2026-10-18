// Hand-maintained (tools/port/owned.csv).
#include "Actors/SequencePlayback.h"
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Shadow.h"
#include "ActorAssets.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorState.h"
#include "Actors/Appearance.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
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
#include "Core/Memory/WordCopy.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Event/ScriptVm.h"
#include "Field/Actors.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/ModelDraw.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: BATTLE.PRG func_800AFA28 (src/BATTLE/BATTLE.PRG/44F14.c:1252)
// Native frame ranges are local; no pointer-bearing context is placed at the PS1 scratchpad's end.
void Actors_AdvanceSkeletalAnimation(Actors_ActorRecord* actor, Actors_AnimationPose* state, int mode)
{
    u_short* header;
    Actors_AnimationTrackContext context;
    Actors_ShadowBlend* shadow;
    int frame;
    int frameOrOffset;
    int x;
    int y;
    int z;
    int delta;
    int lastFrame;
    int i;
    u_short rootTrack;
    Actors_SeqAnimationBank* bank;
    u_char queuedAnimation;

    state->reachedEnd = 0;
    frameOrOffset = (short)state->frame;
    i = 0;
    if (actor->framesPerStep != 0) {
        shadow = &actor->shadowBlend;
    advance_frame:
        if (shadow->levelFrames != 0) {
            delta = shadow->levelDelta / shadow->levelFrames;
            shadow->levelDelta -= delta;
            shadow->level += delta;
            --shadow->levelFrames;
        }
        shadow->level &= 0xFFF;
        if (shadow->radiusFrames != 0) {
            delta = shadow->radiusExpandDelta / shadow->radiusFrames;
            shadow->radiusExpandDelta -= delta;
            shadow->radiusExpandRate += delta;
            delta = shadow->radiusShrinkDelta / shadow->radiusFrames;
            shadow->radiusShrinkDelta -= delta;
            shadow->radiusShrinkRate += delta;
            --shadow->radiusFrames;
        }
        if ((short)state->frame >= (int)(u_short)actor->animationLength) {
            state->reachedEnd = -1;
            queuedAnimation = actor->queuedAnimation;
            if (queuedAnimation != 0) {
                Actors_PlayActorAnimationAtFrame((int)actor->instance.slot, (int)actor->queuedAnimation,
                    (int)actor->queuedParameterA, (int)actor->queuedParameterB);
                actor->queuedAnimation = 0;
                return;
            }
        }
        if (actor->instance.isFrozen) {
            if (state->nextEventFrame == (short)state->frame) {
                Actors_RunAnimationEvents(actor, state, mode);
            }
        }
        if ((short)state->frame >= (int)(u_short)actor->animationLength) {
            state->frame -= 1;
            state->holdingLastFrame = -1;
        }
        state->frame += 1;
        i += 1;
        if (i >= (int)actor->framesPerStep) {
            goto update_bones;
        }
        goto advance_frame;
    }
update_bones:
    frame = (short)state->frame;
    if ((frame != 1) && !state->holdingLastFrame) {
        lastFrame = frame - 1;
        if (state->eventWait == 0) {
            if (lastFrame < frameOrOffset) {
                frameOrOffset = 1;
            }
            if (frameOrOffset == 0) {
                frameOrOffset = 1;
            }
            bank = (Actors_SeqAnimationBank*)Actors_FindActorAnimationHeader(actor, actor->animationId, &header);
            if (bank != NULL && header != NULL) {
                context.firstFrame = frameOrOffset;
                context.lastFrame = lastFrame;
                context.data = VS_ONDISC(bank, animationData, const u_char*);
                rootTrack = header[4];
                if (rootTrack != 0) {
                    Actors_DecodeAnimationTrack(&context, rootTrack, &state->scaleKeys[41], &state->trackStates[41], 0);
                    x = state->scaleKeys[0x29].vx;
                    if (x < 0) {
                        x += 3;
                    }
                    actor->instance.position.vx = (u_short)actor->instance.position.vx + (x >> 2);
                    y = state->scaleKeys[0x29].vy;
                    if (y < 0) {
                        y += 3;
                    }
                    actor->instance.position.vy = (u_short)actor->instance.position.vy + (y >> 2);
                    z = state->scaleKeys[0x29].vz;
                    if (z < 0) {
                        z += 3;
                    }
                    actor->instance.position.vz = (u_short)actor->instance.position.vz + (z >> 2);
                }
                Actors_DecodeAnimationTrack(&context, header[3], &state->boneKeys[41], &state->reserved2A0[41], 0);
                for (i = 0; i < actor->instance.boneCount; ++i) {
                    Actors_DecodeAnimationTrack(&context, header[i + 5], &state->boneKeys[i], &state->reserved2A0[i], 1);
                    if (((u_char*)header)[3] & 2) {
                        Actors_DecodeAnimationTrack(&context, header[i + actor->instance.boneCount + 5],
                            &state->scaleKeys[i], &state->trackStates[i], 0);
                    }
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_800AFDE8 (src/BATTLE/BATTLE.PRG/44F14.c:1373)
// The original delta was value + 84 SVECTORs; callers now pass the matching pose array explicitly.
void Actors_DecodeAnimationTrack(const Actors_AnimationTrackContext* context, int offset,
    SVECTOR* value, SVECTOR* delta, int rotation)
{
    const u_char* start = context->data + offset;
    const u_char* data = start + (u_short)value->pad;
    int nextFrame = (u_short)delta->pad;
    int frame;

    for (frame = context->firstFrame; frame <= context->lastFrame; ++frame) {
        while (1) {
            int flags;
            int count;

            if (nextFrame >= frame) {
                addVector(value, delta);

                if (rotation) {
                    *(int*)value &= 0x1FFF1FFF;
                    value->vz &= 0x1FFF;
                }
                break;
            }

            flags = *data;

            if (flags == 0) {
                return;
            }

            if (flags & 0xE0) {
                count = flags & 0x1F;

                if (count == 0x1F) {
                    nextFrame += 32;
                    nextFrame += data[1];
                    data += 2;
                } else {
                    nextFrame += count + 1;
                    ++data;
                }
            } else {
                int packed;

                count = flags & 3;

                if (count == 3) {
                    nextFrame += 4;
                    nextFrame += data[1];
                    data += 2;
                } else {
                    nextFrame += count + 1;
                    ++data;
                }

                flags <<= 3;
                count = (signed char)data[0];
                count <<= 8;
                count |= data[1];
                data += 2;
                packed = count;
                count >>= 3;

                if (packed & 4) {
                    delta->vx = count;
                    flags &= 0x60;

                    if (packed & 3) {
                        count = (signed char)data[0];
                        count <<= 8;
                        count |= data[1];
                        data += 2;
                    }
                }

                if (packed & 2) {
                    delta->vy = count;
                    flags &= 0xA0;

                    if (packed & 1) {
                        count = (signed char)data[0];
                        count <<= 8;
                        count |= data[1];
                        data += 2;
                    }
                }

                if (packed & 1) {
                    delta->vz = count;
                    flags &= 0xC0;
                }
            }

            if (flags & 0x80) {
                delta->vx = (signed char)*data++;
            }

            if (flags & 0x40) {
                delta->vy = (signed char)*data++;
            }

            if (flags & 0x20) {
                delta->vz = (signed char)*data++;
            }
        }
    }

    value->pad = (short)(data - start);
    delta->pad = nextFrame;
}

static short BlendComponent(short from, short to, int weight, int rotation)
{
    int delta = (int)to - from;
    if (rotation) {
        if (delta >= ONE) {
            delta -= ONE * 2;
        } else if (delta < -ONE) {
            delta += ONE * 2;
        }
    }
    // GPF loads a signed 16-bit IR component, then shifts its product arithmetically by 12.
    return (short)(from + (((int)(short)delta * weight) >> 12));
}

static void BlendVector(SVECTOR* dst, const SVECTOR* src, int weight, int rotation)
{
    dst->vx = BlendComponent(dst->vx, src->vx, weight, rotation);
    dst->vy = BlendComponent(dst->vy, src->vy, weight, rotation);
    dst->vz = BlendComponent(dst->vz, src->vz, weight, rotation);
}

// decomp: BATTLE.PRG func_800B147C (src/BATTLE/BATTLE.PRG/48C7C.s:10-184).
// Full copies use the native pose extent, not the assembly's fixed byte count.
void Actors_BlendActorPose(Actors_AnimationPose* dst, Actors_AnimationPose* src, int count,
    int frame, int frames)
{
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
    BlendVector(&dst->boneKeys[41], &src->boneKeys[41], weight, 0);
    for (i = 0; i < count; ++i) {
        BlendVector(&dst->boneKeys[i], &src->boneKeys[i], weight, 1);
        BlendVector(&dst->scaleKeys[i], &src->scaleKeys[i], weight, 0);
    }
}
