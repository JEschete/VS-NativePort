// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_AISTATE_H_TYPES
#define VS_BATTLE_AI_AISTATE_H_TYPES
#include "Legacy/Psx.h"

struct Actors_ActorRecord;
struct Battle_Actor;
struct Battle_ActorStats;
struct Battle_Ai_ActionCandidate;
struct Battle_Ai_ActionChoiceView;
struct Battle_Ai_ActionExecutionView;
struct Battle_Ai_ActionMetadataView;
struct Battle_Ai_ActionPlanView;
struct Battle_Ai_ActionSlotMetadata;
struct Battle_Ai_ActorAiState;
struct Battle_Ai_AiActorView;
struct Battle_Ai_AiCreationView;
struct Battle_Ai_ApproachActorView;
struct Battle_Ai_ApproachTargetRecordView;
struct Battle_Ai_AreaRouteView;
struct Battle_Ai_BlockedRecoveryView;
struct Battle_Ai_CandidateMovementView;
struct Battle_Ai_CandidateReadinessView;
struct Battle_Ai_CheckObjectView;
struct Battle_Ai_CreationModelView;
struct Battle_Ai_CreationStatsView;
struct Battle_Ai_DecisionDispatchView;
struct Battle_Ai_DirectionCacheView;
struct Battle_Ai_EngagementActorView;
struct Battle_Ai_EngagementData;
struct Battle_Ai_EngagementView;
struct Battle_Ai_EscortEntryView;
struct Battle_Ai_EscortRelationsView;
struct Battle_Ai_EscortView;
struct Battle_Ai_FloorHeightView;
struct Battle_Ai_MetadataStatsView;
struct Battle_Ai_ModelActiveView;
struct Battle_Ai_ModelStatePrefix;
struct Battle_Ai_MoveTargetView;
struct Battle_Ai_MovementCheckView;
struct Battle_Ai_MovementDecisionView;
struct Battle_Ai_MovementDestinationView;
struct Battle_Ai_MovementPathView;
struct Battle_Ai_MovementRecoveryView;
struct Battle_Ai_MovementStepView;
struct Battle_Ai_ObstructionModel;
struct Battle_Ai_ObstructionView;
struct Battle_Ai_OrbitStatsView;
struct Battle_Ai_OrbitTrackingView;
struct Battle_Ai_PatrolView;
struct Battle_Ai_PerceivedObjectView;
struct Battle_Ai_PerceptionUpdateView;
struct Battle_Ai_PerceptionView;
struct Battle_Ai_PlanEvaluationView;
struct Battle_Ai_PositionLinkView;
struct Battle_Ai_RadialRetreatView;
struct Battle_Ai_ReactionView;
struct Battle_Ai_RecoveryModelView;
struct Battle_Ai_RoomAiState;
struct Battle_Ai_SettingsActorView;
struct Battle_Ai_SettingsView;
struct Battle_Ai_TargetApproachView;
struct Battle_Ai_TerrainMoveActorView;
struct Battle_Ai_TerrainMoveView;
struct Battle_Ai_WanderRecordView;
struct Battle_Ai_WanderView;
struct Battle_Ai_WaypointSearchView;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_BATTLE_AI_AISTATE_H_SET_TYPES_ONLY
#endif
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#ifdef VS_BATTLE_AI_AISTATE_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_BATTLE_AI_AISTATE_H_SET_TYPES_ONLY
#endif

// layout: ledger (upstream merge) for D_800F58BC_t
struct Battle_Ai_RoomAiState {
    /* 0x00 */ u_char maxTileX;
    /* 0x01 */ u_char maxTileZ;
    /* 0x02 */ u_char minTileX;
    /* 0x03 */ u_char minTileZ;
    /* 0x04 */ char reserved4[4];
    /* 0x08 */ u_char defaultActionDelay;
    /* 0x09 */ union {
        char reserved9;
        struct {
            char reserved9Prefix;
            u_char allowsNonSpellActions;
            union {
                char reservedB[9];
                struct {
                    u_char reservedBPrefix[2];
                    u_char engagementLimit;
                    u_char reservedEPrefix[2];
                    u_char engaged_mask_bytes[2];
                    u_char engagedCount;
                    u_char reserved13;
                };
            };
        };
    };
    /* 0x14 */ int flightCeiling;
    /* 0x18 */ u_short readinessLevel;
    /* 0x1A */ char reserved1A[2];
    /* 0x1C */ Battle_Ai_ActorAiState *engagedHead;
    /* 0x20 */ Battle_Ai_ActorAiState *engagedTail;
    /* 0x24 */ int approachMoves;
    /* 0x28 */ Battle_Actor *specialActors[4];
};

// decomp: actionCandidateState (src/BATTLE/BATTLE.PRG/6FC00.c:513)
struct Battle_Ai_CandidateReadinessView {
    char reserved0_0[84];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved0_6C[197];
    unsigned char actionStatus;
    char reserved132_132[90];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved132_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    Battle_Ai_RelationByteView relations[16];
    unsigned int distancesSquared[16];
    char reserved224_224[8];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    char reserved224_230[512];
    Battle_Ai_CandidateEntryView* candidateOrder[16];
    int candidateCount;
};

// decomp: actionChoiceState (src/BATTLE/BATTLE.PRG/74088.c:885)
struct Battle_Ai_ActionChoiceView {
    char reserved0[0x1B];
    u_char isDead;
    char reserved1C_1C[56];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    Battle_ActorStats* stats;
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved60_6C[22];
    short pointHeight;
    char reserved84[0xBC];
    Battle_Ai_ActionChoiceEntryView currentChoice;
    char reserved154_154[56];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved154_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved154_1A4[136];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    Battle_Ai_ActionSlotConfigView actionSlots[6][4];
};

// decomp: actionDispatchState (src/BATTLE/BATTLE.PRG/71618.c:1085)
struct Battle_Ai_DecisionDispatchView {
    unsigned int opaque0, opaque4, opaque8;
    unsigned short reservedC, reservedE;
    char reserved10[4];
    unsigned char opaque14;
    char reserved15[8];
    unsigned char opaque1D;
    char reserved1E[0x16];
    unsigned char tileX, reserved35, tileZ;
    char reserved37[0x11];
    int savedDestination;
    short position[3];
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved52_6C[22];
    short pointHeight;
    char reserved84[0x34];
    short bodyHeight;
    char reservedBA[0x78];
    signed char opaque132;
    char reserved133[0xD];
    Battle_Ai_DispatchEntryView fallback;
    char reserved150[4];
    signed char opaque154;
    char reserved155[7];
    unsigned char movementTarget;
    char reserved15D_15D[47];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved15D_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved15D_1A4[64];
    unsigned int distance;
    char reserved1E8_1E8[68];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    char reserved1E8_230[512];
    Battle_Ai_DispatchEntryView* candidateOrder[16];
    int candidateCount;
};

// decomp: actionMetadataState (src/BATTLE/BATTLE.PRG/7CD68.c:525)
struct Battle_Ai_ActionMetadataView {
    char reserved0[0x17];
    unsigned char actionCostEnabled;
    char reserved18_18[60];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    Battle_Ai_MetadataStatsView* stats;
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved60_6C[28];
    unsigned char actorId;
    char reserved89_89[259];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved89_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved89_1A4[136];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    Battle_Ai_ActionMetadataEntry actionSlots[6][4];
};

// decomp: actionPlanState (src/BATTLE/BATTLE.PRG/74088.c:1284)
struct Battle_Ai_ActionPlanView {
    char reserved0[0x26];
    u_char allowFallbackAction;
    char reserved27[0x2D];
    Battle_Actor* actor;
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    Battle_ActorStats* stats;
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved60_6C[28];
    u_char actorId;
    char reserved89[0xA5];
    u_char planMode;
    char reserved12F[0x10];
    u_char minActionDelay;
    char reserved140_140[76];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved140_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved140_1A4[136];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    char reserved140_230[192];
    Battle_Ai_CandidateEntryView candidates[16];
    Battle_Ai_CandidateEntryView* candidateOrder[16];
    int candidateCount;
};

// decomp: actionReactionState (src/BATTLE/BATTLE.PRG/74088.c:703)
struct Battle_Ai_ReactionView {
    char reserved0[0x13];
    union { u_char usesRoomActionDelay; u_char temporaryAshleyAlliance; };
    char reserved14[12];
    unsigned char settingsBit26, settingsByte3Bit0;
    char reserved22[5];
    unsigned char settingsBit27;
    char reserved28[4];
    unsigned char settingsBits21;
    char reserved2D[3];
    unsigned char settingsBit25;
    char reserved31_31[35];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved31_6C[28];
    unsigned char actorId;
    union {
        char reserved89[0xA4];
        struct {
            char reserved89Prefix[0x9D];
            u_char allianceReactionCount;
            char reserved127[6];
        };
    };
    unsigned char behaviorMode;
    char reserved12E_12E[94];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved12E_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    Battle_Ai_RelationWordView relations[16];
};

// decomp: actorEvaluationState (src/BATTLE/BATTLE.PRG/74088.c:1538)
struct Battle_Ai_PlanEvaluationView {
    char reserved0[19];
    unsigned char usesRoomActionDelay;
    char reserved14[7];
    unsigned char isDead;
    char reserved1C_1C[56];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved1C_6C[28];
    unsigned char actorId;
    char reserved89[17];
    unsigned short reengagementGraceFrames;
    char reserved9C[75];
    unsigned char opaqueE7;
    unsigned char opaqueE8;
    char reservedE9_E9[163];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reservedE9_190[10];
    unsigned char opaque19A;
};

// decomp: actorInitializationActor (src/BATTLE/BATTLE.PRG/7CD68.c:869)
struct Battle_Ai_AiActorView {
    void* nativePointer0; /* PS1 pointer slot 0x0 */
    int actorId;
    Battle_Ai_ActorStateFlags stateFlags;
    char reservedC[16];
    unsigned short aiControlFlags;
    char reserved1E[1];
    unsigned char startCommandFlag;
    char reserved20[16];
    int savedAiSettings;
    unsigned int savedAiSettingsFlags;
    int aiSettings;
    Battle_Ai_CreationStatsView* stats;
    int listExemptFlag;
    Battle_Ai_CreationModelView* model;
};

// decomp: actorInitializationState (src/BATTLE/BATTLE.PRG/7CD68.c:927)
struct Battle_Ai_AiCreationView {
    char reserved0[42];
    unsigned char hasTaggedBone3;
    char reserved2B[9];
    int tile;
    int previousTile;
    char reserved3C[6];
    short options;
    char reserved44[16];
    Battle_Ai_AiActorView* actor;
    Battle_Ai_CreationModelView* model;
    Battle_Ai_CreationStatsView* stats;
    short* positionSource;
    unsigned int* tileSource;
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved68_6C[22];
    unsigned short pointHeight;
    char reserved84[4];
    unsigned char actorId;
    unsigned char opaque89;
    unsigned char opaque8A;
    char reserved8B[7];
    unsigned short radius;
    unsigned short collisionRadius;
    char reserved96[6];
    int opaque9C;
    char reservedA0[12];
    unsigned int farDistanceSquared;
    char reservedB0[3];
    unsigned char maxRange;
    int moveDistance;
    unsigned short bodyHeight;
    char reservedBA[2];
    short groundClearance;
    short flightHeight;
    char reservedC0[67];
    unsigned char opaque103;
    char reserved104[12];
    int opaque110;
    char reserved114[16];
    unsigned char opaque124;
    char reserved125[8];
    unsigned char behaviorMode;
    char reserved12E[14];
    unsigned short stepHeight;
    char reserved13E[32];
    unsigned short movementFlags;
    char reserved160[16];
    int nextTile;
    char reserved174[16];
    int moveY;
    char reserved188_188[4];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved188_190[4];
    int opaque194;
    char reserved198_198[8];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved198_1A4[128];
    unsigned char opaque224;
    unsigned char opaque225;
    unsigned char opaque226;
    unsigned char opaque227;
};

// decomp: actorRelationState (src/BATTLE/BATTLE.PRG/6FC00.c:638)
struct Battle_Ai_PerceptionView {
    char reserved0[0x22];
    u_char settingsBit23;
    char reserved23[2];
    u_char settingsBit15;
    char reserved26[0xE];
    Battle_Ai_TilePosition tile;
    char reserved38[0x14];
    SVECTOR position;
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Battle_Ai_PerceivedObjectView* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved5C_6C[22];
    short pointHeight;
    char reserved84[4];
    u_char actorId;
    char reserved89[9];
    u_short radius;
    char reserved94[0x10];
    u_int nearDistanceSquared;
    u_int viewCosine;
    u_int farDistanceSquared;
    char reservedB0[8];
    short bodyHeight;
    char reservedBA[0x71];
    u_char phase;
    char reserved12C_12C[96];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved12C_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    Battle_Ai_TargetRelation relations[16];
    u_int distancesSquared[16];
};

// decomp: actorSettingsState (src/BATTLE/BATTLE.PRG/7CD68.c:673)
struct Battle_Ai_SettingsView {
    char reserved0[22];
    unsigned char linkedActor;
    char reserved17[2];
    unsigned char settingsConstant19;
    char reserved1A[6];
    unsigned char settingsBit26;
    unsigned char settingsByte3Bit0;
    unsigned char settingsBit23;
    unsigned char settingsConstant23;
    unsigned char settingsBit13;
    unsigned char settingsBit15;
    unsigned char allowFallbackAction;
    unsigned char settingsBit27;
    unsigned char reserved28;
    unsigned char settingsBit14;
    unsigned char hasTaggedBone3;
    unsigned char settingsModeLow;
    unsigned char settingsBits21;
    unsigned char opaque2D;
    unsigned char reserved2E;
    unsigned char settingsBits28;
    unsigned char settingsBit25;
    unsigned char settingsBits30;
    unsigned char settingsBits30Value;
    unsigned char settingsByte0Bit7;
    char reserved34[32];
    Battle_Ai_SettingsActorView* actor;
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    int (*decisionCallback)(Battle_Ai_ActorAiState*);
    union {
        char reserved6C[40];
        int (*savedDecisionCallback)(Battle_Ai_ActorAiState*);
    };
    unsigned short collisionRadius;
    char reserved96[14];
    unsigned int nearDistanceSquared;
    int viewCosine;
    unsigned int farDistanceSquared;
    unsigned short opaqueB0;
    unsigned char opaqueB2;
    char reservedB3[7];
    unsigned short facingDelay;
    char reservedBC[12];
    unsigned int opaqueC8;
    unsigned char opaqueCC;
    char reservedCD[1];
    unsigned char opaqueCE;
    char reservedCF[5];
    unsigned short restPeriod;
    unsigned short movePeriod;
    unsigned char wanderRate;
    char reservedD9[40];
    unsigned char opaque101;
    unsigned char opaque102;
    char reserved103[37];
    unsigned short angle;
    char reserved12A[52];
    unsigned short movementFlags;
};

// decomp: actorTimerState (src/BATTLE/BATTLE.PRG/73700.c:462)
struct Battle_Ai_EngagementView {
    char reserved0[0x54];
    Battle_Ai_EngagementActorView* actor;
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved58_6C[28];
    unsigned char actorId;
    char reserved89[5];
    unsigned short reserved8E;
    char reserved90[8];
    unsigned short reserved98, reserved9A;
    char reserved9C[0x93];
    signed char phase, direction;
    char reserved131_131[91];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved131_190[16];
    struct Battle_Ai_EngagementView* nextEngaged;
    unsigned char targetFlags;
};

// decomp: actorUpdateState (src/BATTLE/BATTLE.PRG/6FC00.c:958)
struct Battle_Ai_PerceptionUpdateView {
    char reserved0[8];
    u_char opaque8;
    char reserved9[5];
    u_char opaqueE;
    char reservedF[5];
    u_char opaque14;
    char reserved15[3];
    u_char opaque18;
    char reserved19[3];
    u_char opaque1C;
    u_char opaque1D;
    char reserved1E[0x11];
    u_char settingsBits28;
    char reserved30[4];
    u_int tile;
    union {
        int word;
        u_char bytes[4];
    } previousTile;
    u_int opaque3C;
    u_short opaque40;
    char reserved42[3];
    u_char opaque45;
    u_short opaque46;
    char reserved48[4];
    int opaque4C;
    int opaque50;
    Battle_Actor* actor;
    Battle_Ai_PerceivedObjectView* model;
    Battle_Ai_PositionLinkView* stats;
    Battle_Ai_PositionLinkView* positionSource;
    Battle_Ai_PositionLinkView* tileSource;
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved68_6C[4];
    union {
        u_char samples[8];
        int words[2];
    } historyX;
    union {
        u_char samples[8];
        int words[2];
    } historyZ;
    u_char historyIndex;
    u_char historyMask;
    char reserved82[0xE];
    u_short statusFlags;
    char reserved92[0x68];
    u_char detourStep;
    u_char opaqueFB;
    char reservedFC[0x18];
    u_short opaque114;
    char reserved116[0xA];
    u_int opaque120;
    u_char opaque124;
    char reserved125[5];
    u_char awarenessFramesRemaining;
    char reserved12B[6];
    u_char actionStatus;
    char reserved132[6];
    u_short awarenessMarker;
    char reserved13A[0x1C];
    u_short opaque156;
    char reserved158[0x34];
    void* opaque18C;
    u_int reserved190 : 5;
    u_int opaque190 : 7;
    u_int reserved191 : 20;
    char reserved194[6];
    u_char opaque19A;
    char reserved19B_19B[5];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    u_int relations[16];
};

// decomp: candidateMovementState (src/BATTLE/BATTLE.PRG/71618.c:504)
struct Battle_Ai_CandidateMovementView {
    char reserved0[1];
    unsigned char opaque1;
    char reserved2[74];
    short position[3];
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved52_6C[22];
    short pointHeight;
    char reserved84[47];
    unsigned char maxRange;
    char reservedB4[122];
    unsigned char planMode;
    char reserved12F[5];
    Battle_Ai_CandidateTileWord plannedTile;
    char reserved138[36];
    unsigned char movementTarget;
    char reserved15D[1];
    unsigned short movementFlags;
    char reserved160[8];
    Battle_Ai_CandidateTileWord destination;
    char reserved16C_16C[32];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved16C_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    Battle_Ai_CandidateTileWord relations[16];
    unsigned int distancesSquared[16];
    char reserved224_224[8];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    char reserved224_230[512];
    void* nativePointer430; /* PS1 pointer slot 0x430 */
    void* nativePointer434; /* PS1 pointer slot 0x434 */
    void* nativePointer438; /* PS1 pointer slot 0x438 */
    void* nativePointer43C; /* PS1 pointer slot 0x43C */
    void* nativePointer440; /* PS1 pointer slot 0x440 */
    void* nativePointer444; /* PS1 pointer slot 0x444 */
    void* nativePointer448; /* PS1 pointer slot 0x448 */
    void* nativePointer44C; /* PS1 pointer slot 0x44C */
    void* nativePointer450; /* PS1 pointer slot 0x450 */
    void* nativePointer454; /* PS1 pointer slot 0x454 */
    void* nativePointer458; /* PS1 pointer slot 0x458 */
    void* nativePointer45C; /* PS1 pointer slot 0x45C */
    void* nativePointer460; /* PS1 pointer slot 0x460 */
    void* nativePointer464; /* PS1 pointer slot 0x464 */
    void* nativePointer468; /* PS1 pointer slot 0x468 */
    void* nativePointer46C; /* PS1 pointer slot 0x46C */
    int candidateCount;
};

// decomp: da4bcState (src/BATTLE/BATTLE.PRG/71618.c:747)
struct Battle_Ai_EscortView {
    char reserved0[14];
    unsigned char opaqueE, opaqueF;
    char reserved10[6];
    signed char linkedActor;
    char reserved17;
    unsigned char opaque18;
    char reserved19[27];
    unsigned int tile;
    char reserved38[12];
    unsigned char opaque44;
    char reserved45[7];
    short x;
    char reserved4E[2];
    short z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved52_6C[28];
    unsigned char actorId;
    char reserved89[42];
    unsigned char maxRange;
    char reservedB4[53];
    unsigned char opaqueE9;
    char reservedEA[68];
    unsigned char planMode;
    char reserved12F[2];
    unsigned char actionStatus;
    char reserved132[42];
    unsigned char movementTarget;
    char reserved15D_15D[47];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved15D_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    unsigned int relations[16];
    unsigned int distancesSquared[16];
    char reserved224_224[8];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    char reserved224_230[512];
    Battle_Ai_EscortEntryView* candidateOrder[16];
};

// decomp: da4bcTargetState (src/BATTLE/BATTLE.PRG/71618.c:771)
struct Battle_Ai_EscortRelationsView {
    char reserved0_0[84];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved0_6C[288];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved0_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    Battle_Ai_EscortRelationBits relations[16];
};

// decomp: directionalCacheState (src/BATTLE/BATTLE.PRG/771A8.c:896)
struct Battle_Ai_DirectionCacheView {
    char reserved0[0x34];
    Battle_Ai_DirectionCacheTile tile, previousTile;
    char reserved3C_3C[24];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved3C_6C[82];
    short flightHeight;
};

// decomp: func_800D8400_t (src/BATTLE/BATTLE.PRG/6FC00.c:426)
struct Battle_Ai_MoveTargetView {
    unsigned char reserved0[7];
    unsigned char movementDisabled;
    unsigned char reserved8[0x2C];
    Battle_Ai_TilePosition tile;
    char reserved38_38[28];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved38_6C[252];
    Battle_Ai_TilePosition destination;
};

// decomp: func_800DCAA0_t (src/BATTLE/BATTLE.PRG/6F014.c:112)
struct Battle_Ai_ActionCandidate {
    int opaque0;
    u_int row : 3;
    u_int column : 2;
    u_int action : 8;
    u_int threshold : 16;
    u_int reserved7Bit5 : 3;
    int distanceSquared;
    u_int targetId : 4;
    u_int targetLimb : 3;
    u_int opaqueCBit7 : 12;
    u_int weight : 8;
    u_int reservedFBit3 : 5;
    Battle_Ai_CandidateFlags opaque10;
};
static_assert(sizeof(Battle_Ai_ActionCandidate) == 0x14, "Battle_Ai_ActionCandidate must keep its PS1 layout");

// layout: ledger (upstream merge) for func_800DD604_t
struct Battle_Ai_ActionSlotMetadata {
    /* 0x00 */ u_int reserved0Bit0 : 1;
    /* 0x00 */ u_int opaque0Bit1 : 1;
    /* 0x00 */ u_int opaque0Bit2 : 1;
    /* 0x00 */ u_int kind : 2;
    /* 0x00 */ u_int longRange : 1;
    /* 0x00 */ u_int special : 1;
    /* 0x00 */ u_int opaque0Bit7 : 1;
    /* 0x01 */ u_int radius : 16;
    /* 0x03 */ u_int reserved3Bit0 : 8;
    /* 0x04 */ u_int rangeSquared;
};
static_assert(sizeof(Battle_Ai_ActionSlotMetadata) == 0x8, "Battle_Ai_ActionSlotMetadata must keep its PS1 layout");

// decomp: func_800DEEA4_t2 (src/BATTLE/BATTLE.PRG/6F014.c:437)
struct Battle_Ai_ActionExecutionView {
    char reserved0[0x16];
    signed char linkedActor;
    char reserved17[0x21];
    int previousTile;
    char reserved3C[0x18];
    Battle_Ai_EngagementData* actor;
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved58_6C[46];
    short reserved9A;
    char reserved9C[0x5F];
    u_char reservedFB;
    char reservedFC[0x2E];
    u_char reserved12A;
    char reserved12B[0xD];
    u_short reserved138;
    char reserved13A_13A[82];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved13A_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved13A_1A4[136];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    char reserved13A_230[512];
    Battle_Ai_ActionCandidate* candidateOrder[16];
    int candidateCount;
};

// decomp: func_800E0850_t (src/BATTLE/BATTLE.PRG/6F014.c:412)
struct Battle_Ai_FloorHeightView {
    char reserved0[7];
    u_char movementDisabled;
    char reserved8[0x2C];
    u_char reserved34;
    char reserved35;
    u_char reserved36;
    char reserved37[0x17];
    short reserved4E;
    char reserved50_50[4];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Battle_Ai_ModelStatePrefix* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved5C_6C[82];
    short flightHeight;
    char reservedC0[0xBC];
    int moveX;
    int moveZ;
    int moveY;
    short heading;
    char reserved18A_18A[2];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved18A_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved18A_1A4[132];
    int stallCounter;
};

// layout: ledger (upstream merge) for func_800E78F4_t
struct Battle_Ai_ActorAiState {
    /* 0x00 */ char reserved0;
    /* 0x01 */ u_char opaque1;
    /* 0x02 */ u_char isStuck;
    /* 0x03 */ u_char opaque3;
    /* 0x04 */ u_char opaque4;
    /* 0x05 */ u_char opaque5;
    /* 0x06 */ union { char reserved6; u_char ignore_floor_mask; };
    /* 0x07 */ u_char movementDisabled;
    /* 0x08 */ u_char opaque8;
    /* 0x09 */ u_char opaque9;
    /* 0x0A */ char reservedA[3];
    /* 0x0D */ u_char opaqueD;
    /* 0x0E */ u_char opaqueE;
    /* 0x0F */ u_char opaqueF;
    /* 0x10 */ union {
        char reserved10[3];
        struct {
            u_char resourceStatusRestricted;
            u_char lowMp;
            u_char lowHp;
        };
    };
    /* 0x13 */ u_char usesRoomActionDelay;
    /* 0x14 */ char reserved14[2];
    /* 0x16 */ signed char linkedActor;
    /* 0x17 */ u_char actionCostEnabled;
    /* 0x18 */ u_char opaque18;
    /* 0x19 */ char settingsConstant19;
    /* 0x1A */ u_char opaque1A;
    /* 0x1B */ u_char isDead;
    /* 0x1C */ u_char opaque1C;
    /* 0x1D */ char reserved1D[3];
    /* 0x20 */ u_char settingsBit26;
    /* 0x21 */ u_char settingsByte3Bit0;
    /* 0x22 */ union {
        char reserved22[4];
        struct {
            u_char reserved22Prefix;
            signed char movementEnabled;
            u_char reserved24[2];
        };
    };
    /* 0x26 */ u_char allowFallbackAction;
    /* 0x27 */ u_char settingsBit27;
    /* 0x28 */ char reserved28;
    /* 0x29 */ u_char settingsBit14;
    /* 0x2A */ char hasTaggedBone3;
    /* 0x2B */ u_char settingsModeLow;
    /* 0x2C */ u_char settingsBits21;
    /* 0x2D */ char reserved2D[3];
    /* 0x30 */ u_char settingsBit25;
    /* 0x31 */ union {
        char reserved31[3];
        struct {
            u_char reserved31Prefix;
            u_char terrain_reject_mask;
            u_char reserved33;
        };
    };
    /* 0x34 */ Battle_Ai_TilePosition tile;
    /* 0x38 */ int previousTile;
    /* 0x3C */ union {
        char reserved3C[8];
        struct {
            u_int rememberedTargetTile;
            u_short targetMemoryFrames;
            short savedRelationOptions;
        };
    };
    /* 0x44 */ u_char opaque44;
    /* 0x45 */ char reserved45[3];
    /* 0x48 */ Battle_Ai_TilePosition savedDestination;
    /* 0x4C */ SVECTOR position;
    /* 0x54 */ Battle_Ai_EngagementData *actor;
    /* 0x58 */ Battle_Ai_ModelStatePrefix *model;
    /* 0x5C */ Battle_ActorStats *stats;
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved60_6C[22];
    /* 0x82 */ short pointHeight;
    /* 0x84 */ char reserved84[4];
    /* 0x88 */ u_char actorId;
    /* 0x89 */ union {
        char reserved89[3];
        struct {
            u_char requestedMovementMode;
            u_char currentMovementMode;
            u_char reserved8B;
        };
    };
    /* 0x8C */ u_short moveAttempts;
    /* 0x8E */ u_short engagementFrames;
    /* 0x90 */ union {
        char reserved90[2];
        u_short actionDelay;
    };
    /* 0x92 */ u_short radius;
    /* 0x94 */ union {
        char reserved94[4];
        struct {
            u_short collision_radius;
            u_char reserved96[2];
        };
    };
    /* 0x98 */ u_short engagementCooldownFrames;
    /* 0x9A */ u_short reengagementGraceFrames;
    /* 0x9C */ union {
        char reserved9C[23];
        struct {
            u_char last_slide_tile_bytes[4];
            u_char reservedA0[16];
            u_char step_height_bytes[2];
            u_char reservedB2;
        };
    };
    /* 0xB3 */ u_char maxRange;
    /* 0xB4 */ int moveDistance;
    /* 0xB8 */ short bodyHeight;
    /* 0xBA */ u_short facingDelay;
    /* 0xBC */ char reservedBC[2];
    /* 0xBE */ short flightHeight;
    /* 0xC0 */ u_short wanderPhase;
    /* 0xC2 */ u_short wanderTimer;
    /* 0xC4 */ int wanderRangeSquared;
    /* 0xC8 */ union {
        char reservedC8[8];
        struct {
            int wander_center_radius;
            Battle_Ai_TilePosition wander_center;
        };
    };
    /* 0xD0 */ short wanderHeading;
    /* 0xD2 */ u_short wanderCooldown;
    /* 0xD4 */ u_short restPeriod;
    /* 0xD6 */ u_short movePeriod;
    /* 0xD8 */ u_char wanderRate;
    /* 0xD9 */ char reservedD9[14];
    /* 0xE7 */ u_char opaqueE7;
    /* 0xE8 */ u_char opaqueE8;
    /* 0xE9 */ u_char opaqueE9;
    /* 0xEA */ char reservedEA[16];
    /* 0xFA */ u_char detourStep;
    /* 0xFB */ u_char opaqueFB;
    /* 0xFC */ union {
        char reservedFC[12];
        struct {
            u_char reservedFCPrefix[4];
            u_char patrolIndex;
            u_char patrolRow;
            u_char patrolRate;
            signed char patrolDirection;
            u_char patrolCooldown;
            u_char reserved105[3];
        };
    };
    /* 0x108 */ Battle_Ai_TilePosition previousFacingTile;
    /* 0x10C */ u_char facingPending;
    /* 0x10D */ char reserved10D[3];
    Battle_Ai_TilePosition flee_tile;
    u_short flee_timer;
    char reserved116[2];
    /* 0x118 */ u_char detourDirections[4];
    /* 0x11C */ Battle_Ai_TilePosition goal;
    /* 0x120 */ union {
        char reserved120[10];
        struct {
            u_char fine_x;
            u_char fine_layer;
            u_char fine_z;
            u_char fine_status;
            u_char reserved124;
            u_char locomotionMode;
            u_char reserved126[4];
        };
    };
    /* 0x12A */ u_char awarenessFramesRemaining;
    /* 0x12B */ union {
        char reserved12B[2];
        struct {
            u_char reserved12BPrefix;
            u_char engagementRequest;
        };
    };
    /* 0x12D */ u_char behaviorMode;
    /* 0x12E */ u_char planMode;
    /* 0x12F */ signed char opaque12F;
    /* 0x130 */ signed char opaque130;
    /* 0x131 */ u_char actionStatus;
    /* 0x132 */ u_char ground_area;
    /* 0x133 */ u_char reserved133;
    /* 0x134 */ Battle_Ai_TilePosition plannedTile;
    /* 0x138 */ u_short awarenessMarker;
    /* 0x13A */ u_short opaque13A;
    /* 0x13C */ u_short opaque13C;
    /* 0x13E */ char reserved13E;
    /* 0x13F */ u_char minActionDelay;
    /* 0x140 */ Battle_Ai_ActionCandidate currentChoice;
    /* 0x154 */ union {
        char reserved154[4];
        struct {
            u_char reserved154Area;
            u_char decisionDelay;
            union { u_char reserved156[2]; u_short movement_wait; };
        };
    };
    /* 0x158 */ short targetX;
    /* 0x15A */ short targetZ;
    /* 0x15C */ u_char movementTarget;
    /* 0x15D */ u_char opaque15D;
    /* 0x15E */ u_short movementFlags;
    /* 0x160 */ union { char reserved160[8]; u_int scriptedTargetXZ; };
    /* 0x168 */ Battle_Ai_TilePosition destination;
    /* 0x16C */ char reserved16C[4];
    /* 0x170 */ Battle_Ai_TilePosition nextTile;
    /* 0x174 */ union {
        char reserved174[8];
        SVECTOR nextPosition;
    };
    /* 0x17C */ int moveX;
    /* 0x180 */ int moveZ;
    /* 0x184 */ int moveY;
    /* 0x188 */ short heading;
    /* 0x18A */ short angle;
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    u_int navigationFlags;
    union { char reserved194[6]; struct {
        u_char detour_tile_bytes[4];
        u_char detour_repeats;
        u_char detour_run;
    }; };
    /* 0x19A */ u_char opaque19A;
    /* 0x19B */ union { char reserved19B[5]; struct {
        u_char route_gate;
        u_char route_exit_bytes[4];
    }; };
    /* 0x1A0 */ struct Battle_Ai_ActorAiState *nextEngaged;
    /* 0x1A4 */ Battle_Ai_RelationWord relations[16];
    /* 0x1E4 */ u_int distancesSquared[16];
    /* 0x224 */ union {
        char reserved224[4];
        struct {
            u_short locomotionFlags;
            u_char walkStep;
            u_char runStep;
        };
    };
    /* 0x228 */ int stallCounter;
    /* 0x22C */ Battle_Ai_ActionCandidate *currentCandidate;
    /* 0x230 */ Battle_Ai_ActionSlotMetadata actionSlots[6][4];
    /* 0x2F0 */ Battle_Ai_ActionCandidate candidates[16];
    /* 0x430 */ Battle_Ai_ActionCandidate *candidateOrder[16];
    /* 0x470 */ int candidateCount;
    /* 0x474 */ union {
        char reserved474[13];
        u_char movementModeCounters[13];
    };
    /* 0x481 */ u_char reactionPending;
};

// decomp: movementCheckState (src/BATTLE/BATTLE.PRG/771A8.c:979)
struct Battle_Ai_MovementCheckView {
    char reserved0[0x34];
    int tile;
    char reserved38_38[28];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Battle_Ai_CheckObjectView* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved5C_6C[82];
    short flightHeight;
    char reservedC0[0x9E];
    unsigned short movementFlags;
    char reserved160[8];
    int destination;
};

// decomp: movementDecisionContext (src/BATTLE/BATTLE.PRG/771A8.c:2182)
struct Battle_Ai_MovementDecisionView {
    char reserved0[0x4C];
    short x;
    short padding4E;
    short z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Actors_ActorRecord* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char padding5C_6C[28];
    u_char actorId;
    char reserved89[0x31];
    u_short facingDelay;
    char paddingBC[0x4C];
    int previousFacingTile;
    u_char facingPending;
    char padding10D[0x2D];
    u_short opaque13A;
    char padding13C[0x2C];
    int destination;
    char padding16C[0x10];
    int moveX;
    int moveZ;
    char reserved184[4];
    u_int reserved188 : 16;
    int angle : 16;
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char padding18C_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    u_int targetFlags[17];
    char padding1E8_1E8[68];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    char padding1E8_230[512];
    void* nativePointer430; /* PS1 pointer slot 0x430 */
    void* nativePointer434; /* PS1 pointer slot 0x434 */
    void* nativePointer438; /* PS1 pointer slot 0x438 */
    void* nativePointer43C; /* PS1 pointer slot 0x43C */
    void* nativePointer440; /* PS1 pointer slot 0x440 */
    void* nativePointer444; /* PS1 pointer slot 0x444 */
    void* nativePointer448; /* PS1 pointer slot 0x448 */
    void* nativePointer44C; /* PS1 pointer slot 0x44C */
    void* nativePointer450; /* PS1 pointer slot 0x450 */
    void* nativePointer454; /* PS1 pointer slot 0x454 */
    void* nativePointer458; /* PS1 pointer slot 0x458 */
    void* nativePointer45C; /* PS1 pointer slot 0x45C */
    void* nativePointer460; /* PS1 pointer slot 0x460 */
    void* nativePointer464; /* PS1 pointer slot 0x464 */
    void* nativePointer468; /* PS1 pointer slot 0x468 */
    void* nativePointer46C; /* PS1 pointer slot 0x46C */
    char padding1E8_470[17];
    u_char reactionPending;
};

// decomp: movementDestinationState (src/BATTLE/BATTLE.PRG/771A8.c:1385)
struct Battle_Ai_MovementDestinationView {
    char reserved0[0xD];
    unsigned char opaqueD;
    char reservedE[0x26];
    Battle_Ai_TilePosition tile;
    char reserved38[0x10];
    Battle_Ai_TilePosition savedDestination;
    short x;
    char reserved4E[2];
    short z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved52_6C[32];
    unsigned short moveAttempts;
    char reserved8E[0x26];
    int moveDistance;
    char reservedB8[0xA6];
    unsigned short movementFlags;
    char reserved160[8];
    Battle_Ai_TilePosition destination;
    char reserved16C[4];
    Battle_Ai_TilePosition nextTile;
    char reserved174[8];
    int moveX, moveZ;
};

// decomp: movementPathState (src/BATTLE/BATTLE.PRG/771A8.c:2381)
struct Battle_Ai_MovementPathView {
    char reserved0;
    u_char opaque1;
    char reserved2[2];
    u_char opaque4;
    u_char opaque5;
    char reserved6[0x2E];
    Battle_Ai_TilePosition tile;
    char reserved38[0x14];
    short x;
    char reserved4E[2];
    short z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved52_6C[120];
    u_char opaqueE4;
    char reservedE5[2];
    u_char opaqueE7;
    u_char opaqueE8;
    char reservedE9[0x75];
    u_short movementFlags;
    char reserved160[8];
    Battle_Ai_TilePosition destination;
    char reserved16C[4];
    Battle_Ai_TilePosition nextTile;
    char reserved174_174[24];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    Battle_Ai_DetourWord path;
    char reserved194[5];
    u_char opaque199;
    u_char waitTimer;
};

// decomp: movementRecoveryState (src/BATTLE/BATTLE.PRG/771A8.c:2243)
struct Battle_Ai_MovementRecoveryView {
    char reserved0[9];
    unsigned char opaque9;
    char reservedA[0x2A];
    Battle_Ai_TilePosition tile;
    char reserved38[0x14];
    short x;
    char reserved4E[2];
    short z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Battle_Ai_RecoveryModelView* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved5C_6C[206];
    unsigned short opaque13A, duration;
    char reserved13E[0x1A];
    short targetX, targetZ;
    unsigned char movementTarget;
    char reserved15D;
    unsigned short movementFlags;
    char reserved160[0x28];
    unsigned short heading, angle;
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved18C_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved18C_1A4[132];
    int stallCounter;
};
struct Battle_Ai_MovementRecoveryView;

// decomp: movementStepState (src/BATTLE/BATTLE.PRG/771A8.c:1051)
struct Battle_Ai_MovementStepView {
    unsigned char opaque0;
    char reserved1[6];
    unsigned char movementDisabled;
    char reserved8[0x2C];
    Battle_Ai_DirectionCacheTile tile;
    char reserved38[0x14];
    Battle_Ai_StepPositionWord position;
    short z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Battle_Ai_CheckObjectView* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved5C_6C[80];
    short groundClearance;
    short flightHeight;
    char reservedC0[0x25];
    unsigned char actorId;
    char reservedE6[0x82];
    Battle_Ai_DirectionCacheTile destination;
    char reserved16C[0x10];
    int moveX;
    int moveZ;
    int moveY;
    short heading;
    char reserved18A_18A[2];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved18A_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved18A_1A4[132];
    int stallCounter;
};

// decomp: nearestWaypointState (src/BATTLE/BATTLE.PRG/74088.c:941)
struct Battle_Ai_WaypointSearchView {
    char reserved0[0x4C];
    short x, y, z;
};

// decomp: obstructionState (src/BATTLE/BATTLE.PRG/771A8.c:790)
struct Battle_Ai_ObstructionView {
    char reserved0[0x4C];
    Battle_Ai_ObstructionVector position;
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Battle_Ai_ObstructionModel* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved5C_6C[28];
    unsigned char actorId;
    char reserved89[9];
    unsigned short radius;
    char reserved94[0x1F];
    unsigned char maxRange;
    char reservedB4[4];
    short height;
};

// decomp: patrolWaypointState (src/BATTLE/BATTLE.PRG/771A8.c:1854)
struct Battle_Ai_PatrolView {
    char reserved0[0x34];
    Battle_Ai_TilePosition tile;
    char reserved38_38[28];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved38_6C[148];
    unsigned char index, row, rate, direction, opaque104;
    char reserved105[0x63];
    Battle_Ai_TilePosition destination;
};

// decomp: radialMotionState (src/BATTLE/BATTLE.PRG/7EE98.c:480)
struct Battle_Ai_RadialRetreatView {
    char reserved0[0x4C];
    short x, y, z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Battle_Ai_ModelActiveView* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved5C_6C[272];
    int moveX, moveZ, moveY;
    char reserved188_188[4];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved188_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    char reserved188_1A4[136];
    void* nativePointer22C; /* PS1 pointer slot 0x22C */
    char reserved188_230[512];
    void* nativePointer430; /* PS1 pointer slot 0x430 */
    void* nativePointer434; /* PS1 pointer slot 0x434 */
    void* nativePointer438; /* PS1 pointer slot 0x438 */
    void* nativePointer43C; /* PS1 pointer slot 0x43C */
    void* nativePointer440; /* PS1 pointer slot 0x440 */
    void* nativePointer444; /* PS1 pointer slot 0x444 */
    void* nativePointer448; /* PS1 pointer slot 0x448 */
    void* nativePointer44C; /* PS1 pointer slot 0x44C */
    void* nativePointer450; /* PS1 pointer slot 0x450 */
    void* nativePointer454; /* PS1 pointer slot 0x454 */
    void* nativePointer458; /* PS1 pointer slot 0x458 */
    void* nativePointer45C; /* PS1 pointer slot 0x45C */
    void* nativePointer460; /* PS1 pointer slot 0x460 */
    void* nativePointer464; /* PS1 pointer slot 0x464 */
    void* nativePointer468; /* PS1 pointer slot 0x468 */
    void* nativePointer46C; /* PS1 pointer slot 0x46C */
    int candidateCount;
};
struct Battle_Ai_RadialRetreatView;

// decomp: routeEdgeState (src/BATTLE/BATTLE.PRG/771A8.c:554)
struct Battle_Ai_AreaRouteView {
    char reserved0[5];
    u_char opaque5;
    char reserved6[0x2E];
    Battle_Ai_TilePosition tile;
    char reserved38[0x14];
    short x;
    short y;
    short z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved52_6C[28];
    u_char actorId;
    char reserved89[0xB1];
    u_short cooldown;
    u_short period;
    char reserved13E[0x16];
    u_char area;
    char reserved155[7];
    u_char movementTarget;
    char reserved15D[0xB];
    Battle_Ai_TilePosition destination;
    char reserved16C[4];
    Battle_Ai_TilePosition nextTile;
    char reserved174[8];
    int moveX;
    int moveZ;
    char reserved184[4];
    short heading;
    short angle;
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved18C_190[11];
    u_char gate;
    Battle_Ai_RouteGateTile exit;
};

// decomp: stuckMovementState (src/BATTLE/BATTLE.PRG/71618.c:1210)
struct Battle_Ai_BlockedRecoveryView {
    char reserved0[2];
    u_char isStuck;
    char reserved3[0x19];
    u_char opaque1C;
    char reserved1D[0x17];
    Battle_Ai_BlockedTileWord tile;
    Battle_Ai_BlockedTileWord previousTile;
    char reserved3C[0x10];
    short x;
    char reserved4E[2];
    short z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved52_6C[142];
    u_char detourStep;
    u_char opaqueFB;
    char reservedFC[0x1C];
    u_char detourDirections[4];
    char reserved11C[0x40];
    u_char movementTarget;
    char reserved15D[0xB];
    Battle_Ai_BlockedTileWord destination;
};

// decomp: targetMovementState (src/BATTLE/BATTLE.PRG/771A8.c:1967)
struct Battle_Ai_TargetApproachView {
    unsigned char opaque0;
    unsigned char opaque1;
    char reserved2[1];
    unsigned char opaque3;
    unsigned char opaque4;
    char reserved5[4];
    unsigned char opaque9;
    char reservedA[2];
    unsigned char opaqueC;
    unsigned char opaqueD;
    char reservedE[22];
    unsigned char settingsBit13;
    char reserved25[14];
    unsigned char settingsByte0Bit7;
    Battle_Ai_TilePosition tile;
    char reserved38[16];
    Battle_Ai_ApproachTileWord savedDestination;
    short x;
    char reserved4E[2];
    short z;
    char reserved52[2];
    Battle_Ai_ApproachActorView* actor;
    Battle_Ai_ApproachTargetRecordView* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved5C_6C[28];
    unsigned char actorId;
    char reserved89[83];
    int range;
    int previousRange;
    unsigned char opaqueE4;
    unsigned char targetId;
    unsigned char timeout;
    unsigned char opaqueE7;
    unsigned char opaqueE8;
    unsigned char rangeMode;
    unsigned char previousOption;
    unsigned char previousTargetId;
    Battle_Ai_ApproachTileWord cachedDestination;
    char reservedF0[4];
    unsigned char targetTileX;
    char reservedF5[1];
    unsigned char targetTileZ;
    char reservedF7[1];
    unsigned short opaqueF8;
    char reservedFA[94];
    short targetX;
    short targetZ;
    unsigned char movementTarget;
    char reserved15D[1];
    unsigned short movementFlags;
    char reserved160[8];
    Battle_Ai_ApproachTileWord destination;
    char reserved16C[16];
    int moveX;
    int moveZ;
    char reserved184_184[8];
    void* nativePointer18C; /* PS1 pointer slot 0x18C */
    char reserved184_190[16];
    void* nativePointer1A0; /* PS1 pointer slot 0x1A0 */
    Battle_Ai_ApproachRelationBits relations[16];
    unsigned int distancesSquared[16];
};

// decomp: terrainMovementState (src/BATTLE/BATTLE.PRG/771A8.c:1508)
struct Battle_Ai_TerrainMoveView {
    char reserved0[2];
    unsigned char isStuck;
    char reserved3[1];
    unsigned char opaque4;
    unsigned char opaque5;
    char reserved6[20];
    unsigned char opaque1A;
    char reserved1B[14];
    unsigned char settingsBit14;
    char reserved2A[10];
    Battle_Ai_TerrainTileWord tile;
    char reserved38[20];
    short position[3];
    char reserved52[2];
    Battle_Ai_TerrainMoveActorView* actor;
    void* nativePointer58; /* PS1 pointer slot 0x58 */
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved58_6C[252];
    Battle_Ai_TerrainTileWord destination;
    char reserved16C[16];
    int moveX;
    int moveZ;
    char reserved184[4];
    short angle;
};

// decomp: trackingMotionState (src/BATTLE/BATTLE.PRG/7EE98.c:571)
struct Battle_Ai_OrbitTrackingView {
    char reserved0[0x4C];
    short x, y, z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Battle_Ai_ModelActiveView* model;
    Battle_Ai_OrbitStatsView* stats;
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved60_6C[240];
    unsigned char movementTarget;
    char reserved15D[0x1F];
    int moveX, moveZ, moveY;
};
struct Battle_Ai_OrbitTrackingView;

// decomp: wanderState (src/BATTLE/BATTLE.PRG/771A8.c:1625)
struct Battle_Ai_WanderView {
    char reserved0[2];
    unsigned char isStuck;
    unsigned char opaque3;
    char reserved4[4];
    unsigned char opaque8;
    char reserved9[0x2B];
    Battle_Ai_TilePosition tile;
    char reserved38[0x10];
    Battle_Ai_TilePosition savedDestination;
    short x, y, z;
    char reserved52_52[2];
    void* nativePointer54; /* PS1 pointer slot 0x54 */
    Battle_Ai_WanderRecordView* model;
    void* nativePointer5C; /* PS1 pointer slot 0x5C */
    void* nativePointer60; /* PS1 pointer slot 0x60 */
    void* nativePointer64; /* PS1 pointer slot 0x64 */
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved5C_6C[72];
    unsigned int moveDistance;
    char reservedB8[8];
    unsigned short wanderPhase;
    unsigned short wanderTimer;
    int wanderRangeSquared;
    char reservedC8[8];
    short wanderHeading;
    unsigned short wanderCooldown;
    unsigned short restPeriod;
    unsigned short movePeriod;
    unsigned char wanderRate;
    char reservedD9[0x43];
    Battle_Ai_TilePosition goal;
    char reserved120[13];
    unsigned char behaviorMode;
    char reserved12E[0x2F];
    unsigned char count;
    unsigned short movementFlags;
    char reserved160[8];
    Battle_Ai_TilePosition destination;
};

#endif // VS_BATTLE_AI_AISTATE_H_TYPES
