// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Render/Palette.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"

// decomp: BATTLE.PRG func_800AAD4C (src/BATTLE/BATTLE.PRG/40564.c:1301)
int Actors_SetActorTextureAnimation(int actorId, int index, int value, int mode)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actorId];
    Actors_TextureAnimationSlot* slot;

    if (actor == NULL) {
        return -1;
    }

    if (index >= 2) {
        return -1;
    }

    if (mode == 1 && value == 0) {
        return 0;
    }

    slot = &actor->textureAnimationSlots[index];

    switch (mode) {
    case 0:
        slot->mode = 0;
        break;

    case 1:
        if (slot->mode == 1 && slot->timeline == value) {
            return 0;
        }

        slot->timeline = value;
        slot->keyIndex = 0;

        slot->ticks = (index == 1) ? actorId * 10 : 0;

        slot->mode = mode;
        break;

    case 2:
        switch (slot->mode) {
        case 0:
            break;

        case 1:
            slot->mode = 2;
            break;

        case 2:
            slot->mode = 1;
            break;
        }
        break;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800AAE9C (src/BATTLE/BATTLE.PRG/40564.c:1357)
int Actors_UpdateActorTextureAnimations(Actors_ActorRecord* actor)
{
    int i;

    if (Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex] == 0) {
        return 0;
    }

    for (i = 0; i < 2; ++i) {
        Actors_TextureAnimationSlot* slot = &actor->textureAnimationSlots[i];
        Actors_TextureAnimationKey* p;

        if (slot->timeline == 0) {
            continue;
        }

        switch (slot->mode) {
        case 2:
            continue;

        case 0:
            if (slot->currentFrame == 1) {
                slot->timeline = 0;
                continue;
            }
            // Fallthrough
        case 1:
            if (i == 0) {
                p = &(*(Actors_TextureAnimationKey (*)[][28])&Actors_sheathedWeaponAnimationBases)[slot->timeline][slot->keyIndex];
            } else {
                p = &Actors_secondarySlotSequences[slot->timeline][slot->keyIndex];
            }

            ++slot->ticks;

            if (p->duration < slot->ticks) {
                slot->ticks = 0;
                ++slot->keyIndex;
                ++p;

                if (p->frame == 0) {
                    slot->keyIndex = 0;
                    p = (i == 0) ? (*(Actors_TextureAnimationKey (*)[][28])&Actors_sheathedWeaponAnimationBases)[slot->timeline] : Actors_secondarySlotSequences[slot->timeline];
                }
            }

            if (p->frame == 0) {
                return;
            }

            slot->currentFrame = p->frame;
            break;
        }

        Actors_UploadActorTextureFrame(actor, i, slot->currentFrame - 1);
    }

    // BUG: no return value, in practice it isn't read by the only caller.
}

// decomp: src/BATTLE/BATTLE.PRG/40564.c:108
#define getActorFlags(actor) (*((int*)&(actor)->sequenceFlags - 2))
// decomp: BATTLE.PRG func_800AB098 (src/BATTLE/BATTLE.PRG/40564.c:1417)
void Actors_UploadActorTextureFrame(Actors_ActorRecord* actor, int index, int frame)
{
    Actors_TextureAnimationFrame fallback;
    RECT rect;
    DR_MOVE* packet;
    Actors_TextureAnimationFrame* animation;
    Actors_TextureOverlayRect* overlay;
    int srcX;
    int srcY;
    int dstX;
    int width;
    int height;
    int x;
    int y;
    int row;
    int py;

    if (actor->textureRowOffsets[Actors_sharedSequenceBankIndex] == 0xFF) {
        return;
    }

    animation = &actor->textureAnimationFrames[index];

    if (animation->columns == 0) {
        fallback.overlayIndex = index ? 7 : 5;
        fallback.sourceX = actor->textureCellOrigins[index].x;
        fallback.sourceY = actor->textureCellOrigins[index].y;
        fallback.columns = actor->textureCellOrigins[index].columns;

        if (fallback.columns == 0) {
            return;
        }

        animation = &fallback;
    }

    srcX = animation->sourceX;
    srcY = animation->sourceY;
    overlay = &actor->textureOverlays[animation->overlayIndex - 1];
    dstX = overlay->x;
    width = overlay->width;
    height = overlay->height;

    if (!(getActorFlags(actor) & 0x8000000)) {
        srcX /= 4;
        width /= 4;
        dstX /= 4;
    } else {
        srcX /= 2;
        width /= 2;
        dstX /= 2;
    }

    srcX += (frame % animation->columns) * width;
    srcY += (frame / animation->columns) * height;
    srcY += actor->textureRowOffsets[Actors_sharedSequenceBankIndex];
    packet = Render_BeginTextureMovePacket();
    x = (*(short (*)[])&Actors_textureSlotOrigins)[actor->textureSlot * 2];
    y = (*(short (*)[])&Actors_textureSlotOrigins)[actor->textureSlot * 2 + 1];
    row = Actors_textureSlotRowOffsets[actor->textureSlot];
    py = overlay->y;
    frame = Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex] & 15;
    setRECT(&rect, (frame * 64) + srcX, srcY + 256, width, height);
    srcY = (y * 256) + (row * 64) + py;
    SetDrawMove(packet, &rect, dstX + (x * 64), srcY);
    ((void (*)(DR_MOVE*))Render_AppendOverlayPrimitive)(packet);
    Render_EndTextureMovePacket(packet);
}
#undef getActorFlags

// decomp: BATTLE.PRG func_800AB2AC (src/BATTLE/BATTLE.PRG/40564.c:1486)
void Actors_ResetActorTextureFrames(int arg0)
{

    if (arg0 == Actors_sharedSequenceBankIndex) {
        int i;

        for (i = 0; i < 17; ++i) {
            Actors_ActorRecord* temp_s0 = Actors_actorRecords[i];
            if ((temp_s0 != NULL) && (temp_s0->textureRowOffsets[arg0] != 0xFF)) {
                Actors_UploadActorTextureFrame(temp_s0, 0, 0);
                Actors_UploadActorTextureFrame(temp_s0, 1, 0);
            }
        }
    }
}

// decomp: src/BATTLE/BATTLE.PRG/40564.c:108
#define getActorFlags(actor) (*((int*)&(actor)->sequenceFlags - 2))
// decomp: BATTLE.PRG func_800AB358 (src/BATTLE/BATTLE.PRG/40564.c:1502)
void Actors_UploadActorTextureOverlay(int index, Actors_TextureOverlayUpload* arg)
{
    RECT rect;
    DR_MOVE* packet;
    Actors_ActorRecord* actor = Actors_actorRecords[index];
    Actors_TextureOverlayRect* overlay;
    int srcX;
    int srcY;
    int dstX;
    int width;
    int height;
    int x;
    int y;
    int row;
    int py;

    if (Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex] == 0) {
        return;
    }

    srcX = arg->sourceX;
    srcY = arg->sourceY;
    srcY += actor->textureRowOffsets[Actors_sharedSequenceBankIndex];
    overlay = &actor->textureOverlays[arg->overlayIndex];
    dstX = overlay->x;
    width = overlay->width;
    height = overlay->height;

    if (!(getActorFlags(actor) & 0x8000000)) {
        srcX /= 4;
        width /= 4;
        dstX /= 4;
    } else {
        srcX /= 2;
        width /= 2;
        dstX /= 2;
    }

    packet = Render_BeginTextureMovePacket();
    x = (*(short (*)[])&Actors_textureSlotOrigins)[actor->textureSlot * 2];
    y = (*(short (*)[])&Actors_textureSlotOrigins)[actor->textureSlot * 2 + 1];
    row = Actors_textureSlotRowOffsets[actor->textureSlot];
    py = overlay->y;
    setRECT(
        &rect, ((Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex] & 15) * 64) + srcX, srcY + 256, width, height);
    SetDrawMove(packet, &rect, dstX + (x * 64), (y * 256) + (row * 64) + py);
    ((void (*)(DR_MOVE*))Render_AppendOverlayPrimitive)(packet);
    Render_EndTextureMovePacket(packet);
}
#undef getActorFlags

// decomp: src/BATTLE/BATTLE.PRG/40564.c:108
#define getActorFlags(actor) (*((int*)&(actor)->sequenceFlags - 2))
// decomp: BATTLE.PRG func_800AB4F0 (src/BATTLE/BATTLE.PRG/40564.c:1552)
void Actors_UpdateActorTextureScrolling(Actors_ActorRecord* actor)
{
    int originX = (*(short (*)[])&Actors_textureSlotOrigins)[actor->textureSlot * 2] * 64;
    int originY =
        ((*(short (*)[])&Actors_textureSlotOrigins)[actor->textureSlot * 2 + 1] * 256) + (Actors_textureSlotRowOffsets[actor->textureSlot] * 64);
    int i;
    int height;

    for (i = 0; i < 4; ++i) {
        Actors_TextureScrollBounds* bounds = &actor->textureScrollBounds[i];
        Actors_TextureScrollState* scroll;
        Actors_TextureOverlayRect* overlay;
        RECT rect;
        DR_MOVE* packet;
        int x;
        int y;
        int width;
        int scrollX;
        int scrollY;
        int dx;
        int dy;
        int minX;
        int maxX;
        int maxY;
        int dstX;

        if (bounds->width == 0) {
            continue;
        }

        scroll = &actor->textureScroll[i];
        scroll->timer += State_tickSpeed / 2;

        if (scroll->timer <= scroll->interval) {
            continue;
        }

        scroll->timer = 0;
        overlay = &actor->textureOverlays[i];

        // BUG: height is read before it is first set
        if (bounds->width == 0 || bounds->width == height) {
            continue;
        }

        width = overlay->width;
        scrollX = scroll->x;
        dx = scroll->stepX;
        scrollY = scroll->y;
        dy = scroll->stepY;
        minX = bounds->x;
        height = overlay->height;
        x = scrollX + dx;
        y = scrollY + dy;
        maxX = minX + bounds->width;
        maxY = bounds->y + bounds->height;

        if (maxX < x + width || x < minX) {
            if (bounds->bounces) {
                x -= dx * 2;
                scroll->stepX = -scroll->stepX;
            } else {
                x = minX;
            }
        }

        if (maxY < y + height || y < bounds->y) {
            if (bounds->bounces) {
                y -= scroll->stepY * 2;
                scroll->stepY = -scroll->stepY;
            } else {
                y = bounds->y;
            }
        }

        scroll->x = x;
        scroll->y = y;

        if (!(getActorFlags(actor) & 0x8000000)) {
            x /= 4;
            width /= 4;
            dstX = overlay->x >> 2;
        } else {
            x /= 2;
            width /= 2;
            dstX = overlay->x >> 1;
        }

        rect.x = originX + x;
        packet = Render_BeginTextureMovePacket();
        rect.y = originY + y;
        rect.w = width;
        rect.h = height;
        SetDrawMove(packet, &rect, originX + dstX, originY + overlay->y);
        ((void (*)(DR_MOVE*))Render_AppendOverlayPrimitive)(packet);
        Render_EndTextureMovePacket(packet);
    }
}
#undef getActorFlags

