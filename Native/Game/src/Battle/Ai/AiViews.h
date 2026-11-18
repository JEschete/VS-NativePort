// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Battle_ActorStats;
struct Battle_Ai_ActionCandidate;
struct Battle_Ai_ActionChoiceEntryView;
struct Battle_Ai_ActionChoiceFlagsView;
struct Battle_Ai_ActionChoiceInfoView;
struct Battle_Ai_ActionCostView;
struct Battle_Ai_ActionMetadataEntry;
struct Battle_Ai_ActionRangeShape;
struct Battle_Ai_ActionRangeView;
struct Battle_Ai_ActionRangeView2;
struct Battle_Ai_ActionSetupView;
struct Battle_Ai_ActionSlotConfigView;
struct Battle_Ai_ActionTiming;
struct Battle_Ai_ActorListNodeView;
struct Battle_Ai_ActorPositionView;
union Battle_Ai_ActorStateFlags;
struct Battle_Ai_ApproachActorView;
struct Battle_Ai_ApproachRelationBits;
struct Battle_Ai_ApproachTargetRecordView;
union Battle_Ai_ApproachTileWord;
union Battle_Ai_BlockedTileWord;
struct Battle_Ai_CandidateEntryView;
struct Battle_Ai_CandidateFlags;
struct Battle_Ai_CandidateTable;
union Battle_Ai_CandidateTileWord;
struct Battle_Ai_CheckObjectView;
struct Battle_Ai_CreationModelView;
struct Battle_Ai_CreationStatsView;
union Battle_Ai_CreationTileWord;
union Battle_Ai_DetourWord;
struct Battle_Ai_DirectionCacheBits;
union Battle_Ai_DirectionCacheTile;
struct Battle_Ai_DispatchActorView;
struct Battle_Ai_DispatchEntryView;
struct Battle_Ai_DispatchModelView;
struct Battle_Ai_DispatchRegistryView;
struct Battle_Ai_EngagementActor;
struct Battle_Ai_EngagementActorView;
struct Battle_Ai_EngagementData;
struct Battle_Ai_EscortActorView;
struct Battle_Ai_EscortEntryView;
struct Battle_Ai_EscortRelationBits;
struct Battle_Ai_EscortTargetView;
struct Battle_Ai_EvaluationActorView;
struct Battle_Ai_EvaluationContextView;
struct Battle_Ai_EvaluationStatsView;
struct Battle_Ai_GuardModelView;
union Battle_Ai_GuardTileWord;
struct Battle_Ai_HpPairView;
struct Battle_Ai_MetadataActionView;
struct Battle_Ai_MetadataSlotView;
struct Battle_Ai_MetadataStatsView;
union Battle_Ai_MetadataWord;
struct Battle_Ai_ModelActiveView;
struct Battle_Ai_ModelStatePrefix;
struct Battle_Ai_ObstructionCache;
struct Battle_Ai_ObstructionModel;
struct Battle_Ai_ObstructionVector;
struct Battle_Ai_OrbitPlayerRecordView;
struct Battle_Ai_OrbitStatsView;
struct Battle_Ai_OrbitTrackingOrbitView;
struct Battle_Ai_PerceivedObjectView;
struct Battle_Ai_PerceptionActorView;
struct Battle_Ai_PerceptionStatsView;
struct Battle_Ai_PositionLinkView;
struct Battle_Ai_RadialRetreatOrbitView;
struct Battle_Ai_ReactionActionView;
struct Battle_Ai_ReactionOrbitView;
struct Battle_Ai_ReactionRecord;
struct Battle_Ai_RecoveryModelView;
struct Battle_Ai_RelationByteView;
union Battle_Ai_RelationWordView;
struct Battle_Ai_RetreatPlayerRecordView;
struct Battle_Ai_RouteActorView;
struct Battle_Ai_RouteGateTile;
struct Battle_Ai_RouteGateView;
struct Battle_Ai_SavedSettingsPair;
struct Battle_Ai_SettingsActorView;
union Battle_Ai_SettingsWord;
struct Battle_Ai_StatApplicationView;
struct Battle_Ai_StatDeltaView;
struct Battle_Ai_StepActorView;
union Battle_Ai_StepPositionWord;
union Battle_Ai_TargetRelation;
struct Battle_Ai_TerrainMoveActorView;
union Battle_Ai_TerrainTileWord;
struct Battle_Ai_WanderRecordView;
struct Battle_Ai_WanderTiming;
struct Battle_Ai_WaypointTile;
struct Battle_Ai_WordPair;

#include "Battle/Ai/Navigation.h"

// decomp: actionCandidateEntry (src/BATTLE/BATTLE.PRG/6FC00.c:500)
struct Battle_Ai_CandidateEntryView {
    int reserved0;
    unsigned int row : 3, column : 2, action : 8, threshold : 16, reserved7Bit5 : 3;
    unsigned int distanceSquared;
    unsigned int targetId : 4, reservedCBit4 : 3, reservedCBit7 : 12, weight : 8, reservedFBit3 : 5;
    unsigned int reserved10Bit0 : 1, opaque10Bit1 : 1, reserved10Bit2 : 1, reserved10Bit3 : 1, reserved10Bit4 : 1, reserved10Bit5 : 1,
        range : 16, reserved12Bit6 : 10;
};
static_assert(sizeof(Battle_Ai_CandidateEntryView) == 0x14, "Battle_Ai_CandidateEntryView must keep its PS1 layout");

// decomp: actionCandidateTarget (src/BATTLE/BATTLE.PRG/6FC00.c:503)
struct Battle_Ai_RelationByteView {
    unsigned char opaque0, reserved1[3];
};
static_assert(sizeof(Battle_Ai_RelationByteView) == 0x4, "Battle_Ai_RelationByteView must keep its PS1 layout");

// decomp: actionChoiceFlags (src/BATTLE/BATTLE.PRG/74088.c:859)
struct Battle_Ai_ActionChoiceFlagsView {
    u_int reserved0Bit0 : 1;
    u_int reserved0Bit1 : 1;
    u_int reserved0Bit2 : 1;
    u_int reserved0Bit3 : 1;
    u_int reserved0Bit4 : 28;
};
static_assert(sizeof(Battle_Ai_ActionChoiceFlagsView) == 0x4, "Battle_Ai_ActionChoiceFlagsView must keep its PS1 layout");

// decomp: actionChoice (src/BATTLE/BATTLE.PRG/74088.c:872)
struct Battle_Ai_ActionChoiceEntryView {
    int reserved0;
    u_int reserved4Bit0 : 3;
    u_int reserved4Bit3 : 2;
    u_int reserved4Bit5 : 8;
    u_int reserved5Bit5 : 19;
    u_int reserved8;
    u_int reservedCBit0 : 19;
    u_int reservedEBit3 : 8;
    u_int reservedFBit3 : 5;
    Battle_Ai_ActionChoiceFlagsView reserved10;
};
static_assert(sizeof(Battle_Ai_ActionChoiceEntryView) == 0x14, "Battle_Ai_ActionChoiceEntryView must keep its PS1 layout");

// decomp: actionChoiceInfo (src/BATTLE/BATTLE.PRG/74088.c:897)
struct Battle_Ai_ActionChoiceInfoView {
    char reserved0[0xC];
    u_int reservedCBit0 : 23;
    u_int reservedEBit7 : 1;
    u_int reservedFBit0 : 4;
    u_int reservedFBit4 : 1;
    u_int reservedFBit5 : 1;
    u_int reservedFBit6 : 2;
    char reserved10[0x24];
};
static_assert(sizeof(Battle_Ai_ActionChoiceInfoView) == 0x34, "Battle_Ai_ActionChoiceInfoView must keep its PS1 layout");

// decomp: actionCostView (src/BATTLE/BATTLE.PRG/74088.c:471)
struct Battle_Ai_ActionCostView {
    unsigned int reserved0;
    char reserved4[48];
};
static_assert(sizeof(Battle_Ai_ActionCostView) == 0x34, "Battle_Ai_ActionCostView must keep its PS1 layout");

// decomp: actionDispatchActor (src/BATTLE/BATTLE.PRG/71618.c:1055)
struct Battle_Ai_DispatchActorView {
    struct Battle_Ai_DispatchActorView* next;
    int id;
    char reserved8_8[52];
    void* nativePointer3C; /* PS1 pointer slot 0x3C */
    char reserved8_40[4];
    Battle_Ai_DispatchModelView* model;
};

// decomp: actionDispatchEntry (src/BATTLE/BATTLE.PRG/71618.c:1040)
struct Battle_Ai_DispatchEntryView {
    unsigned int reserved0, reserved4, reserved8, reservedC;
};
static_assert(sizeof(Battle_Ai_DispatchEntryView) == 0x10, "Battle_Ai_DispatchEntryView must keep its PS1 layout");

// decomp: actionDispatchModel (src/BATTLE/BATTLE.PRG/71618.c:1049)
struct Battle_Ai_DispatchModelView {
    char reserved0[8];
    unsigned int opaque8;
    char reservedC[6];
    unsigned char reserved12;
    char reserved13[7];
    short reserved1A;
    short position[3];
};

// decomp: actionDispatchRegistry (src/BATTLE/BATTLE.PRG/71618.c:1089)
struct Battle_Ai_DispatchRegistryView {
    char reserved0[0x28];
    Battle_Ai_DispatchActorView* reserved28[4];
};

// decomp: actionMetadataWord (src/BATTLE/BATTLE.PRG/7CD68.c:497)
union Battle_Ai_MetadataWord {
    unsigned int word;
    unsigned char bytes[4];
};
static_assert(sizeof(Battle_Ai_MetadataWord) == 0x4, "Battle_Ai_MetadataWord must keep its PS1 layout");

// decomp: actionMetadataAction (src/BATTLE/BATTLE.PRG/7CD68.c:503)
struct Battle_Ai_MetadataActionView {
    Battle_Ai_MetadataWord opaque0;
    char reserved4[8];
    unsigned int opaqueC;
    char reserved10[36];
};
static_assert(sizeof(Battle_Ai_MetadataActionView) == 0x34, "Battle_Ai_MetadataActionView must keep its PS1 layout");

// decomp: actionMetadataEntry (src/BATTLE/BATTLE.PRG/7CD68.c:515)
struct Battle_Ai_ActionMetadataEntry {
    unsigned int reserved0Bit0 : 1, opaque0Bit1 : 1, opaque0Bit2 : 1, kind : 2, longRange : 1, special : 1, reserved0Bit7 : 1,
        radius : 16, reserved3Bit0 : 8;
    unsigned int rangeSquared;
};
static_assert(sizeof(Battle_Ai_ActionMetadataEntry) == 0x8, "Battle_Ai_ActionMetadataEntry must keep its PS1 layout");

// decomp: actionMetadataSlot (src/BATTLE/BATTLE.PRG/7CD68.c:506)
struct Battle_Ai_MetadataSlotView {
    unsigned short id, reserved2;
};
static_assert(sizeof(Battle_Ai_MetadataSlotView) == 0x4, "Battle_Ai_MetadataSlotView must keep its PS1 layout");

// decomp: actionMetadataStats (src/BATTLE/BATTLE.PRG/7CD68.c:510)
struct Battle_Ai_MetadataStatsView {
    char reserved0[0x8C0];
    Battle_Ai_MetadataSlotView slots[6][4];
};
static_assert(sizeof(Battle_Ai_MetadataStatsView) == 0x920, "Battle_Ai_MetadataStatsView must keep its PS1 layout");

// decomp: actionReactionAction (src/BATTLE/BATTLE.PRG/74088.c:708)
struct Battle_Ai_ReactionActionView {
    char reserved0[20];
    unsigned int effect1, effect2;
    char reserved1C[24];
};
static_assert(sizeof(Battle_Ai_ReactionActionView) == 0x34, "Battle_Ai_ReactionActionView must keep its PS1 layout");

// decomp: actionReactionMotion (src/BATTLE/BATTLE.PRG/74088.c:712)
struct Battle_Ai_ReactionOrbitView {
    char reserved0[10];
    unsigned char isOrbiting;
};
static_assert(sizeof(Battle_Ai_ReactionOrbitView) == 0xB, "Battle_Ai_ReactionOrbitView must keep its PS1 layout");

// decomp: actionScoreActor (src/BATTLE/BATTLE.PRG/74088.c:563)
struct Battle_Ai_ActorListNodeView {
    struct Battle_Ai_ActorListNodeView* next;
    int id;
};

// decomp: actionSetupContext (src/BATTLE/BATTLE.PRG/71618.c:1004)
struct Battle_Ai_ActionSetupView {
    char reserved0[0x13];
    u_char reserved13;
    char reserved14[0x12B];
    u_char reserved13F;
    char reserved140[0xEC];
    Battle_Ai_ActionCandidate* reserved22C;
};

// decomp: actionSlotConfig (src/BATTLE/BATTLE.PRG/74088.c:851)
struct Battle_Ai_ActionSlotConfigView {
    u_int reserved0Bit0 : 2;
    u_int reserved0Bit2 : 1;
    u_int reserved0Bit3 : 2;
    u_int reserved0Bit5 : 1;
    u_int reserved0Bit6 : 1;
    u_int reserved0Bit7 : 1;
    u_int reserved1Bit0 : 16;
    u_int reserved3Bit0 : 8;
    u_int reserved4;
};
static_assert(sizeof(Battle_Ai_ActionSlotConfigView) == 0x8, "Battle_Ai_ActionSlotConfigView must keep its PS1 layout");

// decomp: actionStatDelta (src/BATTLE/BATTLE.PRG/74088.c:459)
struct Battle_Ai_StatDeltaView {
    unsigned char actor;
    char reserved1;
    unsigned char reserved2;
    char reserved3;
    short hp, reserved6;
    char reserved8[12];
    unsigned int reserved14, reserved18, modes;
    char reserved20[24];
    short reserved38;
    char reserved3A[2];
    short reserved3C;
    char reserved3E[2];
    int skip;
};
static_assert(sizeof(Battle_Ai_StatDeltaView) == 0x44, "Battle_Ai_StatDeltaView must keep its PS1 layout");

// decomp: actionStatApplication (src/BATTLE/BATTLE.PRG/74088.c:467)
struct Battle_Ai_StatApplicationView {
    unsigned short action;
    char reserved2[2];
    Battle_Ai_StatDeltaView first;
    char reserved48[2];
    unsigned short count;
    Battle_Ai_StatDeltaView entries[1];
};
static_assert(sizeof(Battle_Ai_StatApplicationView) == 0x90, "Battle_Ai_StatApplicationView must keep its PS1 layout");

// decomp: actorEvaluationActor (src/BATTLE/BATTLE.PRG/74088.c:1523)
struct Battle_Ai_EvaluationActorView {
    struct Battle_Ai_EvaluationActorView* next;
    int id;
    unsigned char opaque8;
    char reserved9[51];
    Battle_Ai_EvaluationStatsView* stats;
};

// decomp: actorEvaluationContext (src/BATTLE/BATTLE.PRG/74088.c:1544)
struct Battle_Ai_EvaluationContextView {
    char reserved0[4];
    unsigned char id;
    char reserved5[63];
    int opaque44;
};
static_assert(sizeof(Battle_Ai_EvaluationContextView) == 0x48, "Battle_Ai_EvaluationContextView must keep its PS1 layout");

// decomp: actorEvaluationStats (src/BATTLE/BATTLE.PRG/74088.c:1516)
struct Battle_Ai_EvaluationStatsView {
    char reserved0[24];
    short hp;
    char reserved1A[23];
    unsigned char opaque31;
    char reserved32[1];
    unsigned char opaque33;
};
static_assert(sizeof(Battle_Ai_EvaluationStatsView) == 0x34, "Battle_Ai_EvaluationStatsView must keep its PS1 layout");

// decomp: actorInitializationPoint (src/BATTLE/BATTLE.PRG/7CD68.c:833)
union Battle_Ai_CreationTileWord {
    unsigned int word;
    struct {
        unsigned char tileX, tag, tileZ, searchStatus;
    } tile;
};

// decomp: actorInitializationModel (src/BATTLE/BATTLE.PRG/7CD68.c:853)
struct Battle_Ai_CreationModelView {
    char reserved0[8];
    unsigned int opaque8;
    char reservedC[16];
    short position[3];
    char reserved22[58];
    Battle_Ai_CreationTileWord tile;
    char reserved60_60[8];
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved60_6C[256];
    unsigned int kind;
    char reserved170_170[1120];
    void* nativePointer5D0; /* PS1 pointer slot 0x5D0 */
    void* nativePointer5D4; /* PS1 pointer slot 0x5D4 */
    void* nativePointer5D8; /* PS1 pointer slot 0x5D8 */
    void* nativePointer5DC; /* PS1 pointer slot 0x5DC */
    void* nativePointer5E0; /* PS1 pointer slot 0x5E0 */
    void* nativePointer5E4; /* PS1 pointer slot 0x5E4 */
    void* nativePointer5E8; /* PS1 pointer slot 0x5E8 */
    char reserved170_5EC[80];
    unsigned short radius;
    unsigned short opaque63E;
};

// decomp: actorInitializationStats (src/BATTLE/BATTLE.PRG/7CD68.c:840)
struct Battle_Ai_CreationStatsView {
    char reserved0[48];
    unsigned char opaque30;
    unsigned char opaque31;
    unsigned char opaque32;
    unsigned char opaque33;
};
static_assert(sizeof(Battle_Ai_CreationStatsView) == 0x34, "Battle_Ai_CreationStatsView must keep its PS1 layout");

// decomp: actorObject (src/BATTLE/BATTLE.PRG/6FC00.c:588)
struct Battle_Ai_PerceivedObjectView {
    char reserved0[8];
    union {
        u_int word;
        struct {
            u_int reserved0 : 16;
            u_int skipStateA : 3;
            u_int skipStateB : 2;
            u_int reserved21 : 10;
            u_int forcesPerception : 1;
        } bits;
    } opaque8;
    char reservedC[8];
    short opaque14;
    char reserved16[6];
    SVECTOR position;
    short reserved24;
    short facing;
    char reserved28[0x34];
    Battle_Ai_TilePosition tile;
    Battle_Ai_TilePosition lastTile;
    char reserved64_64[4];
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved64_6C[1344];
    u_int reserved5ACBit0 : 15;
    u_int opaque5ADBit7 : 1;
    u_int reserved5AEBit0 : 16;
};

// decomp: actorPhaseFlags (src/BATTLE/BATTLE.PRG/73700.c:447)
struct Battle_Ai_EngagementActorView {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    char reserved0_4[4];
    unsigned int reserved8Bit0 : 26, mode : 2, reservedBBit4 : 4;
};

// decomp: actorSettingsWord (src/BATTLE/BATTLE.PRG/7CD68.c:604)
union Battle_Ai_SettingsWord {
    unsigned int word;
    unsigned short halves[2];
    unsigned char bytes[4];
};
static_assert(sizeof(Battle_Ai_SettingsWord) == 0x4, "Battle_Ai_SettingsWord must keep its PS1 layout");

// decomp: actorSettingsActor (src/BATTLE/BATTLE.PRG/7CD68.c:616)
struct Battle_Ai_SettingsActorView {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    char reserved0_4[22];
    unsigned short kind;
    char reserved1C[20];
    Battle_Ai_SettingsWord opaque30;
    union {
        unsigned int word;
        struct {
            unsigned int reserved0 : 12, mode : 2, upperBits : 18;
        } bits;
    } opaque34;
};

// decomp: actorSettingsEntry (src/BATTLE/BATTLE.PRG/7CD68.c:677)
struct Battle_Ai_WanderTiming {
    unsigned short movePeriod, restPeriod;
    unsigned char rate, reserved5;
};
static_assert(sizeof(Battle_Ai_WanderTiming) == 0x6, "Battle_Ai_WanderTiming must keep its PS1 layout");

// decomp: actorUpdateActor (src/BATTLE/BATTLE.PRG/6FC00.c:868)
struct Battle_Ai_PerceptionActorView {
    struct Battle_Ai_PerceptionActorView* next;
    int id;
    u_char opaque8;
    u_char opaque9;
    char reservedA;
    u_char opaqueBBit0 : 2;
    u_char reservedBBit2 : 6;
    char reservedC[0x30];
    Battle_ActorStats* stats;
};

// decomp: actorUpdateHp (src/BATTLE/BATTLE.PRG/6FC00.c:878)
struct Battle_Ai_HpPairView {
    u_int current : 16;
    u_int max : 16;
};
static_assert(sizeof(Battle_Ai_HpPairView) == 0x4, "Battle_Ai_HpPairView must keep its PS1 layout");

// decomp: actorUpdatePoint (src/BATTLE/BATTLE.PRG/6FC00.c:873)
struct Battle_Ai_PositionLinkView {
    u_int position;
    u_int opaque4;
};
static_assert(sizeof(Battle_Ai_PositionLinkView) == 0x8, "Battle_Ai_PositionLinkView must keep its PS1 layout");

// decomp: actorUpdateStats (src/BATTLE/BATTLE.PRG/6FC00.c:885)
struct Battle_Ai_PerceptionStatsView {
    char reserved0[0x18];
    Battle_Ai_HpPairView hp;
    int reserved1C;
    int reserved20;
};
static_assert(sizeof(Battle_Ai_PerceptionStatsView) == 0x24, "Battle_Ai_PerceptionStatsView must keep its PS1 layout");

// decomp: da4bcActor (src/BATTLE/BATTLE.PRG/71618.c:776)
struct Battle_Ai_EscortActorView {
    struct Battle_Ai_EscortActorView* next;
    int id;
};

// decomp: da4bcEntry (src/BATTLE/BATTLE.PRG/71618.c:713)
struct Battle_Ai_EscortEntryView {
    unsigned int reserved0, reserved4, reserved8, opaqueC;
};
static_assert(sizeof(Battle_Ai_EscortEntryView) == 0x10, "Battle_Ai_EscortEntryView must keep its PS1 layout");

// decomp: da4bcEntryTarget (src/BATTLE/BATTLE.PRG/71618.c:780)
struct Battle_Ai_EscortTargetView {
    unsigned int target : 4, reserved0Bit4 : 28;
};
static_assert(sizeof(Battle_Ai_EscortTargetView) == 0x4, "Battle_Ai_EscortTargetView must keep its PS1 layout");

// decomp: da4bcModel (src/BATTLE/BATTLE.PRG/71618.c:761)
struct Battle_Ai_GuardModelView {
    char reserved0[28];
    short x;
    char reserved1E[2];
    short z;
    char reserved22[58];
    Battle_Ai_TilePosition tile;
};

// decomp: da4bcTargetBits (src/BATTLE/BATTLE.PRG/71618.c:766)
struct Battle_Ai_EscortRelationBits {
    unsigned int reserved0Bit0 : 6, mode : 2, reserved1Bit0 : 12, category : 7, reserved3Bit3 : 1, oldMode : 2,
        reserved3Bit6 : 2;
};
static_assert(sizeof(Battle_Ai_EscortRelationBits) == 0x4, "Battle_Ai_EscortRelationBits must keep its PS1 layout");

// decomp: directionalCacheBits (src/BATTLE/BATTLE.PRG/771A8.c:900)
struct Battle_Ai_DirectionCacheBits {
    unsigned char tested, valid;
};
static_assert(sizeof(Battle_Ai_DirectionCacheBits) == 0x2, "Battle_Ai_DirectionCacheBits must keep its PS1 layout");

// decomp: func_800DC888_t (src/BATTLE/BATTLE.PRG/6F014.c:253)
struct Battle_Ai_ActionTiming {
    u_int opaque0Bit0 : 4;
    u_int reserved0Bit4 : 28;
    short opaque4;
    short opaque6;
    short opaque8;
    short opaqueA;
    u_int opaqueC;
    short opaque10;
    short opaque12;
    short opaque14;
    short opaque16;
};
static_assert(sizeof(Battle_Ai_ActionTiming) == 0x18, "Battle_Ai_ActionTiming must keep its PS1 layout");

// decomp: func_800DCAA0_t1 (src/BATTLE/BATTLE.PRG/6F014.c:417)
struct Battle_Ai_CandidateTable {
    char reserved0[0x88];
    u_char reserved88;
};
static_assert(sizeof(Battle_Ai_CandidateTable) == 0x89, "Battle_Ai_CandidateTable must keep its PS1 layout");

// decomp: func_800DCAA0_t2 (src/BATTLE/BATTLE.PRG/6F014.c:96)
struct Battle_Ai_CandidateFlags {
    u_int opaque0Bit0 : 1;
    u_int opaque0Bit1 : 1;
    u_int opaque0Bit2 : 1;
    u_int opaque0Bit3 : 1;
    u_int opaque0Bit4 : 1;
    u_int opaque0Bit5 : 1;
    u_int opaque0Bit6 : 16;
    u_int reserved2Bit6 : 10;
};
static_assert(sizeof(Battle_Ai_CandidateFlags) == 0x4, "Battle_Ai_CandidateFlags must keep its PS1 layout");

// decomp: func_800DEB10_t (src/BATTLE/BATTLE.PRG/6F014.c:278)
struct Battle_Ai_EngagementActor {
    u_char opaque0;
    char reserved1;
    u_char opaque2;
    char reserved3;
    short opaque4;
    short opaque6;
    char reserved8[0xC];
    u_int opaque14;
    u_int opaque18;
    u_int opaque1CBit0 : 2;
    u_int opaque1CBit2 : 2;
    u_int reserved1CBit4 : 14;
    u_int opaque1EBit2 : 2;
    u_int reserved1EBit4 : 2;
    u_int opaque1EBit6 : 2;
    u_int reserved1FBit0 : 8;
    char reserved20[0x18];
    short opaque38;
    char reserved3A[2];
    short opaque3C;
    char reserved3E[2];
    int opaque40;
};
static_assert(sizeof(Battle_Ai_EngagementActor) == 0x44, "Battle_Ai_EngagementActor must keep its PS1 layout");

// decomp: func_800DEB10_t2 (src/BATTLE/BATTLE.PRG/6F014.c:85)
struct Battle_Ai_EngagementData {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    char reserved0_4[4];
    u_char opaque8;
    u_char opaque9;
    char reservedA;
    u_char reservedBBit0 : 2;
    u_char opaqueBBit2 : 2;
    u_char reservedBBit4 : 4;
    u_short opaqueC;
    u_short opaqueE;
    SVECTOR opaque10;
    char reserved18[0x24];
    Battle_ActorStats* opaque3C;
};

// decomp: func_800DEEA4_t (src/BATTLE/BATTLE.PRG/6F014.c:287)
struct Battle_Ai_ReactionRecord {
    u_short opaque0;
    char reserved2[2];
    Battle_Ai_EngagementActor opaque4;
    char reserved48[2];
    u_short opaque4A;
    Battle_Ai_EngagementActor opaque4C[0];
};
static_assert(sizeof(Battle_Ai_ReactionRecord) == 0x4C, "Battle_Ai_ReactionRecord must keep its PS1 layout");

// decomp: func_800DFA54_t (src/BATTLE/BATTLE.PRG/6F014.c:49)
struct Battle_Ai_RouteGateTile {
    u_char opaque0;
    u_char opaque1;
    u_char opaque2;
};
static_assert(sizeof(Battle_Ai_RouteGateTile) == 0x3, "Battle_Ai_RouteGateTile must keep its PS1 layout");

// decomp: func_800E1238_t (src/BATTLE/BATTLE.PRG/6F014.c:139)
struct Battle_Ai_ModelStatePrefix {
    int reserved0;
    int reserved4;
    u_int reserved8Bit0 : 21;
    u_int opaqueABit5 : 1;
    u_int reservedABit6 : 10;
};
static_assert(sizeof(Battle_Ai_ModelStatePrefix) == 0xC, "Battle_Ai_ModelStatePrefix must keep its PS1 layout");

// decomp: func_800E5568_t (src/BATTLE/BATTLE.PRG/6F014.c:27)
struct Battle_Ai_ActionRangeShape {
    char reserved0;
    char rangeY;
    char rangeZ;
    u_char shape : 3;
    u_char angle : 5;
};
static_assert(sizeof(Battle_Ai_ActionRangeShape) == 0x4, "Battle_Ai_ActionRangeShape must keep its PS1 layout");

// decomp: func_800E5698_t (src/BATTLE/BATTLE.PRG/6F014.c:37)
struct Battle_Ai_ActionRangeView {
    char reserved0_0[84];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    Battle_Ai_ActionRangeView2* opaque5C;
};

// decomp: func_800E5698_t2 (src/BATTLE/BATTLE.PRG/6F014.c:32)
struct Battle_Ai_ActionRangeView2 {
    char reserved0[0x38];
    Battle_Ai_ActionRangeShape opaque38;
};
static_assert(sizeof(Battle_Ai_ActionRangeView2) == 0x3C, "Battle_Ai_ActionRangeView2 must keep its PS1 layout");

// decomp: func_800E5A74_t (src/BATTLE/BATTLE.PRG/6F014.c:19)
struct Battle_Ai_SavedSettingsPair {
    int reserved0;
    u_short opaque4;
};
static_assert(sizeof(Battle_Ai_SavedSettingsPair) == 0x8, "Battle_Ai_SavedSettingsPair must keep its PS1 layout");

// decomp: func_800E78F4_t2 (src/BATTLE/BATTLE.PRG/6F014.c:390)
struct Battle_Ai_WordPair {
    int reserved0;
    u_int reserved4;
};
static_assert(sizeof(Battle_Ai_WordPair) == 0x8, "Battle_Ai_WordPair must keep its PS1 layout");

// decomp: movementActorPosition (src/BATTLE/BATTLE.PRG/771A8.c:2269)
struct Battle_Ai_ActorPositionView {
    char reserved0[0x1C];
    short x, reserved1E, z;
};
static_assert(sizeof(Battle_Ai_ActorPositionView) == 0x22, "Battle_Ai_ActorPositionView must keep its PS1 layout");

// decomp: movementCheckObject (src/BATTLE/BATTLE.PRG/771A8.c:967)
struct Battle_Ai_CheckObjectView {
    char reserved0[8];
    unsigned int opaque8;
};

// decomp: movementRecoveryContext (src/BATTLE/BATTLE.PRG/771A8.c:2242)
struct Battle_Ai_RecoveryModelView {
    char reserved0[8];
    unsigned int opaque8;
    char reservedC[8];
    short angle14;
    char reserved16[0x10];
    short angle26;
};

// decomp: directionalCachePoint (src/BATTLE/BATTLE.PRG/771A8.c:890)
union Battle_Ai_DirectionCacheTile {
    unsigned int word;
    struct {
        unsigned char tileX, tag, tileZ, searchStatus;
    } tile;
};

// decomp: movementStepActor (src/BATTLE/BATTLE.PRG/771A8.c:1059)
struct Battle_Ai_StepActorView {
    char reserved0[0x1E];
    short y;
    char reserved20[0x3C];
    Battle_Ai_DirectionCacheTile tile;
    char reserved60_60[8];
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved60_6C[1380];
    void* nativePointer5D0; /* PS1 pointer slot 0x5D0 */
    void* nativePointer5D4; /* PS1 pointer slot 0x5D4 */
    void* nativePointer5D8; /* PS1 pointer slot 0x5D8 */
    void* nativePointer5DC; /* PS1 pointer slot 0x5DC */
    void* nativePointer5E0; /* PS1 pointer slot 0x5E0 */
    void* nativePointer5E4; /* PS1 pointer slot 0x5E4 */
    void* nativePointer5E8; /* PS1 pointer slot 0x5E8 */
    char reserved60_5EC[82];
    unsigned short height;
};

// decomp: nearestWaypointDestination (src/BATTLE/BATTLE.PRG/74088.c:944)
struct Battle_Ai_WaypointTile {
    unsigned char x, reserved1, z, reserved3;
};
static_assert(sizeof(Battle_Ai_WaypointTile) == 0x4, "Battle_Ai_WaypointTile must keep its PS1 layout");

// decomp: obstructionCache (src/BATTLE/BATTLE.PRG/771A8.c:793)
struct Battle_Ai_ObstructionCache {
    unsigned char reserved0, reserved1, tested, reserved3, result[8];
};
static_assert(sizeof(Battle_Ai_ObstructionCache) == 0xC, "Battle_Ai_ObstructionCache must keep its PS1 layout");

// decomp: obstructionVector (src/BATTLE/BATTLE.PRG/771A8.c:772)
struct Battle_Ai_ObstructionVector {
    short x, y, z, reserved6;
};
static_assert(sizeof(Battle_Ai_ObstructionVector) == 0x8, "Battle_Ai_ObstructionVector must keep its PS1 layout");

// decomp: obstructionModel (src/BATTLE/BATTLE.PRG/771A8.c:776)
struct Battle_Ai_ObstructionModel {
    char reserved0[0x1C];
    Battle_Ai_ObstructionVector position;
};

// decomp: radialMotion (src/BATTLE/BATTLE.PRG/7EE98.c:499)
struct Battle_Ai_RadialRetreatOrbitView {
    short heading;
    unsigned char phase, distanceCached;
    char reserved4[6];
    unsigned char isOrbiting, pathStep;
    char reservedC[4];
    short orbitAngle;
    char reserved12[6];
    short startX, startZ;
};
static_assert(sizeof(Battle_Ai_RadialRetreatOrbitView) == 0x1C, "Battle_Ai_RadialRetreatOrbitView must keep its PS1 layout");

// decomp: radialMotionActor (src/BATTLE/BATTLE.PRG/7EE98.c:503)
struct Battle_Ai_RetreatPlayerRecordView {
    char reserved0[8];
    unsigned int opaque8;
};

// decomp: radialMotionContext (src/BATTLE/BATTLE.PRG/7EE98.c:479)
struct Battle_Ai_ModelActiveView {
    char reserved0_0[104];
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved0_6C[1348];
    unsigned int reserved5B0Bit0 : 4, active : 1, reserved5B0Bit5 : 27;
};

// decomp: routeEdgeActor (src/BATTLE/BATTLE.PRG/771A8.c:559)
struct Battle_Ai_RouteActorView {
    char reserved0_0[84];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    short* position;
};

// decomp: routeEdgeGlobals (src/BATTLE/BATTLE.PRG/771A8.c:564)
struct Battle_Ai_RouteGateView {
    char reserved0[14];
    u_char closedGates;
};
static_assert(sizeof(Battle_Ai_RouteGateView) == 0xF, "Battle_Ai_RouteGateView must keep its PS1 layout");

// decomp: targetMovementActor (src/BATTLE/BATTLE.PRG/771A8.c:1900)
struct Battle_Ai_ApproachActorView {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    char reserved0_4[5];
    unsigned char id;
};

// decomp: targetMovementFlags (src/BATTLE/BATTLE.PRG/771A8.c:1896)
struct Battle_Ai_ApproachRelationBits {
    unsigned int active : 1, opaque0Bit1 : 1, opaque0Bit2 : 1, reserved0Bit3 : 2, visible : 1, reserved0Bit6 : 26;
};
static_assert(sizeof(Battle_Ai_ApproachRelationBits) == 0x4, "Battle_Ai_ApproachRelationBits must keep its PS1 layout");

// decomp: targetMovementModel (src/BATTLE/BATTLE.PRG/771A8.c:1908)
struct Battle_Ai_ApproachTargetRecordView {
    char reserved0[8];
    unsigned int opaque8;
    char reservedC[16];
    short x;
    char reserved1E[2];
    short z;
};

// decomp: terrainMovementActor (src/BATTLE/BATTLE.PRG/771A8.c:1484)
struct Battle_Ai_TerrainMoveActorView {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    char reserved0_4[5];
    unsigned char commandOpcode;
    char reservedA[2];
    unsigned short commandHeading;
};

// decomp: trackingMotion (src/BATTLE/BATTLE.PRG/7EE98.c:591)
struct Battle_Ai_OrbitTrackingOrbitView {
    unsigned short heading;
    unsigned char phase, distanceCached;
    int distanceSquared;
    char reserved8[4];
    short cooldown, requested, orbitAngle, savedHeight;
    unsigned char healthPhase;
    char reserved15[7];
    unsigned char started;
};
static_assert(sizeof(Battle_Ai_OrbitTrackingOrbitView) == 0x20, "Battle_Ai_OrbitTrackingOrbitView must keep its PS1 layout");

// decomp: trackingMotionActor (src/BATTLE/BATTLE.PRG/7EE98.c:595)
struct Battle_Ai_OrbitPlayerRecordView {
    char reserved0[0x1C];
    short x, reserved1E, z;
};

// decomp: trackingMotionStats (src/BATTLE/BATTLE.PRG/7EE98.c:570)
struct Battle_Ai_OrbitStatsView {
    char reserved0[0x18];
    short currentHp, maxHp;
};
static_assert(sizeof(Battle_Ai_OrbitStatsView) == 0x1C, "Battle_Ai_OrbitStatsView must keep its PS1 layout");

// decomp: wanderObject (src/BATTLE/BATTLE.PRG/771A8.c:1590)
struct Battle_Ai_WanderRecordView {
    char reserved0[8];
    unsigned int opaque8;
};

// decomp: actionReactionTarget (src/BATTLE/BATTLE.PRG/74088.c:685)
union Battle_Ai_RelationWordView {
    unsigned int word;
    struct {
        unsigned int reserved0Bits : 6, classification : 2, reserved8Bits : 7, timer : 5, counter : 7,
            reserved27Bit : 1, previousClassification : 2, reserved30Bits : 2;
    } reactionBits;
    struct {
        unsigned char firstByte, reserved1[3];
    } bytes;
};

// decomp: actorInitializationFlags (src/BATTLE/BATTLE.PRG/7CD68.c:827)
union Battle_Ai_ActorStateFlags {
    unsigned int word;
    struct {
        unsigned char byte0, byte1, byte2, byte3;
    } bytes;
    struct {
        unsigned int lowBits : 24, bits24 : 2, bits26 : 2, highBits : 4;
    } bits;
};

// decomp: actorRelation (src/BATTLE/BATTLE.PRG/6FC00.c:608)
union Battle_Ai_TargetRelation {
    u_int word;
    struct {
        u_int active : 1;
        u_int wasActive : 1;
        u_int hidden : 1;
        u_int inView : 1;
        u_int reserved4 : 1;
        u_int visible : 1;
        u_int classification : 2;
        u_int counter : 7;
        u_int timer : 5;
        u_int classificationTimer : 7;
        u_int blocked : 1;
        u_int previousClassification : 2;
        u_int outOfReach : 1;
        u_int onMarkedTile : 1;
    } bits;
};

// decomp: candidateMovementWord (src/BATTLE/BATTLE.PRG/71618.c:479)
union Battle_Ai_CandidateTileWord {
    unsigned int word;
    unsigned char bytes[4];
};
static_assert(sizeof(Battle_Ai_CandidateTileWord) == 0x4, "Battle_Ai_CandidateTileWord must keep its PS1 layout");

// decomp: da4bcPoint (src/BATTLE/BATTLE.PRG/71618.c:752)
union Battle_Ai_GuardTileWord {
    unsigned int word;
    unsigned char bytes[4];
};
static_assert(sizeof(Battle_Ai_GuardTileWord) == 0x4, "Battle_Ai_GuardTileWord must keep its PS1 layout");

// decomp: movementPath (src/BATTLE/BATTLE.PRG/771A8.c:2351)
union Battle_Ai_DetourWord {
    u_int word;
    struct {
        u_int reserved0 : 1;
        u_int heading : 3;
        u_int clockwise : 1;
        u_int steps : 7;
        u_int visited : 4;
        u_int stop : 1;
        u_int changed : 1;
        u_int cell : 4;
        u_int boundX : 5;
        u_int boundZ : 5;
    } bits;
};

// decomp: movementStepPosition (src/BATTLE/BATTLE.PRG/771A8.c:1025)
union Battle_Ai_StepPositionWord {
    unsigned int word;
    struct {
        short positionX;
        short positionY;
    } tile;
};

// decomp: stuckMovementPosition (src/BATTLE/BATTLE.PRG/71618.c:1187)
union Battle_Ai_BlockedTileWord {
    u_int word;
    struct {
        u_char tileX, tag, tileZ, searchStatus;
    } tile;
};

// decomp: targetMovementPoint (src/BATTLE/BATTLE.PRG/771A8.c:1893)
union Battle_Ai_ApproachTileWord {
    unsigned int word;
    unsigned char bytes[4];
};
static_assert(sizeof(Battle_Ai_ApproachTileWord) == 0x4, "Battle_Ai_ApproachTileWord must keep its PS1 layout");

// decomp: terrainMovementPoint (src/BATTLE/BATTLE.PRG/771A8.c:1478)
union Battle_Ai_TerrainTileWord {
    unsigned int word;
    struct {
        unsigned char tileX, tag, tileZ, searchStatus;
    } tile;
};
