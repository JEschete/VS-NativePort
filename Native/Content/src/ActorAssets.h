// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Content_ActorAssetRequest;
struct Content_SequenceBuffer;

#include "Field/Actors.h"

// decomp: func_8009C378_t (src/BATTLE/BATTLE.PRG/30DB0.h:9)
struct Content_SequenceBuffer {
    u_int firstSectionPointer; /* offset from the struct (on-disc pointer) */
    u_int secondSectionPointer; /* offset from the struct (on-disc pointer) */
    char body[0x78F8];
};
static_assert(sizeof(Content_SequenceBuffer) == 0x7900, "Content_SequenceBuffer must keep its PS1 layout");

// decomp: vs_battle_objectData (src/BATTLE/BATTLE.PRG/2842C.h:36)
struct Content_ActorAssetRequest {
    u_char assetKind;
    signed char targetSlot;
    short modelId;
    void* destination;
    uintptr_t dataAddress;
    Field_ActorPlacement placement;
    u_char subKind;
    char textureSlot;
    char material;
    char variant;
};
