// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_AIVIEWS_H_TYPES
#define VS_BATTLE_AI_AIVIEWS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_ActorStats Battle_ActorStats;
typedef struct Battle_Ai_ActionCandidate Battle_Ai_ActionCandidate;
typedef struct Battle_Ai_ActionChoiceEntryView Battle_Ai_ActionChoiceEntryView;
typedef struct Battle_Ai_ActionChoiceFlagsView Battle_Ai_ActionChoiceFlagsView;
typedef struct Battle_Ai_ActionChoiceInfoView Battle_Ai_ActionChoiceInfoView;
typedef struct Battle_Ai_ActionCostView Battle_Ai_ActionCostView;
typedef struct Battle_Ai_ActionMetadataEntry Battle_Ai_ActionMetadataEntry;
typedef struct Battle_Ai_ActionRangeShape Battle_Ai_ActionRangeShape;
typedef struct Battle_Ai_ActionRangeView Battle_Ai_ActionRangeView;
typedef struct Battle_Ai_ActionRangeView2 Battle_Ai_ActionRangeView2;
typedef struct Battle_Ai_ActionSetupView Battle_Ai_ActionSetupView;
typedef struct Battle_Ai_ActionSlotConfigView Battle_Ai_ActionSlotConfigView;
typedef struct Battle_Ai_ActionTiming Battle_Ai_ActionTiming;
typedef struct Battle_Ai_ActorListNodeView Battle_Ai_ActorListNodeView;
typedef struct Battle_Ai_ActorPositionView Battle_Ai_ActorPositionView;
typedef union Battle_Ai_ActorStateFlags Battle_Ai_ActorStateFlags;
typedef struct Battle_Ai_AiModelView Battle_Ai_AiModelView;
typedef struct Battle_Ai_ApproachActorView Battle_Ai_ApproachActorView;
typedef struct Battle_Ai_ApproachRelationBits Battle_Ai_ApproachRelationBits;
typedef struct Battle_Ai_ApproachTargetRecordView Battle_Ai_ApproachTargetRecordView;
typedef union Battle_Ai_ApproachTileWord Battle_Ai_ApproachTileWord;
typedef union Battle_Ai_BlockedTileWord Battle_Ai_BlockedTileWord;
typedef struct Battle_Ai_CandidateEntryView Battle_Ai_CandidateEntryView;
typedef struct Battle_Ai_CandidateFlags Battle_Ai_CandidateFlags;
typedef struct Battle_Ai_CandidateTable Battle_Ai_CandidateTable;
typedef union Battle_Ai_CandidateTileWord Battle_Ai_CandidateTileWord;
typedef struct Battle_Ai_CheckObjectView Battle_Ai_CheckObjectView;
typedef struct Battle_Ai_CreationModelView Battle_Ai_CreationModelView;
typedef struct Battle_Ai_CreationStatsView Battle_Ai_CreationStatsView;
typedef union Battle_Ai_CreationTileWord Battle_Ai_CreationTileWord;
typedef union Battle_Ai_DetourWord Battle_Ai_DetourWord;
typedef struct Battle_Ai_DirectionCacheBits Battle_Ai_DirectionCacheBits;
typedef union Battle_Ai_DirectionCacheTile Battle_Ai_DirectionCacheTile;
typedef struct Battle_Ai_DispatchActorView Battle_Ai_DispatchActorView;
typedef struct Battle_Ai_DispatchEntryView Battle_Ai_DispatchEntryView;
typedef struct Battle_Ai_DispatchModelView Battle_Ai_DispatchModelView;
typedef struct Battle_Ai_DispatchRegistryView Battle_Ai_DispatchRegistryView;
typedef struct Battle_Ai_EngagementActor Battle_Ai_EngagementActor;
typedef struct Battle_Ai_EngagementActorView Battle_Ai_EngagementActorView;
typedef struct Battle_Ai_EngagementData Battle_Ai_EngagementData;
typedef struct Battle_Ai_EscortActorView Battle_Ai_EscortActorView;
typedef struct Battle_Ai_EscortEntryView Battle_Ai_EscortEntryView;
typedef struct Battle_Ai_EscortRelationBits Battle_Ai_EscortRelationBits;
typedef struct Battle_Ai_EscortTargetView Battle_Ai_EscortTargetView;
typedef struct Battle_Ai_EvaluationActorView Battle_Ai_EvaluationActorView;
typedef struct Battle_Ai_EvaluationContextView Battle_Ai_EvaluationContextView;
typedef struct Battle_Ai_EvaluationStatsView Battle_Ai_EvaluationStatsView;
typedef struct Battle_Ai_GuardModelView Battle_Ai_GuardModelView;
typedef union Battle_Ai_GuardTileWord Battle_Ai_GuardTileWord;
typedef struct Battle_Ai_HpPairView Battle_Ai_HpPairView;
typedef struct Battle_Ai_MetadataActionView Battle_Ai_MetadataActionView;
typedef struct Battle_Ai_MetadataSlotView Battle_Ai_MetadataSlotView;
typedef struct Battle_Ai_MetadataStatsView Battle_Ai_MetadataStatsView;
typedef union Battle_Ai_MetadataWord Battle_Ai_MetadataWord;
typedef struct Battle_Ai_ModelActiveView Battle_Ai_ModelActiveView;
typedef struct Battle_Ai_ObstructionCache Battle_Ai_ObstructionCache;
typedef struct Battle_Ai_ObstructionModel Battle_Ai_ObstructionModel;
typedef struct Battle_Ai_ObstructionVector Battle_Ai_ObstructionVector;
typedef struct Battle_Ai_OrbitPlayerRecordView Battle_Ai_OrbitPlayerRecordView;
typedef struct Battle_Ai_OrbitStatsView Battle_Ai_OrbitStatsView;
typedef struct Battle_Ai_OrbitTrackingOrbitView Battle_Ai_OrbitTrackingOrbitView;
typedef struct Battle_Ai_PerceivedObjectView Battle_Ai_PerceivedObjectView;
typedef struct Battle_Ai_PerceptionActorView Battle_Ai_PerceptionActorView;
typedef struct Battle_Ai_PerceptionStatsView Battle_Ai_PerceptionStatsView;
typedef struct Battle_Ai_PositionLinkView Battle_Ai_PositionLinkView;
typedef struct Battle_Ai_RadialRetreatOrbitView Battle_Ai_RadialRetreatOrbitView;
typedef struct Battle_Ai_ReactionActionView Battle_Ai_ReactionActionView;
typedef struct Battle_Ai_ReactionOrbitView Battle_Ai_ReactionOrbitView;
typedef struct Battle_Ai_ReactionRecord Battle_Ai_ReactionRecord;
typedef struct Battle_Ai_RecoveryModelView Battle_Ai_RecoveryModelView;
typedef struct Battle_Ai_RelationByteView Battle_Ai_RelationByteView;
typedef union Battle_Ai_RelationWordView Battle_Ai_RelationWordView;
typedef struct Battle_Ai_RetreatPlayerRecordView Battle_Ai_RetreatPlayerRecordView;
typedef struct Battle_Ai_RouteActorView Battle_Ai_RouteActorView;
typedef struct Battle_Ai_RouteGateTile Battle_Ai_RouteGateTile;
typedef struct Battle_Ai_RouteGateView Battle_Ai_RouteGateView;
typedef struct Battle_Ai_SavedSettingsPair Battle_Ai_SavedSettingsPair;
typedef struct Battle_Ai_SettingsActorView Battle_Ai_SettingsActorView;
typedef union Battle_Ai_SettingsWord Battle_Ai_SettingsWord;
typedef struct Battle_Ai_StatApplicationView Battle_Ai_StatApplicationView;
typedef struct Battle_Ai_StatDeltaView Battle_Ai_StatDeltaView;
typedef struct Battle_Ai_StepActorView Battle_Ai_StepActorView;
typedef union Battle_Ai_StepPositionWord Battle_Ai_StepPositionWord;
typedef union Battle_Ai_TargetRelation Battle_Ai_TargetRelation;
typedef struct Battle_Ai_TerrainMoveActorView Battle_Ai_TerrainMoveActorView;
typedef union Battle_Ai_TerrainTileWord Battle_Ai_TerrainTileWord;
typedef struct Battle_Ai_WanderRecordView Battle_Ai_WanderRecordView;
typedef struct Battle_Ai_WanderTiming Battle_Ai_WanderTiming;
typedef struct Battle_Ai_WaypointTile Battle_Ai_WaypointTile;
typedef struct Battle_Ai_WordPair Battle_Ai_WordPair;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_BATTLE_AI_AIVIEWS_H_SET_TYPES_ONLY
#endif
#include "Battle/Ai/Navigation.h"
#ifdef VS_BATTLE_AI_AIVIEWS_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_BATTLE_AI_AIVIEWS_H_SET_TYPES_ONLY
#endif

// decomp: actionCandidateEntry (src/BATTLE/BATTLE.PRG/6FC00.c:500)
typedef struct Battle_Ai_CandidateEntryView {
    int reserved0;
    unsigned int row : 3, column : 2, action : 8, threshold : 16, reserved7Bit5 : 3;
    unsigned int distanceSquared;
    unsigned int targetId : 4, reservedCBit4 : 3, reservedCBit7 : 12, weight : 8, reservedFBit3 : 5;
    unsigned int reserved10Bit0 : 1, opaque10Bit1 : 1, reserved10Bit2 : 1, reserved10Bit3 : 1, reserved10Bit4 : 1, reserved10Bit5 : 1,
        range : 16, reserved12Bit6 : 10;
} Battle_Ai_CandidateEntryView;
_Static_assert(sizeof(Battle_Ai_CandidateEntryView) == 0x14, "Battle_Ai_CandidateEntryView must keep its PS1 layout");

// decomp: actionCandidateTarget (src/BATTLE/BATTLE.PRG/6FC00.c:503)
typedef struct Battle_Ai_RelationByteView {
    unsigned char opaque0, reserved1[3];
} Battle_Ai_RelationByteView;
_Static_assert(sizeof(Battle_Ai_RelationByteView) == 0x4, "Battle_Ai_RelationByteView must keep its PS1 layout");

// decomp: actionChoiceFlags (src/BATTLE/BATTLE.PRG/74088.c:859)
typedef struct Battle_Ai_ActionChoiceFlagsView {
    u_int reserved0Bit0 : 1;
    u_int reserved0Bit1 : 1;
    u_int reserved0Bit2 : 1;
    u_int reserved0Bit3 : 1;
    u_int reserved0Bit4 : 28;
} Battle_Ai_ActionChoiceFlagsView;
_Static_assert(sizeof(Battle_Ai_ActionChoiceFlagsView) == 0x4, "Battle_Ai_ActionChoiceFlagsView must keep its PS1 layout");

// decomp: actionChoice (src/BATTLE/BATTLE.PRG/74088.c:872)
typedef struct Battle_Ai_ActionChoiceEntryView {
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
} Battle_Ai_ActionChoiceEntryView;
_Static_assert(sizeof(Battle_Ai_ActionChoiceEntryView) == 0x14, "Battle_Ai_ActionChoiceEntryView must keep its PS1 layout");

// decomp: actionChoiceInfo (src/BATTLE/BATTLE.PRG/74088.c:897)
typedef struct Battle_Ai_ActionChoiceInfoView {
    char reserved0[0xC];
    u_int reservedCBit0 : 23;
    u_int reservedEBit7 : 1;
    u_int reservedFBit0 : 4;
    u_int reservedFBit4 : 1;
    u_int reservedFBit5 : 1;
    u_int reservedFBit6 : 2;
    char reserved10[0x24];
} Battle_Ai_ActionChoiceInfoView;
_Static_assert(sizeof(Battle_Ai_ActionChoiceInfoView) == 0x34, "Battle_Ai_ActionChoiceInfoView must keep its PS1 layout");

// decomp: actionCostView (src/BATTLE/BATTLE.PRG/74088.c:471)
typedef struct Battle_Ai_ActionCostView {
    unsigned int reserved0;
    char reserved4[48];
} Battle_Ai_ActionCostView;
_Static_assert(sizeof(Battle_Ai_ActionCostView) == 0x34, "Battle_Ai_ActionCostView must keep its PS1 layout");

// decomp: actionDispatchActor (src/BATTLE/BATTLE.PRG/71618.c:1055)
typedef struct Battle_Ai_DispatchActorView {
    struct Battle_Ai_DispatchActorView* next;
    int id;
    char reserved8_8[52];
    void* nativePointer3C; /* PS1 pointer slot 0x3C */
    char reserved8_40[4];
    Battle_Ai_DispatchModelView* model;
} Battle_Ai_DispatchActorView;

// decomp: actionDispatchEntry (src/BATTLE/BATTLE.PRG/71618.c:1040)
typedef struct Battle_Ai_DispatchEntryView {
    unsigned int reserved0, reserved4, reserved8, reservedC;
} Battle_Ai_DispatchEntryView;
_Static_assert(sizeof(Battle_Ai_DispatchEntryView) == 0x10, "Battle_Ai_DispatchEntryView must keep its PS1 layout");

// decomp: actionDispatchModel (src/BATTLE/BATTLE.PRG/71618.c:1049)
typedef struct Battle_Ai_DispatchModelView {
    char reserved0[8];
    unsigned int opaque8;
    char reservedC[6];
    unsigned char reserved12;
    char reserved13[7];
    short reserved1A;
    short position[3];
} Battle_Ai_DispatchModelView;

// decomp: actionDispatchRegistry (src/BATTLE/BATTLE.PRG/71618.c:1089)
typedef struct Battle_Ai_DispatchRegistryView {
    char reserved0[0x28];
    Battle_Ai_DispatchActorView* reserved28[4];
} Battle_Ai_DispatchRegistryView;

// decomp: actionMetadataWord (src/BATTLE/BATTLE.PRG/7CD68.c:497)
typedef union Battle_Ai_MetadataWord {
    unsigned int word;
    unsigned char bytes[4];
} Battle_Ai_MetadataWord;
_Static_assert(sizeof(Battle_Ai_MetadataWord) == 0x4, "Battle_Ai_MetadataWord must keep its PS1 layout");

// decomp: actionMetadataAction (src/BATTLE/BATTLE.PRG/7CD68.c:503)
typedef struct Battle_Ai_MetadataActionView {
    Battle_Ai_MetadataWord opaque0;
    char reserved4[8];
    unsigned int opaqueC;
    char reserved10[36];
} Battle_Ai_MetadataActionView;
_Static_assert(sizeof(Battle_Ai_MetadataActionView) == 0x34, "Battle_Ai_MetadataActionView must keep its PS1 layout");

// decomp: actionMetadataEntry (src/BATTLE/BATTLE.PRG/7CD68.c:515)
typedef struct Battle_Ai_ActionMetadataEntry {
    unsigned int reserved0Bit0 : 1, opaque0Bit1 : 1, opaque0Bit2 : 1, kind : 2, longRange : 1, special : 1, reserved0Bit7 : 1,
        radius : 16, reserved3Bit0 : 8;
    unsigned int rangeSquared;
} Battle_Ai_ActionMetadataEntry;
_Static_assert(sizeof(Battle_Ai_ActionMetadataEntry) == 0x8, "Battle_Ai_ActionMetadataEntry must keep its PS1 layout");

// decomp: actionMetadataSlot (src/BATTLE/BATTLE.PRG/7CD68.c:506)
typedef struct Battle_Ai_MetadataSlotView {
    unsigned short id, reserved2;
} Battle_Ai_MetadataSlotView;
_Static_assert(sizeof(Battle_Ai_MetadataSlotView) == 0x4, "Battle_Ai_MetadataSlotView must keep its PS1 layout");

// decomp: actionMetadataStats (src/BATTLE/BATTLE.PRG/7CD68.c:510)
typedef struct Battle_Ai_MetadataStatsView {
    char reserved0[0x8C0];
    Battle_Ai_MetadataSlotView slots[6][4];
} Battle_Ai_MetadataStatsView;
_Static_assert(sizeof(Battle_Ai_MetadataStatsView) == 0x920, "Battle_Ai_MetadataStatsView must keep its PS1 layout");

// decomp: actionReactionAction (src/BATTLE/BATTLE.PRG/74088.c:708)
typedef struct Battle_Ai_ReactionActionView {
    char reserved0[20];
    unsigned int effect1, effect2;
    char reserved1C[24];
} Battle_Ai_ReactionActionView;
_Static_assert(sizeof(Battle_Ai_ReactionActionView) == 0x34, "Battle_Ai_ReactionActionView must keep its PS1 layout");

// decomp: actionReactionMotion (src/BATTLE/BATTLE.PRG/74088.c:712)
typedef struct Battle_Ai_ReactionOrbitView {
    char reserved0[10];
    unsigned char isOrbiting;
} Battle_Ai_ReactionOrbitView;
_Static_assert(sizeof(Battle_Ai_ReactionOrbitView) == 0xB, "Battle_Ai_ReactionOrbitView must keep its PS1 layout");

// decomp: actionScoreActor (src/BATTLE/BATTLE.PRG/74088.c:563)
typedef struct Battle_Ai_ActorListNodeView {
    struct Battle_Ai_ActorListNodeView* next;
    int id;
} Battle_Ai_ActorListNodeView;

// decomp: actionSetupContext (src/BATTLE/BATTLE.PRG/71618.c:1004)
typedef struct Battle_Ai_ActionSetupView {
    char reserved0[0x13];
    u_char reserved13;
    char reserved14[0x12B];
    u_char reserved13F;
    char reserved140[0xEC];
    Battle_Ai_ActionCandidate* reserved22C;
} Battle_Ai_ActionSetupView;

// decomp: actionSlotConfig (src/BATTLE/BATTLE.PRG/74088.c:851)
typedef struct Battle_Ai_ActionSlotConfigView {
    u_int reserved0Bit0 : 2;
    u_int reserved0Bit2 : 1;
    u_int reserved0Bit3 : 2;
    u_int reserved0Bit5 : 1;
    u_int reserved0Bit6 : 1;
    u_int reserved0Bit7 : 1;
    u_int reserved1Bit0 : 16;
    u_int reserved3Bit0 : 8;
    u_int reserved4;
} Battle_Ai_ActionSlotConfigView;
_Static_assert(sizeof(Battle_Ai_ActionSlotConfigView) == 0x8, "Battle_Ai_ActionSlotConfigView must keep its PS1 layout");

// decomp: actionStatDelta (src/BATTLE/BATTLE.PRG/74088.c:459)
typedef struct Battle_Ai_StatDeltaView {
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
} Battle_Ai_StatDeltaView;
_Static_assert(sizeof(Battle_Ai_StatDeltaView) == 0x44, "Battle_Ai_StatDeltaView must keep its PS1 layout");

// decomp: actionStatApplication (src/BATTLE/BATTLE.PRG/74088.c:467)
typedef struct Battle_Ai_StatApplicationView {
    unsigned short action;
    char reserved2[2];
    Battle_Ai_StatDeltaView first;
    char reserved48[2];
    unsigned short count;
    Battle_Ai_StatDeltaView entries[1];
} Battle_Ai_StatApplicationView;
_Static_assert(sizeof(Battle_Ai_StatApplicationView) == 0x90, "Battle_Ai_StatApplicationView must keep its PS1 layout");

// decomp: actorEvaluationActor (src/BATTLE/BATTLE.PRG/74088.c:1523)
typedef struct Battle_Ai_EvaluationActorView {
    struct Battle_Ai_EvaluationActorView* next;
    int id;
    unsigned char opaque8;
    char reserved9[51];
    Battle_Ai_EvaluationStatsView* stats;
} Battle_Ai_EvaluationActorView;

// decomp: actorEvaluationContext (src/BATTLE/BATTLE.PRG/74088.c:1544)
typedef struct Battle_Ai_EvaluationContextView {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    unsigned char id;
    char reserved5_5[55];
    void* nativePointer3C; /* PS1 pointer slot 0x3C */
    char reserved5_40[4];
    void* opaque44;
} Battle_Ai_EvaluationContextView;

// decomp: actorEvaluationStats (src/BATTLE/BATTLE.PRG/74088.c:1516)
typedef struct Battle_Ai_EvaluationStatsView {
    char reserved0[24];
    short hp;
    char reserved1A[23];
    unsigned char opaque31;
    char reserved32[1];
    unsigned char opaque33;
} Battle_Ai_EvaluationStatsView;
_Static_assert(sizeof(Battle_Ai_EvaluationStatsView) == 0x34, "Battle_Ai_EvaluationStatsView must keep its PS1 layout");

// decomp: actorInitializationPoint (src/BATTLE/BATTLE.PRG/7CD68.c:833)
typedef union Battle_Ai_CreationTileWord {
    unsigned int word;
    struct {
        unsigned char tileX, tag, tileZ, searchStatus;
    } tile;
} Battle_Ai_CreationTileWord;

// decomp: actorInitializationModel (src/BATTLE/BATTLE.PRG/7CD68.c:853)
typedef struct Battle_Ai_CreationModelView {
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
} Battle_Ai_CreationModelView;

// decomp: actorInitializationStats (src/BATTLE/BATTLE.PRG/7CD68.c:840)
typedef struct Battle_Ai_CreationStatsView {
    char reserved0[48];
    unsigned char opaque30;
    unsigned char opaque31;
    unsigned char opaque32;
    unsigned char opaque33;
} Battle_Ai_CreationStatsView;
_Static_assert(sizeof(Battle_Ai_CreationStatsView) == 0x34, "Battle_Ai_CreationStatsView must keep its PS1 layout");

// decomp: actorObject (src/BATTLE/BATTLE.PRG/6FC00.c:588)
typedef struct Battle_Ai_PerceivedObjectView {
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
} Battle_Ai_PerceivedObjectView;

// decomp: actorPhaseFlags (src/BATTLE/BATTLE.PRG/73700.c:447)
typedef struct Battle_Ai_EngagementActorView {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    char reserved0_4[4];
    unsigned int reserved8Bit0 : 26, mode : 2, reservedBBit4 : 4;
} Battle_Ai_EngagementActorView;

// decomp: actorSettingsWord (src/BATTLE/BATTLE.PRG/7CD68.c:604)
typedef union Battle_Ai_SettingsWord {
    unsigned int word;
    unsigned short halves[2];
    unsigned char bytes[4];
} Battle_Ai_SettingsWord;
_Static_assert(sizeof(Battle_Ai_SettingsWord) == 0x4, "Battle_Ai_SettingsWord must keep its PS1 layout");

// decomp: actorSettingsActor (src/BATTLE/BATTLE.PRG/7CD68.c:616)
typedef struct Battle_Ai_SettingsActorView {
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
} Battle_Ai_SettingsActorView;

// decomp: actorSettingsEntry (src/BATTLE/BATTLE.PRG/7CD68.c:677)
typedef struct Battle_Ai_WanderTiming {
    unsigned short movePeriod, restPeriod;
    unsigned char rate, reserved5;
} Battle_Ai_WanderTiming;
_Static_assert(sizeof(Battle_Ai_WanderTiming) == 0x6, "Battle_Ai_WanderTiming must keep its PS1 layout");

// decomp: actorUpdateActor (src/BATTLE/BATTLE.PRG/6FC00.c:868)
typedef struct Battle_Ai_PerceptionActorView {
    struct Battle_Ai_PerceptionActorView* next;
    int id;
    u_char opaque8;
    u_char opaque9;
    char reservedA;
    u_char opaqueBBit0 : 2;
    u_char reservedBBit2 : 6;
    char reservedC[0x30];
    Battle_ActorStats* stats;
} Battle_Ai_PerceptionActorView;

// decomp: actorUpdateHp (src/BATTLE/BATTLE.PRG/6FC00.c:878)
typedef struct Battle_Ai_HpPairView {
    u_int current : 16;
    u_int max : 16;
} Battle_Ai_HpPairView;
_Static_assert(sizeof(Battle_Ai_HpPairView) == 0x4, "Battle_Ai_HpPairView must keep its PS1 layout");

// decomp: actorUpdatePoint (src/BATTLE/BATTLE.PRG/6FC00.c:873)
typedef struct Battle_Ai_PositionLinkView {
    u_int position;
    u_int opaque4;
} Battle_Ai_PositionLinkView;
_Static_assert(sizeof(Battle_Ai_PositionLinkView) == 0x8, "Battle_Ai_PositionLinkView must keep its PS1 layout");

// decomp: actorUpdateStats (src/BATTLE/BATTLE.PRG/6FC00.c:885)
typedef struct Battle_Ai_PerceptionStatsView {
    char reserved0[0x18];
    Battle_Ai_HpPairView hp;
    int reserved1C;
    int reserved20;
} Battle_Ai_PerceptionStatsView;
_Static_assert(sizeof(Battle_Ai_PerceptionStatsView) == 0x24, "Battle_Ai_PerceptionStatsView must keep its PS1 layout");

// decomp: da4bcActor (src/BATTLE/BATTLE.PRG/71618.c:776)
typedef struct Battle_Ai_EscortActorView {
    struct Battle_Ai_EscortActorView* next;
    int id;
} Battle_Ai_EscortActorView;

// decomp: da4bcEntry (src/BATTLE/BATTLE.PRG/71618.c:713)
typedef struct Battle_Ai_EscortEntryView {
    unsigned int reserved0, reserved4, reserved8, opaqueC;
} Battle_Ai_EscortEntryView;
_Static_assert(sizeof(Battle_Ai_EscortEntryView) == 0x10, "Battle_Ai_EscortEntryView must keep its PS1 layout");

// decomp: da4bcEntryTarget (src/BATTLE/BATTLE.PRG/71618.c:780)
typedef struct Battle_Ai_EscortTargetView {
    unsigned int target : 4, reserved0Bit4 : 28;
} Battle_Ai_EscortTargetView;
_Static_assert(sizeof(Battle_Ai_EscortTargetView) == 0x4, "Battle_Ai_EscortTargetView must keep its PS1 layout");

// decomp: da4bcModel (src/BATTLE/BATTLE.PRG/71618.c:761)
typedef struct Battle_Ai_GuardModelView {
    char reserved0[28];
    short x;
    char reserved1E[2];
    short z;
    char reserved22[58];
    Battle_Ai_TilePosition tile;
} Battle_Ai_GuardModelView;

// decomp: da4bcTargetBits (src/BATTLE/BATTLE.PRG/71618.c:766)
typedef struct Battle_Ai_EscortRelationBits {
    unsigned int reserved0Bit0 : 6, mode : 2, reserved1Bit0 : 12, category : 7, reserved3Bit3 : 1, oldMode : 2,
        reserved3Bit6 : 2;
} Battle_Ai_EscortRelationBits;
_Static_assert(sizeof(Battle_Ai_EscortRelationBits) == 0x4, "Battle_Ai_EscortRelationBits must keep its PS1 layout");

// decomp: directionalCacheBits (src/BATTLE/BATTLE.PRG/771A8.c:900)
typedef struct Battle_Ai_DirectionCacheBits {
    unsigned char tested, valid;
} Battle_Ai_DirectionCacheBits;
_Static_assert(sizeof(Battle_Ai_DirectionCacheBits) == 0x2, "Battle_Ai_DirectionCacheBits must keep its PS1 layout");

// decomp: func_800DC888_t (src/BATTLE/BATTLE.PRG/6F014.c:253)
typedef struct Battle_Ai_ActionTiming {
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
} Battle_Ai_ActionTiming;
_Static_assert(sizeof(Battle_Ai_ActionTiming) == 0x18, "Battle_Ai_ActionTiming must keep its PS1 layout");

// decomp: func_800DCAA0_t1 (src/BATTLE/BATTLE.PRG/6F014.c:417)
typedef struct Battle_Ai_CandidateTable {
    char reserved0[0x88];
    u_char reserved88;
} Battle_Ai_CandidateTable;
_Static_assert(sizeof(Battle_Ai_CandidateTable) == 0x89, "Battle_Ai_CandidateTable must keep its PS1 layout");

// decomp: func_800DCAA0_t2 (src/BATTLE/BATTLE.PRG/6F014.c:96)
typedef struct Battle_Ai_CandidateFlags {
    u_int opaque0Bit0 : 1;
    u_int opaque0Bit1 : 1;
    u_int opaque0Bit2 : 1;
    u_int opaque0Bit3 : 1;
    u_int opaque0Bit4 : 1;
    u_int opaque0Bit5 : 1;
    u_int opaque0Bit6 : 16;
    u_int reserved2Bit6 : 10;
} Battle_Ai_CandidateFlags;
_Static_assert(sizeof(Battle_Ai_CandidateFlags) == 0x4, "Battle_Ai_CandidateFlags must keep its PS1 layout");

// decomp: func_800DEB10_t (src/BATTLE/BATTLE.PRG/6F014.c:278)
typedef struct Battle_Ai_EngagementActor {
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
} Battle_Ai_EngagementActor;
_Static_assert(sizeof(Battle_Ai_EngagementActor) == 0x44, "Battle_Ai_EngagementActor must keep its PS1 layout");

// decomp: func_800DEB10_t2 (src/BATTLE/BATTLE.PRG/6F014.c:85)
typedef struct Battle_Ai_EngagementData {
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
} Battle_Ai_EngagementData;

// decomp: func_800DEEA4_t (src/BATTLE/BATTLE.PRG/6F014.c:287)
typedef struct Battle_Ai_ReactionRecord {
    u_short opaque0;
    char reserved2[2];
    Battle_Ai_EngagementActor opaque4;
    char reserved48[2];
    u_short opaque4A;
    Battle_Ai_EngagementActor opaque4C[0];
} Battle_Ai_ReactionRecord;
_Static_assert(sizeof(Battle_Ai_ReactionRecord) == 0x4C, "Battle_Ai_ReactionRecord must keep its PS1 layout");

// decomp: func_800DFA54_t (src/BATTLE/BATTLE.PRG/6F014.c:49)
typedef struct Battle_Ai_RouteGateTile {
    u_char opaque0;
    u_char opaque1;
    u_char opaque2;
} Battle_Ai_RouteGateTile;
_Static_assert(sizeof(Battle_Ai_RouteGateTile) == 0x3, "Battle_Ai_RouteGateTile must keep its PS1 layout");

// layout: ledger (upstream merge) for func_800E1238_t
typedef struct Battle_Ai_AiModelView {
    /* 0x00 */ int reserved0;
    /* 0x04 */ int reserved4;
    /* 0x08 */ u_int reserved8Bit0 : 21;
    /* 0x0A */ u_int opaqueABit5 : 1;
    /* 0x0A */ u_int reservedABit6 : 10;
    /* 0x0C */ char reservedC[8];
    /* 0x14 */ short opaque14;
    /* 0x16 */ char reserved16[6];
    /* 0x1C */ SVECTOR opaque1C;
    /* 0x24 */ char reserved24[2];
    /* 0x26 */ short opaque26;
    /* 0x28 */ char reserved28[52];
    /* 0x5C */ Battle_Ai_TilePosition opaque5C;
    char reserved60_60[8];
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved60_6C[1348];
    /* 0x5B0 */ u_int reserved5B0Bit0 : 4;
    /* 0x5B0 */ u_int opaque5B0Bit4 : 1;
} Battle_Ai_AiModelView;

// decomp: func_800E5568_t (src/BATTLE/BATTLE.PRG/6F014.c:27)
typedef struct Battle_Ai_ActionRangeShape {
    char reserved0;
    char rangeY;
    char rangeZ;
    u_char shape : 3;
    u_char angle : 5;
} Battle_Ai_ActionRangeShape;
_Static_assert(sizeof(Battle_Ai_ActionRangeShape) == 0x4, "Battle_Ai_ActionRangeShape must keep its PS1 layout");

// decomp: func_800E5698_t (src/BATTLE/BATTLE.PRG/6F014.c:37)
typedef struct Battle_Ai_ActionRangeView {
    char reserved0_0[84];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    Battle_Ai_ActionRangeView2* opaque5C;
} Battle_Ai_ActionRangeView;

// decomp: func_800E5698_t2 (src/BATTLE/BATTLE.PRG/6F014.c:32)
typedef struct Battle_Ai_ActionRangeView2 {
    char reserved0[0x38];
    Battle_Ai_ActionRangeShape opaque38;
} Battle_Ai_ActionRangeView2;
_Static_assert(sizeof(Battle_Ai_ActionRangeView2) == 0x3C, "Battle_Ai_ActionRangeView2 must keep its PS1 layout");

// decomp: func_800E5A74_t (src/BATTLE/BATTLE.PRG/6F014.c:19)
typedef struct Battle_Ai_SavedSettingsPair {
    int reserved0;
    u_short opaque4;
} Battle_Ai_SavedSettingsPair;
_Static_assert(sizeof(Battle_Ai_SavedSettingsPair) == 0x8, "Battle_Ai_SavedSettingsPair must keep its PS1 layout");

// decomp: func_800E78F4_t2 (src/BATTLE/BATTLE.PRG/6F014.c:390)
typedef struct Battle_Ai_WordPair {
    int reserved0;
    u_int reserved4;
} Battle_Ai_WordPair;
_Static_assert(sizeof(Battle_Ai_WordPair) == 0x8, "Battle_Ai_WordPair must keep its PS1 layout");

// decomp: movementActorPosition (src/BATTLE/BATTLE.PRG/771A8.c:2269)
typedef struct Battle_Ai_ActorPositionView {
    char reserved0[0x1C];
    short x, reserved1E, z;
} Battle_Ai_ActorPositionView;
_Static_assert(sizeof(Battle_Ai_ActorPositionView) == 0x22, "Battle_Ai_ActorPositionView must keep its PS1 layout");

// decomp: movementCheckObject (src/BATTLE/BATTLE.PRG/771A8.c:967)
typedef struct Battle_Ai_CheckObjectView {
    char reserved0[8];
    unsigned int opaque8;
} Battle_Ai_CheckObjectView;

// decomp: movementRecoveryContext (src/BATTLE/BATTLE.PRG/771A8.c:2242)
typedef struct Battle_Ai_RecoveryModelView {
    char reserved0[8];
    unsigned int opaque8;
    char reservedC[8];
    short angle14;
    char reserved16[0x10];
    short angle26;
} Battle_Ai_RecoveryModelView;

// decomp: directionalCachePoint (src/BATTLE/BATTLE.PRG/771A8.c:890)
typedef union Battle_Ai_DirectionCacheTile {
    unsigned int word;
    struct {
        unsigned char tileX, tag, tileZ, searchStatus;
    } tile;
} Battle_Ai_DirectionCacheTile;

// decomp: movementStepActor (src/BATTLE/BATTLE.PRG/771A8.c:1059)
typedef struct Battle_Ai_StepActorView {
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
} Battle_Ai_StepActorView;

// decomp: nearestWaypointDestination (src/BATTLE/BATTLE.PRG/74088.c:944)
typedef struct Battle_Ai_WaypointTile {
    unsigned char x, reserved1, z, reserved3;
} Battle_Ai_WaypointTile;
_Static_assert(sizeof(Battle_Ai_WaypointTile) == 0x4, "Battle_Ai_WaypointTile must keep its PS1 layout");

// decomp: obstructionCache (src/BATTLE/BATTLE.PRG/771A8.c:793)
typedef struct Battle_Ai_ObstructionCache {
    unsigned char reserved0, reserved1, tested, reserved3, result[8];
} Battle_Ai_ObstructionCache;
_Static_assert(sizeof(Battle_Ai_ObstructionCache) == 0xC, "Battle_Ai_ObstructionCache must keep its PS1 layout");

// decomp: obstructionVector (src/BATTLE/BATTLE.PRG/771A8.c:772)
typedef struct Battle_Ai_ObstructionVector {
    short x, y, z, reserved6;
} Battle_Ai_ObstructionVector;
_Static_assert(sizeof(Battle_Ai_ObstructionVector) == 0x8, "Battle_Ai_ObstructionVector must keep its PS1 layout");

// decomp: obstructionModel (src/BATTLE/BATTLE.PRG/771A8.c:776)
typedef struct Battle_Ai_ObstructionModel {
    char reserved0[0x1C];
    Battle_Ai_ObstructionVector position;
} Battle_Ai_ObstructionModel;

// decomp: radialMotion (src/BATTLE/BATTLE.PRG/7EE98.c:499)
typedef struct Battle_Ai_RadialRetreatOrbitView {
    short heading;
    unsigned char phase, distanceCached;
    char reserved4[6];
    unsigned char isOrbiting, pathStep;
    char reservedC[4];
    short orbitAngle;
    char reserved12[6];
    short startX, startZ;
} Battle_Ai_RadialRetreatOrbitView;
_Static_assert(sizeof(Battle_Ai_RadialRetreatOrbitView) == 0x1C, "Battle_Ai_RadialRetreatOrbitView must keep its PS1 layout");

// decomp: radialMotionActor (src/BATTLE/BATTLE.PRG/7EE98.c:503)
typedef struct Battle_Ai_RetreatPlayerRecordView {
    char reserved0[8];
    unsigned int opaque8;
} Battle_Ai_RetreatPlayerRecordView;

// decomp: radialMotionContext (src/BATTLE/BATTLE.PRG/7EE98.c:479)
typedef struct Battle_Ai_ModelActiveView {
    char reserved0_0[104];
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved0_6C[1348];
    unsigned int reserved5B0Bit0 : 4, active : 1, reserved5B0Bit5 : 27;
} Battle_Ai_ModelActiveView;

// decomp: routeEdgeActor (src/BATTLE/BATTLE.PRG/771A8.c:559)
typedef struct Battle_Ai_RouteActorView {
    char reserved0_0[84];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    short* position;
} Battle_Ai_RouteActorView;

// decomp: routeEdgeGlobals (src/BATTLE/BATTLE.PRG/771A8.c:564)
typedef struct Battle_Ai_RouteGateView {
    char reserved0[14];
    u_char closedGates;
} Battle_Ai_RouteGateView;
_Static_assert(sizeof(Battle_Ai_RouteGateView) == 0xF, "Battle_Ai_RouteGateView must keep its PS1 layout");

// decomp: targetMovementActor (src/BATTLE/BATTLE.PRG/771A8.c:1900)
typedef struct Battle_Ai_ApproachActorView {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    char reserved0_4[5];
    unsigned char id;
} Battle_Ai_ApproachActorView;

// decomp: targetMovementFlags (src/BATTLE/BATTLE.PRG/771A8.c:1896)
typedef struct Battle_Ai_ApproachRelationBits {
    unsigned int active : 1, opaque0Bit1 : 1, opaque0Bit2 : 1, reserved0Bit3 : 2, visible : 1, reserved0Bit6 : 26;
} Battle_Ai_ApproachRelationBits;
_Static_assert(sizeof(Battle_Ai_ApproachRelationBits) == 0x4, "Battle_Ai_ApproachRelationBits must keep its PS1 layout");

// decomp: targetMovementModel (src/BATTLE/BATTLE.PRG/771A8.c:1908)
typedef struct Battle_Ai_ApproachTargetRecordView {
    char reserved0[8];
    unsigned int opaque8;
    char reservedC[16];
    short x;
    char reserved1E[2];
    short z;
} Battle_Ai_ApproachTargetRecordView;

// decomp: terrainMovementActor (src/BATTLE/BATTLE.PRG/771A8.c:1484)
typedef struct Battle_Ai_TerrainMoveActorView {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    char reserved0_4[5];
    unsigned char commandOpcode;
    char reservedA[2];
    unsigned short commandHeading;
} Battle_Ai_TerrainMoveActorView;

// decomp: trackingMotion (src/BATTLE/BATTLE.PRG/7EE98.c:591)
typedef struct Battle_Ai_OrbitTrackingOrbitView {
    unsigned short heading;
    unsigned char phase, distanceCached;
    int distanceSquared;
    char reserved8[4];
    short cooldown, requested, orbitAngle, savedHeight;
    unsigned char healthPhase;
    char reserved15[7];
    unsigned char started;
} Battle_Ai_OrbitTrackingOrbitView;
_Static_assert(sizeof(Battle_Ai_OrbitTrackingOrbitView) == 0x20, "Battle_Ai_OrbitTrackingOrbitView must keep its PS1 layout");

// decomp: trackingMotionActor (src/BATTLE/BATTLE.PRG/7EE98.c:595)
typedef struct Battle_Ai_OrbitPlayerRecordView {
    char reserved0[0x1C];
    short x, reserved1E, z;
} Battle_Ai_OrbitPlayerRecordView;

// decomp: trackingMotionStats (src/BATTLE/BATTLE.PRG/7EE98.c:570)
typedef struct Battle_Ai_OrbitStatsView {
    char reserved0[0x18];
    short currentHp, maxHp;
} Battle_Ai_OrbitStatsView;
_Static_assert(sizeof(Battle_Ai_OrbitStatsView) == 0x1C, "Battle_Ai_OrbitStatsView must keep its PS1 layout");

// decomp: wanderObject (src/BATTLE/BATTLE.PRG/771A8.c:1590)
typedef struct Battle_Ai_WanderRecordView {
    char reserved0[8];
    unsigned int opaque8;
} Battle_Ai_WanderRecordView;

// decomp: actionReactionTarget (src/BATTLE/BATTLE.PRG/74088.c:685)
typedef union Battle_Ai_RelationWordView {
    unsigned int word;
    struct {
        unsigned int reserved0Bits : 6, classification : 2, reserved8Bits : 7, timer : 5, counter : 7,
            reserved27Bit : 1, previousClassification : 2, reserved30Bits : 2;
    } reactionBits;
    struct {
        unsigned char firstByte, reserved1[3];
    } bytes;
} Battle_Ai_RelationWordView;

// decomp: actorInitializationFlags (src/BATTLE/BATTLE.PRG/7CD68.c:827)
typedef union Battle_Ai_ActorStateFlags {
    unsigned int word;
    struct {
        unsigned char byte0, byte1, byte2, byte3;
    } bytes;
    struct {
        unsigned int lowBits : 24, bits24 : 2, bits26 : 2, highBits : 4;
    } bits;
} Battle_Ai_ActorStateFlags;

// decomp: actorRelation (src/BATTLE/BATTLE.PRG/6FC00.c:608)
typedef union Battle_Ai_TargetRelation {
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
} Battle_Ai_TargetRelation;

// decomp: candidateMovementWord (src/BATTLE/BATTLE.PRG/71618.c:479)
typedef union Battle_Ai_CandidateTileWord {
    unsigned int word;
    unsigned char bytes[4];
} Battle_Ai_CandidateTileWord;
_Static_assert(sizeof(Battle_Ai_CandidateTileWord) == 0x4, "Battle_Ai_CandidateTileWord must keep its PS1 layout");

// decomp: da4bcPoint (src/BATTLE/BATTLE.PRG/71618.c:752)
typedef union Battle_Ai_GuardTileWord {
    unsigned int word;
    unsigned char bytes[4];
} Battle_Ai_GuardTileWord;
_Static_assert(sizeof(Battle_Ai_GuardTileWord) == 0x4, "Battle_Ai_GuardTileWord must keep its PS1 layout");

// decomp: movementPath (src/BATTLE/BATTLE.PRG/771A8.c:2351)
typedef union Battle_Ai_DetourWord {
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
} Battle_Ai_DetourWord;

// decomp: movementStepPosition (src/BATTLE/BATTLE.PRG/771A8.c:1025)
typedef union Battle_Ai_StepPositionWord {
    unsigned int word;
    struct {
        short positionX;
        short positionY;
    } tile;
} Battle_Ai_StepPositionWord;

// decomp: stuckMovementPosition (src/BATTLE/BATTLE.PRG/71618.c:1187)
typedef union Battle_Ai_BlockedTileWord {
    u_int word;
    struct {
        u_char tileX, tag, tileZ, searchStatus;
    } tile;
} Battle_Ai_BlockedTileWord;

// decomp: targetMovementPoint (src/BATTLE/BATTLE.PRG/771A8.c:1893)
typedef union Battle_Ai_ApproachTileWord {
    unsigned int word;
    unsigned char bytes[4];
} Battle_Ai_ApproachTileWord;
_Static_assert(sizeof(Battle_Ai_ApproachTileWord) == 0x4, "Battle_Ai_ApproachTileWord must keep its PS1 layout");

// decomp: terrainMovementPoint (src/BATTLE/BATTLE.PRG/771A8.c:1478)
typedef union Battle_Ai_TerrainTileWord {
    unsigned int word;
    struct {
        unsigned char tileX, tag, tileZ, searchStatus;
    } tile;
} Battle_Ai_TerrainTileWord;

#endif // VS_BATTLE_AI_AIVIEWS_H_TYPES
