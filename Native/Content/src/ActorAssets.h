// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORASSETS_H_TYPES
#define VS_ACTORASSETS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_ActorAssetRequest Content_ActorAssetRequest;
typedef struct Content_SequenceBuffer Content_SequenceBuffer;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ACTORASSETS_H_SET_TYPES_ONLY
#endif
#include "Field/Actors.h"
#ifdef VS_ACTORASSETS_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ACTORASSETS_H_SET_TYPES_ONLY
#endif

// decomp: func_8009C378_t (src/BATTLE/BATTLE.PRG/30DB0.h:9)
typedef struct Content_SequenceBuffer {
    u_int firstSectionPointer; /* offset from the struct (on-disc pointer) */
    u_int secondSectionPointer; /* offset from the struct (on-disc pointer) */
    char body[0x78F8];
} Content_SequenceBuffer;
_Static_assert(sizeof(Content_SequenceBuffer) == 0x7900, "Content_SequenceBuffer must keep its PS1 layout");

// decomp: vs_battle_objectData (src/BATTLE/BATTLE.PRG/2842C.h:36)
typedef struct Content_ActorAssetRequest {
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
} Content_ActorAssetRequest;

#endif // VS_ACTORASSETS_H_TYPES
