// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/Animation.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Spawning.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Audio/ActorSound.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Legacy.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Battle/Abilities.h"
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
#include "Battle/Targeting.h"
#include "Battle/TextBoxes.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Interpolation.h"
#include "Core/Random.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg045.h"
#include "Effects/Plugins/Plg049.h"
#include "Effects/Plugins/Plg050.h"
#include "Effects/Plugins/Plg057.h"
#include "Effects/Plugins/Plg061.h"
#include "Effects/Plugins/Plg065.h"
#include "Effects/Plugins/Plg069.h"
#include "Effects/Plugins/Plg073.h"
#include "Effects/Plugins/Plg077.h"
#include "Effects/Plugins/Plg082.h"
#include "Effects/Plugins/Plg085.h"
#include "Effects/Plugins/Plg086.h"
#include "Effects/Plugins/Plg087.h"
#include "Effects/Plugins/Plg114.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Plugins/Plg129.h"
#include "Effects/Plugins/Plg134.h"
#include "Effects/Plugins/Plg135.h"
#include "Effects/Plugins/Plg137.h"
#include "Effects/Plugins/Plg138.h"
#include "Effects/Plugins/Plg141.h"
#include "Effects/Plugins/Plg143.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg147.h"
#include "Effects/Plugins/Plg148.h"
#include "Effects/Plugins/Plg150.h"
#include "Effects/Plugins/Plg153.h"
#include "Effects/Plugins/Plg154.h"
#include "Effects/Plugins/Plg156.h"
#include "Effects/Plugins/Plg158.h"
#include "Effects/Plugins/Plg160.h"
#include "Effects/Plugins/Plg161.h"
#include "Effects/Plugins/Plg165.h"
#include "Effects/Plugins/Plg166.h"
#include "Effects/Plugins/Plg167.h"
#include "Effects/Plugins/Plg168.h"
#include "Effects/Plugins/Plg169.h"
#include "Effects/Plugins/Plg170.h"
#include "Effects/Plugins/Plg224.h"
#include "Effects/Plugins/Plg232.h"
#include "Effects/Plugins/Plg242.h"
#include "Effects/Plugins/Plg255.h"
#include "Effects/Plugins/Plg260.h"
#include "Effects/Plugins/Plg263.h"
#include "Effects/Plugins/Plg265.h"
#include "Effects/Plugins/Plg266.h"
#include "Effects/Plugins/Plg272.h"
#include "Effects/Plugins/Plg273.h"
#include "Effects/Plugins/Plg291.h"
#include "Effects/Plugins/Plg297.h"
#include "Effects/Plugins/Plg300.h"
#include "Effects/Plugins/Plg301.h"
#include "Effects/Runtime.h"
#include "Effects/Trails.h"
#include "Ending/ClearResults.h"
#include "Ending/Credits.h"
#include "Ending/Credits/StaffRoll.h"
#include "Ending/Results.h"
#include "Event/Actors.h"
#include "Event/Script.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptDialog.h"
#include "Event/ScriptField.h"
#include "Event/ScriptVm.h"
#include "Event/Sound.h"
#include "Field/Background.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Locations.h"
#include "Field/MapProgress.h"
#include "Field/TriggerMarkers.h"
#include "Items/Categories.h"
#include "Items/Materials.h"
#include "LegacyFiles.h"
#include "Menu/BattleAbilities.h"
#include "Menu/BreakArts.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Files.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Magic.h"
#include "Menu/Manual.h"
#include "Menu/Map.h"
#include "Menu/Options.h"
#include "Menu/Rename.h"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreScreen.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/TextBox.h"
#include "ModelLoading.h"
#include "Render/Font.h"
#include "Render/FramebufferEffects.h"
#include "Render/Images.h"
#include "Render/Loading.h"
#include "Render/ResultAtlas.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
#include "Render/Upload.h"
#include "Title/Credits.h"
#include "Title/SaveFiles.h"
#include "Title/TitleScreen.h"

// Globals whose contents are in the module images (DATA_LEDGER segment data/rodata/asset, no pointers).
#include "Content/ModuleData.h"

// decomp: BATTLE/BATTLE.PRG 7B5DC.s:func_800E4180 indexed hesitation lengths; native module-owned data
unsigned int Content_battleAiHesitationLengths[1];
// decomp: BATTLE/BATTLE.PRG 58578.c INCLUDE_RODATA D_80069860; strings end at jtbl_800698F8
unsigned int Content_battleLimbNameBytes[38];
// decomp: BATTLE/BATTLE.PRG 30DB0.c:_loadGimmick; three 16-byte headers plus modelDataSize payloads
unsigned int Content_battleGimmickTemplateBytes[455];
// decomp: BATTLE/BATTLE.PRG 5BF94.c:textbox frame vertices; nine styles precede D_800EB588
unsigned int Content_battleTextboxFrameBytes[631];
// decomp: SLUS_010.40 25AC.c:SoundVM_B4_Vibrato; sixteen short waveform sequences precede g_Sound_LfoTable
unsigned int Content_residentLfoSamples[513];

const Content_ModuleDataEntry Content_moduleData[] = {
    {"BATTLE/BATTLE.PRG", 0x41C, sizeof Field_legacyBackgroundGradientPackets, &Field_legacyBackgroundGradientPackets, sizeof Field_legacyBackgroundGradientPackets}, // G00012 D_80068C1C
    {"BATTLE/BATTLE.PRG", 0x13BC, sizeof Battle_Ai_kTimedStatusFlags, &Battle_Ai_kTimedStatusFlags, sizeof Battle_Ai_kTimedStatusFlags}, // G00013 D_80069BBC
    {"BATTLE/BATTLE.PRG", 0x1418, sizeof Battle_Ai_kSettingsBits30Values, &Battle_Ai_kSettingsBits30Values, sizeof Battle_Ai_kSettingsBits30Values}, // G00014 D_80069C18
    {"BATTLE/BATTLE.PRG", 0x141C, sizeof Battle_Ai_kPerceptionRangesSquared, &Battle_Ai_kPerceptionRangesSquared, sizeof Battle_Ai_kPerceptionRangesSquared}, // G00015 D_80069C1C
    {"BATTLE/BATTLE.PRG", 0x143C, sizeof Battle_Ai_kWanderTimingPresets, &Battle_Ai_kWanderTimingPresets, sizeof Battle_Ai_kWanderTimingPresets}, // G00016 D_80069C3C
    {"BATTLE/BATTLE.PRG", 0x2C7AC, sizeof Audio_legacyUnreferencedTable, &Audio_legacyUnreferencedTable, sizeof Audio_legacyUnreferencedTable}, // G00017 D_80094FAC
    {"BATTLE/BATTLE.PRG", 0x2C7FC, sizeof Audio_legacyUnreferencedWord, &Audio_legacyUnreferencedWord, sizeof Audio_legacyUnreferencedWord}, // G00018 D_80094FFC
    {"BATTLE/BATTLE.PRG", 0x7F984, sizeof Battle_categoryNeedsTargetSelection, &Battle_categoryNeedsTargetSelection, sizeof Battle_categoryNeedsTargetSelection}, // G00019 D_800E8184
    {"BATTLE/BATTLE.PRG", 0x7F994, sizeof Battle_categoryIsAreaTarget, &Battle_categoryIsAreaTarget, sizeof Battle_categoryIsAreaTarget}, // G00020 D_800E8194
    {"BATTLE/BATTLE.PRG", 0x7F9B4, sizeof Battle_categoryIncludesActingActor, &Battle_categoryIncludesActingActor, sizeof Battle_categoryIncludesActingActor}, // G00021 D_800E81B4
    {"BATTLE/BATTLE.PRG", 0x7F9C4, sizeof Battle_categoryTargetsBodyParts, &Battle_categoryTargetsBodyParts, sizeof Battle_categoryTargetsBodyParts}, // G00022 D_800E81C4
    {"BATTLE/BATTLE.PRG", 0x7F9D4, sizeof Battle_categoryTargetActorMasks, &Battle_categoryTargetActorMasks, sizeof Battle_categoryTargetActorMasks}, // G00023 D_800E81D4
    {"BATTLE/BATTLE.PRG", 0x7F9E4, sizeof Items_weaponCategoryUpperIds, &Items_weaponCategoryUpperIds, sizeof Items_weaponCategoryUpperIds}, // G00024 _wepIdCategories
    {"BATTLE/BATTLE.PRG", 0x7FA00, sizeof Battle_limbInjuryStatusIds, &Battle_limbInjuryStatusIds, sizeof Battle_limbInjuryStatusIds}, // G00025 D_800E8200
    {"BATTLE/BATTLE.PRG", 0x7FA04, sizeof Battle_limbStatusFlags, &Battle_limbStatusFlags, sizeof Battle_limbStatusFlags}, // G00026 D_800E8204
    {"BATTLE/BATTLE.PRG", 0x7FA14, sizeof Items_itemCategoryUpperIds, &Items_itemCategoryUpperIds, sizeof Items_itemCategoryUpperIds}, // G00027 _itemCategorySizes
    {"BATTLE/BATTLE.PRG", 0x7FAF4, sizeof Battle_bladePpDamageBonuses, &Battle_bladePpDamageBonuses, sizeof Battle_bladePpDamageBonuses}, // G00030 D_800E82F4
    {"BATTLE/BATTLE.PRG", 0x7FC90, sizeof Battle_vitalsTickCounter, &Battle_vitalsTickCounter, sizeof Battle_vitalsTickCounter}, // G00033 D_800E8490
    {"BATTLE/BATTLE.PRG", 0x7FC94, sizeof Battle_regeneratingLimb, &Battle_regeneratingLimb, sizeof Battle_regeneratingLimb}, // G00034 D_800E8494
    {"BATTLE/BATTLE.PRG", 0x7FC98, sizeof Battle_targetingRiskTimer, &Battle_targetingRiskTimer, sizeof Battle_targetingRiskTimer}, // G00035 D_800E8498
    {"BATTLE/BATTLE.PRG", 0x7FC9C, sizeof Field_lockedDoorIndexRanges, &Field_lockedDoorIndexRanges, sizeof Field_lockedDoorIndexRanges}, // G00036 D_800E849C
    {"BATTLE/BATTLE.PRG", 0x7FCAC, sizeof Field_lockedDoorRoomPairs, &Field_lockedDoorRoomPairs, sizeof Field_lockedDoorRoomPairs}, // G00037 D_800E84AC
    {"BATTLE/BATTLE.PRG", 0x7FD08, sizeof Field_countableRoomMask, &Field_countableRoomMask, sizeof Field_countableRoomMask}, // G00038 vs_battle_mapCompletionFlags
    {"BATTLE/BATTLE.PRG", 0x7FD6C, sizeof Field_triggerMarkerMatrix, &Field_triggerMarkerMatrix, sizeof Field_triggerMarkerMatrix}, // G00039 D_800E856C
    {"BATTLE/BATTLE.PRG", 0x7FD94, sizeof Field_triggerMarkerVertex, &Field_triggerMarkerVertex, sizeof Field_triggerMarkerVertex}, // G00040 D_800E8594
    {"BATTLE/BATTLE.PRG", 0x7FDA0, sizeof Field_triggerMarkerRecordTail, &Field_triggerMarkerRecordTail, sizeof Field_triggerMarkerRecordTail}, // G00041 D_800E85A0
    {"BATTLE/BATTLE.PRG", 0x7FDBC, sizeof Field_triggerMarkerTextureSource, &Field_triggerMarkerTextureSource, sizeof Field_triggerMarkerTextureSource}, // G00042 D_800E85BC
    {"BATTLE/BATTLE.PRG", 0x7FDC4, sizeof Field_triggerMarkerPaletteSource, &Field_triggerMarkerPaletteSource, sizeof Field_triggerMarkerPaletteSource}, // G00043 D_800E85C4
    {"BATTLE/BATTLE.PRG", 0x7FE1C, sizeof Battle_valueCaptionRects, &Battle_valueCaptionRects, sizeof Battle_valueCaptionRects}, // G00046 D_800E861C
    {"BATTLE/BATTLE.PRG", 0x7FE34, sizeof Battle_lockedLabelHalfHeights, &Battle_lockedLabelHalfHeights, sizeof Battle_lockedLabelHalfHeights}, // G00047 D_800E8634
    {"BATTLE/BATTLE.PRG", 0x7FE40, sizeof Battle_lockedLabelPieces, &Battle_lockedLabelPieces, sizeof Battle_lockedLabelPieces}, // G00048 D_800E8640
    {"BATTLE/BATTLE.PRG", 0x7FE60, sizeof Content_legacyShpDiscOffsets, &Content_legacyShpDiscOffsets, sizeof Content_legacyShpDiscOffsets}, // G00049 _shpLbaOffsets
    {"BATTLE/BATTLE.PRG", 0x80060, sizeof Content_legacyShpSectorCounts, &Content_legacyShpSectorCounts, sizeof Content_legacyShpSectorCounts}, // G00050 _shpFileSectorSizes
    {"BATTLE/BATTLE.PRG", 0x80160, sizeof Actors_sharedSequenceIdsByKind, &Actors_sharedSequenceIdsByKind, sizeof Actors_sharedSequenceIdsByKind}, // G00051 D_800E8960
    {"BATTLE/BATTLE.PRG", 0x80260, sizeof Content_legacySequenceDiscOffsets, &Content_legacySequenceDiscOffsets, sizeof Content_legacySequenceDiscOffsets}, // G00052 D_800E8A60
    {"BATTLE/BATTLE.PRG", 0x80340, sizeof Content_legacySequenceSectorCounts, &Content_legacySequenceSectorCounts, sizeof Content_legacySequenceSectorCounts}, // G00053 D_800E8B40
    {"BATTLE/BATTLE.PRG", 0x803B0, sizeof Content_legacyEtmDiscOffsets, &Content_legacyEtmDiscOffsets, sizeof Content_legacyEtmDiscOffsets}, // G00054 _etmLbaOffsets
    {"BATTLE/BATTLE.PRG", 0x80490, sizeof Content_legacyEtmSectorCounts, &Content_legacyEtmSectorCounts, sizeof Content_legacyEtmSectorCounts}, // G00055 _etmFileSectorSizes
    {"BATTLE/BATTLE.PRG", 0x80500, sizeof Content_legacyWepDiscOffsets, &Content_legacyWepDiscOffsets, sizeof Content_legacyWepDiscOffsets}, // G00056 D_800E8D00
    {"BATTLE/BATTLE.PRG", 0x80600, sizeof Content_legacyWepSectorCounts, &Content_legacyWepSectorCounts, sizeof Content_legacyWepSectorCounts}, // G00057 _wepFileSectorOffsets
    {"BATTLE/BATTLE.PRG", 0x80680, sizeof Battle_chainAbilityAnimationGroups, &Battle_chainAbilityAnimationGroups, sizeof Battle_chainAbilityAnimationGroups}, // G00058 D_800E8E80
    {"BATTLE/BATTLE.PRG", 0x8068E, sizeof Content_legacyMaterialSequenceDiscOffsets, &Content_legacyMaterialSequenceDiscOffsets, sizeof Content_legacyMaterialSequenceDiscOffsets}, // G00059 D_800E8E8E
    {"BATTLE/BATTLE.PRG", 0x806A3, sizeof Content_legacyMaterialSequenceSectorCounts, &Content_legacyMaterialSequenceSectorCounts, sizeof Content_legacyMaterialSequenceSectorCounts}, // G00060 D_800E8EA3
    {"BATTLE/BATTLE.PRG", 0x806A8, sizeof Content_legacyMaterialModelSequenceDiscOffsets, &Content_legacyMaterialModelSequenceDiscOffsets, sizeof Content_legacyMaterialModelSequenceDiscOffsets}, // G00061 D_800E8EA8
    {"BATTLE/BATTLE.PRG", 0x806FC, sizeof Content_legacyMaterialModelSequenceSectorCounts, &Content_legacyMaterialModelSequenceSectorCounts, sizeof Content_legacyMaterialModelSequenceSectorCounts}, // G00062 D_800E8EFC
    {"BATTLE/BATTLE.PRG", 0x80728, sizeof Menu_Inventory_equipmentModelLoadStep, &Menu_Inventory_equipmentModelLoadStep, sizeof Menu_Inventory_equipmentModelLoadStep}, // G00063 vs_battle_loadObjectDataState
    {"BATTLE/BATTLE.PRG", 0x80729, sizeof Content_modelParseResetFlag, &Content_modelParseResetFlag, sizeof Content_modelParseResetFlag}, // G00064 D_800E8F29
    {"BATTLE/BATTLE.PRG", 0x8072A, sizeof Content_legacyModelLoadPhase, &Content_legacyModelLoadPhase, sizeof Content_legacyModelLoadPhase}, // G00065 _loadShpState
    {"BATTLE/BATTLE.PRG", 0x8072B, sizeof Content_legacySequenceParsePhase, &Content_legacySequenceParsePhase, sizeof Content_legacySequenceParsePhase}, // G00066 D_800E8F2B
    {"BATTLE/BATTLE.PRG", 0x8072C, sizeof Actors_sharedSequenceBankIndex, &Actors_sharedSequenceBankIndex, sizeof Actors_sharedSequenceBankIndex}, // G00067 D_800E8F2C
    {"BATTLE/BATTLE.PRG", 0x8072D, sizeof Content_legacyEtmLoadPhase, &Content_legacyEtmLoadPhase, sizeof Content_legacyEtmLoadPhase}, // G00068 _loadEtmState
    {"BATTLE/BATTLE.PRG", 0x80730, sizeof Actors_textureSlotOrigins, &Actors_textureSlotOrigins, sizeof Actors_textureSlotOrigins}, // G00069 D_800E8F30
    {"BATTLE/BATTLE.PRG", 0x80790, sizeof Actors_textureSlotRowOffsets, &Actors_textureSlotRowOffsets, sizeof Actors_textureSlotRowOffsets}, // G00070 D_800E8F90
    {"BATTLE/BATTLE.PRG", 0x807A8, sizeof Field_gimmickPaletteParameters, &Field_gimmickPaletteParameters, sizeof Field_gimmickPaletteParameters}, // G00071 D_800E8FA8
    {"BATTLE/BATTLE.PRG", 0x807C0, sizeof Actors_reservedControlByte, &Actors_reservedControlByte, sizeof Actors_reservedControlByte}, // G00073 D_800E8FC0
    {"BATTLE/BATTLE.PRG", 0x807C4, sizeof Content_assetQueuePeakUsage, &Content_assetQueuePeakUsage, sizeof Content_assetQueuePeakUsage}, // G00074 D_800E8FC4
    {"BATTLE/BATTLE.PRG", 0x807C8, sizeof Items_weaponMaterialPaletteIndices, &Items_weaponMaterialPaletteIndices, sizeof Items_weaponMaterialPaletteIndices}, // G00075 _weaponMaterialPaletteMap
    {"BATTLE/BATTLE.PRG", 0x807D0, sizeof Field_kGimmickRollOrientations, &Field_kGimmickRollOrientations, sizeof Field_kGimmickRollOrientations}, // G00076 D_800E8FD0
    {"BATTLE/BATTLE.PRG", 0x80830, sizeof Field_kGimmickOrientationRotations, &Field_kGimmickOrientationRotations, sizeof Field_kGimmickOrientationRotations}, // G00077 D_800E9030
    {"BATTLE/BATTLE.PRG", 0x80890, sizeof Actors_drawnWeaponAnimationBases, &Actors_drawnWeaponAnimationBases, sizeof Actors_drawnWeaponAnimationBases}, // G00078 D_800E9090
    {"BATTLE/BATTLE.PRG", 0x8089C, sizeof Actors_sheathedWeaponAnimationBases, &Actors_sheathedWeaponAnimationBases, sizeof Actors_sheathedWeaponAnimationBases}, // G00079 D_800E909C
    {"BATTLE/BATTLE.PRG", 0x808A8, sizeof Field_interactingGimmickId, &Field_interactingGimmickId, sizeof Field_interactingGimmickId}, // G00080 D_800E90A8
    {"BATTLE/BATTLE.PRG", 0x808AC, sizeof Actors_kJumpHangFrames, &Actors_kJumpHangFrames, sizeof Actors_kJumpHangFrames}, // G00081 D_800E90AC
    {"BATTLE/BATTLE.PRG", 0x808B4, sizeof Actors_kJumpHeightAdjustments, &Actors_kJumpHeightAdjustments, sizeof Actors_kJumpHeightAdjustments}, // G00082 D_800E90B4
    {"BATTLE/BATTLE.PRG", 0x808BC, sizeof Actors_motionResultResetWord, &Actors_motionResultResetWord, sizeof Actors_motionResultResetWord}, // G00083 D_800E90BC
    {"BATTLE/BATTLE.PRG", 0x808C3, sizeof Actors_kMotionResultAnimations, &Actors_kMotionResultAnimations, sizeof Actors_kMotionResultAnimations}, // G00084 D_800E90C3
    {"BATTLE/BATTLE.PRG", 0x808C4, sizeof Actors_kMotionResultOneAnimationId, &Actors_kMotionResultOneAnimationId, sizeof Actors_kMotionResultOneAnimationId}, // G00085 D_800E90C4
    {"BATTLE/BATTLE.PRG", 0x808C6, sizeof Actors_kHangMotionAnimationId, &Actors_kHangMotionAnimationId, sizeof Actors_kHangMotionAnimationId}, // G00086 D_800E90C6
    {"BATTLE/BATTLE.PRG", 0x808CC, sizeof Actors_kSlowQuadrantMoveAnimations, &Actors_kSlowQuadrantMoveAnimations, sizeof Actors_kSlowQuadrantMoveAnimations}, // G00087 D_800E90CC
    {"BATTLE/BATTLE.PRG", 0x808D0, sizeof Actors_kFastQuadrantMoveAnimations, &Actors_kFastQuadrantMoveAnimations, sizeof Actors_kFastQuadrantMoveAnimations}, // G00088 D_800E90D0
    {"BATTLE/BATTLE.PRG", 0x8096C, sizeof Actors_secondarySlotSequences, &Actors_secondarySlotSequences, sizeof Actors_secondarySlotSequences}, // G00089 D_800E916C
    {"BATTLE/BATTLE.PRG", 0x8099C, sizeof Field_gimmickTemplateIndices, &Field_gimmickTemplateIndices, sizeof Field_gimmickTemplateIndices}, // G00090 D_800E919C
    {"BATTLE/BATTLE.PRG", 0x809A4, sizeof Audio_objectSurfaceSoundIds, &Audio_objectSurfaceSoundIds, sizeof Audio_objectSurfaceSoundIds}, // G00091 D_800E91A4
    {"BATTLE/BATTLE.PRG", 0x809AC, sizeof Field_kGimmickSpinHeightSteps, &Field_kGimmickSpinHeightSteps, sizeof Field_kGimmickSpinHeightSteps}, // G00092 D_800E91AC
    {"BATTLE/BATTLE.PRG", 0x809BC, sizeof Field_kGimmickSpinPitchSteps, &Field_kGimmickSpinPitchSteps, sizeof Field_kGimmickSpinPitchSteps}, // G00093 D_800E91BC
    {"BATTLE/BATTLE.PRG", 0x809CC, sizeof Field_kGimmickSpinYawSteps, &Field_kGimmickSpinYawSteps, sizeof Field_kGimmickSpinYawSteps}, // G00094 D_800E91CC
    {"BATTLE/BATTLE.PRG", 0x809DC, sizeof Field_kGimmickSpinRollSteps, &Field_kGimmickSpinRollSteps, sizeof Field_kGimmickSpinRollSteps}, // G00095 D_800E91DC
    {"BATTLE/BATTLE.PRG", 0x809EC, sizeof Field_kGimmickBounceHeightSteps, &Field_kGimmickBounceHeightSteps, sizeof Field_kGimmickBounceHeightSteps}, // G00096 D_800E91EC
    {"BATTLE/BATTLE.PRG", 0x809F0, sizeof Field_kGimmickTiltDepthSteps, &Field_kGimmickTiltDepthSteps, sizeof Field_kGimmickTiltDepthSteps}, // G00097 D_800E91F0
    {"BATTLE/BATTLE.PRG", 0x809FC, sizeof Field_kGimmickTiltAngleSteps, &Field_kGimmickTiltAngleSteps, sizeof Field_kGimmickTiltAngleSteps}, // G00098 D_800E91FC
    {"BATTLE/BATTLE.PRG", 0x80A08, sizeof Field_kGimmickRollHeightSteps, &Field_kGimmickRollHeightSteps, sizeof Field_kGimmickRollHeightSteps}, // G00099 D_800E9208
    {"BATTLE/BATTLE.PRG", 0x80A18, sizeof Field_kGimmickRollDisplacementSteps, &Field_kGimmickRollDisplacementSteps, sizeof Field_kGimmickRollDisplacementSteps}, // G00100 D_800E9218
    {"BATTLE/BATTLE.PRG", 0x80A28, sizeof Field_kGimmickRollAngleSteps, &Field_kGimmickRollAngleSteps, sizeof Field_kGimmickRollAngleSteps}, // G00101 D_800E9228
    {"BATTLE/BATTLE.PRG", 0x80A38, sizeof Field_kGimmickSquashHeightSteps, &Field_kGimmickSquashHeightSteps, sizeof Field_kGimmickSquashHeightSteps}, // G00102 D_800E9238
    {"BATTLE/BATTLE.PRG", 0x80A48, sizeof Field_kGimmickSquashScaleX, &Field_kGimmickSquashScaleX, sizeof Field_kGimmickSquashScaleX}, // G00103 D_800E9248
    {"BATTLE/BATTLE.PRG", 0x80A58, sizeof Field_kGimmickSquashScaleY, &Field_kGimmickSquashScaleY, sizeof Field_kGimmickSquashScaleY}, // G00104 D_800E9258
    {"BATTLE/BATTLE.PRG", 0x80A68, sizeof Field_kGimmickSquashScaleZ, &Field_kGimmickSquashScaleZ, sizeof Field_kGimmickSquashScaleZ}, // G00105 D_800E9268
    {"BATTLE/BATTLE.PRG", 0x80A78, sizeof Field_kTileSurfaceIds, &Field_kTileSurfaceIds, sizeof Field_kTileSurfaceIds}, // G00106 D_800E9278
    {"BATTLE/BATTLE.PRG", 0x80A98, sizeof Actors_animationEventLengths, &Actors_animationEventLengths, sizeof Actors_animationEventLengths}, // G00107 D_800E9298
    {"BATTLE/BATTLE.PRG", 0x80ADC, sizeof Actors_drawnWeaponSoundIds, &Actors_drawnWeaponSoundIds, sizeof Actors_drawnWeaponSoundIds}, // G00108 D_800E92DC
    {"BATTLE/BATTLE.PRG", 0x80AE8, sizeof Actors_kAirTimerEndAnimations, &Actors_kAirTimerEndAnimations, sizeof Actors_kAirTimerEndAnimations}, // G00109 D_800E92E8
    {"BATTLE/BATTLE.PRG", 0x80AF0, sizeof Actors_kSoftLandingAnimations, &Actors_kSoftLandingAnimations, sizeof Actors_kSoftLandingAnimations}, // G00110 D_800E92F0
    {"BATTLE/BATTLE.PRG", 0x80AF8, sizeof Actors_kHardLandingAnimations, &Actors_kHardLandingAnimations, sizeof Actors_kHardLandingAnimations}, // G00111 D_800E92F8
    {"BATTLE/BATTLE.PRG", 0x80B00, sizeof Actors_kLandingRecoveryAnimations, &Actors_kLandingRecoveryAnimations, sizeof Actors_kLandingRecoveryAnimations}, // G00112 D_800E9300
    {"BATTLE/BATTLE.PRG", 0x80B08, sizeof Actors_renderPassCounter, &Actors_renderPassCounter, sizeof Actors_renderPassCounter}, // G00113 D_800E9308
    {"BATTLE/BATTLE.PRG", 0x81228, sizeof Actors_animationEventPoints, &Actors_animationEventPoints, sizeof Actors_animationEventPoints}, // G00114 D_800E9A28
    {"BATTLE/BATTLE.PRG", 0x81334, sizeof Event_sfxSourceIds, &Event_sfxSourceIds, sizeof Event_sfxSourceIds}, // G00115 D_800E9B34
    {"BATTLE/BATTLE.PRG", 0x8134C, sizeof Event_weaponDrawStates, &Event_weaponDrawStates, sizeof Event_weaponDrawStates}, // G00116 D_800E9B4C
    {"BATTLE/BATTLE.PRG", 0x81358, sizeof Event_animationBankBases, &Event_animationBankBases, sizeof Event_animationBankBases}, // G00117 D_800E9B58
    {"BATTLE/BATTLE.PRG", 0x8135C, sizeof Event_specialSubroutineScript, &Event_specialSubroutineScript, sizeof Event_specialSubroutineScript}, // G00118 D_800E9B5C
    {"BATTLE/BATTLE.PRG", 0x813E0, sizeof Event_skipPaletteResetCommand, &Event_skipPaletteResetCommand, sizeof Event_skipPaletteResetCommand}, // G00119 D_800E9BE0
    {"BATTLE/BATTLE.PRG", 0x813E8, sizeof Event_kSkipSoundRestoreMarker, &Event_kSkipSoundRestoreMarker, sizeof Event_kSkipSoundRestoreMarker}, // G00120 D_800E9BE8
    {"BATTLE/BATTLE.PRG", 0x813EC, sizeof Event_kDialogPlacementOrder, &Event_kDialogPlacementOrder, sizeof Event_kDialogPlacementOrder}, // G00121 D_800E9BEC
    {"BATTLE/BATTLE.PRG", 0x813F0, sizeof Event_kMenuTransitionModes, &Event_kMenuTransitionModes, sizeof Event_kMenuTransitionModes}, // G00122 D_800E9BF0
    {"BATTLE/BATTLE.PRG", 0x813F4, sizeof Event_kSceneTimerModes, &Event_kSceneTimerModes, sizeof Event_kSceneTimerModes}, // G00123 D_800E9BF4
    {"BATTLE/BATTLE.PRG", 0x813F8, sizeof Event_kSceneTimerResetScopes, &Event_kSceneTimerResetScopes, sizeof Event_kSceneTimerResetScopes}, // G00124 D_800E9BF8
    {"BATTLE/BATTLE.PRG", 0x81400, sizeof Event_kSpecialOperationTenArguments, &Event_kSpecialOperationTenArguments, sizeof Event_kSpecialOperationTenArguments}, // G00125 D_800E9C00
    {"BATTLE/BATTLE.PRG", 0x81408, sizeof Event_kDefaultScreenTint, &Event_kDefaultScreenTint, sizeof Event_kDefaultScreenTint}, // G00126 D_800E9C08
    {"BATTLE/BATTLE.PRG", 0x8140C, sizeof Event_kDefaultScreenTransform, &Event_kDefaultScreenTransform, sizeof Event_kDefaultScreenTransform}, // G00127 D_800E9C0C
    {"BATTLE/BATTLE.PRG", 0x8141C, sizeof Event_fieldCameraReturnCommands, &Event_fieldCameraReturnCommands, sizeof Event_fieldCameraReturnCommands}, // G00128 D_800E9C1C
    {"BATTLE/BATTLE.PRG", 0x81424, sizeof Event_kOrbitReturnHeightOffsets, &Event_kOrbitReturnHeightOffsets, sizeof Event_kOrbitReturnHeightOffsets}, // G00129 D_800E9C24
    {"BATTLE/BATTLE.PRG", 0x81428, sizeof Event_kOrbitReturnFirstCommand, &Event_kOrbitReturnFirstCommand, sizeof Event_kOrbitReturnFirstCommand}, // G00130 D_800E9C28
    {"BATTLE/BATTLE.PRG", 0x8142C, sizeof Event_kOrbitReturnSecondCommand, &Event_kOrbitReturnSecondCommand, sizeof Event_kOrbitReturnSecondCommand}, // G00131 D_800E9C2C
    {"BATTLE/BATTLE.PRG", 0x81430, sizeof Event_kOpcodeSizes, &Event_kOpcodeSizes, sizeof Event_kOpcodeSizes}, // G00132 D_800E9C30
    {"BATTLE/BATTLE.PRG", 0x81A34, sizeof Battle_sphereArcBases, &Battle_sphereArcBases, sizeof Battle_sphereArcBases}, // G00134 D_800EA234
    {"BATTLE/BATTLE.PRG", 0x81AC4, sizeof Battle_sphereMirroredVertices, &Battle_sphereMirroredVertices, sizeof Battle_sphereMirroredVertices}, // G00135 D_800EA2C4
    {"BATTLE/BATTLE.PRG", 0x81C38, sizeof Battle_sphereShapeCommandStarts, &Battle_sphereShapeCommandStarts, sizeof Battle_sphereShapeCommandStarts}, // G00136 D_800EA438
    {"BATTLE/BATTLE.PRG", 0x81C6C, sizeof Battle_sphereWireframeCommands, &Battle_sphereWireframeCommands, sizeof Battle_sphereWireframeCommands}, // G00137 D_800EA46C
    {"BATTLE/BATTLE.PRG", 0x81E70, sizeof Battle_positionCursorSpriteOffsets, &Battle_positionCursorSpriteOffsets, sizeof Battle_positionCursorSpriteOffsets}, // G00139 D_800EA670
    {"BATTLE/BATTLE.PRG", 0x81E84, sizeof Battle_positionCursorSpriteSizes, &Battle_positionCursorSpriteSizes, sizeof Battle_positionCursorSpriteSizes}, // G00140 D_800EA684
    {"BATTLE/BATTLE.PRG", 0x81E98, sizeof Battle_positionCursorSpriteTexCoords, &Battle_positionCursorSpriteTexCoords, sizeof Battle_positionCursorSpriteTexCoords}, // G00141 D_800EA698
    {"BATTLE/BATTLE.PRG", 0x81EAC, sizeof Battle_positionCursorBlinkFrame, &Battle_positionCursorBlinkFrame, sizeof Battle_positionCursorBlinkFrame}, // G00142 D_800EA6AC
    {"BATTLE/BATTLE.PRG", 0x82068, sizeof Battle_statusStrings, &Battle_statusStrings, sizeof Battle_statusStrings}, // G00143 vs_battle_statusStrings
    {"BATTLE/BATTLE.PRG", 0x82184, sizeof Battle_targetInfoTemplate, &Battle_targetInfoTemplate, sizeof Battle_targetInfoTemplate}, // G00144 D_800EA984
    {"BATTLE/BATTLE.PRG", 0x821B6, sizeof Battle_targetStatusLineTemplate, &Battle_targetStatusLineTemplate, sizeof Battle_targetStatusLineTemplate}, // G00145 D_800EA9B6
    {"BATTLE/BATTLE.PRG", 0x82254, sizeof Battle_emptyStatusString, &Battle_emptyStatusString, sizeof Battle_emptyStatusString}, // G00146 D_800EAA54
    {"BATTLE/BATTLE.PRG", 0x823AA, sizeof Battle_targetSelectionStep, &Battle_targetSelectionStep, sizeof Battle_targetSelectionStep}, // G00147 D_800EABAA
    {"BATTLE/BATTLE.PRG", 0x82F08, sizeof Render_compactGlyphLeftTrim, &Render_compactGlyphLeftTrim, sizeof Render_compactGlyphLeftTrim}, // G00149 D_800EB708
    {"BATTLE/BATTLE.PRG", 0x82FB4, sizeof Render_compactGlyphWidths, &Render_compactGlyphWidths, sizeof Render_compactGlyphWidths}, // G00150 D_800EB7B4
    {"BATTLE/BATTLE.PRG", 0x82FD4, sizeof Render_compactGlyphIndices, &Render_compactGlyphIndices, sizeof Render_compactGlyphIndices}, // G00151 D_800EB7D4
    {"BATTLE/BATTLE.PRG", 0x83010, sizeof Render_variableFontGlyphWidths, &Render_variableFontGlyphWidths, sizeof Render_variableFontGlyphWidths}, // G00152 vs_battle_characterWidths
    {"BATTLE/BATTLE.PRG", 0x8318C, sizeof Render_kGlyphPixelExpansion, &Render_kGlyphPixelExpansion, sizeof Render_kGlyphPixelExpansion}, // G00153 D_800EB98C
    {"BATTLE/BATTLE.PRG", 0x831AC, sizeof Battle_reuseSphereVolumes, &Battle_reuseSphereVolumes, sizeof Battle_reuseSphereVolumes}, // G00154 D_800EB9AC
    {"BATTLE/BATTLE.PRG", 0x831AD, sizeof Menu_Framework_loadedSubMenu, &Menu_Framework_loadedSubMenu, sizeof Menu_Framework_loadedSubMenu}, // G00155 _loadedSubMenu
    {"BATTLE/BATTLE.PRG", 0x831AE, sizeof Battle_hudStatusIconsHidden, &Battle_hudStatusIconsHidden, sizeof Battle_hudStatusIconsHidden}, // G00156 D_800EB9AE
    {"BATTLE/BATTLE.PRG", 0x831AF, sizeof Battle_hudHidden, &Battle_hudHidden, sizeof Battle_hudHidden}, // G00157 D_800EB9AF
    {"BATTLE/BATTLE.PRG", 0x831B0, sizeof Battle_hudSlideOffset, &Battle_hudSlideOffset, sizeof Battle_hudSlideOffset}, // G00158 D_800EB9B0
    {"BATTLE/BATTLE.PRG", 0x831CC, sizeof Battle_hudBattleMode, &Battle_hudBattleMode, sizeof Battle_hudBattleMode}, // G00165 D_800EB9CC
    {"BATTLE/BATTLE.PRG", 0x831CD, sizeof Battle_battleModeIndicatorStep, &Battle_battleModeIndicatorStep, sizeof Battle_battleModeIndicatorStep}, // G00166 D_800EB9CD
    {"BATTLE/BATTLE.PRG", 0x831CE, sizeof Menu_textBoxBufferIndex, &Menu_textBoxBufferIndex, sizeof Menu_textBoxBufferIndex}, // G00167 D_800EB9CE
    {"BATTLE/BATTLE.PRG", 0x831DC, sizeof Menu_menuStrings, &Menu_menuStrings, sizeof Menu_menuStrings}, // G00171 vs_battle_menuStrings
    {"BATTLE/BATTLE.PRG", 0x83278, sizeof Menu_Framework_kMainMenuHelpText, &Menu_Framework_kMainMenuHelpText, sizeof Menu_Framework_kMainMenuHelpText}, // G00172 D_800EBA78
    {"BATTLE/BATTLE.PRG", 0x833B8, sizeof Menu_Framework_gradientTopColors, &Menu_Framework_gradientTopColors, sizeof Menu_Framework_gradientTopColors}, // G00173 D_800EBBB8
    {"BATTLE/BATTLE.PRG", 0x833C0, sizeof Menu_Framework_gradientBottomColors, &Menu_Framework_gradientBottomColors, sizeof Menu_Framework_gradientBottomColors}, // G00174 D_800EBBC0
    {"BATTLE/BATTLE.PRG", 0x833C8, sizeof Menu_Framework_rowAnimationSteps, &Menu_Framework_rowAnimationSteps, sizeof Menu_Framework_rowAnimationSteps}, // G00175 vs_battle_rowAnimationSteps
    {"BATTLE/BATTLE.PRG", 0x833DC, sizeof Battle_hudGaugePositions, &Battle_hudGaugePositions, sizeof Battle_hudGaugePositions}, // G00176 D_800EBBDC
    {"BATTLE/BATTLE.PRG", 0x833EC, sizeof Battle_hudGaugeLabelPositions, &Battle_hudGaugeLabelPositions, sizeof Battle_hudGaugeLabelPositions}, // G00177 D_800EBBEC
    {"BATTLE/BATTLE.PRG", 0x833FC, sizeof Battle_hudGaugeLabelWidths, &Battle_hudGaugeLabelWidths, sizeof Battle_hudGaugeLabelWidths}, // G00178 D_800EBBFC
    {"BATTLE/BATTLE.PRG", 0x83400, sizeof Battle_hudGaugeLabelTexU, &Battle_hudGaugeLabelTexU, sizeof Battle_hudGaugeLabelTexU}, // G00179 D_800EBC00
    {"BATTLE/BATTLE.PRG", 0x83404, sizeof Render_kStatGaugeColors, &Render_kStatGaugeColors, sizeof Render_kStatGaugeColors}, // G00180 D_800EBC04
    {"BATTLE/BATTLE.PRG", 0x83414, sizeof Battle_statusIconTexOffsets, &Battle_statusIconTexOffsets, sizeof Battle_statusIconTexOffsets}, // G00181 vs_battle_statusIconTexOffsets
    {"BATTLE/BATTLE.PRG", 0x83454, sizeof Battle_limbStatusColors, &Battle_limbStatusColors, sizeof Battle_limbStatusColors}, // G00182 D_800EBC54
    {"BATTLE/BATTLE.PRG", 0x83468, sizeof Battle_upperStatusIconFlags, &Battle_upperStatusIconFlags, sizeof Battle_upperStatusIconFlags}, // G00183 D_800EBC68
    {"BATTLE/BATTLE.PRG", 0x83478, sizeof Battle_shortcutHudRequested, &Battle_shortcutHudRequested, sizeof Battle_shortcutHudRequested}, // G00184 D_800EBC78
    {"BATTLE/BATTLE.PRG", 0x8347C, sizeof Menu_Framework_animationIndices, &Menu_Framework_animationIndices, sizeof Menu_Framework_animationIndices}, // G00185 vs_battle_animationIndices
    {"BATTLE/BATTLE.PRG", 0x83488, sizeof Battle_firstShortcutDigitOffsets, &Battle_firstShortcutDigitOffsets, sizeof Battle_firstShortcutDigitOffsets}, // G00186 D_800EBC88
    {"BATTLE/BATTLE.PRG", 0x83498, sizeof Battle_secondShortcutMarkerPositions, &Battle_secondShortcutMarkerPositions, sizeof Battle_secondShortcutMarkerPositions}, // G00187 D_800EBC98
    {"BATTLE/BATTLE.PRG", 0x834A0, sizeof Battle_secondShortcutIconPositions, &Battle_secondShortcutIconPositions, sizeof Battle_secondShortcutIconPositions}, // G00188 D_800EBCA0
    {"BATTLE/BATTLE.PRG", 0x834A8, sizeof Battle_secondShortcutIconSizes, &Battle_secondShortcutIconSizes, sizeof Battle_secondShortcutIconSizes}, // G00189 D_800EBCA8
    {"BATTLE/BATTLE.PRG", 0x834B8, sizeof Battle_secondShortcutIconTexCoords, &Battle_secondShortcutIconTexCoords, sizeof Battle_secondShortcutIconTexCoords}, // G00190 D_800EBCB8
    {"BATTLE/BATTLE.PRG", 0x834C0, sizeof Battle_firstShortcutMarkerPositions, &Battle_firstShortcutMarkerPositions, sizeof Battle_firstShortcutMarkerPositions}, // G00191 D_800EBCC0
    {"BATTLE/BATTLE.PRG", 0x834C8, sizeof Battle_firstShortcutIconPositions, &Battle_firstShortcutIconPositions, sizeof Battle_firstShortcutIconPositions}, // G00192 D_800EBCC8
    {"BATTLE/BATTLE.PRG", 0x834D0, sizeof Battle_firstShortcutIconSizes, &Battle_firstShortcutIconSizes, sizeof Battle_firstShortcutIconSizes}, // G00193 D_800EBCD0
    {"BATTLE/BATTLE.PRG", 0x834E0, sizeof Battle_firstShortcutIconTexCoords, &Battle_firstShortcutIconTexCoords, sizeof Battle_firstShortcutIconTexCoords}, // G00194 D_800EBCE0
    {"BATTLE/BATTLE.PRG", 0x834E8, sizeof Battle_shortcutHudAnimationStep, &Battle_shortcutHudAnimationStep, sizeof Battle_shortcutHudAnimationStep}, // G00195 D_800EBCE8
    {"BATTLE/BATTLE.PRG", 0x834EC, sizeof Battle_limbDisplaySpriteOffsets, &Battle_limbDisplaySpriteOffsets, sizeof Battle_limbDisplaySpriteOffsets}, // G00196 D_800EBCEC
    {"BATTLE/BATTLE.PRG", 0x83514, sizeof Battle_limbDisplaySpriteSizes, &Battle_limbDisplaySpriteSizes, sizeof Battle_limbDisplaySpriteSizes}, // G00197 D_800EBD14
    {"BATTLE/BATTLE.PRG", 0x8353C, sizeof Battle_limbDisplaySpriteTexCoords, &Battle_limbDisplaySpriteTexCoords, sizeof Battle_limbDisplaySpriteTexCoords}, // G00198 D_800EBD3C
    {"BATTLE/BATTLE.PRG", 0x83564, sizeof Battle_battleModeIndicatorWobble, &Battle_battleModeIndicatorWobble, sizeof Battle_battleModeIndicatorWobble}, // G00199 D_800EBD64
    {"BATTLE/BATTLE.PRG", 0x83568, sizeof Menu_Framework_abilityGroupActionRanges, &Menu_Framework_abilityGroupActionRanges, sizeof Menu_Framework_abilityGroupActionRanges}, // G00200 D_800EBD68
    {"BATTLE/BATTLE.PRG", 0x83570, sizeof Menu_Magic_warlockSpellIds, &Menu_Magic_warlockSpellIds, sizeof Menu_Magic_warlockSpellIds}, // G00201 vs_battle_warlockSpellIds
    {"BATTLE/BATTLE.PRG", 0x83584, sizeof Menu_Magic_shamanSpellIds, &Menu_Magic_shamanSpellIds, sizeof Menu_Magic_shamanSpellIds}, // G00202 vs_battle_shamanSpellIds
    {"BATTLE/BATTLE.PRG", 0x8358C, sizeof Menu_Magic_sorcererSpellIds, &Menu_Magic_sorcererSpellIds, sizeof Menu_Magic_sorcererSpellIds}, // G00203 vs_battle_sorcererSpellIds
    {"BATTLE/BATTLE.PRG", 0x835A0, sizeof Menu_Magic_enchanterSpellIds, &Menu_Magic_enchanterSpellIds, sizeof Menu_Magic_enchanterSpellIds}, // G00204 vs_battle_enchanterSpellIds
    {"BATTLE/BATTLE.PRG", 0x835A8, sizeof Battle_spellClassCounts, &Battle_spellClassCounts, sizeof Battle_spellClassCounts}, // G00205 _spellClassCounts
    {"BATTLE/BATTLE.PRG", 0x835BC, sizeof Battle_chainAbilityActionIds, &Battle_chainAbilityActionIds, sizeof Battle_chainAbilityActionIds}, // G00207 vs_battle_chainAbilityOffsets
    {"BATTLE/BATTLE.PRG", 0x835CC, sizeof Battle_defenseAbilityActionIds, &Battle_defenseAbilityActionIds, sizeof Battle_defenseAbilityActionIds}, // G00208 vs_battle_defenseAbilityOffsets
    {"BATTLE/BATTLE.PRG", 0x835DC, sizeof Battle_doorStrings, &Battle_doorStrings, sizeof Battle_doorStrings}, // G00209 D_800EBDDC
    {"BATTLE/BATTLE.PRG", 0x83758, sizeof Menu_kNotificationItemNames, &Menu_kNotificationItemNames, sizeof Menu_kNotificationItemNames}, // G00210 D_800EBF58
    {"BATTLE/BATTLE.PRG", 0x83A6C, sizeof Core_interfaceRandomState, &Core_interfaceRandomState, sizeof Core_interfaceRandomState}, // G00213 _keystreamState
    {"BATTLE/BATTLE.PRG", 0x83A70, sizeof Menu_Framework_cursorBrightnessAnimation, &Menu_Framework_cursorBrightnessAnimation, sizeof Menu_Framework_cursorBrightnessAnimation}, // G00214 vs_battle_cursorBrightnessAnimation
    {"BATTLE/BATTLE.PRG", 0x83A80, sizeof Render_legacyUploadRectCursor, &Render_legacyUploadRectCursor, sizeof Render_legacyUploadRectCursor}, // G00215 D_800EC280
    {"BATTLE/BATTLE.PRG", 0x83A84, sizeof Menu_textBoxAnchors, &Menu_textBoxAnchors, sizeof Menu_textBoxAnchors}, // G00216 D_800EC284
    {"BATTLE/BATTLE.PRG", 0x83ACC, sizeof Menu_informationHeaderPositions, &Menu_informationHeaderPositions, sizeof Menu_informationHeaderPositions}, // G00217 D_800EC2CC
    {"BATTLE/BATTLE.PRG", 0x83AD8, sizeof Menu_kInformationHeaderSpriteWords, &Menu_kInformationHeaderSpriteWords, sizeof Menu_kInformationHeaderSpriteWords}, // G00218 D_800EC2D8
    {"BATTLE/BATTLE.PRG", 0x83AE4, sizeof Effects_effectBufferIndex, &Effects_effectBufferIndex, sizeof Effects_effectBufferIndex}, // G00219 D_800EC2E4
    {"BATTLE/BATTLE.PRG", 0x83AE8, sizeof Effects_trailAdditiveColors, &Effects_trailAdditiveColors, sizeof Effects_trailAdditiveColors}, // G00220 D_800EC2E8
    {"BATTLE/BATTLE.PRG", 0x83B08, sizeof Effects_trailColors, &Effects_trailColors, sizeof Effects_trailColors}, // G00221 D_800EC308
    {"BATTLE/BATTLE.PRG", 0x83B28, sizeof Effects_emissionCounter, &Effects_emissionCounter, sizeof Effects_emissionCounter}, // G00223 D_800EC328
    {"BATTLE/BATTLE.PRG", 0x83B2C, sizeof Effects_actionVariantValues, &Effects_actionVariantValues, sizeof Effects_actionVariantValues}, // G00224 D_800EC32C
    {"BATTLE/BATTLE.PRG", 0x83B30, sizeof Effects_pairedParticleColorPresets, &Effects_pairedParticleColorPresets, sizeof Effects_pairedParticleColorPresets}, // G00225 D_800EC330
    {"BATTLE/BATTLE.PRG", 0x83B68, sizeof Effects_particleColorPresetTable, &Effects_particleColorPresetTable, sizeof Effects_particleColorPresetTable}, // G00226 D_800EC368
    {"BATTLE/BATTLE.PRG", 0x83CB8, sizeof Effects_lastReactionSoundVariant, &Effects_lastReactionSoundVariant, sizeof Effects_lastReactionSoundVariant}, // G00228 D_800EC4B8
    {"BATTLE/BATTLE.PRG", 0x83CBC, sizeof Effects_emptyPackage, &Effects_emptyPackage, sizeof Effects_emptyPackage}, // G00229 D_800EC4BC
    {"BATTLE/BATTLE.PRG", 0x88EEC, sizeof Battle_Ai_directionDeltas, &Battle_Ai_directionDeltas, sizeof Battle_Ai_directionDeltas}, // G00230 D_800F16EC
    {"BATTLE/BATTLE.PRG", 0x88EF4, sizeof Battle_Ai_frameTickBudget, &Battle_Ai_frameTickBudget, sizeof Battle_Ai_frameTickBudget}, // G00231 D_800F16F4
    {"BATTLE/BATTLE.PRG", 0x88F5C, sizeof Battle_Ai_routeStepDirections, &Battle_Ai_routeStepDirections, sizeof Battle_Ai_routeStepDirections}, // G00232 D_800F175C
    {"BATTLE/BATTLE.PRG", 0x88FB8, sizeof Battle_Ai_viewAngles, &Battle_Ai_viewAngles, sizeof Battle_Ai_viewAngles}, // G00234 D_800F17B8
    {"BATTLE/BATTLE.PRG", 0x88FC8, sizeof Battle_Ai_settingsBits8Values, &Battle_Ai_settingsBits8Values, sizeof Battle_Ai_settingsBits8Values}, // G00235 D_800F17C8
    {"BATTLE/BATTLE.PRG", 0x88FD8, sizeof Battle_Ai_facingDelayScales, &Battle_Ai_facingDelayScales, sizeof Battle_Ai_facingDelayScales}, // G00236 D_800F17D8
    {"BATTLE/BATTLE.PRG", 0x88FDC, sizeof Battle_Ai_radialRetreatPath, &Battle_Ai_radialRetreatPath, sizeof Battle_Ai_radialRetreatPath}, // G00237 D_800F17DC
    {"EFFECT/PLG045.BIN", 0x1DB4, sizeof Effects_Plg045_particleEmissionCounter, &Effects_Plg045_particleEmissionCounter, sizeof Effects_Plg045_particleEmissionCounter}, // G00634 D_800FB5B4
    {"EFFECT/PLG049.BIN", 0x4BD4, sizeof Effects_Plg049_trailEmissionCounter, &Effects_Plg049_trailEmissionCounter, sizeof Effects_Plg049_trailEmissionCounter}, // G00637 D_800FE3D4
    {"EFFECT/PLG049.BIN", 0x4BF8, sizeof Effects_Plg049_tiledRingColorFactors, &Effects_Plg049_tiledRingColorFactors, sizeof Effects_Plg049_tiledRingColorFactors}, // G00639 D_800FE3F8
    {"EFFECT/PLG050.BIN", 0x2E44, sizeof Effects_Plg050_trailEmissionCounter, &Effects_Plg050_trailEmissionCounter, sizeof Effects_Plg050_trailEmissionCounter}, // G00641 D_800FC644
    {"EFFECT/PLG057.BIN", 0x61B4, sizeof Effects_Plg057_particleEmissionCounter, &Effects_Plg057_particleEmissionCounter, sizeof Effects_Plg057_particleEmissionCounter}, // G00645 D_800FF9B4
    {"EFFECT/PLG057.BIN", 0x61B8, sizeof Effects_Plg057_trailEmissionCounter, &Effects_Plg057_trailEmissionCounter, sizeof Effects_Plg057_trailEmissionCounter}, // G00646 D_800FF9B8
    {"EFFECT/PLG057.BIN", 0x61BC, sizeof Effects_Plg057_trailEmissionCounter2, &Effects_Plg057_trailEmissionCounter2, sizeof Effects_Plg057_trailEmissionCounter2}, // G00647 D_800FF9BC
    {"EFFECT/PLG061.BIN", 0x3290, sizeof Effects_Plg061_surfaceVertices, &Effects_Plg061_surfaceVertices, sizeof Effects_Plg061_surfaceVertices}, // G00648 D_800FCA90
    {"EFFECT/PLG065.BIN", 0x4174, sizeof Effects_Plg065_particleEmissionCounter, &Effects_Plg065_particleEmissionCounter, sizeof Effects_Plg065_particleEmissionCounter}, // G00653 D_800FD974
    {"EFFECT/PLG069.BIN", 0x3C24, sizeof Effects_Plg069_mesh10Vertices, &Effects_Plg069_mesh10Vertices, sizeof Effects_Plg069_mesh10Vertices}, // G00654 D_800FD424
    {"EFFECT/PLG069.BIN", 0x3C74, sizeof Effects_Plg069_mesh10Faces, &Effects_Plg069_mesh10Faces, sizeof Effects_Plg069_mesh10Faces}, // G00655 D_800FD474
    {"EFFECT/PLG069.BIN", 0x3CD4, sizeof Effects_Plg069_trailEmissionCounter, &Effects_Plg069_trailEmissionCounter, sizeof Effects_Plg069_trailEmissionCounter}, // G00656 D_800FD4D4
    {"EFFECT/PLG069.BIN", 0x3CD8, sizeof Effects_Plg069_tiledRingColorFactors, &Effects_Plg069_tiledRingColorFactors, sizeof Effects_Plg069_tiledRingColorFactors}, // G00657 D_800FD4D8
    {"EFFECT/PLG073.BIN", 0x573C, sizeof Effects_Plg073_tiledRingColorFactors, &Effects_Plg073_tiledRingColorFactors, sizeof Effects_Plg073_tiledRingColorFactors}, // G00658 D_800FEF3C
    {"EFFECT/PLG073.BIN", 0x577C, sizeof Effects_Plg073_trailEmissionCounter, &Effects_Plg073_trailEmissionCounter, sizeof Effects_Plg073_trailEmissionCounter}, // G00660 D_800FEF7C
    {"EFFECT/PLG077.BIN", 0x4108, sizeof Effects_Plg077_mesh10Vertices, &Effects_Plg077_mesh10Vertices, sizeof Effects_Plg077_mesh10Vertices}, // G00661 D_800FD908
    {"EFFECT/PLG077.BIN", 0x4158, sizeof Effects_Plg077_mesh10Faces, &Effects_Plg077_mesh10Faces, sizeof Effects_Plg077_mesh10Faces}, // G00662 D_800FD958
    {"EFFECT/PLG077.BIN", 0x41D8, sizeof Effects_Plg077_surfaceVertices, &Effects_Plg077_surfaceVertices, sizeof Effects_Plg077_surfaceVertices}, // G00664 D_800FD9D8
    {"EFFECT/PLG082.BIN", 0x3888, sizeof Effects_Plg082_particleEmissionCounter, &Effects_Plg082_particleEmissionCounter, sizeof Effects_Plg082_particleEmissionCounter}, // G00668 D_800FD088
    {"EFFECT/PLG085.BIN", 0x1A60, sizeof Effects_Plg085_tiledRingColorFactors, &Effects_Plg085_tiledRingColorFactors, sizeof Effects_Plg085_tiledRingColorFactors}, // G00670 D_800FB260
    {"EFFECT/PLG086.BIN", 0x4BB0, sizeof Effects_Plg086_trailEmissionCounter, &Effects_Plg086_trailEmissionCounter, sizeof Effects_Plg086_trailEmissionCounter}, // G00672 D_800FE3B0
    {"EFFECT/PLG087.BIN", 0x5F5C, sizeof Effects_Plg087_surfaceVertices, &Effects_Plg087_surfaceVertices, sizeof Effects_Plg087_surfaceVertices}, // G00673 D_800FF75C
    {"EFFECT/PLG087.BIN", 0x61FC, sizeof Effects_Plg087_trailEmissionCounter, &Effects_Plg087_trailEmissionCounter, sizeof Effects_Plg087_trailEmissionCounter}, // G00675 D_800FF9FC
    {"EFFECT/PLG087.BIN", 0x6200, sizeof Effects_Plg087_particleEmissionCounter, &Effects_Plg087_particleEmissionCounter, sizeof Effects_Plg087_particleEmissionCounter}, // G00676 D_800FFA00
    {"EFFECT/PLG114.BIN", 0x34A0, sizeof Effects_Plg114_particleEmissionCounter, &Effects_Plg114_particleEmissionCounter, sizeof Effects_Plg114_particleEmissionCounter}, // G00677 D_800FCCA0
    {"EFFECT/PLG114.BIN", 0x34A4, sizeof Effects_Plg114_particleEmissionCounter2, &Effects_Plg114_particleEmissionCounter2, sizeof Effects_Plg114_particleEmissionCounter2}, // G00678 D_800FCCA4
    {"EFFECT/PLG128.BIN", 0x6CF0, sizeof Effects_Plg128_surfaceVertices, &Effects_Plg128_surfaceVertices, sizeof Effects_Plg128_surfaceVertices}, // G00679 D_801004F0
    {"EFFECT/PLG128.BIN", 0x6F90, sizeof Effects_Plg128_surfaceVertices2, &Effects_Plg128_surfaceVertices2, sizeof Effects_Plg128_surfaceVertices2}, // G00681 D_80100790
    {"EFFECT/PLG128.BIN", 0x7210, sizeof Effects_Plg128_trailEmissionCounter, &Effects_Plg128_trailEmissionCounter, sizeof Effects_Plg128_trailEmissionCounter}, // G00682 D_80100A10
    {"EFFECT/PLG129.BIN", 0x3DE8, sizeof Effects_Plg129_mesh10Vertices, &Effects_Plg129_mesh10Vertices, sizeof Effects_Plg129_mesh10Vertices}, // G00684 D_800FD5E8
    {"EFFECT/PLG129.BIN", 0x3E38, sizeof Effects_Plg129_mesh10Faces, &Effects_Plg129_mesh10Faces, sizeof Effects_Plg129_mesh10Faces}, // G00685 D_800FD638
    {"EFFECT/PLG129.BIN", 0x3E98, sizeof Effects_Plg129_surfaceVertices, &Effects_Plg129_surfaceVertices, sizeof Effects_Plg129_surfaceVertices}, // G00686 D_800FD698
    {"EFFECT/PLG134.BIN", 0x316C, sizeof Effects_Plg134_trailEmissionCounter, &Effects_Plg134_trailEmissionCounter, sizeof Effects_Plg134_trailEmissionCounter}, // G00688 D_800FC96C
    {"EFFECT/PLG135.BIN", 0x318C, sizeof Effects_Plg135_trailEmissionCounter, &Effects_Plg135_trailEmissionCounter, sizeof Effects_Plg135_trailEmissionCounter}, // G00691 D_800FC98C
    {"EFFECT/PLG137.BIN", 0x478C, sizeof Effects_Plg137_mesh10Vertices, &Effects_Plg137_mesh10Vertices, sizeof Effects_Plg137_mesh10Vertices}, // G00692 D_800FDF8C
    {"EFFECT/PLG137.BIN", 0x47DC, sizeof Effects_Plg137_mesh10Faces, &Effects_Plg137_mesh10Faces, sizeof Effects_Plg137_mesh10Faces}, // G00693 D_800FDFDC
    {"EFFECT/PLG137.BIN", 0x483C, sizeof Effects_Plg137_trailEmissionCounter, &Effects_Plg137_trailEmissionCounter, sizeof Effects_Plg137_trailEmissionCounter}, // G00694 D_800FE03C
    {"EFFECT/PLG137.BIN", 0x4840, sizeof Effects_Plg137_tiledRingColorFactors, &Effects_Plg137_tiledRingColorFactors, sizeof Effects_Plg137_tiledRingColorFactors}, // G00695 D_800FE040
    {"EFFECT/PLG138.BIN", 0x355C, sizeof Effects_Plg138_tiledRingColorFactors, &Effects_Plg138_tiledRingColorFactors, sizeof Effects_Plg138_tiledRingColorFactors}, // G00697 D_800FCD5C
    {"EFFECT/PLG141.BIN", 0x2200, sizeof Effects_Plg141_trailEmissionCounter, &Effects_Plg141_trailEmissionCounter, sizeof Effects_Plg141_trailEmissionCounter}, // G00698 D_800FBA00
    {"EFFECT/PLG143.BIN", 0x1E64, sizeof Effects_Plg143_tiledRingColorFactors, &Effects_Plg143_tiledRingColorFactors, sizeof Effects_Plg143_tiledRingColorFactors}, // G00701 D_800FB664
    {"EFFECT/PLG146.BIN", 0x607C, sizeof Effects_Plg146_tiledRingColorFactors, &Effects_Plg146_tiledRingColorFactors, sizeof Effects_Plg146_tiledRingColorFactors}, // G00703 D_800FF87C
    {"EFFECT/PLG146.BIN", 0x609C, sizeof Effects_Plg146_mirrorSurfaceVertices, &Effects_Plg146_mirrorSurfaceVertices, sizeof Effects_Plg146_mirrorSurfaceVertices}, // G00704 D_800FF89C
    {"EFFECT/PLG146.BIN", 0x631C, sizeof Effects_Plg146_particleEmissionCounter, &Effects_Plg146_particleEmissionCounter, sizeof Effects_Plg146_particleEmissionCounter}, // G00705 D_800FFB1C
    {"EFFECT/PLG146.BIN", 0x6340, sizeof Effects_Plg146_trailEmissionCounter, &Effects_Plg146_trailEmissionCounter, sizeof Effects_Plg146_trailEmissionCounter}, // G00707 D_800FFB40
    {"EFFECT/PLG147.BIN", 0x2330, sizeof Effects_Plg147_particleEmissionCounter, &Effects_Plg147_particleEmissionCounter, sizeof Effects_Plg147_particleEmissionCounter}, // G00709 D_800FBB30
    {"EFFECT/PLG148.BIN", 0x30F8, sizeof Effects_Plg148_trailEmissionCounter, &Effects_Plg148_trailEmissionCounter, sizeof Effects_Plg148_trailEmissionCounter}, // G00710 D_800FC8F8
    {"EFFECT/PLG148.BIN", 0x30FC, sizeof Effects_Plg148_tiledRingColorFactors, &Effects_Plg148_tiledRingColorFactors, sizeof Effects_Plg148_tiledRingColorFactors}, // G00711 D_800FC8FC
    {"EFFECT/PLG150.BIN", 0x321C, sizeof Effects_Plg150_tiledRingColorFactors, &Effects_Plg150_tiledRingColorFactors, sizeof Effects_Plg150_tiledRingColorFactors}, // G00713 D_800FCA1C
    {"EFFECT/PLG150.BIN", 0x323C, sizeof Effects_Plg150_surfaceVertices, &Effects_Plg150_surfaceVertices, sizeof Effects_Plg150_surfaceVertices}, // G00714 D_800FCA3C
    {"EFFECT/PLG153.BIN", 0x3504, sizeof Effects_Plg153_mesh10Vertices, &Effects_Plg153_mesh10Vertices, sizeof Effects_Plg153_mesh10Vertices}, // G00716 D_800FCD04
    {"EFFECT/PLG153.BIN", 0x3554, sizeof Effects_Plg153_mesh10Faces, &Effects_Plg153_mesh10Faces, sizeof Effects_Plg153_mesh10Faces}, // G00717 D_800FCD54
    {"EFFECT/PLG153.BIN", 0x35D4, sizeof Effects_Plg153_particleEmissionCounter, &Effects_Plg153_particleEmissionCounter, sizeof Effects_Plg153_particleEmissionCounter}, // G00719 D_800FCDD4
    {"EFFECT/PLG154.BIN", 0x1A9C, sizeof Effects_Plg154_mesh10Vertices, &Effects_Plg154_mesh10Vertices, sizeof Effects_Plg154_mesh10Vertices}, // G00720 D_800FB29C
    {"EFFECT/PLG154.BIN", 0x1AEC, sizeof Effects_Plg154_mesh10Faces, &Effects_Plg154_mesh10Faces, sizeof Effects_Plg154_mesh10Faces}, // G00721 D_800FB2EC
    {"EFFECT/PLG156.BIN", 0x291C, sizeof Effects_Plg156_particleEmissionCounter, &Effects_Plg156_particleEmissionCounter, sizeof Effects_Plg156_particleEmissionCounter}, // G00723 D_800FC11C
    {"EFFECT/PLG158.BIN", 0x3638, sizeof Effects_Plg158_surfaceVertices, &Effects_Plg158_surfaceVertices, sizeof Effects_Plg158_surfaceVertices}, // G00724 D_800FCE38
    {"EFFECT/PLG158.BIN", 0x38B8, sizeof Effects_Plg158_mirrorSurfaceVertices, &Effects_Plg158_mirrorSurfaceVertices, sizeof Effects_Plg158_mirrorSurfaceVertices}, // G00725 D_800FD0B8
    {"EFFECT/PLG160.BIN", 0x1B1C, sizeof Effects_Plg160_tiledRingColorFactors, &Effects_Plg160_tiledRingColorFactors, sizeof Effects_Plg160_tiledRingColorFactors}, // G00727 D_800FB31C
    {"EFFECT/PLG161.BIN", 0x5488, sizeof Effects_Plg161_trailEmissionCounter, &Effects_Plg161_trailEmissionCounter, sizeof Effects_Plg161_trailEmissionCounter}, // G00730 D_800FEC88
    {"EFFECT/PLG165.BIN", 0x4B8C, sizeof Effects_Plg165_trailEmissionCounter, &Effects_Plg165_trailEmissionCounter, sizeof Effects_Plg165_trailEmissionCounter}, // G00735 D_800FE38C
    {"EFFECT/PLG166.BIN", 0x36C0, sizeof Effects_Plg166_mirrorSurfaceVertices, &Effects_Plg166_mirrorSurfaceVertices, sizeof Effects_Plg166_mirrorSurfaceVertices}, // G00736 D_800FCEC0
    {"EFFECT/PLG167.BIN", 0x4044, sizeof Effects_Plg167_trailEmissionCounter, &Effects_Plg167_trailEmissionCounter, sizeof Effects_Plg167_trailEmissionCounter}, // G00740 D_800FD844
    {"EFFECT/PLG168.BIN", 0x4454, sizeof Effects_Plg168_trailEmissionCounter, &Effects_Plg168_trailEmissionCounter, sizeof Effects_Plg168_trailEmissionCounter}, // G00741 D_800FDC54
    {"EFFECT/PLG168.BIN", 0x4458, sizeof Effects_Plg168_particleEmissionCounter, &Effects_Plg168_particleEmissionCounter, sizeof Effects_Plg168_particleEmissionCounter}, // G00742 D_800FDC58
    {"EFFECT/PLG169.BIN", 0x3D90, sizeof Effects_Plg169_trailEmissionCounter, &Effects_Plg169_trailEmissionCounter, sizeof Effects_Plg169_trailEmissionCounter}, // G00743 D_800FD590
    {"EFFECT/PLG170.BIN", 0x4DC4, sizeof Effects_Plg170_mirrorSurfaceVertices, &Effects_Plg170_mirrorSurfaceVertices, sizeof Effects_Plg170_mirrorSurfaceVertices}, // G00746 D_800FE5C4
    {"EFFECT/PLG170.BIN", 0x5064, sizeof Effects_Plg170_surfaceVertices, &Effects_Plg170_surfaceVertices, sizeof Effects_Plg170_surfaceVertices}, // G00748 D_800FE864
    {"EFFECT/PLG224.BIN", 0x16EC, sizeof Effects_Plg224_particleEmissionCounter, &Effects_Plg224_particleEmissionCounter, sizeof Effects_Plg224_particleEmissionCounter}, // G00750 D_800FAEEC
    {"EFFECT/PLG232.BIN", 0x2200, sizeof Effects_Plg232_trailEmissionCounter, &Effects_Plg232_trailEmissionCounter, sizeof Effects_Plg232_trailEmissionCounter}, // G00751 D_800FBA00
    {"EFFECT/PLG242.BIN", 0x3C60, sizeof Effects_Plg242_tiledRingColorFactors, &Effects_Plg242_tiledRingColorFactors, sizeof Effects_Plg242_tiledRingColorFactors}, // G00755 D_800FD460
    {"EFFECT/PLG242.BIN", 0x3C80, sizeof Effects_Plg242_trailEmissionCounter, &Effects_Plg242_trailEmissionCounter, sizeof Effects_Plg242_trailEmissionCounter}, // G00756 D_800FD480
    {"EFFECT/PLG255.BIN", 0x38EC, sizeof Effects_Plg255_trailEmissionCounter, &Effects_Plg255_trailEmissionCounter, sizeof Effects_Plg255_trailEmissionCounter}, // G00758 D_800FD0EC
    {"EFFECT/PLG255.BIN", 0x38F0, sizeof Effects_Plg255_particleEmissionCounter, &Effects_Plg255_particleEmissionCounter, sizeof Effects_Plg255_particleEmissionCounter}, // G00759 D_800FD0F0
    {"EFFECT/PLG260.BIN", 0xB2C, sizeof Effects_Plg260_mesh10Vertices, &Effects_Plg260_mesh10Vertices, sizeof Effects_Plg260_mesh10Vertices}, // G00760 D_800FA32C
    {"EFFECT/PLG260.BIN", 0xB7C, sizeof Effects_Plg260_mesh10Faces, &Effects_Plg260_mesh10Faces, sizeof Effects_Plg260_mesh10Faces}, // G00761 D_800FA37C
    {"EFFECT/PLG263.BIN", 0x44B0, sizeof Effects_Plg263_surfaceVertices, &Effects_Plg263_surfaceVertices, sizeof Effects_Plg263_surfaceVertices}, // G00762 D_800FDCB0
    {"EFFECT/PLG263.BIN", 0x4770, sizeof Effects_Plg263_particleEmissionCounter, &Effects_Plg263_particleEmissionCounter, sizeof Effects_Plg263_particleEmissionCounter}, // G00765 D_800FDF70
    {"EFFECT/PLG265.BIN", 0x6598, sizeof Effects_Plg265_tiledRingColorFactors, &Effects_Plg265_tiledRingColorFactors, sizeof Effects_Plg265_tiledRingColorFactors}, // G00766 D_800FFD98
    {"EFFECT/PLG265.BIN", 0x65B8, sizeof Effects_Plg265_particleEmissionCounter, &Effects_Plg265_particleEmissionCounter, sizeof Effects_Plg265_particleEmissionCounter}, // G00767 D_800FFDB8
    {"EFFECT/PLG265.BIN", 0x65BC, sizeof Effects_Plg265_trailEmissionCounter, &Effects_Plg265_trailEmissionCounter, sizeof Effects_Plg265_trailEmissionCounter}, // G00768 D_800FFDBC
    {"EFFECT/PLG265.BIN", 0x65C0, sizeof Effects_Plg265_particleEmissionCounter2, &Effects_Plg265_particleEmissionCounter2, sizeof Effects_Plg265_particleEmissionCounter2}, // G00769 D_800FFDC0
    {"EFFECT/PLG266.BIN", 0x56A0, sizeof Effects_Plg266_particleEmissionCounter, &Effects_Plg266_particleEmissionCounter, sizeof Effects_Plg266_particleEmissionCounter}, // G00770 D_800FEEA0
    {"EFFECT/PLG266.BIN", 0x56A4, sizeof Effects_Plg266_particleEmissionCounter2, &Effects_Plg266_particleEmissionCounter2, sizeof Effects_Plg266_particleEmissionCounter2}, // G00771 D_800FEEA4
    {"EFFECT/PLG266.BIN", 0x56A8, sizeof Effects_Plg266_trailEmissionCounter, &Effects_Plg266_trailEmissionCounter, sizeof Effects_Plg266_trailEmissionCounter}, // G00772 D_800FEEA8
    {"EFFECT/PLG272.BIN", 0x2690, sizeof Effects_Plg272_surfaceVertices, &Effects_Plg272_surfaceVertices, sizeof Effects_Plg272_surfaceVertices}, // G00774 D_800FBE90
    {"EFFECT/PLG273.BIN", 0xC74, sizeof Effects_Plg273_mirrorSurfaceVertices, &Effects_Plg273_mirrorSurfaceVertices, sizeof Effects_Plg273_mirrorSurfaceVertices}, // G00775 D_800FA474
    {"EFFECT/PLG291.BIN", 0x3CD8, sizeof Effects_Plg291_trailEmissionCounter, &Effects_Plg291_trailEmissionCounter, sizeof Effects_Plg291_trailEmissionCounter}, // G00776 D_800FD4D8
    {"EFFECT/PLG297.BIN", 0x3370, sizeof Effects_Plg297_mirrorSurfaceVertices, &Effects_Plg297_mirrorSurfaceVertices, sizeof Effects_Plg297_mirrorSurfaceVertices}, // G00778 D_800FCB70
    {"EFFECT/PLG300.BIN", 0x48CC, sizeof Effects_Plg300_mirrorSurfaceVertices, &Effects_Plg300_mirrorSurfaceVertices, sizeof Effects_Plg300_mirrorSurfaceVertices}, // G00781 D_800FE0CC
    {"EFFECT/PLG300.BIN", 0x4B4C, sizeof Effects_Plg300_particleEmissionCounter, &Effects_Plg300_particleEmissionCounter, sizeof Effects_Plg300_particleEmissionCounter}, // G00782 D_800FE34C
    {"EFFECT/PLG301.BIN", 0x2D24, sizeof Effects_Plg301_particleEmissionCounter, &Effects_Plg301_particleEmissionCounter, sizeof Effects_Plg301_particleEmissionCounter}, // G00785 D_800FC524
    {"ENDING/ENDING.PRG", 0x5BFC, sizeof Ending_Credits_scriptBytes, &Ending_Credits_scriptBytes, sizeof Ending_Credits_scriptBytes}, // G00787 D_8006E3FC
    {"ENDING/ENDING.PRG", 0x777C, sizeof Ending_Credits_glyphWidths, &Ending_Credits_glyphWidths, sizeof Ending_Credits_glyphWidths}, // G00788 _glyphWidths
    {"ENDING/ENDING.PRG", 0x785C, sizeof Ending_Credits_embeddedTextureOne, &Ending_Credits_embeddedTextureOne, sizeof Ending_Credits_embeddedTextureOne}, // G00789 D_8007005C
    {"ENDING/ENDING.PRG", 0xE89C, sizeof Ending_Credits_embeddedTextureEight, &Ending_Credits_embeddedTextureEight, sizeof Ending_Credits_embeddedTextureEight}, // G00790 D_8007709C
    {"ENDING/ENDING.PRG", 0x202BC, sizeof Ending_Credits_embeddedTextureNine, &Ending_Credits_embeddedTextureNine, sizeof Ending_Credits_embeddedTextureNine}, // G00791 D_80088ABC
    {"ENDING/ENDING.PRG", 0x31CDC, sizeof Ending_Credits_embeddedTextureTen, &Ending_Credits_embeddedTextureTen, sizeof Ending_Credits_embeddedTextureTen}, // G00792 D_8009A4DC
    {"ENDING/ENDING.PRG", 0x436FC, sizeof Ending_Credits_embeddedTextureEleven, &Ending_Credits_embeddedTextureEleven, sizeof Ending_Credits_embeddedTextureEleven}, // G00793 D_800ABEFC
    {"ENDING/ENDING.PRG", 0x5511C, sizeof Ending_Credits_embeddedTextureTwelve, &Ending_Credits_embeddedTextureTwelve, sizeof Ending_Credits_embeddedTextureTwelve}, // G00794 D_800BD91C
    {"ENDING/ENDING.PRG", 0x66B3C, sizeof Ending_Credits_embeddedTextureTwo, &Ending_Credits_embeddedTextureTwo, sizeof Ending_Credits_embeddedTextureTwo}, // G00795 D_800CF33C
    {"ENDING/ENDING.PRG", 0x6F77C, sizeof Ending_Credits_embeddedTextureThree, &Ending_Credits_embeddedTextureThree, sizeof Ending_Credits_embeddedTextureThree}, // G00796 D_800D7F7C
    {"ENDING/ENDING.PRG", 0x6FB7C, sizeof Ending_Credits_embeddedTextureFive, &Ending_Credits_embeddedTextureFive, sizeof Ending_Credits_embeddedTextureFive}, // G00797 D_800D837C
    {"ENDING/ENDING.PRG", 0x70F5C, sizeof Ending_Credits_embeddedTextureFour, &Ending_Credits_embeddedTextureFour, sizeof Ending_Credits_embeddedTextureFour}, // G00798 D_800D975C
    {"ENDING/ENDING.PRG", 0x7289C, sizeof Ending_Credits_embeddedTextureSix, &Ending_Credits_embeddedTextureSix, sizeof Ending_Credits_embeddedTextureSix}, // G00799 D_800DB09C
    {"ENDING/ENDING.PRG", 0x72A6C, sizeof Ending_Credits_embeddedTextureSeven, &Ending_Credits_embeddedTextureSeven, sizeof Ending_Credits_embeddedTextureSeven}, // G00800 D_800DB26C
    {"ENDING/ENDING.PRG", 0x72F2C, sizeof Ending_Credits_illustrationDecodePhase, &Ending_Credits_illustrationDecodePhase, sizeof Ending_Credits_illustrationDecodePhase}, // G00801 D_800DB72C
    {"ENDING/ENDING.PRG", 0x72FB4, sizeof Ending_Credits_illustrationHoldFrames, &Ending_Credits_illustrationHoldFrames, sizeof Ending_Credits_illustrationHoldFrames}, // G00803 _illustrationTimers
    {"ENDING/ENDING.PRG", 0x72FD4, sizeof Ending_countableRoomBits, &Ending_countableRoomBits, sizeof Ending_countableRoomBits}, // G00804 _countableRooms
    {"ENDING/ENDING.PRG", 0x73014, sizeof Ending_resultAtlasRegions, &Ending_resultAtlasRegions, sizeof Ending_resultAtlasRegions}, // G00805 _disMap
    {"ENDING/ENDING.PRG", 0x73014, sizeof Render_resultAtlasRegionTemplates, &Render_resultAtlasRegionTemplates, sizeof Render_resultAtlasRegionTemplates}, // G00806 _disMap
    {"ENDING/ENDING.PRG", 0x73076, sizeof Ending_kRankHeadingWidth, &Ending_kRankHeadingWidth, sizeof Ending_kRankHeadingWidth}, // G00807 D_800DB876
    {"ENDING/ENDING.PRG", 0x7323C, sizeof Ending_rankLabelRegionIds, &Ending_rankLabelRegionIds, sizeof Ending_rankLabelRegionIds}, // G00808 _riskbreakerRanks
    {"ENDING/ENDING.PRG", 0x7327C, sizeof Ending_congratulationsTint, &Ending_congratulationsTint, sizeof Ending_congratulationsTint}, // G00809 D_800DBA7C
    {"ENDING/ENDING.PRG", 0x73288, sizeof Ending_scoreTint, &Ending_scoreTint, sizeof Ending_scoreTint}, // G00810 D_800DBA88
    {"ENDING/ENDING.PRG", 0x73290, sizeof Ending_countingScoreTint, &Ending_countingScoreTint, sizeof Ending_countingScoreTint}, // G00811 D_800DBA90
    {"ENDING/ENDING.PRG", 0x73298, sizeof Ending_mapCompletionTint, &Ending_mapCompletionTint, sizeof Ending_mapCompletionTint}, // G00812 D_800DBA98
    {"ENDING/ENDING.PRG", 0x732A0, sizeof Ending_countingMapCompletionTint, &Ending_countingMapCompletionTint, sizeof Ending_countingMapCompletionTint}, // G00813 D_800DBAA0
    {"ENDING/ENDING.PRG", 0x732A8, sizeof Ending_rankHeadingTint, &Ending_rankHeadingTint, sizeof Ending_rankHeadingTint}, // G00814 D_800DBAA8
    {"ENDING/ENDING.PRG", 0x732B0, sizeof Ending_rankLabelTint, &Ending_rankLabelTint, sizeof Ending_rankLabelTint}, // G00815 D_800DBAB0
    {"ENDING/ENDING.PRG", 0x732B8, sizeof Ending_rankTitleThresholds, &Ending_rankTitleThresholds, sizeof Ending_rankTitleThresholds}, // G00816 titleThresholds
    {"ENDING/ENDING.PRG", 0x732D8, sizeof Ending_rankScoreThresholds, &Ending_rankScoreThresholds, sizeof Ending_rankScoreThresholds}, // G00817 rankScores
    {"MENU/MENU0.PRG", 0x2258, 4924u, &Menu_Magic_magicStrings, sizeof Menu_Magic_magicStrings}, // G00978 _magicStrings
    {"MENU/MENU0.PRG", 0x3594, 3197u, &Menu_Magic_teleportationStrings, sizeof Menu_Magic_teleportationStrings}, // G00980 _teleportationStrings
    {"MENU/MENU1.PRG", 0xC78, 4302u, &Menu_BreakArts_strings, sizeof Menu_BreakArts_strings}, // G01004 _strings
    {"MENU/MENU2.PRG", 0x1E90, 2120u, &Menu_BattleAbilities_battleAbilityStrings, sizeof Menu_BattleAbilities_battleAbilityStrings}, // G01025 _battleAbilityStrings
    {"MENU/MENU2.PRG", 0x26D8, 584u, &Menu_BattleAbilities_battleAbilityMenuStrings, sizeof Menu_BattleAbilities_battleAbilityMenuStrings}, // G01024 _battleAbilityMenuStrings
    {"MENU/MENU3.PRG", 0x6BB8, 944u, &Menu_Inventory_menuText, sizeof Menu_Inventory_menuText}, // G01068 _menuText
    {"MENU/MENU4.PRG", 0x4C48, 3520u, &Menu_Status_statusStrings, sizeof Menu_Status_statusStrings}, // G01183 _statusStrings
    {"MENU/MENU5.PRG", 0x5BFC, 564u, &Menu_Map_mapNames, sizeof Menu_Map_mapNames}, // G01228 _mapNames
    {"MENU/MENU5.PRG", 0x5E30, 640u, &Menu_Map_connectingMaps, sizeof Menu_Map_connectingMaps}, // G01213 _connectingMaps
    {"MENU/MENU5.PRG", 0x60B0, 1044u, &Menu_Map_doors, sizeof Menu_Map_doors}, // G01218 _doors
    {"MENU/MENU7.PRG", 0x81B0, 1790u, &Menu_Files_containerStrings, sizeof Menu_Files_containerStrings}, // G01269 _containerStrings
    {"MENU/MENU8.PRG", 0x2D58, 2140u, &Menu_Options_menuStrings, sizeof Menu_Options_menuStrings}, // G01393 _menuStrings
    {"MENU/MENU8.PRG", 0x35B4, 416u, &Menu_Rename_renderRenameMenuStrings, sizeof Menu_Rename_renderRenameMenuStrings}, // G01396 _renderRenameMenuStrings
    {"MENU/MENU9.PRG", 0x5D20, 68u, &Menu_Score_menuText, sizeof Menu_Score_menuText}, // G01449 _menuText
    {"MENU/MENU9.PRG", 0x5D64, 384u, &Menu_Score_rankText, sizeof Menu_Score_rankText}, // G01463 _rankText
    {"MENU/MENU9.PRG", 0x5EE4, 728u, &Menu_Score_titleText, sizeof Menu_Score_titleText}, // G01474 _titleText
    {"MENU/MENU9.PRG", 0x61BC, 1568u, &Menu_Score_titleDescriptions, sizeof Menu_Score_titleDescriptions}, // G01473 _titleDescriptions
    {"MENU/MENU9.PRG", 0x67DC, 548u, &Menu_Score_statHeaders, sizeof Menu_Score_statHeaders}, // G01469 _statHeaders
    {"MENU/MENU9.PRG", 0x6A00, 920u, &Menu_Score_statDescriptions, sizeof Menu_Score_statDescriptions}, // G01468 _statDescriptions
    {"MENU/MENU9.PRG", 0x6D98, 116u, &Menu_Score_timeAttacks, sizeof Menu_Score_timeAttacks}, // G01472 _timeAttacks
    {"MENU/MENU9.PRG", 0x6E0C, 464u, &Menu_Score_timeAttackDescriptions, sizeof Menu_Score_timeAttackDescriptions}, // G01470 _timeAttackDescriptions
    {"MENU/MENU9.PRG", 0x6FDC, 3292u, &Menu_Score_miscInfo, sizeof Menu_Score_miscInfo}, // G01450 _miscInfo
    {"MENU/MENUD.PRG", 0x6D2C, 1380u, &Menu_Container_menuText, sizeof Menu_Container_menuText}, // G01739 _menuText
    {"MENU/MENUE.PRG", 0x2654, 1032u, &Menu_Manual_menuText, sizeof Menu_Manual_menuText}, // G01765 _menuText
    {"SLUS_010.40", 0x1FD44, sizeof Audio_Driver_legacySpuEvent, &Audio_Driver_legacySpuEvent, sizeof Audio_Driver_legacySpuEvent}, // G01826 _soundEvent
    {"SLUS_010.40", 0x1FD48, sizeof Audio_Driver_recentTickCostTotal, &Audio_Driver_recentTickCostTotal, sizeof Audio_Driver_recentTickCostTotal}, // G01827 D_8002F548
    {"SLUS_010.40", 0x1FD4C, sizeof Audio_Seq_tickCounter, &Audio_Seq_tickCounter, sizeof Audio_Seq_tickCounter}, // G01828 D_8002F54C
    {"SLUS_010.40", 0x1FD50, sizeof Audio_Driver_recentTickCosts, &Audio_Driver_recentTickCosts, sizeof Audio_Driver_recentTickCosts}, // G01829 D_8002F550
    {"SLUS_010.40", 0x1FD60, sizeof Audio_Driver_legacySpuFlushBytes, &Audio_Driver_legacySpuFlushBytes, sizeof Audio_Driver_legacySpuFlushBytes}, // G01830 _soundFlush
    {"SLUS_010.40", 0x1FDA0, sizeof Audio_Seq_kOpcodeLengths, &Audio_Seq_kOpcodeLengths, sizeof Audio_Seq_kOpcodeLengths}, // G01831 D_8002F5A0
    {"SLUS_010.40", 0x1FE00, sizeof Audio_Seq_kExtendedOpcodeLengths, &Audio_Seq_kExtendedOpcodeLengths, sizeof Audio_Seq_kExtendedOpcodeLengths}, // G01832 D_8002F600
    {"SLUS_010.40", 0x1FE20, sizeof Audio_Seq_kNoteLengths, &Audio_Seq_kNoteLengths, sizeof Audio_Seq_kNoteLengths}, // G01833 D_8002F620
    {"SLUS_010.40", 0x1FE38, sizeof Audio_Seq_kIdleProgram, &Audio_Seq_kIdleProgram, sizeof Audio_Seq_kIdleProgram}, // G01834 g_Sound_ProgramCounter
    {"SLUS_010.40", 0x1FE3C, sizeof Audio_Seq_kSemitonePitches, &Audio_Seq_kSemitonePitches, sizeof Audio_Seq_kSemitonePitches}, // G01835 D_8002F63C
    {"SLUS_010.40", 0x1FE6C, sizeof Audio_Seq_kNoteRemapTables, &Audio_Seq_kNoteRemapTables, sizeof Audio_Seq_kNoteRemapTables}, // G01836 D_8002F66C
    {"SLUS_010.40", 0x1FE9C, sizeof Audio_Seq_kRandomPitchOffsets, &Audio_Seq_kRandomPitchOffsets, sizeof Audio_Seq_kRandomPitchOffsets}, // G01837 D_8002F69C
    {"SLUS_010.40", 0x1FF9C, sizeof Audio_Mixer_kPanGains, &Audio_Mixer_kPanGains, sizeof Audio_Mixer_kPanGains}, // G01838 g_Sound_StereoPanGainTableQ15
    {"SLUS_010.40", 0x30A10, sizeof Core_kSineTable, &Core_kSineTable, sizeof Core_kSineTable}, // G01902 _trig_table
    {"SLUS_010.40", 0x31214, sizeof Core_Math_kArcTangentTable, &Core_Math_kArcTangentTable, sizeof Core_Math_kArcTangentTable}, // G01903 D_80040A14
    {"SLUS_010.40", 0x39950, sizeof Render_legacyLoadingImageHeader, &Render_legacyLoadingImageHeader, sizeof Render_legacyLoadingImageHeader}, // G02006 _nowLoading_header
    {"SLUS_010.40", 0x39954, sizeof Render_loadingScreenPixels, &Render_loadingScreenPixels, sizeof Render_loadingScreenPixels}, // G02005 _nowLoading_data
    {"SLUS_010.40", 0x3C1DC, sizeof Battle_actionDefinitions, &Battle_actionDefinitions, sizeof Battle_actionDefinitions}, // G01904 vs_main_actions
    {"SLUS_010.40", 0x3F5DC, sizeof Audio_Seq_unusedMaterialModifiers, &Audio_Seq_unusedMaterialModifiers, sizeof Audio_Seq_unusedMaterialModifiers}, // G01905 vs_main_materialModifiers
    {"SLUS_010.40", 0x3F6CC, sizeof Battle_statusEffectParams, &Battle_statusEffectParams, sizeof Battle_statusEffectParams}, // G01906 vs_main_statusEffectParams
    {"SLUS_010.40", 0x3F8CC, sizeof Field_locationDestinations, &Field_locationDestinations, sizeof Field_locationDestinations}, // G01913 vs_main_locationZoneMapIds
    {"SLUS_010.40", 0x404CC, sizeof Field_zoneMusicIds, &Field_zoneMusicIds, sizeof Field_zoneMusicIds}, // G01914 D_8004FCCC
    {"SLUS_010.40", 0x405D0, sizeof Actors_placeholderActorName, &Actors_placeholderActorName, sizeof Actors_placeholderActorName}, // G01915 D_8004FDD0
    {"SLUS_010.40", 0x405E4, sizeof Battle_breakArtPointRequirements, &Battle_breakArtPointRequirements, sizeof Battle_breakArtPointRequirements}, // G01916 vs_main_artsPointsRequirements
    {"SLUS_010.40", 0x4065C, sizeof Battle_battleAbilityPointRequirements, &Battle_battleAbilityPointRequirements, sizeof Battle_battleAbilityPointRequirements}, // G01917 vs_main_battleAbilitiesPointsRequirements
    {"SLUS_010.40", 0x40688, sizeof Actors_usesSecondaryBodyModel, &Actors_usesSecondaryBodyModel, sizeof Actors_usesSecondaryBodyModel}, // G01918 D_8004FE88
    {"TITLE/TITLE.PRG", 0xA70C, sizeof Title_publisherLogo, &Title_publisherLogo, sizeof Title_publisherLogo}, // G02143 publisher_clut
    {"TITLE/TITLE.PRG", 0xBF2C, sizeof Title_developerLogo, &Title_developerLogo, sizeof Title_developerLogo}, // G02019 developer
    {"TITLE/TITLE.PRG", 0xE2D4, sizeof Title_titleScreenBackground, &Title_titleScreenBackground, sizeof Title_titleScreenBackground}, // G02020 _titleScreenBg
    {"TITLE/TITLE.PRG", 0x449A8, sizeof Title_copyrightImage, &Title_copyrightImage, sizeof Title_copyrightImage}, // G02021 _menuCopyright
    {"TITLE/TITLE.PRG", 0x46B68, sizeof Title_saveMenuFontAtlas, &Title_saveMenuFontAtlas, sizeof Title_saveMenuFontAtlas}, // G02022 _fontTable
    {"TITLE/TITLE.PRG", 0x54B68, sizeof Title_debugFontAtlas, &Title_debugFontAtlas, sizeof Title_debugFontAtlas}, // G02023 _debugFont
    {"TITLE/TITLE.PRG", 0x59A68, sizeof Title_saveMenuUiAtlas, &Title_saveMenuUiAtlas, sizeof Title_saveMenuUiAtlas}, // G02024 _uiTable
    {"TITLE/TITLE.PRG", 0x68A68, sizeof Title_saveMenuBackground, &Title_saveMenuBackground, sizeof Title_saveMenuBackground}, // G02089 _saveMenuBg_clut
    {"BATTLE/BATTLE.PRG", 0x88F58, 4u, Content_battleAiHesitationLengths, sizeof Content_battleAiHesitationLengths},
    {"BATTLE/BATTLE.PRG", 0x1060, 152u, Content_battleLimbNameBytes, sizeof Content_battleLimbNameBytes},
    {"BATTLE/BATTLE.PRG", 0x80B0C, 1820u, Content_battleGimmickTemplateBytes, sizeof Content_battleGimmickTemplateBytes},
    {"BATTLE/BATTLE.PRG", 0x823AC, 2524u, Content_battleTextboxFrameBytes, sizeof Content_battleTextboxFrameBytes},
    {"SLUS_010.40", 0x2019C, 2052u, Content_residentLfoSamples, sizeof Content_residentLfoSamples},
};
const unsigned Content_moduleDataCount = sizeof Content_moduleData / sizeof Content_moduleData[0];

