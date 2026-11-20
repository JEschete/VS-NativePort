// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Actors/Spawning.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Legacy.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/Seq/Sequencer.hpp"
#include "Audio/SurfaceSounds.hpp"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/StatusEffects.h"
#include "Battle/TargetingState.h"
#include "Battle/TextBoxes.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Math/FixedPoint.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Plugins/Plg045.hpp"
#include "Effects/Plugins/Plg049.hpp"
#include "Effects/Plugins/Plg050.hpp"
#include "Effects/Plugins/Plg053.hpp"
#include "Effects/Plugins/Plg057.hpp"
#include "Effects/Plugins/Plg061.hpp"
#include "Effects/Plugins/Plg065.hpp"
#include "Effects/Plugins/Plg069.hpp"
#include "Effects/Plugins/Plg073.hpp"
#include "Effects/Plugins/Plg077.hpp"
#include "Effects/Plugins/Plg081.hpp"
#include "Effects/Plugins/Plg082.hpp"
#include "Effects/Plugins/Plg085.hpp"
#include "Effects/Plugins/Plg086.hpp"
#include "Effects/Plugins/Plg087.hpp"
#include "Effects/Plugins/Plg114.hpp"
#include "Effects/Plugins/Plg128.hpp"
#include "Effects/Plugins/Plg129.hpp"
#include "Effects/Plugins/Plg134.hpp"
#include "Effects/Plugins/Plg135.hpp"
#include "Effects/Plugins/Plg137.hpp"
#include "Effects/Plugins/Plg138.hpp"
#include "Effects/Plugins/Plg141.hpp"
#include "Effects/Plugins/Plg142.hpp"
#include "Effects/Plugins/Plg143.hpp"
#include "Effects/Plugins/Plg146.hpp"
#include "Effects/Plugins/Plg147.hpp"
#include "Effects/Plugins/Plg148.hpp"
#include "Effects/Plugins/Plg149.hpp"
#include "Effects/Plugins/Plg150.hpp"
#include "Effects/Plugins/Plg153.hpp"
#include "Effects/Plugins/Plg154.hpp"
#include "Effects/Plugins/Plg156.hpp"
#include "Effects/Plugins/Plg158.hpp"
#include "Effects/Plugins/Plg160.hpp"
#include "Effects/Plugins/Plg161.hpp"
#include "Effects/Plugins/Plg162.hpp"
#include "Effects/Plugins/Plg165.hpp"
#include "Effects/Plugins/Plg166.hpp"
#include "Effects/Plugins/Plg167.hpp"
#include "Effects/Plugins/Plg168.hpp"
#include "Effects/Plugins/Plg169.hpp"
#include "Effects/Plugins/Plg170.hpp"
#include "Effects/Plugins/Plg224.hpp"
#include "Effects/Plugins/Plg232.hpp"
#include "Effects/Plugins/Plg236.hpp"
#include "Effects/Plugins/Plg242.hpp"
#include "Effects/Plugins/Plg250.hpp"
#include "Effects/Plugins/Plg255.hpp"
#include "Effects/Plugins/Plg260.hpp"
#include "Effects/Plugins/Plg263.hpp"
#include "Effects/Plugins/Plg265.hpp"
#include "Effects/Plugins/Plg266.hpp"
#include "Effects/Plugins/Plg272.hpp"
#include "Effects/Plugins/Plg273.hpp"
#include "Effects/Plugins/Plg291.hpp"
#include "Effects/Plugins/Plg297.hpp"
#include "Effects/Plugins/Plg300.hpp"
#include "Effects/Plugins/Plg301.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/BoneTrailLiveState.hpp"
#include "Ending/ClearResults.hpp"
#include "Ending/Credits.hpp"
#include "Ending/Credits/StaffRoll.hpp"
#include "Ending/Results.hpp"
#include "Event/Actors.h"
#include "Event/Script.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptVm.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Locations.h"
#include "Field/MapProgress.h"
#include "Field/TriggerMarkers.h"
#include "Items/Categories.hpp"
#include "Items/Materials.hpp"
#include "Legacy/Psx.h"
#include "LegacyFiles.h"
#include "Menu/BattleAbilitiesLiveState.hpp"
#include "Menu/BreakArtsLive.hpp"
#include "Menu/Container/ItemView.hpp"
#include "Menu/FilesLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/Magic.hpp"
#include "Menu/Manual.hpp"
#include "Menu/Map.hpp"
#include "Menu/OptionsLiveState.hpp"
#include "Menu/RenameLiveState.hpp"
#include "Menu/Score/Records.hpp"
#include "Menu/Score/ScoreScreen.hpp"
#include "Menu/Status/ActorStatus.hpp"
#include "Menu/TextBox.hpp"
#include "ModelLoading.h"
#include "Render/FramebufferEffects.hpp"
#include "Render/Images.hpp"
#include "Render/Loading.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/Text/VariableFont.hpp"
#include "Render/UiPrimitives.hpp"
#include "Title/Credits.hpp"
#include "Title/SaveFiles.hpp"
#include "Title/TitleScreen.hpp"

// Globals whose contents are in the module images (DATA_LEDGER segment data/rodata/asset, no pointers).
#include "Content/ModuleData.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Effects/TrailEmitterLive.hpp"

// decomp: EFFECT/PLG074.BIN PLG074 trailEffect.h origin cursor; original image matches PLG073 but mutable state remains overlay-local
int VS::Content::plg074TrailEmissionCounter[1];
// decomp: BATTLE/BATTLE.PRG 4B16C.s:1208 eighteen packed signed16 X/Z vertices; imported source data only
unsigned int Content_battleShadowVertices[18];
// decomp: BATTLE/BATTLE.PRG 4B16C.s:1208 twenty-four triangles with byte offsets into projected shadow vertices
unsigned int Content_battleShadowFaces[24];
// decomp: BATTLE/BATTLE.PRG 7B5DC.s:func_800E4180 indexed hesitation lengths; native module-owned data
unsigned int VS::Content::battleAiHesitationLengths[1];
// decomp: BATTLE/BATTLE.PRG 5BF94.c:2934 D_800EBA78 main menu help text; bytes 316..319 open the gradient window
unsigned int Content_battleMainMenuHelpText[80];
// decomp: BATTLE/BATTLE.PRG 58578.c INCLUDE_RODATA D_80069860; strings end at jtbl_800698F8
unsigned int Content_battleLimbNameBytes[38];
// decomp: BATTLE/BATTLE.PRG 30DB0.c:_loadGimmick; three 16-byte headers plus modelDataSize payloads
unsigned int Content_battleGimmickTemplateBytes[455];
// decomp: BATTLE/BATTLE.PRG 5BF94.c:textbox frame vertices; nine styles precede D_800EB588
unsigned int Content_battleTextboxFrameBytes[631];
// decomp: BATTLE/BATTLE.PRG 146C.c:func_8008E19C complete one-group/one-quad marker definition; G00039-G00041 are interior aliases
unsigned int Content_battleTriggerMarkerBytes[29];
// decomp: BATTLE/BATTLE.PRG 146C.c:17537 D_80068C1C sky dome: 34 twelve-byte vertices (colour word 2) and 64 four-byte faces; SetRoomBackgroundColors patches the vertex colours
unsigned int Content_battleSkyDome[166];
// decomp: SLUS_010.40 25AC.c:LFO selectors include source-code bytes as sample/control data; original PS-X EXE payload, never host code
short Content_residentLfoSamples[167936];

const VS::Content::ModuleDataEntry VS::Content::kModuleData[] = {
    {"BATTLE/BATTLE.PRG", 0x13BC, sizeof Battle_Ai_kTimedStatusFlags,
     &Battle_Ai_kTimedStatusFlags,
     sizeof Battle_Ai_kTimedStatusFlags},  // G00013 D_80069BBC
    {"BATTLE/BATTLE.PRG", 0x1418, sizeof Battle_Ai_kSettingsBits30Values,
     &Battle_Ai_kSettingsBits30Values,
     sizeof Battle_Ai_kSettingsBits30Values},  // G00014 D_80069C18
    {"BATTLE/BATTLE.PRG", 0x141C, sizeof Battle_Ai_kPerceptionRangesSquared,
     &Battle_Ai_kPerceptionRangesSquared,
     sizeof Battle_Ai_kPerceptionRangesSquared},  // G00015 D_80069C1C
    {"BATTLE/BATTLE.PRG", 0x143C, sizeof Battle_Ai_kWanderTimingPresets,
     &Battle_Ai_kWanderTimingPresets,
     sizeof Battle_Ai_kWanderTimingPresets},  // G00016 D_80069C3C
    {"BATTLE/BATTLE.PRG", 0x2C7AC, sizeof VS::Audio::legacyUnreferencedTable,
     &VS::Audio::legacyUnreferencedTable,
     sizeof VS::Audio::legacyUnreferencedTable},  // G00017 D_80094FAC
    {"BATTLE/BATTLE.PRG", 0x2C7FC, sizeof VS::Audio::legacyUnreferencedWord,
     &VS::Audio::legacyUnreferencedWord,
     sizeof VS::Audio::legacyUnreferencedWord},  // G00018 D_80094FFC
    {"BATTLE/BATTLE.PRG", 0x7F984, sizeof Battle_categoryNeedsTargetSelection,
     &Battle_categoryNeedsTargetSelection,
     sizeof Battle_categoryNeedsTargetSelection},  // G00019 D_800E8184
    {"BATTLE/BATTLE.PRG", 0x7F994, sizeof Battle_categoryIsAreaTarget,
     &Battle_categoryIsAreaTarget,
     sizeof Battle_categoryIsAreaTarget},  // G00020 D_800E8194
    {"BATTLE/BATTLE.PRG", 0x7F9B4, sizeof Battle_categoryIncludesActingActor,
     &Battle_categoryIncludesActingActor,
     sizeof Battle_categoryIncludesActingActor},  // G00021 D_800E81B4
    {"BATTLE/BATTLE.PRG", 0x7F9C4, sizeof Battle_categoryTargetsBodyParts,
     &Battle_categoryTargetsBodyParts,
     sizeof Battle_categoryTargetsBodyParts},  // G00022 D_800E81C4
    {"BATTLE/BATTLE.PRG", 0x7F9D4, sizeof Battle_categoryTargetActorMasks,
     &Battle_categoryTargetActorMasks,
     sizeof Battle_categoryTargetActorMasks},  // G00023 D_800E81D4
    {"BATTLE/BATTLE.PRG", 0x7F9E4, sizeof VS::Items::weaponCategoryUpperIds,
     &VS::Items::weaponCategoryUpperIds,
     sizeof VS::Items::weaponCategoryUpperIds},  // G00024 _wepIdCategories
    {"BATTLE/BATTLE.PRG", 0x7FA00, sizeof Battle_limbInjuryStatusIds,
     &Battle_limbInjuryStatusIds,
     sizeof Battle_limbInjuryStatusIds},  // G00025 D_800E8200
    {"BATTLE/BATTLE.PRG", 0x7FA04, sizeof Battle_limbStatusFlags,
     &Battle_limbStatusFlags,
     sizeof Battle_limbStatusFlags},  // G00026 D_800E8204
    {"BATTLE/BATTLE.PRG", 0x7FA14, sizeof VS::Items::itemCategoryUpperIds,
     &VS::Items::itemCategoryUpperIds,
     sizeof VS::Items::itemCategoryUpperIds},  // G00027 _itemCategorySizes
    {"BATTLE/BATTLE.PRG", 0x7FAF4, sizeof Battle_bladePpDamageBonuses,
     &Battle_bladePpDamageBonuses,
     sizeof Battle_bladePpDamageBonuses},  // G00030 D_800E82F4
    {"BATTLE/BATTLE.PRG", 0x7FC90, sizeof Battle_vitalsTickCounter,
     &Battle_vitalsTickCounter,
     sizeof Battle_vitalsTickCounter},  // G00033 D_800E8490
    {"BATTLE/BATTLE.PRG", 0x7FC94, sizeof Battle_regeneratingLimb,
     &Battle_regeneratingLimb,
     sizeof Battle_regeneratingLimb},  // G00034 D_800E8494
    {"BATTLE/BATTLE.PRG", 0x7FC98, sizeof Battle_targetingRiskTimer,
     &Battle_targetingRiskTimer,
     sizeof Battle_targetingRiskTimer},  // G00035 D_800E8498
    {"BATTLE/BATTLE.PRG", 0x7FC9C, sizeof Field_lockedDoorIndexRanges,
     &Field_lockedDoorIndexRanges,
     sizeof Field_lockedDoorIndexRanges},  // G00036 D_800E849C
    {"BATTLE/BATTLE.PRG", 0x7FCAC, sizeof Field_lockedDoorRoomPairs,
     &Field_lockedDoorRoomPairs,
     sizeof Field_lockedDoorRoomPairs},  // G00037 D_800E84AC
    {"BATTLE/BATTLE.PRG", 0x7FD08, sizeof Field_countableRoomMask,
     &Field_countableRoomMask,
     sizeof Field_countableRoomMask},  // G00038 vs_battle_mapCompletionFlags
    {"BATTLE/BATTLE.PRG", 0x7FD6C, sizeof Field_triggerMarkerMatrix,
     &Field_triggerMarkerMatrix,
     sizeof Field_triggerMarkerMatrix},  // G00039 D_800E856C
    {"BATTLE/BATTLE.PRG", 0x7FD94, sizeof Field_triggerMarkerVertex,
     &Field_triggerMarkerVertex,
     sizeof Field_triggerMarkerVertex},  // G00040 D_800E8594
    {"BATTLE/BATTLE.PRG", 0x7FDA0, sizeof Field_triggerMarkerRecordTail,
     &Field_triggerMarkerRecordTail,
     sizeof Field_triggerMarkerRecordTail},  // G00041 D_800E85A0
    {"BATTLE/BATTLE.PRG", 0x7FDBC, sizeof Field_triggerMarkerTextureSource,
     &Field_triggerMarkerTextureSource,
     sizeof Field_triggerMarkerTextureSource},  // G00042 D_800E85BC
    {"BATTLE/BATTLE.PRG", 0x7FDC4, sizeof Field_triggerMarkerPaletteSource,
     &Field_triggerMarkerPaletteSource,
     sizeof Field_triggerMarkerPaletteSource},  // G00043 D_800E85C4
    {"BATTLE/BATTLE.PRG", 0x7FE1C, sizeof Battle_valueCaptionRects,
     &Battle_valueCaptionRects,
     sizeof Battle_valueCaptionRects},  // G00046 D_800E861C
    {"BATTLE/BATTLE.PRG", 0x7FE34, sizeof Battle_lockedLabelHalfHeights,
     &Battle_lockedLabelHalfHeights,
     sizeof Battle_lockedLabelHalfHeights},  // G00047 D_800E8634
    {"BATTLE/BATTLE.PRG", 0x7FE40, sizeof Battle_lockedLabelPieces,
     &Battle_lockedLabelPieces,
     sizeof Battle_lockedLabelPieces},  // G00048 D_800E8640
    {"BATTLE/BATTLE.PRG", 0x7FE60, sizeof Content_legacyShpDiscOffsets,
     &Content_legacyShpDiscOffsets,
     sizeof Content_legacyShpDiscOffsets},  // G00049 _shpLbaOffsets
    {"BATTLE/BATTLE.PRG", 0x80060, sizeof Content_legacyShpSectorCounts,
     &Content_legacyShpSectorCounts,
     sizeof Content_legacyShpSectorCounts},  // G00050 _shpFileSectorSizes
    {"BATTLE/BATTLE.PRG", 0x80160, sizeof Actors_sharedSequenceIdsByKind,
     &Actors_sharedSequenceIdsByKind,
     sizeof Actors_sharedSequenceIdsByKind},  // G00051 D_800E8960
    {"BATTLE/BATTLE.PRG", 0x80260, sizeof Content_legacySequenceDiscOffsets,
     &Content_legacySequenceDiscOffsets,
     sizeof Content_legacySequenceDiscOffsets},  // G00052 D_800E8A60
    {"BATTLE/BATTLE.PRG", 0x80340, sizeof Content_legacySequenceSectorCounts,
     &Content_legacySequenceSectorCounts,
     sizeof Content_legacySequenceSectorCounts},  // G00053 D_800E8B40
    {"BATTLE/BATTLE.PRG", 0x803B0, sizeof Content_legacyEtmDiscOffsets,
     &Content_legacyEtmDiscOffsets,
     sizeof Content_legacyEtmDiscOffsets},  // G00054 _etmLbaOffsets
    {"BATTLE/BATTLE.PRG", 0x80490, sizeof Content_legacyEtmSectorCounts,
     &Content_legacyEtmSectorCounts,
     sizeof Content_legacyEtmSectorCounts},  // G00055 _etmFileSectorSizes
    {"BATTLE/BATTLE.PRG", 0x80500, sizeof Content_legacyWepDiscOffsets,
     &Content_legacyWepDiscOffsets,
     sizeof Content_legacyWepDiscOffsets},  // G00056 D_800E8D00
    {"BATTLE/BATTLE.PRG", 0x80600, sizeof Content_legacyWepSectorCounts,
     &Content_legacyWepSectorCounts,
     sizeof Content_legacyWepSectorCounts},  // G00057 _wepFileSectorOffsets
    {"BATTLE/BATTLE.PRG", 0x80680, sizeof Battle_chainAbilityAnimationGroups,
     &Battle_chainAbilityAnimationGroups,
     sizeof Battle_chainAbilityAnimationGroups},  // G00058 D_800E8E80
    {"BATTLE/BATTLE.PRG", 0x8068E,
     sizeof Content_legacyMaterialSequenceDiscOffsets,
     &Content_legacyMaterialSequenceDiscOffsets,
     sizeof Content_legacyMaterialSequenceDiscOffsets},  // G00059 D_800E8E8E
    {"BATTLE/BATTLE.PRG", 0x806A3,
     sizeof Content_legacyMaterialSequenceSectorCounts,
     &Content_legacyMaterialSequenceSectorCounts,
     sizeof Content_legacyMaterialSequenceSectorCounts},  // G00060 D_800E8EA3
    {"BATTLE/BATTLE.PRG", 0x806A8,
     sizeof Content_legacyMaterialModelSequenceDiscOffsets,
     &Content_legacyMaterialModelSequenceDiscOffsets,
     sizeof Content_legacyMaterialModelSequenceDiscOffsets},  // G00061
                                                              // D_800E8EA8
    {"BATTLE/BATTLE.PRG", 0x806FC,
     sizeof Content_legacyMaterialModelSequenceSectorCounts,
     &Content_legacyMaterialModelSequenceSectorCounts,
     sizeof Content_legacyMaterialModelSequenceSectorCounts},  // G00062
                                                               // D_800E8EFC
    {"BATTLE/BATTLE.PRG", 0x80728, sizeof VS::Content::objectDataLoadStep,
     &VS::Content::objectDataLoadStep,
     sizeof VS::Content::objectDataLoadStep},  // G00063
                                               // vs_battle_loadObjectDataState
    {"BATTLE/BATTLE.PRG", 0x8072A, sizeof Content_legacyModelLoadPhase,
     &Content_legacyModelLoadPhase,
     sizeof Content_legacyModelLoadPhase},  // G00065 _loadShpState
    {"BATTLE/BATTLE.PRG", 0x8072B, sizeof Content_legacySequenceParsePhase,
     &Content_legacySequenceParsePhase,
     sizeof Content_legacySequenceParsePhase},  // G00066 D_800E8F2B
    {"BATTLE/BATTLE.PRG", 0x8072C, sizeof Actors_sharedSequenceBankIndex,
     &Actors_sharedSequenceBankIndex,
     sizeof Actors_sharedSequenceBankIndex},  // G00067 D_800E8F2C
    {"BATTLE/BATTLE.PRG", 0x8072D, sizeof Content_legacyEtmLoadPhase,
     &Content_legacyEtmLoadPhase,
     sizeof Content_legacyEtmLoadPhase},  // G00068 _loadEtmState
    {"BATTLE/BATTLE.PRG", 0x80730, sizeof Actors_textureSlotOrigins,
     &Actors_textureSlotOrigins,
     sizeof Actors_textureSlotOrigins},  // G00069 D_800E8F30
    {"BATTLE/BATTLE.PRG", 0x80790, sizeof Actors_textureSlotRowOffsets,
     &Actors_textureSlotRowOffsets,
     sizeof Actors_textureSlotRowOffsets},  // G00070 D_800E8F90
    {"BATTLE/BATTLE.PRG", 0x807A8, sizeof Field_gimmickPaletteParameters,
     &Field_gimmickPaletteParameters,
     sizeof Field_gimmickPaletteParameters},  // G00071 D_800E8FA8
    {"BATTLE/BATTLE.PRG", 0x807C0, sizeof Actors_forcedModelOverrides,
     &Actors_forcedModelOverrides,
     sizeof Actors_forcedModelOverrides},  // G00073 D_800E8FC0
    {"BATTLE/BATTLE.PRG", 0x807C8,
     sizeof VS::Items::weaponMaterialPaletteIndices,
     &VS::Items::weaponMaterialPaletteIndices,
     sizeof VS::Items::
         weaponMaterialPaletteIndices},  // G00075 _weaponMaterialPaletteMap
    {"BATTLE/BATTLE.PRG", 0x807D0, sizeof Field_kGimmickRollOrientations,
     &Field_kGimmickRollOrientations,
     sizeof Field_kGimmickRollOrientations},  // G00076 D_800E8FD0
    {"BATTLE/BATTLE.PRG", 0x80830, sizeof Field_kGimmickOrientationRotations,
     &Field_kGimmickOrientationRotations,
     sizeof Field_kGimmickOrientationRotations},  // G00077 D_800E9030
    {"BATTLE/BATTLE.PRG", 0x80890, sizeof Actors_drawnWeaponAnimationBases,
     &Actors_drawnWeaponAnimationBases,
     sizeof Actors_drawnWeaponAnimationBases},  // G00078 D_800E9090
    {"BATTLE/BATTLE.PRG", 0x8089C, sizeof Actors_sheathedWeaponAnimationBases,
     &Actors_sheathedWeaponAnimationBases,
     sizeof Actors_sheathedWeaponAnimationBases},  // G00079 D_800E909C
    {"BATTLE/BATTLE.PRG", 0x808A8, sizeof Field_interactingGimmickId,
     &Field_interactingGimmickId,
     sizeof Field_interactingGimmickId},  // G00080 D_800E90A8
    {"BATTLE/BATTLE.PRG", 0x808AC, sizeof Actors_kJumpHangFrames,
     &Actors_kJumpHangFrames,
     sizeof Actors_kJumpHangFrames},  // G00081 D_800E90AC
    {"BATTLE/BATTLE.PRG", 0x808B4, sizeof Actors_kJumpHeightAdjustments,
     &Actors_kJumpHeightAdjustments,
     sizeof Actors_kJumpHeightAdjustments},  // G00082 D_800E90B4
    {"BATTLE/BATTLE.PRG", 0x808C3, sizeof Actors_kMotionResultAnimations,
     &Actors_kMotionResultAnimations,
     sizeof Actors_kMotionResultAnimations},  // G00084 D_800E90C3
    {"BATTLE/BATTLE.PRG", 0x808C4, sizeof Actors_kMotionResultOneAnimationId,
     &Actors_kMotionResultOneAnimationId,
     sizeof Actors_kMotionResultOneAnimationId},  // G00085 D_800E90C4
    {"BATTLE/BATTLE.PRG", 0x808C6, sizeof Actors_kHangMotionAnimationId,
     &Actors_kHangMotionAnimationId,
     sizeof Actors_kHangMotionAnimationId},  // G00086 D_800E90C6
    {"BATTLE/BATTLE.PRG", 0x808CC, sizeof Actors_kSlowQuadrantMoveAnimations,
     &Actors_kSlowQuadrantMoveAnimations,
     sizeof Actors_kSlowQuadrantMoveAnimations},  // G00087 D_800E90CC
    {"BATTLE/BATTLE.PRG", 0x808D0, sizeof Actors_kFastQuadrantMoveAnimations,
     &Actors_kFastQuadrantMoveAnimations,
     sizeof Actors_kFastQuadrantMoveAnimations},  // G00088 D_800E90D0
    {"BATTLE/BATTLE.PRG", 0x8096C, sizeof Actors_secondarySlotSequences,
     &Actors_secondarySlotSequences,
     sizeof Actors_secondarySlotSequences},  // G00089 D_800E916C
    {"BATTLE/BATTLE.PRG", 0x8099C, sizeof Field_gimmickTemplateIndices,
     &Field_gimmickTemplateIndices,
     sizeof Field_gimmickTemplateIndices},  // G00090 D_800E919C
    {"BATTLE/BATTLE.PRG", 0x809A4, sizeof VS::Audio::objectSurfaceSoundIds,
     &VS::Audio::objectSurfaceSoundIds,
     sizeof VS::Audio::objectSurfaceSoundIds},  // G00091 D_800E91A4
    {"BATTLE/BATTLE.PRG", 0x809AC, sizeof Field_kGimmickSpinHeightSteps,
     &Field_kGimmickSpinHeightSteps,
     sizeof Field_kGimmickSpinHeightSteps},  // G00092 D_800E91AC
    {"BATTLE/BATTLE.PRG", 0x809BC, sizeof Field_kGimmickSpinPitchSteps,
     &Field_kGimmickSpinPitchSteps,
     sizeof Field_kGimmickSpinPitchSteps},  // G00093 D_800E91BC
    {"BATTLE/BATTLE.PRG", 0x809CC, sizeof Field_kGimmickSpinYawSteps,
     &Field_kGimmickSpinYawSteps,
     sizeof Field_kGimmickSpinYawSteps},  // G00094 D_800E91CC
    {"BATTLE/BATTLE.PRG", 0x809DC, sizeof Field_kGimmickSpinRollSteps,
     &Field_kGimmickSpinRollSteps,
     sizeof Field_kGimmickSpinRollSteps},  // G00095 D_800E91DC
    {"BATTLE/BATTLE.PRG", 0x809EC, sizeof Field_kGimmickBounceHeightSteps,
     &Field_kGimmickBounceHeightSteps,
     sizeof Field_kGimmickBounceHeightSteps},  // G00096 D_800E91EC
    {"BATTLE/BATTLE.PRG", 0x809F0, sizeof Field_kGimmickTiltDepthSteps,
     &Field_kGimmickTiltDepthSteps,
     sizeof Field_kGimmickTiltDepthSteps},  // G00097 D_800E91F0
    {"BATTLE/BATTLE.PRG", 0x809FC, sizeof Field_kGimmickTiltAngleSteps,
     &Field_kGimmickTiltAngleSteps,
     sizeof Field_kGimmickTiltAngleSteps},  // G00098 D_800E91FC
    {"BATTLE/BATTLE.PRG", 0x80A08, sizeof Field_kGimmickRollHeightSteps,
     &Field_kGimmickRollHeightSteps,
     sizeof Field_kGimmickRollHeightSteps},  // G00099 D_800E9208
    {"BATTLE/BATTLE.PRG", 0x80A18, sizeof Field_kGimmickRollDisplacementSteps,
     &Field_kGimmickRollDisplacementSteps,
     sizeof Field_kGimmickRollDisplacementSteps},  // G00100 D_800E9218
    {"BATTLE/BATTLE.PRG", 0x80A28, sizeof Field_kGimmickRollAngleSteps,
     &Field_kGimmickRollAngleSteps,
     sizeof Field_kGimmickRollAngleSteps},  // G00101 D_800E9228
    {"BATTLE/BATTLE.PRG", 0x80A38, sizeof Field_kGimmickSquashHeightSteps,
     &Field_kGimmickSquashHeightSteps,
     sizeof Field_kGimmickSquashHeightSteps},  // G00102 D_800E9238
    {"BATTLE/BATTLE.PRG", 0x80A48, sizeof Field_kGimmickSquashScaleX,
     &Field_kGimmickSquashScaleX,
     sizeof Field_kGimmickSquashScaleX},  // G00103 D_800E9248
    {"BATTLE/BATTLE.PRG", 0x80A58, sizeof Field_kGimmickSquashScaleY,
     &Field_kGimmickSquashScaleY,
     sizeof Field_kGimmickSquashScaleY},  // G00104 D_800E9258
    {"BATTLE/BATTLE.PRG", 0x80A68, sizeof Field_kGimmickSquashScaleZ,
     &Field_kGimmickSquashScaleZ,
     sizeof Field_kGimmickSquashScaleZ},  // G00105 D_800E9268
    {"BATTLE/BATTLE.PRG", 0x80A78, sizeof Field_kTileSurfaceIds,
     &Field_kTileSurfaceIds,
     sizeof Field_kTileSurfaceIds},  // G00106 D_800E9278
    {"BATTLE/BATTLE.PRG", 0x80A98, sizeof Actors_animationEventLengths,
     &Actors_animationEventLengths,
     sizeof Actors_animationEventLengths},  // G00107 D_800E9298
    {"BATTLE/BATTLE.PRG", 0x80ADC, sizeof Actors_drawnWeaponSoundIds,
     &Actors_drawnWeaponSoundIds,
     sizeof Actors_drawnWeaponSoundIds},  // G00108 D_800E92DC
    {"BATTLE/BATTLE.PRG", 0x80AE8, sizeof Actors_kAirTimerEndAnimations,
     &Actors_kAirTimerEndAnimations,
     sizeof Actors_kAirTimerEndAnimations},  // G00109 D_800E92E8
    {"BATTLE/BATTLE.PRG", 0x80AF0, sizeof Actors_kSoftLandingAnimations,
     &Actors_kSoftLandingAnimations,
     sizeof Actors_kSoftLandingAnimations},  // G00110 D_800E92F0
    {"BATTLE/BATTLE.PRG", 0x80AF8, sizeof Actors_kHardLandingAnimations,
     &Actors_kHardLandingAnimations,
     sizeof Actors_kHardLandingAnimations},  // G00111 D_800E92F8
    {"BATTLE/BATTLE.PRG", 0x80B00, sizeof Actors_kLandingRecoveryAnimations,
     &Actors_kLandingRecoveryAnimations,
     sizeof Actors_kLandingRecoveryAnimations},  // G00112 D_800E9300
    {"BATTLE/BATTLE.PRG", 0x81228, sizeof Actors_animationEventPoints,
     &Actors_animationEventPoints,
     sizeof Actors_animationEventPoints},  // G00114 D_800E9A28
    {"BATTLE/BATTLE.PRG", 0x8134C, sizeof Event_weaponDrawStates,
     &Event_weaponDrawStates,
     sizeof Event_weaponDrawStates},  // G00116 D_800E9B4C
    {"BATTLE/BATTLE.PRG", 0x81358, sizeof Event_animationBankBases,
     &Event_animationBankBases,
     sizeof Event_animationBankBases},  // G00117 D_800E9B58
    {"BATTLE/BATTLE.PRG", 0x8135C, sizeof Event_specialSubroutineScript,
     &Event_specialSubroutineScript,
     sizeof Event_specialSubroutineScript},  // G00118 D_800E9B5C
    {"BATTLE/BATTLE.PRG", 0x813E0, sizeof Event_skipPaletteResetCommand,
     &Event_skipPaletteResetCommand,
     sizeof Event_skipPaletteResetCommand},  // G00119 D_800E9BE0
    {"BATTLE/BATTLE.PRG", 0x813E8, sizeof Event_kSkipSoundRestoreMarker,
     &Event_kSkipSoundRestoreMarker,
     sizeof Event_kSkipSoundRestoreMarker},  // G00120 D_800E9BE8
    {"BATTLE/BATTLE.PRG", 0x813F4, sizeof Event_kSceneTimerModes,
     &Event_kSceneTimerModes,
     sizeof Event_kSceneTimerModes},  // G00123 D_800E9BF4
    {"BATTLE/BATTLE.PRG", 0x813F8, sizeof Event_kSceneTimerResetScopes,
     &Event_kSceneTimerResetScopes,
     sizeof Event_kSceneTimerResetScopes},  // G00124 D_800E9BF8
    {"BATTLE/BATTLE.PRG", 0x81400, sizeof Event_kSpecialOperationTenArguments,
     &Event_kSpecialOperationTenArguments,
     sizeof Event_kSpecialOperationTenArguments},  // G00125 D_800E9C00
    {"BATTLE/BATTLE.PRG", 0x81408, sizeof Event_kDefaultScreenTint,
     &Event_kDefaultScreenTint,
     sizeof Event_kDefaultScreenTint},  // G00126 D_800E9C08
    {"BATTLE/BATTLE.PRG", 0x8140C, sizeof Event_kDefaultScreenTransform,
     &Event_kDefaultScreenTransform,
     sizeof Event_kDefaultScreenTransform},  // G00127 D_800E9C0C
    {"BATTLE/BATTLE.PRG", 0x8141C, sizeof Event_fieldCameraReturnCommands,
     &Event_fieldCameraReturnCommands,
     sizeof Event_fieldCameraReturnCommands},  // G00128 D_800E9C1C
    {"BATTLE/BATTLE.PRG", 0x81424, sizeof Event_kOrbitReturnHeightOffsets,
     &Event_kOrbitReturnHeightOffsets,
     sizeof Event_kOrbitReturnHeightOffsets},  // G00129 D_800E9C24
    {"BATTLE/BATTLE.PRG", 0x81428, sizeof Event_kOrbitReturnFirstCommand,
     &Event_kOrbitReturnFirstCommand,
     sizeof Event_kOrbitReturnFirstCommand},  // G00130 D_800E9C28
    {"BATTLE/BATTLE.PRG", 0x8142C, sizeof Event_kOrbitReturnSecondCommand,
     &Event_kOrbitReturnSecondCommand,
     sizeof Event_kOrbitReturnSecondCommand},  // G00131 D_800E9C2C
    {"BATTLE/BATTLE.PRG", 0x81430, sizeof Event_kOpcodeSizes,
     &Event_kOpcodeSizes, sizeof Event_kOpcodeSizes},  // G00132 D_800E9C30
    {"BATTLE/BATTLE.PRG", 0x81A34, sizeof Battle_sphereArcBases,
     &Battle_sphereArcBases,
     sizeof Battle_sphereArcBases},  // G00134 D_800EA234
    {"BATTLE/BATTLE.PRG", 0x81AC4, sizeof Battle_sphereMirroredVertices,
     &Battle_sphereMirroredVertices,
     sizeof Battle_sphereMirroredVertices},  // G00135 D_800EA2C4
    {"BATTLE/BATTLE.PRG", 0x81C38, sizeof Battle_sphereShapeCommandStarts,
     &Battle_sphereShapeCommandStarts,
     sizeof Battle_sphereShapeCommandStarts},  // G00136 D_800EA438
    {"BATTLE/BATTLE.PRG", 0x81C6C, sizeof Battle_sphereWireframeCommands,
     &Battle_sphereWireframeCommands,
     sizeof Battle_sphereWireframeCommands},  // G00137 D_800EA46C
    {"BATTLE/BATTLE.PRG", 0x81E70, sizeof Battle_positionCursorSpriteOffsets,
     &Battle_positionCursorSpriteOffsets,
     sizeof Battle_positionCursorSpriteOffsets},  // G00139 D_800EA670
    {"BATTLE/BATTLE.PRG", 0x81E84, sizeof Battle_positionCursorSpriteSizes,
     &Battle_positionCursorSpriteSizes,
     sizeof Battle_positionCursorSpriteSizes},  // G00140 D_800EA684
    {"BATTLE/BATTLE.PRG", 0x81E98, sizeof Battle_positionCursorSpriteTexCoords,
     &Battle_positionCursorSpriteTexCoords,
     sizeof Battle_positionCursorSpriteTexCoords},  // G00141 D_800EA698
    {"BATTLE/BATTLE.PRG", 0x81EAC, sizeof Battle_positionCursorBlinkFrame,
     &Battle_positionCursorBlinkFrame,
     sizeof Battle_positionCursorBlinkFrame},  // G00142 D_800EA6AC
    {"BATTLE/BATTLE.PRG", 0x82068, sizeof Battle_statusStrings,
     &Battle_statusStrings,
     sizeof Battle_statusStrings},  // G00143 vs_battle_statusStrings
    {"BATTLE/BATTLE.PRG", 0x82184, sizeof Battle_targetInfoTemplate,
     &Battle_targetInfoTemplate,
     sizeof Battle_targetInfoTemplate},  // G00144 D_800EA984
    {"BATTLE/BATTLE.PRG", 0x821B6, sizeof Battle_targetStatusLineTemplate,
     &Battle_targetStatusLineTemplate,
     sizeof Battle_targetStatusLineTemplate},  // G00145 D_800EA9B6
    {"BATTLE/BATTLE.PRG", 0x82254, sizeof Battle_emptyStatusString,
     &Battle_emptyStatusString,
     sizeof Battle_emptyStatusString},  // G00146 D_800EAA54
    {"BATTLE/BATTLE.PRG", 0x823AA, sizeof Battle_targetSelectionStep,
     &Battle_targetSelectionStep,
     sizeof Battle_targetSelectionStep},  // G00147 D_800EABAA
    {"BATTLE/BATTLE.PRG", 0x82F08, sizeof VS::Render::compactGlyphLeftTrim,
     &VS::Render::compactGlyphLeftTrim,
     sizeof VS::Render::compactGlyphLeftTrim},  // G00149 D_800EB708
    {"BATTLE/BATTLE.PRG", 0x82FB4, sizeof VS::Render::compactGlyphWidths,
     &VS::Render::compactGlyphWidths,
     sizeof VS::Render::compactGlyphWidths},  // G00150 D_800EB7B4
    {"BATTLE/BATTLE.PRG", 0x82FD4, sizeof VS::Render::compactGlyphIndices,
     &VS::Render::compactGlyphIndices,
     sizeof VS::Render::compactGlyphIndices},  // G00151 D_800EB7D4
    {"BATTLE/BATTLE.PRG", 0x83010, sizeof VS::Render::variableFontGlyphWidths,
     &VS::Render::variableFontGlyphWidths,
     sizeof VS::Render::variableFontGlyphWidths},  // G00152
                                                   // vs_battle_characterWidths
    {"BATTLE/BATTLE.PRG", 0x8318C, sizeof VS::Render::kGlyphPixelExpansion,
     &VS::Render::kGlyphPixelExpansion,
     sizeof VS::Render::kGlyphPixelExpansion},  // G00153 D_800EB98C
    {"BATTLE/BATTLE.PRG", 0x831AC, sizeof Battle_reuseSphereVolumes,
     &Battle_reuseSphereVolumes,
     sizeof Battle_reuseSphereVolumes},  // G00154 D_800EB9AC
    {"BATTLE/BATTLE.PRG", 0x831AE, sizeof Battle_hudStatusIconsHidden,
     &Battle_hudStatusIconsHidden,
     sizeof Battle_hudStatusIconsHidden},  // G00156 D_800EB9AE
    {"BATTLE/BATTLE.PRG", 0x831AF, sizeof Battle_hudHidden, &Battle_hudHidden,
     sizeof Battle_hudHidden},  // G00157 D_800EB9AF
    {"BATTLE/BATTLE.PRG", 0x831B0, sizeof Battle_hudSlideOffset,
     &Battle_hudSlideOffset,
     sizeof Battle_hudSlideOffset},  // G00158 D_800EB9B0
    {"BATTLE/BATTLE.PRG", 0x831CC, sizeof Battle_hudBattleMode,
     &Battle_hudBattleMode, sizeof Battle_hudBattleMode},  // G00165 D_800EB9CC
    {"BATTLE/BATTLE.PRG", 0x831CD, sizeof Battle_battleModeIndicatorStep,
     &Battle_battleModeIndicatorStep,
     sizeof Battle_battleModeIndicatorStep},  // G00166 D_800EB9CD
    {"BATTLE/BATTLE.PRG", 0x831DC, sizeof VS::Menu::menuStrings,
     &VS::Menu::menuStrings,
     sizeof VS::Menu::menuStrings},  // G00171 vs_battle_menuStrings
    {"BATTLE/BATTLE.PRG", 0x833B8,
     sizeof VS::Menu::Framework::gradientTopColors,
     &VS::Menu::Framework::gradientTopColors,
     sizeof VS::Menu::Framework::gradientTopColors},  // G00173 D_800EBBB8
    {"BATTLE/BATTLE.PRG", 0x833C0,
     sizeof VS::Menu::Framework::gradientBottomColors,
     &VS::Menu::Framework::gradientBottomColors,
     sizeof VS::Menu::Framework::gradientBottomColors},  // G00174 D_800EBBC0
    {"BATTLE/BATTLE.PRG", 0x833C8,
     sizeof VS::Menu::Framework::rowAnimationSteps,
     &VS::Menu::Framework::rowAnimationSteps,
     sizeof VS::Menu::Framework::
         rowAnimationSteps},  // G00175 vs_battle_rowAnimationSteps
    {"BATTLE/BATTLE.PRG", 0x833DC, sizeof Battle_hudGaugePositions,
     &Battle_hudGaugePositions,
     sizeof Battle_hudGaugePositions},  // G00176 D_800EBBDC
    {"BATTLE/BATTLE.PRG", 0x833EC, sizeof Battle_hudGaugeLabelPositions,
     &Battle_hudGaugeLabelPositions,
     sizeof Battle_hudGaugeLabelPositions},  // G00177 D_800EBBEC
    {"BATTLE/BATTLE.PRG", 0x833FC, sizeof Battle_hudGaugeLabelWidths,
     &Battle_hudGaugeLabelWidths,
     sizeof Battle_hudGaugeLabelWidths},  // G00178 D_800EBBFC
    {"BATTLE/BATTLE.PRG", 0x83400, sizeof Battle_hudGaugeLabelTexU,
     &Battle_hudGaugeLabelTexU,
     sizeof Battle_hudGaugeLabelTexU},  // G00179 D_800EBC00
    {"BATTLE/BATTLE.PRG", 0x83404, sizeof VS::Render::kStatGaugeColors,
     &VS::Render::kStatGaugeColors,
     sizeof VS::Render::kStatGaugeColors},  // G00180 D_800EBC04
    {"BATTLE/BATTLE.PRG", 0x83414, sizeof Battle_statusIconTexOffsets,
     &Battle_statusIconTexOffsets,
     sizeof Battle_statusIconTexOffsets},  // G00181
                                           // vs_battle_statusIconTexOffsets
    {"BATTLE/BATTLE.PRG", 0x83454, sizeof Battle_limbStatusColors,
     &Battle_limbStatusColors,
     sizeof Battle_limbStatusColors},  // G00182 D_800EBC54
    {"BATTLE/BATTLE.PRG", 0x83468, sizeof Battle_upperStatusIconFlags,
     &Battle_upperStatusIconFlags,
     sizeof Battle_upperStatusIconFlags},  // G00183 D_800EBC68
    {"BATTLE/BATTLE.PRG", 0x83478, sizeof Battle_shortcutHudRequested,
     &Battle_shortcutHudRequested,
     sizeof Battle_shortcutHudRequested},  // G00184 D_800EBC78
    {"BATTLE/BATTLE.PRG", 0x8347C, sizeof VS::Menu::Framework::animationIndices,
     &VS::Menu::Framework::animationIndices,
     sizeof VS::Menu::Framework::
         animationIndices},  // G00185 vs_battle_animationIndices
    {"BATTLE/BATTLE.PRG", 0x83488, sizeof Battle_firstShortcutDigitOffsets,
     &Battle_firstShortcutDigitOffsets,
     sizeof Battle_firstShortcutDigitOffsets},  // G00186 D_800EBC88
    {"BATTLE/BATTLE.PRG", 0x83498, sizeof Battle_secondShortcutMarkerPositions,
     &Battle_secondShortcutMarkerPositions,
     sizeof Battle_secondShortcutMarkerPositions},  // G00187 D_800EBC98
    {"BATTLE/BATTLE.PRG", 0x834A0, sizeof Battle_secondShortcutIconPositions,
     &Battle_secondShortcutIconPositions,
     sizeof Battle_secondShortcutIconPositions},  // G00188 D_800EBCA0
    {"BATTLE/BATTLE.PRG", 0x834A8, sizeof Battle_secondShortcutIconSizes,
     &Battle_secondShortcutIconSizes,
     sizeof Battle_secondShortcutIconSizes},  // G00189 D_800EBCA8
    {"BATTLE/BATTLE.PRG", 0x834B8, sizeof Battle_secondShortcutIconTexCoords,
     &Battle_secondShortcutIconTexCoords,
     sizeof Battle_secondShortcutIconTexCoords},  // G00190 D_800EBCB8
    {"BATTLE/BATTLE.PRG", 0x834C0, sizeof Battle_firstShortcutMarkerPositions,
     &Battle_firstShortcutMarkerPositions,
     sizeof Battle_firstShortcutMarkerPositions},  // G00191 D_800EBCC0
    {"BATTLE/BATTLE.PRG", 0x834C8, sizeof Battle_firstShortcutIconPositions,
     &Battle_firstShortcutIconPositions,
     sizeof Battle_firstShortcutIconPositions},  // G00192 D_800EBCC8
    {"BATTLE/BATTLE.PRG", 0x834D0, sizeof Battle_firstShortcutIconSizes,
     &Battle_firstShortcutIconSizes,
     sizeof Battle_firstShortcutIconSizes},  // G00193 D_800EBCD0
    {"BATTLE/BATTLE.PRG", 0x834E0, sizeof Battle_firstShortcutIconTexCoords,
     &Battle_firstShortcutIconTexCoords,
     sizeof Battle_firstShortcutIconTexCoords},  // G00194 D_800EBCE0
    {"BATTLE/BATTLE.PRG", 0x834E8, sizeof Battle_shortcutHudAnimationStep,
     &Battle_shortcutHudAnimationStep,
     sizeof Battle_shortcutHudAnimationStep},  // G00195 D_800EBCE8
    {"BATTLE/BATTLE.PRG", 0x834EC, sizeof Battle_limbDisplaySpriteOffsets,
     &Battle_limbDisplaySpriteOffsets,
     sizeof Battle_limbDisplaySpriteOffsets},  // G00196 D_800EBCEC
    {"BATTLE/BATTLE.PRG", 0x83514, sizeof Battle_limbDisplaySpriteSizes,
     &Battle_limbDisplaySpriteSizes,
     sizeof Battle_limbDisplaySpriteSizes},  // G00197 D_800EBD14
    {"BATTLE/BATTLE.PRG", 0x8353C, sizeof Battle_limbDisplaySpriteTexCoords,
     &Battle_limbDisplaySpriteTexCoords,
     sizeof Battle_limbDisplaySpriteTexCoords},  // G00198 D_800EBD3C
    {"BATTLE/BATTLE.PRG", 0x83564, sizeof Battle_battleModeIndicatorWobble,
     &Battle_battleModeIndicatorWobble,
     sizeof Battle_battleModeIndicatorWobble},  // G00199 D_800EBD64
    {"BATTLE/BATTLE.PRG", 0x83568,
     sizeof VS::Menu::Framework::abilityGroupActionRanges,
     &VS::Menu::Framework::abilityGroupActionRanges,
     sizeof VS::Menu::Framework::abilityGroupActionRanges},  // G00200
                                                             // D_800EBD68
    {"BATTLE/BATTLE.PRG", 0x83570, sizeof VS::Menu::Magic::warlockSpellIds,
     &VS::Menu::Magic::warlockSpellIds,
     sizeof VS::Menu::Magic::warlockSpellIds},  // G00201
                                                // vs_battle_warlockSpellIds
    {"BATTLE/BATTLE.PRG", 0x83584, sizeof VS::Menu::Magic::shamanSpellIds,
     &VS::Menu::Magic::shamanSpellIds,
     sizeof VS::Menu::Magic::shamanSpellIds},  // G00202
                                               // vs_battle_shamanSpellIds
    {"BATTLE/BATTLE.PRG", 0x8358C, sizeof VS::Menu::Magic::sorcererSpellIds,
     &VS::Menu::Magic::sorcererSpellIds,
     sizeof VS::Menu::Magic::sorcererSpellIds},  // G00203
                                                 // vs_battle_sorcererSpellIds
    {"BATTLE/BATTLE.PRG", 0x835A0, sizeof VS::Menu::Magic::enchanterSpellIds,
     &VS::Menu::Magic::enchanterSpellIds,
     sizeof VS::Menu::Magic::enchanterSpellIds},  // G00204
                                                  // vs_battle_enchanterSpellIds
    {"BATTLE/BATTLE.PRG", 0x835A8, sizeof Battle_spellClassCounts,
     &Battle_spellClassCounts,
     sizeof Battle_spellClassCounts},  // G00205 _spellClassCounts
    {"BATTLE/BATTLE.PRG", 0x835BC, sizeof Battle_chainAbilityActionIds,
     &Battle_chainAbilityActionIds,
     sizeof Battle_chainAbilityActionIds},  // G00207
                                            // vs_battle_chainAbilityOffsets
    {"BATTLE/BATTLE.PRG", 0x835CC, sizeof Battle_defenseAbilityActionIds,
     &Battle_defenseAbilityActionIds,
     sizeof Battle_defenseAbilityActionIds},  // G00208
                                              // vs_battle_defenseAbilityOffsets
    {"BATTLE/BATTLE.PRG", 0x835DC, sizeof VS::Battle::doorStrings,
     &VS::Battle::doorStrings,
     sizeof VS::Battle::doorStrings},  // G00209 D_800EBDDC
    {"BATTLE/BATTLE.PRG", 0x83758, sizeof VS::Menu::kNotificationItemNames,
     &VS::Menu::kNotificationItemNames,
     sizeof VS::Menu::kNotificationItemNames},  // G00210 D_800EBF58
    {"BATTLE/BATTLE.PRG", 0x83A6C, sizeof VS::Core::interfaceRandomState,
     &VS::Core::interfaceRandomState,
     sizeof VS::Core::interfaceRandomState},  // G00213 _keystreamState
    {"BATTLE/BATTLE.PRG", 0x83A70,
     sizeof VS::Menu::Framework::cursorBrightnessAnimation,
     &VS::Menu::Framework::cursorBrightnessAnimation,
     sizeof VS::Menu::Framework::
         cursorBrightnessAnimation},  // G00214
                                      // vs_battle_cursorBrightnessAnimation
    {"BATTLE/BATTLE.PRG", 0x83A84, sizeof VS::Menu::textBoxAnchors,
     &VS::Menu::textBoxAnchors,
     sizeof VS::Menu::textBoxAnchors},  // G00216 D_800EC284
    {"BATTLE/BATTLE.PRG", 0x83ACC, sizeof VS::Menu::informationHeaderPositions,
     &VS::Menu::informationHeaderPositions,
     sizeof VS::Menu::informationHeaderPositions},  // G00217 D_800EC2CC
    {"BATTLE/BATTLE.PRG", 0x83AD8,
     sizeof VS::Menu::kInformationHeaderSpriteWords,
     &VS::Menu::kInformationHeaderSpriteWords,
     sizeof VS::Menu::kInformationHeaderSpriteWords},  // G00218 D_800EC2D8
    {"BATTLE/BATTLE.PRG", 0x83AE4, sizeof VS::Effects::effectBufferIndex,
     &VS::Effects::effectBufferIndex,
     sizeof VS::Effects::effectBufferIndex},  // G00219 D_800EC2E4
    {"BATTLE/BATTLE.PRG", 0x83AE8, sizeof VS::Effects::trailAdditiveColors,
     &VS::Effects::trailAdditiveColors,
     sizeof VS::Effects::trailAdditiveColors},  // G00220 D_800EC2E8
    {"BATTLE/BATTLE.PRG", 0x83B08, sizeof VS::Effects::trailColors,
     &VS::Effects::trailColors, sizeof VS::Effects::trailColors},  // G00221 D_800EC308
    {"BATTLE/BATTLE.PRG", 0x83B28, sizeof VS::Effects::emissionCounter,
     &VS::Effects::emissionCounter,
     sizeof VS::Effects::emissionCounter},  // G00223 D_800EC328
    {"BATTLE/BATTLE.PRG", 0x83B2C, sizeof VS::Effects::actionVariantValues,
     &VS::Effects::actionVariantValues,
     sizeof VS::Effects::actionVariantValues},  // G00224 D_800EC32C
    {"BATTLE/BATTLE.PRG", 0x83B30, sizeof VS::Effects::pairedParticleColorPresets,
     &VS::Effects::pairedParticleColorPresets,
     sizeof VS::Effects::pairedParticleColorPresets},  // G00225 D_800EC330
    {"BATTLE/BATTLE.PRG", 0x83B68, sizeof VS::Effects::particleColorPresetTable,
     &VS::Effects::particleColorPresetTable,
     sizeof VS::Effects::particleColorPresetTable},  // G00226 D_800EC368
    {"BATTLE/BATTLE.PRG", 0x83CB8, sizeof VS::Effects::lastReactionSoundVariant,
     &VS::Effects::lastReactionSoundVariant,
     sizeof VS::Effects::lastReactionSoundVariant},  // G00228 D_800EC4B8
    {"BATTLE/BATTLE.PRG", 0x83CBC, sizeof VS::Effects::emptyPackage,
     &VS::Effects::emptyPackage, sizeof VS::Effects::emptyPackage},  // G00229 D_800EC4BC
    {"BATTLE/BATTLE.PRG", 0x88EEC, sizeof Battle_Ai_directionDeltas,
     &Battle_Ai_directionDeltas,
     sizeof Battle_Ai_directionDeltas},  // G00230 D_800F16EC
    {"BATTLE/BATTLE.PRG", 0x88EF4, sizeof Battle_Ai_frameTickBudget,
     &Battle_Ai_frameTickBudget,
     sizeof Battle_Ai_frameTickBudget},  // G00231 D_800F16F4
    {"BATTLE/BATTLE.PRG", 0x88F5C, sizeof Battle_Ai_routeStepDirections,
     &Battle_Ai_routeStepDirections,
     sizeof Battle_Ai_routeStepDirections},  // G00232 D_800F175C
    {"BATTLE/BATTLE.PRG", 0x88FB8, sizeof Battle_Ai_viewAngles,
     &Battle_Ai_viewAngles, sizeof Battle_Ai_viewAngles},  // G00234 D_800F17B8
    {"BATTLE/BATTLE.PRG", 0x88FC8, sizeof Battle_Ai_settingsBits8Values,
     &Battle_Ai_settingsBits8Values,
     sizeof Battle_Ai_settingsBits8Values},  // G00235 D_800F17C8
    {"BATTLE/BATTLE.PRG", 0x88FD8, sizeof Battle_Ai_facingDelayScales,
     &Battle_Ai_facingDelayScales,
     sizeof Battle_Ai_facingDelayScales},  // G00236 D_800F17D8
    {"BATTLE/BATTLE.PRG", 0x88FDC, sizeof Battle_Ai_radialRetreatPath,
     &Battle_Ai_radialRetreatPath,
     sizeof Battle_Ai_radialRetreatPath},  // G00237 D_800F17DC
    {"EFFECT/PLG045.BIN", 0x1DB4,
     sizeof VS::Effects::Plg045::particleEmissionCounter,
     &VS::Effects::Plg045::particleEmissionCounter,
     sizeof VS::Effects::Plg045::particleEmissionCounter},  // G00634 D_800FB5B4
    {"EFFECT/PLG049.BIN", 0x4BD4,
     sizeof VS::Effects::Plg049::trailEmissionCounter,
     &VS::Effects::Plg049::trailEmissionCounter,
     sizeof VS::Effects::Plg049::trailEmissionCounter},  // G00637 D_800FE3D4
    {"EFFECT/PLG049.BIN", 0x4BD8, sizeof VS::Effects::Plg049::ringColorFactors,
     &VS::Effects::Plg049::ringColorFactors,
     sizeof VS::Effects::Plg049::ringColorFactors},  // G00638 D_800FE3D8
    {"EFFECT/PLG049.BIN", 0x4BF8,
     sizeof VS::Effects::Plg049::tiledRingColorFactors,
     &VS::Effects::Plg049::tiledRingColorFactors,
     sizeof VS::Effects::Plg049::tiledRingColorFactors},  // G00639 D_800FE3F8
    {"EFFECT/PLG050.BIN", 0x2E24, sizeof VS::Effects::Plg050::ringColorFactors,
     &VS::Effects::Plg050::ringColorFactors,
     sizeof VS::Effects::Plg050::ringColorFactors},  // G00640 D_800FC624
    {"EFFECT/PLG050.BIN", 0x2E44,
     sizeof VS::Effects::Plg050::trailEmissionCounter,
     &VS::Effects::Plg050::trailEmissionCounter,
     sizeof VS::Effects::Plg050::trailEmissionCounter},  // G00641 D_800FC644
    {"EFFECT/PLG053.BIN", 0x1AD4,
     sizeof VS::Effects::Plg053::closedRingColorFactors,
     &VS::Effects::Plg053::closedRingColorFactors,
     sizeof VS::Effects::Plg053::closedRingColorFactors},  // G00644 D_800FB2D4
    {"EFFECT/PLG057.BIN", 0x61B4,
     sizeof VS::Effects::Plg057::particleEmissionCounter,
     &VS::Effects::Plg057::particleEmissionCounter,
     sizeof VS::Effects::Plg057::particleEmissionCounter},  // G00645 D_800FF9B4
    {"EFFECT/PLG057.BIN", 0x61B8,
     sizeof VS::Effects::Plg057::trailEmissionCounter,
     &VS::Effects::Plg057::trailEmissionCounter,
     sizeof VS::Effects::Plg057::trailEmissionCounter},  // G00646 D_800FF9B8
    {"EFFECT/PLG057.BIN", 0x61BC,
     sizeof VS::Effects::Plg057::trailEmissionCounter2,
     &VS::Effects::Plg057::trailEmissionCounter2,
     sizeof VS::Effects::Plg057::trailEmissionCounter2},  // G00647 D_800FF9BC
    {"EFFECT/PLG061.BIN", 0x3290, sizeof VS::Effects::Plg061::surfaceVertices,
     &VS::Effects::Plg061::surfaceVertices,
     sizeof VS::Effects::Plg061::surfaceVertices},  // G00648 D_800FCA90
    {"EFFECT/PLG061.BIN", 0x3510, sizeof VS::Effects::Plg061::ringColorFactors,
     &VS::Effects::Plg061::ringColorFactors,
     sizeof VS::Effects::Plg061::ringColorFactors},  // G00649 D_800FCD10
    {"EFFECT/PLG061.BIN", 0x3530,
     sizeof VS::Effects::Plg061::closedRingColorFactors,
     &VS::Effects::Plg061::closedRingColorFactors,
     sizeof VS::Effects::Plg061::closedRingColorFactors},  // G00650 D_800FCD30
    {"EFFECT/PLG065.BIN", 0x4134,
     sizeof VS::Effects::Plg065::closedRingColorFactors,
     &VS::Effects::Plg065::closedRingColorFactors,
     sizeof VS::Effects::Plg065::closedRingColorFactors},  // G00651 D_800FD934
    {"EFFECT/PLG065.BIN", 0x4154, sizeof VS::Effects::Plg065::ringColorFactors,
     &VS::Effects::Plg065::ringColorFactors,
     sizeof VS::Effects::Plg065::ringColorFactors},  // G00652 D_800FD954
    {"EFFECT/PLG065.BIN", 0x4174,
     sizeof VS::Effects::Plg065::particleEmissionCounter,
     &VS::Effects::Plg065::particleEmissionCounter,
     sizeof VS::Effects::Plg065::particleEmissionCounter},  // G00653 D_800FD974
    {"EFFECT/PLG069.BIN", 0x3C24, sizeof VS::Effects::Plg069::mesh10Vertices,
     &VS::Effects::Plg069::mesh10Vertices,
     sizeof VS::Effects::Plg069::mesh10Vertices},  // G00654 D_800FD424
    {"EFFECT/PLG069.BIN", 0x3C74, sizeof VS::Effects::Plg069::mesh10Faces,
     &VS::Effects::Plg069::mesh10Faces,
     sizeof VS::Effects::Plg069::mesh10Faces},  // G00655 D_800FD474
    {"EFFECT/PLG069.BIN", 0x3CD4,
     sizeof VS::Effects::Plg069::trailEmissionCounter,
     &VS::Effects::Plg069::trailEmissionCounter,
     sizeof VS::Effects::Plg069::trailEmissionCounter},  // G00656 D_800FD4D4
    {"EFFECT/PLG069.BIN", 0x3CD8,
     sizeof VS::Effects::Plg069::tiledRingColorFactors,
     &VS::Effects::Plg069::tiledRingColorFactors,
     sizeof VS::Effects::Plg069::tiledRingColorFactors},  // G00657 D_800FD4D8
    {"EFFECT/PLG073.BIN", 0x573C,
     sizeof VS::Effects::Plg073::tiledRingColorFactors,
     &VS::Effects::Plg073::tiledRingColorFactors,
     sizeof VS::Effects::Plg073::tiledRingColorFactors},  // G00658 D_800FEF3C
    {"EFFECT/PLG073.BIN", 0x575C, sizeof VS::Effects::Plg073::ringColorFactors,
     &VS::Effects::Plg073::ringColorFactors,
     sizeof VS::Effects::Plg073::ringColorFactors},  // G00659 D_800FEF5C
    {"EFFECT/PLG073.BIN", 0x577C,
     sizeof VS::Effects::Plg073::trailEmissionCounter,
     &VS::Effects::Plg073::trailEmissionCounter,
     sizeof VS::Effects::Plg073::trailEmissionCounter},  // G00660 D_800FEF7C
    {"EFFECT/PLG077.BIN", 0x4108, sizeof VS::Effects::Plg077::mesh10Vertices,
     &VS::Effects::Plg077::mesh10Vertices,
     sizeof VS::Effects::Plg077::mesh10Vertices},  // G00661 D_800FD908
    {"EFFECT/PLG077.BIN", 0x4158, sizeof VS::Effects::Plg077::mesh10Faces,
     &VS::Effects::Plg077::mesh10Faces,
     sizeof VS::Effects::Plg077::mesh10Faces},  // G00662 D_800FD958
    {"EFFECT/PLG077.BIN", 0x41B8, sizeof VS::Effects::Plg077::ringColorFactors,
     &VS::Effects::Plg077::ringColorFactors,
     sizeof VS::Effects::Plg077::ringColorFactors},  // G00663 D_800FD9B8
    {"EFFECT/PLG077.BIN", 0x41D8, sizeof VS::Effects::Plg077::surfaceVertices,
     &VS::Effects::Plg077::surfaceVertices,
     sizeof VS::Effects::Plg077::surfaceVertices},  // G00664 D_800FD9D8
    {"EFFECT/PLG077.BIN", 0x4458, sizeof VS::Effects::Plg077::ringColorFactors2,
     &VS::Effects::Plg077::ringColorFactors2,
     sizeof VS::Effects::Plg077::ringColorFactors2},  // G00665 D_800FDC58
    {"EFFECT/PLG081.BIN", 0x2A4C, sizeof VS::Effects::Plg081::ringColorFactors,
     &VS::Effects::Plg081::ringColorFactors,
     sizeof VS::Effects::Plg081::ringColorFactors},  // G00666 D_800FC24C
    {"EFFECT/PLG081.BIN", 0x2A6C,
     sizeof VS::Effects::Plg081::closedRingColorFactors,
     &VS::Effects::Plg081::closedRingColorFactors,
     sizeof VS::Effects::Plg081::closedRingColorFactors},  // G00667 D_800FC26C
    {"EFFECT/PLG082.BIN", 0x3888,
     sizeof VS::Effects::Plg082::particleEmissionCounter,
     &VS::Effects::Plg082::particleEmissionCounter,
     sizeof VS::Effects::Plg082::particleEmissionCounter},  // G00668 D_800FD088
    {"EFFECT/PLG082.BIN", 0x388C,
     sizeof VS::Effects::Plg082::closedRingColorFactors,
     &VS::Effects::Plg082::closedRingColorFactors,
     sizeof VS::Effects::Plg082::closedRingColorFactors},  // G00669 D_800FD08C
    {"EFFECT/PLG085.BIN", 0x1A60,
     sizeof VS::Effects::Plg085::tiledRingColorFactors,
     &VS::Effects::Plg085::tiledRingColorFactors,
     sizeof VS::Effects::Plg085::tiledRingColorFactors},  // G00670 D_800FB260
    {"EFFECT/PLG086.BIN", 0x4B90,
     sizeof VS::Effects::Plg086::closedRingColorFactors,
     &VS::Effects::Plg086::closedRingColorFactors,
     sizeof VS::Effects::Plg086::closedRingColorFactors},  // G00671 D_800FE390
    {"EFFECT/PLG086.BIN", 0x4BB0,
     sizeof VS::Effects::Plg086::trailEmissionCounter,
     &VS::Effects::Plg086::trailEmissionCounter,
     sizeof VS::Effects::Plg086::trailEmissionCounter},  // G00672 D_800FE3B0
    {"EFFECT/PLG087.BIN", 0x5F5C, sizeof VS::Effects::Plg087::surfaceVertices,
     &VS::Effects::Plg087::surfaceVertices,
     sizeof VS::Effects::Plg087::surfaceVertices},  // G00673 D_800FF75C
    {"EFFECT/PLG087.BIN", 0x61DC,
     sizeof VS::Effects::Plg087::closedRingColorFactors,
     &VS::Effects::Plg087::closedRingColorFactors,
     sizeof VS::Effects::Plg087::closedRingColorFactors},  // G00674 D_800FF9DC
    {"EFFECT/PLG087.BIN", 0x61FC,
     sizeof VS::Effects::Plg087::trailEmissionCounter,
     &VS::Effects::Plg087::trailEmissionCounter,
     sizeof VS::Effects::Plg087::trailEmissionCounter},  // G00675 D_800FF9FC
    {"EFFECT/PLG087.BIN", 0x6200,
     sizeof VS::Effects::Plg087::particleEmissionCounter,
     &VS::Effects::Plg087::particleEmissionCounter,
     sizeof VS::Effects::Plg087::particleEmissionCounter},  // G00676 D_800FFA00
    {"EFFECT/PLG114.BIN", 0x34A0,
     sizeof VS::Effects::Plg114::particleEmissionCounter,
     &VS::Effects::Plg114::particleEmissionCounter,
     sizeof VS::Effects::Plg114::particleEmissionCounter},  // G00677 D_800FCCA0
    {"EFFECT/PLG114.BIN", 0x34A4,
     sizeof VS::Effects::Plg114::particleEmissionCounter2,
     &VS::Effects::Plg114::particleEmissionCounter2,
     sizeof VS::Effects::Plg114::particleEmissionCounter2},  // G00678
                                                             // D_800FCCA4
    {"EFFECT/PLG128.BIN", 0x6CF0, sizeof VS::Effects::Plg128::surfaceVertices,
     &VS::Effects::Plg128::surfaceVertices,
     sizeof VS::Effects::Plg128::surfaceVertices},  // G00679 D_801004F0
    {"EFFECT/PLG128.BIN", 0x6F70,
     sizeof VS::Effects::Plg128::closedRingColorFactors,
     &VS::Effects::Plg128::closedRingColorFactors,
     sizeof VS::Effects::Plg128::closedRingColorFactors},  // G00680 D_80100770
    {"EFFECT/PLG128.BIN", 0x6F90, sizeof VS::Effects::Plg128::surfaceVertices2,
     &VS::Effects::Plg128::surfaceVertices2,
     sizeof VS::Effects::Plg128::surfaceVertices2},  // G00681 D_80100790
    {"EFFECT/PLG128.BIN", 0x7210,
     sizeof VS::Effects::Plg128::trailEmissionCounter,
     &VS::Effects::Plg128::trailEmissionCounter,
     sizeof VS::Effects::Plg128::trailEmissionCounter},  // G00682 D_80100A10
    {"EFFECT/PLG129.BIN", 0x3DA8, sizeof VS::Effects::Plg129::ringColorFactors,
     &VS::Effects::Plg129::ringColorFactors,
     sizeof VS::Effects::Plg129::ringColorFactors},  // G00683 D_800FD5A8
    {"EFFECT/PLG129.BIN", 0x3DE8, sizeof VS::Effects::Plg129::mesh10Vertices,
     &VS::Effects::Plg129::mesh10Vertices,
     sizeof VS::Effects::Plg129::mesh10Vertices},  // G00684 D_800FD5E8
    {"EFFECT/PLG129.BIN", 0x3E38, sizeof VS::Effects::Plg129::mesh10Faces,
     &VS::Effects::Plg129::mesh10Faces,
     sizeof VS::Effects::Plg129::mesh10Faces},  // G00685 D_800FD638
    {"EFFECT/PLG129.BIN", 0x3E98, sizeof VS::Effects::Plg129::surfaceVertices,
     &VS::Effects::Plg129::surfaceVertices,
     sizeof VS::Effects::Plg129::surfaceVertices},  // G00686 D_800FD698
    {"EFFECT/PLG129.BIN", 0x4118, sizeof VS::Effects::Plg129::ringColorFactors2,
     &VS::Effects::Plg129::ringColorFactors2,
     sizeof VS::Effects::Plg129::ringColorFactors2},  // G00687 D_800FD918
    {"EFFECT/PLG134.BIN", 0x316C,
     sizeof VS::Effects::Plg134::trailEmissionCounter,
     &VS::Effects::Plg134::trailEmissionCounter,
     sizeof VS::Effects::Plg134::trailEmissionCounter},  // G00688 D_800FC96C
    {"EFFECT/PLG134.BIN", 0x3170,
     sizeof VS::Effects::Plg134::closedRingColorFactors,
     &VS::Effects::Plg134::closedRingColorFactors,
     sizeof VS::Effects::Plg134::closedRingColorFactors},  // G00689 D_800FC970
    {"EFFECT/PLG135.BIN", 0x316C,
     sizeof VS::Effects::Plg135::closedRingColorFactors,
     &VS::Effects::Plg135::closedRingColorFactors,
     sizeof VS::Effects::Plg135::closedRingColorFactors},  // G00690 D_800FC96C
    {"EFFECT/PLG135.BIN", 0x318C,
     sizeof VS::Effects::Plg135::trailEmissionCounter,
     &VS::Effects::Plg135::trailEmissionCounter,
     sizeof VS::Effects::Plg135::trailEmissionCounter},  // G00691 D_800FC98C
    {"EFFECT/PLG137.BIN", 0x478C, sizeof VS::Effects::Plg137::mesh10Vertices,
     &VS::Effects::Plg137::mesh10Vertices,
     sizeof VS::Effects::Plg137::mesh10Vertices},  // G00692 D_800FDF8C
    {"EFFECT/PLG137.BIN", 0x47DC, sizeof VS::Effects::Plg137::mesh10Faces,
     &VS::Effects::Plg137::mesh10Faces,
     sizeof VS::Effects::Plg137::mesh10Faces},  // G00693 D_800FDFDC
    {"EFFECT/PLG137.BIN", 0x483C,
     sizeof VS::Effects::Plg137::trailEmissionCounter,
     &VS::Effects::Plg137::trailEmissionCounter,
     sizeof VS::Effects::Plg137::trailEmissionCounter},  // G00694 D_800FE03C
    {"EFFECT/PLG137.BIN", 0x4840,
     sizeof VS::Effects::Plg137::tiledRingColorFactors,
     &VS::Effects::Plg137::tiledRingColorFactors,
     sizeof VS::Effects::Plg137::tiledRingColorFactors},  // G00695 D_800FE040
    {"EFFECT/PLG138.BIN", 0x353C, sizeof VS::Effects::Plg138::ringColorFactors,
     &VS::Effects::Plg138::ringColorFactors,
     sizeof VS::Effects::Plg138::ringColorFactors},  // G00696 D_800FCD3C
    {"EFFECT/PLG138.BIN", 0x355C,
     sizeof VS::Effects::Plg138::tiledRingColorFactors,
     &VS::Effects::Plg138::tiledRingColorFactors,
     sizeof VS::Effects::Plg138::tiledRingColorFactors},  // G00697 D_800FCD5C
    {"EFFECT/PLG141.BIN", 0x2200,
     sizeof VS::Effects::Plg141::trailEmissionCounter,
     &VS::Effects::Plg141::trailEmissionCounter,
     sizeof VS::Effects::Plg141::trailEmissionCounter},  // G00698 D_800FBA00
    {"EFFECT/PLG142.BIN", 0x35B8,
     sizeof VS::Effects::Plg142::closedRingColorFactors,
     &VS::Effects::Plg142::closedRingColorFactors,
     sizeof VS::Effects::Plg142::closedRingColorFactors},  // G00699 D_800FCDB8
    {"EFFECT/PLG142.BIN", 0x35D8, sizeof VS::Effects::Plg142::ringColorFactors,
     &VS::Effects::Plg142::ringColorFactors,
     sizeof VS::Effects::Plg142::ringColorFactors},  // G00700 D_800FCDD8
    {"EFFECT/PLG143.BIN", 0x1E64,
     sizeof VS::Effects::Plg143::tiledRingColorFactors,
     &VS::Effects::Plg143::tiledRingColorFactors,
     sizeof VS::Effects::Plg143::tiledRingColorFactors},  // G00701 D_800FB664
    {"EFFECT/PLG143.BIN", 0x1E84,
     sizeof VS::Effects::Plg143::closedRingColorFactors,
     &VS::Effects::Plg143::closedRingColorFactors,
     sizeof VS::Effects::Plg143::closedRingColorFactors},  // G00702 D_800FB684
    {"EFFECT/PLG146.BIN", 0x607C,
     sizeof VS::Effects::Plg146::tiledRingColorFactors,
     &VS::Effects::Plg146::tiledRingColorFactors,
     sizeof VS::Effects::Plg146::tiledRingColorFactors},  // G00703 D_800FF87C
    {"EFFECT/PLG146.BIN", 0x609C,
     sizeof VS::Effects::Plg146::mirrorSurfaceVertices,
     &VS::Effects::Plg146::mirrorSurfaceVertices,
     sizeof VS::Effects::Plg146::mirrorSurfaceVertices},  // G00704 D_800FF89C
    {"EFFECT/PLG146.BIN", 0x631C,
     sizeof VS::Effects::Plg146::particleEmissionCounter,
     &VS::Effects::Plg146::particleEmissionCounter,
     sizeof VS::Effects::Plg146::particleEmissionCounter},  // G00705 D_800FFB1C
    {"EFFECT/PLG146.BIN", 0x6320, sizeof VS::Effects::Plg146::ringColorFactors,
     &VS::Effects::Plg146::ringColorFactors,
     sizeof VS::Effects::Plg146::ringColorFactors},  // G00706 D_800FFB20
    {"EFFECT/PLG146.BIN", 0x6340,
     sizeof VS::Effects::Plg146::trailEmissionCounter,
     &VS::Effects::Plg146::trailEmissionCounter,
     sizeof VS::Effects::Plg146::trailEmissionCounter},  // G00707 D_800FFB40
    {"EFFECT/PLG147.BIN", 0x2310, sizeof VS::Effects::Plg147::ringColorFactors,
     &VS::Effects::Plg147::ringColorFactors,
     sizeof VS::Effects::Plg147::ringColorFactors},  // G00708 D_800FBB10
    {"EFFECT/PLG147.BIN", 0x2330,
     sizeof VS::Effects::Plg147::particleEmissionCounter,
     &VS::Effects::Plg147::particleEmissionCounter,
     sizeof VS::Effects::Plg147::particleEmissionCounter},  // G00709 D_800FBB30
    {"EFFECT/PLG148.BIN", 0x30F8,
     sizeof VS::Effects::Plg148::trailEmissionCounter,
     &VS::Effects::Plg148::trailEmissionCounter,
     sizeof VS::Effects::Plg148::trailEmissionCounter},  // G00710 D_800FC8F8
    {"EFFECT/PLG148.BIN", 0x30FC,
     sizeof VS::Effects::Plg148::tiledRingColorFactors,
     &VS::Effects::Plg148::tiledRingColorFactors,
     sizeof VS::Effects::Plg148::tiledRingColorFactors},  // G00711 D_800FC8FC
    {"EFFECT/PLG149.BIN", 0xC24, sizeof VS::Effects::Plg149::ringColorFactors,
     &VS::Effects::Plg149::ringColorFactors,
     sizeof VS::Effects::Plg149::ringColorFactors},  // G00712 D_800FA424
    {"EFFECT/PLG150.BIN", 0x321C,
     sizeof VS::Effects::Plg150::tiledRingColorFactors,
     &VS::Effects::Plg150::tiledRingColorFactors,
     sizeof VS::Effects::Plg150::tiledRingColorFactors},  // G00713 D_800FCA1C
    {"EFFECT/PLG150.BIN", 0x323C, sizeof VS::Effects::Plg150::surfaceVertices,
     &VS::Effects::Plg150::surfaceVertices,
     sizeof VS::Effects::Plg150::surfaceVertices},  // G00714 D_800FCA3C
    {"EFFECT/PLG150.BIN", 0x34BC, sizeof VS::Effects::Plg150::ringColorFactors,
     &VS::Effects::Plg150::ringColorFactors,
     sizeof VS::Effects::Plg150::ringColorFactors},  // G00715 D_800FCCBC
    {"EFFECT/PLG153.BIN", 0x3504, sizeof VS::Effects::Plg153::mesh10Vertices,
     &VS::Effects::Plg153::mesh10Vertices,
     sizeof VS::Effects::Plg153::mesh10Vertices},  // G00716 D_800FCD04
    {"EFFECT/PLG153.BIN", 0x3554, sizeof VS::Effects::Plg153::mesh10Faces,
     &VS::Effects::Plg153::mesh10Faces,
     sizeof VS::Effects::Plg153::mesh10Faces},  // G00717 D_800FCD54
    {"EFFECT/PLG153.BIN", 0x35B4, sizeof VS::Effects::Plg153::ringColorFactors,
     &VS::Effects::Plg153::ringColorFactors,
     sizeof VS::Effects::Plg153::ringColorFactors},  // G00718 D_800FCDB4
    {"EFFECT/PLG153.BIN", 0x35D4,
     sizeof VS::Effects::Plg153::particleEmissionCounter,
     &VS::Effects::Plg153::particleEmissionCounter,
     sizeof VS::Effects::Plg153::particleEmissionCounter},  // G00719 D_800FCDD4
    {"EFFECT/PLG154.BIN", 0x1A9C, sizeof VS::Effects::Plg154::mesh10Vertices,
     &VS::Effects::Plg154::mesh10Vertices,
     sizeof VS::Effects::Plg154::mesh10Vertices},  // G00720 D_800FB29C
    {"EFFECT/PLG154.BIN", 0x1AEC, sizeof VS::Effects::Plg154::mesh10Faces,
     &VS::Effects::Plg154::mesh10Faces,
     sizeof VS::Effects::Plg154::mesh10Faces},  // G00721 D_800FB2EC
    {"EFFECT/PLG154.BIN", 0x1B4C,
     sizeof VS::Effects::Plg154::closedRingColorFactors,
     &VS::Effects::Plg154::closedRingColorFactors,
     sizeof VS::Effects::Plg154::closedRingColorFactors},  // G00722 D_800FB34C
    {"EFFECT/PLG156.BIN", 0x291C,
     sizeof VS::Effects::Plg156::particleEmissionCounter,
     &VS::Effects::Plg156::particleEmissionCounter,
     sizeof VS::Effects::Plg156::particleEmissionCounter},  // G00723 D_800FC11C
    {"EFFECT/PLG158.BIN", 0x3638, sizeof VS::Effects::Plg158::surfaceVertices,
     &VS::Effects::Plg158::surfaceVertices,
     sizeof VS::Effects::Plg158::surfaceVertices},  // G00724 D_800FCE38
    {"EFFECT/PLG158.BIN", 0x38B8,
     sizeof VS::Effects::Plg158::mirrorSurfaceVertices,
     &VS::Effects::Plg158::mirrorSurfaceVertices,
     sizeof VS::Effects::Plg158::mirrorSurfaceVertices},  // G00725 D_800FD0B8
    {"EFFECT/PLG158.BIN", 0x3B38,
     sizeof VS::Effects::Plg158::closedRingColorFactors,
     &VS::Effects::Plg158::closedRingColorFactors,
     sizeof VS::Effects::Plg158::closedRingColorFactors},  // G00726 D_800FD338
    {"EFFECT/PLG160.BIN", 0x1B1C,
     sizeof VS::Effects::Plg160::tiledRingColorFactors,
     &VS::Effects::Plg160::tiledRingColorFactors,
     sizeof VS::Effects::Plg160::tiledRingColorFactors},  // G00727 D_800FB31C
    {"EFFECT/PLG160.BIN", 0x1B3C, sizeof VS::Effects::Plg160::ringColorFactors,
     &VS::Effects::Plg160::ringColorFactors,
     sizeof VS::Effects::Plg160::ringColorFactors},  // G00728 D_800FB33C
    {"EFFECT/PLG161.BIN", 0x5468, sizeof VS::Effects::Plg161::ringColorFactors,
     &VS::Effects::Plg161::ringColorFactors,
     sizeof VS::Effects::Plg161::ringColorFactors},  // G00729 D_800FEC68
    {"EFFECT/PLG161.BIN", 0x5488,
     sizeof VS::Effects::Plg161::trailEmissionCounter,
     &VS::Effects::Plg161::trailEmissionCounter,
     sizeof VS::Effects::Plg161::trailEmissionCounter},  // G00730 D_800FEC88
    {"EFFECT/PLG161.BIN", 0x548C, sizeof VS::Effects::Plg161::ringColorFactors2,
     &VS::Effects::Plg161::ringColorFactors2,
     sizeof VS::Effects::Plg161::ringColorFactors2},  // G00731 D_800FEC8C
    {"EFFECT/PLG162.BIN", 0x398C, sizeof VS::Effects::Plg162::ringColorFactors,
     &VS::Effects::Plg162::ringColorFactors,
     sizeof VS::Effects::Plg162::ringColorFactors},  // G00732 D_800FD18C
    {"EFFECT/PLG162.BIN", 0x39AC, sizeof VS::Effects::Plg162::ringColorFactors2,
     &VS::Effects::Plg162::ringColorFactors2,
     sizeof VS::Effects::Plg162::ringColorFactors2},  // G00733 D_800FD1AC
    {"EFFECT/PLG162.BIN", 0x39CC,
     sizeof VS::Effects::Plg162::closedRingColorFactors,
     &VS::Effects::Plg162::closedRingColorFactors,
     sizeof VS::Effects::Plg162::closedRingColorFactors},  // G00734 D_800FD1CC
    {"EFFECT/PLG165.BIN", 0x4B8C,
     sizeof VS::Effects::Plg165::trailEmissionCounter,
     &VS::Effects::Plg165::trailEmissionCounter,
     sizeof VS::Effects::Plg165::trailEmissionCounter},  // G00735 D_800FE38C
    {"EFFECT/PLG166.BIN", 0x36C0,
     sizeof VS::Effects::Plg166::mirrorSurfaceVertices,
     &VS::Effects::Plg166::mirrorSurfaceVertices,
     sizeof VS::Effects::Plg166::mirrorSurfaceVertices},  // G00736 D_800FCEC0
    {"EFFECT/PLG166.BIN", 0x3940,
     sizeof VS::Effects::Plg166::closedRingColorFactors,
     &VS::Effects::Plg166::closedRingColorFactors,
     sizeof VS::Effects::Plg166::closedRingColorFactors},  // G00737 D_800FD140
    {"EFFECT/PLG166.BIN", 0x3960, sizeof VS::Effects::Plg166::ringColorFactors,
     &VS::Effects::Plg166::ringColorFactors,
     sizeof VS::Effects::Plg166::ringColorFactors},  // G00738 D_800FD160
    {"EFFECT/PLG167.BIN", 0x4024,
     sizeof VS::Effects::Plg167::closedRingColorFactors,
     &VS::Effects::Plg167::closedRingColorFactors,
     sizeof VS::Effects::Plg167::closedRingColorFactors},  // G00739 D_800FD824
    {"EFFECT/PLG167.BIN", 0x4044,
     sizeof VS::Effects::Plg167::trailEmissionCounter,
     &VS::Effects::Plg167::trailEmissionCounter,
     sizeof VS::Effects::Plg167::trailEmissionCounter},  // G00740 D_800FD844
    {"EFFECT/PLG168.BIN", 0x4454,
     sizeof VS::Effects::Plg168::trailEmissionCounter,
     &VS::Effects::Plg168::trailEmissionCounter,
     sizeof VS::Effects::Plg168::trailEmissionCounter},  // G00741 D_800FDC54
    {"EFFECT/PLG168.BIN", 0x4458,
     sizeof VS::Effects::Plg168::particleEmissionCounter,
     &VS::Effects::Plg168::particleEmissionCounter,
     sizeof VS::Effects::Plg168::particleEmissionCounter},  // G00742 D_800FDC58
    {"EFFECT/PLG169.BIN", 0x3D90,
     sizeof VS::Effects::Plg169::trailEmissionCounter,
     &VS::Effects::Plg169::trailEmissionCounter,
     sizeof VS::Effects::Plg169::trailEmissionCounter},  // G00743 D_800FD590
    {"EFFECT/PLG169.BIN", 0x3D94, sizeof VS::Effects::Plg169::ringColorFactors,
     &VS::Effects::Plg169::ringColorFactors,
     sizeof VS::Effects::Plg169::ringColorFactors},  // G00744 D_800FD594
    {"EFFECT/PLG169.BIN", 0x3DB4,
     sizeof VS::Effects::Plg169::closedRingColorFactors,
     &VS::Effects::Plg169::closedRingColorFactors,
     sizeof VS::Effects::Plg169::closedRingColorFactors},  // G00745 D_800FD5B4
    {"EFFECT/PLG170.BIN", 0x4DC4,
     sizeof VS::Effects::Plg170::mirrorSurfaceVertices,
     &VS::Effects::Plg170::mirrorSurfaceVertices,
     sizeof VS::Effects::Plg170::mirrorSurfaceVertices},  // G00746 D_800FE5C4
    {"EFFECT/PLG170.BIN", 0x5044, sizeof VS::Effects::Plg170::ringColorFactors,
     &VS::Effects::Plg170::ringColorFactors,
     sizeof VS::Effects::Plg170::ringColorFactors},  // G00747 D_800FE844
    {"EFFECT/PLG170.BIN", 0x5064, sizeof VS::Effects::Plg170::surfaceVertices,
     &VS::Effects::Plg170::surfaceVertices,
     sizeof VS::Effects::Plg170::surfaceVertices},  // G00748 D_800FE864
    {"EFFECT/PLG170.BIN", 0x52E4,
     sizeof VS::Effects::Plg170::closedRingColorFactors,
     &VS::Effects::Plg170::closedRingColorFactors,
     sizeof VS::Effects::Plg170::closedRingColorFactors},  // G00749 D_800FEAE4
    {"EFFECT/PLG224.BIN", 0x16EC,
     sizeof VS::Effects::Plg224::particleEmissionCounter,
     &VS::Effects::Plg224::particleEmissionCounter,
     sizeof VS::Effects::Plg224::particleEmissionCounter},  // G00750 D_800FAEEC
    {"EFFECT/PLG232.BIN", 0x2200,
     sizeof VS::Effects::Plg232::trailEmissionCounter,
     &VS::Effects::Plg232::trailEmissionCounter,
     sizeof VS::Effects::Plg232::trailEmissionCounter},  // G00751 D_800FBA00
    {"EFFECT/PLG236.BIN", 0x3990,
     sizeof VS::Effects::Plg236::closedRingColorFactors,
     &VS::Effects::Plg236::closedRingColorFactors,
     sizeof VS::Effects::Plg236::closedRingColorFactors},  // G00752 D_800FD190
    {"EFFECT/PLG236.BIN", 0x39B0, sizeof VS::Effects::Plg236::ringColorFactors,
     &VS::Effects::Plg236::ringColorFactors,
     sizeof VS::Effects::Plg236::ringColorFactors},  // G00753 D_800FD1B0
    {"EFFECT/PLG236.BIN", 0x39D0, sizeof VS::Effects::Plg236::ringColorFactors2,
     &VS::Effects::Plg236::ringColorFactors2,
     sizeof VS::Effects::Plg236::ringColorFactors2},  // G00754 D_800FD1D0
    {"EFFECT/PLG242.BIN", 0x3C60,
     sizeof VS::Effects::Plg242::tiledRingColorFactors,
     &VS::Effects::Plg242::tiledRingColorFactors,
     sizeof VS::Effects::Plg242::tiledRingColorFactors},  // G00755 D_800FD460
    {"EFFECT/PLG242.BIN", 0x3C80,
     sizeof VS::Effects::Plg242::trailEmissionCounter,
     &VS::Effects::Plg242::trailEmissionCounter,
     sizeof VS::Effects::Plg242::trailEmissionCounter},  // G00756 D_800FD480
    {"EFFECT/PLG250.BIN", 0x1E28,
     sizeof VS::Effects::Plg250::closedRingColorFactors,
     &VS::Effects::Plg250::closedRingColorFactors,
     sizeof VS::Effects::Plg250::closedRingColorFactors},  // G00757 D_800FB628
    {"EFFECT/PLG255.BIN", 0x38EC,
     sizeof VS::Effects::Plg255::trailEmissionCounter,
     &VS::Effects::Plg255::trailEmissionCounter,
     sizeof VS::Effects::Plg255::trailEmissionCounter},  // G00758 D_800FD0EC
    {"EFFECT/PLG255.BIN", 0x38F0,
     sizeof VS::Effects::Plg255::particleEmissionCounter,
     &VS::Effects::Plg255::particleEmissionCounter,
     sizeof VS::Effects::Plg255::particleEmissionCounter},  // G00759 D_800FD0F0
    {"EFFECT/PLG260.BIN", 0xB2C, sizeof VS::Effects::Plg260::mesh10Vertices,
     &VS::Effects::Plg260::mesh10Vertices,
     sizeof VS::Effects::Plg260::mesh10Vertices},  // G00760 D_800FA32C
    {"EFFECT/PLG260.BIN", 0xB7C, sizeof VS::Effects::Plg260::mesh10Faces,
     &VS::Effects::Plg260::mesh10Faces,
     sizeof VS::Effects::Plg260::mesh10Faces},  // G00761 D_800FA37C
    {"EFFECT/PLG263.BIN", 0x44B0, sizeof VS::Effects::Plg263::surfaceVertices,
     &VS::Effects::Plg263::surfaceVertices,
     sizeof VS::Effects::Plg263::surfaceVertices},  // G00762 D_800FDCB0
    {"EFFECT/PLG263.BIN", 0x4730, sizeof VS::Effects::Plg263::ringColorFactors,
     &VS::Effects::Plg263::ringColorFactors,
     sizeof VS::Effects::Plg263::ringColorFactors},  // G00763 D_800FDF30
    {"EFFECT/PLG263.BIN", 0x4750, sizeof VS::Effects::Plg263::ringColorFactors2,
     &VS::Effects::Plg263::ringColorFactors2,
     sizeof VS::Effects::Plg263::ringColorFactors2},  // G00764 D_800FDF50
    {"EFFECT/PLG263.BIN", 0x4770,
     sizeof VS::Effects::Plg263::particleEmissionCounter,
     &VS::Effects::Plg263::particleEmissionCounter,
     sizeof VS::Effects::Plg263::particleEmissionCounter},  // G00765 D_800FDF70
    {"EFFECT/PLG265.BIN", 0x6598,
     sizeof VS::Effects::Plg265::tiledRingColorFactors,
     &VS::Effects::Plg265::tiledRingColorFactors,
     sizeof VS::Effects::Plg265::tiledRingColorFactors},  // G00766 D_800FFD98
    {"EFFECT/PLG265.BIN", 0x65B8,
     sizeof VS::Effects::Plg265::particleEmissionCounter,
     &VS::Effects::Plg265::particleEmissionCounter,
     sizeof VS::Effects::Plg265::particleEmissionCounter},  // G00767 D_800FFDB8
    {"EFFECT/PLG265.BIN", 0x65BC,
     sizeof VS::Effects::Plg265::trailEmissionCounter,
     &VS::Effects::Plg265::trailEmissionCounter,
     sizeof VS::Effects::Plg265::trailEmissionCounter},  // G00768 D_800FFDBC
    {"EFFECT/PLG265.BIN", 0x65C0,
     sizeof VS::Effects::Plg265::particleEmissionCounter2,
     &VS::Effects::Plg265::particleEmissionCounter2,
     sizeof VS::Effects::Plg265::particleEmissionCounter2},  // G00769
                                                             // D_800FFDC0
    {"EFFECT/PLG266.BIN", 0x56A0,
     sizeof VS::Effects::Plg266::particleEmissionCounter,
     &VS::Effects::Plg266::particleEmissionCounter,
     sizeof VS::Effects::Plg266::particleEmissionCounter},  // G00770 D_800FEEA0
    {"EFFECT/PLG266.BIN", 0x56A4,
     sizeof VS::Effects::Plg266::particleEmissionCounter2,
     &VS::Effects::Plg266::particleEmissionCounter2,
     sizeof VS::Effects::Plg266::particleEmissionCounter2},  // G00771
                                                             // D_800FEEA4
    {"EFFECT/PLG266.BIN", 0x56A8,
     sizeof VS::Effects::Plg266::trailEmissionCounter,
     &VS::Effects::Plg266::trailEmissionCounter,
     sizeof VS::Effects::Plg266::trailEmissionCounter},  // G00772 D_800FEEA8
    {"EFFECT/PLG272.BIN", 0x2670,
     sizeof VS::Effects::Plg272::closedRingColorFactors,
     &VS::Effects::Plg272::closedRingColorFactors,
     sizeof VS::Effects::Plg272::closedRingColorFactors},  // G00773 D_800FBE70
    {"EFFECT/PLG272.BIN", 0x2690, sizeof VS::Effects::Plg272::surfaceVertices,
     &VS::Effects::Plg272::surfaceVertices,
     sizeof VS::Effects::Plg272::surfaceVertices},  // G00774 D_800FBE90
    {"EFFECT/PLG273.BIN", 0xC74,
     sizeof VS::Effects::Plg273::mirrorSurfaceVertices,
     &VS::Effects::Plg273::mirrorSurfaceVertices,
     sizeof VS::Effects::Plg273::mirrorSurfaceVertices},  // G00775 D_800FA474
    {"EFFECT/PLG291.BIN", 0x3CD8,
     sizeof VS::Effects::Plg291::trailEmissionCounter,
     &VS::Effects::Plg291::trailEmissionCounter,
     sizeof VS::Effects::Plg291::trailEmissionCounter},  // G00776 D_800FD4D8
    {"EFFECT/PLG291.BIN", 0x3CDC,
     sizeof VS::Effects::Plg291::closedRingColorFactors,
     &VS::Effects::Plg291::closedRingColorFactors,
     sizeof VS::Effects::Plg291::closedRingColorFactors},  // G00777 D_800FD4DC
    {"EFFECT/PLG297.BIN", 0x3370,
     sizeof VS::Effects::Plg297::mirrorSurfaceVertices,
     &VS::Effects::Plg297::mirrorSurfaceVertices,
     sizeof VS::Effects::Plg297::mirrorSurfaceVertices},  // G00778 D_800FCB70
    {"EFFECT/PLG297.BIN", 0x35F0,
     sizeof VS::Effects::Plg297::closedRingColorFactors,
     &VS::Effects::Plg297::closedRingColorFactors,
     sizeof VS::Effects::Plg297::closedRingColorFactors},  // G00779 D_800FCDF0
    {"EFFECT/PLG297.BIN", 0x3610, sizeof VS::Effects::Plg297::ringColorFactors,
     &VS::Effects::Plg297::ringColorFactors,
     sizeof VS::Effects::Plg297::ringColorFactors},  // G00780 D_800FCE10
    {"EFFECT/PLG300.BIN", 0x48CC,
     sizeof VS::Effects::Plg300::mirrorSurfaceVertices,
     &VS::Effects::Plg300::mirrorSurfaceVertices,
     sizeof VS::Effects::Plg300::mirrorSurfaceVertices},  // G00781 D_800FE0CC
    {"EFFECT/PLG300.BIN", 0x4B4C,
     sizeof VS::Effects::Plg300::particleEmissionCounter,
     &VS::Effects::Plg300::particleEmissionCounter,
     sizeof VS::Effects::Plg300::particleEmissionCounter},  // G00782 D_800FE34C
    {"EFFECT/PLG300.BIN", 0x4B50, sizeof VS::Effects::Plg300::ringColorFactors,
     &VS::Effects::Plg300::ringColorFactors,
     sizeof VS::Effects::Plg300::ringColorFactors},  // G00783 D_800FE350
    {"EFFECT/PLG300.BIN", 0x4B70, sizeof VS::Effects::Plg300::ringColorFactors2,
     &VS::Effects::Plg300::ringColorFactors2,
     sizeof VS::Effects::Plg300::ringColorFactors2},  // G00784 D_800FE370
    {"EFFECT/PLG301.BIN", 0x2D24,
     sizeof VS::Effects::Plg301::particleEmissionCounter,
     &VS::Effects::Plg301::particleEmissionCounter,
     sizeof VS::Effects::Plg301::particleEmissionCounter},  // G00785 D_800FC524
    {"EFFECT/PLG301.BIN", 0x2D28,
     sizeof VS::Effects::Plg301::closedRingColorFactors,
     &VS::Effects::Plg301::closedRingColorFactors,
     sizeof VS::Effects::Plg301::closedRingColorFactors},  // G00786 D_800FC528
    {"ENDING/ENDING.PRG", 0x5BFC, sizeof VS::Ending::Credits::scriptBytes,
     &VS::Ending::Credits::scriptBytes,
     sizeof VS::Ending::Credits::scriptBytes},  // G00787 D_8006E3FC
    {"ENDING/ENDING.PRG", 0x777C, sizeof VS::Ending::Credits::glyphWidths,
     &VS::Ending::Credits::glyphWidths,
     sizeof VS::Ending::Credits::glyphWidths},  // G00788 _glyphWidths
    {"ENDING/ENDING.PRG", 0x72FB4,
     sizeof VS::Ending::Credits::illustrationHoldFrames,
     &VS::Ending::Credits::illustrationHoldFrames,
     sizeof VS::Ending::Credits::
         illustrationHoldFrames},  // G00803 _illustrationTimers
    {"ENDING/ENDING.PRG", 0x72FD4, sizeof VS::Ending::countableRoomBits,
     &VS::Ending::countableRoomBits,
     sizeof VS::Ending::countableRoomBits},  // G00804 _countableRooms
    {"ENDING/ENDING.PRG", 0x73014, sizeof VS::Ending::resultAtlasRegions,
     &VS::Ending::resultAtlasRegions,
     sizeof VS::Ending::resultAtlasRegions},  // G00805 _disMap
    {"ENDING/ENDING.PRG", 0x7323C, sizeof VS::Ending::rankLabelRegionIds,
     &VS::Ending::rankLabelRegionIds,
     sizeof VS::Ending::rankLabelRegionIds},  // G00808 _riskbreakerRanks
    {"ENDING/ENDING.PRG", 0x7327C, sizeof VS::Ending::congratulationsTint,
     &VS::Ending::congratulationsTint,
     sizeof VS::Ending::congratulationsTint},  // G00809 D_800DBA7C
    {"ENDING/ENDING.PRG", 0x73288, sizeof VS::Ending::scoreTint,
     &VS::Ending::scoreTint,
     sizeof VS::Ending::scoreTint},  // G00810 D_800DBA88
    {"ENDING/ENDING.PRG", 0x73290, sizeof VS::Ending::countingScoreTint,
     &VS::Ending::countingScoreTint,
     sizeof VS::Ending::countingScoreTint},  // G00811 D_800DBA90
    {"ENDING/ENDING.PRG", 0x73298, sizeof VS::Ending::mapCompletionTint,
     &VS::Ending::mapCompletionTint,
     sizeof VS::Ending::mapCompletionTint},  // G00812 D_800DBA98
    {"ENDING/ENDING.PRG", 0x732A0, sizeof VS::Ending::countingMapCompletionTint,
     &VS::Ending::countingMapCompletionTint,
     sizeof VS::Ending::countingMapCompletionTint},  // G00813 D_800DBAA0
    {"ENDING/ENDING.PRG", 0x732A8, sizeof VS::Ending::rankHeadingTint,
     &VS::Ending::rankHeadingTint,
     sizeof VS::Ending::rankHeadingTint},  // G00814 D_800DBAA8
    {"ENDING/ENDING.PRG", 0x732B0, sizeof VS::Ending::rankLabelTint,
     &VS::Ending::rankLabelTint,
     sizeof VS::Ending::rankLabelTint},  // G00815 D_800DBAB0
    {"ENDING/ENDING.PRG", 0x732B8, sizeof VS::Ending::rankTitleThresholds,
     &VS::Ending::rankTitleThresholds,
     sizeof VS::Ending::rankTitleThresholds},  // G00816 titleThresholds
    {"ENDING/ENDING.PRG", 0x732D8, sizeof VS::Ending::rankScoreThresholds,
     &VS::Ending::rankScoreThresholds,
     sizeof VS::Ending::rankScoreThresholds},  // G00817 rankScores
    {"MENU/MENU0.PRG", 0x2258, 4924u, &VS::Menu::Magic::magicStrings,
     sizeof VS::Menu::Magic::magicStrings},  // G00978 _magicStrings
    {"MENU/MENU0.PRG", 0x3594, 3197u, &VS::Menu::Magic::teleportationStrings,
     sizeof VS::Menu::Magic::teleportationStrings},  // G00980
                                                     // _teleportationStrings
    {"MENU/MENU1.PRG", 0xC78, 4302u, &VS::Menu::break_arts::strings,
     sizeof VS::Menu::break_arts::strings},  // G01004 _strings
    {"MENU/MENU2.PRG", 0x1E90, 2120u,
     &VS::Menu::BattleAbilities::battleAbilityStrings,
     sizeof VS::Menu::BattleAbilities::
         battleAbilityStrings},  // G01025
                                 // _battleAbilityStrings
    {"MENU/MENU2.PRG", 0x26D8, 584u,
     &VS::Menu::BattleAbilities::battleAbilityMenuStrings,
     sizeof VS::Menu::BattleAbilities::
         battleAbilityMenuStrings},  // G01024
                                     // _battleAbilityMenuStrings
    {"MENU/MENU3.PRG", 0x6BB8, 944u, &VS::Menu::Inventory::menuText,
     sizeof VS::Menu::Inventory::menuText},  // G01068 _menuText
    {"MENU/MENU4.PRG", 0x4C48, 3520u, &VS::Menu::Status::statusStrings,
     sizeof VS::Menu::Status::statusStrings},  // G01183 _statusStrings
    {"MENU/MENU5.PRG", 0x5BFC, 564u, &VS::Menu::map::mapNames,
     sizeof VS::Menu::map::mapNames},  // G01228 _mapNames
    {"MENU/MENU5.PRG", 0x5E30, 640u, &VS::Menu::map::connectingMaps,
     sizeof VS::Menu::map::connectingMaps},  // G01213 _connectingMaps
    {"MENU/MENU5.PRG", 0x60B0, 1044u, &VS::Menu::map::doors,
     sizeof VS::Menu::map::doors},  // G01218 _doors
    {"MENU/MENU7.PRG", 0x81B0, 1790u, &VS::Menu::Files::containerStrings,
     sizeof VS::Menu::Files::containerStrings},  // G01269 _containerStrings
    {"MENU/MENU8.PRG", 0x2D58, 2140u, &VS::Menu::Options::menuStrings,
     sizeof VS::Menu::Options::menuStrings},  // G01393 _menuStrings
    {"MENU/MENU8.PRG", 0x35B4, 416u, &VS::Menu::Rename::renderRenameMenuStrings,
     sizeof VS::Menu::Rename::
         renderRenameMenuStrings},  // G01396 _renderRenameMenuStrings
    {"MENU/MENU9.PRG", 0x5D20, 68u, &VS::Menu::Score::menuText,
     sizeof VS::Menu::Score::menuText},  // G01449 _menuText
    {"MENU/MENU9.PRG", 0x5D64, 384u, &VS::Menu::Score::rankText,
     sizeof VS::Menu::Score::rankText},  // G01463 _rankText
    {"MENU/MENU9.PRG", 0x5EE4, 728u, &VS::Menu::Score::titleText,
     sizeof VS::Menu::Score::titleText},  // G01474 _titleText
    {"MENU/MENU9.PRG", 0x61BC, 1568u, &VS::Menu::Score::titleDescriptions,
     sizeof VS::Menu::Score::titleDescriptions},  // G01473 _titleDescriptions
    {"MENU/MENU9.PRG", 0x67DC, 548u, &VS::Menu::Score::statHeaders,
     sizeof VS::Menu::Score::statHeaders},  // G01469 _statHeaders
    {"MENU/MENU9.PRG", 0x6A00, 920u, &VS::Menu::Score::statDescriptions,
     sizeof VS::Menu::Score::statDescriptions},  // G01468 _statDescriptions
    {"MENU/MENU9.PRG", 0x6D98, 116u, &VS::Menu::Score::timeAttacks,
     sizeof VS::Menu::Score::timeAttacks},  // G01472 _timeAttacks
    {"MENU/MENU9.PRG", 0x6E0C, 464u, &VS::Menu::Score::timeAttackDescriptions,
     sizeof VS::Menu::Score::
         timeAttackDescriptions},  // G01470 _timeAttackDescriptions
    {"MENU/MENU9.PRG", 0x6FDC, 3292u, &VS::Menu::Score::miscInfo,
     sizeof VS::Menu::Score::miscInfo},  // G01450 _miscInfo
    {"MENU/MENUD.PRG", 0x6D2C, 1380u, &VS::Menu::container::menuText,
     sizeof VS::Menu::container::menuText},  // G01739 _menuText
    {"MENU/MENUE.PRG", 0x2654, 1032u, &VS::Menu::manual::menuTextTable,
     sizeof VS::Menu::manual::menuTextTable},  // G01765 _menuText
    {"SLUS_010.40", 0x1FD44, sizeof VS::Audio::Driver::legacySpuEvent,
     &VS::Audio::Driver::legacySpuEvent,
     sizeof VS::Audio::Driver::legacySpuEvent},  // G01826 _soundEvent
    {"SLUS_010.40", 0x1FD48, sizeof VS::Audio::Driver::recentTickCostTotal,
     &VS::Audio::Driver::recentTickCostTotal,
     sizeof VS::Audio::Driver::recentTickCostTotal},  // G01827 D_8002F548
    {"SLUS_010.40", 0x1FD4C, sizeof VS::Audio::Seq::tickCounter,
     &VS::Audio::Seq::tickCounter,
     sizeof VS::Audio::Seq::tickCounter},  // G01828 D_8002F54C
    {"SLUS_010.40", 0x1FD50, sizeof VS::Audio::Driver::recentTickCosts,
     &VS::Audio::Driver::recentTickCosts,
     sizeof VS::Audio::Driver::recentTickCosts},  // G01829 D_8002F550
    {"SLUS_010.40", 0x1FD60, sizeof VS::Audio::Driver::legacySpuFlushBytes,
     &VS::Audio::Driver::legacySpuFlushBytes,
     sizeof VS::Audio::Driver::legacySpuFlushBytes},  // G01830 _soundFlush
    {"SLUS_010.40", 0x1FDA0, sizeof VS::Audio::Seq::kOpcodeLengths,
     &VS::Audio::Seq::kOpcodeLengths,
     sizeof VS::Audio::Seq::kOpcodeLengths},  // G01831 D_8002F5A0
    {"SLUS_010.40", 0x1FE00, sizeof VS::Audio::Seq::kExtendedOpcodeLengths,
     &VS::Audio::Seq::kExtendedOpcodeLengths,
     sizeof VS::Audio::Seq::kExtendedOpcodeLengths},  // G01832 D_8002F600
    {"SLUS_010.40", 0x1FE20, sizeof VS::Audio::Seq::kNoteLengths,
     &VS::Audio::Seq::kNoteLengths,
     sizeof VS::Audio::Seq::kNoteLengths},  // G01833 D_8002F620
    {"SLUS_010.40", 0x1FE38, sizeof VS::Audio::Seq::kIdleProgram,
     &VS::Audio::Seq::kIdleProgram,
     sizeof VS::Audio::Seq::kIdleProgram},  // G01834 g_Sound_ProgramCounter
    {"SLUS_010.40", 0x1FE3C, sizeof VS::Audio::Seq::kSemitonePitches,
     &VS::Audio::Seq::kSemitonePitches,
     sizeof VS::Audio::Seq::kSemitonePitches},  // G01835 D_8002F63C
    {"SLUS_010.40", 0x1FE6C, sizeof VS::Audio::Seq::kNoteRemapTables,
     &VS::Audio::Seq::kNoteRemapTables,
     sizeof VS::Audio::Seq::kNoteRemapTables},  // G01836 D_8002F66C
    {"SLUS_010.40", 0x1FE9C, sizeof VS::Audio::Seq::kRandomPitchOffsets,
     &VS::Audio::Seq::kRandomPitchOffsets,
     sizeof VS::Audio::Seq::kRandomPitchOffsets},  // G01837 D_8002F69C
    {"SLUS_010.40", 0x1FF9C, sizeof VS::Audio::Mixer::kPanGains,
     &VS::Audio::Mixer::kPanGains,
     sizeof VS::Audio::Mixer::kPanGains},  // G01838
                                           // g_Sound_StereoPanGainTableQ15
    {"SLUS_010.40", 0x31214, sizeof VS::Core::Math::kArcTangentTable,
     &VS::Core::Math::kArcTangentTable,
     sizeof VS::Core::Math::kArcTangentTable},  // G01903 D_80040A14
    {"SLUS_010.40", 0x39950, sizeof VS::Render::legacyLoadingImageHeader,
     &VS::Render::legacyLoadingImageHeader,
     sizeof VS::Render::legacyLoadingImageHeader},  // G02006 _nowLoading_header
    {"SLUS_010.40", 0x39954, sizeof VS::Render::loadingScreenPixels,
     &VS::Render::loadingScreenPixels,
     sizeof VS::Render::loadingScreenPixels},  // G02005 _nowLoading_data
    {"SLUS_010.40", 0x3C1DC, sizeof Battle_actionDefinitions,
     &Battle_actionDefinitions,
     sizeof Battle_actionDefinitions},  // G01904 vs_main_actions
    {"SLUS_010.40", 0x3F5DC, sizeof VS::Items::materialModifiers,
     &VS::Items::materialModifiers,
     sizeof VS::Items::materialModifiers},  // G01905 vs_main_materialModifiers
    {"SLUS_010.40", 0x3F6CC, sizeof Battle_statusEffectParams,
     &Battle_statusEffectParams,
     sizeof Battle_statusEffectParams},  // G01906 vs_main_statusEffectParams
    {"SLUS_010.40", 0x3F8CC, sizeof Field_locationDestinations,
     &Field_locationDestinations,
     sizeof Field_locationDestinations},  // G01913 vs_main_locationZoneMapIds
    {"SLUS_010.40", 0x404CC, sizeof Field_zoneMusicIds, &Field_zoneMusicIds,
     sizeof Field_zoneMusicIds},  // G01914 D_8004FCCC
    {"SLUS_010.40", 0x405D0, sizeof Actors_placeholderActorName,
     &Actors_placeholderActorName,
     sizeof Actors_placeholderActorName},  // G01915 D_8004FDD0
    {"SLUS_010.40", 0x405E4, sizeof Battle_breakArtPointRequirements,
     &Battle_breakArtPointRequirements,
     sizeof Battle_breakArtPointRequirements},  // G01916
                                                // vs_main_artsPointsRequirements
    {"SLUS_010.40", 0x4065C, sizeof Battle_battleAbilityPointRequirements,
     &Battle_battleAbilityPointRequirements,
     sizeof Battle_battleAbilityPointRequirements},  // G01917
                                                     // vs_main_battleAbilitiesPointsRequirements
    {"SLUS_010.40", 0x40688, sizeof Actors_usesSecondaryBodyModel,
     &Actors_usesSecondaryBodyModel,
     sizeof Actors_usesSecondaryBodyModel},  // G01918 D_8004FE88
    {"TITLE/TITLE.PRG", 0xA70C, sizeof VS::Title::publisherLogo,
     &VS::Title::publisherLogo,
     sizeof VS::Title::publisherLogo},  // G02143 publisher_clut
    {"TITLE/TITLE.PRG", 0xBF2C, sizeof VS::Title::developerLogo,
     &VS::Title::developerLogo,
     sizeof VS::Title::developerLogo},  // G02019 developer
    {"TITLE/TITLE.PRG", 0xE2D4, sizeof VS::Title::titleScreenBackground,
     &VS::Title::titleScreenBackground,
     sizeof VS::Title::titleScreenBackground},  // G02020 _titleScreenBg
    {"TITLE/TITLE.PRG", 0x449A8, sizeof VS::Title::copyrightImage,
     &VS::Title::copyrightImage,
     sizeof VS::Title::copyrightImage},  // G02021 _menuCopyright
    {"TITLE/TITLE.PRG", 0x46B68, sizeof VS::Title::saveMenuFontAtlas,
     &VS::Title::saveMenuFontAtlas,
     sizeof VS::Title::saveMenuFontAtlas},  // G02022 _fontTable
    {"TITLE/TITLE.PRG", 0x54B68, sizeof VS::Title::debugFontAtlas,
     &VS::Title::debugFontAtlas,
     sizeof VS::Title::debugFontAtlas},  // G02023 _debugFont
    {"TITLE/TITLE.PRG", 0x59A68, sizeof VS::Title::saveMenuUiAtlas,
     &VS::Title::saveMenuUiAtlas,
     sizeof VS::Title::saveMenuUiAtlas},  // G02024 _uiTable
    {"TITLE/TITLE.PRG", 0x68A68, sizeof VS::Title::saveMenuBackground,
     &VS::Title::saveMenuBackground,
     sizeof VS::Title::saveMenuBackground},  // G02089 _saveMenuBg_clut
    {"EFFECT/PLG074.BIN", 0x577C, 4u, VS::Content::plg074TrailEmissionCounter,
     sizeof VS::Content::plg074TrailEmissionCounter},
    {"BATTLE/BATTLE.PRG", 0x81268, 72u, Content_battleShadowVertices,
     sizeof Content_battleShadowVertices},
    {"BATTLE/BATTLE.PRG", 0x812D4, 96u, Content_battleShadowFaces,
     sizeof Content_battleShadowFaces},
    {"BATTLE/BATTLE.PRG", 0x88F58, 4u, VS::Content::battleAiHesitationLengths,
     sizeof VS::Content::battleAiHesitationLengths},
    {"BATTLE/BATTLE.PRG", 0x83278, 320u, Content_battleMainMenuHelpText,
     sizeof Content_battleMainMenuHelpText},
    {"BATTLE/BATTLE.PRG", 0x1060, 152u, Content_battleLimbNameBytes,
     sizeof Content_battleLimbNameBytes},
    {"BATTLE/BATTLE.PRG", 0x80B0C, 1820u, Content_battleGimmickTemplateBytes,
     sizeof Content_battleGimmickTemplateBytes},
    {"BATTLE/BATTLE.PRG", 0x823AC, 2524u, Content_battleTextboxFrameBytes,
     sizeof Content_battleTextboxFrameBytes},
    {"BATTLE/BATTLE.PRG", 0x7FD48, 116u, Content_battleTriggerMarkerBytes,
     sizeof Content_battleTriggerMarkerBytes},
    {"BATTLE/BATTLE.PRG", 0x41C, 664u, Content_battleSkyDome,
     sizeof Content_battleSkyDome},
    {"SLUS_010.40", 0x800, 335872u, Content_residentLfoSamples,
     sizeof Content_residentLfoSamples},
};
const unsigned VS::Content::kModuleDataCount = sizeof VS::Content::kModuleData / sizeof VS::Content::kModuleData[0];

