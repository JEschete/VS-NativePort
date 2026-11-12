"""Reviewed fixes applied by copy_to_native.py to copied function text, after renaming.

The copy is mechanical. A few places need a change it cannot derive: the decomp compiled them through a view of a
struct whose member was an array, or an int, where the ledger's (upstream-merged) layout has a struct or union.
Each fix states why and is applied only to the named catalog rows; the copy fails if a fix no longer matches,
so a stale fix is noticed.

  FIXES: catalog id -> [(old, new, reason)]   exact text replacement (every occurrence)
  REGEX_FIXES: [(row filter regex on final_name, pattern, replacement, reason)]
  MACRO_REPLACEMENTS: macro name -> C definition, for game macros whose body is MIPS assembly
  NESTED_SAME_AS: nested type id -> the nested type it is, inside a same-as outer type
"""
from __future__ import annotations

# Function id -> {parameter index or "ret": native type} for promoted carriers (none outstanding).
NATIVE_SIGNATURE_REWRITES: dict[str, dict] = {}

# FSoundChannel.effectId carries either a built-in ID or a native program pointer.
NATIVE_POINTER_FIELDS = {"T00278.012"}

# F00989, F01024, F01026, F04178, F04179, F04181-F04187 #46 typed promotion: generated callers of the
# VS::Actors motion requests use these legacy-signature bindings.
_MOTION_REQUEST_LIVE = {
    "SaveActorGroundedTransform": "SaveLiveActorGroundedTransform",
    "BeginTileJump": "BeginLiveTileJump",
    "BeginAirborneTileJump": "BeginLiveAirborneTileJump",
    "BeginNavigationJump": "BeginLiveNavigationJump",
    "BeginTeleport": "BeginLiveTeleport",
    "BeginTileTravel": "BeginLiveTileTravel",
    "BeginTimedTravel": "BeginLiveTimedTravel",
    "QueueGroundOffset": "QueueLiveGroundOffset",
    "QueueMovementVector": "QueueLiveMovementVector",
    "QueueFlightMovement": "QueueLiveFlightMovement",
    "TurnNavigationActor": "TurnLiveNavigationActor",
    "TurnActorAtSpeed": "TurnLiveActorAtSpeed",
}

# F02764, F02767, F03628, F03634 #46 typed promotion: generated MENU7 and TITLE callers of the
# VS::Menu file pickers use these one-frame bindings over the menu globals and controller.
_FILE_MENU_LIVE = {
    "UpdateDataSaveMenu": "UpdateLiveDataSaveMenu",
    "UpdateDataLoadMenu": "UpdateLiveDataLoadMenu",
    "UpdateLoadMenu": "UpdateLiveLoadMenu",
    "UpdateSaveMenu": "UpdateLiveSaveMenu",
}

# #276 slice A3 (core math): record bindings over the typed VS::Core::Math cores for generated callers whose
# original argument order and record pointers differ from the typed shape (OWNED_CALL_BINDINGS), by header.
_CORE_MATH_LIVE = {
    "VectorLive.hpp": (
        "AddLiveVectors", "AddLiveVectorsTo", "SubtractLiveVectors", "CopyLiveVector", "CopyLiveVectorTo",
        "SelectLiveGreatestMagnitude", "ScaleLiveFixedVector", "MeasureLiveFixedVector",
        "WidenLiveShortVectorToFixed", "NormalizeLiveFixedVector", "StoreLiveCrossProduct", "NormalizeLiveVector"),
    "MatrixLive.hpp": (
        "BuildLiveRotationXyz", "BuildLiveEulerRotationMatrix", "BuildLiveLookAtOrientation",
        "BuildLiveDirectionRotationMatrix", "SetLiveIdentityRotation", "SetLiveTranslationTransform"),
    "InterpolationLive.hpp": (
        "InterpolateLiveSevenBitScalar", "InterpolateLiveShortPair", "InterpolateLiveSevenBitVector",
        "InterpolateLiveSevenBitPlane", "InterpolateLiveThirteenBitScalar", "InterpolateLiveThirteenBitVector",
        "InterpolateLiveTimedScalar", "InterpolateLiveTimedVector", "InterpolateLiveCosineScalar",
        "InterpolateLiveCosineVector", "InterpolateLiveFixedPointRatio", "InterpolateLiveLagrangeVector",
        "InterpolateLiveLagrangeScalar", "EvaluateLiveEase"),
}

# #276 slice A4 (render display, camera and HUD packets, #279): Live adapters keeping the original call shape
# over the typed VS::Render cores, by header and owned row.
_RENDER_LIVE = {
    "DisplayControlLive.hpp": (
        ("ConfigureLiveFieldDisplay", "F00162"), ("PresentLiveFieldFrame", "F00163"),
        ("EnableLiveDisplay", "F00258"), ("DisableLiveDisplay", "F00259"), ("ShowLiveDisplay", "F00540"),
        ("HideLiveDisplay", "F00541"), ("ShowLiveLoadingScreen", "F03421")),
    "CameraViewLive.hpp": (
        ("BuildLiveCameraViewMatrix", "F00211"), ("SetLiveCameraNearClip", "F00286"),
        ("SetLiveCameraProjectionDistance", "F00287"), ("SetLiveCameraFarClip", "F00288"),
        ("PrepareLiveActorCameraMatrices", "F01006")),
    "DepthCueLive.hpp": (("ApplyLiveDepthCueColor", "F03404"), ("SetLiveFogNearDistance", "F03408")),
    "UiPacketsLive.hpp": (
        ("AppendLiveOverlayPrimitive", "F00200"), ("DrawLiveNumericGlyph", "F01351"),
        ("DrawLiveIntegerRightAligned", "F01352"), ("DrawLiveStatGauge", "F01353"),
        ("InsertLiveThreeWordPrimitive", "F01405"), ("InsertLiveDrawModeCommand", "F01406"),
        ("UploadLiveImageRectangle", "F01408"),
        # #276 row 6 (render textures): the texture move, the ENDING texture-page command, the file menus'
        # packed-rectangle uploads and reads and the immediate sprite.
        ("QueueLiveTextureRectangleMove", "F00971"), ("AppendLiveTexturePage", "F02421"),
        ("UploadLiveImageRectangle", "F02717"), ("UploadLiveImageRectangle", "F03584"),
        ("ReadLiveImageRectangle", "F02718"), ("ReadLiveImageRectangle", "F03585"),
        ("DrawLiveImmediateSprite", "F02739")),
    # #276 row 6 (render textures): the texture-page reservations and the TIM parse.
    "TexturePagesLive.hpp": (
        ("InitializeLiveTexturePages", "F00329"), ("AdvanceLiveTexturePageRetirements", "F00330"),
        ("ReserveLiveTexturePageRange", "F00331"), ("RetireLiveTexturePageRange", "F00332")),
    "TimImageLive.hpp": (("ParseLiveTim", "F00609"), ("ParseLiveTim", "F02452")),
    # #276 slice A5 (screen overlays): the framebuffer effect and the room transition draw.
    "ScreenOverlayLive.hpp": (
        ("DrawLiveScreenTransition", "F00070"), ("ResetLiveFramebufferEffect", "F00306"),
        ("InitializeLiveFramebufferEffect", "F00307"), ("DrawLiveFramebufferEffect", "F00309"),
        ("SetLiveFramebufferEffectEnabled", "F00310"), ("SetLiveFramebufferEffectRotation", "F00311"),
        ("SetLiveFramebufferEffectScale", "F00312"), ("SetLiveFramebufferEffectTint", "F00313"),
        ("SetLiveFramebufferEffectOffset", "F00314"), ("SetLiveFramebufferEffectBlendMode", "F00315"),
        ("SetLiveFramebufferEffectDepth", "F00317"), ("SetLiveFramebufferPreviousFrame", "F00318"),
        ("ReadLiveFramebufferEffectScale", "F00321"), ("ReadLiveFramebufferEffectTint", "F00322"),
        ("ReadLiveFramebufferEffectOffset", "F00323")),
    # #276 slice 5b (palette animation): the CLUT rows, the room background gradient and the model tints.
    "PaletteAnimationLive.hpp": (
        ("LoadLiveClutColors", "F03561"), ("CommitLiveClutRows", "F03565"),
        ("InitializeLiveClutAnimation", "F03567"), ("UpdateLiveBackgroundGradientTransition", "F04177"),
        ("UpdateLiveModelTintTransitions", "F03558"), ("UpdateLiveClutTransitions", "F03559"),
        ("CaptureLiveRoomBackgroundColors", "F03564"), ("SetLiveClutAnimationEnabled", "F03566"),
        ("TransformLiveRoomPalettes", "F00215"), ("TransformLiveSceneColors", "F00216"),
        ("TransformLiveActorColors", "F00217"), ("TransformLiveBackgroundColors", "F00218")),
    # #276 row 6 (render GIM images): the event scripts' load, release and the game loop's per-frame update.
    "GimImagesLive.hpp": (
        ("QueueLiveGimImage", "F01338"), ("ReleaseLiveGimImages", "F01340"), ("UpdateLiveGimImages", "F01341")),
    # #276 row 6 (render fonts): the battle string formatter over the battle string context, and the compact,
    # variable-width and text box fonts over the shared font style.
    "EncodedTextLive.hpp": (("FormatLiveEncodedText", "F01328"), ("MeasureLiveEncodedTextLine", "F01331")),
    "GameFontLive.hpp": (
        ("DrawLiveCompactTextColor", "F01325"), ("DrawLiveCompactText", "F01326"),
        ("DrawLiveVariableWidthGlyph", "F01332"), ("SetLiveMenuFontStyle", "F01333"),
        ("DrawLiveProgressiveTextBoxText", "F01334"), ("DrawLiveScaledTextBoxText", "F01335"),
        ("BlendLiveTextBoxIntoFramebuffer", "F01337")),
}

# #276 slice 8 (save-memory-card): the C-shaped bindings over the typed VS::Save::MemoryCard game-file core and the
# VS::Save snapshot core, by binding name and the rows whose generated callers reach them (OWNED_CALL_BINDINGS).
# PollEvents, ClearEvents, ReadSaveSummary and HasSaveSuffix have no generated callers left, and AnySaveFileExists'
# only caller (F03670) is the typed VS::Title::Run (#276 slice 53), whose host picks the native initial selection.
_SAVE_MEMORY_CARD_LIVE = {
    "BuildLiveSaveFilename": ("F02720", "F03587"),
    "BuildLiveTemporaryFilename": ("F02721", "F03588"),
    "NewestLiveSaveSlot": ("F02722",),
    "FindLiveMatchingSaveSlot": ("F02723",),
    "ParseLiveSaveFileNumber": ("F02724", "F03591"),
    "HasLiveChecksumError": ("F02725", "F03592"),
    "DecodeLiveSaveBytes": ("F02726", "F03593"),
    "DeleteLiveRedundantTemporaryFiles": ("F02729", "F03596"),
    "ScanLiveSaveDirectory": ("F02730",),
    "CreateLiveSaveFile": ("F02731", "F03598"),
    "AdvanceLiveCardReadiness": ("F02732",),
    "ValidateAndApplyLiveLoadedSave": ("F02733",),
    "PackageLiveGameSave": ("F02734",),
    "ReadLiveSaveStep": ("F02735",),
    "WriteLiveSaveStep": ("F02736",),
    "LoadLiveIntroSnapshotStep": ("F02768",),
    "NextLiveTitleCipherBits": ("F03581",),
    "PackageLiveClearSave": ("F03601",),
}
_SAVE_SNAPSHOT_LIVE = {
    "MarkLiveAshleySnapshotValid": ("F03442",),
    "CaptureLiveAshleySnapshot": ("F03443",),
}

# #276 slice 18 (items): Live adapters keeping the original call shape over the typed VS::Items cores, by header
# and owned row. Rows absent here (F00024-F00026, F00036, F00037, F00039-F00044, F00395, F00400-F00402, F02505,
# F02507) have no generated callers outside their own cores.
_ITEMS_LIVE = {
    r"Native\Game\include\Items\EquipmentLive.hpp": (
        ("BuildLiveWeaponStatsFromParts", "F00011"), ("BuildLiveShieldStatsFromParts", "F00012"),
        ("BuildLiveAccessoryStatsFromPart", "F00013"), ("BuildLiveArmorStatsFromPart", "F00014"),
        ("ReadLiveInventoryBladeStats", "F00015"), ("ReadLiveInventoryGripStats", "F00016"),
        ("ReadLiveInventoryArmorStats", "F00017"), ("ReadLiveInventoryGemStats", "F00018"),
        ("LoadLiveInventoryWeaponStats", "F00019"), ("LoadLiveInventoryShieldStats", "F00020"),
        ("LoadLiveInventoryArmorStats", "F00021"), ("LoadLiveInventoryAccessoryStats", "F00022"),
        ("EquipLiveAshleyWeapon", "F00027"), ("EquipLiveAshleyShield", "F00028"),
        ("EquipLiveAshleyArmor", "F00029"), ("EquipLiveAshleyAccessory", "F00030"),
        ("WriteLiveInventoryBladeStats", "F00031"), ("WriteLiveInventoryGripStats", "F00032"),
        ("WriteLiveInventoryGemStats", "F00033"), ("WriteLiveInventoryArmorStats", "F00034"),
        ("StoreLiveInventoryWeaponStats", "F00035"), ("StoreLiveInventoryAccessoryStats", "F00038"),
        ("RebuildLiveArmorClassAffinity", "F00335"), ("RebuildLiveShieldClassAffinity", "F00336"),
        ("RebuildLiveWeaponClassAffinity", "F00337"), ("ConsumeLiveHitEquipmentDp", "F00398"),
        ("AccumulateLiveHitEquipmentPp", "F00399"), ("GrowLiveDefenderEquipmentRatings", "F00403"),
        ("GrowLiveAttackerWeaponRatings", "F00404"), ("CalculateLiveItemRecovery", "F00423")),
    r"Native\Game\include\Items\LootLive.hpp": (
        ("QueueLiveEnemyLoot", "F00045"), ("OpenLivePendingEnemyLoot", "F00095"),
        ("PresentLiveScriptedLoot", "F00230"), ("ClearLiveRoomLootList", "F00549"),
        ("FindLiveTreasureChestAtTile", "F00603"), ("RestoreLiveRoomChestStates", "F00608"),
        ("LiveRoomTreasureChests", "F00622"), ("LoadLiveRoomChestLoot", "F00631")),
    r"Native\Game\include\Items\InventoryLive.hpp": (
        ("CountLiveInventoryItem", "F00338"), ("ConsumeLiveMiscItem", "F01400"),
        ("Inventory::UnequipLiveWeapon", "F02500"), ("Inventory::UnequipLiveShield", "F02501"),
        ("Inventory::ClearLiveItemSlot", "F02502"), ("Inventory::FindLiveDisplayPosition", "F02506"),
        ("Inventory::RebuildLiveDisplayIndices", "F02508"), ("Inventory::CountLiveItems", "F02509"),
        ("Inventory::FindLiveFirstEmptySlot", "F02510"), ("Inventory::CopyLiveItem", "F02511"),
        ("Inventory::StoreLiveEquippedStats", "F02551")),
}

# #276 row 22 (actors-appearance-rendering): the Live adapter each generated caller of an actor appearance or model
# rendering function uses instead, in the original argument order.
_ACTORS_APPEARANCE_LIVE = {
    r"Native\Game\include\Actors\AppearanceLive.hpp": (
        ("SetLiveActorPaletteFade", "F00810"), ("ResetLiveActorWeaponVisibilityBlend", "F00812"),
        ("SetLiveWeaponBrightnessOverride", "F00839"), ("SetLiveRoomActorScale", "F00844"),
        ("EnableLiveModelPalettePulse", "F00849"), ("DisableLiveModelPalettePulse", "F00850"),
        ("BeginLiveActorShadowBlend", "F00858"), ("LiveActorFadeParameters", "F00859"),
        ("SetLiveActorFadeParameters", "F00860"), ("LiveActorTint", "F00861"), ("SetLiveActorTint", "F00862"),
        ("StartLiveActorAppearanceTransition", "F00983")),
}
_ACTORS_TEXTURE_LIVE = {
    r"Native\Game\include\Actors\TextureAnimationLive.hpp": (
        ("SetLiveActorTextureAnimation", "F00973"), ("UpdateLiveActorTextureAnimations", "F00974"),
        ("UploadLiveActorTextureFrame", "F00975"), ("ResetLiveActorTextureFrames", "F00976"),
        ("UploadLiveActorTextureOverlay", "F00977"), ("UpdateLiveActorTextureScrolling", "F00978")),
}
_ACTORS_MODEL_RENDERING_LIVE = {
    r"Native\Game\include\Actors\ModelRenderingLive.hpp": (
        ("PrepareLiveActorRootTransform", "F01005"), ("UpdateAndDrawLiveRoomActors", "F01008"),
        ("TransformLiveActorRootOffset", "F01012"), ("DrawLiveGimmickModel", "F01020"),
        ("DrawLiveActorWeaponModel", "F01021"), ("InitializeLiveActorModelResources", "F01860")),
}
# #276 row 20 (actors-animation-skeleton): the Live adapter each generated caller of a skeleton query uses instead,
# in the original argument order.
_ACTORS_SKELETON_LIVE = {
    r"Native\Game\include\Actors\SkeletonLive.hpp": (
        ("LiveActorPitchToPoint", "F00822"), ("SetLiveActorHeadTracking", "F00847"),
        ("ResetLiveActorHeadTrackingSpeed", "F00848"), ("LiveActorTaggedBoneMatrix", "F00874"),
        ("LiveActorBoneMatrix", "F00875"), ("ProjectLiveActorTaggedBone", "F00876"),
        ("ProjectLiveActorBone", "F00877"), ("FindLiveActorBoneTag", "F00879"), ("LiveActorBoneTag", "F00880"),
        ("LiveActorAnchorMatrix", "F00881"), ("LiveActorBoneWorldMatrix", "F00882"),
        ("LiveActorTaggedBonePosition", "F00883"), ("LiveActorBonePosition", "F00884"),
        ("LiveActorHighGroupBonePosition", "F00885"), ("LiveActorLowGroupBonePosition", "F00886"),
        ("LiveActorBoneEndpointPosition", "F00887"), ("LiveActorBoneViewTransform", "F00888"),
        ("BuildLiveActorPoseAtMatrix", "F00918")),
}
# #276 row 21 (actors-movement): the Live adapter each generated caller of an actor motion rule uses instead, in the
# original argument order. F01011 has no callers (its body is AdvanceShadowBlend).
_ACTORS_MOVEMENT_LIVE = {
    r"Native\Game\include\Actors\ActorMotionLive.hpp": (
        ("BeginLiveScriptedActorPath", "F00297"), ("IsLiveScriptedActorPathFinished", "F00298"),
        ("ResetLiveActorMovement", "F00302"), ("StopLiveActorMotion", "F00815"),
        ("ClearLiveActorMovementModes", "F00816"), ("LiveActorFacingDeltaToPoint", "F00821"),
        ("ResetLiveActorRunMomentum", "F00868"), ("SetLiveActorWalkRunSpeeds", "F00869"),
        ("SetLiveActorTravelStepCounts", "F00870"), ("LiveActorTravelStepCounts", "F00871"),
        ("StepLiveActorTransitionDisplacement", "F00899"), ("SetLiveActorFallDestination", "F00900"),
        ("ClearLiveActorMovementRequest", "F00925"), ("MoveLiveActorToTileAtRate", "F00946"),
        ("MoveLiveActorToPositionAtRate", "F00947"), ("TranslateLiveActorToTileAtRate", "F00948"),
        ("TurnLiveActorTowardTileAtRate", "F00949"), ("TranslateLiveActorByAtRate", "F00950"),
        ("TranslateLiveActorToAtRate", "F00951"), ("TurnLiveActorByAtRate", "F00952"),
        ("TurnLiveActorToAtRate", "F00953"), ("RotateLiveActorByAtRate", "F00954"),
        ("RotateLiveActorToAtRate", "F00955"), ("MoveLiveActorToTileOverTime", "F00956"),
        ("TranslateLiveActorToTileOverTime", "F00957"), ("SetLiveActorTileTranslation", "F00958"),
        ("TurnLiveActorTowardTileOverTime", "F00959"), ("SetLiveActorTileTurn", "F00960"),
        ("SetLiveActorMotion", "F00961"), ("TranslateLiveActorToOverTime", "F00962"),
        ("SetLiveActorTranslation", "F00963"), ("SetLiveActorYawMotion", "F00964"),
        ("TurnLiveActorToOverTime", "F00965"), ("TurnLiveActorToAtAngularSpeed", "F00966"),
        ("SetLiveActorYawTurn", "F00967"), ("SetLiveActorRotationMotion", "F00968"),
        ("SetLiveActorRotation", "F00969"), ("ScaleLiveActorToOverTime", "F00970"),
        ("UpdateLiveActorRoute", "F00986"), ("LiveActorJumpAnimationProfile", "F00987")),
    # F00889 (tile jump) and the internal movement steps have no generated callers outside the promoted file.
    r"Native\Game\include\Actors\ActorMovementLive.hpp": (
        ("CancelLiveActorTeleportTransition", "F00897"), ("BeginLiveActorJumpMovement", "F00919"),
        ("UpdateLiveActorPlatformContact", "F00921"), ("UpdateLiveRoomActorMovement", "F00922"),
        ("UpdateLiveActorMovement", "F00923"), ("UpdateLiveActorJumpAndFall", "F00988")),
}
# #276 row 20b (actors-animation-control): the Live adapter each generated caller of an animation control rule uses
# instead, in the original argument order. No module image stores any of these addresses.
_ACTORS_ANIMATION_LIVE = {
    r"Native\Game\include\Actors\AnimationControlLive.hpp": (
        ("QueueLiveActorBaseSequenceLoad", "F00170"), ("QueueLiveActorWeaponSequenceLoad", "F00172"),
        ("InterruptLiveActorAction", "F00255"), ("QueueLiveSharedSequenceLoad", "F00278"),
        ("UnloadLiveSharedSequence", "F00279"), ("QueueLiveSharedSequenceWithTextures", "F00280"),
        ("QueueLiveSequenceEffectTextures", "F00281"), ("ReleaseLiveSequenceEffectTextures", "F00282"),
        ("SetLiveSharedSequenceBank", "F00283"), ("ReadLiveSharedSequenceBank", "F00284"),
        ("ReturnLiveActorToIdle", "F00291"), ("ReturnAllLiveActorsToIdle", "F00292"),
        ("SelectLiveSharedSequenceBank", "F00801"), ("LiveSharedSequenceBankId", "F00802"),
        ("RequestLiveActorAnimationAdvance", "F00811"), ("SetLiveActorWeaponDrawn", "F00813"),
        ("PlayLiveActorWeaponDrawSound", "F00814"), ("PlayLiveActorActionPose", "F00820"),
        ("IsLiveAshleyTransitionPose", "F00824"), ("IsLiveActorActionAnimationBusy", "F00825"),
        ("RestoreLiveActorIdlePose", "F00826"), ("RestoreLiveActorNeutralPose", "F00827"),
        ("RestoreLiveActorBasePose", "F00829"), ("LiveAnimationBattleWindowFrame", "F00840"),
        ("SetLiveActorSoundOverrides", "F00842"), ("ApplyLiveActorSoundOverrides", "F00843"),
        ("SetLiveActorTravelAnimation", "F00845"), ("LiveActorTravelAnimation", "F00846"),
        ("QueueLiveActorAnimation", "F00855"), ("PlayLiveActorAnimation", "F00856"),
        ("PlayLiveActorAnimationAtFrame", "F00857"), ("SetLiveActorTransitionMode", "F00863"),
        ("SetLiveActorAnimationLocked", "F00865"), ("PlayLiveActorTurnAnimation", "F00890"),
        ("UpdateLiveActorLocomotionAnimation", "F00930"), ("SetLiveActorBonePoseLock", "F00984"),
        ("ClearLiveActorBonePoseLocks", "F00985")),
}
# #276 row 20c (actors-animation-pose): the Live adapter each generated caller of an animation pose rule uses
# instead, in the original argument order. No module image stores any of these addresses.
_ACTORS_ANIMATION_POSE_LIVE = {
    r"Native\Game\include\Actors\AnimationPoseLive.hpp": (
        ("FindLiveActorAnimationHeader", "F00993"), ("InitializeLiveActorAnimationPose", "F00992"),
        ("UpdateLiveActorAnimationBlend", "F01009"), ("UpdateLiveMotionAndLookAt", "F04130")),
}

# #276 row 17 (state-boot): the Live adapter each generated caller of a State record rule uses instead, in the
# original argument order.
_STATE_LIVE = {
    r"Native\Game\include\State\ScriptFlagsLive.hpp": (
        ("ResetLiveZoneScriptFlags", "F01256"), ("LiveScriptFlag", "F01258"), ("SetLiveScriptFlag", "F01259")),
    r"Native\Game\include\State\GameClockLive.hpp": (("UpdateLivePlayTimeAndLoading", "F03426"),),
    r"Native\Game\include\State\SceneTimerLive.hpp": (
        ("StartLiveSceneTimer", "F00232"), ("ResetLiveSceneTimer", "F00233"), ("SetLiveSceneTimerPaused", "F00234"),
        ("SetLiveTickSpeed", "F00256"), ("UpdateLiveSceneTimer", "F00524")),
    r"Native\Game\include\State\KillScoreLive.hpp": (
        ("RecordLiveEnemyKillScore", "F00046"), ("UpdateLiveWeaponKillStreak", "F00047")),
}

# #276 row 17 (state-boot): generated Boot callers use the Live lifecycle boundary.
_BOOT_LIVE = {
    r"Native\Game\include\Boot\SessionLive.hpp": (
        ("LoadBattleOverlaysLive", "F03417"), ("LoadTitleOverlayLive", "F03418"),
        ("LoadEndingOverlayLive", "F03419"), ("ResetToTitleLive", "F03423"),
        ("EnterBattleLive", "F03424"), ("RunEndingAndReturnToTitleLive", "F03425"),
        ("InitializePlatformServicesLive", "F03436"), ("InitializeTitleSessionLive", "F03437"),
        ("InitializeGameplayOverlayLive", "F01858"), ("RunGameplayLoopLive", "F00199"),
        ("WaitForOverlayReadyLive", "F03440"), ("SetSoftResetEnabledLive", "F03441")),
}

# #276 row 23 (field-collision-map): the Live adapter each generated caller of a room collision map, map progress or
# trap rule uses instead, in the original argument order.
_FIELD_COLLISION_MAP_LIVE = {
    r"Native\Game\include\Field\CollisionMapLive.hpp": (
        ("LiveRoomCollisionCells", "F00571"), ("LiveCollisionMapDimensions", "F00573"),
        ("LiveRoomCameraBounds", "F00574"), ("LiveCollisionCellAt", "F00575"),
        ("SetLiveCollisionCellBlocked", "F00576"), ("LiveInterpolatedFloorHeight", "F00612"),
        ("LiveFloorHeightAt", "F00613"), ("LiveCeilingHeightAt", "F00614"),
        ("LiveFloorAndCeilingHeightsAt", "F00615"), ("SetLiveActorCollisionBypass", "F00866"),
        ("LiveActorCollisionBypass", "F00867"), ("LiveActorSupportHeight", "F00904"),
        ("SnapLiveRoomActorsToSupport", "F00905"), ("LiveRoomCeilingHeight", "F00909"),
        ("LiveIsPointOnMovingPlatform", "F00924"), ("FindLiveCollisionCellAtPosition", "F00939"),
        ("LiveGimmickSupportHeight", "F00943"), ("LiveMovingPlatformContactHeight", "F00944"),
        ("LiveGimmickSupportHeightBelow", "F01015")),
    r"Native\Game\include\Field\MapLive.hpp": (
        ("SetLiveMinimapVisible", "F00566"), ("RestoreLiveSceneRoomVisits", "F00567"),
        ("BuildLiveMinimapRotation", "F00569"), ("LiveMapCompletionPercent", "F00630")),
    r"Native\Game\include\Field\TrapsLive.hpp": (
        ("RollLiveRiskLockBreak", "F00350"), ("CalculateLiveTrapPanelDamage", "F00420"),
        ("CalculateLiveHealingPanelRecovery", "F00422")),
}

# #276 row 23 remainder (field-actor-collision): the Live bindings each generated caller of an actor terrain collision
# rule uses instead, in the original argument order. The other five rows have no callers outside the typed core.
_FIELD_ACTOR_COLLISION_LIVE = {
    r"Native\Game\include\Field\ActorCollisionLive.hpp": (
        ("LiveFindActorOccupyingTile", "F00911"), ("LiveCheckPlatformActorObstruction", "F00915"),
        ("LiveCanActorHangFromLedge", "F00928"), ("LiveHasActorLedgeSideSupport", "F00929"),
        ("LiveSampleActorSupportHeight", "F00931"), ("LiveSampleActorCollisionPerimeter", "F00932"),
        ("LiveApplyActorWallRepulsion", "F00933"), ("LiveResolveActorTerrainMotion", "F00934"),
        ("LiveCalcActorLedgeOffset", "F00945")),
}

# #276 slice 16 (content): Live adapters keeping the original call shape over the typed VS::Content cores, by header
# and owned row. Rows absent here have no generated callers outside their own cores (the asset loaders, sharing
# probes, drive interrupt handlers and F00779), or are called by their typed name (F01627).
_CONTENT_LIVE = {
    r"Native\Content\include\Content\ActorAssetsLive.hpp": (
        ("FindLiveFreeActorAssetRequest", "F00776"), ("QueueLiveActorAssetRequest", "F00777"),
        ("UpdateLiveActorAssetQueue", "F00778"), ("OffsetLiveModelTextureCoordinates", "F00784"),
        ("CopyLiveModelGeometry", "F00786"), ("ReleaseLiveWeaponModel", "F00790"),
        ("CopyLiveWeaponSequence", "F00797"), ("ReleaseLiveActorSequence", "F00800"),
        ("ReleaseLiveActorModel", "F00803"), ("ReleaseLiveSequenceEffectTexture", "F00804")),
    r"Native\Content\include\Content\CdDriveLive.hpp": (
        ("ReadLiveDiscFileBlocking", "F03476"), ("InitializeLiveDiscQueue", "F03480"),
        ("AllocateLiveDiscRequest", "F03481"), ("ReleaseLiveDiscRequest", "F03482"),
        ("EnqueueLiveDiscRequest", "F03483"), ("EnqueueLivePriorityDiscRequest", "F03484"),
        ("UpdateLiveDiscQueue", "F03486")),
    r"Native\Content\include\Content\OverlayWorkspaceLive.hpp": (
        ("AcquireLiveOverlayWorkspace", "F00333"), ("ReleaseLiveOverlayWorkspace", "F00334")),
    r"Native\Content\include\Content\PFileLive.hpp": (
        ("PFileLiveTimelineSection", "F01555"), ("PFileLiveTimelineTrackData", "F01556")),
    r"Native\Content\include\Content\SystemResourcesLive.hpp": (
        ("LoadLiveSystemResources", "F01852"),),
}

NATIVE_HELPER_HEADERS = {
    "Battle_Ai_RefreshActionCandidateReadinessEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Battle_Ai_ApplySimulatedActionResultsEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Battle_Ai_SimulateActionCandidateEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Battle_Ai_UpdateCandidateHitWeightEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Battle_Ai_EvaluateActionCandidateEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Battle_Ai_SelectFallbackActionEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Battle_Ai_EnumerateActionCandidatesEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Battle_Ai_AdvancePlanningSimulationEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Battle_Ai_BuildActorActionPlanEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Battle_Ai_ReconcileCompletedActionPlanEntry": r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h",
    "Field_CurrentSavePointIdEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_RefreshAshleyInteractionTileEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_CreateGimmickInstanceEntry": r"Native\Game\src\Field\GimmickResourcesLive.h",
    "Field_ReleaseGimmickInstanceEntry": r"Native\Game\src\Field\GimmickResourcesLive.h",
    "Field_FindSharedGimmickModelEntry": r"Native\Game\src\Field\GimmickResourcesLive.h",
    "Field_SetGimmickPaletteTableEntry": r"Native\Game\src\Field\GimmickResourcesLive.h",
    "Field_MovingPlatformTargetWaypointEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_SetPlatformFreezeAppearanceEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_DrawMovingPlatformShadowsEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_UpdateMovingPlatformsEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_CollectMovingPlatformPositionsEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_InitializeMovingPlatformsEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_ResetMovingPlatformWaypointEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_SetMovingPlatformActorEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_SetMovingPlatformsPausedEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_FreezeMovingPlatformsEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_FreezeRoomGimmicksTemporarilyEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_SetMovingPlatformVisibilityEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_MovingPlatformDwellTicksEntry": r"Native\Game\src\Field\PlatformPlacementLive.h",
    "Field_ApplyUnlockEffectToRoomObjectEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_ToggleDoorCollisionEntry": r"Native\Game\src\Field\RoomSwitchesLive.h",
    "Field_BeginDoorToggleEntry": r"Native\Game\src\Field\RoomSwitchesLive.h",
    "Field_StartDoorAnimationEntry": r"Native\Game\src\Field\RoomSwitchesLive.h",
    "Field_FinishDoorAnimationEntry": r"Native\Game\src\Field\RoomSwitchesLive.h",
    "Field_InteractiveDoorAtTileEntry": r"Native\Game\src\Field\RoomSwitchesLive.h",
    "Field_MarkSigilDoorUnlockedEntry": r"Native\Game\src\Field\RoomSwitchesLive.h",
    "Field_IsRoomSigilDoorUnlockedEntry": r"Native\Game\src\Field\RoomSwitchesLive.h",
    "Field_SceneDoorIdEntry": r"Native\Game\src\Field\MinimapSceneLive.h",
    "Field_MarkEntranceDoorOnSceneMapEntry": r"Native\Game\src\Field\MinimapSceneLive.h",
    "Field_StoreEntranceDoorForReloadEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_SetEntranceDoorRecordEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_FindEntranceDoorNearTileEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_CheckAshleyTileTriggersEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_CheckActorTrapAndFallTriggersEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "VS::Field::LiveCollisionCellOrAbsent": r"Native\Game\include\Field\CollisionMapLive.hpp",
    "Field_TryTriggerFacingTileEventEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_FindFacingRoomObjectEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_FindFacingUnlootedChestEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_TileInFrontOfActorEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_WaitForChestCameraFocusEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_StepTrapCameraFocusEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_IsChestAnimationReadyEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_TrackInteractionCameraToAshleyEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_TriggerActorTrapEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_InteractWithLockedRoomObjectEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_BeginObjectInteractionWaitEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_EndObjectInteractionWaitEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_DispatchTrapOrChestInteractionEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_BeginTrapInteractionEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_FinishTrapInteractionEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_BeginChestInteractionEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_FinishChestInteractionEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_OpenChestContentsEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_UpdateChestContentsSelectionEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_TryOpenChestEntry": r"Native\Game\src\Field\ChestInteractionsLive.h",
    "Field_BeginSwitchToggleEntry": r"Native\Game\src\Field\RoomSwitchesLive.h",
    "Field_ResetGimmickAnimationQueueEntry": r"Native\Game\src\Field\GimmickAnimationQueueLive.h",
    "Field_UpdateGimmickAnimationQueueEntry": r"Native\Game\src\Field\GimmickAnimationQueueLive.h",
    "Field_QueueGimmickAnimationEntry": r"Native\Game\src\Field\GimmickAnimationQueueLive.h",
    "Field_ToggleRoomSwitchEntry": r"Native\Game\src\Field\RoomSwitchesLive.h",
    "Field_GimmickPlacementEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_FindGimmickBelowEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_GimmickSubtypeEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_AreGimmicksMovingEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_MeasureTileStackEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_IsGimmickWithinHeightBandEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_SetGimmickOrientationEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_CanPlaceGimmickAtPointEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_DropUnsupportedGimmickStackEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_FindInteractableGimmickAheadEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_BeginGimmickInteractionEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_ApplyGimmickInteractionCommandEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Field_UpdateGimmickReactionAnimationEntry": r"Native\Game\src\Field\GimmickReactionsLive.h",
    "Field_UpdateGimmickMotionEntry": r"Native\Game\src\Field\GimmickDynamicsLive.h",
    "Field_DisableTrapAtPositionEntry": r"Native\Game\src\Field\InteractionTargetLive.h",
    "Field_RoomTriggerAtTileEntry": r"Native\Game\src\Field\InteractionTargetLive.h",
    "Field_SetRoomTriggerEnabledEntry": r"Native\Game\src\Field\InteractionTargetLive.h",
    "Field_FindTopGimmickAtTileEntry": r"Native\Game\src\Field\GimmickPlacementLive.h",
    "Event_EvaluateInterpolationCurveEntry": r"Native\Game\src\Event\InterpolationCurveLive.h",
    "Event_EvaluateRoomEventTriggersEntry": r"Native\Game\src\Event\RoomTriggerEntryLive.h",
    "VS::Event::ResolveLiveScriptActorReference": r"Native\Game\include\Event\ScriptVmLive.hpp",
    **{f"VS::Core::Math::{name}": rf"Native\Engine\include\Core\Math\{header}"
       for header, names in _CORE_MATH_LIVE.items() for name in names},
    **{f"VS::Render::{name}": rf"Native\Engine\include\Render\{header}"
       for header, rows in _RENDER_LIVE.items() for name, _ in rows},
    **{f"VS::Items::{name}": header for header, rows in _ITEMS_LIVE.items() for name, _ in rows},
    **{f"VS::Actors::{name}": header for header, rows in _ACTORS_APPEARANCE_LIVE.items() for name, _ in rows},
    **{f"VS::Actors::{name}": header for header, rows in _ACTORS_TEXTURE_LIVE.items() for name, _ in rows},
    **{f"VS::Actors::{name}": header for header, rows in _ACTORS_MODEL_RENDERING_LIVE.items() for name, _ in rows},
    **{f"VS::Actors::{name}": header for header, rows in _ACTORS_SKELETON_LIVE.items() for name, _ in rows},
    **{f"VS::Actors::{name}": header for header, rows in _ACTORS_MOVEMENT_LIVE.items() for name, _ in rows},
    **{f"VS::Actors::{name}": header for header, rows in _ACTORS_ANIMATION_LIVE.items() for name, _ in rows},
    **{f"VS::Actors::{name}": header for header, rows in _ACTORS_ANIMATION_POSE_LIVE.items() for name, _ in rows},
    **{f"VS::Content::{name}": header for header, rows in _CONTENT_LIVE.items() for name, _ in rows},
    **{f"VS::State::{name}": header for header, rows in _STATE_LIVE.items() for name, _ in rows},
    **{f"VS::Boot::{name}": header for header, rows in _BOOT_LIVE.items() for name, _ in rows},
    **{f"VS::Field::{name}": header for header, rows in _FIELD_COLLISION_MAP_LIVE.items() for name, _ in rows},
    **{f"VS::Field::{name}": header for header, rows in _FIELD_ACTOR_COLLISION_LIVE.items() for name, _ in rows},
    **{f"VS::Field::{name}": r"Native\Game\include\Field\ActorCollisionLive.hpp"
       for name in ("LiveCollisionProbeCellWord", "LiveSampleSupportHeightAt")},
    "VS::Render::IsLiveFramebufferEffectShowing": r"Native\Engine\include\Render\ScreenOverlayLive.hpp",
    **{f"VS::Render::{name}": r"Native\Engine\include\Render\GimImagesLive.hpp"
       for name in ("LiveGimImageLoading", "LiveGimImagesPresent", "ConfigureLiveGimLayer", "LiveGimLayerPlacement",
                    "PlaceLiveGimLayer", "ForgetLiveGimImages")},
    "VS::Battle::Ai::CurrentTerrain": r"Native\Game\src\Battle\Ai\CurrentTerrain.hpp",
    "VS::Battle::Ai::BattleRoomAiRuntime": r"Native\Game\include\Battle\Ai\RoomAiMap.hpp",
    "VS::Battle::Ai::BattleActorReactionDelays": r"Native\Game\include\Battle\Ai\ReactionDelay.hpp",
    "VS::Actors::UpdateActorSoundTransition": r"Native\Game\src\Actors\SoundTransitionBinding.hpp",
    "VS::Actors::ActorRecordMovementRequests": r"Native\Game\include\Actors\MovementCommand.hpp",
    "VS::Battle::Ai::CaptureBattleActorConditions": r"Native\Game\src\Battle\Ai\ConditionSnapshotBinding.hpp",
    "VS::Field::LiveRoomCollisionMap": r"Native\Game\include\Field\CollisionSampling.hpp",
    "VS::Audio::Seq::ReleaseLiveSavedVoiceConflicts": r"Native\Engine\include\Audio\MusicVoiceConflicts.hpp",
    "VS::Audio::Driver::ResetLivePlaybackState": r"Native\Engine\include\Audio\PlaybackResetLive.hpp",
    "VS::Effects::BindLiveEffectTextureTables": r"Native\Game\include\Effects\TextureBindings.hpp",
    **{name: r"Native\Engine\include\Render\SpritesLive.hpp"
       for name in ("VS::Render::QueueLiveDefaultSprite", "VS::Render::QueueLiveSpriteWithTexturePage",
                    "VS::Render::QueueLiveSprite", "VS::Render::LoadLiveFrameViewMatrix")},
    **{f"VS::Render::{name}": r"Native\Engine\include\Render\ModelGeometryLive.hpp"
       for name in ("ProjectLiveGimmickModel", "DrawLiveGimmickGouraudPolygons",
                    "DrawLiveGimmickGouraudPolygonsWithOverrides", "ProjectLiveWeaponModel",
                    "DrawLiveWeaponFlatPolygons", "DrawLiveWeaponFlatPolygonsWithOverrides", "RecordLiveModelExtent",
                    "RecordLiveModelCopy", "ForgetLiveModelExtent")},
    **{f"VS::Render::{name}": r"Native\Engine\include\Render\RoomDrawLive.hpp"
       for name in ("LiveRoomClipState", "ValidateLiveRoomGeometry", "UpdateLiveRoomGroupTransforms",
                    "DrawLiveRoomGeometry", "BuildLiveRoomGeometryViewMatrix", "BuildLiveScaleCompensatedRoomView",
                    "DrawLiveSkyDome", "TintLiveRoomGeometryPrimitives", "GetLiveProjectionScreenOffset",
                    "SetLiveProjectionScreenOffset")},
    **{name: r"Native\Game\include\Boot\SessionLive.hpp"
       for name in ("VS::Boot::JumpToLiveTitle", "VS::Boot::JumpToLiveGameLoop")},
    "VS::Boot::DelayAfterOverlayLoad": r"Native\Game\include\Boot\Session.hpp",
    **{f"VS::Battle::Ai::{name}": r"Native\Game\include\Battle\Ai\DecisionsLive.hpp"
       for name in ("UpdateLivePatrolCombatBehavior", "UpdateLiveFleeOrWanderBehavior",
                    "UpdateLiveAdaptivePursuitBehavior", "UpdateLiveSupportFollowBehavior",
                    "UpdateLiveEngagementApproachBehavior", "UpdateLiveActorLocomotionMode",
                    "DispatchLiveMovementBehavior", "SelectLiveDecisionBehavior")},
    **{f"VS::Battle::Ai::{name}": r"Native\Game\include\Battle\Ai\FrameUpdateLive.hpp"
       for name in ("CacheLiveActorAiWorkingState", "RunLiveActorAiUpdateStages", "TickLiveActorDecisionTimers",
                    "InitializeLiveActorLocomotionMode", "SetLiveActorVerticalMovementSteps")},
    **{f"VS::Core::Memory::{name}": r"Native\Engine\include\Core\Memory\WordCopyLive.hpp"
       for name in ("CopyLiveAlignedWords", "ZeroLiveAlignedWords", "ClearLiveAlignedWords", "CopyLiveHalfwords",
                    "CopyLiveHalfwordBlocks")},
    **{f"VS::Core::Memory::{name}": r"Native\Engine\include\Core\Memory\CopyLive.hpp"
       for name in ("ReadLiveUnalignedShort", "ZeroLiveWordsBackward", "CopyLiveBytesBackward", "CopyLiveWords",
                    "SwapLiveWords", "ReadLiveUnalignedShortAt", "ZeroLiveWordsBackwardAt",
                    "CopyLiveBytesBackwardAt", "CopyLiveWordsAt", "SwapLiveWordsAt")},
    **{f"VS::Core::Memory::{name}": r"Native\Engine\include\Core\Memory\HeapLive.hpp"
       for name in ("AllocateLiveFromLowEnd", "AllocateLiveFromHighEnd", "FreeLiveFromLowEnd", "FreeLiveFromHighEnd",
                    "ResetLiveGameHeap", "AllocateLiveFromLowEndAt", "AllocateLiveFromHighEndAt",
                    "FreeLiveFromLowEndAt", "FreeLiveFromHighEndAt", "InitializeLiveHeapAt")},
    **{f"VS::Core::{name}": r"Native\Engine\include\Core\Runtime.hpp"
       for name in ("IgnoreCall", "ReturnZero", "IgnoreDiagnostic", "IdleInstruction", "NoOperation")},
    "VS::Core::IgnoreLiveActorCallback": r"Native\Engine\include\Core\RuntimeLive.hpp",
    **{f"VS::Core::{name}": r"Native\Engine\include\Core\RandomLive.hpp"
       for name in ("NextLiveInterfaceRandomBits", "LiveRandomRangeExclusive", "InitializeLiveRandomShuffleTable",
                    "NextLiveRandomBounded", "NextLiveRandomAveraged")},
    **{f"VS::Core::String::{name}": r"Native\Engine\include\Core\String\GameTextLive.hpp"
       for name in ("CopyLiveGameText", "FormatLiveGameInteger", "FormatLiveGamePercent")},
    **{f"VS::Render::DrawLiveTexture{kind}": r"Native\Engine\include\Render\TextureRegionDrawLive.hpp"
       for kind in ("TintFade", "FlashFade", "Wipe")},
    "VS::Battle::CheckedTargetPreviews": r"Native\Game\include\Battle\TargetPages.hpp",
    **{f"VS::Actors::UpdateLive{kind}": r"Native\Game\include\Actors\AppearanceTransitionLive.hpp"
       for kind in ("ActorPalettePulse", "GimmickPalettePulse", "ActorAppearanceTransition",
                    "GimmickAppearanceTransition")},
    "VS::Menu::Files::UpdateLiveContainerSave": r"Native\Game\include\Save\NativeContainerLive.hpp",
    **{name: r"Native\Game\include\Actors\SequencePlaybackLive.hpp"
       for name in ("VS::Actors::AdvanceLiveSkeletalAnimation", "VS::Actors::BlendLiveActorPose",
                    "VS::Actors::RegisterSequenceBank", "VS::Actors::RegisterSequenceBankCopy",
                    "VS::Actors::RegisterEmbeddedSequenceBank")},
    **{name: r"Native\Game\include\Save\MenuAssetsLive.hpp"
       for name in ("VS::Save::MemoryCard::StepLiveMenuResources", "VS::Save::MemoryCard::ReleaseLiveMenuResources",
                    "VS::Save::MemoryCard::StepLiveTitleResources", "VS::Save::MemoryCard::ReleaseLiveTitleResources")},
    **{f"VS::Save::MemoryCard::{name}": r"Native\Game\include\Save\MemoryCard\GameFilesLive.hpp"
       for name in _SAVE_MEMORY_CARD_LIVE},
    **{f"VS::Save::{name}": r"Native\Game\include\Save\SnapshotLive.hpp" for name in _SAVE_SNAPSHOT_LIVE},
    **{name: r"Native\Game\include\Actors\ModelDrawingLive.hpp"
       for name in ("VS::Actors::DrawLiveActorModel", "VS::Actors::ResolveLiveAttachmentParentMatrix",
                    "VS::Actors::ProjectLiveActorBoneEndpoint")},
    "VS::Battle::DrawLiveSphereTerrainIntersection": r"Native\Game\include\Battle\SphereTerrainLive.hpp",
    "VS::Battle::Ai::RebuildLiveActorActionPlans": r"Native\Game\include\Battle\Ai\PlanningWorkspaceLive.hpp",
    "VS::Battle::Ai::IssueLiveActorCommand": r"Native\Game\include\Battle\Ai\ActorCommandsLive.hpp",
    "VS::Battle::Ai::ApplyLiveSavedAiSettingsFlags": r"Native\Game\include\Battle\Ai\ActorSettingsLive.hpp",
    "VS::Battle::Ai::ApplyLiveActorAiSettings": r"Native\Game\include\Battle\Ai\ActorSettingsLive.hpp",
    "VS::Battle::Ai::CreateLiveActorAiState": r"Native\Game\include\Battle\Ai\ActorCreationLive.hpp",
    **{name: r"Native\Game\include\Battle\Ai\RelationsLive.hpp"
       for name in ("VS::Battle::Ai::InitializeLiveActorAiRelations",
                    "VS::Battle::Ai::ApplyLiveSavedActorAiRelations",
                    "VS::Battle::Ai::EndLiveAshleyAlliance", "VS::Battle::Ai::TryBeginLiveAshleyAlliance",
                    "VS::Battle::Ai::ResolveLiveAiLinkedActors")},
    **{f"VS::Battle::Ai::{name}": r"Native\Game\include\Battle\Ai\EngagementLive.hpp"
       for name in ("SetLiveActorAwarenessIndicator", "ResetLiveActorAwarenessIndicator", "IsLiveEngagementReady",
                    "UpdateLiveTemporaryAwarenessIndicator", "DisengageLiveActorById", "DisengageLiveActor",
                    "TryEngageLiveActor")},
    **{f"VS::Battle::Ai::{name}": r"Native\Game\include\Battle\Ai\DecisionPrimitivesLive.hpp"
       for name in ("DoesLiveActionRequireCooldown", "IsLiveActionCooldownReady", "CanExecuteLiveActionCandidate",
                    "ScoreLiveActorCondition", "IsLiveActionHarmfulToActor", "IsLiveActionBeneficialToActor",
                    "CacheLiveFallbackActionCandidate", "PromoteLiveRandomizedActionCandidate",
                    "NextLiveActorRandomMasked", "IsLiveAiTargetBeyondNearRadius", "CanApproachLiveEngagedTarget")},
    **{f"VS::Battle::Ai::{name}": r"Native\Game\include\Battle\Ai\PlanningPrimitivesLive.hpp"
       for name in ("RememberLiveAiActionTarget", "ForgetLiveAiActionTarget", "CopyLiveActionCandidate",
                    "ApplyLiveSimulatedStatDelta",
                    "AppendLiveActionCandidate", "SortLiveActionCandidates", "CopyLivePlanningActorStats",
                    "SnapshotLivePlanningActorStats", "RestoreLivePlanningActorStats",
                    "CanLiveAiActionAffectActor", "RefreshLiveAiActionUsability",
                    "RefreshLiveAiResourceRestrictions", "LiveActorStatusConditionScore", "LiveActorHpRatio",
                    "LiveActorMpRatio", "LiveActorEquipmentDpRatio", "BuildLiveAiActionMetadata")},
    **{f"VS::Battle::Ai::{name}": r"Native\Game\include\Battle\Ai\MovementBehaviorLive.hpp"
       for name in ("SetLiveMovementBehavior", "RefreshLiveAiMovementCooldown", "ApproachLiveActor",
                    "RefreshLiveAiTargetTile", "RefreshLiveAiTargetTileWhenTracking", "WanderAroundLiveSavedCenter",
                    "WanderAroundLiveTile", "MoveLiveActorToTile", "PatrolLiveWaypoints", "FleeLiveVisibleEnemies",
                    "TrackLiveActorPosition", "FindLiveActorByRelationship", "LiveTargetFacingCosine",
                    "SelectLiveActionMovementMode", "ApplyLiveActionRelationshipChanges")},
    **{f"VS::Battle::Ai::{name}": r"Native\Game\include\Battle\Ai\ActorCleanupLive.hpp"
       for name in ("ReleaseLiveAiDestinationTile", "FreeLiveActorNavigationOverride",
                    "ForceReleaseLiveActorAiReservations", "ReleaseLiveActorAiReservations", "FreeLiveActorAiState",
                    "AllocateLiveOrbitingBossState", "SetLiveActorAiReactionWithDelay", "MakeLiveTargetHostile",
                    "RefreshLiveActorAiReservations", "ReleaseLiveActorAiContactOccupancy",
                    "ClearLiveActorAiLinksAndPlan", "PackLiveActorAiPersistenceFlags",
                    "ClearLiveActorAiNavigationReset", "InitializeLiveActorAiExtensionHook")},
    "VS::Effects::ScreenDistortion::WarpLivePolygonVertices":
        r"Native\Game\include\Effects\ScreenDistortion\RadialWarpLive.hpp",
    "VS::Battle::DrawLiveSphereVolumeWireframe": r"Native\Game\include\Battle\TargetingDrawLive.hpp",
    "VS::Battle::ApplyLiveActionTargetEffects": r"Native\Game\include\Battle\ActionResultsLive.hpp",
    "VS::Field::TransitionLiveDoorsAtTile": r"Native\Game\include\Field\DoorMatchesLive.hpp",
    "VS::Field::DrawLiveRoomTriggerMarkers": r"Native\Game\include\Field\RoomTriggerMarkersLive.hpp",
    "VS::Field::SampleLiveRoomLighting": r"Native\Game\include\Field\RoomLightingLive.hpp",
    "VS::Field::ResolveLiveSceneRoomData": r"Native\Game\include\Field\MinimapSceneLive.hpp",
    "VS::Field::DrawLiveMinimapScene": r"Native\Game\include\Field\MinimapSceneLive.hpp",
    "VS::Field::IsLivePointOutsideSolidVolumes": r"Native\Game\include\Field\SegmentCollisionLive.hpp",
    "VS::Field::TraceLiveActorAndTerrainCollision": r"Native\Game\include\Field\SegmentCollisionLive.hpp",
    "VS::Effects::DrawLiveEffectParticleList": r"Native\Game\include\Effects\ParticleRenderLive.hpp",
    "VS::Effects::DrawLiveAnimatedSpriteParticles": r"Native\Game\include\Effects\ParticleRenderLive.hpp",
    "VS::Effects::UpdateLivePlg074TrailEmitter": r"Native\Game\include\Effects\TrailEmitterLive.hpp",
    **{f"VS::Effects::{name}": r"Native\Game\include\Effects\PackageExtents.hpp"
       for name in ("ForgetLivePackageExtents", "RecordLiveParticleDefinitions", "RecordLiveEffectCurves")},
    **{name: r"Native\Engine\include\Movie\IntroPlayerLive.hpp"
       for name in ("VS::Movie::Intro::StartLive", "VS::Movie::Intro::StopLive",
                    "VS::Movie::Intro::StopLiveAfterAudioTail", "VS::Movie::Intro::PlayLive")},
    **{name: r"Native\Content\include\Content\DiscReaderLive.hpp"
       for name in ("VS::Content::BeginLiveDiscRead", "VS::Content::UpdateLiveDiscCommands",
                    "VS::Content::ResetLiveDiscDrive")},
    **{name: r"Native\Engine\include\Audio\XaMixLive.hpp"
       for name in ("VS::Audio::Control::SetLiveDiscMixVolume", "VS::Audio::Seq::WriteLiveDiscVolume")},
    **{name: r"Native\Game\include\Field\PlatformPlacementLive.hpp"
       for name in ("VS::Field::LiveMovingPlatformPlacement", "VS::Field::FindLiveMovingPlatformById")},
    "Event_ScriptSetAshleyBattleModeOpcode": r"Native\Game\src\Event\WeaponModeOpcode.h",
    "VS::Battle::Ai::ActorFlightNeighborhood": r"Native\Game\src\Battle\Ai\CurrentTerrain.hpp",
    "VS::Battle::Ai::FloodFillLiveAiTerrainHeights": r"Native\Game\include\Battle\Ai\SpecialTerrainLive.hpp",
    "VS::Battle::Ai::FindLiveAdjacentAiGroundHeight": r"Native\Game\include\Battle\Ai\SpecialTerrainLive.hpp",
    "VS::Battle::Ai::BuildLiveAiSpecialTerrainHeights": r"Native\Game\include\Battle\Ai\SpecialTerrainLive.hpp",
    "VS::Battle::Ai::InitializeLiveRoomNavigation": r"Native\Game\include\Battle\Ai\RoomAiMapLive.hpp",
    "VS::Battle::Ai::ResolveLiveRouteGateExit": r"Native\Game\include\Battle\Ai\AreaRouteLive.hpp",
    "VS::Battle::Ai::FindLiveNearestRouteGateEntry": r"Native\Game\include\Battle\Ai\AreaRouteLive.hpp",
    "VS::Battle::Ai::FindLiveRouteGateAtTile": r"Native\Game\include\Battle\Ai\AreaRouteLive.hpp",
    "VS::Battle::Ai::ResolveLiveNavigationAreaRoute": r"Native\Game\include\Battle\Ai\AreaRouteLive.hpp",
    "VS::Battle::Ai::FindLiveNearestActiveWaypoint": r"Native\Game\include\Battle\Ai\WaypointsLive.hpp",
    "VS::Battle::Ai::UpdateLiveWaypointPatrol": r"Native\Game\include\Battle\Ai\WaypointsLive.hpp",
    "VS::Battle::Ai::UpdateLiveNavigationAreaMovement": r"Native\Game\include\Battle\Ai\AreaRouteLive.hpp",
    "VS::Battle::Ai::FilterLiveDirectionsByGroundHeight": r"Native\Game\include\Battle\Ai\AreaRouteLive.hpp",
    "VS::Battle::Ai::MoveLiveWithHeightFilteredDirections": r"Native\Game\include\Battle\Ai\AreaRouteLive.hpp",
    "VS::Battle::Ai::ChooseNextLiveWanderTile": r"Native\Game\include\Battle\Ai\WanderLive.hpp",
    "VS::Battle::Ai::UpdateLiveWanderMovement": r"Native\Game\include\Battle\Ai\WanderLive.hpp",
    "VS::Battle::Ai::ProjectLiveClearRadialDestination": r"Native\Game\include\Battle\Ai\RadialMovementLive.hpp",
    "VS::Battle::Ai::MoveLiveToRadialDestination": r"Native\Game\include\Battle\Ai\RadialMovementLive.hpp",
    "VS::Battle::Ai::MoveLiveAwayFromReference": r"Native\Game\include\Battle\Ai\RadialMovementLive.hpp",
    "VS::Battle::Ai::RecoverLiveMovement": r"Native\Game\include\Battle\Ai\RadialMovementLive.hpp",
    "VS::Battle::Ai::FindLiveAdjacentMovementDestination": r"Native\Game\include\Battle\Ai\BlockedMovementLive.hpp",
    "VS::Battle::Ai::IsLiveMovementHistoryStalled": r"Native\Game\include\Battle\Ai\BlockedMovementLive.hpp",
    "VS::Battle::Ai::SaveLiveMovementRandomState": r"Native\Game\include\Battle\Ai\BlockedMovementLive.hpp",
    "VS::Battle::Ai::TryRecoverLiveBlockedMovement": r"Native\Game\include\Battle\Ai\BlockedMovementLive.hpp",
    "VS::Battle::Ai::IsLiveFlightDirectionObstructed": r"Native\Game\include\Battle\Ai\FlightMovementLive.hpp",
    "VS::Battle::Ai::CanLiveActorFlyInDirection": r"Native\Game\include\Battle\Ai\FlightMovementLive.hpp",
    "VS::Battle::Ai::UpdateLiveFlightGroundTransition": r"Native\Game\include\Battle\Ai\FlightMovementLive.hpp",
    "VS::Battle::Ai::UpdateLiveFlightMovement": r"Native\Game\include\Battle\Ai\FlightMovementLive.hpp",
    "VS::Battle::Ai::UpdateLiveFlightDirectionalMovement": r"Native\Game\include\Battle\Ai\FlightMovementLive.hpp",
    "VS::Battle::Ai::SetLiveTemporaryAwarenessIndicator": r"Native\Game\include\Battle\Ai\EngagementLive.hpp",
    "VS::Battle::Ai::FollowLiveObstacleDetour": r"Native\Game\include\Battle\Ai\GroundNavigationLive.hpp",
    "VS::Battle::Ai::UpdateLiveTargetApproachMovement": r"Native\Game\include\Battle\Ai\TargetApproachLive.hpp",
    "VS::Battle::Ai::UpdateLiveTargetFacing": r"Native\Game\include\Battle\Ai\TargetApproachLive.hpp",
    "VS::Battle::Ai::UnlinkLiveEngagedActor": r"Native\Game\include\Battle\Ai\EngagementLive.hpp",
    "VS::Battle::Ai::UpdateLiveEngagementBuildup": r"Native\Game\include\Battle\Ai\EngagementLive.hpp",
    "VS::Battle::Ai::AnimateLiveEngagementPhase": r"Native\Game\include\Battle\Ai\EngagementLive.hpp",
    "VS::Battle::Ai::ApplyLiveActorActionReaction": r"Native\Game\include\Battle\Ai\RelationsLive.hpp",
    "VS::Battle::Ai::PropagateLiveActorRelationships": r"Native\Game\include\Battle\Ai\RelationsLive.hpp",
    "VS::Battle::Ai::NotifyLiveActionRelationshipChanges": r"Native\Game\include\Battle\Ai\RelationsLive.hpp",
    "VS::Battle::Ai::UpdateLiveBossRadialRetreat": r"Native\Game\include\Battle\Ai\BossOrbitLive.hpp",
    "VS::Battle::Ai::IsLiveBossActionReady": r"Native\Game\include\Battle\Ai\BossOrbitLive.hpp",
    "VS::Battle::Ai::UpdateLiveBossOrbitTracking": r"Native\Game\include\Battle\Ai\BossOrbitLive.hpp",
    "VS::Battle::Ai::UpdateLiveOrbitingBossBehavior": r"Native\Game\include\Battle\Ai\BossOrbitLive.hpp",
    "VS::Battle::Ai::UpdateLiveTargetPerception": r"Native\Game\include\Battle\Ai\ActorPerceptionLive.hpp",
    "VS::Battle::Ai::UpdateLiveActorPerception": r"Native\Game\include\Battle\Ai\ActorPerceptionLive.hpp",
    "VS::Battle::Ai::MoveLiveForActionCandidate": r"Native\Game\include\Battle\Ai\CandidateMovementLive.hpp",
    "VS::Battle::Ai::UpdateLiveCombatPursuit": r"Native\Game\include\Battle\Ai\CandidateMovementLive.hpp",
    "VS::Battle::Ai::TryApproachLivePlannedTarget": r"Native\Game\include\Battle\Ai\CandidateMovementLive.hpp",
    "VS::Battle::Ai::UpdateLiveCloseFollowBehavior": r"Native\Game\include\Battle\Ai\CandidateMovementLive.hpp",
    "VS::Battle::Ai::UpdateLiveEnemyPursuitBehavior": r"Native\Game\include\Battle\Ai\CandidateMovementLive.hpp",
    "VS::Battle::Ai::UpdateLiveGuardMovementBehavior": r"Native\Game\include\Battle\Ai\CandidateMovementLive.hpp",
    "VS::Battle::Ai::UpdateLiveEscortCombatBehavior": r"Native\Game\include\Battle\Ai\CandidateMovementLive.hpp",
    "VS::Battle::Ai::TryExecuteLiveActionCandidate": r"Native\Game\include\Battle\Ai\ActionExecutionLive.hpp",
    "VS::Battle::Ai::TryExecuteLivePlannedActions": r"Native\Game\include\Battle\Ai\ActionExecutionLive.hpp",
    "VS::Battle::Ai::UpdateLiveActorDecisions": r"Native\Game\include\Battle\Ai\ActionExecutionLive.hpp",
    "VS::Battle::QueueLiveApplicableStatusChanges": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveStatusRemovalMask": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveStrengthDown": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveIntelligenceDown": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveAgilityDown": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveEquipmentDown": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveHalfMovement": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveStrengthUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveIntelligenceUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveAgilityUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveEquipmentUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveQuicken": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveSilence": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveMagicWard": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveRegeneration": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveBanish": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveParalysis": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLivePoison": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveNumbness": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveCurse": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveAirAttackUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveFireAttackUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveEarthAttackUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveWaterAttackUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveAirDefenseUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveFireDefenseUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveEarthDefenseUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveWaterDefenseUp": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveParalysisCure": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLivePoisonCure": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveCurseCure": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveNumbnessCure": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveDispel": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::QueueLiveNegativeStatusCure": r"Native\Game\include\Battle\StatusQueueLive.hpp",
    "VS::Battle::ApplyLiveWeaponElementEnchantment": r"Native\Game\include\Battle\StatusHelpersLive.hpp",
    "VS::Battle::ApplyLiveArmorElementEnchantment": r"Native\Game\include\Battle\StatusHelpersLive.hpp",
    "VS::Battle::QueueLiveActorStatChangeNumber": r"Native\Game\include\Battle\StatusHelpersLive.hpp",
    "VS::Battle::UpdateLiveTargetingRiskPenalty": r"Native\Game\include\Battle\StatusHelpersLive.hpp",
    "VS::Battle::ResetLiveTargetingRiskPenaltyTimer": r"Native\Game\include\Battle\StatusHelpersLive.hpp",
    "VS::Battle::PreserveLiveAshleyStatuses": r"Native\Game\include\Battle\StatusLifecycleLive.hpp",
    "VS::Battle::RemoveLiveActorStatuses": r"Native\Game\include\Battle\StatusLifecycleLive.hpp",
    "VS::Battle::InflictLiveActorStatuses": r"Native\Game\include\Battle\StatusLifecycleLive.hpp",
    "VS::Battle::UpdateLiveActorStatusTimers": r"Native\Game\include\Battle\StatusLifecycleLive.hpp",
    "VS::Battle::ResolveLiveActorDefeat": r"Native\Game\include\Battle\ActorVitalsLive.hpp",
    "VS::Battle::ApplyLiveActorActionResult": r"Native\Game\include\Battle\ActionResultsLive.hpp",
    "VS::Battle::UpdateLiveActorVitals": r"Native\Game\include\Battle\ActorVitalsLive.hpp",
    "VS::Battle::UpdateLiveRoomActorVitals": r"Native\Game\include\Battle\ActorVitalsLive.hpp",
    "VS::Battle::AttackLiveGemHitBonus": r"Native\Game\include\Battle\HitModifiersLive.hpp",
    "VS::Battle::DefenseLiveGemHitBonus": r"Native\Game\include\Battle\HitModifiersLive.hpp",
    "VS::Battle::PreviousLiveHitSuccessThreshold": r"Native\Game\include\Battle\HitModifiersLive.hpp",
    "VS::Battle::LiveGemHitChance": r"Native\Game\include\Battle\HitModifiersLive.hpp",
    "VS::Battle::LiveRiskHitChance": r"Native\Game\include\Battle\HitModifiersLive.hpp",
    "VS::Battle::LiveChainHitChance": r"Native\Game\include\Battle\HitModifiersLive.hpp",
    "VS::Battle::LiveAgilityHitChance": r"Native\Game\include\Battle\HitCalculationsLive.hpp",
    "VS::Battle::LiveIntelligenceHitChance": r"Native\Game\include\Battle\HitCalculationsLive.hpp",
    "VS::Battle::LiveCostAdjustedAgilityHitChance": r"Native\Game\include\Battle\HitCalculationsLive.hpp",
    "VS::Battle::LiveRollActionHit": r"Native\Game\include\Battle\HitCalculationsLive.hpp",
    "VS::Battle::LiveCalculateActionStatChange": r"Native\Game\include\Battle\HitCalculationsLive.hpp",
    "VS::Battle::RejectLiveHitSelector": r"Native\Game\include\Battle\HitCalculationsLive.hpp",
    "VS::Battle::RejectLiveStatSelector": r"Native\Game\include\Battle\HitCalculationsLive.hpp",
    "VS::Battle::RejectLiveEffectSelector": r"Native\Game\include\Battle\HitCalculationsLive.hpp",
    "VS::Battle::LiveScalePreviousTargetHit": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveScaleSourceMissingHp": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveScaleTargetCurrentHp": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveWeaponPpWithChainOffset": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveScaleIncomingSpellDamage": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveScaleSourceMissingMp": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveScaleIncomingPhysicalDamage": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveTargetMissingHp": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveTargetMissingMp": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveScaleIncomingSpellCost": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveScaleIncomingDamage": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveSourceShieldPp": r"Native\Game\include\Battle\StatFactorsLive.hpp",
    "VS::Battle::LiveApplyDamageGemMultiplier": r"Native\Game\include\Battle\DamagePoliciesLive.hpp",
    "VS::Battle::LiveDistributeHitLimbDamage": r"Native\Game\include\Battle\DamagePoliciesLive.hpp",
    "VS::Battle::LiveDistributeLimbRecovery": r"Native\Game\include\Battle\DamagePoliciesLive.hpp",
    "VS::Battle::LiveLastActionStatValue": r"Native\Game\include\Battle\DamagePoliciesLive.hpp",
    "VS::Battle::LiveScaleLastActionStatValue": r"Native\Game\include\Battle\DamagePoliciesLive.hpp",
    "VS::Battle::LiveTargetRisk": r"Native\Game\include\Battle\DamagePoliciesLive.hpp",
    "VS::Battle::LiveCalculatePhysicalHitDamage": r"Native\Game\include\Battle\DamageCalculationsLive.hpp",
    "VS::Battle::LivePhysicalDamageWithoutDebuffs": r"Native\Game\include\Battle\DamageCalculationsLive.hpp",
    "VS::Battle::LiveCalculateSpellHitDamage": r"Native\Game\include\Battle\DamageCalculationsLive.hpp",
    "VS::Battle::LiveCalculateSpellHealing": r"Native\Game\include\Battle\DamageCalculationsLive.hpp",
    "VS::Battle::LivePhysicalDamageWithDebuffs": r"Native\Game\include\Battle\DamageCalculationsLive.hpp",
    # Reference adapters the scalar matrix REGEX_FIXES substitute for in-place matrix calls.
    **{f"VS::Core::Math::Apply{name}": r"Native\Engine\include\Core\Math\ScalarMath.hpp"
       for name in (
           "MultiplyMatrixIntoRight", "MultiplyMatrixIntoLeft", "MultiplyRotationMatrixInPlace",
           "PremultiplyRotationMatrix", "BuildEulerRotationMatrix",
           "BuildEulerRotationMatrixQuadrant", "ScaleMatrixColumns",
       )},
    "Field_FindGimmickForActorSlot":
        r"Native\Game\src\Field\GimmickLookup.h",
    **{name: r"Native\Game\src\Ending\Credits\EmbeddedArt.h"
       for name in (
           "Ending_Credits_PrepareEmbeddedArt", "Ending_Credits_EmbeddedArt",
       )},
    **{name: r"Native\Game\include\Diagnostics\SceneTrace.h"
       for name in (
           "Diagnostics_RecordSceneCommand", "Diagnostics_RecordSceneResult",
           "Diagnostics_RecordScriptFlagWrite",
       )},
    **{f"VS::Battle::Ai::{name}": r"Native\Game\include\Battle\Ai\NavigationPrimitivesLive.hpp"
       for name in ("UpdateLiveRouteGateAvailability", "IsLiveAdjacentTileStepBlocked", "ReserveLiveAiDestinationTile",
                    "LiveActorNavigationOverrideStatus", "HasLiveActorNavigationState",
                    "CopyLiveActorNavigationOverrideState", "SetLiveActorNavigationOverride",
                    "UpdateLiveActorNavigationOverrides", "AddLiveAiTileOccupant", "RemoveLiveAiTileOccupant",
                    "MarkLiveAiTileDynamicObstacle", "BuildLiveAiTerrainHeightsWithWorkspace")},
    **{f"VS::Battle::Ai::{name}": r"Native\Game\include\Battle\Ai\GroundNavigationLive.hpp"
       for name in ("LiveDestinationOccupancy", "CanTraverseLiveTileEdge", "LiveDirectionalBlockingActor",
                    "CanLiveActorWalkInDirection", "FindLiveGapLandingTile", "ResolveLiveDirectionalMovement",
                    "SetLiveDetourBoundary", "RefreshLiveDetourProgress", "LiveCardinalPassabilityMask",
                    "InitializeLiveObstacleDetour", "UpdateLiveHesitationTurn",
                    "SelectLiveActorVerticalMovementStep", "IsTileBehindLiveActorTarget", "FindLiveAiDescentTile",
                    "BeginLiveAiTerrainMovement", "BeginLiveAiAlternateTerrainMovement",
                    "MoveLiveActorTowardDestinationTile", "BeginLiveAiGroundMovement",
                    "ClearLiveAiContactOccupancy", "SetLiveAiContactOccupancy", "BeginLiveAiMovementTowardPoint",
                    "DispatchLiveAiLocomotionMode")},
    **{f"VS::Actors::{name}": r"Native\Game\include\Actors\MotionRequestsLive.hpp"
       for name in _MOTION_REQUEST_LIVE.values()},
    **{f"VS::Menu::{name}": r"Native\Game\include\Save\FileMenuLive.hpp"
       for name in _FILE_MENU_LIVE.values()},
    "Save_NativeInitialTitleSelection": r"Native\Game\include\Save\NativeMenu.h",
    "Save_NativeDrawFileOverlay": r"Native\Game\include\Save\NativeMenu.h",
    **{name: r"Native\Game\include\Save\NativeResume.h"
       for name in (
           "Save_NativeResumePending", "Save_NativeFinishResume",
           "Save_NativeCancelResume", "Save_NativeResetSession",
       )},
    "Battle_Ai_TerrainRegionCell": r"Native\Game\src\Battle\Ai\FrameUpdate.h",
    "Audio_Seq_LfoWaveform": r"Native\Engine\include\Audio\LfoWaveforms.h",
    **{name: r"Native\Engine\include\Audio\MusicState.h"
       for name in (
           "Audio_Native_SaveMusicSnapshot",
           "Audio_Native_RestoreMusicSnapshot",
       )},
    **{name: r"Native\Engine\include\Audio\StreamPlayback.h"
       for name in (
           "Audio_Native_UpdateCutsceneStream",
           "Audio_Native_ResetCutsceneStream",
       )},
    **{name: r"Native\Engine\include\Audio\SequencerOutput.h"
       for name in (
           "Audio_Native_Reset", "Audio_Native_Tick",
           "Audio_Native_ConfigureChannel", "Audio_Native_Envelope",
           "Audio_Native_SetNoiseClock",
           "Audio_Native_ReverbMode", "Audio_Native_SetReverbMode",
           "Audio_Native_SetReverbDepth", "Audio_Native_EnableReverb",
       )},
    **{name: r"Native\Engine\include\Audio\VoiceOutputLive.hpp"
       for name in (
           "VS::Audio::Driver::KeyOnLiveVoices",
           "VS::Audio::Driver::KeyOffLiveVoices",
           "VS::Audio::Driver::SetLiveVoiceReverbMask",
           "VS::Audio::Driver::SetLiveVoiceNoiseMask",
           "VS::Audio::Driver::SetLiveVoiceFrequencyModulationMask",
           "VS::Audio::Driver::SetLiveVoiceVolume",
           "VS::Audio::Driver::SetLiveVoiceSampleRate",
           "VS::Audio::Driver::SetLiveVoiceReleaseRateAndMode",
           "VS::Audio::Samples::UploadLiveBank",
       )},
    **{f"VS::Audio::Seq::Live{name}": r"Native\Engine\include\Audio\SeqVoiceCommandsLive.hpp"
       for name in (
           "SetVolumeBalance",
           "SlideVolumeBalance",
           "SetVolume",
           "SlideVolume",
           "SetKeyOnVolumeRamp",
           "EnableFullNoteLength",
           "DisableFullNoteLength",
           "SetPan",
           "SlidePan",
           "SetOctave",
           "IncreaseOctave",
           "DecreaseOctave",
           "LoadInstrument",
           "LoadSilentInstrument",
           "SelectPatch",
           "ResetEnvelope",
           "SetTranspose",
           "AddTranspose",
           "SetPitchBendSlide",
           "EnablePortamento",
           "DisablePortamento",
           "SetFineTune",
           "AddFineTune",
           "EnableVibrato",
           "SetVibratoDepth",
           "SlideVibratoDepth",
           "DisableVibrato",
           "EnableTremolo",
           "SetTremoloDepth",
           "SlideTremoloDepth",
           "DisableTremolo",
           "EnableAutoPan",
           "SetAutoPanDepth",
           "SlideAutoPanDepth",
           "DisableAutoPan",
           "EnableNoise",
           "DisableNoise",
           "EnableFrequencyModulation",
           "DisableFrequencyModulation",
           "EnableReverb",
           "DisableReverb",
           "EnableLegato",
           "EnableSustainedSoundEffect",
           "SetNoiseClock",
           "SetAttackRate",
           "SetDecayRate",
           "SetSustainLevel",
           "SetSustainRate",
           "SetReleaseRate",
           "SetAttackMode",
           "SetSustainMode",
           "SetReleaseMode",
           "SetNextNoteLength",
           "SetFixedNoteLength",
           "EnableDrumKeymap",
           "MuteInstrument",
           "SetDecayAndSustain",
           "EnableTimedNoise",
           "SetNoiseToggleDelay",
           "EnableTimedFrequencyModulation",
           "SetFrequencyModulationToggleDelay",
           "ResetVoiceEffects",
           "EnablePitchSidechain",
           "DisablePitchSidechain",
           "EnableVolumeSidechain",
           "DisableVolumeSidechain",
           "EnableLoopBoundaryStop",
           "SetRandomPitchDepth",
           "ResetRandomPitchDepth",
       )},
    **{f"VS::Audio::Seq::Live{name}": r"Native\Engine\include\Audio\SeqControlCommandsLive.hpp"
       for name in (
           "FinishChannel",
           "FinishChannelOnReplacement",
           "StopOnInvalidInstruction",
           "IgnoreInstruction",
           "SetTempo",
           "SlideTempo",
           "SetReverbDepth",
           "SlideReverbDepth",
           "JumpRelative",
           "JumpAtThreshold",
           "CallRelative",
           "ReturnFromCall",
           "SetSequenceControlValue",
           "SetVoiceAllocationStart",
           "ResetVoiceAllocationStart",
           "PushLoop",
           "RepeatLoop",
           "JumpOnLoopPass",
           "ExitLoopOnPass",
           "RepeatLoopForever",
           "SetTimerDivisors",
           "SetTimerCount",
           "PlayEmbeddedSoundEffect",
           "SelectNoteRemapTable",
           "MarkChannelKeyed",
           "ClearChannelKeyed",
       )},
    **{f"VS::Audio::Seq::Live{name}": r"Native\Engine\include\Audio\SeqSequencerLive.hpp"
       for name in (
           "ExecuteChannelStep",
           "TickPlayback",
           "UpdateGlobalSlides",
           "ReleaseSoundEffectVoice",
       )},
    **{f"VS::Audio::Seq::Live{name}": r"Native\Engine\include\Audio\SeqMusicSequenceLive.hpp"
       for name in (
           "ResetSoundEffectChannel",
           "LoadMusicSequence",
           "ReleaseMusicConfiguration",
           "RestoreMusicSequence",
           "StartOrResumeMusic",
           "SaveCurrentMusic",
           "StartMusicLayer",
           "CrossfadeMusicLayers",
           "StartMusicWithLoopCounter",
           "SetMusicVolume",
           "FadeMusicVolume",
           "FadeMusicVolumeFrom",
           "SetDiscVolume",
           "FadeDiscVolume",
           "FadeDiscVolumeFrom",
       )},
    **{f"VS::Audio::Mixer::Live{name}": r"Native\Engine\include\Audio\MixControlLive.hpp"
       for name in (
           "SetSoundEffectPitchScale",
           "SlideSoundEffectPitchScale",
           "SetWorldSoundEffectPitchScale",
           "SlideWorldSoundEffectPitchScale",
           "SetTempoScale",
           "SlideTempoScale",
           "SetTempoScaleRamp",
           "SetMusicPitchScale",
           "SlideMusicPitchScale",
           "SetMusicPitchScaleRamp",
           "StopMusicAndCancelReplacement",
           "StopMatchingMusic",
           "StopWorldSoundEffects",
           "EnableStereoOutput",
           "EnableMonoOutput",
           "SetMusicMuteMask",
           "SetMusicJumpThreshold",
           "PauseMusic",
           "ResumeMusic",
           "PauseWorldSoundEffects",
           "ResumeWorldSoundEffects",
           "IgnoreCommand",
           "DispatchCommand",
       )},
    **{f"VS::Audio::Playback::Live{name}": r"Native\Engine\include\Audio\SoundPlaybackLive.hpp"
       for name in (
           "Initialize",
           "Shutdown",
           "BindSoundEffectPrograms",
           "MusicVolume",
           "PackedMusicSampleBankIds",
           "MusicPosition",
           "StartMusic",
           "StopMusic",
           "SaveMusicState",
           "InvalidateSavedMusicId",
           "CrossfadeMusic",
           "QueueMusicTransition",
           "PlaySoundEffect",
           "PlaySoundEffectBlob",
           "StopSoundEffects",
           "SoundEffectIdForTag",
           "SharedEffectUsesTags",
           "ProgramUsesTags",
           "SetMonoOutput",
       )},
    **{f"VS::Audio::Samples::Live{name}": r"Native\Engine\include\Audio\SampleBanksLive.hpp"
       for name in (
           "LoadBank",
           "TransferStatus",
           "ClearMusicBankIds",
           "LoadMusicBank",
           "LoadSoundEffectBank",
           "SequenceMagicDifference",
           "MapInstrumentToAlternateBank",
           "MapInstrumentToBaseBank",
           "SoundEffectBankSlot",
       )},
    **{f"VS::Audio::Live{name}": r"Native\Engine\include\Audio\SoundBanksLive.hpp"
       for name in (
           "LoadSfxBankSlot",
           "LoadSfxBankIntoFreeSlot",
           "PollSfxBankSlotLoad",
           "PollSfxBankLoads",
           "LoadSfxBankBlocking",
           "SelectSfxBankSlot",
           "FreeSfxBankSlot",
           "BindRoomSfxBank",
           "UnbindRoomSfxBank",
           "LoadWaveBankFile",
           "LoadMappedWaveBank",
           "UpdateWaveBankLoad",
           "LoadWaveBankBlocking",
           "SetMonoOutput",
           "InitializeMenuSoundBanks",
           "ResetSoundSlots",
       )},
    **{f"VS::Audio::Live{name}": r"Native\Engine\include\Audio\SfxLive.hpp"
       for name in (
           "PlaySfx",
           "PlaySfxCentered",
           "SfxPanAndVolume",
           "PlayWorldSfx",
           "ResolveSfxProgram",
           "FadeSfxVolume",
           "FadeSfxPitch",
           "FadeSfxPan",
           "StopSfx",
           "StopAllSfxPlayback",
           "PlayBankSfx",
           "PlayBankSfxCentered",
           "StopBankSfx",
           "FadeBankSfxVolume",
       )},
    **{f"VS::Audio::Control::Live{name}": r"Native\Engine\include\Audio\SoundControlLive.hpp"
       for name in (
           "SetMusicBranchThreshold",
           "SetMusicVolume",
           "FadeMusicVolume",
           "FadeSoundEffectVolume",
           "FadeSoundEffectPan",
           "FadeSoundEffectPitch",
           "SetDiscVolume",
           "FadeDiscVolume",
           "FadeDiscVolumeFrom",
           "FadeMusicTempoScale",
           "FadeMusicPitchScale",
           "StopAllMusic",
           "StopSceneSoundEffects",
           "StartCutsceneStream",
           "StopCutsceneStream",
           "SetCutsceneVolume",
           "FadeCutsceneVolume",
           "SetCutscenePan",
           "SoundEffectChannelIds",
       )},
    **{f"VS::Audio::SoundEffects::Live{name}": r"Native\Engine\include\Audio\SoundEffectProgramsLive.hpp"
       for name in (
           "ReleaseMatchingVoices",
           "PlayProgramPair",
           "InvalidateMusicChannelVolumes",
           "InvalidateSoundEffectVolumes",
           "PlayInterfaceProgramPair",
           "PlayInterfaceProgram",
           "PlaySharedProgram",
           "PlayBlobPrograms",
           "StopPrograms",
           "SetProgramVolume",
           "FadeProgramVolume",
           "SetSceneVolume",
           "FadeSceneVolume",
           "SetProgramPan",
           "FadeProgramPan",
           "SetScenePan",
           "FadeScenePan",
       )},
    **{f"VS::Audio::Live{name}": r"Native\Engine\include\Audio\MusicLive.hpp"
       for name in (
           "SetZoneMusicSwitchEnabled",
           "MusicBeatPosition",
           "SaveMusicPlaybackState",
           "SetMusicJumpThreshold",
           "StopMusicSlotPlayback",
           "ResumeMusicSlot",
           "CrossfadeMusicSlot",
           "PlayMusicSlot",
           "CurrentMusicSlot",
           "CurrentMusicId",
           "LoadMusicSlot",
           "PollMusicSlotLoad",
           "PollMusicLoads",
           "LoadMusicSlotBlocking",
           "FreeMusicSlot",
           "MusicSlotVolume",
           "QueryMusicSlotVolume",
           "FadeMusicSlotVolume",
           "FadeMusicSlotTempo",
           "FadeMusicSlotPitch",
           "StopAndFreeCurrentMusic",
       )},
    **{f"VS::Audio::Live{name}": r"Native\Engine\include\Audio\StreamLive.hpp"
       for name in (
           "BeginXaStream",
           "RequestXaStreamStop",
           "PlayCutsceneStreamSlotCentered",
       )},
    **{f"VS::Audio::Live{name}": r"Native\Engine\include\Audio\ActorSoundLive.hpp"
       for name in (
           "ActorSoundPanAndVolume",
           "PlayActorModelSound",
           "PlayActorModelSoundVariant",
           "PlayActorSoundPair",
           "PlayGimmickSoundPair",
           "PlayActorSurfaceContactSound",
           "PlayActorScriptSound",
       )},
    **{f"VS::Audio::Live{name}": r"Native\Engine\include\Audio\AmbienceLive.hpp"
       for name in (
           "SetRoomAmbienceDucked",
       )},
    **{f"VS::Audio::Driver::Live{name}": r"Native\Engine\include\Audio\SoundDriverLive.hpp"
       for name in (
           "Start",
           "Stop",
           "ArmSampleTransferCallback",
           "WriteSampleMemory",
           "WaitForSampleTransfer",
           "SetVoiceStartAddress",
           "SetVoiceLoopAddress",
           "SetVoiceAttackRateAndMode",
           "SetVoiceDecayRate",
           "SetVoiceSustainLevel",
           "SetVoiceSustainRateAndMode",
       )},
    **{name: r"Native\Engine\include\Audio\StreamControlLive.hpp"
       for name in (
           "VS::Audio::Cutscene::PauseLiveStream",
           "VS::Audio::Cutscene::ResumeLiveStream",
           "VS::Audio::Cutscene::ReplaceLiveStream",
           "VS::Audio::Cutscene::StopLiveStream",
           "VS::Audio::Cutscene::SetLiveStreamVolume",
           "VS::Audio::Cutscene::SlideLiveStreamVolume",
           "VS::Audio::Cutscene::SetLiveStreamPan",
           "VS::Audio::BindLiveRoomCutsceneStream",
           "VS::Audio::UnbindLiveRoomCutsceneStream",
       )},
    **{name: r"Native\Game\src\Battle\EncounterHooks.h"
       for name in (
           "Boot_EncounterRoomPlacements",
           "Boot_EncounterUsesRoomEvents",
           "Boot_EncounterPollStop",
       )},
    "Boot_InitializePlayerSettings": r"Native\Game\src\Boot\PlayerSettings.h",
    "Battle_Ai_InvalidateSessionBindings": r"Native\Game\src\Battle\Ai\RoomState.h",
    **{name: r"Native\Engine\include\Core\Diagnostics.h"
       for name in ("Core_Warnf", "Core_Errorf")},
    **{name: r"Native\Game\src\Menu\Loot\Paging.h"
       for name in (
           "kMenuLootRowCapacity", "Menu_Loot_HasStagingSpace",
           "Menu_Loot_CanStageDrop", "Menu_Loot_AppendStagedDrop",
           "Menu_Loot_BeginPaging", "Menu_Loot_AdvanceStagedInventory",
           "Menu_Loot_ResetPaging",
       )},
    **{f"VS::Input::{name}": r"Native\Engine\include\Input\ControllerLive.hpp"
       for name in (
           "InvalidateLiveControllerConnections", "UnlockLiveAnalogModeSwitch",
           "ConfigureLiveControllerActuators", "ReadLiveControllerPort",
           "UpdateLiveControllerConnection", "SetLiveMotorCommand",
           "StartLiveVibrationPattern", "StopLiveVibrationPatterns",
           "ResetLiveVibrationPatterns", "UpdateLiveVibrationPatterns",
           "PlayLiveVibrationPreset", "UpdateLiveControllers",
       )},
    **{name: r"Native\Engine\include\Input\ControllerLiveState.h"
       for name in (
           "Input_controllerPorts", "Input_releasedButtons",
           "Input_buttonHeldFrames", "Input_stickPosBuf", "Input_buttonRepeat",
           "Input_previousButtons", "Input_buttonsPressed", "Input_buttonsState",
           "Input_settings",
       )},
}

NATIVE_FUNCTION_REDIRECTS: dict[str, tuple[str, str]] = {}

# Owned rows whose generated callers keep the original call shape: calls to the row (and SDK calls of its libgte
# entry, copy_to_native.SDK_PROMOTED_ROWS) bind to this record adapter instead of the typed function. Each name
# must be listed in NATIVE_HELPER_HEADERS. Rows absent here are called by their typed name directly.
OWNED_CALL_BINDINGS = {
    fid: f"VS::Core::Math::{name}"
    for name, fids in (
        ("AddLiveVectors", ("F01241",)),
        ("AddLiveVectorsTo", ("F01462", "F01510", "F01511")),
        ("SubtractLiveVectors", ("F01242",)),
        ("CopyLiveVector", ("F01244",)),
        ("CopyLiveVectorTo", ("F01512", "F01513")),
        ("SelectLiveGreatestMagnitude", ("F01456",)),
        ("ScaleLiveFixedVector", ("F01243",)),
        ("MeasureLiveFixedVector", ("F01245",)),
        ("WidenLiveShortVectorToFixed", ("F01247",)),
        ("NormalizeLiveFixedVector", ("F01251",)),
        ("StoreLiveCrossProduct", ("F03407",)),
        ("NormalizeLiveVector", ("F03411", "F03412", "F03413")),
        ("BuildLiveRotationXyz", ("F03398",)),
        ("BuildLiveEulerRotationMatrix", ("F03397",)),
        ("BuildLiveLookAtOrientation", ("F00212",)),
        ("BuildLiveDirectionRotationMatrix", ("F01250",)),
        ("SetLiveIdentityRotation", ("F04132",)),
        ("SetLiveTranslationTransform", ("F04133",)),
        ("InterpolateLiveSevenBitScalar", ("F01454",)),
        ("InterpolateLiveShortPair", ("F01461",)),
        ("InterpolateLiveSevenBitVector", ("F01458", "F01459")),
        ("InterpolateLiveSevenBitPlane", ("F01460",)),
        ("InterpolateLiveThirteenBitScalar", ("F01479",)),
        ("InterpolateLiveThirteenBitVector", ("F01480", "F01483")),
        ("InterpolateLiveTimedScalar", ("F01487",)),
        ("InterpolateLiveTimedVector", ("F01482", "F01485")),
        ("InterpolateLiveCosineScalar", ("F01457", "F01486")),
        ("InterpolateLiveCosineVector", ("F01481", "F01484")),
        ("InterpolateLiveFixedPointRatio", ("F01476",)),
        ("InterpolateLiveLagrangeVector", ("F01477",)),
        ("InterpolateLiveLagrangeScalar", ("F01478",)),
        ("EvaluateLiveEase", ("F02428",)),
    )
    for fid in fids
} | {fid: f"VS::Render::{name}" for rows in _RENDER_LIVE.values() for name, fid in rows} | {
    fid: f"VS::Save::MemoryCard::{name}" for name, fids in _SAVE_MEMORY_CARD_LIVE.items() for fid in fids
} | {
    fid: f"VS::Save::{name}" for name, fids in _SAVE_SNAPSHOT_LIVE.items() for fid in fids
} | {fid: f"VS::Items::{name}" for rows in _ITEMS_LIVE.values() for name, fid in rows} | {
    fid: f"VS::Actors::{name}" for rows in _ACTORS_APPEARANCE_LIVE.values() for name, fid in rows} | {
    fid: f"VS::Actors::{name}" for rows in _ACTORS_TEXTURE_LIVE.values() for name, fid in rows} | {
    fid: f"VS::Actors::{name}" for rows in _ACTORS_MODEL_RENDERING_LIVE.values() for name, fid in rows} | {
    fid: f"VS::Actors::{name}" for rows in _ACTORS_SKELETON_LIVE.values() for name, fid in rows} | {
    fid: f"VS::Actors::{name}" for rows in _ACTORS_MOVEMENT_LIVE.values() for name, fid in rows} | {
    fid: f"VS::Actors::{name}" for rows in _ACTORS_ANIMATION_LIVE.values() for name, fid in rows} | {
    fid: f"VS::Actors::{name}" for rows in _ACTORS_ANIMATION_POSE_LIVE.values() for name, fid in rows} | {
    fid: f"VS::Content::{name}" for rows in _CONTENT_LIVE.values() for name, fid in rows} | {
    fid: f"VS::State::{name}" for rows in _STATE_LIVE.values() for name, fid in rows
} | {
    fid: f"VS::Boot::{name}" for rows in _BOOT_LIVE.values() for name, fid in rows
} | {
    fid: f"VS::Field::{name}" for rows in _FIELD_COLLISION_MAP_LIVE.values() for name, fid in rows
} | {
    fid: f"VS::Field::{name}" for rows in _FIELD_ACTOR_COLLISION_LIVE.values() for name, fid in rows
}
OWNED_CALL_BINDINGS["F01275"] = "VS::Event::ResolveLiveScriptActorReference"
OWNED_CALL_BINDINGS["F01276"] = "Event_EvaluateInterpolationCurveEntry"
OWNED_CALL_BINDINGS["F01261"] = "Event_EvaluateRoomEventTriggersEntry"
OWNED_CALL_BINDINGS["F00853"] = "Field_GimmickPlacementEntry"
OWNED_CALL_BINDINGS["F00837"] = "Field_FindGimmickBelowEntry"
OWNED_CALL_BINDINGS["F00838"] = "Field_GimmickSubtypeEntry"
OWNED_CALL_BINDINGS["F00917"] = "Field_AreGimmicksMovingEntry"
OWNED_CALL_BINDINGS["F02468"] = "Field_MeasureTileStackEntry"
OWNED_CALL_BINDINGS["F02469"] = "Field_IsGimmickWithinHeightBandEntry"
OWNED_CALL_BINDINGS["F00854"] = "Field_SetGimmickOrientationEntry"
OWNED_CALL_BINDINGS["F00912"] = "Field_CanPlaceGimmickAtPointEntry"
OWNED_CALL_BINDINGS["F00836"] = "Field_DropUnsupportedGimmickStackEntry"
OWNED_CALL_BINDINGS["F00908"] = "Field_FindInteractableGimmickAheadEntry"
OWNED_CALL_BINDINGS["F00913"] = "Field_BeginGimmickInteractionEntry"
OWNED_CALL_BINDINGS["F00914"] = "Field_ApplyGimmickInteractionCommandEntry"
OWNED_CALL_BINDINGS["F01014"] = "Field_UpdateGimmickReactionAnimationEntry"
OWNED_CALL_BINDINGS["F01013"] = "Field_UpdateGimmickMotionEntry"
OWNED_CALL_BINDINGS["F00065"] = "Field_DisableTrapAtPositionEntry"
OWNED_CALL_BINDINGS["F00547"] = "Field_CurrentSavePointIdEntry"
OWNED_CALL_BINDINGS["F01637"] = "Battle_Ai_RefreshActionCandidateReadinessEntry"
OWNED_CALL_BINDINGS["F01697"] = "Battle_Ai_ApplySimulatedActionResultsEntry"
OWNED_CALL_BINDINGS["F01698"] = "Battle_Ai_SimulateActionCandidateEntry"
OWNED_CALL_BINDINGS["F01699"] = "Battle_Ai_UpdateCandidateHitWeightEntry"
OWNED_CALL_BINDINGS["F01704"] = "Battle_Ai_EvaluateActionCandidateEntry"
OWNED_CALL_BINDINGS["F01707"] = "Battle_Ai_SelectFallbackActionEntry"
OWNED_CALL_BINDINGS["F01712"] = "Battle_Ai_EnumerateActionCandidatesEntry"
OWNED_CALL_BINDINGS["F01713"] = "Battle_Ai_AdvancePlanningSimulationEntry"
OWNED_CALL_BINDINGS["F01714"] = "VS::Battle::Ai::SnapshotLivePlanningActorStats"
OWNED_CALL_BINDINGS["F01715"] = "VS::Battle::Ai::RestoreLivePlanningActorStats"
OWNED_CALL_BINDINGS["F01716"] = "Battle_Ai_BuildActorActionPlanEntry"
OWNED_CALL_BINDINGS["F01717"] = "Battle_Ai_ReconcileCompletedActionPlanEntry"
OWNED_CALL_BINDINGS["F01793"] = "VS::Battle::Ai::ForgetLiveAiActionTarget"
OWNED_CALL_BINDINGS["F01810"] = "VS::Battle::Ai::BuildLiveAiActionMetadata"
OWNED_CALL_BINDINGS["F01811"] = "VS::Battle::Ai::ResolveLiveAiLinkedActors"
OWNED_CALL_BINDINGS["F01812"] = "VS::Battle::Ai::ApplyLiveSavedAiSettingsFlags"
OWNED_CALL_BINDINGS["F01813"] = "VS::Battle::Ai::ApplyLiveActorAiSettings"
OWNED_CALL_BINDINGS["F01818"] = "VS::Battle::Ai::CreateLiveActorAiState"
OWNED_CALL_BINDINGS["F01700"] = "VS::Battle::Ai::LiveActorStatusConditionScore"
OWNED_CALL_BINDINGS["F01701"] = "VS::Battle::Ai::LiveActorHpRatio"
OWNED_CALL_BINDINGS["F01702"] = "VS::Battle::Ai::LiveActorMpRatio"
OWNED_CALL_BINDINGS["F01703"] = "VS::Battle::Ai::LiveActorEquipmentDpRatio"
OWNED_CALL_BINDINGS["F01814"] = "VS::Battle::Ai::FloodFillLiveAiTerrainHeights"
OWNED_CALL_BINDINGS["F01815"] = "VS::Battle::Ai::FindLiveAdjacentAiGroundHeight"
OWNED_CALL_BINDINGS["F01816"] = "VS::Battle::Ai::BuildLiveAiSpecialTerrainHeights"
OWNED_CALL_BINDINGS["F01817"] = "VS::Battle::Ai::InitializeLiveRoomNavigation"
OWNED_CALL_BINDINGS["F01721"] = "VS::Battle::Ai::ResolveLiveRouteGateExit"
OWNED_CALL_BINDINGS["F01722"] = "VS::Battle::Ai::FindLiveNearestRouteGateEntry"
OWNED_CALL_BINDINGS["F01723"] = "VS::Battle::Ai::FindLiveRouteGateAtTile"
OWNED_CALL_BINDINGS["F01728"] = "VS::Battle::Ai::ResolveLiveNavigationAreaRoute"
OWNED_CALL_BINDINGS["F01710"] = "VS::Battle::Ai::FindLiveNearestActiveWaypoint"
OWNED_CALL_BINDINGS["F01747"] = "VS::Battle::Ai::UpdateLiveWaypointPatrol"
OWNED_CALL_BINDINGS["F01740"] = "VS::Battle::Ai::UpdateLiveNavigationAreaMovement"
OWNED_CALL_BINDINGS["F01738"] = "VS::Battle::Ai::FilterLiveDirectionsByGroundHeight"
OWNED_CALL_BINDINGS["F01739"] = "VS::Battle::Ai::MoveLiveWithHeightFilteredDirections"
OWNED_CALL_BINDINGS["F01742"] = "VS::Battle::Ai::ChooseNextLiveWanderTile"
OWNED_CALL_BINDINGS["F01745"] = "VS::Battle::Ai::UpdateLiveWanderMovement"
OWNED_CALL_BINDINGS["F01735"] = "VS::Battle::Ai::ProjectLiveClearRadialDestination"
OWNED_CALL_BINDINGS["F01736"] = "VS::Battle::Ai::MoveLiveToRadialDestination"
OWNED_CALL_BINDINGS["F01737"] = "VS::Battle::Ai::MoveLiveAwayFromReference"
OWNED_CALL_BINDINGS["F01750"] = "VS::Battle::Ai::RecoverLiveMovement"
OWNED_CALL_BINDINGS["F01636"] = "VS::Battle::Ai::FindLiveAdjacentMovementDestination"
OWNED_CALL_BINDINGS["F01642"] = "VS::Battle::Ai::IsLiveMovementHistoryStalled"
OWNED_CALL_BINDINGS["F01644"] = "VS::Battle::Ai::SaveLiveMovementRandomState"
OWNED_CALL_BINDINGS["F01660"] = "VS::Battle::Ai::TryRecoverLiveBlockedMovement"
OWNED_CALL_BINDINGS["F01729"] = "VS::Battle::Ai::IsLiveFlightDirectionObstructed"
OWNED_CALL_BINDINGS["F01730"] = "VS::Battle::Ai::CanLiveActorFlyInDirection"
OWNED_CALL_BINDINGS["F01732"] = "VS::Battle::Ai::UpdateLiveFlightGroundTransition"
OWNED_CALL_BINDINGS["F01733"] = "VS::Battle::Ai::UpdateLiveFlightMovement"
OWNED_CALL_BINDINGS["F01734"] = "VS::Battle::Ai::UpdateLiveFlightDirectionalMovement"
OWNED_CALL_BINDINGS["F01630"] = "VS::Battle::Ai::SetLiveTemporaryAwarenessIndicator"
OWNED_CALL_BINDINGS["F01751"] = "VS::Battle::Ai::FollowLiveObstacleDetour"
OWNED_CALL_BINDINGS["F01748"] = "VS::Battle::Ai::UpdateLiveTargetApproachMovement"
OWNED_CALL_BINDINGS["F01749"] = "VS::Battle::Ai::UpdateLiveTargetFacing"
OWNED_CALL_BINDINGS["F01678"] = "VS::Battle::Ai::UnlinkLiveEngagedActor"
OWNED_CALL_BINDINGS["F01679"] = "VS::Battle::Ai::UpdateLiveEngagementBuildup"
OWNED_CALL_BINDINGS["F01680"] = "VS::Battle::Ai::AnimateLiveEngagementPhase"
OWNED_CALL_BINDINGS["F01705"] = "VS::Battle::Ai::ApplyLiveActorActionReaction"
OWNED_CALL_BINDINGS["F01706"] = "VS::Battle::Ai::PropagateLiveActorRelationships"
OWNED_CALL_BINDINGS["F01719"] = "VS::Battle::Ai::NotifyLiveActionRelationshipChanges"
OWNED_CALL_BINDINGS["F01846"] = "VS::Battle::Ai::UpdateLiveBossRadialRetreat"
OWNED_CALL_BINDINGS["F01847"] = "VS::Battle::Ai::IsLiveBossActionReady"
OWNED_CALL_BINDINGS["F01848"] = "VS::Battle::Ai::UpdateLiveBossOrbitTracking"
OWNED_CALL_BINDINGS["F01849"] = "VS::Battle::Ai::UpdateLiveOrbitingBossBehavior"
OWNED_CALL_BINDINGS["F01640"] = "VS::Battle::Ai::UpdateLiveTargetPerception"
OWNED_CALL_BINDINGS["F01643"] = "VS::Battle::Ai::UpdateLiveActorPerception"
OWNED_CALL_BINDINGS["F01649"] = "VS::Battle::Ai::MoveLiveForActionCandidate"
OWNED_CALL_BINDINGS["F01650"] = "VS::Battle::Ai::UpdateLiveCombatPursuit"
OWNED_CALL_BINDINGS["F01651"] = "VS::Battle::Ai::TryApproachLivePlannedTarget"
OWNED_CALL_BINDINGS["F01652"] = "VS::Battle::Ai::UpdateLiveCloseFollowBehavior"
OWNED_CALL_BINDINGS["F01653"] = "VS::Battle::Ai::UpdateLiveEnemyPursuitBehavior"
OWNED_CALL_BINDINGS["F01654"] = "VS::Battle::Ai::UpdateLiveGuardMovementBehavior"
OWNED_CALL_BINDINGS["F01656"] = "VS::Battle::Ai::UpdateLiveEscortCombatBehavior"
OWNED_CALL_BINDINGS["F01657"] = "VS::Battle::Ai::TryExecuteLiveActionCandidate"
OWNED_CALL_BINDINGS["F01658"] = "VS::Battle::Ai::TryExecuteLivePlannedActions"
OWNED_CALL_BINDINGS["F01659"] = "VS::Battle::Ai::UpdateLiveActorDecisions"
OWNED_CALL_BINDINGS["F00405"] = "VS::Battle::QueueLiveApplicableStatusChanges"
OWNED_CALL_BINDINGS["F00406"] = "VS::Battle::QueueLiveStatusRemovalMask"
OWNED_CALL_BINDINGS["F00437"] = "VS::Battle::QueueLiveStrengthDown"
OWNED_CALL_BINDINGS["F00438"] = "VS::Battle::QueueLiveIntelligenceDown"
OWNED_CALL_BINDINGS["F00439"] = "VS::Battle::QueueLiveAgilityDown"
OWNED_CALL_BINDINGS["F00440"] = "VS::Battle::QueueLiveEquipmentDown"
OWNED_CALL_BINDINGS["F00441"] = "VS::Battle::QueueLiveHalfMovement"
OWNED_CALL_BINDINGS["F00442"] = "VS::Battle::QueueLiveStrengthUp"
OWNED_CALL_BINDINGS["F00443"] = "VS::Battle::QueueLiveIntelligenceUp"
OWNED_CALL_BINDINGS["F00444"] = "VS::Battle::QueueLiveAgilityUp"
OWNED_CALL_BINDINGS["F00445"] = "VS::Battle::QueueLiveEquipmentUp"
OWNED_CALL_BINDINGS["F00446"] = "VS::Battle::QueueLiveQuicken"
OWNED_CALL_BINDINGS["F00447"] = "VS::Battle::QueueLiveSilence"
OWNED_CALL_BINDINGS["F00448"] = "VS::Battle::QueueLiveMagicWard"
OWNED_CALL_BINDINGS["F00449"] = "VS::Battle::QueueLiveRegeneration"
OWNED_CALL_BINDINGS["F00450"] = "VS::Battle::QueueLiveBanish"
OWNED_CALL_BINDINGS["F00451"] = "VS::Battle::QueueLiveParalysis"
OWNED_CALL_BINDINGS["F00452"] = "VS::Battle::QueueLivePoison"
OWNED_CALL_BINDINGS["F00453"] = "VS::Battle::QueueLiveNumbness"
OWNED_CALL_BINDINGS["F00454"] = "VS::Battle::QueueLiveCurse"
OWNED_CALL_BINDINGS["F00455"] = "VS::Battle::QueueLiveAirAttackUp"
OWNED_CALL_BINDINGS["F00456"] = "VS::Battle::QueueLiveFireAttackUp"
OWNED_CALL_BINDINGS["F00457"] = "VS::Battle::QueueLiveEarthAttackUp"
OWNED_CALL_BINDINGS["F00458"] = "VS::Battle::QueueLiveWaterAttackUp"
OWNED_CALL_BINDINGS["F00459"] = "VS::Battle::QueueLiveAirDefenseUp"
OWNED_CALL_BINDINGS["F00460"] = "VS::Battle::QueueLiveFireDefenseUp"
OWNED_CALL_BINDINGS["F00461"] = "VS::Battle::QueueLiveEarthDefenseUp"
OWNED_CALL_BINDINGS["F00462"] = "VS::Battle::QueueLiveWaterDefenseUp"
OWNED_CALL_BINDINGS["F00463"] = "VS::Battle::QueueLiveParalysisCure"
OWNED_CALL_BINDINGS["F00464"] = "VS::Battle::QueueLivePoisonCure"
OWNED_CALL_BINDINGS["F00465"] = "VS::Battle::QueueLiveCurseCure"
OWNED_CALL_BINDINGS["F00466"] = "VS::Battle::QueueLiveNumbnessCure"
OWNED_CALL_BINDINGS["F00467"] = "VS::Battle::QueueLiveDispel"
OWNED_CALL_BINDINGS["F00468"] = "VS::Battle::QueueLiveNegativeStatusCure"
OWNED_CALL_BINDINGS["F00518"] = "VS::Battle::ApplyLiveWeaponElementEnchantment"
OWNED_CALL_BINDINGS["F00519"] = "VS::Battle::ApplyLiveArmorElementEnchantment"
OWNED_CALL_BINDINGS["F00521"] = "VS::Battle::QueueLiveActorStatChangeNumber"
OWNED_CALL_BINDINGS["F00527"] = "VS::Battle::UpdateLiveTargetingRiskPenalty"
OWNED_CALL_BINDINGS["F00528"] = "VS::Battle::ResetLiveTargetingRiskPenaltyTimer"
OWNED_CALL_BINDINGS["F00254"] = "VS::Battle::PreserveLiveAshleyStatuses"
OWNED_CALL_BINDINGS["F00517"] = "VS::Battle::RemoveLiveActorStatuses"
OWNED_CALL_BINDINGS["F00520"] = "VS::Battle::InflictLiveActorStatuses"
OWNED_CALL_BINDINGS["F00523"] = "VS::Battle::UpdateLiveActorStatusTimers"
OWNED_CALL_BINDINGS["F00194"] = "VS::Battle::ResolveLiveActorDefeat"
OWNED_CALL_BINDINGS["F00522"] = "VS::Battle::ApplyLiveActorActionResult"
OWNED_CALL_BINDINGS["F00525"] = "VS::Battle::UpdateLiveActorVitals"
OWNED_CALL_BINDINGS["F00526"] = "VS::Battle::UpdateLiveRoomActorVitals"
OWNED_CALL_BINDINGS["F00339"] = "VS::Battle::AttackLiveGemHitBonus"
OWNED_CALL_BINDINGS["F00340"] = "VS::Battle::DefenseLiveGemHitBonus"
OWNED_CALL_BINDINGS["F00342"] = "VS::Battle::PreviousLiveHitSuccessThreshold"
OWNED_CALL_BINDINGS["F00343"] = "VS::Battle::LiveGemHitChance"
OWNED_CALL_BINDINGS["F00347"] = "VS::Battle::LiveRiskHitChance"
OWNED_CALL_BINDINGS["F00348"] = "VS::Battle::LiveChainHitChance"
OWNED_CALL_BINDINGS["F00344"] = "VS::Battle::LiveAgilityHitChance"
OWNED_CALL_BINDINGS["F00345"] = "VS::Battle::LiveIntelligenceHitChance"
OWNED_CALL_BINDINGS["F00346"] = "VS::Battle::LiveCostAdjustedAgilityHitChance"
OWNED_CALL_BINDINGS["F00349"] = "VS::Battle::LiveRollActionHit"
OWNED_CALL_BINDINGS["F00435"] = "VS::Battle::LiveCalculateActionStatChange"
OWNED_CALL_BINDINGS["F00412"] = "VS::Battle::LiveScalePreviousTargetHit"
OWNED_CALL_BINDINGS["F00413"] = "VS::Battle::LiveScaleSourceMissingHp"
OWNED_CALL_BINDINGS["F00414"] = "VS::Battle::LiveScalePreviousTargetHit"
OWNED_CALL_BINDINGS["F00415"] = "VS::Battle::LiveScaleTargetCurrentHp"
OWNED_CALL_BINDINGS["F00416"] = "VS::Battle::LiveWeaponPpWithChainOffset"
OWNED_CALL_BINDINGS["F00417"] = "VS::Battle::LiveScaleIncomingSpellDamage"
OWNED_CALL_BINDINGS["F00419"] = "VS::Battle::LiveScaleSourceMissingMp"
OWNED_CALL_BINDINGS["F00426"] = "VS::Battle::LiveScalePreviousTargetHit"
OWNED_CALL_BINDINGS["F00418"] = "VS::Battle::LiveScaleIncomingPhysicalDamage"
OWNED_CALL_BINDINGS["F00424"] = "VS::Battle::LiveTargetMissingHp"
OWNED_CALL_BINDINGS["F00425"] = "VS::Battle::LiveTargetMissingMp"
OWNED_CALL_BINDINGS["F00427"] = "VS::Battle::LiveScaleIncomingSpellDamage"
OWNED_CALL_BINDINGS["F00428"] = "VS::Battle::LiveScaleIncomingPhysicalDamage"
OWNED_CALL_BINDINGS["F00429"] = "VS::Battle::LiveScaleIncomingSpellCost"
OWNED_CALL_BINDINGS["F00430"] = "VS::Battle::LiveScaleIncomingDamage"
OWNED_CALL_BINDINGS["F00431"] = "VS::Battle::LiveSourceShieldPp"
OWNED_CALL_BINDINGS["F00394"] = "VS::Battle::LiveApplyDamageGemMultiplier"
OWNED_CALL_BINDINGS["F00396"] = "VS::Battle::LiveDistributeHitLimbDamage"
OWNED_CALL_BINDINGS["F00397"] = "VS::Battle::LiveDistributeLimbRecovery"
OWNED_CALL_BINDINGS["F00408"] = "VS::Battle::LiveLastActionStatValue"
OWNED_CALL_BINDINGS["F00432"] = "VS::Battle::LiveScaleLastActionStatValue"
OWNED_CALL_BINDINGS["F00434"] = "VS::Battle::LiveTargetRisk"
OWNED_CALL_BINDINGS["F00409"] = "VS::Battle::LiveCalculatePhysicalHitDamage"
OWNED_CALL_BINDINGS["F00410"] = "VS::Battle::LivePhysicalDamageWithoutDebuffs"
OWNED_CALL_BINDINGS["F00411"] = "VS::Battle::LiveCalculateSpellHitDamage"
OWNED_CALL_BINDINGS["F00421"] = "VS::Battle::LiveCalculateSpellHealing"
OWNED_CALL_BINDINGS["F00433"] = "VS::Battle::LivePhysicalDamageWithDebuffs"
OWNED_CALL_BINDINGS["F00828"] = "Field_RefreshAshleyInteractionTileEntry"
OWNED_CALL_BINDINGS["F00602"] = "Field_RoomTriggerAtTileEntry"
OWNED_CALL_BINDINGS["F00616"] = "Field_SetRoomTriggerEnabledEntry"
OWNED_CALL_BINDINGS["F00942"] = "Field_FindTopGimmickAtTileEntry"
OWNED_CALL_BINDINGS["F00607"] = "Field_ToggleRoomSwitchEntry"
OWNED_CALL_BINDINGS["F00584"] = "Field_QueueGimmickAnimationEntry"
OWNED_CALL_BINDINGS["F00585"] = "Field_UpdateGimmickAnimationQueueEntry"
OWNED_CALL_BINDINGS["F00570"] = "Field_ResetGimmickAnimationQueueEntry"
OWNED_CALL_BINDINGS["F00606"] = "Field_BeginSwitchToggleEntry"
OWNED_CALL_BINDINGS["F00132"] = "Field_BeginObjectInteractionWaitEntry"
OWNED_CALL_BINDINGS["F00133"] = "Field_EndObjectInteractionWaitEntry"
OWNED_CALL_BINDINGS["F00134"] = "Field_InteractWithLockedRoomObjectEntry"
OWNED_CALL_BINDINGS["F00135"] = "Field_TriggerActorTrapEntry"
OWNED_CALL_BINDINGS["F00139"] = "Field_WaitForChestCameraFocusEntry"
OWNED_CALL_BINDINGS["F00140"] = "Field_StepTrapCameraFocusEntry"
OWNED_CALL_BINDINGS["F00142"] = "Field_IsChestAnimationReadyEntry"
OWNED_CALL_BINDINGS["F00146"] = "Field_TrackInteractionCameraToAshleyEntry"
OWNED_CALL_BINDINGS["F00152"] = "Field_TileInFrontOfActorEntry"
OWNED_CALL_BINDINGS["F00153"] = "Field_TryTriggerFacingTileEventEntry"
OWNED_CALL_BINDINGS["F00154"] = "Field_FindFacingRoomObjectEntry"
OWNED_CALL_BINDINGS["F00155"] = "Field_FindFacingUnlootedChestEntry"
OWNED_CALL_BINDINGS["F00158"] = "Field_CheckAshleyTileTriggersEntry"
OWNED_CALL_BINDINGS["F00577"] = "Field_SetEntranceDoorRecordEntry"
OWNED_CALL_BINDINGS["F00552"] = "Field_StoreEntranceDoorForReloadEntry"
OWNED_CALL_BINDINGS["F00578"] = "Field_FindEntranceDoorNearTileEntry"
OWNED_CALL_BINDINGS["F00610"] = "Field_SceneDoorIdEntry"
OWNED_CALL_BINDINGS["F00611"] = "Field_MarkEntranceDoorOnSceneMapEntry"
OWNED_CALL_BINDINGS["F00580"] = "Field_InteractiveDoorAtTileEntry"
OWNED_CALL_BINDINGS["F00633"] = "Field_MarkSigilDoorUnlockedEntry"
OWNED_CALL_BINDINGS["F00634"] = "Field_IsRoomSigilDoorUnlockedEntry"
OWNED_CALL_BINDINGS["F00581"] = "Field_ToggleDoorCollisionEntry"
OWNED_CALL_BINDINGS["F00582"] = "Field_BeginDoorToggleEntry"
OWNED_CALL_BINDINGS["F00583"] = "Field_StartDoorAnimationEntry"
OWNED_CALL_BINDINGS["F00586"] = "Field_FinishDoorAnimationEntry"
OWNED_CALL_BINDINGS["F00067"] = "Field_ApplyUnlockEffectToRoomObjectEntry"
OWNED_CALL_BINDINGS["F00591"] = "Field_SetMovingPlatformActorEntry"
OWNED_CALL_BINDINGS["F00604"] = "Field_SetMovingPlatformsPausedEntry"
OWNED_CALL_BINDINGS["F00605"] = "Field_FreezeMovingPlatformsEntry"
OWNED_CALL_BINDINGS["F00066"] = "Field_FreezeRoomGimmicksTemporarilyEntry"
OWNED_CALL_BINDINGS["F00619"] = "Field_SetMovingPlatformVisibilityEntry"
OWNED_CALL_BINDINGS["F00621"] = "Field_MovingPlatformDwellTicksEntry"
OWNED_CALL_BINDINGS["F00593"] = "Field_InitializeMovingPlatformsEntry"
OWNED_CALL_BINDINGS["F00595"] = "Field_UpdateMovingPlatformsEntry"
OWNED_CALL_BINDINGS["F00596"] = "Field_DrawMovingPlatformShadowsEntry"
OWNED_CALL_BINDINGS["F00629"] = "Field_SetPlatformFreezeAppearanceEntry"
OWNED_CALL_BINDINGS["F00620"] = "Field_MovingPlatformTargetWaypointEntry"
OWNED_CALL_BINDINGS["F00780"] = "Field_FindSharedGimmickModelEntry"
OWNED_CALL_BINDINGS["F00781"] = "Field_SetGimmickPaletteTableEntry"
OWNED_CALL_BINDINGS["F00782"] = "Field_CreateGimmickInstanceEntry"
OWNED_CALL_BINDINGS["F00783"] = "Field_ReleaseGimmickInstanceEntry"
OWNED_CALL_BINDINGS["F00597"] = "Field_CollectMovingPlatformPositionsEntry"
OWNED_CALL_BINDINGS["F00594"] = "Field_ResetMovingPlatformWaypointEntry"
OWNED_CALL_BINDINGS["F00159"] = "Field_CheckActorTrapAndFallTriggersEntry"
OWNED_CALL_BINDINGS["F00136"] = "Field_TryOpenChestEntry"
OWNED_CALL_BINDINGS["F00138"] = "Field_BeginChestInteractionEntry"
OWNED_CALL_BINDINGS["F00144"] = "Field_FinishChestInteractionEntry"
OWNED_CALL_BINDINGS["F00147"] = "Field_OpenChestContentsEntry"
OWNED_CALL_BINDINGS["F00148"] = "Field_UpdateChestContentsSelectionEntry"
OWNED_CALL_BINDINGS["F00141"] = "Field_DispatchTrapOrChestInteractionEntry"
OWNED_CALL_BINDINGS["F00137"] = "Field_BeginTrapInteractionEntry"
OWNED_CALL_BINDINGS["F00145"] = "Field_FinishTrapInteractionEntry"

# Controller.cpp rows whose generated callers keep the original port-ID, pad-buffer and raw-pattern call shape.
INPUT_CONTROLLER_LIVE_BINDINGS = {
    "F03444": "InvalidateLiveControllerConnections",
    "F03445": "UnlockLiveAnalogModeSwitch",
    "F03446": "ConfigureLiveControllerActuators",
    "F03447": "ReadLiveControllerPort",
    "F03448": "UpdateLiveControllerConnection",
    "F03449": "SetLiveMotorCommand",
    "F03451": "StartLiveVibrationPattern",
    "F03453": "StopLiveVibrationPatterns",
    "F03458": "ResetLiveVibrationPatterns",
    "F03460": "UpdateLiveVibrationPatterns",
    "F03461": "PlayLiveVibrationPreset",
    "F03462": "UpdateLiveControllers",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Input::{name}" for fid, name in INPUT_CONTROLLER_LIVE_BINDINGS.items()}

# #276 audio-seq-control: generated driver, mixer and sound-effect callers keep the original channel, config and
# sequence-file call shapes; the live bindings load the typed records, bound reads to the sequence data and store back.
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Audio::Seq::Live{name}"
    for fid, name in (
        ("F03199", "ResetSoundEffectChannel"),
        ("F03201", "LoadMusicSequence"),
        ("F03202", "ReleaseMusicConfiguration"),
        ("F03210", "RestoreMusicSequence"),
        ("F03265", "UpdateGlobalSlides"),
        ("F03269", "TickPlayback"),
        ("F03279", "ReleaseSoundEffectVoice"),
    )
}

# #276 audio-mixer-driver: generated sound-control, playback, sample-bank and cutscene-stream callers keep the original
# command, voice and buffer call shapes; the live bindings load the typed records and hosts and store back.
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Audio::{name}"
    for fid, name in (
        ("F03154", "Driver::LiveArmSampleTransferCallback"),
        ("F03155", "Driver::LiveWriteSampleMemory"),
        ("F03157", "Driver::LiveWaitForSampleTransfer"),
        ("F03161", "Driver::LiveStart"),
        ("F03162", "Driver::LiveStop"),
        ("F03170", "Driver::LiveSetVoiceStartAddress"),
        ("F03171", "Driver::LiveSetVoiceLoopAddress"),
        ("F03174", "Driver::LiveSetVoiceAttackRateAndMode"),
        ("F03175", "Driver::LiveSetVoiceDecayRate"),
        ("F03176", "Driver::LiveSetVoiceSustainLevel"),
        ("F03177", "Driver::LiveSetVoiceSustainRateAndMode"),
        ("F03261", "Mixer::LiveDispatchCommand"),
    )
}

# #276 audio-playback-banks: generated music, sound-effect, script and menu callers keep the original sequence,
# program and bank pointer call shapes; the live bindings load the typed records and hosts and store back.
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Audio::{name}"
    for fid, name in (
        ("F03074", "Playback::LiveInitialize"),
        ("F03075", "Playback::LiveShutdown"),
        ("F03076", "Playback::LiveBindSoundEffectPrograms"),
        ("F03078", "Playback::LiveMusicVolume"),
        ("F03079", "Playback::LivePackedMusicSampleBankIds"),
        ("F03081", "Playback::LiveMusicPosition"),
        ("F03082", "Playback::LiveStartMusic"),
        ("F03083", "Playback::LiveStopMusic"),
        ("F03084", "Playback::LiveSaveMusicState"),
        ("F03085", "Playback::LiveInvalidateSavedMusicId"),
        ("F03087", "Playback::LiveCrossfadeMusic"),
        ("F03089", "Playback::LiveQueueMusicTransition"),
        ("F03092", "Playback::LivePlaySoundEffect"),
        ("F03093", "Playback::LivePlaySoundEffectBlob"),
        ("F03094", "Playback::LiveStopSoundEffects"),
        ("F03097", "Playback::LiveSoundEffectIdForTag"),
        ("F03098", "Playback::LiveProgramUsesTags"),
        ("F03099", "Playback::LiveSetMonoOutput"),
        ("F03134", "Samples::LiveLoadBank"),
        ("F03135", "Samples::LiveTransferStatus"),
        ("F03139", "Samples::LiveClearMusicBankIds"),
        ("F03142", "Samples::LiveLoadMusicBank"),
        ("F03143", "Samples::LiveLoadSoundEffectBank"),
        ("F03152", "Samples::LiveSequenceMagicDifference"),
        ("F03196", "Samples::LiveMapInstrumentToAlternateBank"),
        ("F03197", "Samples::LiveMapInstrumentToBaseBank"),
        ("F03211", "Samples::LiveSoundEffectBankSlot"),
        ("F03526", "LiveLoadSfxBankSlot"),
        ("F03527", "LiveLoadSfxBankIntoFreeSlot"),
        ("F03528", "LivePollSfxBankSlotLoad"),
        ("F03529", "LivePollSfxBankLoads"),
        ("F03530", "LiveLoadSfxBankBlocking"),
        ("F03531", "LiveSelectSfxBankSlot"),
        ("F03532", "LiveFreeSfxBankSlot"),
        ("F03533", "LiveBindRoomSfxBank"),
        ("F03534", "LiveUnbindRoomSfxBank"),
        ("F03547", "LiveLoadWaveBankFile"),
        ("F03548", "LiveLoadMappedWaveBank"),
        ("F03549", "LiveUpdateWaveBankLoad"),
        ("F03550", "LiveLoadWaveBankBlocking"),
        ("F03551", "LiveSetMonoOutput"),
        ("F03553", "LiveInitializeMenuSoundBanks"),
        ("F03554", "LiveResetSoundSlots"),
    )
}

# #276 effects-ring-templates: the ring, closed-ring and tiled-ring instances keep the renderer-slot entry shape
# (slot, command, packed parameter) in generated renderer tables and plugin updates; each live binding passes its
# overlay's palette to the typed instance and binds the slot, package, scratch, GTE and frame to RingHost.
# Duplicate catalog rows bind to their canonical instance.
EFFECT_RING_LIVE_BINDINGS = {
    "F01894": "UpdateLivePlg044TiledRing",
    "F01900": "UpdateLivePlg047ClosedRing",
    "F01908": "UpdateLivePlg049Ring",
    "F01909": "UpdateLivePlg049TiledRing",
    "F01911": "UpdateLivePlg050Ring",
    "F01919": "UpdateLivePlg051ClosedRing",
    "F01925": "UpdateLivePlg053ClosedRing",
    "F01927": "UpdateLivePlg053ClosedRing",
    "F01929": "UpdateLivePlg053ClosedRing",
    "F01951": "UpdateLivePlg061Ring",
    "F01952": "UpdateLivePlg061ClosedRing",
    "F01954": "UpdateLivePlg065ClosedRing",
    "F01955": "UpdateLivePlg065Ring",
    "F01974": "UpdateLivePlg069TiledRing",
    "F01975": "UpdateLivePlg073TiledRing",
    "F01976": "UpdateLivePlg073Ring",
    "F01986": "UpdateLivePlg073TiledRing",
    "F01987": "UpdateLivePlg073Ring",
    "F02001": "UpdateLivePlg053ClosedRing",
    "F02003": "UpdateLivePlg081Ring",
    "F02004": "UpdateLivePlg081ClosedRing",
    "F02011": "UpdateLivePlg082ClosedRing",
    "F02013": "UpdateLivePlg085TiledRing",
    "F02014": "UpdateLivePlg086ClosedRing",
    "F02025": "UpdateLivePlg087ClosedRing",
    "F02060": "UpdateLivePlg128ClosedRing",
    "F02076": "UpdateLivePlg129Ring",
    "F02079": "UpdateLivePlg129Ring2",
    "F02088": "UpdateLivePlg134ClosedRing",
    "F02089": "UpdateLivePlg135ClosedRing",
    "F02106": "UpdateLivePlg137TiledRing",
    "F02108": "UpdateLivePlg138Ring",
    "F02110": "UpdateLivePlg138TiledRing",
    "F02111": "UpdateLivePlg139TiledRing",
    "F02121": "UpdateLivePlg142ClosedRing",
    "F02122": "UpdateLivePlg142Ring",
    "F02124": "UpdateLivePlg143TiledRing",
    "F02125": "UpdateLivePlg143ClosedRing",
    "F02126": "UpdateLivePlg146TiledRing",
    "F02138": "UpdateLivePlg146Ring",
    "F02146": "UpdateLivePlg147Ring",
    "F02164": "UpdateLivePlg148TiledRing",
    "F02165": "UpdateLivePlg149Ring",
    "F02166": "UpdateLivePlg150TiledRing",
    "F02169": "UpdateLivePlg150Ring",
    "F02176": "UpdateLivePlg152ClosedRing",
    "F02178": "UpdateLivePlg153Ring",
    "F02185": "UpdateLivePlg154ClosedRing",
    "F02194": "UpdateLivePlg158ClosedRing",
    "F02196": "UpdateLivePlg160TiledRing",
    "F02197": "UpdateLivePlg160Ring",
    "F02199": "UpdateLivePlg161Ring",
    "F02208": "UpdateLivePlg161Ring2",
    "F02211": "UpdateLivePlg162Ring",
    "F02212": "UpdateLivePlg162ClosedRing",
    "F02222": "UpdateLivePlg165ClosedRing",
    "F02225": "UpdateLivePlg166ClosedRing",
    "F02226": "UpdateLivePlg166Ring",
    "F02227": "UpdateLivePlg167ClosedRing",
    "F02261": "UpdateLivePlg169Ring",
    "F02262": "UpdateLivePlg169ClosedRing",
    "F02265": "UpdateLivePlg170Ring",
    "F02268": "UpdateLivePlg170ClosedRing",
    "F02269": "UpdateLivePlg198Ring",
    "F02287": "UpdateLivePlg236ClosedRing",
    "F02290": "UpdateLivePlg236Ring",
    "F02291": "UpdateLivePlg242TiledRing",
    "F02302": "UpdateLivePlg250ClosedRing",
    "F02322": "UpdateLivePlg263Ring",
    "F02329": "UpdateLivePlg265TiledRing",
    "F02375": "UpdateLivePlg272ClosedRing",
    "F02386": "UpdateLivePlg291ClosedRing",
    "F02388": "UpdateLivePlg297ClosedRing",
    "F02390": "UpdateLivePlg297Ring",
    "F02404": "UpdateLivePlg301ClosedRing",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Effects::Templates::{name}" for fid, name in EFFECT_RING_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Effects::Templates::{name}": r"Native\Game\include\Effects\Templates\RingsLive.hpp"
    for name in EFFECT_RING_LIVE_BINDINGS.values()
}

# #276 effects-particle-templates: the particle-set dispatchers keep the renderer-slot entry shape (slot, command,
# packed parameter) in generated renderer tables and plugin updates; each live binding passes its overlay's typed
# dispatch options (origin, target, control and render stages, texturing and counter) and binds the slot, package,
# scratch, control points, GTE and frame to ParticleSetHost.
EFFECT_PARTICLE_LIVE_BINDINGS = {
    "F01899": "UpdateLivePlg045ParticleDispatch",
    "F01934": "UpdateLivePlg057ParticleDispatch",
    "F01965": "UpdateLivePlg065ParticleDispatch",
    "F02009": "UpdateLivePlg082ParticleDispatch",
    "F02043": "UpdateLivePlg087ParticleDispatch",
    "F02048": "UpdateLivePlg114ParticleDispatch",
    "F02058": "UpdateLivePlg114ParticleDispatch2",
    "F02137": "UpdateLivePlg146ParticleDispatch",
    "F02156": "UpdateLivePlg147ParticleDispatch",
    "F02175": "UpdateLivePlg152ParticleDispatch",
    "F02183": "UpdateLivePlg153ParticleDispatch",
    "F02190": "UpdateLivePlg156ParticleDispatch",
    "F02252": "UpdateLivePlg168ParticleDispatch",
    "F02279": "UpdateLivePlg224ParticleDispatch",
    "F02319": "UpdateLivePlg255ParticleDispatch",
    "F02328": "UpdateLivePlg263ParticleDispatch",
    "F02339": "UpdateLivePlg265ParticleDispatch",
    "F02351": "UpdateLivePlg265ParticleDispatch2",
    "F02356": "UpdateLivePlg266ParticleDispatch",
    "F02366": "UpdateLivePlg266ParticleDispatch2",
    "F02396": "UpdateLivePlg300ParticleDispatch",
    "F02403": "UpdateLivePlg301ParticleDispatch",
}
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Effects::Templates::{name}" for fid, name in EFFECT_PARTICLE_LIVE_BINDINGS.items()
}
NATIVE_HELPER_HEADERS |= {
    f"VS::Effects::Templates::{name}": r"Native\Game\include\Effects\Templates\ParticlesLive.hpp"
    for name in EFFECT_PARTICLE_LIVE_BINDINGS.values()
}

# #276 effects-trail-templates: each overlay's five-sample trail dispatch instance keeps the renderer-slot entry shape
# in generated renderer tables and plugin updates; its live binding runs the typed emitter lifecycle
# (TrailEmitter.cpp) with that overlay's emission cursor and typed target, control, init, update and render stages.
# PLG074's dispatch (F01994) keeps its own binding, UpdateLivePlg074TrailEmitter.
EFFECT_TRAIL_LIVE_BINDINGS = {
    "F01948": "UpdateLivePlg057Trail",
    "F01983": "UpdateLivePlg073Trail",
    "F02023": "UpdateLivePlg086Trail",
    "F02033": "UpdateLivePlg087Trail",
    "F02075": "UpdateLivePlg128Trail",
    "F02087": "UpdateLivePlg134Trail",
    "F02104": "UpdateLivePlg137Trail",
    "F02119": "UpdateLivePlg141Trail",
    "F02145": "UpdateLivePlg146Trail",
    "F02163": "UpdateLivePlg148Trail",
    "F02299": "UpdateLivePlg242Trail",
    "F02309": "UpdateLivePlg255Trail",
    "F02346": "UpdateLivePlg265Trail",
    "F02373": "UpdateLivePlg266Trail",
    "F02384": "UpdateLivePlg291Trail",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Effects::Templates::{name}" for fid, name in EFFECT_TRAIL_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Effects::Templates::{name}": r"Native\Game\include\Effects\Templates\TrailsLive.hpp"
    for name in EFFECT_TRAIL_LIVE_BINDINGS.values()
}

# #276 effects-surface-templates: the grid, ten-vertex mesh, surface, mirrored-surface and rotating-surface instances
# keep the renderer-slot entry shape (slot, command, packed parameter) in generated renderer tables and plugin updates;
# each live binding passes its overlay's profile or mesh model to the typed instance and binds the slot, package,
# scratch, GTE, heap and frame to SurfaceHost. Duplicate catalog rows bind to their canonical instance.
EFFECT_SURFACE_LIVE_BINDINGS = {
    "F01910": "UpdateLivePlg049RotatingSurface",
    "F01924": "UpdateLivePlg053Grid",
    "F01949": "UpdateLivePlg061Surface",
    "F01950": "UpdateLivePlg061Grid",
    "F01953": "UpdateLivePlg065RotatingSurface",
    "F01966": "UpdateLivePlg069Mesh10",
    "F01984": "UpdateLivePlg073RotatingSurface",
    "F01985": "UpdateLivePlg073Grid",
    "F01995": "UpdateLivePlg073RotatingSurface",
    "F01997": "UpdateLivePlg077Mesh10",
    "F01999": "UpdateLivePlg077Grid",
    "F02000": "UpdateLivePlg077Surface",
    "F02002": "UpdateLivePlg081RotatingSurface",
    "F02010": "UpdateLivePlg082Grid",
    "F02012": "UpdateLivePlg085Grid",
    "F02015": "UpdateLivePlg086RotatingSurface",
    "F02016": "UpdateLivePlg086Grid",
    "F02024": "UpdateLivePlg087Surface",
    "F02026": "UpdateLivePlg087Grid",
    "F02059": "UpdateLivePlg128Surface",
    "F02061": "UpdateLivePlg128RotatingSurface",
    "F02062": "UpdateLivePlg128Grid",
    "F02063": "UpdateLivePlg128Surface2",
    "F02077": "UpdateLivePlg129Mesh10",
    "F02078": "UpdateLivePlg129Surface",
    "F02080": "UpdateLivePlg129RotatingSurface",
    "F02097": "UpdateLivePlg137Mesh10",
    "F02105": "UpdateLivePlg137Grid",
    "F02107": "UpdateLivePlg138RotatingSurface",
    "F02109": "UpdateLivePlg138Grid",
    "F02112": "UpdateLivePlg139Grid",
    "F02120": "UpdateLivePlg142Grid",
    "F02123": "UpdateLivePlg142RotatingSurface",
    "F02127": "UpdateLivePlg146MirrorSurface",
    "F02167": "UpdateLivePlg150Grid",
    "F02168": "UpdateLivePlg150Surface",
    "F02170": "UpdateLivePlg152Grid",
    "F02177": "UpdateLivePlg153Mesh10",
    "F02184": "UpdateLivePlg154Mesh10",
    "F02191": "UpdateLivePlg156Grid",
    "F02192": "UpdateLivePlg158Surface",
    "F02193": "UpdateLivePlg158MirrorSurface",
    "F02195": "UpdateLivePlg158RotatingSurface",
    "F02198": "UpdateLivePlg161RotatingSurface",
    "F02207": "UpdateLivePlg161Grid",
    "F02210": "UpdateLivePlg162RotatingSurface",
    "F02220": "UpdateLivePlg165RotatingSurface",
    "F02221": "UpdateLivePlg165Grid",
    "F02223": "UpdateLivePlg166RotatingSurface",
    "F02224": "UpdateLivePlg166MirrorSurface",
    "F02228": "UpdateLivePlg167RotatingSurface",
    "F02253": "UpdateLivePlg168Grid",
    "F02263": "UpdateLivePlg170MirrorSurface",
    "F02264": "UpdateLivePlg170RotatingSurface",
    "F02266": "UpdateLivePlg170Grid",
    "F02267": "UpdateLivePlg170Surface",
    "F02288": "UpdateLivePlg236RotatingSurface",
    "F02292": "UpdateLivePlg242Grid",
    "F02300": "UpdateLivePlg246RotatingSurface",
    "F02301": "UpdateLivePlg250RotatingSurface",
    "F02320": "UpdateLivePlg260Mesh10",
    "F02321": "UpdateLivePlg263Surface",
    "F02374": "UpdateLivePlg272Grid",
    "F02376": "UpdateLivePlg272Surface",
    "F02377": "UpdateLivePlg273MirrorSurface",
    "F02385": "UpdateLivePlg291Grid",
    "F02387": "UpdateLivePlg297MirrorSurface",
    "F02389": "UpdateLivePlg297Grid",
    "F02391": "UpdateLivePlg300MirrorSurface",
}
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Effects::Templates::{name}" for fid, name in EFFECT_SURFACE_LIVE_BINDINGS.items()
}
NATIVE_HELPER_HEADERS |= {
    f"VS::Effects::Templates::{name}": r"Native\Game\include\Effects\Templates\SurfacesLive.hpp"
    for name in EFFECT_SURFACE_LIVE_BINDINGS.values()
}

# #276 slice 38 (effects-resources-transforms): the generated effect runtime, timeline, script and PLG callers keep
# the original curve-sampling call shapes (5BF94.c:6314-6365); the live bindings sample the current package's bound
# curve block and block-11 table into Effects_curveSamples.
EFFECT_CURVES_LIVE_BINDINGS = {
    "F01472": "LiveSampleAllEffectCurves",
    "F01473": "LiveSampleEffectCurve",
    "F01475": "LiveSegmentedEffectCurveDuration",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Effects::{name}" for fid, name in EFFECT_CURVES_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Effects::{name}": r"Native\Game\include\Effects\CurvesLive.hpp"
    for name in EFFECT_CURVES_LIVE_BINDINGS.values()
}

# #276 slice 38 (effects-resources-transforms): the generated effect runtime, timeline and PLG128 callers keep the
# original camera call shapes (5BF94.c:6474-8323); the live bindings load the owned camera globals into the typed
# EffectCameraState, run the camera core over the field and render cameras, and store the state back.
EFFECT_CAMERA_LIVE_BINDINGS = {
    "F01488": "LiveResetEffectCameraMotion",
    "F01490": "LiveCaptureEffectCamera",
    "F01494": "LiveSetEffectCameraTransitionMode",
    "F01497": "LiveStartCameraShakeChannels",
    "F01503": "LiveUpdateEffectCameraMotion",
    "F01584": "LiveFocusCameraOnEffectTransform",
    "F01585": "LiveRestoreActionEffectCamera",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Effects::{name}" for fid, name in EFFECT_CAMERA_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Effects::{name}": r"Native\Game\include\Effects\CameraLive.hpp"
    for name in EFFECT_CAMERA_LIVE_BINDINGS.values()
}

# #276 slice 38 (effects-resources-transforms): the generated effect script keeps the original transform call shapes
# (5BF94.c:5940, 6227); the live bindings decode the current package's bound block 10 and run the typed transforms
# over the instance's records.
EFFECT_TRANSFORMS_LIVE_BINDINGS = {
    "F01471": "LiveUpdateEffectTransforms",
    "F04134": "LiveInitializeTransformStates",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Effects::{name}" for fid, name in EFFECT_TRANSFORMS_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Effects::{name}": r"Native\Game\include\Effects\TransformsLive.hpp"
    for name in EFFECT_TRANSFORMS_LIVE_BINDINGS.values()
}

# #276 slice 38 (effects-resources-transforms): the generated effect runtime and timeline keep the original loader,
# context and package-walk call shapes (6F014.c:446-746, EFFPURGE 0.c:911); the live bindings load the owned loader
# globals into the typed EffectResourceState, run the resource core over the disc queue, heap, overlay arena and VRAM,
# store the state back and publish the bound package's legacy block pointers.
EFFECT_RESOURCES_LIVE_BINDINGS = {
    "F01595": "LiveInitializeEffectResources",
    "F01596": "LiveSelectEffectResourceContext",
    "F01597": "LiveSaveEffectResourceContext",
    "F01598": "LiveRestoreEffectResourceContext",
    "F01600": "LiveUpdateParticleGravityAndDrag",
    "F01613": "LiveLoadEffectPurgeOverlay",
    "F01614": "LiveLoadEffectPackage",
    "F01615": "LiveReleaseEffectPackageResources",
    "F01616": "LiveUpdateEffectTextureLoading",
    "F01617": "LiveUpdateEffectPluginLoading",
    "F01626": "LiveBindEffectPackageBlocks",
    "F01890": "LiveConfigureEffectResources",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Effects::{name}" for fid, name in EFFECT_RESOURCES_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Effects::{name}": r"Native\Game\include\Effects\EffectResourcesLive.hpp"
    for name in EFFECT_RESOURCES_LIVE_BINDINGS.values()
}

# #276 field-room-effects: generated room loading, rendering, gimmick, door, event script and cutscene callers keep the
# original index-and-flag call shapes (2842C.c:550-1456); the live bindings run the typed room effects over the owned
# RoomEffectState and bind the loaded room's sections, sound bank, heap and floor query.
FIELD_ROOM_EFFECTS_LIVE_BINDINGS = {
    "F00670": "LiveFindRoomGeometryGroup",
    "F00671": "LiveShowRoomGeometryGroup",
    "F00672": "LiveHideRoomGeometryGroup",
    "F00673": "LiveSwapRoomGeometryGroupFlags",
    "F00674": "LiveAddRoomGeometryDepthBias",
    "F00675": "LiveDisableRoomGeometryGroup",
    "F00676": "LiveUpdateRoomEffects",
    "F00682": "LiveInitializeRoomEffects",
    "F00685": "LiveStartRoomEffect",
    "F00686": "LiveReleaseRoomEffects",
    "F00704": "LiveToggleRoomEffectUpdates",
    "F00705": "LiveIsRoomEffectActive",
    "F00706": "LiveFinishRoomDoorRotation",
    "F00707": "LiveFinishRoomTiltRotation",
    "F00708": "LiveDuckRoomEffectSound",
    "F00709": "LiveSwapRoomGeometryRedGreen",
    "F00710": "LiveRestoreRoomEffectSoundVolume",
    "F00711": "LiveRestartRoomEffectSound",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Field::{name}" for fid, name in FIELD_ROOM_EFFECTS_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Field::{name}": r"Native\Game\include\Field\RoomEffectsLive.hpp"
    for name in FIELD_ROOM_EFFECTS_LIVE_BINDINGS.values()
}

# #276 field-camera-background: generated battle, interaction, room loading, event script, cutscene and sphere callers
# keep the original camera, room background and weather call shapes (146C.c:776-9106, 146C.c:17431-17710,
# 2842C.c:266-547); the live bindings load the owned camera, background and weather globals into the typed
# FieldCameraState, RoomBackgroundState and RoomWeatherState, run the VS::Field cores and store them back. The other
# camera rules, RoomBackgroundColors (F00642), DrawRoomBackground (F00644) and DuckWeatherAmbientSound (F00669) have
# only hand callers, and ApplyWeatherIntensity (F00661) and DrawRoomBackgroundTiles (F00668) only core callers, so they
# have no binding.
FIELD_CAMERA_LIVE_BINDINGS = {
    "F00073": "LiveClampCameraLookAtToZone",
    "F00074": "LiveComputeCameraTrackingStep",
    "F00075": "LiveStepCameraLookAtTowardTarget",
    "F00082": "LiveStorePreferredActionCameraDistance",
    "F00157": "LiveUpdateCameraTileConstraint",
    "F00177": "LiveIsCameraLookAtOnAshley",
    "F00201": "LiveSyncCameraAnglesFromPosition",
    "F00203": "LiveSetCameraPositionFromAngles",
    "F00205": "LiveCameraPosition",
    "F00206": "LiveSetCameraPosition",
    "F00207": "LiveCameraLookAt",
    "F00208": "LiveSetCameraLookAt",
    "F00209": "LiveSetCameraRoll",
    "F00236": "LiveSetFirstPersonViewDisabled",
    "F00289": "LiveInitializeSphericalCamera",
    "F00290": "LiveSetCameraPitchPreset",
}
FIELD_BACKGROUND_LIVE_BINDINGS = {
    "F00635": "LiveInitializeRoomBackground",
    "F00637": "LiveInitializeRoomBackgroundColors",
    "F00639": "LiveSetBackgroundParallaxScale",
    "F00640": "LiveSetTiledBackgroundEnabled",
    "F00643": "LiveSetRoomBackgroundColors",
    "F00645": "LiveQueueTiledBackgroundLoad",
    "F00646": "LiveFinishTiledBackgroundLoad",
    "F00647": "LiveReleaseTiledBackground",
}
FIELD_ENVIRONMENT_LIVE_BINDINGS = {
    "F00658": "LiveSetRoomWeather",
    "F00659": "LiveUpdateRoomWeather",
    "F00662": "LiveSetWeatherVariationEnabled",
    "F00663": "LiveSetWeatherIntensity",
    "F00664": "LiveSetWeatherWindAngles",
    "F00665": "LiveReleaseWeatherParticles",
    "F00666": "LiveSetWeatherAmbientSound",
    "F00667": "LiveSetRoomBackgroundOffset",
}
for _bindings, _header in (
    (FIELD_CAMERA_LIVE_BINDINGS, r"Native\Game\include\Field\CameraLive.hpp"),
    (FIELD_BACKGROUND_LIVE_BINDINGS, r"Native\Game\include\Field\BackgroundLive.hpp"),
    (FIELD_ENVIRONMENT_LIVE_BINDINGS, r"Native\Game\include\Field\EnvironmentLive.hpp"),
):
    OWNED_CALL_BINDINGS |= {fid: f"VS::Field::{name}" for fid, name in _bindings.items()}
    NATIVE_HELPER_HEADERS |= {f"VS::Field::{name}": _header for name in _bindings.values()}

# #276 slice 25a (field-rooms state, mode and transitions): the generated room loading, interaction, battle, effect,
# script, cutscene and menu callers keep the original transition, door, room visit and field mode call shapes
# (146C.c:1866-4686, 8509-8912, 15756-15765; 18.c:468-595); the live bindings load the transition globals and pending
# door, the visit records, the loaded room's display and the battle action selection into the typed Transitions,
# RoomState and GameMode cores and store them back. ClearSavedRoomEnemies (F00231) is the same rule as
# ForgetRoomEnemies (F00004) and binds to its live adapter.
FIELD_TRANSITIONS_LIVE_BINDINGS = {
    "F00257": "LiveScreenTransitionPhase",
    "F00260": "LiveConfigureScreenTransition",
    "F00261": "LiveSetTransitionWipeAngle",
    "F00262": "LiveBeginScreenFadeOut",
    "F00263": "LiveBeginScreenFadeIn",
    "F00265": "LiveBeginRoomTransition",
    "F00266": "LiveBeginScriptedRoomTransition",
    "F00267": "LiveBeginZoneTransition",
    "F00268": "LiveBeginScriptedZoneTransition",
    "F00269": "LiveTeleportToLocation",
    "F01857": "LiveInitializeRoomTransitionPrimitives",
}
FIELD_ROOM_STATE_LIVE_BINDINGS = {
    "F00087": "LiveMarkCurrentRoomVisited",
    "F00243": "LiveShowPrimaryRoomGeometry",
    "F00244": "LiveHidePrimaryRoomGeometry",
    "F00264": "LiveCancelPendingRoomVisit",
    "F00270": "LiveFindLocationId",
    "F00271": "LiveCurrentRoomId",
    "F00553": "LiveEnableOwnedRoomRendering",
    "F00554": "LiveDisableOwnedRoomRendering",
    "F01859": "LiveInitializeRoomRuntimeState",
}
FIELD_GAME_MODE_LIVE_BINDINGS = {
    "F00051": "LivePauseFieldForInteractionDialog",
    "F00052": "LivePauseFieldForObjectInteraction",
    "F00053": "LivePauseFieldForMenu",
    "F00054": "LiveResumeFieldAfterMenu",
    "F00088": "LiveEnterFieldExplorationMode",
    "F00123": "LiveBeginShortcutMenu",
    "F00124": "LiveEndShortcutMenu",
    "F00125": "LiveBeginBattleActionMenu",
    "F00126": "LiveCloseBattleActionMenu",
    "F00127": "LiveDispatchBattleActionMenuResult",
}
for _bindings, _header in (
    (FIELD_TRANSITIONS_LIVE_BINDINGS, r"Native\Game\include\Field\TransitionsLive.hpp"),
    (FIELD_ROOM_STATE_LIVE_BINDINGS, r"Native\Game\include\Field\RoomStateLive.hpp"),
    (FIELD_GAME_MODE_LIVE_BINDINGS, r"Native\Game\include\Field\GameModeLive.hpp"),
):
    OWNED_CALL_BINDINGS |= {fid: f"VS::Field::{name}" for fid, name in _bindings.items()}
    NATIVE_HELPER_HEADERS |= {f"VS::Field::{name}": _header for name in _bindings.values()}
OWNED_CALL_BINDINGS["F00231"] = "VS::Actors::ForgetLiveRoomEnemies"

# #276 slice 25c (field-room-loading): the generated battle flow, event, interaction and menu callers keep the
# original zone and room loading call shapes (146C.c:14766-17331); the live bindings load the zone context, the loaded
# room record, the pending door and the session state into the typed RoomLoading core and store them back.
FIELD_ROOM_LOADING_LIVE_BINDINGS = {
    "F00532": "LiveQueueZoneLoad",
    "F00533": "LiveFinishZoneLoad",
    "F00534": "LiveReleaseZoneData",
    "F00535": "LiveZoneSoundFileId",
    "F00536": "LiveSpawnRoomActors",
    "F00542": "LiveQueueRoomLoad",
    "F00543": "LiveFinishRoomLoad",
    "F00545": "LiveUnloadCurrentRoom",
    "F00598": "LiveIsCurrentRoomWorkshop",
    "F00600": "LiveCurrentSceneId",
    "F00601": "LiveRoomFallDestination",
    "F00625": "LiveIsRoomAudioSequenceAbsent",
    "F00627": "LiveRoomEnvironmentParameter",
    "F00628": "LiveRoomTrapLevel",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Field::{name}" for fid, name in FIELD_ROOM_LOADING_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Field::{name}": r"Native\Game\include\Field\RoomLoadingLive.hpp"
    for name in FIELD_ROOM_LOADING_LIVE_BINDINGS.values()
}

# #276 slice 25b (field-rooms rendering): the generated menu, battle flow, room loading and script callers keep the
# original field rendering, room scene, tint, palette, texture id and occlusion call shapes (146C.c:15521-16449);
# the live bindings load the room rendering globals, the loaded room's geometry, view triggers and palettes into the
# typed RoomRendering core and store them back. The room load (row 25c) clears the tint through
# LiveClearRoomTint. The ground colour wrapper (F00589, 146C.c:16465) is retired: its only callers, the model
# drawing's, sample the floor layer through VS::Field::SampleLiveRoomLighting.
FIELD_ROOM_RENDERING_LIVE_BINDINGS = {
    "F00546": "LiveSetFieldRenderingEnabled",
    "F00556": "LiveDrawRoomScene",
    "F00557": "LiveDrawRoomGeometry",
    "F00558": "LiveAdvanceRoomAnimationClock",
    "F00559": "LiveSetRoomRenderingEnabled",
    "F00560": "LiveCopyZonePaletteRow",
    "F00561": "LiveUploadRoomPalettes",
    "F00562": "LiveOffsetRoomTextureIds",
    "F00563": "LiveSetRoomTint",
    "F00564": "LiveClearRoomTint",
    "F00565": "LiveSetRoomAnimationsPaused",
    "F00587": "LiveApplyCameraGeometryOcclusion",
    "F00588": "LiveClearGeometryOcclusion",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Field::{name}" for fid, name in FIELD_ROOM_RENDERING_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Field::{name}": r"Native\Game\include\Field\RoomRenderingLive.hpp"
    for name in FIELD_ROOM_RENDERING_LIVE_BINDINGS.values()
}

# #276 actors-room-memory: generated room loading, actor loading, battle flow and gimmick callers keep the original
# actor id, actor pointer and map/zone call shapes (146C.c:783-946, 1842-1864, 6028); the live bindings load the saved
# room records (State_recentRoomEnemyStates) or Battle_actors, run the typed RoomEnemyMemory and ActorRoster cores and
# store the records back. FindRememberedEnemy (F00005), AcquireRememberedEnemy (F00006) and CombatLimbIndex (F01711)
# have no generated callers; the spawn and room restore sites read the memory through the FIXES-only bindings below.
ACTORS_ROOM_MEMORY_LIVE_BINDINGS = {
    "F00003": "ClearLiveRoomEnemyMemory",
    "F00004": "ForgetLiveRoomEnemies",
    "F00007": "StoreLiveRoomActorState",
}
ACTORS_ROOM_MEMORY_FIXES_BINDINGS = (
    "IsLiveRememberedRoom",
    "CopyLiveRememberedEnemy",
    "SetLiveRememberedEnemyHidden",
    "LiveRememberedEnemyHidden",
)
ACTOR_ROSTER_LIVE_BINDINGS = {
    "F00008": "LinkLiveActorAfterAshley",
    "F00009": "UnlinkLiveActorFromRoomList",
    "F00010": "RemoveLiveActorAtSlot",
    "F00049": "FreezeLiveActorAnimations",
    "F00050": "UnfreezeLiveActorAnimations",
    "F00164": "InitializeLivePlaceholderActor",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Actors::{name}" for fid, name in ACTORS_ROOM_MEMORY_LIVE_BINDINGS.items()}
OWNED_CALL_BINDINGS |= {fid: f"VS::Actors::{name}" for fid, name in ACTOR_ROSTER_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Actors::{name}": r"Native\Game\include\Actors\RoomEnemyMemoryLive.hpp"
    for name in (*ACTORS_ROOM_MEMORY_LIVE_BINDINGS.values(), *ACTORS_ROOM_MEMORY_FIXES_BINDINGS)
}
NATIVE_HELPER_HEADERS |= {
    f"VS::Actors::{name}": r"Native\Game\include\Actors\ActorRosterLive.hpp"
    for name in ACTOR_ROSTER_LIVE_BINDINGS.values()
}

# #276 slice 19c (actors-spawning): the generated room loading, script and Ashley callers keep the original actor,
# spawn record, character template and placement call shapes; each live binding decodes the legacy records into the
# typed ActorSpawning records and runs the core over the LiveActorSpawnHost. The internal spawn steps (F00166-F00169,
# F00171, F00173, F00174) have no generated callers outside the promoted files; F00273 and F01853 are
# VS::Core::ReturnZero and VS::Core::NoOperation; F00285 and F01854 are the asset queue's free-slot probe.
ACTOR_SPAWNING_LIVE_BINDINGS = {
    "F00165": "CreateLiveAuxiliaryActor",
    "F00175": "CreateLiveActorWithEquipment",
    "F00176": "SpawnLiveRoomEnemy",
    "F00274": "QueueLiveAshleyModelLoad",
    "F00275": "QueueLiveScriptedActorSpawn",
    "F00276": "LoadLiveActorEquipmentModels",
    "F00277": "RefreshLiveActorEquipmentModels",
    "F00305": "RemoveLiveScriptedActor",
    "F01855": "RestoreLiveAshleyRuntimeState",
    "F01856": "SpawnLiveAshleyWithEquipment",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Actors::{name}" for fid, name in ACTOR_SPAWNING_LIVE_BINDINGS.items()}
OWNED_CALL_BINDINGS |= {fid: "VS::Content::FindLiveFreeActorAssetRequest" for fid in ("F00285", "F01854")}
NATIVE_HELPER_HEADERS |= {
    f"VS::Actors::{name}": r"Native\Game\include\Actors\ActorSpawningLive.hpp"
    for name in ACTOR_SPAWNING_LIVE_BINDINGS.values()
}

# #276 actors-state: the generated battle, script and movement callers keep the original slot-id and record call
# shapes; each live binding resolves the id through the actor record, battle actor, gimmick and weapon tables into the
# typed ActorStateHost or BattleActorHost and runs the core from ActorFlags, BattleActorState, ActorLifecycle or
# ActorPlacement. F00806 and F00807 (the empty placement hooks) need no adapter: callers use VS::Core::NoOperation.
ACTOR_STATE_LIVE_BINDINGS = {
    "F00245": "EnableLiveActorModel",
    "F00246": "EnableAllLiveActorModels",
    "F00247": "DisableLiveActorModel",
    "F00248": "DisableAllLiveActorModels",
    "F00251": "SetLiveActorHostile",
    "F00252": "LiveActorHpPercentage",
    "F00253": "RestoreLiveActorVitals",
    "F00293": "FindLiveActorByModelId",
    "F00294": "LiveActorAtSlot",
    "F00295": "CountLiveActorsByDisposition",
    "F00296": "IsLiveActorRetainedAfterDefeat",
    "F00299": "SetLiveActorLogicEnabled",
    "F00300": "SetOtherLiveActorLogicEnabled",
    "F00301": "SetLiveActorDetached",
    "F00808": "ResetLiveActorMotionState",
    "F00809": "ResetAllLiveActorMovement",
    "F00835": "BeginLiveActorDeath",
    "F00841": "SetLiveActorCompanion",
    "F00851": "AttachLiveActorToParentBone",
    "F00852": "ReleaseLiveActorAttachmentLock",
    "F00864": "SetLiveActorDisplayFlags",
    "F00872": "LiveActorStateFlags",
    "F00873": "LiveActorPlacement",
    "F00901": "PlaceLiveActorInRoom",
    "F00902": "FindLiveActorAtFlightPosition",
    "F00903": "SeparateLiveActorFlightPlacement",
    "F00906": "LiveActorSavedPlacement",
    "F00907": "FindLiveNearestActorAhead",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Actors::{name}" for fid, name in ACTOR_STATE_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Actors::{name}": r"Native\Game\include\Actors\ActorStateLive.hpp"
    for name in ACTOR_STATE_LIVE_BINDINGS.values()
}

# #276 effects-trail-nine-templates: each overlay's nine-sample trail dispatch instance (trails9.h:685) keeps the
# renderer-slot entry shape in generated renderer tables; its live binding runs the same typed emitter lifecycle over
# Effects_Trail9State with that overlay's emission cursor and the shared nine-sample stages (TrailNineTarget ...
# TrailNineRender).
EFFECT_TRAIL_NINE_LIVE_BINDINGS = {
    "F01907": "UpdateLivePlg049TrailNine",
    "F01918": "UpdateLivePlg050TrailNine",
    "F01941": "UpdateLivePlg057TrailNine",
    "F01973": "UpdateLivePlg069TrailNine",
    "F02096": "UpdateLivePlg135TrailNine",
    "F02206": "UpdateLivePlg161TrailNine",
    "F02219": "UpdateLivePlg165TrailNine",
    "F02235": "UpdateLivePlg167TrailNine",
    "F02242": "UpdateLivePlg168TrailNine",
    "F02260": "UpdateLivePlg169TrailNine",
    "F02286": "UpdateLivePlg232TrailNine",
}
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Effects::Templates::{name}" for fid, name in EFFECT_TRAIL_NINE_LIVE_BINDINGS.items()
}
NATIVE_HELPER_HEADERS |= {
    f"VS::Effects::Templates::{name}": r"Native\Game\include\Effects\Templates\TrailsLive.hpp"
    for name in EFFECT_TRAIL_NINE_LIVE_BINDINGS.values()
}

# #276 menu-framework: the generated menus, battle flow and MENU*.PRG overlays keep the original row, text, ordering
# table and record call shapes; each live binding loads the framework's variables into the typed State, runs the
# Menu::Framework function over the workspace, ordering tables, disc queue, text boxes and submenus, and stores the
# State back. Overlay copies of one routine bind to the same entry. NoOperation (F02552, F02842, F03062, F03063,
# F03065) needs no adapter: callers use the typed name from Framework.hpp.
MENU_FRAMEWORK_LIVE_BINDINGS = {
    "F01342": "LoadLiveMenuOverlay",
    "F01344": "LiveMenuItem",
    "F01345": "SetLiveMenuItem",
    "F01347": "LiveGradientStopColor",
    "F01348": "DrawLiveMenuItem",
    "F01349": "UpdateLiveMainMenuSelection",
    "F01350": "UpdateLiveMenuOverlay",
    "F01364": "InitLiveGameInterface",
    "F01365": "ResumeLiveAfterActionSelection",
    "F01366": "LiveActionRestrictionFlags",
    "F01367": "OpenLiveGameMenu",
    "F01371": "OpenLiveShortcutMenu",
    "F01372": "ReleaseLiveMenuRows",
    "F01373": "OpenLiveLootListMenu",
    "F01374": "OpenLiveLootEntryMenu",
    "F01375": "OpenLiveChestStorageMenu",
    "F01376": "OpenLiveAbilityUnlockMenu",
    "F01377": "OpenLiveChallengeMenu",
    "F01387": "LiveActionSelectionState",
    "F01407": "DrawLiveMainMenuCursor",
    "F02512": "CreateLiveActionRows",
    "F02513": "LiveHighlightedActionRow",
    "F02514": "LiveActionSelectionResult",
    "F02515": "CreateLiveActionRow",
    "F02516": "UpdateLiveActionMenu",
    "F02517": "UpdateLiveInformationVisibility",
    "F02518": "SetLiveCommandLabel",
    "F02519": "DrawLiveCommandLabel",
    "F02520": "SetLiveBackgroundFade",
    "F02521": "DeactivateLiveRow",
    "F02522": "DismissLiveInformationBox",
    "F02523": "InitializeLiveInformationBox",
    "F02524": "SetLiveInformationMessage",
    "F02525": "DrawLiveButtonIcon",
    "F02526": "DrawLiveCursor",
    "F02527": "DrawLiveColoredInteger",
    "F02528": "DrawLiveComparedInteger",
    "F02529": "DrawLiveNeutralInteger",
    "F02530": "DrawLiveFeatheredRowIcon",
    "F02531": "DrawLiveMaterialIndicator",
    "F02532": "DrawLiveEquipmentStateIcon",
    "F02533": "DrawLiveRowIcon",
    "F02534": "SetLiveActionCostText",
    "F02535": "DrawLiveActionCostPanel",
    "F02536": "DrawLiveButtonPromptBackground",
    "F02537": "SetLiveBackgroundPriority",
    "F02538": "LoadLiveBackgroundTexture",
    "F02539": "CreateLiveMenuRows",
    "F02540": "CloseLiveRowsAndRememberCursor",
    "F02541": "LiveHighlightedRow",
    "F02542": "LiveSelectedRow",
    "F02543": "CreateLiveListRow",
    "F02544": "UpdateLiveRowSelection",
    "F02545": "DrawLiveBackground",
    "F02546": "DrawLiveFeatheredRowCharacter",
    "F02547": "DrawLiveTabNavigation",
    "F02548": "DrawLiveScrollableRow",
    "F02549": "UpdateAndDrawLiveRows",
    "F02550": "UpdateLiveScreen",
    "F02565": "RenderLiveAshley",
    "F02570": "ContinueLiveItemDetailsTransition",
    "F02572": "UpdateLiveItemDetailsSelection",
    "F02582": "LiveWeaponSortValue",
    "F02584": "LiveShieldSortValue",
    "F02586": "LiveMiscSortValue",
    "F02588": "LiveEquipmentSortValue",
    "F02683": "SetLiveMenuVisible",
    "F02688": "ClearLiveMenuExceptTitle",
    "F02697": "BuildLiveYawMatrix",
    "F02698": "BuildLivePitchMatrix",
    "F02798": "SetLiveMenuVisible",
    "F02802": "ShowLiveSecondaryTitle",
    "F02808": "BuildLiveYawMatrix",
    "F02809": "BuildLivePitchMatrix",
    "F02946": "ContinueLiveItemDetailsTransition",
    "F02948": "UpdateLiveItemDetailsSelection",
    "F02960": "LiveWeaponSortValue",
    "F02962": "LiveShieldSortValue",
    "F02964": "LiveMiscSortValue",
    "F02966": "LiveEquipmentSortValue",
    "F02988": "SetLiveMenuVisible",
    "F02990": "ShowLiveSecondaryTitle",
    "F04136": "LiveNextFreeActorAssetRequest",
    "F04138": "FlyOutLiveRowLeft",
    "F04139": "FlyOutLiveRowsLeftExcept",
    "F04140": "FlyOutLiveRowRight",
    "F04141": "ClearLiveMenuExcept",
    "F04142": "FlyOutLiveRowsAndHoistSelection",
    "F04143": "AreLiveMenuRowsIdle",
    "F04144": "LiveQuitDelay",
    "F04145": "ReturnLiveFromSubmenu",
    "F04146": "UpdateLiveMainMenu",
}
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Menu::Framework::{name}" for fid, name in MENU_FRAMEWORK_LIVE_BINDINGS.items()
}
NATIVE_HELPER_HEADERS |= {
    f"VS::Menu::Framework::{name}": r"Native\Game\include\Menu\Framework\FrameworkLive.hpp"
    for name in MENU_FRAMEWORK_LIVE_BINDINGS.values()
}

# #276 menu-files-options: the generated title screen (TitleMenu.cpp, SaveFiles.cpp) keeps the TITLE.PRG call shapes of
# the file screens' shared drawing and display routines; FilesLive.cpp binds them to the typed VS::Menu::Files core
# over the title screen's text table, elements and display buffers. DecimalDigitCount keeps its shape and is called
# by its typed name. The MENU7.PRG and MENU8.PRG screens have only hand-maintained callers.
MENU_FILES_LIVE_BINDINGS = {
    "F02741": "DrawLiveInteger",
    "F03608": "DrawLiveInteger",
    "F02753": "DrawLiveText",
    "F03620": "DrawLiveText",
    "F02755": "LiveElementBackgroundColor",
    "F03622": "LiveElementBackgroundColor",
    "F02758": "DrawLiveBackground",
    "F03625": "DrawLiveBackground",
    "F03635": "InitializeLiveDisplay",
    "F03636": "SwapLiveBuffers",
    "F03639": "UpdateLiveClearSaveReminder",
    "F03640": "DrawLiveExitFade",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Menu::Files::{name}" for fid, name in MENU_FILES_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Menu::Files::{name}": r"Native\Game\include\Menu\FilesLive.hpp"
    for name in MENU_FILES_LIVE_BINDINGS.values()
}

# #276 menu-framework-rows-details: the generated menus keep the MAINMENU.PRG call shapes of the item rows, the
# equipment details and the actor preview; ItemRowsLive.cpp, EquipmentDetailsLive.cpp and ActorPreviewLive.cpp bind
# them to the typed VS::Menu::Framework cores over the menu rows, the item tables and the actor records.
ITEM_ROWS_LIVE_BINDINGS = {
    "F02472": "CreateLiveWeaponComponentRow",
    "F02473": "CreateLiveShieldGemRow",
    "F02474": "FormatLiveWeaponRow",
    "F02475": "FormatLiveInventoryWeaponRow",
    "F02476": "FormatLiveBladeRow",
    "F02477": "FormatLiveGripRow",
    "F02478": "FormatLiveShieldRow",
    "F02479": "FormatLiveInventoryShieldRow",
    "F02480": "FormatLiveArmorRow",
    "F02481": "FormatLiveInventoryArmorRow",
    "F02482": "FormatLiveGemRow",
    "F02483": "FormatLiveMiscItemRow",
    "F02503": "LoadLiveItemText",
    "F02504": "UnloadLiveItemTextWhenReady",
}
EQUIPMENT_DETAILS_LIVE_BINDINGS = {
    "F02484": "ResetLiveEquipmentStats",
    "F02485": "SetLiveWeaponStats",
    "F02486": "SetLiveBladeStats",
    "F02487": "SetLiveGripStats",
    "F02488": "SetLiveShieldStats",
    "F02489": "SetLiveArmorStats",
    "F02490": "SetLiveGemStats",
    "F02497": "ResetLiveItemDetailCursor",
    "F02499": "UpdateLiveItemDetails",
    "F02555": "DrawLiveEquipmentStatPage",
    "F02557": "SetLiveEnabledStatPages",
    "F02558": "UpdateLiveEquipmentStatPages",
    "F02559": "SetLiveRangeAndRisk",
    "F02560": "SetLivePrimaryStats",
    "F02561": "SetLiveActorComparisonStats",
    "F02562": "UpdateLivePrimaryStatPanel",
    "F02563": "SetLiveDpPp",
    "F02564": "UpdateLiveDpPpBars",
}
ACTOR_PREVIEW_LIVE_BINDINGS = {
    "F02463": "LoadLiveActorPreview",
    "F02464": "CloneLiveActorPreview",
    "F02465": "ResetLiveActorPreviewPose",
    "F02466": "ReleaseLiveActorPreview",
    "F02467": "DrawLiveActorPreview",
    "F02470": "ResetLiveAshleyEquipmentPose",
    "F02471": "QueueLiveAshleyWeaponAnimations",
}
for _bindings, _header in ((ITEM_ROWS_LIVE_BINDINGS, "ItemRowsLive.hpp"),
                           (EQUIPMENT_DETAILS_LIVE_BINDINGS, "EquipmentDetailsLive.hpp"),
                           (ACTOR_PREVIEW_LIVE_BINDINGS, "ActorPreviewLive.hpp")):
    OWNED_CALL_BINDINGS |= {fid: f"VS::Menu::Framework::{name}" for fid, name in _bindings.items()}
    NATIVE_HELPER_HEADERS |= {
        f"VS::Menu::Framework::{name}": "Native\\Game\\include\\Menu\\Framework\\" + _header
        for name in _bindings.values()
    }
del _bindings, _header

# Owned globals whose contents are a disc string table: the hand-maintained Live binding defines the storage and
# ModuleData.cpp still fills it from the module image at start-up.
OWNED_MODULE_STRING_TABLES = {
    "G01269",  # Menu_Files_containerStrings (MENU7.PRG _containerStrings), FilesLive.cpp
    "G01393",  # Menu_Options_menuStrings (MENU8.PRG _menuStrings), OptionsLive.cpp
    "G01396",  # Menu_Rename_renderRenameMenuStrings (MENU8.PRG _renderRenameMenuStrings), RenameLive.cpp
}

# #276 audio-control-sfx: generated music, stream, cutscene, actor, field, event and menu callers keep the original
# group/ID, bank pointer, SVECTOR and request call shapes; the live bindings load the tag cursors and bank slots,
# dispatch through the mixer and store the cursors back.
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Audio::{name}"
    for fid, name in (
        ("F03101", "Control::LiveSetMusicBranchThreshold"),
        ("F03118", "Control::LiveFadeMusicVolume"),
        ("F03120", "Control::LiveSetDiscVolume"),
        ("F03124", "Control::LiveFadeMusicTempoScale"),
        ("F03127", "Control::LiveFadeMusicPitchScale"),
        ("F03132", "Control::LiveStopAllMusic"),
        ("F03145", "Control::LiveStartCutsceneStream"),
        ("F03146", "Control::LiveStopCutsceneStream"),
        ("F03148", "Control::LiveFadeCutsceneVolume"),
        ("F03203", "SoundEffects::LiveReleaseMatchingVoices"),
        ("F03515", "LivePlaySfx"),
        ("F03516", "LivePlaySfxCentered"),
        ("F03517", "LiveSfxPanAndVolume"),
        ("F03519", "LivePlayWorldSfx"),
        ("F03521", "LiveFadeSfxVolume"),
        ("F03522", "LiveFadeSfxPitch"),
        ("F03523", "LiveFadeSfxPan"),
        ("F03524", "LiveStopSfx"),
        ("F03525", "LiveStopAllSfxPlayback"),
        ("F03535", "LivePlayBankSfx"),
        ("F03536", "LivePlayBankSfxCentered"),
        ("F03539", "LiveStopBankSfx"),
        ("F03540", "LiveFadeBankSfxVolume"),
    )
}

# #276 audio-music-streams: generated script, room, menu, credits, effect, actor and gimmick callers keep the original
# slot/ID, drive, actor record, model instance and gimmick call shapes; the live bindings read the sound state and
# legacy records, run the typed cores and store the state back.
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Audio::Live{name}"
    for fid, name in (
        ("F00237", "SetZoneMusicSwitchEnabled"),
        ("F00272", "PlayActorScriptSound"),
        ("F00626", "SetRoomAmbienceDucked"),
        ("F00994", "ActorSoundPanAndVolume"),
        ("F00999", "PlayActorModelSound"),
        ("F01000", "PlayActorModelSoundVariant"),
        ("F01001", "PlayActorSoundPair"),
        ("F01002", "PlayGimmickSoundPair"),
        ("F01003", "PlayActorSurfaceContactSound"),
        ("F03477", "BeginXaStream"),
        ("F03478", "RequestXaStreamStop"),
        ("F03487", "MusicBeatPosition"),
        ("F03488", "SaveMusicPlaybackState"),
        ("F03489", "SetMusicJumpThreshold"),
        ("F03490", "StopMusicSlotPlayback"),
        ("F03491", "ResumeMusicSlot"),
        ("F03492", "CrossfadeMusicSlot"),
        ("F03493", "PlayMusicSlot"),
        ("F03494", "CurrentMusicSlot"),
        ("F03495", "CurrentMusicId"),
        ("F03496", "LoadMusicSlot"),
        ("F03498", "PollMusicSlotLoad"),
        ("F03499", "PollMusicLoads"),
        ("F03500", "LoadMusicSlotBlocking"),
        ("F03502", "FreeMusicSlot"),
        ("F03504", "MusicSlotVolume"),
        ("F03505", "QueryMusicSlotVolume"),
        ("F03506", "FadeMusicSlotVolume"),
        ("F03507", "FadeMusicSlotTempo"),
        ("F03508", "FadeMusicSlotPitch"),
        ("F03511", "StopAndFreeCurrentMusic"),
        ("F03543", "PlayCutsceneStreamSlotCentered"),
    )
}

EVENT_SCRIPT_CONTROL_OPCODES = (
    ("F01187", 0x800BB028, "CommandSpecialOperation"),
    ("F01027", 0x800B66A8, "ScriptReportInvalidOpcode"),
    ("F01030", 0x800B66F4, "ScriptWaitForEventFile"),
    ("F01205", 0x800BB5AC, "CommandLoadNextEvent"),
    ("F01206", 0x800BB604, "CommandFinishEvent"),
    ("F01106", 0x800B94D8, "ScriptConfigureTimer"),
    ("F01111", 0x800B9724, "ScriptConfigureEasingCurve"),
    ("F01042", 0x800B6FB0, "ScriptWaitForButtonAndYield"),
    ("F01110", 0x800B96B0, "ScriptWaitForButtonOrTimeout"),
    ("F01032", 0x800B6744, "ScriptBeginSkippableCutscene"),
    ("F01034", 0x800B6868, "ScriptEndCutsceneAndJump"),
    ("F01035", 0x800B68C4, "ScriptEndCutscene"),
    ("F01028", 0x800B66E4, "ScriptNoOperation"),
    ("F01029", 0x800B66EC, "ScriptYieldFrame"),
    ("F01085", 0x800B88D4, "ScriptBranchOnFlagImmediate"),
    ("F01086", 0x800B8A10, "ScriptBranchOnFlagPair"),
    ("F01087", 0x800B8B5C, "ScriptJumpForward"),
    ("F01088", 0x800B8B90, "ScriptJumpForward"),
    ("F01091", 0x800B8C44, "ScriptReturnToSavedAddress"),
    ("F01094", 0x800B8CE8, "ScriptJumpForward"),
    ("F01095", 0x800B8D1C, "ScriptJumpForward"),
    ("F01089", 0x800B8BC4, "ScriptCacheFlagValue"),
    ("F01090", 0x800B8BF8, "ScriptBranchOnCachedFlag"),
    ("F01092", 0x800B8C54, "ScriptSetLoopCounter"),
    ("F01093", 0x800B8C7C, "ScriptLoopBackward"),
    ("F01096", 0x800B8D50, "ScriptApplyImmediateFlagOperation"),
    ("F01097", 0x800B8EDC, "ScriptApplyFlagPairOperation"),
    ("F01098", 0x800B9170, "ScriptStoreInventoryItemPresence"),
    ("F01102", 0x800B9254, "ScriptStoreButtonState"),
    ("F01199", 0x800BB474, "CommandWaitFrames"),
    ("F01200", 0x800BB4CC, "CommandSetTickSpeed"),
    ("F01202", 0x800BB504, "CommandWaitScriptContext"),
    ("F01203", 0x800BB554, "CommandStartScriptContext"),
    ("F01204", 0x800BB5A4, "CommandStopScriptContext"),
)
NATIVE_HELPER_HEADERS |= {
    f"Event_{name}Opcode": r"Native\Game\src\Event\ScriptControlOpcodes.h"
    for _, _, name in EVENT_SCRIPT_CONTROL_OPCODES
}

EVENT_CUTSCENE_LIVE_BINDINGS = {
    "F00128": "PrepareLiveCutsceneActors",
    "F00129": "StartLivePreparedCutscene",
    "F00130": "BeginLiveQueuedCutscene",
    "F00131": "EndLiveFieldCutscene",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Event::{name}" for fid, name in EVENT_CUTSCENE_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Event::{name}": r"Native\Game\include\Event\CutsceneLifecycleLive.hpp"
    for name in EVENT_CUTSCENE_LIVE_BINDINGS.values()
}

# #276 row 34 (event script actor opcodes): the 47 BATTLE.PRG actor opcode handlers of 4A0A8.c are typed C++
# (Event/ScriptActorCommands.cpp) behind a ScriptActorHost. Each opcode table slot holds a C binding with the dispatch
# ABI (Event/ScriptActorOpcodes.h) at the handler's module-image address. The generated script VM, screen and dialog
# callers of the selection, activity and dialog-anchor helpers keep the original call shapes through
# ScriptActorCommandsLive.hpp.
EVENT_SCRIPT_ACTOR_OPCODES = (
    ("F01051", 0x800B7428, "ScriptResetActorGroupState"),
    ("F01052", 0x800B7490, "ScriptStoreActorTile"),
    ("F01053", 0x800B7574, "ScriptSpawnActor"),
    ("F01054", 0x800B75F4, "ScriptNavigateActorToTile"),
    ("F01055", 0x800B76A8, "ScriptPlayActorAnimation"),
    ("F01056", 0x800B770C, "ScriptSetActorGroupFlags"),
    ("F01057", 0x800B77DC, "ScriptSetActorSoundMix"),
    ("F01058", 0x800B784C, "ScriptWalkActorToTile"),
    ("F01059", 0x800B792C, "ScriptWalkActorToWorldPos"),
    ("F01060", 0x800B7A98, "ScriptSlideActorToTile"),
    ("F01061", 0x800B7B58, "ScriptSlideActorToWorldPos"),
    ("F01062", 0x800B7C90, "ScriptSetActorAttachment"),
    ("F01063", 0x800B7D10, "ScriptNavigateActorToWorldPos"),
    ("F01064", 0x800B7DC4, "ScriptTurnActorToFacing"),
    ("F01065", 0x800B7EF0, "ScriptFaceOtherActor"),
    ("F01066", 0x800B7F84, "ScriptFaceTile"),
    ("F01067", 0x800B804C, "ScriptScaleActor"),
    ("F01068", 0x800B80B4, "ScriptRemoveActorGroup"),
    ("F01069", 0x800B810C, "ScriptWaitForActorFlags"),
    ("F01070", 0x800B81A0, "ScriptSetActorColorOverride"),
    ("F01071", 0x800B822C, "ScriptQueueActorAnimation"),
    ("F01072", 0x800B8290, "ScriptRotateActorAxes"),
    ("F01073", 0x800B8360, "ScriptSetActorHeadAngles"),
    ("F01074", 0x800B83D4, "ScriptPlayActorAnimationAtFrame"),
    ("F01075", 0x800B8438, "ScriptQueueActorAnimationAtFrame"),
    ("F01076", 0x800B849C, "ScriptSetActorIdleAnimation"),
    ("F01077", 0x800B8510, "ScriptLookAtActor"),
    ("F01078", 0x800B8590, "ScriptLookAtWorldPos"),
    ("F01079", 0x800B860C, "ScriptLoadObjectModel"),
    ("F01080", 0x800B86E8, "ScriptWaitForObjectLoad"),
    ("F01081", 0x800B8708, "ScriptReleaseObjectModel"),
    ("F01082", 0x800B8764, "ScriptSelectObjectTextureBank"),
    ("F01083", 0x800B8788, "ScriptSetActorOverlayPass"),
    ("F01084", 0x800B884C, "ScriptSetActorWeaponDrawn"),
    ("F01100", 0x800B91F0, "ScriptPlayActorAnimationSound"),
    ("F01101", 0x800B9230, "ScriptSetNpcUpdatesEnabled"),
    ("F01103", 0x800B92A0, "ScriptRestoreActorVitals"),
    ("F01108", 0x800B9608, "ScriptSetActorEquipmentModels"),
    ("F01109", 0x800B9670, "ScriptSetActorYawOscillation"),
    ("F01115", 0x800B97F8, "ScriptClearAshleyStatusEffects"),
    ("F01129", 0x800B9D74, "ScriptSetActorRenderMode"),
    ("F01180", 0x800BAC38, "CommandSetEnteredDoor"),
    ("F01181", 0x800BAC5C, "CommandStoreActorRangeBearing"),
    ("F01182", 0x800BAD78, "CommandSetActorActivity"),
    ("F01183", 0x800BADE0, "CommandAwardMiscLoot"),
    ("F01184", 0x800BAE74, "CommandConsumeMiscItem"),
    ("F01185", 0x800BAEA0, "CommandStoreActorDistance"),
)
NATIVE_HELPER_HEADERS |= {
    f"Event_{name}Opcode": r"Native\Game\src\Event\ScriptActorOpcodes.h" for _, _, name in EVENT_SCRIPT_ACTOR_OPCODES
}
EVENT_SCRIPT_ACTOR_LIVE_BINDINGS = {
    "F01186": "SetLiveEventActorActivity",
    "F01208": "BuildLiveScriptActorSelection",
    "F01209": "NextLiveScriptSelectedActor",
    "F01210": "LiveActorDialogAnchor",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Event::{name}" for fid, name in EVENT_SCRIPT_ACTOR_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Event::{name}": r"Native\Game\include\Event\ScriptActorCommandsLive.hpp"
    for name in EVENT_SCRIPT_ACTOR_LIVE_BINDINGS.values()
}

# #276 row 36 (event script audio, dialog and effects): the 53 BATTLE.PRG sound, music, dialog box, effect and
# screen effect opcode handlers of 4A0A8.c are typed C++ (Event/Script{Audio,Dialog,Effect}Commands.cpp) behind
# ScriptAudioHost, ScriptDialogHost and ScriptEffectHost. Each opcode table slot holds a C binding with the dispatch
# ABI at the handler's module-image address. The dialog placement helper's only caller is promoted; the script VM's
# per-frame scanline, splash tween and weather tween updates keep their call shapes through
# ScriptEffectCommandsLive.hpp.
EVENT_SCRIPT_AUDIO_OPCODES = (
    ("F01143", 0x800BA2E0, "ScriptPlaySoundCommand"),
    ("F01144", 0x800BA35C, "CommandPlayActorSfx"),
    ("F01145", 0x800BA39C, "CommandSlideSfxVolume"),
    ("F01146", 0x800BA3E4, "CommandStopAllSfx"),
    ("F01147", 0x800BA404, "CommandStopSfx"),
    ("F01148", 0x800BA444, "CommandLoadSfxSlot"),
    ("F01149", 0x800BA470, "CommandWaitSfxSlotRelease"),
    ("F01150", 0x800BA494, "CommandFreeSfx"),
    ("F01151", 0x800BA4C8, "CommandSelectSfxBank"),
    ("F01152", 0x800BA4EC, "CommandLoadEventMusic"),
    ("F01153", 0x800BA52C, "CommandCrossfadeMusic"),
    ("F01154", 0x800BA588, "CommandLoadMusicSlot"),
    ("F01155", 0x800BA5B4, "CommandFreeMusic"),
    ("F01156", 0x800BA5E8, "CommandPlayMusic"),
    ("F01157", 0x800BA61C, "CommandSlideMusicVolume"),
    ("F01158", 0x800BA650, "CommandSlideMusicTempo"),
    ("F01159", 0x800BA684, "CommandSlideMusicPitch"),
    ("F01160", 0x800BA6B8, "CommandStopMusic"),
    ("F01161", 0x800BA6E0, "CommandWaitMusicSlot"),
    ("F01162", 0x800BA72C, "CommandPushMusicState"),
    ("F01163", 0x800BA74C, "CommandWaitMusicSlotLoad"),
    ("F01164", 0x800BA774, "CommandLoadSoundFile"),
    ("F01165", 0x800BA798, "CommandWaitSoundQueue"),
    ("F01166", 0x800BA7BC, "CommandWaitMusicTimer"),
    ("F01167", 0x800BA800, "CommandSlideSfxPitch"),
    ("F01168", 0x800BA848, "CommandSlideSfxPan"),
    ("F01169", 0x800BA890, "CommandResumeMusicSlot"),
    ("F01177", 0x800BAAF0, "CommandSetMusicJumpThreshold"),
    ("F01207", 0x800BB668, "CommandPlayMenuSelectSfx"),
)
EVENT_SCRIPT_DIALOG_OPCODES = (
    ("F01036", 0x800B6908, "ScriptInitDialogBox"),
    ("F01037", 0x800B6AB0, "ScriptShowDialogText"),
    ("F01038", 0x800B6B74, "ScriptDismissDialogBox"),
    ("F01040", 0x800B6D48, "ScriptInitActorDialogBox"),
    ("F01041", 0x800B6F8C, "ScriptWaitForDialogDismissal"),
    ("F01048", 0x800B7218, "ScriptSetDialogInteger"),
    ("F01049", 0x800B7358, "ScriptSetDialogTransitionDuration"),
    ("F01050", 0x800B7398, "ScriptSelectDialogChoice"),
    ("F01107", 0x800B9564, "ScriptSetDialogSpeaker"),
)
EVENT_SCRIPT_EFFECT_OPCODES = (
    ("F01170", 0x800BA8BC, "CommandPlayActorEffect"),
    ("F01171", 0x800BA90C, "CommandAdvanceEffectStage"),
    ("F01172", 0x800BA94C, "CommandWaitEffectState"),
    ("F01173", 0x800BAA04, "CommandYieldEffectRelease"),
    ("F01174", 0x800BAA34, "CommandWaitEffectContinuation"),
    ("F01175", 0x800BAA58, "CommandWaitEffectRelease"),
    ("F01176", 0x800BAACC, "CommandMarkEffectStageReady"),
    ("F01178", 0x800BAB14, "CommandPlayVibrationPattern"),
    ("F01179", 0x800BABCC, "CommandSetActorTrail"),
    ("F01188", 0x800BB288, "CommandWaitScreenEffectLoad"),
    ("F01189", 0x800BB2B8, "CommandSetScreenDistortion"),
    ("F01190", 0x800BB2E4, "CommandLoadScreenEffects"),
    ("F01191", 0x800BB304, "CommandReservedScreenEffect"),
    ("F01192", 0x800BB30C, "CommandFreeScreenEffects"),
    ("F01201", 0x800BB4F0, "CommandSetScanlineOverlay"),
)
EVENT_SCRIPT_MEDIA_OPCODE_HEADERS = (
    (r"Native\Game\src\Event\ScriptAudioOpcodes.h", EVENT_SCRIPT_AUDIO_OPCODES),
    (r"Native\Game\src\Event\ScriptDialogOpcodes.h", EVENT_SCRIPT_DIALOG_OPCODES),
    (r"Native\Game\src\Event\ScriptEffectOpcodes.h", EVENT_SCRIPT_EFFECT_OPCODES),
)
EVENT_SCRIPT_MEDIA_OPCODES = EVENT_SCRIPT_AUDIO_OPCODES + EVENT_SCRIPT_DIALOG_OPCODES + EVENT_SCRIPT_EFFECT_OPCODES
NATIVE_HELPER_HEADERS |= {
    f"Event_{name}Opcode": header for header, rows in EVENT_SCRIPT_MEDIA_OPCODE_HEADERS for _, _, name in rows
}
EVENT_SCRIPT_EFFECT_LIVE_BINDINGS = {
    "F01211": "DrawLiveScanlineOverlay",
    "F01212": "UpdateLiveImageTweens",
    "F01213": "UpdateLiveWeatherTween",
}
OWNED_CALL_BINDINGS |= {fid: f"VS::Event::{name}" for fid, name in EVENT_SCRIPT_EFFECT_LIVE_BINDINGS.items()}
NATIVE_HELPER_HEADERS |= {
    f"VS::Event::{name}": r"Native\Game\include\Event\ScriptEffectCommandsLive.hpp"
    for name in EVENT_SCRIPT_EFFECT_LIVE_BINDINGS.values()
}

# #276 row 31 (battle targeting and prerequisites): the 83 BATTLE.PRG target collection, sphere target menu,
# attack prerequisite, battle ability and battle mode functions are typed C++ (Battle/{Targeting,AttackPrerequisites,
# Abilities,BattleMode}.cpp) behind ActionTargetingHost, TargetSelectionHost, PrerequisiteTarget, AbilityHost and
# BattleModeHost. Generated callers keep their call shapes through the *Live.hpp bindings; each attack prerequisite
# table slot holds a Battle_*Slot C binding with the table ABI at the rule's module-image address.
BATTLE_TARGETING_LIVE_BINDINGS = {
    "F00102": "LiveReturnToBattleActionMenu",
    "F00103": "LiveFilterAreaActionTargets",
    "F00105": "LiveSetSingleActionTarget",
    "F00106": "LiveSetAllActionTargets",
    "F00107": "LiveCollectEnvironmentActionTargets",
    "F00115": "LiveInitializeActionTargeting",
    "F00116": "LiveBeginActorActionSelection",
    "F01313": "LiveUpdateSphereTargetPosition",
    "F01323": "LiveUpdateTargetSelectionMenu",
    "F01808": "LiveCalculateActionAreaReach",
    "F01809": "LiveCalculateActionRangeSquared",
}
BATTLE_ABILITIES_LIVE_BINDINGS = {
    "F00000": "LiveArmDefenseAbilityTimingWindow",
    "F00091": "LivePendingBattleAbilityUnlocks",
    "F00093": "LiveBeginPendingAbilityReward",
    "F00094": "LiveGrantPendingAbilityUnlock",
    "F00303": "LiveIsActionUnlocked",
    "F00304": "LiveUnlockAction",
    "F01368": "LiveHasUnlockedSpellClass",
    "F01369": "LiveHasUnlockedWeaponBreakArt",
    "F01370": "LiveHasUnlockedBattleAbility",
}
BATTLE_ATTACK_PREREQUISITES_LIVE_BINDINGS = {"F00393": "LiveCheckActionHitPrerequisite"}
BATTLE_MODE_LIVE_BINDINGS = {"F00249": "LiveIsActorWeaponDrawn", "F00250": "LiveSetActorBattleMode"}
BATTLE_TARGETING_PREREQUISITE_LIVE_HEADERS = (
    (r"Native\Game\include\Battle\TargetingLive.hpp", BATTLE_TARGETING_LIVE_BINDINGS),
    (r"Native\Game\include\Battle\AbilitiesLive.hpp", BATTLE_ABILITIES_LIVE_BINDINGS),
    (r"Native\Game\include\Battle\AttackPrerequisitesLive.hpp", BATTLE_ATTACK_PREREQUISITES_LIVE_BINDINGS),
    (r"Native\Game\include\Battle\BattleModeLive.hpp", BATTLE_MODE_LIVE_BINDINGS),
)
OWNED_CALL_BINDINGS |= {
    fid: f"VS::Battle::{name}" for _, bindings in BATTLE_TARGETING_PREREQUISITE_LIVE_HEADERS
    for fid, name in bindings.items()
}
NATIVE_HELPER_HEADERS |= {
    f"VS::Battle::{name}": header for header, bindings in BATTLE_TARGETING_PREREQUISITE_LIVE_HEADERS
    for name in bindings.values()
}
BATTLE_ATTACK_PREREQUISITE_SLOTS = (
    ("F00353", 0x8007F548, "RejectAttackPrerequisite"),
    ("F00354", 0x8007F550, "AcceptAttackPrerequisite"),
    ("F00355", 0x8007F558, "AcceptAttackPrerequisite"),
    ("F00356", 0x8007F560, "AcceptAttackPrerequisite"),
    ("F00357", 0x8007F568, "IsAttackTargetUndead"),
    ("F00358", 0x8007F58C, "IsAttackTargetLiving"),
    ("F00359", 0x8007F5B0, "RejectAttackPrerequisite"),
    ("F00360", 0x8007F5B8, "RejectAttackPrerequisite"),
    ("F00361", 0x8007F5C0, "RejectAttackPrerequisite"),
    ("F00362", 0x8007F5C8, "CanApplyStrengthDown"),
    ("F00363", 0x8007F5E8, "CanApplyIntelligenceDown"),
    ("F00364", 0x8007F608, "CanApplyAgilityDown"),
    ("F00365", 0x8007F628, "CanApplyEquipmentDown"),
    ("F00366", 0x8007F6D8, "CanApplyHalfMovement"),
    ("F00367", 0x8007F6F8, "CanApplyStrengthUp"),
    ("F00368", 0x8007F718, "CanApplyIntelligenceUp"),
    ("F00369", 0x8007F738, "CanApplyAgilityUp"),
    ("F00370", 0x8007F758, "CanApplyEquipmentUp"),
    ("F00371", 0x8007F808, "CanApplyQuicken"),
    ("F00372", 0x8007F828, "CanApplySilence"),
    ("F00373", 0x8007F848, "CanApplyMagicWard"),
    ("F00374", 0x8007F868, "CanApplyRegeneration"),
    ("F00375", 0x8007F888, "CanApplyParalysis"),
    ("F00376", 0x8007F8A8, "CanApplyPoison"),
    ("F00377", 0x8007F8C8, "CanApplyNumbness"),
    ("F00378", 0x8007F8E8, "CanApplyCurse"),
    ("F00379", 0x8007F908, "CanApplyAirAttackUp"),
    ("F00380", 0x8007F96C, "CanApplyFireAttackUp"),
    ("F00381", 0x8007F9D0, "CanApplyEarthAttackUp"),
    ("F00382", 0x8007FA34, "CanApplyWaterAttackUp"),
    ("F00383", 0x8007FA98, "CanApplyAirDefenseUp"),
    ("F00384", 0x8007FB34, "CanApplyFireDefenseUp"),
    ("F00385", 0x8007FBD0, "CanApplyEarthDefenseUp"),
    ("F00386", 0x8007FC6C, "CanApplyWaterDefenseUp"),
    ("F00387", 0x8007FD08, "CanApplyBanish"),
    ("F00388", 0x8007FD28, "CanApplyExorcise"),
    ("F00389", 0x8007FD74, "CanApplyDrain"),
    ("F00390", 0x8007FDC0, "IgnoreAttackPrerequisite"),
    ("F00391", 0x8007FDC8, "IgnoreAttackPrerequisite"),
    ("F00392", 0x8007FDD0, "IgnoreAttackPrerequisite"),
)
NATIVE_HELPER_HEADERS |= {
    f"Battle_{name}Slot": r"Native\Game\src\Battle\AttackPrerequisites.h"
    for _, _, name in BATTLE_ATTACK_PREREQUISITE_SLOTS
}

POINTER_FREE_TYPES = {
    "T00629": "vs_action_t: two inline 32-bit hit words, no pointers; main.h:74-107",
}

AI_FRAME_FIELDS = {
    0x37C: "actors",
    0x3BC: "heightMap",
    0x3C0: "tileFlags",
    0x3C4: "terrainRegions",
    0x3C8: "movementResult",
    0x3D0: "directionCache",
    0x3DC: "roomBounds",
    0x3EC: "directions",
    0x3F4: "roomData",
    0x3F8: "randomState",
    0x3FC: "lastBudgetTick",
}

MODULE_DATA_BLOBS = (
    ("EFFECT/PLG074.BIN", 0x577C, 0x800FEF7C, 4,
     "Content_plg074TrailEmissionCounter",
     "PLG074 trailEffect.h origin cursor; original image matches PLG073 but mutable state remains overlay-local"),
    ("BATTLE/BATTLE.PRG", 0x81268, 0x800E9A68, 72, "Content_battleShadowVertices",
     "4B16C.s:1208 eighteen packed signed16 X/Z vertices; imported source data only"),
    ("BATTLE/BATTLE.PRG", 0x812D4, 0x800E9AD4, 96, "Content_battleShadowFaces",
     "4B16C.s:1208 twenty-four triangles with byte offsets into projected shadow vertices"),
    ("BATTLE/BATTLE.PRG", 0x88F58, 0x800F1758, 4, "Content_battleAiHesitationLengths",
     "7B5DC.s:func_800E4180 indexed hesitation lengths; native module-owned data"),
    ("BATTLE/BATTLE.PRG", 0x83278, 0x800EBA78, 320, "Content_battleMainMenuHelpText",
     "5BF94.c:2934 D_800EBA78 main menu help text; bytes 316..319 open the gradient window"),
    ("BATTLE/BATTLE.PRG", 0x1060, 0x80069860, 0x98, "Content_battleLimbNameBytes",
     "58578.c INCLUDE_RODATA D_80069860; strings end at jtbl_800698F8"),
    ("BATTLE/BATTLE.PRG", 0x80B0C, 0x800E930C, 0x71C, "Content_battleGimmickTemplateBytes",
     "30DB0.c:_loadGimmick; three 16-byte headers plus modelDataSize payloads"),
    ("BATTLE/BATTLE.PRG", 0x823AC, 0x800EABAC, 0x9DC, "Content_battleTextboxFrameBytes",
     "5BF94.c:textbox frame vertices; nine styles precede D_800EB588"),
    ("BATTLE/BATTLE.PRG", 0x7FD48, 0x800E8548, 116, "Content_battleTriggerMarkerBytes",
     "146C.c:func_8008E19C complete one-group/one-quad marker definition; G00039-G00041 are interior aliases"),
    ("BATTLE/BATTLE.PRG", 0x41C, 0x80068C1C, 664, "Content_battleSkyDome",
     "146C.c:17537 D_80068C1C sky dome: 34 twelve-byte vertices (colour word 2) and 64 four-byte faces; "
     "SetRoomBackgroundColors patches the vertex colours"),
    ("SLUS_010.40", 0x800, 0x80010000, 0x52000, "Content_residentLfoSamples",
     "25AC.c:LFO selectors include source-code bytes as sample/control data; original PS-X EXE payload, never host code"),
)

MODULE_DATA_BLOB_TYPES = {
    "Content_residentLfoSamples": ("short", 2),
    "Content_plg074TrailEmissionCounter": ("int", 4),
}
MODULE_SAMPLE_POINTERS = {"G01840": "Content_residentLfoSamples"}

# Module-image function addresses held by a promoted function's C binding rather than a catalog name (#46): the
# image still stores the original address, so the relocated word must resolve to the binding (plan section 8.2).
MODULE_BINDING_ENTRIES = (
    ("BATTLE/BATTLE.PRG", 0x8009998C, "Field_CreateGimmickInstanceEntry"),
    ("BATTLE/BATTLE.PRG", 0x80099D6C, "Field_ReleaseGimmickInstanceEntry"),
    ("BATTLE/BATTLE.PRG", 0x80099900, "Field_FindSharedGimmickModelEntry"),
    ("BATTLE/BATTLE.PRG", 0x80099960, "Field_SetGimmickPaletteTableEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008E2D4, "Field_MovingPlatformTargetWaypointEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008E4DC, "Field_SetPlatformFreezeAppearanceEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008CEB0, "Field_DrawMovingPlatformShadowsEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008C8A8, "Field_UpdateMovingPlatformsEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008D2C0, "Field_CollectMovingPlatformPositionsEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008C538, "Field_InitializeMovingPlatformsEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008C6B4, "Field_ResetMovingPlatformWaypointEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008C49C, "Field_SetMovingPlatformActorEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008D594, "Field_SetMovingPlatformsPausedEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008D5A0, "Field_FreezeMovingPlatformsEntry"),
    ("BATTLE/BATTLE.PRG", 0x8006CE50, "Field_FreezeRoomGimmicksTemporarilyEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008E224, "Field_SetMovingPlatformVisibilityEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008E320, "Field_MovingPlatformDwellTicksEntry"),
    ("BATTLE/BATTLE.PRG", 0x8006CE70, "Field_ApplyUnlockEffectToRoomObjectEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008BD74, "Field_ToggleDoorCollisionEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008BEBC, "Field_BeginDoorToggleEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008BF48, "Field_StartDoorAnimationEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008C1C8, "Field_FinishDoorAnimationEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008BC04, "Field_InteractiveDoorAtTileEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008E6DC, "Field_MarkSigilDoorUnlockedEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008E7BC, "Field_IsRoomSigilDoorUnlockedEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008D880, "Field_SceneDoorIdEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008D948, "Field_MarkEntranceDoorOnSceneMapEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008AB68, "Field_StoreEntranceDoorForReloadEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008B8F8, "Field_SetEntranceDoorRecordEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008B960, "Field_FindEntranceDoorNearTileEntry"),
    ("BATTLE/BATTLE.PRG", 0x80074BAC, "Field_CheckAshleyTileTriggersEntry"),
    ("BATTLE/BATTLE.PRG", 0x800751B8, "Field_CheckActorTrapAndFallTriggersEntry"),
    ("BATTLE/BATTLE.PRG", 0x80074860, "Field_TryTriggerFacingTileEventEntry"),
    ("BATTLE/BATTLE.PRG", 0x800748B8, "Field_FindFacingRoomObjectEntry"),
    ("BATTLE/BATTLE.PRG", 0x80074950, "Field_FindFacingUnlootedChestEntry"),
    ("BATTLE/BATTLE.PRG", 0x80074798, "Field_TileInFrontOfActorEntry"),
    ("BATTLE/BATTLE.PRG", 0x80074120, "Field_WaitForChestCameraFocusEntry"),
    ("BATTLE/BATTLE.PRG", 0x8007418C, "Field_StepTrapCameraFocusEntry"),
    ("BATTLE/BATTLE.PRG", 0x8007424C, "Field_IsChestAnimationReadyEntry"),
    ("BATTLE/BATTLE.PRG", 0x80074374, "Field_TrackInteractionCameraToAshleyEntry"),
    ("BATTLE/BATTLE.PRG", 0x80073D30, "Field_TriggerActorTrapEntry"),
    ("BATTLE/BATTLE.PRG", 0x80073AFC, "Field_InteractWithLockedRoomObjectEntry"),
    ("BATTLE/BATTLE.PRG", 0x80073AA4, "Field_BeginObjectInteractionWaitEntry"),
    ("BATTLE/BATTLE.PRG", 0x80073ACC, "Field_EndObjectInteractionWaitEntry"),
    ("BATTLE/BATTLE.PRG", 0x800741D4, "Field_DispatchTrapOrChestInteractionEntry"),
    ("BATTLE/BATTLE.PRG", 0x80073F7C, "Field_BeginTrapInteractionEntry"),
    ("BATTLE/BATTLE.PRG", 0x80074314, "Field_FinishTrapInteractionEntry"),
    ("BATTLE/BATTLE.PRG", 0x80074050, "Field_BeginChestInteractionEntry"),
    ("BATTLE/BATTLE.PRG", 0x800742A4, "Field_FinishChestInteractionEntry"),
    ("BATTLE/BATTLE.PRG", 0x800743E0, "Field_OpenChestContentsEntry"),
    ("BATTLE/BATTLE.PRG", 0x800744B8, "Field_UpdateChestContentsSelectionEntry"),
    ("BATTLE/BATTLE.PRG", 0x80073E30, "Field_TryOpenChestEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008D5FC, "Field_BeginSwitchToggleEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008B6B4, "Field_ResetGimmickAnimationQueueEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008C0C0, "Field_UpdateGimmickAnimationQueueEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008C070, "Field_QueueGimmickAnimationEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008D658, "Field_ToggleRoomSwitchEntry"),
    ("BATTLE/BATTLE.PRG", 0x800A0024, "Field_GimmickPlacementEntry"),
    ("BATTLE/BATTLE.PRG", 0x8009F794, "Field_FindGimmickBelowEntry"),
    ("BATTLE/BATTLE.PRG", 0x8009F858, "Field_GimmickSubtypeEntry"),
    ("BATTLE/BATTLE.PRG", 0x800A47C4, "Field_AreGimmicksMovingEntry"),
    ("MENU/MAINMENU.PRG", 0x800FA188, "Field_MeasureTileStackEntry"),
    ("MENU/MAINMENU.PRG", 0x800FA238, "Field_IsGimmickWithinHeightBandEntry"),
    ("BATTLE/BATTLE.PRG", 0x800A0104, "Field_SetGimmickOrientationEntry"),
    ("BATTLE/BATTLE.PRG", 0x800A3DB4, "Field_CanPlaceGimmickAtPointEntry"),
    ("BATTLE/BATTLE.PRG", 0x8009F530, "Field_DropUnsupportedGimmickStackEntry"),
    ("BATTLE/BATTLE.PRG", 0x800A38E0, "Field_FindInteractableGimmickAheadEntry"),
    ("BATTLE/BATTLE.PRG", 0x800A3E6C, "Field_BeginGimmickInteractionEntry"),
    ("BATTLE/BATTLE.PRG", 0x800A41D0, "Field_ApplyGimmickInteractionCommandEntry"),
    ("BATTLE/BATTLE.PRG", 0x800B101C, "Field_UpdateGimmickReactionAnimationEntry"),
    ("BATTLE/BATTLE.PRG", 0x800B0908, "Field_UpdateGimmickMotionEntry"),
    ("BATTLE/BATTLE.PRG", 0x8006CDD8, "Field_DisableTrapAtPositionEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008A4FC, "Field_CurrentSavePointIdEntry"),
    ("BATTLE/BATTLE.PRG", 0x800D85D8, "Battle_Ai_RefreshActionCandidateReadinessEntry"),
    ("BATTLE/BATTLE.PRG", 0x800DC888, "Battle_Ai_ApplySimulatedActionResultsEntry"),
    ("BATTLE/BATTLE.PRG", 0x800DCAA0, "Battle_Ai_SimulateActionCandidateEntry"),
    ("BATTLE/BATTLE.PRG", 0x800DCBD8, "Battle_Ai_UpdateCandidateHitWeightEntry"),
    ("BATTLE/BATTLE.PRG", 0x800DCC94, "Battle_Ai_EvaluateActionCandidateEntry"),
    ("BATTLE/BATTLE.PRG", 0x800DD604, "Battle_Ai_SelectFallbackActionEntry"),
    ("BATTLE/BATTLE.PRG", 0x800DD918, "Battle_Ai_EnumerateActionCandidatesEntry"),
    ("BATTLE/BATTLE.PRG", 0x800DE030, "Battle_Ai_AdvancePlanningSimulationEntry"),
    ("BATTLE/BATTLE.PRG", 0x800DE3E4, "Battle_Ai_BuildActorActionPlanEntry"),
    ("BATTLE/BATTLE.PRG", 0x800DEB10, "Battle_Ai_ReconcileCompletedActionPlanEntry"),
    ("BATTLE/BATTLE.PRG", 0x800E4CE8, "VS::Battle::Ai::ForgetLiveAiActionTarget"),  # F01793
    ("BATTLE/BATTLE.PRG", 0x800E5710, "VS::Battle::Ai::BuildLiveAiActionMetadata"),  # F01810
    ("BATTLE/BATTLE.PRG", 0x800E5998, "VS::Battle::Ai::ResolveLiveAiLinkedActors"),  # F01811
    ("BATTLE/BATTLE.PRG", 0x800E5A74, "VS::Battle::Ai::ApplyLiveSavedAiSettingsFlags"),  # F01812
    ("BATTLE/BATTLE.PRG", 0x800E5A9C, "VS::Battle::Ai::ApplyLiveActorAiSettings"),  # F01813
    ("BATTLE/BATTLE.PRG", 0x800E6178, "VS::Battle::Ai::CreateLiveActorAiState"),  # F01818
    ("BATTLE/BATTLE.PRG", 0x8009E634, "Field_RefreshAshleyInteractionTileEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008D438, "Field_RoomTriggerAtTileEntry"),
    ("BATTLE/BATTLE.PRG", 0x8008DEAC, "Field_SetRoomTriggerEnabledEntry"),
    ("BATTLE/BATTLE.PRG", 0x800A91DC, "Field_FindTopGimmickAtTileEntry"),
    ("BATTLE/BATTLE.PRG", 0x800BFEBC, "Event_EvaluateInterpolationCurveEntry"),
    ("BATTLE/BATTLE.PRG", 0x800BEC58, "Event_EvaluateRoomEventTriggersEntry"),
    ("BATTLE/BATTLE.PRG", 0x800BFE50, "VS::Event::ResolveLiveScriptActorReference"),
    ("SLUS_010.40", 0x80042364, "VS::Boot::ResetToTitleLive"),  # F03423
    ("SLUS_010.40", 0x80042420, "VS::Boot::EnterBattleLive"),  # F03424
    ("SLUS_010.40", 0x800424E4, "VS::Boot::RunEndingAndReturnToTitleLive"),  # F03425
    ("SLUS_010.40", 0x80042C74, "VS::Boot::WaitForOverlayReadyLive"),  # F03440
    ("SLUS_010.40", 0x80042C94, "VS::Boot::SetSoftResetEnabledLive"),  # F03441
    ("BATTLE/BATTLE.PRG", 0x800B9DB4, "Event_ScriptSetAshleyBattleModeOpcode"),  # F01130 opcode table
    *(('BATTLE/BATTLE.PRG', address, f"Event_{name}Opcode") for _, address, name in EVENT_SCRIPT_CONTROL_OPCODES),
    # #276 row 34 opcode table slots
    *(("BATTLE/BATTLE.PRG", address, f"Event_{name}Opcode") for _, address, name in EVENT_SCRIPT_ACTOR_OPCODES),
    # #276 row 36 opcode table slots
    *(("BATTLE/BATTLE.PRG", address, f"Event_{name}Opcode") for _, address, name in EVENT_SCRIPT_MEDIA_OPCODES),
    # #276 row 31 attack prerequisite table slots
    *(("BATTLE/BATTLE.PRG", address, f"Battle_{name}Slot") for _, address, name in BATTLE_ATTACK_PREREQUISITE_SLOTS),
    ("BATTLE/BATTLE.PRG", 0x800E6BA0, "VS::Field::IsLivePointOutsideSolidVolumes"),  # F01833
    ("BATTLE/BATTLE.PRG", 0x800E6C34, "VS::Field::TraceLiveActorAndTerrainCollision"),  # F01834
    ("BATTLE/BATTLE.PRG", 0x800E65DC, "VS::Battle::Ai::CacheLiveActorAiWorkingState"),  # F01819
    ("BATTLE/BATTLE.PRG", 0x800E6828, "VS::Battle::Ai::InitializeLiveActorLocomotionMode"),  # F01824
    ("BATTLE/BATTLE.PRG", 0x800E685C, "VS::Battle::Ai::SetLiveActorVerticalMovementSteps"),  # F01825
    ("BATTLE/BATTLE.PRG", 0x800DAA6C, "VS::Battle::Ai::UpdateLivePatrolCombatBehavior"),  # F01655
    ("BATTLE/BATTLE.PRG", 0x800DB7B4, "VS::Battle::Ai::UpdateLiveActorLocomotionMode"),  # F01669
    ("BATTLE/BATTLE.PRG", 0x800DB820, "VS::Battle::Ai::DispatchLiveMovementBehavior"),  # F01670
    ("BATTLE/BATTLE.PRG", 0x800DBA4C, "VS::Battle::Ai::UpdateLiveFleeOrWanderBehavior"),  # F04203
    ("BATTLE/BATTLE.PRG", 0x800DBA88, "VS::Battle::Ai::UpdateLiveAdaptivePursuitBehavior"),  # F04204
    ("BATTLE/BATTLE.PRG", 0x800DBB98, "VS::Battle::Ai::UpdateLiveSupportFollowBehavior"),  # F04205
    ("BATTLE/BATTLE.PRG", 0x800DBC0C, "VS::Battle::Ai::UpdateLiveEngagementApproachBehavior"),  # F04206
    ("BATTLE/BATTLE.PRG", 0x800E6EB0, "VS::Battle::Ai::ReleaseLiveActorAiContactOccupancy"),  # F01836
    ("BATTLE/BATTLE.PRG", 0x800D954C, "VS::Battle::Ai::LiveDestinationOccupancy"),  # F01645
    ("BATTLE/BATTLE.PRG", 0x800D96C8, "VS::Battle::Ai::CanTraverseLiveTileEdge"),  # F01646
    ("BATTLE/BATTLE.PRG", 0x800D98E8, "VS::Battle::Ai::LiveDirectionalBlockingActor"),  # F01647
    ("BATTLE/BATTLE.PRG", 0x800D9B6C, "VS::Battle::Ai::CanLiveActorWalkInDirection"),  # F04202
    ("BATTLE/BATTLE.PRG", 0x800DB93C, "VS::Battle::Ai::FindLiveGapLandingTile"),  # F01672
    ("BATTLE/BATTLE.PRG", 0x800DC638, "VS::Battle::Ai::UpdateLiveRouteGateAvailability"),  # F01694
    ("BATTLE/BATTLE.PRG", 0x800E3600, "VS::Battle::Ai::ResolveLiveDirectionalMovement"),  # F01752
    ("BATTLE/BATTLE.PRG", 0x800E3BC8, "VS::Battle::Ai::SetLiveDetourBoundary"),  # F01753
    ("BATTLE/BATTLE.PRG", 0x800E3CDC, "VS::Battle::Ai::RefreshLiveDetourProgress"),  # F01754
    ("BATTLE/BATTLE.PRG", 0x800E3D5C, "VS::Battle::Ai::LiveCardinalPassabilityMask"),  # F01755
    ("BATTLE/BATTLE.PRG", 0x800E3DDC, "VS::Battle::Ai::InitializeLiveObstacleDetour"),  # F01756
    ("BATTLE/BATTLE.PRG", 0x800E4180, "VS::Battle::Ai::UpdateLiveHesitationTurn"),  # F01757
    ("BATTLE/BATTLE.PRG", 0x800E4690, "VS::Battle::Ai::IsLiveAdjacentTileStepBlocked"),  # F01775
    ("BATTLE/BATTLE.PRG", 0x800E4764, "VS::Battle::Ai::SelectLiveActorVerticalMovementStep"),  # F01776
    ("BATTLE/BATTLE.PRG", 0x800E47E4, "VS::Battle::Ai::IsTileBehindLiveActorTarget"),  # F01777
    ("BATTLE/BATTLE.PRG", 0x800E49D8, "VS::Battle::Ai::FindLiveAiDescentTile"),  # F01781
    ("BATTLE/BATTLE.PRG", 0x800E4B10, "VS::Battle::Ai::BeginLiveAiTerrainMovement"),  # F01782
    ("BATTLE/BATTLE.PRG", 0x800E4B18, "VS::Battle::Ai::BeginLiveAiAlternateTerrainMovement"),  # F01783
    ("BATTLE/BATTLE.PRG", 0x800E4B2C, "VS::Battle::Ai::MoveLiveActorTowardDestinationTile"),  # F01784
    ("BATTLE/BATTLE.PRG", 0x800E4FE0, "VS::Battle::Ai::BeginLiveAiMovementTowardPoint"),  # F01798
    ("BATTLE/BATTLE.PRG", 0x800E527C, "VS::Battle::Ai::SetLiveActorNavigationOverride"),  # F01805
    ("BATTLE/BATTLE.PRG", 0x800E5308, "VS::Battle::Ai::UpdateLiveActorNavigationOverrides"),  # F01806
    ("BATTLE/BATTLE.PRG", 0x800E6898, "VS::Battle::Ai::AddLiveAiTileOccupant"),  # F01826
    ("BATTLE/BATTLE.PRG", 0x800E68A0, "VS::Battle::Ai::RemoveLiveAiTileOccupant"),  # F01827
    ("BATTLE/BATTLE.PRG", 0x800E68EC, "VS::Battle::Ai::MarkLiveAiTileDynamicObstacle"),  # F01828
    ("BATTLE/BATTLE.PRG", 0x800E6B4C, "VS::Battle::Ai::BuildLiveAiTerrainHeightsWithWorkspace"),  # F01832
    ("BATTLE/BATTLE.PRG", 0x800E5EC0, "VS::Battle::Ai::FloodFillLiveAiTerrainHeights"),  # F01814
    ("BATTLE/BATTLE.PRG", 0x800E5FDC, "VS::Battle::Ai::FindLiveAdjacentAiGroundHeight"),  # F01815
    ("BATTLE/BATTLE.PRG", 0x800E6098, "VS::Battle::Ai::BuildLiveAiSpecialTerrainHeights"),  # F01816
    ("BATTLE/BATTLE.PRG", 0x800E6158, "VS::Battle::Ai::InitializeLiveRoomNavigation"),  # F01817
    ("BATTLE/BATTLE.PRG", 0x800DF9A8, "VS::Battle::Ai::ResolveLiveRouteGateExit"),  # F01721
    ("BATTLE/BATTLE.PRG", 0x800DFA54, "VS::Battle::Ai::FindLiveNearestRouteGateEntry"),  # F01722
    ("BATTLE/BATTLE.PRG", 0x800DFAF8, "VS::Battle::Ai::FindLiveRouteGateAtTile"),  # F01723
    ("BATTLE/BATTLE.PRG", 0x800DFBCC, "VS::Battle::Ai::ResolveLiveNavigationAreaRoute"),  # F01728
    ("BATTLE/BATTLE.PRG", 0x800DD7CC, "VS::Battle::Ai::FindLiveNearestActiveWaypoint"),  # F01710
    ("BATTLE/BATTLE.PRG", 0x800E23AC, "VS::Battle::Ai::UpdateLiveWaypointPatrol"),  # F01747
    ("BATTLE/BATTLE.PRG", 0x800E19FC, "VS::Battle::Ai::UpdateLiveNavigationAreaMovement"),  # F01740
    ("BATTLE/BATTLE.PRG", 0x800E1850, "VS::Battle::Ai::FilterLiveDirectionsByGroundHeight"),  # F01738
    ("BATTLE/BATTLE.PRG", 0x800E1908, "VS::Battle::Ai::MoveLiveWithHeightFilteredDirections"),  # F01739
    ("BATTLE/BATTLE.PRG", 0x800E1BB8, "VS::Battle::Ai::ChooseNextLiveWanderTile"),  # F01742
    ("BATTLE/BATTLE.PRG", 0x800E1EAC, "VS::Battle::Ai::UpdateLiveWanderMovement"),  # F01745
    ("BATTLE/BATTLE.PRG", 0x800E1388, "VS::Battle::Ai::ProjectLiveClearRadialDestination"),  # F01735
    ("BATTLE/BATTLE.PRG", 0x800E153C, "VS::Battle::Ai::MoveLiveToRadialDestination"),  # F01736
    ("BATTLE/BATTLE.PRG", 0x800E1764, "VS::Battle::Ai::MoveLiveAwayFromReference"),  # F01737
    ("BATTLE/BATTLE.PRG", 0x800E2CCC, "VS::Battle::Ai::RecoverLiveMovement"),  # F01750
    ("BATTLE/BATTLE.PRG", 0x800D8400, "VS::Battle::Ai::FindLiveAdjacentMovementDestination"),  # F01636
    ("BATTLE/BATTLE.PRG", 0x800D9538, "VS::Battle::Ai::SaveLiveMovementRandomState"),  # F01644
    ("BATTLE/BATTLE.PRG", 0x800DB0BC, "VS::Battle::Ai::TryRecoverLiveBlockedMovement"),  # F01660
    ("BATTLE/BATTLE.PRG", 0x800E02B4, "VS::Battle::Ai::IsLiveFlightDirectionObstructed"),  # F01729
    ("BATTLE/BATTLE.PRG", 0x800E0678, "VS::Battle::Ai::CanLiveActorFlyInDirection"),  # F01730
    ("BATTLE/BATTLE.PRG", 0x800E0918, "VS::Battle::Ai::UpdateLiveFlightGroundTransition"),  # F01732
    ("BATTLE/BATTLE.PRG", 0x800E0A68, "VS::Battle::Ai::UpdateLiveFlightMovement"),  # F01733
    ("BATTLE/BATTLE.PRG", 0x800E1238, "VS::Battle::Ai::UpdateLiveFlightDirectionalMovement"),  # F01734
    ("BATTLE/BATTLE.PRG", 0x800D8260, "VS::Battle::Ai::SetLiveTemporaryAwarenessIndicator"),  # F01630
    ("BATTLE/BATTLE.PRG", 0x800E2F5C, "VS::Battle::Ai::FollowLiveObstacleDetour"),  # F01751
    ("BATTLE/BATTLE.PRG", 0x800E24EC, "VS::Battle::Ai::UpdateLiveTargetApproachMovement"),  # F01748
    ("BATTLE/BATTLE.PRG", 0x800E2B2C, "VS::Battle::Ai::UpdateLiveTargetFacing"),  # F01749
    ("BATTLE/BATTLE.PRG", 0x800DBF00, "VS::Battle::Ai::UnlinkLiveEngagedActor"),  # F01678
    ("BATTLE/BATTLE.PRG", 0x800DBFE4, "VS::Battle::Ai::UpdateLiveEngagementBuildup"),  # F01679
    ("BATTLE/BATTLE.PRG", 0x800DC0D0, "VS::Battle::Ai::AnimateLiveEngagementPhase"),  # F01680
    ("BATTLE/BATTLE.PRG", 0x800DD000, "VS::Battle::Ai::ApplyLiveActorActionReaction"),  # F01705
    ("BATTLE/BATTLE.PRG", 0x800DD344, "VS::Battle::Ai::PropagateLiveActorRelationships"),  # F01706
    ("BATTLE/BATTLE.PRG", 0x800DEEA4, "VS::Battle::Ai::NotifyLiveActionRelationshipChanges"),  # F01719
    ("BATTLE/BATTLE.PRG", 0x800E7698, "VS::Battle::Ai::UpdateLiveBossRadialRetreat"),  # F01846
    ("BATTLE/BATTLE.PRG", 0x800E78F4, "VS::Battle::Ai::IsLiveBossActionReady"),  # F01847
    ("BATTLE/BATTLE.PRG", 0x800E7960, "VS::Battle::Ai::UpdateLiveBossOrbitTracking"),  # F01848
    ("BATTLE/BATTLE.PRG", 0x800E7F8C, "VS::Battle::Ai::UpdateLiveOrbitingBossBehavior"),  # F01849
    ("BATTLE/BATTLE.PRG", 0x800D87E8, "VS::Battle::Ai::UpdateLiveTargetPerception"),  # F01640
    ("BATTLE/BATTLE.PRG", 0x800D8EF4, "VS::Battle::Ai::UpdateLiveActorPerception"),  # F01643
    ("BATTLE/BATTLE.PRG", 0x800D9E18, "VS::Battle::Ai::MoveLiveForActionCandidate"),  # F01649
    ("BATTLE/BATTLE.PRG", 0x800DA1D4, "VS::Battle::Ai::UpdateLiveCombatPursuit"),  # F01650
    ("BATTLE/BATTLE.PRG", 0x800DA4BC, "VS::Battle::Ai::TryApproachLivePlannedTarget"),  # F01651
    ("BATTLE/BATTLE.PRG", 0x800DA5CC, "VS::Battle::Ai::UpdateLiveCloseFollowBehavior"),  # F01652
    ("BATTLE/BATTLE.PRG", 0x800DA6A4, "VS::Battle::Ai::UpdateLiveEnemyPursuitBehavior"),  # F01653
    ("BATTLE/BATTLE.PRG", 0x800DA738, "VS::Battle::Ai::UpdateLiveGuardMovementBehavior"),  # F01654
    ("BATTLE/BATTLE.PRG", 0x800DAAD8, "VS::Battle::Ai::UpdateLiveEscortCombatBehavior"),  # F01656
    ("BATTLE/BATTLE.PRG", 0x800DAC80, "VS::Battle::Ai::TryExecuteLiveActionCandidate"),  # F01657
    ("BATTLE/BATTLE.PRG", 0x800DAD9C, "VS::Battle::Ai::TryExecuteLivePlannedActions"),  # F01658
    ("BATTLE/BATTLE.PRG", 0x800DAED0, "VS::Battle::Ai::UpdateLiveActorDecisions"),  # F01659
    ("BATTLE/BATTLE.PRG", 0x80081020, "VS::Battle::QueueLiveApplicableStatusChanges"),  # F00405
    ("BATTLE/BATTLE.PRG", 0x800810CC, "VS::Battle::QueueLiveStatusRemovalMask"),  # F00406
    ("BATTLE/BATTLE.PRG", 0x80083998, "VS::Battle::QueueLiveStrengthDown"),  # F00437
    ("BATTLE/BATTLE.PRG", 0x800839E8, "VS::Battle::QueueLiveIntelligenceDown"),  # F00438
    ("BATTLE/BATTLE.PRG", 0x80083A38, "VS::Battle::QueueLiveAgilityDown"),  # F00439
    ("BATTLE/BATTLE.PRG", 0x80083A88, "VS::Battle::QueueLiveEquipmentDown"),  # F00440
    ("BATTLE/BATTLE.PRG", 0x80083AD8, "VS::Battle::QueueLiveHalfMovement"),  # F00441
    ("BATTLE/BATTLE.PRG", 0x80083B28, "VS::Battle::QueueLiveStrengthUp"),  # F00442
    ("BATTLE/BATTLE.PRG", 0x80083B78, "VS::Battle::QueueLiveIntelligenceUp"),  # F00443
    ("BATTLE/BATTLE.PRG", 0x80083BC8, "VS::Battle::QueueLiveAgilityUp"),  # F00444
    ("BATTLE/BATTLE.PRG", 0x80083C18, "VS::Battle::QueueLiveEquipmentUp"),  # F00445
    ("BATTLE/BATTLE.PRG", 0x80083C68, "VS::Battle::QueueLiveQuicken"),  # F00446
    ("BATTLE/BATTLE.PRG", 0x80083CB8, "VS::Battle::QueueLiveSilence"),  # F00447
    ("BATTLE/BATTLE.PRG", 0x80083D08, "VS::Battle::QueueLiveMagicWard"),  # F00448
    ("BATTLE/BATTLE.PRG", 0x80083D2C, "VS::Battle::QueueLiveRegeneration"),  # F00449
    ("BATTLE/BATTLE.PRG", 0x80083D7C, "VS::Battle::QueueLiveBanish"),  # F00450
    ("BATTLE/BATTLE.PRG", 0x80083DEC, "VS::Battle::QueueLiveParalysis"),  # F00451
    ("BATTLE/BATTLE.PRG", 0x80083E3C, "VS::Battle::QueueLivePoison"),  # F00452
    ("BATTLE/BATTLE.PRG", 0x80083E8C, "VS::Battle::QueueLiveNumbness"),  # F00453
    ("BATTLE/BATTLE.PRG", 0x80083EDC, "VS::Battle::QueueLiveCurse"),  # F00454
    ("BATTLE/BATTLE.PRG", 0x80083F2C, "VS::Battle::QueueLiveAirAttackUp"),  # F00455
    ("BATTLE/BATTLE.PRG", 0x80083F50, "VS::Battle::QueueLiveFireAttackUp"),  # F00456
    ("BATTLE/BATTLE.PRG", 0x80083F74, "VS::Battle::QueueLiveEarthAttackUp"),  # F00457
    ("BATTLE/BATTLE.PRG", 0x80083F98, "VS::Battle::QueueLiveWaterAttackUp"),  # F00458
    ("BATTLE/BATTLE.PRG", 0x80083FBC, "VS::Battle::QueueLiveAirDefenseUp"),  # F00459
    ("BATTLE/BATTLE.PRG", 0x80083FE0, "VS::Battle::QueueLiveFireDefenseUp"),  # F00460
    ("BATTLE/BATTLE.PRG", 0x80084004, "VS::Battle::QueueLiveEarthDefenseUp"),  # F00461
    ("BATTLE/BATTLE.PRG", 0x80084028, "VS::Battle::QueueLiveWaterDefenseUp"),  # F00462
    ("BATTLE/BATTLE.PRG", 0x8008404C, "VS::Battle::QueueLiveParalysisCure"),  # F00463
    ("BATTLE/BATTLE.PRG", 0x8008409C, "VS::Battle::QueueLivePoisonCure"),  # F00464
    ("BATTLE/BATTLE.PRG", 0x800840EC, "VS::Battle::QueueLiveCurseCure"),  # F00465
    ("BATTLE/BATTLE.PRG", 0x8008413C, "VS::Battle::QueueLiveNumbnessCure"),  # F00466
    ("BATTLE/BATTLE.PRG", 0x8008418C, "VS::Battle::QueueLiveDispel"),  # F00467
    ("BATTLE/BATTLE.PRG", 0x800841C8, "VS::Battle::QueueLiveNegativeStatusCure"),  # F00468
    ("BATTLE/BATTLE.PRG", 0x80086EA4, "VS::Battle::ApplyLiveWeaponElementEnchantment"),  # F00518
    ("BATTLE/BATTLE.PRG", 0x80086EF4, "VS::Battle::ApplyLiveArmorElementEnchantment"),  # F00519
    ("BATTLE/BATTLE.PRG", 0x80087578, "VS::Battle::QueueLiveActorStatChangeNumber"),  # F00521
    ("BATTLE/BATTLE.PRG", 0x80088B8C, "VS::Battle::UpdateLiveTargetingRiskPenalty"),  # F00527
    ("BATTLE/BATTLE.PRG", 0x80088CA0, "VS::Battle::ResetLiveTargetingRiskPenaltyTimer"),  # F00528
    ("BATTLE/BATTLE.PRG", 0x8007C31C, "VS::Battle::PreserveLiveAshleyStatuses"),  # F00254
    ("BATTLE/BATTLE.PRG", 0x80086754, "VS::Battle::RemoveLiveActorStatuses"),  # F00517
    ("BATTLE/BATTLE.PRG", 0x80086FA8, "VS::Battle::InflictLiveActorStatuses"),  # F00520
    ("BATTLE/BATTLE.PRG", 0x80087EF4, "VS::Battle::UpdateLiveActorStatusTimers"),  # F00523
    ("BATTLE/BATTLE.PRG", 0x800792E4, "VS::Battle::ResolveLiveActorDefeat"),  # F00194
    ("BATTLE/BATTLE.PRG", 0x800875C8, "VS::Battle::ApplyLiveActorActionResult"),  # F00522
    ("BATTLE/BATTLE.PRG", 0x80088554, "VS::Battle::UpdateLiveActorVitals"),  # F00525
    ("BATTLE/BATTLE.PRG", 0x80088B6C, "VS::Battle::UpdateLiveRoomActorVitals"),  # F00526
    ("BATTLE/BATTLE.PRG", 0x8007E5E0, "VS::Battle::AttackLiveGemHitBonus"),  # F00339
    ("BATTLE/BATTLE.PRG", 0x8007E6A0, "VS::Battle::DefenseLiveGemHitBonus"),  # F00340
    ("BATTLE/BATTLE.PRG", 0x8007E97C, "VS::Battle::PreviousLiveHitSuccessThreshold"),  # F00342
    ("BATTLE/BATTLE.PRG", 0x8007E99C, "VS::Battle::LiveGemHitChance"),  # F00343
    ("BATTLE/BATTLE.PRG", 0x8007F138, "VS::Battle::LiveRiskHitChance"),  # F00347
    ("BATTLE/BATTLE.PRG", 0x8007F1A4, "VS::Battle::LiveChainHitChance"),  # F00348
    ("BATTLE/BATTLE.PRG", 0x8007EA74, "VS::Battle::LiveAgilityHitChance"),  # F00344
    ("BATTLE/BATTLE.PRG", 0x8007ECA8, "VS::Battle::LiveIntelligenceHitChance"),  # F00345
    ("BATTLE/BATTLE.PRG", 0x8007EEFC, "VS::Battle::LiveCostAdjustedAgilityHitChance"),  # F00346
    ("BATTLE/BATTLE.PRG", 0x8007F230, "VS::Battle::LiveRollActionHit"),  # F00349
    ("BATTLE/BATTLE.PRG", 0x800838EC, "VS::Battle::LiveCalculateActionStatChange"),  # F00435
    # #350: retired unused sentinels keep separate, correctly typed relocation guards.
    ("BATTLE/BATTLE.PRG", 0x8007E974, "VS::Battle::RejectLiveHitSelector"),  # retired F00341
    ("BATTLE/BATTLE.PRG", 0x80081130, "VS::Battle::RejectLiveStatSelector"),  # retired F00407
    ("BATTLE/BATTLE.PRG", 0x80083990, "VS::Battle::RejectLiveEffectSelector"),  # retired F00436
    ("BATTLE/BATTLE.PRG", 0x80082144, "VS::Battle::LiveScalePreviousTargetHit"),  # F00412
    ("BATTLE/BATTLE.PRG", 0x800821B0, "VS::Battle::LiveScaleSourceMissingHp"),  # F00413
    ("BATTLE/BATTLE.PRG", 0x80082234, "VS::Battle::LiveScalePreviousTargetHit"),  # F00414
    ("BATTLE/BATTLE.PRG", 0x800822A0, "VS::Battle::LiveScaleTargetCurrentHp"),  # F00415
    ("BATTLE/BATTLE.PRG", 0x8008231C, "VS::Battle::LiveWeaponPpWithChainOffset"),  # F00416
    ("BATTLE/BATTLE.PRG", 0x80082360, "VS::Battle::LiveScaleIncomingSpellDamage"),  # F00417
    ("BATTLE/BATTLE.PRG", 0x800824D8, "VS::Battle::LiveScaleSourceMissingMp"),  # F00419
    ("BATTLE/BATTLE.PRG", 0x80083524, "VS::Battle::LiveScalePreviousTargetHit"),  # F00426
    ("BATTLE/BATTLE.PRG", 0x8008241C, "VS::Battle::LiveScaleIncomingPhysicalDamage"),  # F00418
    ("BATTLE/BATTLE.PRG", 0x800834A4, "VS::Battle::LiveTargetMissingHp"),  # F00424
    ("BATTLE/BATTLE.PRG", 0x800834E4, "VS::Battle::LiveTargetMissingMp"),  # F00425
    ("BATTLE/BATTLE.PRG", 0x80083590, "VS::Battle::LiveScaleIncomingSpellDamage"),  # F00427
    ("BATTLE/BATTLE.PRG", 0x8008364C, "VS::Battle::LiveScaleIncomingPhysicalDamage"),  # F00428
    ("BATTLE/BATTLE.PRG", 0x80083708, "VS::Battle::LiveScaleIncomingSpellCost"),  # F00429
    ("BATTLE/BATTLE.PRG", 0x8008379C, "VS::Battle::LiveScaleIncomingDamage"),  # F00430
    ("BATTLE/BATTLE.PRG", 0x8008381C, "VS::Battle::LiveSourceShieldPp"),  # F00431
    ("BATTLE/BATTLE.PRG", 0x8007FE5C, "VS::Battle::LiveApplyDamageGemMultiplier"),  # F00394
    ("BATTLE/BATTLE.PRG", 0x80080000, "VS::Battle::LiveDistributeHitLimbDamage"),  # F00396
    ("BATTLE/BATTLE.PRG", 0x800801E0, "VS::Battle::LiveDistributeLimbRecovery"),  # F00397
    ("BATTLE/BATTLE.PRG", 0x80081138, "VS::Battle::LiveLastActionStatValue"),  # F00408
    ("BATTLE/BATTLE.PRG", 0x8008384C, "VS::Battle::LiveScaleLastActionStatValue"),  # F00432
    ("BATTLE/BATTLE.PRG", 0x800838BC, "VS::Battle::LiveTargetRisk"),  # F00434
    ("BATTLE/BATTLE.PRG", 0x80081148, "VS::Battle::LiveCalculatePhysicalHitDamage"),  # F00409
    ("BATTLE/BATTLE.PRG", 0x800819D0, "VS::Battle::LivePhysicalDamageWithoutDebuffs"),  # F00410
    ("BATTLE/BATTLE.PRG", 0x800819FC, "VS::Battle::LiveCalculateSpellHitDamage"),  # F00411
    ("BATTLE/BATTLE.PRG", 0x800829B8, "VS::Battle::LiveCalculateSpellHealing"),  # F00421
    ("BATTLE/BATTLE.PRG", 0x8008388C, "VS::Battle::LivePhysicalDamageWithDebuffs"),  # F00433
    # The sound command table (Audio_Mixer_kCommandHandlers, D_800301E0) holds the cutscene stream commands.
    ("SLUS_010.40", 0x80018B34, "VS::Audio::Cutscene::PauseLiveStream"),  # F03257 command 0x9F
    ("SLUS_010.40", 0x80018B84, "VS::Audio::Cutscene::ResumeLiveStream"),  # F03258 command 0x9E
    ("SLUS_010.40", 0x8001D644, "VS::Audio::Cutscene::ReplaceLiveStream"),  # F03392 command 0xE0
    ("SLUS_010.40", 0x8001D698, "VS::Audio::Cutscene::StopLiveStream"),  # F03393 command 0xE2
    ("SLUS_010.40", 0x8001D6B8, "VS::Audio::Cutscene::SetLiveStreamVolume"),  # F03394 command 0xE4
    ("SLUS_010.40", 0x8001D728, "VS::Audio::Cutscene::SlideLiveStreamVolume"),  # F03395 command 0xE5
    ("SLUS_010.40", 0x8001D7A8, "VS::Audio::Cutscene::SetLiveStreamPan"),  # F03396 command 0xE6
    # #276 audio-seq-voices: Audio_Seq_kOpcodeHandlers and kExtendedOpcodeHandlers hold the voice/channel opcodes.
    ("SLUS_010.40", 0x8001B4EC, "VS::Audio::Seq::LiveSetVolumeBalance"),  # F03290
    ("SLUS_010.40", 0x8001B514, "VS::Audio::Seq::LiveSlideVolumeBalance"),  # F03291
    ("SLUS_010.40", 0x8001B598, "VS::Audio::Seq::LiveSetVolume"),  # F03292
    ("SLUS_010.40", 0x8001B5C8, "VS::Audio::Seq::LiveSlideVolume"),  # F03293
    ("SLUS_010.40", 0x8001B654, "VS::Audio::Seq::LiveSetKeyOnVolumeRamp"),  # F03294
    ("SLUS_010.40", 0x8001B6E4, "VS::Audio::Seq::LiveEnableFullNoteLength"),  # F03295
    ("SLUS_010.40", 0x8001B6F8, "VS::Audio::Seq::LiveDisableFullNoteLength"),  # F03296
    ("SLUS_010.40", 0x8001B804, "VS::Audio::Seq::LiveSetPan"),  # F03301
    ("SLUS_010.40", 0x8001B838, "VS::Audio::Seq::LiveSlidePan"),  # F03302
    ("SLUS_010.40", 0x8001B8C4, "VS::Audio::Seq::LiveSetOctave"),  # F03303
    ("SLUS_010.40", 0x8001B8E0, "VS::Audio::Seq::LiveIncreaseOctave"),  # F03304
    ("SLUS_010.40", 0x8001B8F8, "VS::Audio::Seq::LiveDecreaseOctave"),  # F03305
    ("SLUS_010.40", 0x8001B910, "VS::Audio::Seq::LiveLoadInstrument"),  # F03306
    ("SLUS_010.40", 0x8001B9B8, "VS::Audio::Seq::LiveLoadSilentInstrument"),  # F03307
    ("SLUS_010.40", 0x8001BA24, "VS::Audio::Seq::LiveSelectPatch"),  # F03308
    ("SLUS_010.40", 0x8001BAB8, "VS::Audio::Seq::LiveResetEnvelope"),  # F03309
    ("SLUS_010.40", 0x8001BB00, "VS::Audio::Seq::LiveSetTranspose"),  # F03310
    ("SLUS_010.40", 0x8001BB24, "VS::Audio::Seq::LiveAddTranspose"),  # F03311
    ("SLUS_010.40", 0x8001BB50, "VS::Audio::Seq::LiveSetPitchBendSlide"),  # F03312
    ("SLUS_010.40", 0x8001BB98, "VS::Audio::Seq::LiveEnablePortamento"),  # F03313
    ("SLUS_010.40", 0x8001BBD0, "VS::Audio::Seq::LiveDisablePortamento"),  # F03314
    ("SLUS_010.40", 0x8001BBD8, "VS::Audio::Seq::LiveSetFineTune"),  # F03315
    ("SLUS_010.40", 0x8001BC38, "VS::Audio::Seq::LiveAddFineTune"),  # F03316
    ("SLUS_010.40", 0x8001BC9C, "VS::Audio::Seq::LiveEnableVibrato"),  # F03317
    ("SLUS_010.40", 0x8001BD94, "VS::Audio::Seq::LiveSetVibratoDepth"),  # F03318
    ("SLUS_010.40", 0x8001BDF4, "VS::Audio::Seq::LiveSlideVibratoDepth"),  # F03319
    ("SLUS_010.40", 0x8001BE60, "VS::Audio::Seq::LiveDisableVibrato"),  # F03320
    ("SLUS_010.40", 0x8001BE84, "VS::Audio::Seq::LiveEnableTremolo"),  # F03321
    ("SLUS_010.40", 0x8001BF34, "VS::Audio::Seq::LiveSetTremoloDepth"),  # F03322
    ("SLUS_010.40", 0x8001BF58, "VS::Audio::Seq::LiveSlideTremoloDepth"),  # F03323
    ("SLUS_010.40", 0x8001BFC8, "VS::Audio::Seq::LiveDisableTremolo"),  # F03324
    ("SLUS_010.40", 0x8001BFEC, "VS::Audio::Seq::LiveEnableAutoPan"),  # F03325
    ("SLUS_010.40", 0x8001C058, "VS::Audio::Seq::LiveSetAutoPanDepth"),  # F03326
    ("SLUS_010.40", 0x8001C078, "VS::Audio::Seq::LiveSlideAutoPanDepth"),  # F03327
    ("SLUS_010.40", 0x8001C0E4, "VS::Audio::Seq::LiveDisableAutoPan"),  # F03328
    ("SLUS_010.40", 0x8001C108, "VS::Audio::Seq::LiveEnableNoise"),  # F03329
    ("SLUS_010.40", 0x8001C168, "VS::Audio::Seq::LiveDisableNoise"),  # F03330
    ("SLUS_010.40", 0x8001C1D0, "VS::Audio::Seq::LiveEnableFrequencyModulation"),  # F03331
    ("SLUS_010.40", 0x8001C244, "VS::Audio::Seq::LiveDisableFrequencyModulation"),  # F03332
    ("SLUS_010.40", 0x8001C2AC, "VS::Audio::Seq::LiveEnableReverb"),  # F03333
    ("SLUS_010.40", 0x8001C30C, "VS::Audio::Seq::LiveDisableReverb"),  # F03334
    ("SLUS_010.40", 0x8001C36C, "VS::Audio::Seq::LiveEnableLegato"),  # F03335
    ("SLUS_010.40", 0x8001C380, "VS::Audio::Seq::LiveEnableSustainedSoundEffect"),  # F03337
    ("SLUS_010.40", 0x8001C3A4, "VS::Audio::Seq::LiveSetNoiseClock"),  # F03339
    ("SLUS_010.40", 0x8001C440, "VS::Audio::Seq::LiveSetAttackRate"),  # F03340
    ("SLUS_010.40", 0x8001C484, "VS::Audio::Seq::LiveSetDecayRate"),  # F03341
    ("SLUS_010.40", 0x8001C4B8, "VS::Audio::Seq::LiveSetSustainLevel"),  # F03342
    ("SLUS_010.40", 0x8001C4E8, "VS::Audio::Seq::LiveSetSustainRate"),  # F03343
    ("SLUS_010.40", 0x8001C52C, "VS::Audio::Seq::LiveSetReleaseRate"),  # F03344
    ("SLUS_010.40", 0x8001C56C, "VS::Audio::Seq::LiveSetAttackMode"),  # F03345
    ("SLUS_010.40", 0x8001C5B0, "VS::Audio::Seq::LiveSetSustainMode"),  # F03346
    ("SLUS_010.40", 0x8001C628, "VS::Audio::Seq::LiveSetReleaseMode"),  # F03347
    ("SLUS_010.40", 0x8001C8BC, "VS::Audio::Seq::LiveSetNextNoteLength"),  # F03355
    ("SLUS_010.40", 0x8001C8E4, "VS::Audio::Seq::LiveSetFixedNoteLength"),  # F03356
    ("SLUS_010.40", 0x8001C92C, "VS::Audio::Seq::LiveEnableDrumKeymap"),  # F03357
    ("SLUS_010.40", 0x8001C964, "VS::Audio::Seq::LiveMuteInstrument"),  # F03358
    ("SLUS_010.40", 0x8001C9F0, "VS::Audio::Seq::LiveSetDecayAndSustain"),  # F03361
    ("SLUS_010.40", 0x8001CA2C, "VS::Audio::Seq::LiveEnableTimedNoise"),  # F03362
    ("SLUS_010.40", 0x8001CA70, "VS::Audio::Seq::LiveSetNoiseToggleDelay"),  # F03363
    ("SLUS_010.40", 0x8001CAA0, "VS::Audio::Seq::LiveEnableTimedFrequencyModulation"),  # F03364
    ("SLUS_010.40", 0x8001CAE4, "VS::Audio::Seq::LiveSetFrequencyModulationToggleDelay"),  # F03365
    ("SLUS_010.40", 0x8001CB14, "VS::Audio::Seq::LiveResetVoiceEffects"),  # F03366
    ("SLUS_010.40", 0x8001CB7C, "VS::Audio::Seq::LiveEnablePitchSidechain"),  # F03367
    ("SLUS_010.40", 0x8001CB90, "VS::Audio::Seq::LiveDisablePitchSidechain"),  # F03368
    ("SLUS_010.40", 0x8001CBA4, "VS::Audio::Seq::LiveEnableVolumeSidechain"),  # F03369
    ("SLUS_010.40", 0x8001CBB8, "VS::Audio::Seq::LiveDisableVolumeSidechain"),  # F03370
    ("SLUS_010.40", 0x8001CC84, "VS::Audio::Seq::LiveEnableLoopBoundaryStop"),  # F03372
    ("SLUS_010.40", 0x8001CD08, "VS::Audio::Seq::LiveSetRandomPitchDepth"),  # F03376
    ("SLUS_010.40", 0x8001CD24, "VS::Audio::Seq::LiveResetRandomPitchDepth"),  # F03377
    # #276 audio-seq-control: Audio_Mixer_kCommandHandlers holds the music commands; Audio_Seq_kOpcodeHandlers and
    # kExtendedOpcodeHandlers hold the sequencer flow-control opcodes.
    ("SLUS_010.40", 0x80016DF8, "VS::Audio::Seq::LiveStartOrResumeMusic"),  # F03212 command 0x10
    ("SLUS_010.40", 0x80016E74, "VS::Audio::Seq::LiveSaveCurrentMusic"),  # F03213 command 0x40
    ("SLUS_010.40", 0x80016F0C, "VS::Audio::Seq::LiveStartMusicLayer"),  # F03214 command 0x19
    ("SLUS_010.40", 0x80016FF4, "VS::Audio::Seq::LiveCrossfadeMusicLayers"),  # F03215 command 0x1A
    ("SLUS_010.40", 0x800171C8, "VS::Audio::Seq::LiveStartMusicWithLoopCounter"),  # F03216 command 0x12
    ("SLUS_010.40", 0x80017498, "VS::Audio::Seq::LiveSetMusicVolume"),  # F03222 command 0xC0
    ("SLUS_010.40", 0x8001754C, "VS::Audio::Seq::LiveFadeMusicVolume"),  # F03223 command 0xC1
    ("SLUS_010.40", 0x8001766C, "VS::Audio::Seq::LiveFadeMusicVolumeFrom"),  # F03224 command 0xC2
    ("SLUS_010.40", 0x80017764, "VS::Audio::Seq::LiveSetDiscVolume"),  # F03225 command 0xC8
    ("SLUS_010.40", 0x80017798, "VS::Audio::Seq::LiveFadeDiscVolume"),  # F03226 command 0xC9
    ("SLUS_010.40", 0x80017800, "VS::Audio::Seq::LiveFadeDiscVolumeFrom"),  # F03227 command 0xCA
    ("SLUS_010.40", 0x8001B138, "VS::Audio::Seq::LiveFinishChannel"),  # F03280 opcode 0xA0
    ("SLUS_010.40", 0x8001B20C, "VS::Audio::Seq::LiveFinishChannelOnReplacement"),  # F03281 opcode 0xFE1F
    ("SLUS_010.40", 0x8001B248, "VS::Audio::Seq::LiveSetTempo"),  # F03282 opcode 0xFE00
    ("SLUS_010.40", 0x8001B294, "VS::Audio::Seq::LiveSlideTempo"),  # F03283 opcode 0xFE01
    ("SLUS_010.40", 0x8001B334, "VS::Audio::Seq::LiveSetReverbDepth"),  # F03284 opcode 0xFE02
    ("SLUS_010.40", 0x8001B37C, "VS::Audio::Seq::LiveSlideReverbDepth"),  # F03285 opcode 0xFE03
    ("SLUS_010.40", 0x8001B424, "VS::Audio::Seq::LiveJumpRelative"),  # F03286 opcode 0xFE06
    ("SLUS_010.40", 0x8001B450, "VS::Audio::Seq::LiveJumpAtThreshold"),  # F03287 opcode 0xFE07
    ("SLUS_010.40", 0x8001B4A8, "VS::Audio::Seq::LiveCallRelative"),  # F03288 opcode 0xFE0E
    ("SLUS_010.40", 0x8001B4E0, "VS::Audio::Seq::LiveReturnFromCall"),  # F03289 opcode 0xFE0F
    ("SLUS_010.40", 0x8001B70C, "VS::Audio::Seq::LiveSetSequenceControlValue"),  # F03297 opcode 0xFE0C
    ("SLUS_010.40", 0x8001C378, "VS::Audio::Seq::LiveIgnoreInstruction"),  # F03336 opcode 0xCD
    ("SLUS_010.40", 0x8001C39C, "VS::Audio::Seq::LiveIgnoreInstruction"),  # F03338 opcode 0xD1
    ("SLUS_010.40", 0x8001C66C, "VS::Audio::Seq::LiveSetVoiceAllocationStart"),  # F03348 opcode 0xFE10
    ("SLUS_010.40", 0x8001C68C, "VS::Audio::Seq::LiveResetVoiceAllocationStart"),  # F03349 opcode 0xFE11
    ("SLUS_010.40", 0x8001C69C, "VS::Audio::Seq::LivePushLoop"),  # F03350 opcode 0xC8
    ("SLUS_010.40", 0x8001C6EC, "VS::Audio::Seq::LiveRepeatLoop"),  # F03351 opcode 0xC9
    ("SLUS_010.40", 0x8001C780, "VS::Audio::Seq::LiveJumpOnLoopPass"),  # F03352 opcode 0xFE08
    ("SLUS_010.40", 0x8001C7EC, "VS::Audio::Seq::LiveExitLoopOnPass"),  # F03353 opcode 0xFE09
    ("SLUS_010.40", 0x8001C868, "VS::Audio::Seq::LiveRepeatLoopForever"),  # F03354 opcode 0xCA
    ("SLUS_010.40", 0x8001C97C, "VS::Audio::Seq::LiveSetTimerDivisors"),  # F03359 opcode 0xFE15
    ("SLUS_010.40", 0x8001C9B4, "VS::Audio::Seq::LiveSetTimerCount"),  # F03360 opcode 0xFE16
    ("SLUS_010.40", 0x8001CBCC, "VS::Audio::Seq::LivePlayEmbeddedSoundEffect"),  # F03371 opcode 0xFE0B
    ("SLUS_010.40", 0x8001CC98, "VS::Audio::Seq::LiveSelectNoteRemapTable"),  # F03373 opcode 0xFE1C
    ("SLUS_010.40", 0x8001CCC8, "VS::Audio::Seq::LiveMarkChannelKeyed"),  # F03374 opcode 0xFE1D
    ("SLUS_010.40", 0x8001CCE8, "VS::Audio::Seq::LiveClearChannelKeyed"),  # F03375 opcode 0xFE1E
    ("SLUS_010.40", 0x8001CD2C, "VS::Audio::Seq::LiveStopOnInvalidInstruction"),  # F03378 unimplemented opcodes
    # #276 audio-mixer-driver: Audio_Mixer_kCommandHandlers holds the mixer's pitch, tempo, stop, output, pause and
    # ignored commands.
    ("SLUS_010.40", 0x80017FB0, "VS::Audio::Mixer::LiveSetSoundEffectPitchScale"),  # F03236 command 0xA4
    ("SLUS_010.40", 0x80018090, "VS::Audio::Mixer::LiveSlideSoundEffectPitchScale"),  # F03237 command 0xA5
    ("SLUS_010.40", 0x80018218, "VS::Audio::Mixer::LiveSetWorldSoundEffectPitchScale"),  # F03238 command 0xAC
    ("SLUS_010.40", 0x80018270, "VS::Audio::Mixer::LiveSlideWorldSoundEffectPitchScale"),  # F03239 command 0xAD
    ("SLUS_010.40", 0x8001833C, "VS::Audio::Mixer::LiveSetTempoScale"),  # F03240 command 0xD0
    ("SLUS_010.40", 0x80018358, "VS::Audio::Mixer::LiveSlideTempoScale"),  # F03241 command 0xD1
    ("SLUS_010.40", 0x800183C0, "VS::Audio::Mixer::LiveSetTempoScaleRamp"),  # F03242 command 0xD2
    ("SLUS_010.40", 0x80018434, "VS::Audio::Mixer::LiveSetMusicPitchScale"),  # F03243 command 0xD4
    ("SLUS_010.40", 0x80018450, "VS::Audio::Mixer::LiveSlideMusicPitchScale"),  # F03244 command 0xD5
    ("SLUS_010.40", 0x800184B8, "VS::Audio::Mixer::LiveSetMusicPitchScaleRamp"),  # F03245 command 0xD6
    ("SLUS_010.40", 0x8001852C, "VS::Audio::Mixer::LiveStopMusicAndCancelReplacement"),  # F03246 command 0xF0
    ("SLUS_010.40", 0x800185A0, "VS::Audio::Mixer::LiveStopMatchingMusic"),  # F03247 command 0x11
    ("SLUS_010.40", 0x80018608, "VS::Audio::Mixer::LiveStopWorldSoundEffects"),  # F03248 command 0xF1
    ("SLUS_010.40", 0x800186E4, "VS::Audio::Mixer::LiveEnableStereoOutput"),  # F03249 command 0x80
    ("SLUS_010.40", 0x80018744, "VS::Audio::Mixer::LiveEnableMonoOutput"),  # F03250 command 0x81
    ("SLUS_010.40", 0x800187A4, "VS::Audio::Mixer::LiveSetMusicMuteMask"),  # F03251 command 0x90
    ("SLUS_010.40", 0x800187E4, "VS::Audio::Mixer::LiveSetMusicJumpThreshold"),  # F03252 command 0x92
    ("SLUS_010.40", 0x800187F8, "VS::Audio::Mixer::LivePauseMusic"),  # F03253 command 0x9B
    ("SLUS_010.40", 0x800188E8, "VS::Audio::Mixer::LiveResumeMusic"),  # F03254 command 0x9A
    ("SLUS_010.40", 0x80018984, "VS::Audio::Mixer::LivePauseWorldSoundEffects"),  # F03255 command 0x9D
    ("SLUS_010.40", 0x80018AA0, "VS::Audio::Mixer::LiveResumeWorldSoundEffects"),  # F03256 command 0x9C
    ("SLUS_010.40", 0x80018BD8, "VS::Audio::Mixer::LiveIgnoreCommand"),  # F03259 204 unused commands
    # #276 audio-control-sfx: Audio_Mixer_kCommandHandlers holds the sound-effect program commands.
    ("SLUS_010.40", 0x80017208, "VS::Audio::SoundEffects::LivePlayInterfaceProgramPair"),  # F03217 command 0x34
    ("SLUS_010.40", 0x80017254, "VS::Audio::SoundEffects::LivePlayInterfaceProgram"),  # F03218 command 0x30
    ("SLUS_010.40", 0x800172D4, "VS::Audio::SoundEffects::LivePlaySharedProgram"),  # F03219 command 0x20
    ("SLUS_010.40", 0x8001733C, "VS::Audio::SoundEffects::LivePlayBlobPrograms"),  # F03220 command 0x24
    ("SLUS_010.40", 0x8001746C, "VS::Audio::SoundEffects::LiveStopPrograms"),  # F03221 command 0x21
    ("SLUS_010.40", 0x80017870, "VS::Audio::SoundEffects::LiveSetProgramVolume"),  # F03228 command 0xA0
    ("SLUS_010.40", 0x80017958, "VS::Audio::SoundEffects::LiveFadeProgramVolume"),  # F03229 command 0xA1
    ("SLUS_010.40", 0x80017AD8, "VS::Audio::SoundEffects::LiveSetSceneVolume"),  # F03230 command 0xA8
    ("SLUS_010.40", 0x80017B54, "VS::Audio::SoundEffects::LiveFadeSceneVolume"),  # F03231 command 0xA9
    ("SLUS_010.40", 0x80017C1C, "VS::Audio::SoundEffects::LiveSetProgramPan"),  # F03232 command 0xA2
    ("SLUS_010.40", 0x80017CFC, "VS::Audio::SoundEffects::LiveFadeProgramPan"),  # F03233 command 0xA3
    ("SLUS_010.40", 0x80017E74, "VS::Audio::SoundEffects::LiveSetScenePan"),  # F03234 command 0xAA
    ("SLUS_010.40", 0x80017EEC, "VS::Audio::SoundEffects::LiveFadeScenePan"),  # F03235 command 0xAB
    # #276 Core runtime: the original addresses of the empty handlers, copies, random draws and keystream.
    *(("BATTLE/BATTLE.PRG", address, "VS::Core::IgnoreLiveActorCallback")  # F00023, F00055
      for address in (0x8006B20C, 0x8006C478)),
    *(("BATTLE/BATTLE.PRG", address, "VS::Core::NoOperation")  # F00213, F00214, F00241, F00328
      for address in (0x8007B0FC, 0x8007B104, 0x8007BF04, 0x8007DF40)),
    *(("SLUS_010.40", address, "VS::Core::IgnoreCall")  # F03427-F03432, F03485, F03571, F03574
      for address in (0x8004272C, 0x80042734, 0x8004273C, 0x80042744, 0x80044C6C, 0x800490A8, 0x80049130)),
    *(("SLUS_010.40", address, "VS::Core::ReturnZero")  # F03503, F03509, F03510
      for address in (0x800454B0, 0x800455E4, 0x800455EC)),
    ("SLUS_010.40", 0x8004908C, "VS::Core::IdleInstruction"),  # F03568
    *(("SLUS_010.40", address, "VS::Core::IgnoreDiagnostic")  # F03569, F03570
      for address in (0x80049098, 0x800490A0)),
    ("BATTLE/BATTLE.PRG", 0x800BFE00, "VS::Core::Memory::ReadLiveUnalignedShortAt"),  # F01273
    ("BATTLE/BATTLE.PRG", 0x800CCC1C, "VS::Core::Memory::ZeroLiveWordsBackwardAt"),  # F01401
    ("BATTLE/BATTLE.PRG", 0x800CCC34, "VS::Core::Memory::CopyLiveBytesBackwardAt"),  # F01402
    ("SLUS_010.40", 0x80019154, "VS::Core::Memory::CopyLiveWordsAt"),  # F03263
    ("SLUS_010.40", 0x800191D4, "VS::Core::Memory::SwapLiveWordsAt"),  # F03264
    ("SLUS_010.40", 0x80043C60, "VS::Core::Memory::FreeLiveFromHighEndAt"),  # F03463
    ("SLUS_010.40", 0x80043D50, "VS::Core::Memory::FreeLiveFromLowEndAt"),  # F03464
    ("SLUS_010.40", 0x80043E3C, "VS::Core::Memory::AllocateLiveFromHighEndAt"),  # F03465
    ("SLUS_010.40", 0x80043EC4, "VS::Core::Memory::AllocateLiveFromLowEndAt"),  # F03466
    ("SLUS_010.40", 0x80043F74, "VS::Core::Memory::InitializeLiveHeapAt"),  # F03467
    ("BATTLE/BATTLE.PRG", 0x800CCC8C, "VS::Core::NextLiveInterfaceRandomBits"),  # F01404
    ("BATTLE/BATTLE.PRG", 0x800CFB80, "VS::Core::LiveRandomRangeExclusive"),  # F01455
    ("SLUS_010.40", 0x8004274C, "VS::Core::InitializeLiveRandomShuffleTable"),  # F03433
    ("SLUS_010.40", 0x800427A8, "VS::Core::NextLiveRandomBounded"),  # F03434
    ("SLUS_010.40", 0x80042860, "VS::Core::NextLiveRandomAveraged"),  # F03435
    ("BATTLE/BATTLE.PRG", 0x80083430, "VS::Items::CalculateLiveItemRecovery"),  # F00423 Battle_statCalculators[15]
    ("BATTLE/BATTLE.PRG", 0x8008255C, "VS::Field::CalculateLiveTrapPanelDamage"),  # F00420 Battle_statCalculators[12]
    ("BATTLE/BATTLE.PRG", 0x80082FF4, "VS::Field::CalculateLiveHealingPanelRecovery"),  # F00422 [14]
    ("EFFECT/PLG074.BIN", 0x800F98F8, "VS::Effects::Templates::UpdateLivePlg073TiledRing"),  # F01986
    ("EFFECT/PLG074.BIN", 0x800FA7F0, "VS::Effects::Templates::UpdateLivePlg073Ring"),  # F01987
    ("EFFECT/PLG074.BIN", 0x800FD534, "VS::Effects::Templates::UpdateLivePlg073RotatingSurface"),  # F01995
)

# #276 effects-trail-templates: PLG074's trail stages (F01989-F01993) are typed VS::Effects::Plg073 stages that only
# the emitter calls; no image stores their addresses, so they no longer have module entries.
MODULE_FUNCTION_ENTRIES: tuple[tuple[str, int, str], ...] = ()

FIXES: dict[str, list[tuple[str, str, str]]] = {
    "F01013": [
        ("VS::Battle::Ai::AddAiTileOccupant(object);",
         "VS::Battle::Ai::AddAiTileOccupant((const Field_GimmickInstance*)object);",
         "the rebased physics view aliases the native owning Gimmick; pass its checked tile prefix explicitly"),
    ],
    "F00514": [
        ("*(int*)&spec->recoveryDelay & 0x4000",
         "spec->resetsHitResults",
         "Action byte13 bit6 is the typed source interception flag; do not alias four bytes starting at recoveryDelay"),
        ("&origin, &position, &impact, result->primaryTarget.target.targetActor);",
         "&origin, &position, &impact, result->primaryTarget.target.targetActor) != NULL;",
         "Native segment trace returns an output pointer; this caller consumes only collision truth"),
        ("VS::Field::TraceActorAndTerrainCollision(",
         "VS::Field::TraceLiveActorAndTerrainCollision(",
         "F01834 typed core takes a trace world; the generated caller uses the live binding"),
    ],
    "F01363": [
        ("Effects_screenEffectDefaults[2] = 0x1000;",
         "Effects_screenEffectDefaults.radialWarpArg0 = 0x1000;",
         "G00512 owns the complete native overlay state; reset by typed member rather than PS1 short-array index"),
    ],
    "F01389": [
        ("*(int*)&temp_s0->radialWarpArg0 != 0x1000",
         "temp_s0->radialWarpArg0 != 0x1000 || temp_s0->radialWarpArg1 != 0",
         "Compare the two typed warp parameters without aliasing adjacent shorts as int"),
    ],
    "F01390": [
        ("(Effects_ScreenEffectOverlay*)Effects_screenEffectDefaults",
         "&Effects_screenEffectDefaults",
         "Default controller storage is a native overlay object including its widened request pointer"),
    ],
    "F00298": [
        ("VS::Battle::Ai::ActorNavigationOverrideStatus()",
         "VS::Battle::Ai::ActorNavigationOverrideStatus(arg0)",
         "7C7E0.s consumes the actor ID in a0; preserve it explicitly instead of relying on the caller's PS1 register"),
    ],
    "F02461": [
        ("void** arg3", "u_long* arg3",
         "SCREFF2/5E8.c:23 receives one 32-bit ordering link, not native pointer storage"),
        ("(LegacyGpu_Handle(*arg3) & 0xFFFFFF)", "(*arg3 & 0xFFFFFF)",
         "the existing ordering head is already an encoded link"),
        ("*arg3 = (void*)(((uintptr_t)arg2 << 8) >> 8);",
         "*arg3 = LegacyGpu_Handle(arg2);",
         "publish a single-word packet handle without clobbering the adjacent ordering entry"),
    ],
    "F02462": [
        ("void** temp_s4;", "u_long* temp_s4;",
         "SCREFF2/5E8.c:41 shares the same single-word overlay ordering head"),
        ("(LegacyGpu_Handle(*temp_s4) & 0xFFFFFF)", "(*temp_s4 & 0xFFFFFF)",
         "retain the prior encoded head when linking shaded ring packets"),
        ("*temp_s4 = (void*)(((uintptr_t)var_s2 << 8) >> 8);",
         "*temp_s4 = LegacyGpu_Handle(var_s2);",
         "ring packets must publish encoded 32-bit links, never raw x64 addresses"),
    ],
    "F00782": [
        ("memset(s1, 0, 0x178);",
         "memset(s1, 0, offsetof(Field_GimmickInstance, reserved178));",
         "Clear the native runtime prefix, not the PS1 prefix or the larger actor-compatibility view"),
        ("VS::Core::Memory::CopyAlignedWords(s1, Field_gimmickInstances[arg0->dataAddress], 0x178);",
         "VS::Core::Memory::CopyAlignedWords(s1, Field_gimmickInstances[arg0->dataAddress], "
         "offsetof(Field_GimmickInstance, reserved178));",
         "Shared gimmicks copy their complete native prefix including the object-info tail"),
    ],
    **{fid: [(f"if (index == {capacity})", f"if (index > {capacity})",
              "The staging index is one-based; inspect the final valid slot before rejecting a full array.")]
       for fid, capacity in (
           ("F02846", "16"), ("F02847", "16"), ("F02848", "0x30"),
           ("F02850", "16"), ("F02852", "64"),
       )},
    "F02849": [
        ("if (++index == 8)", "if (++index > 8)",
         "Use all eight original temporary weapon slots."),
        ("    int index = 1;\n\n    while",
         """    int index = 1;
    int gems = 0;

    for (i = 0; i < 3; ++i) {
        gems += arg0->gems[i].id != 0;
    }
    if (!Menu_Loot_HasStagingSpace(Items_ItemCategory_Weapon, 1)
        || !Menu_Loot_HasStagingSpace(Items_ItemCategory_Blade, 1)
        || !Menu_Loot_HasStagingSpace(Items_ItemCategory_Grip, 1)
        || !Menu_Loot_HasStagingSpace(Items_ItemCategory_Gem, gems)) {
        return 0;
    }

    while""",
         "Reject a composite staging attempt before reserving its parent or any component."),
    ],
    "F02851": [
        ("if (index == 8)", "if (index > 8)",
         "Use all eight original temporary shield slots."),
        ("    int index = 1;\n\n    while",
         """    int index = 1;
    int gems = 0;

    for (i = 0; i < 3; ++i) {
        gems += arg0->gems[i].id != 0;
    }
    if (!Menu_Loot_HasStagingSpace(Items_ItemCategory_Shield, 1)
        || !Menu_Loot_HasStagingSpace(Items_ItemCategory_Gem, gems)) {
        return 0;
    }

    while""",
         "Reject a shield before staging a partial set of mounted gems."),
    ],
    "F02858": [
        *[(f"Menu_Loot_IsDropAllowed(arg0->{member}.difficultyFlags)",
           f"Menu_Loot_CanStageDrop(&arg0->{member}.difficultyFlags)",
           "Track eligible overflow separately from successfully staged drops.")
          for member in ("weapon", "blade", "grip", "shield", "armor[i]", "accessory", "gem", "misc[i]")],
        *[(f"Menu_Loot_AppendLootRow({category},",
           f"Menu_Loot_AppendStagedDrop(&arg0->{member}.difficultyFlags, {category},",
           "Never append a zero staging result; retain the complete source for the next batch.")
          for member, category in (
              ("weapon", "Items_ItemCategory_Weapon"), ("blade", "Items_ItemCategory_Blade"),
              ("grip", "Items_ItemCategory_Grip"), ("shield", "Items_ItemCategory_Shield"),
              ("gem", "Items_ItemCategory_Gem"), ("misc[i]", "Items_ItemCategory_Misc"),
          )],
        ("Menu_Loot_AppendLootRow(\n                Items_ItemCategory_Armor, Menu_Loot_StageArmor(&arg0->armor[i].armor));",
         "Menu_Loot_AppendStagedDrop(&arg0->armor[i].difficultyFlags,\n"
         "                Items_ItemCategory_Armor, Menu_Loot_StageArmor(&arg0->armor[i].armor));",
         "Retain each deferred limb armor independently."),
        ("Menu_Loot_AppendLootRow(\n            Items_ItemCategory_Armor, Menu_Loot_StageArmor(&arg0->accessory.accessory));",
         "Menu_Loot_AppendStagedDrop(&arg0->accessory.difficultyFlags,\n"
         "            Items_ItemCategory_Armor, Menu_Loot_StageArmor(&arg0->accessory.accessory));",
         "Accessories share armor storage but retain their own source entry."),
    ],
    "F02859": [
        ("    VS::Core::Memory::ZeroWordsBackward(Menu_Loot_inventory, sizeof *Menu_Loot_inventory);",
         "    Menu_Loot_BeginPaging(node);\n"
         "    VS::Core::Memory::ZeroWordsBackward(Menu_Loot_inventory, sizeof *Menu_Loot_inventory);",
         "Start each batch with independent row indices and no stale source pointers."),
        ("\n}", """
}

/* Native batching for MENUB 260.c:1124/1174; original bag capacities remain unchanged. */
static struct {
    Items_LootListNode* nodes;
    char* stagedFlags[kMenuLootRowCapacity];
    int count;
    int deferred;
} Menu_Loot_paging;

void Menu_Loot_ResetPaging(void)
{
    VS::Core::Memory::ZeroAlignedWords(&Menu_Loot_paging, sizeof Menu_Loot_paging);
}

void Menu_Loot_BeginPaging(Items_LootListNode* nodes)
{
    Menu_Loot_ResetPaging();
    Menu_Loot_paging.nodes = nodes;
    Menu_Loot_lootListCount = 0;
    Menu_Loot_lootListOffset = 0;
    Menu_Loot_selectedLoot = 0;
}

int Menu_Loot_HasStagingSpace(int category, int quantity)
{
    return VS::Items::Inventory::CountLiveItems(category, Menu_Loot_inventory) + quantity
        <= Items_Inventory_inventoryItemCapacities[category];
}

int Menu_Loot_CanStageDrop(char* difficulty_flags)
{
    if (Menu_Loot_IsDropAllowed(*difficulty_flags) == 0) {
        return 0;
    }
    if (Menu_Loot_paging.count == kMenuLootRowCapacity) {
        Menu_Loot_paging.deferred = 1;
        return 0;
    }
    return 1;
}

void Menu_Loot_AppendStagedDrop(char* difficulty_flags, int category, int index)
{
    if (index == 0) {
        Menu_Loot_paging.deferred = 1;
        return;
    }
    Menu_Loot_AppendLootRow(category, index);
    Menu_Loot_paging.stagedFlags[Menu_Loot_paging.count++] = difficulty_flags;
}

int Menu_Loot_AdvanceStagedInventory(void)
{
    int i;
    Items_LootListNode* nodes = Menu_Loot_paging.nodes;
    if (Menu_Loot_paging.deferred == 0) {
        return 0;
    }
    /* Called only after claims finish or the existing leave confirmation accepts this batch. */
    for (i = 0; i < Menu_Loot_paging.count; ++i) {
        *Menu_Loot_paging.stagedFlags[i] = 0;
    }
    Menu_Loot_BuildStagedInventory(nodes);
    Menu_Loot_lootListOffset = 0;
    Menu_Loot_selectedLoot = 0;
    return Menu_Loot_lootListCount != 0;
}""",
         "Keep excess source entries alive and expose them only after the current batch is resolved."),
    ],
    "F02864": [
        ("""            } else {
                return i;
            }""",
         """            } else if (Menu_Loot_AdvanceStagedInventory()) {
                Menu_Loot_controlsEnabled = 0;
                Menu_Loot_showLoot = 1;
                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Init;
            } else {
                return i;
            }""",
         "Claims or confirmed departure resolve only the visible batch; cancel and key protection stay original."),
    ],
    "F02865": [
        (" * 160", " * kMenuLootRowCapacity",
         "Reserve metadata for every original inventory slot, including rows created by disassembly."),
        ("        VS::Core::Memory::FreeFromHighEnd(Menu_Loot_inventory);",
         "        Menu_Loot_ResetPaging();\n"
         "        VS::Core::Memory::FreeFromHighEnd(Menu_Loot_inventory);",
         "Discard page source references before the working inventory and pending nodes are released."),
    ],
    "F02844": [
        ("160 * sizeof", "kMenuLootRowCapacity * sizeof",
         "Disassembly backup/restore must use the complete native row allocation."),
    ],
    "F02845": [
        ("160 * sizeof", "kMenuLootRowCapacity * sizeof",
         "Consolidation backup uses the same row allocation as the live screen."),
        ("sizeof *Menu_Loot_lootList * 160", "sizeof *Menu_Loot_lootList * kMenuLootRowCapacity",
         "Consolidation restoration includes every supported row."),
    ],
    "F01563": [
        ("(*(u_int*)((char*)arg0 + 0x14) & 0xF800) == 0",
         "arg0->childCount == 0",
         "the original +0x14 child-count bits move when native instance pointers grow"),
    ],
    "F01515": [
        ("    for (i = 0; i < count; ++i) {\n        node = Effects_AllocateEffectParticle(arg0);",
         '    if (count <= 0) {\n'
         '        Core_Debugf("effects", "particle emission skipped: count=%d used=%d capacity=%d",\n'
         '            count, Effects_usedParticleCount, Effects_particleCapacity);\n'
         '        return;\n'
         '    }\n\n'
         '    for (i = 0; i < count; ++i) {\n        node = Effects_AllocateEffectParticle(arg0);',
         "zero/full-capacity bursts have no first particle; avoid the copied C's uninitialized node"),
        ("""                scratch->vector.vx =
                    (((scratch->vector.vx * scratch->cosAngle0) >> 12) * scratch->vector.vy)
                    / scratch->extent.vy;
                scratch->vector.vz =
                    (((scratch->vector.vz * scratch->sinAngle0) >> 12) * scratch->vector.vy)
                    / scratch->extent.vy;""",
         """                if (scratch->extent.vy == 0) {
                    // BATTLE.PRG, DIV at 0x800D34BC/0x800D34F8:
                    // zero height makes both dividends zero; the original LO is -1.
                    scratch->vector.vx = scratch->vector.vz = -1;
                } else {
                    scratch->vector.vx =
                        (((scratch->vector.vx * scratch->cosAngle0) >> 12) * scratch->vector.vy)
                        / scratch->extent.vy;
                    scratch->vector.vz =
                        (((scratch->vector.vz * scratch->sinAngle0) >> 12) * scratch->vector.vy)
                        / scratch->extent.vy;
                }""",
         "Preserve the original cone's nontrapping 0/0 quotient when its height is zero, "
         "without changing nonzero scaling, particle emission or RNG consumption."),
    ],
    "F01531": [
        ("arg0->registers[Effects_ReadScriptByte(arg0)] = Effects_ReadScriptHalfword(arg0);",
         "u_char registerIndex = Effects_ReadScriptByte(arg0);\n"
         "    u_short value = Effects_ReadScriptHalfword(arg0);\n"
         "    arg0->registers[registerIndex] = value;",
         "5BF94.c:7633 / 800D4AFC..800D4B18 reads the index before the halfword; "
         "native assignment evaluation must not reorder the shared cursor"),
    ],
    "F01532": [
        ("Effects_BranchEffectScriptIf(arg0->registers[Effects_ReadScriptByte(arg0)] == "
         "(Effects_ReadScriptHalfword(arg0)), arg0);",
         "u_char registerIndex = Effects_ReadScriptByte(arg0);\n"
         "    u_short value = Effects_ReadScriptHalfword(arg0);\n"
         "    Effects_BranchEffectScriptIf((u_char)arg0->registers[registerIndex] == value, arg0);",
         "5BF94.c:7639 / 800D4B44..800D4B74 preserves ordered operands and the original lbu comparison"),
    ],
    "F01535": [
        ("    /* The original caller supplies arguments to this argument-free wrapper. */\n", "",
         "the retained assembly renderer now has an explicit native particle-list contract"),
        ("((void (*)())VS::Effects::DrawEffectParticleList)(arg0->particles, (void*)((uintptr_t)Core_ScratchBase() + 0x15C));",
         "VS::Effects::DrawLiveEffectParticleList(arg0->particles);",
         "F01593 typed core takes pool indices and a renderer; the generated caller uses the live binding"),
        ("        ++node->age;\n",
         "        ++node->age;\n"
         "        Effects_PluginEffectState parent = {0};\n"
         "        parent.position.vx = node->motion.position.vx;\n"
         "        parent.position.vy = node->motion.position.vy;\n"
         "        parent.position.vz = node->motion.position.vz;\n"
         "        parent.transparency = node->transparency;\n"
         "        parent.variant = node->opaque61Bit0;\n",
         "project native particle fields into the existing child-emission context; "
         "the original PS1 prefix alias no longer matches after pointer widening"),
        ("(void*)node", "&parent",
         "tick, lifetime-end and texture-end emissions must not reinterpret "
         "native particle links and texture pointers as parent coordinates or appearance"),
    ],
    "F00731": [
        ("*ot = ((u_long)prim << 8) >> 8;", "*ot = LegacyGpu_Handle(prim);",
         "overhead awareness frame links must retain native packet ownership, not truncated addresses"),
    ],
    "F02799": [
        ("*before = ((u_long)prim << 8) >> 8;",
         "*before = LegacyGpu_Handle(prim);",
         "238.c:54 retain every Score scroll-fade packet in the native ordering chain"),
    ],
    "F02810": [
        ("    void** scratch = (void**)Core_ScratchAddr(0);\n\n"
         "    line = scratch[0];",
         "    line = Render_frameScratch.primitiveCursor;",
         "238.c:1552 Score viewer banners borrow the typed native packet cursor"),
        ("AddPrim(*((void**)Core_ScratchAddr(1)) + 0x1C, line++);",
         "AddPrim(Render_frameScratch.orderingTable + 0x1C, line++);",
         "PS1 scratch word1 must not read a native pointer at byte4"),
        ("    scratch = (void**)Core_ScratchAddr(0);\n\n"
         "    AddPrim(scratch[1] + 0x1C, poly++);\n\n"
         "    scratch[0] = (void*)poly;",
         "    AddPrim(Render_frameScratch.orderingTable + 0x1C, poly++);\n\n"
         "    Render_frameScratch.primitiveCursor = poly;",
         "Publish viewer banner and shadow packets through native frame fields"),
    ],
    "F02816": [
        ("char digits[2];", 'char digits[sizeof "00"];',
         "238.c:1835 two title digits also need the native sprintf terminator"),
        ("poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);",
         "poly = Render_frameScratch.primitiveCursor;",
         "Title digits use the typed native packet cursor"),
        ("AddPrim(*(void**)((uintptr_t)Core_ScratchBase() + 0x008), poly++);",
         "AddPrim(Render_frameScratch.overlayOrderingTable, poly++);",
         "PS1 scratch offset8 is the overlay ordering pointer, not native offset8"),
        ("*(void**)((uintptr_t)Core_ScratchBase() + 0x000) = poly;",
         "Render_frameScratch.primitiveCursor = poly;",
         "Publish the advanced title-digit cursor through the native frame"),
    ],
    "F02817": [
        ("char digits[2];", 'char digits[sizeof "000"];',
         "238.c:1878 three encyclopedia digits also need the native sprintf terminator"),
        ("poly = *(void**)Core_ScratchAddr(0);",
         "poly = Render_frameScratch.primitiveCursor;",
         "Encyclopedia digits use the typed native packet cursor"),
        ("AddPrim(*(void**)Core_ScratchAddr(2), poly++);",
         "AddPrim(Render_frameScratch.overlayOrderingTable, poly++);",
         "PS1 scratch word2 is the overlay ordering pointer, not native word2"),
        ("*(void**)Core_ScratchAddr(0) = poly;",
         "Render_frameScratch.primitiveCursor = poly;",
         "Publish the advanced encyclopedia-digit cursor through the native frame"),
    ],
    "F02820": [
        ("char sp60[12];", 'char sp60[sizeof "NO.   000/000"];',
         "238.c:2069 preserve the complete valid encyclopedia header and its terminator"),
    ],
    "F02839": [
        ("VS::Core::Memory::AllocateFromLowEnd(0x3200)",
         "VS::Core::Memory::AllocateFromLowEnd(2 * sizeof(Actors_ActorRecord))",
         "592C.c:171 companion previews own two native actor records"),
        ("VS::Core::Memory::AllocateFromLowEnd(0x24D0)",
         "VS::Core::Memory::AllocateFromLowEnd(sizeof(Actors_ActorRecord) + "
         "2 * sizeof(Actors_WeaponModel))",
         "Normal previews own a native actor plus two native equipment records"),
    ],
    "F02840": [
        ("Content_ActorAssetRequest slot;",
         "Content_ActorAssetRequest slot = {0};",
         "592C.c:203 deterministic preview placement must not inherit moving-platform bits from the stack"),
        ("Menu_Score_zudBuffer + 0x1900",
         "(u_char*)Menu_Score_zudBuffer + sizeof(Actors_ActorRecord)",
         "Companion and first equipment records follow the complete native actor record"),
        ("Menu_Score_zudBuffer + 0x1EE8",
         "(u_char*)Menu_Score_zudBuffer + sizeof(Actors_ActorRecord) + "
         "sizeof(Actors_WeaponModel)",
         "Shield storage follows the complete native weapon record without overlap"),
    ],
    "F02553": [
        (r'"MISC\00000|0xF8|"',
         r'"\x16\x12\x1C\x0C\xE7\x00\x00\xF8"',
         "C48.c:485 pragma vsstring encodes MISC then E7 termination; preserve reset bytes and avoid runaway glyph packets"),
        ("VS::Core::Memory::AllocateFromHighEnd(0x1B00)",
         "VS::Core::Memory::AllocateFromHighEnd(0x1900 + 64 * 2 * sizeof(*menuText))",
         "C48.c:546 scratch ends with 128 text pointers; reserve native pointer width, not the PS1 512 bytes"),
    ],
    "F00508": [
        ("arg0, 0x84C", "arg0, sizeof(Battle_ActionRequest)",
         "Clear the complete native action result including all supported targets"),
    ],
    "F01554": [
        ("((Effects_ParticleAttractionView*)node)->attraction",
         "node->attraction",
         "Read native attraction strength rather than emitter flags at the old PS1 offset"),
        *[(f"((Effects_ParticleAttractionView*)node)->target{axis}",
           f"node->attractionTarget[{index}]",
           "Read the native particle target rather than texture-animation pointer bytes")
          for index, axis in enumerate("XYZ")],
    ],
    "F01279": [
        ("void Battle_InitSphere(void)",
         '#include "Battle/TargetPages.h"\n\nvoid Battle_InitSphere(void)',
         "Expose native page lifetime hooks to the sphere translation unit"),
        ("    int index;", "    Battle_ResetTargetPages();\n    int index;",
         "New sphere entry clears any previous action page view"),
    ],
    "F01280": [
        ("    if (Battle_targetSphere != NULL) {",
         "    Battle_ResetTargetPages();\n    if (Battle_targetSphere != NULL) {",
         "Free/re-entry cannot retain a borrowed page pointer into an old action state"),
    ],
    "F01297": [
        ("    if ((*(Battle_TargetSphere* (*))&Battle_targetSphere) != NULL) {",
         "    Battle_ResetTargetPages();\n    if ((*(Battle_TargetSphere* (*))&Battle_targetSphere) != NULL) {",
         "Clearing target selection invalidates the native page view"),
    ],
    "F02703": [
        ("*(int**)Core_ScratchAddr(1) + 7",
         "(u_long*)Render_frameScratch.orderingTable + 7",
         "4D8.c:1965 scratch word1 is the upper half of a native cursor, not the native ordering pointer"),
    ],
    "F02699": [
        ("    u_long* pScratch1;\n    u_long* pScratch2;\n", "",
         "4D8.c:1788 use native frame fields instead of two32-bit scratch words"),
        ("    pScratch1 = Core_ScratchAddr(0);\n", "",
         "native primitive cursor does not need a word-pointer view"),
        ("sprt = (SPRT*)pScratch1[0];",
         "sprt = Render_frameScratch.primitiveCursor;",
         "preserve all pointer bits on the first framebuffer strip"),
        ("(void*)(pScratch1[1] + 0x7FE * 4)",
         "(u_long*)Render_frameScratch.orderingTable + 0x7FE",
         "first framebuffer strip uses the native scene ordering table"),
        ("*Core_ScratchAddr(0) = (u_long)sprt;",
         "Render_frameScratch.primitiveCursor = sprt;",
         "store the complete native cursor after the first strip"),
        ("sprt = (SPRT*)*Core_ScratchAddr(0);",
         "sprt = Render_frameScratch.primitiveCursor;",
         "preserve all pointer bits on the second framebuffer strip"),
        ("    pScratch2 = Core_ScratchAddr(0);\n", "",
         "second strip also uses typed native frame fields"),
        ("(void*)(pScratch2[1] + 0x7FE * 4)",
         "(u_long*)Render_frameScratch.orderingTable + 0x7FE",
         "second framebuffer strip uses the native scene ordering table"),
        ("pScratch2[0] = (uintptr_t)sprt;",
         "Render_frameScratch.primitiveCursor = sprt;",
         "do not truncate the second strip cursor through a32-bit lvalue"),
    ],
    "F04153": [
        ("    short (*var_s1)[4] = (void*)Core_ScratchAddr(64);",
         "    enum { kMapVertexCapacity = 256 };\n"
         "    short projectedVertices[kMapVertexCapacity][4];\n"
         "    short (*var_s1)[4] = projectedVertices;",
         "4D8.c:1112 byte-indexed scene vertices need a local256-entry cache, not the96-entry shared scratch tail"),
    ],
    "G00864": [
        ("char Menu_Framework_comparisonStatTotals[8]",
         "short Menu_Framework_comparisonStatTotals[4]",
         "C48.c:992 original adjacent8-byte totals region contains signed halfwords, not independent character state"),
    ],
    "F02561": [
        *[(f"Menu_Framework_comparisonStatDeltas[{index + 4}]",
           f"Menu_Framework_comparisonStatTotals[{index}]",
           "C48.c:992 write the explicit totals buffer instead of overrunning the four delta/index halfwords")
          for index in range(3)],
        ("Menu_Framework_comparisonStatDeltas[4 + i]",
         "Menu_Framework_comparisonStatTotals[i]",
         "C48.c:1011 subtract the same typed totals that the original clears through D_80102490"),
    ],
    "F02562": [
        ("Menu_Framework_comparisonStatDeltas[i + 4]",
         "Menu_Framework_comparisonStatTotals[i]",
         "C48.c:1025 compare against explicit totals without reading past the delta/index buffer"),
    ],
    "F02555": [
        ("((u_long)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
         "C48.c:673 stat-page bars at ordering entry15 must retain traversal through all lower UI entries"),
        (r'"|f0|CLASS|f0|/|f0|AFFINITY|f0|/|f0|TYPE"',
         r'"\xFB\x00\x0C\x15\x0A\x1C\x1C\xFB\x00\xA1\xFB\x00\x0A\x0F\x0F\x12\x17\x12\x1D\x22\xFB\x00\xA1\xFB\x00\x1D\x22\x19\x0E"',
         "C48.c:673 pragma vsstring page header is29bytes; preserve font operands at1/11/24 and19visible glyphs"),
    ],
    "F02524": [
        (r'"|!0|' + "0" * 96 + r'\0\0"',
         "{ [0] = Core_TextCode_ChunkSize, [98] = Core_TextCode_Terminator,\n"
         "          [99] = Core_TextCode_Terminator, [100] = 0 }",
         "58EC.c:475 pragma vsstring encodes |!0| as F8 00; retain 96-byte payload and both E7 guards"),
    ],
    **{fid: [
        ("VS::Field::ResolveSceneRoomData(Battle_unusedSceneBuffer)",
         "VS::Field::ResolveLiveSceneRoomData(Battle_unusedSceneBuffer, Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size)",
         "bound ARM scene decoding to its loaded file extent"),
        ("VS::Field::RestoreLiveSceneRoomVisits(Battle_unusedSceneBuffer)",
         "VS::Field::RestoreLiveSceneRoomVisits(Battle_unusedSceneBuffer, Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size)",
         "bound the room visit restore to the loaded ARM file extent"),
        ("*(u_short*)Core_ScratchAddr(22)",
         "(u_short)Render_frameScratch.camera.angles.vy",
         "map camera initialization/reset must follow native yaw, not original scratch byte88"),
    ] for fid in ("F04150", "F04151")},
    "G00620": [
        (r'"Ashley|>6|Riot\0"',
         r'"\x0A\x36\x2B\x2F\x28\x3C\xFA\x06\x1B\x2C\x32\x37\xE7"',
         "INITBTL.PRG 18.c pragma vsstring default actor name; original encode_raw glyphs spacing and terminator"),
        (r'"Fandango\0"', r'"\x0F\x24\x31\x27\x24\x31\x2A\x32\xE7"',
         "INITBTL.PRG 18.c pragma vsstring default weapon name; preserve original encoded text"),
    ],
    "F02604": [
        ("((u_int)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
         "MENU0/84.c rank selector must link its final frame packet with a native handle; truncation hides the lower UI chain"),
    ],
    "F02610": [
        ("static u_short Menu_BreakArts_artListActionIds[4];",
         "static u_short Menu_BreakArts_artListActionIds[5];",
         "MENU1/30.c weapon arts need four action IDs plus View Arts; adapt before hoisting so the reload initializer has the same extent"),
    ],
    "F02662": [
        ("((u_long)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
         "Status limb indicator packets retain native link handles"),
    ],
        "F02663": [
           ("((uintptr_t)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
            "Status limb leader-line link must not terminate traversal before the lower UI entries"),
        ],
    "F02677": [
        ("((uintptr_t)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
         "Status model fade packets retain native link handles"),
    ],
    "F02678": [
        ("((u_long)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
         "Status background packets retain native link handles instead of hiding the remaining frame chain"),
    ],
    "F02682": [
        ("        Menu_Status_updateScreenEnabled = 1;\n        Menu_Status_updateAnimWait = 1;",
         "        Menu_Status_updateAnimWait = 1;",
         "the native preview slot is not initialized until the actor-switch producer runs"),
        ("        if (Menu_Status_updateAnimWait != 0) {\n            if (Menu_Status_AnimateActorSwitch(0)) {",
         "        if (Menu_Status_updateAnimWait != 0) {\n"
         "            int preview_ready = Menu_Status_AnimateActorSwitch(0);\n"
         "            Menu_Status_updateScreenEnabled = 1;\n"
         "            if (preview_ready) {",
         "enable Status drawing only after the actor-switch producer has loaded its native preview"),
    ],
    "F02467": [
        ("        int* p = (int*)((uintptr_t)Core_ScratchBase() + 0x000);\n", "",
         "use the native frame ordering table"),
        ("        p = (int*)((uintptr_t)Core_ScratchBase() + 0x000);\n", "",
         "restore the native frame ordering table"),
        ("p[1] += new_var2;", "Render_frameScratch.orderingTable += new_var2 / sizeof(u_long);",
         "preview companion depth changes the native ordering pointer, not half of a pointer"),
        ("p[1] -= new_var3;", "Render_frameScratch.orderingTable -= new_var3 / sizeof(u_long);",
         "restore the preview ordering pointer"),
    ],
    "F02464": [
        ("    Content_ActorAssetRequest sp10;", "    Content_ActorAssetRequest sp10 = {0};",
         "menu clones must not interpret uninitialized placement bytes as a platform id"),
        ("VS::Core::Memory::AllocateFromLowEnd(0x1900)", "VS::Core::Memory::AllocateFromLowEnd(sizeof(Actors_ActorRecord))",
         "menu actor and companion clones need complete native record storage"),
    ],
    "F02465": [
        ("Battle_unusedGlobal1[i]->opaque8 |= 0x10;",
         "Actors_weaponModels[i + 2]->reserved08Bit4 = 1;",
         "D_800F4590 is the preview pair at vs_battle_wepModels+8, not independent pose storage"),
        ("Battle_unusedGlobal1[i]", "Actors_weaponModels[i + 2]",
         "check the real preview weapon/shield slots using native pointer stride"),
    ],
    "F02637": [
        ("Battle_unusedGlobal2 &&", "Actors_weaponModels[3] != NULL &&",
         "D_800F4594 aliases vs_battle_wepModels[3]; two-handed changes release the real shield"),
        ("Battle_unusedGlobal1[i]->opaque11 = 0;",
         "Actors_weaponModels[i + 2]->reserved11 = 0;",
         "MENU3/68C4.c resets the preview weapon fade at byte 0x11 of the same model"),
        ("Battle_unusedGlobal1[i]", "Actors_weaponModels[i + 2]",
         "D_800F4590 aliases vs_battle_wepModels[2], including its second preview slot"),
    ],
    "F00195": [
        ("Battle_cameraEaseMode", "Field_sphericalCamera.delta.mode",
         "D_800F19EC aliases the spherical camera mode at +0x1C"),
        ("Battle_settledCameraDistance", "Field_sphericalCamera.values.distance",
         "D_800F19D8 aliases the spherical camera distance at +0x8"),
    ],
    "F00186": [
        ("    VECTOR sp30;",
         "    VECTOR sp30;\n"
         '    Core_Debugf("combat-flow", "camera mode=%d zoom=%d yaw=%d distance=%d action=%u targets=%u",\n'
         "        Field_sphericalCamera.delta.mode, Battle_actionState->cameraZoomState,\n"
         "        Field_sphericalCamera.delta.yaw, Field_sphericalCamera.values.distance,\n"
         "        (unsigned)Battle_actionState->selection.actionId, (unsigned)Battle_actionState->selection.targetCount);",
         "temporary combat camera gate trace"),
        ("VS::Field::LiveComputeCameraTrackingStep(&sp18, &sp30, &sp20, 1, 0x28);",
         "VS::Field::LiveComputeCameraTrackingStep(&sp18, &sp30, &sp20, 1, 0x28);\n"
         '                Core_Debugf("combat-flow", "tracking error=%ld,%ld,%ld", sp20.vx, sp20.vy, sp20.vz);',
         "temporary camera convergence trace"),
    ],
    "F01364": [
        ("    VS::Core::Memory::ZeroWordsBackward(&Menu_Framework_actionSelection, sizeof Menu_Framework_actionSelection);",
         "    VS::Core::Memory::ZeroWordsBackward(&Menu_Framework_actionSelection, sizeof Menu_Framework_actionSelection);\n"
         "    Battle_actionSelection = Menu_Framework_ActionSelectionState();",
         "basic attacks may precede the first menu open; bind the interface-owned selection state at initialization"),
        ("    Render_gimLayers = NULL;", "    VS::Render::ForgetLiveGimImages();",
         "#276 row 6: the heap was reset under the GIM set; the typed set is forgotten without being freed"),
    ],
    "F01301": [
        ("((u_long)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
         "target-label backdrop uses native ordering links"),
    ],
    "F01289": [
        ("    Battle_UpdateSphereTargetLabels();",
         "    Battle_UpdateSphereTargetLabels();\n"
         "    Battle_DrawTargetPageHint();",
         "Only extended native target lists display a readable page indicator"),
    ],
    "F01302": [
        ("    for (var_s3 = 0; var_s3 < 24; ++var_s3, var_s0 += 10, ++var_s1) {",
         "    for (var_s3 = 0; var_s3 < 24; ++var_s3, ++var_s1) {\n"
         "        var_s0 = &var_s1->screenX;",
         "follow the native label stride; the PS1 20-byte step corrupts adjacent text pointers"),
    ],
    "F01287": [
        ("((u_long)primitive << 8) >> 8", "LegacyGpu_Handle(primitive)",
         "sphere wireframe packets use native ordering links"),
    ],
    "G00229": [
        ("Effects_PackageFile Effects_emptyPackage", "u_int Effects_emptyPackage[0x5200 / sizeof(u_int)]",
         "D_800EC4BC contains nine tagged blocks and the terminator at +0x51FC, not just a header"),
    ],
    "G00582": [("int Battle_Ai_roomHasWaypoints", "void* Battle_Ai_roomHasWaypoints",
                 "func_800E6F9C stores the room auxiliary-data pointer, not a boolean")],
    "F01221": [
        ("        c->durationFrames = script[2];",
         "        c->durationFrames = script[2];\n"
         '        Core_Debugf("event.camera", "start track=%s mode=%u easing=%u duration=%u",\n'
         '            c == &Event_camera->eye ? "eye" : "look_at",\n'
         "            (unsigned)c->motionMode, (unsigned)c->easingPrimary,\n"
         "            (unsigned)c->durationFrames);",
         "record actual camera-track producers when investigating zero-duration interpolation"),
        ("        VS::Actors::LiveActorPlacement(id, &Event_camera->actorQueryScratch[1]);\n"
         "        s->actorRotation.vz = 0;\n"
         "        s->actorRotation.vx = 0;\n"
         "        s->actorRotation.vy = *(short*)((char*)Event_camera + 0x1DA);",
         "        Actors_ActorLocation placement{};\n"
         "        VS::Actors::LiveActorPlacement(id, &placement);\n"
         "        s->actorRotation.vz = 0;\n"
         "        s->actorRotation.vx = 0;\n"
         "        s->actorRotation.vy = placement.position.pad;",
         "#276 slice 19b: the original staged the placement in the camera's actorQueryScratch[1..3] (no other "
         "reader) and read the facing back from camera offset 0x1DA, its position.pad; a typed local replaces "
         "both, and an empty slot now gives facing 0 instead of the previous call's stale bytes"),
    ],
    "F01275": [
        ("    case 2:\n        return arg0;\n    }\n}",
         "    case 2:\n        return arg0;\n    }\n    return arg0;\n}",
         "#303: 573B8.c:156 has no return after the switch; the PS1 epilogue (func_800BFE50) returns the reference "
         "in v0 unchanged for types 3-15, so the native path returns it instead of an indeterminate value"),
    ],
    "F01223": [
        ("    if (c->motionMode != 11) {",
         "    if (c->durationFrames == 0 && c->motionMode != 11) {\n"
         '        Core_Warnf("event.camera", "zero duration track=%s mode=%u easing=%u elapsed=%u tick_speed=%u",\n'
         '            c == &Event_camera->eye ? "eye" : "look_at",\n'
         "            (unsigned)c->motionMode, (unsigned)c->easingPrimary,\n"
         "            (unsigned)c->elapsedFrames, (unsigned)State_tickSpeed);\n"
         "    }\n"
         "    if (c->motionMode != 11) {",
         "capture active zero-duration track provenance before the original interpolation operation"),
    ],
    "F01238": [
        ("&Event_kOrbitReturnFirstCommand", "Event_kOrbitReturnFirstCommand",
         "4A0A8.c:4068 consumes the complete four-byte eye-orbit command"),
        ("&Event_kOrbitReturnSecondCommand", "Event_kOrbitReturnSecondCommand",
         "4A0A8.c:4069 consumes the complete three-byte look-at tween command"),
    ],
    "F01384": [
        ("int* value", "const State_SceneTimer* timer",
         "read the authoritative native scene clock through a typed const contract"),
        ("    char sp10[4];",
         "    char sp10[4] = {0, timer->seconds, timer->minutes, timer->hours};",
         "format named clock fields without reinterpreting an independent packed alias"),
        ("    *(int*)sp10 = *value;\n", "",
         "the HUD no longer copies the detached PS1 timer-word view"),
        ("Battle_puzzleTimerTextPointer", "Battle_puzzleTimerText",
         "5BF94.c:3844 the native timer owns its writable buffer instead of depending on a dropped PS1 pointer binding"),
        ("    VS::Render::DrawLiveCompactText(Battle_puzzleTimerText,",
         '    Core_Debugf("hud.timer", "text=%s seconds=%d minutes=%d hours=%d frames=%d",\n'
         "        Battle_puzzleTimerText, timer->seconds, timer->minutes, timer->hours, timer->frames);\n"
         "    VS::Render::DrawLiveCompactText(Battle_puzzleTimerText,",
         "retain exact per-render timer text in detailed gameplay diagnostics"),
    ],
    "F01392": [
        ("Battle_DrawPuzzleTimer(&Battle_puzzleTimer);",
         "Battle_DrawPuzzleTimer(&State_sceneTimer);",
         "SLUS 0x8005046C is the time-field subobject of the authoritative timer at 0x80050468, not independent native storage"),
        ("        Render_activeTextBoxIndex = id;\n\n", "",
         "#276 row 6: the text box drawers take the box's layers from its index in Menu_textBoxes (D_800F4E80 retired)"),
    ],
    "F01590": [
        ("    arg0[0] = 0;\n    arg0[1] = 0;\n    arg0[2] = 0;\n"
         "    arg0[3] = 0;\n    arg0[4] = 0;\n    arg0[5] = 0;\n"
         "    arg0[6] = 0;\n    arg0[7] = 0;",
         "    Effects_ParticleTextureAnimation* state = (Effects_ParticleTextureAnimation*)arg0;\n"
         "    *state = (Effects_ParticleTextureAnimation){0};",
         "func_800D6CCC: retain the existing C entry contract but clear the complete native texture state, including widened pointers"),
    ],
    "F01591": [
        ("    arg0->textureIndex = textureIndex;",
         "    if (textureIndex < 0 || textureIndex >= Effects_package->block1TableCount ||\n"
         "        textureIndex >= 4 || Effects_package->textureInfoTable[textureIndex] == NULL) {\n"
         '        Core_Debugf("effects.texture", "unbound texture=%d tables=%d program=%d context=%s request_effect=%u",\n'
         "            textureIndex, Effects_package->block1TableCount, arg2,\n"
         '            Effects_package == &Effects_actionPackage ? "primary" : "secondary",\n'
         "            (unsigned)(Effects_package == &Effects_actionPackage ?\n"
         "                Effects_actionRequest.effectId : Effects_secondaryRequest.effectId));\n"
         "    }\n    arg0->textureIndex = textureIndex;",
         "record unavailable texture-group provenance before investigating malformed frames; no fallback or changed playback"),
    ],
    "F01861": [
        ("        Effects_InitializeParticleTextureAnimation(&state->texture, effect->ringSegments, effect->ringPositions);",
         "        Effects_InitializeParticleTextureAnimation(&state->texture, effect->ringSegments, effect->ringPositions);\n"
         '        Core_Debugf("effects.texture", "closed ring init definition=%u parameter=0x%X table=%u program=%u context=%s request_effect=%u tables=%d",\n'
         "            (unsigned)state->effect, (unsigned)arg2,\n"
         "            (unsigned)effect->ringSegments, (unsigned)effect->ringPositions,\n"
         '            Effects_package == &Effects_actionPackage ? "primary" : "secondary",\n'
         "            (unsigned)(Effects_package == &Effects_actionPackage ?\n"
         "                Effects_actionRequest.effectId : Effects_secondaryRequest.effectId),\n"
         "            Effects_package->block1TableCount);\n"
         "        if (effect->ringSegments >= Effects_package->block1TableCount ||\n"
         "            effect->ringSegments >= 4 || Effects_package->textureInfoTable[effect->ringSegments] == NULL) {\n"
         '            Core_Debugf("effects.texture", "closed ring definition=%u parameter=0x%X table=%u program=%u",\n'
         "                (unsigned)state->effect, (unsigned)arg2,\n"
         "                (unsigned)effect->ringSegments, (unsigned)effect->ringPositions);\n"
         "        }",
         "capture the exact closed-ring definition and packed arguments for an unavailable texture group without changing rendering"),
    ],
    "F01592": [
        ("        case 0:\n            arg0->textureInfo =",
         "        case 0:\n"
         "            if (arg0->textureIndex >= 4 ||\n"
         "                arg0->textureIndex >= Effects_package->block1TableCount ||\n"
         "                Effects_package->textureInfoTable[arg0->textureIndex] == NULL) {\n"
         '                Core_Warnf("effects.texture", "unbound frame table=%u tables=%d cursor=%u word=0x%08X context=%s request_effect=%u program=%p previous=%p",\n'
         "                    (unsigned)arg0->textureIndex, Effects_package->block1TableCount,\n"
         "                    (unsigned)arg0->cursor, (unsigned)w,\n"
         '                    Effects_package == &Effects_actionPackage ? "primary" : "secondary",\n'
         "                    (unsigned)(Effects_package == &Effects_actionPackage ?\n"
         "                        Effects_actionRequest.effectId : Effects_secondaryRequest.effectId),\n"
         "                    (void*)arg0->program, (void*)arg0->textureInfo);\n"
         "            }\n"
         "            arg0->textureInfo =",
         "capture an unavailable frame table at consumption time to distinguish context changes from initial binding"),
    ],
    "F00836": [
        ("    i = 0;\n\n    while ((temp_v0_2 = func_800A3C00",
         "    if (Field_gimmickInstances[temp_s4] == NULL) {\n"
         '        Core_Debugf("gimmick", "no eligible stack top while removing slot=%d", arg0);\n'
         "        return;\n    }\n\n    i = 0;\n\n"
         "    while ((temp_v0_2 = Field_FindActorAboveGimmick",
         "the top search can have no object; retain preceding stack updates before ending the actor scan"),
    ],
    "F00947": [("    Actors_ActorRecord* actor = Actors_actorRecords[id];",
                 "    Actors_ActorRecord* actor = Actors_actorRecords[id];\n"
                 "    if (actor == NULL) {\n"
                 '        Core_Debugf("actors", "movement skips absent actor=%d", id);\n'
                 "        return;\n    }",
                 "retain optional actor no-op before world-route position/facing access")],
    "F00951": [
        ("    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];",
         "    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];\n"
         "    if (temp_s1 == NULL) {\n"
         '        Core_Debugf("actors", "absolute slide skips absent actor=%d", arg0);\n'
         "        return;\n    }",
         "func_800A9D90: retain the primitive's optional-actor no-op contract before position/rate access"),
    ],
    "F01353": [("((u_long)primBuf << 8) >> 8", "LegacyGpu_Handle(primBuf)", "stat-gauge native packet link")],
    "F01348": [
        ("*(void**)((uintptr_t)Core_ScratchBase() + 0x008) + 8",
         "(u_long*)Render_frameScratch.overlayOrderingTable + 2",
         "menu row overlay pointer uses the native field; the original +8 is a byte offset"),
        ("*(void**)((uintptr_t)Core_ScratchBase() + 0x000)",
         "Render_frameScratch.primitiveCursor",
         "menu row packets use the native frame cursor"),
        ("(u_int)((uintptr_t)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
         "menu row gradient links retain native packet handles"),
    ],
        "F01349": [
           ("VS::Core::Memory::ZeroWordsBackward(Menu_Framework_menuItems, 0xB24)",
            "VS::Core::Memory::ZeroWordsBackward(Menu_Framework_menuItems, sizeof(Menu_Framework_MenuRow) * 40 + 0x124)",
            "clear all forty native rows and the original 0x124-byte scratch tail"),
        ],
        "F01367": [
           ("VS::Core::Memory::AllocateFromHighEnd(0xB24)",
            "VS::Core::Memory::AllocateFromHighEnd(sizeof(Menu_Framework_MenuRow) * 40 + 0x124)",
            "native menu rows contain a full subtext pointer and outgrow the PS1 workspace"),
           ("miscIndex + 0xA00", "(char*)((Menu_Framework_MenuRow*)miscIndex + 40)",
            "string scratch follows all forty native menu rows"),
           ("miscIndex + 0xA60", "(int*)(Menu_Framework_stringBuf + 0x60)",
            "row-type scratch follows the original 96-byte string scratch"),
        ],
        "F02548": [
           ("((u_long)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
            "scrolling-row gradient links retain native packet handles"),
        ],
    "F01416": [("((uintptr_t)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
                 "textbox ordering entries hold native link handles, never truncated address bits")],
    "F02456": [
        ("((uintptr_t)var_t2 << 8) >> 8", "LegacyGpu_Handle(var_t2)",
         "SCREFF2.PRG func_800F986C offscreen feedback link must retain the native packet handle"),
        ("((u_long)var_t2 << 8) >> 8", "LegacyGpu_Handle(var_t2)",
         "SCREFF2.PRG func_800F986C framebuffer restore link must retain the native packet handle"),
    ],
    **{fid: [("((u_long)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
              "shared primitive helpers use the native link table")] for fid in ("F01405", "F01406")},
    "F03423": [
        ("    while (DsControlB(DslPause, NULL, NULL) == 0)\n        ;\n", "",
         "main.c:8119; native extracted-file reads have no CD transport to pause"),
        ("void Boot_ResetToTitle(void)\n{",
         "void Boot_ResetToTitle(void)\n{\n    Save_NativeResetSession();",
         "main.c:8119 cancel staged native resume before resetting live services"),
    ],
    "G00497": [("u_long Menu_textBoxOrderingTables", "u_long Menu_textBoxOrderingTables[102]",
                 "5BF94.c:4044-4046; three 34-word textbox ordering tables")],
    # D4.c:1955 declares two colors, but D4.c:1970 draws from &D_800DBA7C[1], which reads a third. The data
    # runs 12 bytes to D_800DBA88 (G00810) and holds {128,96,64} there; issue #259.
    "G00809": [("CVECTOR Ending_congratulationsTint[2]", "CVECTOR Ending_congratulationsTint[3]",
                 "D4.c:1970 reads D_800DBA7C[2]; 12 bytes to G00810 (issue #259)")],
    "G01780": [("Render_TextureRegion Menu_Results_disMap[]", "Render_TextureRegion Menu_Results_disMap[127]",
                 "3B8.c:1162 _texture_t[127]; the typed texture draws take bounds-checked atlas spans")],
    # Animation-header outputs are native pointers, not PS1-sized integers.
    "F00819": [
        ("    int sp10[2];", "    u_short* sp10;", "native animation-header output"),
        ("VS::Actors::FindLiveActorAnimationHeader(temp_s2, var_s0, sp10);",
         "VS::Actors::FindLiveActorAnimationHeader(temp_s2, var_s0, &sp10);", "use the declared output contract"),
        ("if (sp10[0] == 0)", "if (sp10 == NULL)", "test the complete native pointer"),
    ],
    "F00823": [
        ("    int sp20;", "    u_short* sp20;", "native animation-header output"),
    ],
    "F00833": [
        ("    int animation;", "    u_short* animation;", "native animation-header output"),
    ],
    "F00834": [
        ("    int sp10;", "    u_short* sp10;", "native animation-header output"),
    ],
    "F02995": [
        ("    char buffer[1024];", "    static char buffer[1024];",
         "retain manual text until the deferred textbox update and render"),
    ],
    # MENUE 494.c:577-988 shares the native frame, not two four-byte scratch words.
    "F04163": [
        ("    u_long** s0;", "    Render_FrameScratch* s0;", "typed manual frame"),
        ("s0 = (u_long**)Core_ScratchAddr(0);", "s0 = &Render_frameScratch;", "native frame owner"),
        ("s0[1]", "((u_long*)s0->orderingTable)", "manual control-label ordering"),
    ],
    "F02997": [
        ("    void** q;", "    Render_FrameScratch* q;", "typed clipping frame"),
        ("void** p = (void**)Core_ScratchAddr(0);",
         "Render_FrameScratch* p = &Render_frameScratch;", "native frame owner"),
        ("q = (void**)Core_ScratchAddr(0);", "q = &Render_frameScratch;", "native frame owner"),
        *[(f"{name}[{index}]", f"{name}->{member}", "typed manual clipping packet state")
          for name in ("p", "q")
          for index, member in ((0, "primitiveCursor"), (1, "orderingTable"))],
    ],
    "F03000": [
        ("    u_long** scratch;", "    Render_FrameScratch* scratch;", "typed illustration frame"),
        ("sprite = *((void**)((uintptr_t)Core_ScratchBase() + 0x000));",
         "sprite = Render_frameScratch.primitiveCursor;", "full native sprite cursor"),
        ("scratch = (void*)Core_ScratchAddr(0);", "scratch = &Render_frameScratch;", "native frame owner"),
        ("scratch[0]", "scratch->primitiveCursor", "native sprite cursor handoff"),
        ("scratch[1]", "scratch->orderingTable", "native sprite ordering"),
    ],
    **{fid: [
        ("    u_long** p;", "    Render_FrameScratch* p;", "typed manual panel frame"),
        ("p = (u_long**)Core_ScratchAddr(0);", "p = &Render_frameScratch;", "native frame owner"),
        ("p[0]", "p->primitiveCursor", "native panel cursor handoff"),
        ("p[1]", "((u_long*)p->orderingTable)", "native panel ordering"),
    ] + cursor for fid, cursor in (
        ("F03001", [("*(POLY_G4**)Core_ScratchAddr(0)", "Render_frameScratch.primitiveCursor",
                     "full native page-background cursor")]),
        ("F03004", [("*Core_ScratchAddr(0)", "Render_frameScratch.primitiveCursor",
                     "full native page-arrow cursor")]),
        ("F03007", [
            ("*Core_ScratchAddr(0)", "Render_frameScratch.primitiveCursor", "full native controls cursor"),
            ("*(u_long**)Core_ScratchAddr(1)", "((u_long*)Render_frameScratch.orderingTable)",
             "ordering table follows a native pointer, not a four-byte word"),
        ]),
    )},
    **{fid: [
        ("*(void**)Core_ScratchAddr(0)", "Render_frameScratch.primitiveCursor", "native fade cursor"),
        ("*(u_long**)Core_ScratchAddr(1)", "((u_long*)Render_frameScratch.orderingTable)",
         "native fade ordering"),
    ] for fid in ("F03002", "F03003")},
    "F03005": [
        ("*(LINE_F2**)Core_ScratchAddr(0)", "Render_frameScratch.primitiveCursor", "native diagram cursor"),
        ("*(void**)Core_ScratchAddr(0)", "Render_frameScratch.primitiveCursor", "native diagram cursor handoff"),
        ("*(void**)Core_ScratchAddr(1)", "Render_frameScratch.orderingTable", "native diagram ordering"),
    ],
    # func_800AEF94 / 44F14.c:923: room actor slots are sparse, including an absent companion in slot 1.
    "F04146": [
        ("VS::Menu::Files::UpdateContainerSave(submenuState);",
         "VS::Menu::Files::UpdateLiveContainerSave(*submenuState);",
         "C48.c:334 the container save submenu runs through its typed live binding"),
    ],
        "F01006": [("*(MATRIX*)&source", "*source", "44F14.c:853; copy the supplied camera matrix, not pointer-stack bytes")],
    # #276 row 6 (render GIM images): the event scripts reach the typed GIM set (GimImagesLive.hpp) instead of
    # viewing its PS1 layer records. 4A0A8.c:609 read the pending bit through a possibly missing set.
    "F01031": [
        ("{ return Render_gimLayers->isLoadPending; }", "{ return VS::Render::LiveGimImageLoading(); }",
         "the load-pending query goes through the typed set, which reports a missing set instead of reading it"),
    ],
    # 4A0A8.c:857-877: the tween stops and the layer takes the script's routine and placement, then activates.
    "F01044": [
        ("    Event_SplashTween* new_var;\n\n"
         "    Event_SplashLayerView* temp_a1 = Render_gimLayers;\n\n"
         "    if (temp_a1 == NULL) {\n"
         "        return 0;\n"
         "    }\n\n"
         "    new_var = &Event_splashTweens[arg0[1] >> 6];\n"
         "    new_var->durationFrames = 0;\n"
         "    temp_a1 = temp_a1 + ((arg0[1] >> 6));\n"
         "    temp_a1->renderMode = arg0[1] & 3;\n"
         "    temp_a1->offsetX = (signed char)arg0[2] * 2;\n"
         "    temp_a1->offsetY = (signed char)arg0[3] * 2;\n"
         "    temp_a1->scaleFixed12 = arg0[4] << 7;\n"
         "    temp_a1->brightness = arg0[5];\n"
         "    VS::Render::ActivateGimLayer(arg0[1] >> 6, temp_a1, arg0);",
         "    if (!VS::Render::LiveGimImagesPresent()) {\n"
         "        return 0;\n"
         "    }\n\n"
         "    Event_splashTweens[arg0[1] >> 6].durationFrames = 0;\n"
         "    VS::Render::ConfigureLiveGimLayer(arg0[1] >> 6, arg0[1] & 3, (signed char)arg0[2] * 2,\n"
         "                                      (signed char)arg0[3] * 2, arg0[4] << 7, arg0[5]);",
         "configure and activate the layer through the typed set; a fourth layer index is refused there"),
    ],
    # 4A0A8.c:879-903: the tween starts from the layer's placement. The original read a fourth layer past the
    # set and its tween was never advanced (F01212 tweens three), so the script waited forever; it is skipped.
    "F01045": [
        ("    Event_SplashLayerView* temp_a3 = Render_gimLayers;\n"
         "    int temp_a2 = arg0[5] >> 6;\n"
         "    Event_SplashTween* temp_a1 = &Event_splashTweens[temp_a2];\n\n"
         "    if (temp_a3 == NULL) {\n"
         "        return 0;\n"
         "    }\n\n"
         "    temp_a3 += temp_a2;\n\n",
         "    int temp_a2 = arg0[5] >> 6;\n"
         "    Event_SplashTween* temp_a1 = &Event_splashTweens[temp_a2];\n"
         "    const auto splash_layer = VS::Render::LiveGimLayerPlacement(temp_a2);\n\n"
         "    if (!splash_layer) {\n"
         "        return 0;\n"
         "    }\n\n",
         "the tween reads the typed placement; no set or a fourth layer continues the script"),
        ("(temp_a1->startX = temp_a3->offsetX)", "(temp_a1->startX = splash_layer->offset_x)",
         "start X from the typed placement"),
        ("(temp_a1->startY = temp_a3->offsetY)", "(temp_a1->startY = splash_layer->offset_y)",
         "start Y from the typed placement"),
        ("(temp_a1->startScaleFixed12 = temp_a3->scaleFixed12)",
         "(temp_a1->startScaleFixed12 = splash_layer->scale_and_order)",
         "start scale (with the scene-order flag, as the original's halfword) from the typed placement"),
        ("(temp_a1->startBrightness = ((char*)temp_a3)[3])", "(temp_a1->startBrightness = splash_layer->brightness)",
         "the original read the layer's unsigned brightness byte (offset 3)"),
    ],
    # Actor IDs are 0..15 (146C.c:8936); 0x10 is the linked-actor sentinel, not a table index.
    # 7E05C.s:541-575 identifies the next BSS symbol, tileFlagMap at 0x800F58B8: 0x40 bytes after D_800F5878.
    "G00130": [
        ("Event_kOrbitReturnFirstCommand", "Event_kOrbitReturnFirstCommand[4]",
         "BATTLE.PRG 0x800E9C28 stores D4 10 10 55; preserve the complete camera command payload"),
    ],
    "G00131": [
        ("Event_kOrbitReturnSecondCommand", "Event_kOrbitReturnSecondCommand[3]",
         "BATTLE.PRG 0x800E9C2C stores C2 01 55; preserve the complete camera command payload"),
    ],
    "G00574": [
        ("Effects_effectRenderers[]", "Effects_effectRenderers[8]",
         "eight native function pointers occupy the original table range 0x800F56A8..0x800F56C8"),
    ],
    "G00579": [("Battle_Ai_actorAiStates[]", "Battle_Ai_actorAiStates[16]", "AI actor pointer table extent")],
    # main.c:9042 declares the disc ring buffer as heap-header pointers only to step it in 16-byte units (128 per
    # 2048-byte sector). Natively it is a byte pointer: the heap header type is retired (F03463-F03467 #276) and an
    # x64 header stride would have stepped 3072 bytes per sector.
    "G01921": [("Core_HeapBlockHeader* Content_legacyDiscReadBuffer", "u_char* Content_legacyDiscReadBuffer",
                "F03468 #276 the ring buffer is bytes")],
    "G00614": [
        ("char const Battle_puzzleTimerText[]", "char Battle_puzzleTimerText[9]",
         "5BF94.c:3842 the timer rewrites six digits; retain nine bytes of mutable native storage, including separators and terminator"),
    ],
    "G00623": [
        ("Effects_effectEntryPointIndices[]", "Effects_effectEntryPointIndices[400]",
         "publish the catalogued 400-index plugin slice boundary for bounded native consumers"),
    ],
    # BATTLE 5BF94.c:7885/8442: the timeline header stays eight bytes, but tracks contain native pointers.
    "F01557": [
        ("entry->trackCount * 16 + 8", "sizeof(Effects_EffectTimeline) + entry->trackCount * sizeof(Effects_TimelineTrack)",
         "allocate native timeline tracks"),
        ("    p->flags = count ? 2 : 0;", "    p->flags = count ? 2 : 0;\n"
         '    Core_Debugf("effects", "timeline=%d state=%p tracks=%d stride=%u", arg1, (void*)p,\n'
         "        (int)p->trackCount, (unsigned int)sizeof(Effects_TimelineTrack));", "timeline allocation diagnosis"),
    ],
    "F01589": [
        ("(Effects_TimelineTrack*)&arg0->context.timeline[var_s1 * 2 + 1]", "&arg0->context.timeline->tracks[var_s1]",
         "index native tracks, not PS1 eight-byte header units"),
    ],
    # BATTLE 573B8.c:67: opcode results are control codes 0..4 or native instruction addresses.
    "F01271": [
        ("    int ret;", "    intptr_t ret;", "preserve opcode branch addresses"),
        ("        Event_isSkipForced = 0;",
         '        Core_Debugf("event", "skip event=%d source=%d thread=%d start=%u forced=%d",\n'
         "            (int)Event_eventId, (int)Event_eventSource, (int)arg1,\n"
         "            (unsigned)((Input_buttonsPressed.allButtons & PADstart) != 0), (int)Event_isSkipForced);\n"
         "        Event_isSkipForced = 0;",
         "preserve the actual user/forced skip reason before the source resets it"),
        ("    while (1) {\n        if (Event_skipState != 2)",
         "    while (1) {\n"
         "        uint64_t scene_command = Diagnostics_RecordSceneCommand(\n"
         "            (int32_t)Event_eventId, (int32_t)Event_eventSource, (int32_t)arg1,\n"
         "            script, Event_scriptBuffer, Event_kOpcodeSizes[*script], (int32_t)Event_skipState);\n"
         "        if (Event_skipState != 2)",
         "capture full instruction provenance before the handler changes script context"),
        ("        switch (ret) {",
         "        Diagnostics_RecordSceneResult(scene_command, (int64_t)ret);\n"
         '        Core_Debugf("event-step", "event=%d thread=%d op=%02X result=%lld args=%02X,%02X,%02X pc=%p",\n'
         "            (int)Event_eventId, (int)arg1, (unsigned)*script, (long long)ret,\n"
         "            Event_kOpcodeSizes[*script] > 1 ? (unsigned)script[1] : 0,\n"
         "            Event_kOpcodeSizes[*script] > 2 ? (unsigned)script[2] : 0,\n"
         "            Event_kOpcodeSizes[*script] > 3 ? (unsigned)script[3] : 0, (void*)script);\n"
         "        switch (ret) {", "toggleable event wait and command argument diagnostics"),
        ("        default:\n            script = (u_char*)ret;",
         '        default:\n            Core_Debugf("event", "thread=%d branch=%p -> %p skip=%d",\n'
         "                (int)arg1, (void*)script, (void*)ret, (int)Event_skipState);\n"
         "            script = (u_char*)ret;", "native branch trace"),
    ],
    "F01259": [
        ("    flags[id] = value;",
         "    const unsigned char previous_flag = (unsigned char)flags[id];\n"
         "    flags[id] = value;\n"
         "    Diagnostics_RecordScriptFlagWrite((int32_t)id, previous_flag, (unsigned char)value);",
         "record ordered actual flag writes without changing their byte semantics"),
    ],
    "G00133": [
        ("int (*", "intptr_t (*", "normal opcode return contract"),
        ("Event_kOpcodeHandlers[]", "Event_kOpcodeHandlers[256]", "one handler per byte opcode"),
    ],
    **{rid: [(f"{name}[]", f"{name}[{count}]", evidence)] for rid, name, count, evidence in (
        ("G00029", "Battle_attackPrerequisiteFunctions", 40,
         "146C.c:_canPerformAttack; US table has forty functions before the hit-factor words"),
        ("G00032", "Battle_actionEffectMediators", 70,
         "146C.c:func_80085390; US table ends after the seventy effect mediators"),
        ("G00138", "Battle_limbNames", 20,
         "5BF94.h:limb-name strings; twenty US addresses precede signed coordinate pairs"),
        ("G00222", "Effects_scriptRenderers", 2,
         "5BF94.c:func_800D47F4; renderer and null entry precede D_800EC32C byte selectors"),
        ("G00227", "Effects_scriptOpcodeHandlers", 49,
         "5BF94.c:func_800D51D8; forty-nine US handlers precede effect resource data"),
        ("G00233", "Battle_Ai_decisionCallbacks", 9,
         "7CD68.c:decision callback selection; nine US function addresses precede AI constants"),
        ("G01840", "Audio_Seq_kLfoWaveforms", 256,
         "25AC.c:LBU selector covers a byte; original MUSIC172 selects65 and aliases adjacent source pointer entries as sample data"),
        ("G01841", "Audio_Mixer_kCommandHandlers", 256,
         "25AC.c:SoundCommand; byte command table ends at D_800305E0"),
        ("G01842", "Audio_Seq_kOpcodeHandlers", 96,
         "25AC.c:func_8001A8D8 indexes byte opcode minus 0xA0; table ends at D_80030760"),
        ("G01843", "Audio_Seq_kExtendedOpcodeHandlers", 32,
         "25AC.c:func_8001A8D8 extended dispatch; thirty-two handlers precede numeric data"),
    )},
    "G00466": [("int (**", "intptr_t (**", "copied skip opcode return contract")],
    "G00621": [
        ("int (*", "intptr_t (*", "skip opcode return contract"),
        ("Event_kSkipOpcodeHandlers[]", "Event_kSkipOpcodeHandlers[256]", "one handler per byte opcode"),
    ],
    # INITBTL 12AC.c:69 copied 256 four-byte pointers; native pointers require the full table size.
    "F01850": [
        ("0x400", "sizeof(Event_kSkipOpcodeHandlers)",
         "native skip table allocation and copy"),
        ("    Event_isAmbientSoundSuspended = 0;",
         "    Event_isAmbientSoundSuspended = 0;\n"
         "    Event_playbackPhase = 0;\n"
         "    Event_isStartConfirmed = 0;\n"
         "    Event_isMainThreadFinished = 0;\n"
         "    VS::Core::Memory::ZeroAlignedWords(Event_threadCursors, sizeof Event_threadCursors);\n"
         "    VS::Core::Memory::ZeroAlignedWords(Event_threadWaitFrames, sizeof Event_threadWaitFrames);",
         "native globals survive overlay loads; a new gameplay session must not resume old event threads"),
    ],
    # BATTLE 4A0A8.c:2110 is called only through the opcode table; it ignores both dispatch arguments.
    "F01126": [("Event_ScriptWaitForScreenTransition(void)",
                 "Event_ScriptWaitForScreenTransition(u_char* arg0, short arg1)", "consistent opcode signature")],
    # 771A8.c:942 adds a direction delta to `arg0->unk34`, an int in the pinned view; the merged layout of
    # T00532 has the packed tile word as Battle::Ai::TilePosition (union of the word and its bytes).
    "F01731": [("arg0->tile + ", "arg0->tile.word + ", "packed tile word arithmetic (771A8.c:950)")],
    # BATTLE 5BF94.c:1332 func_800C64D0 walks and splices an ordering-table list by raw 24-bit addresses (mask kept
    # in a variable, so the hazard index cannot see them as links): links are handles (Legacy/Render/LibGpu.h).
    "F01324": [
        ("intptr_t temp_t0 = ((uintptr_t)(arg0 - 1) & a3);", "u_long temp_t0 = LegacyGpu_Handle(arg0 - 1);",
         "link of the list's head entry"),
        ("arg0 = (u_long*)((*arg0 & a3) | 0x80000000);", "arg0 = (u_long*)LegacyGpu_Pointer(*arg0 & a3);",
         "follow a link"),
        ("((u_long)arg1 & a3)", "LegacyGpu_Handle(arg1)", "link to the spliced packet"),
    ],
    # The game heap is a fixed PS1 RAM range (0x8010C000, 0xF2000 bytes) in three start-up paths; natively the live
    # game heap is reset over Core's arena (Core/Arena.h, HeapLive.hpp).
    **{fid: [("VS::Core::Memory::InitializeHeap((Core_HeapBlockHeader*)0x8010C000, 0xF2000)",
              "VS::Core::Memory::ResetLiveGameHeap()", "F03467 #276 the live game heap spans Core's arena")]
       for fid in ("F01858", "F03437")},
    # vs_overlay_slots (overlay.c) holds the PS1 load addresses of the overlay modules; natively they are offsets in
    # Core's overlay block (Core/Arena.h), which keeps the slots that overlap overlapping.
    "G02018": [(f"(void*)0x{a:08X}", f"Core_OverlayAddress(0x{a:08X}u)", "overlay load address")
               for a in (0x80068800, 0x800F9800, 0x80102800)],
    # main.c:8093 reaches the loading image's pixels as the words after its 4-byte size header (one rgb555Sized
    # asset in the module image); natively the pixels are their own global, filled from the same bytes.
    "F03421": [("(u_long*)&Render_legacyLoadingImageHeader + 1", "Render_loadingScreenPixels",
                "pixels follow the size header")],
    # 40564.c:1769 reads a gimmick instance through raw PS1 offsets: the flag byte at 8 (bit 7 transitionFlag), the
    # palettes at 0x12C/0x14C, and a 4-byte clear at 0x34 (slideX and velocityY).
    "F00980": [("flags = *(u_int*)((char*)arg0 + 8);", "flags = arg0->transitionFlag ? 0x80 : 0;", "flag byte at 8"),
               ("(void*)((char*)arg0 + 0x14C)", "(void*)arg0->pulsedPalette", "palette at 0x14C"),
               ("(void*)((char*)arg0 + 0x12C)", "(void*)arg0->basePalette", "palette at 0x12C"),
               ("(u_short*)((char*)object + 0x14C)", "(u_short*)object->pulsedPalette", "palette at 0x14C"),
               ("*(int*)((char*)object + 0x34) = 0;", "object->slideX = 0;\n            object->velocityY = 0;",
                "clear at 0x34")],
}

# #269: model geometry projection and polygon walks read only a model's recorded extent. Each loader records the
# bytes it placed; each release forgets them before the memory goes back to the heap.
FIXES["F00782"] = FIXES.get("F00782", []) + [
    ("        if (s1->model == NULL) {\n            return -2;\n        }\n",
     "        if (s1->model == NULL) {\n            return -2;\n        }\n"
     "        VS::Render::RecordLiveModelExtent(s1->model, temp_v1_2->modelDataSize);\n",
     "#269: the copied gimmick SHP holds modelDataSize bytes"),
]
FIXES["F00783"] = FIXES.get("F00783", []) + [
    ("        VS::Core::Memory::FreeFromLowEnd(temp_s0->model);",
     "        VS::Render::ForgetLiveModelExtent(temp_s0->model);\n"
     "        VS::Core::Memory::FreeFromLowEnd(temp_s0->model);",
     "#269: the released SHP no longer has an extent"),
]
FIXES.setdefault("F01229", []).append(
    ("VS::Render::ReadLiveFramebufferEffectScale(&values.x)", "VS::Render::ReadLiveFramebufferEffectScale(&values)",
     "#276 slice A5: 4A0A8.c passes the record's first word for the three-word scale getter; the binding takes the "
     "record"))

# #276 row 23 remainder (field-actor-collision): the probe cell is a typed CollisionCellRef the Live layer reads back
# as one word, and the gimmick samples pass their three-halfword position by value.
_PROBE_CELL_WORD = "VS::Field::LiveCollisionProbeCellWord()"
FIXES.setdefault("F00912", []).append(
    ("if ((*Field_collisionProbeTile >> 20) & 1) {",
     f"if (({_PROBE_CELL_WORD} >> 20) & 1) {{",
     "3A1A0.c:708 reads bit 20 of the cell the last support sample found"))
FIXES.setdefault("F00836", []).append(
    ("Field_FindActorAboveGimmick(Field_gimmickInstances[temp_s4], i)",
     "VS::Field::LiveFindActorOccupyingTile("
     "(u_char)Field_gimmickInstances[temp_s4]->tileX, "
     "(u_char)Field_gimmickInstances[temp_s4]->tileZ, "
     "Field_gimmickInstances[temp_s4]->positionY, i)",
     "F00910 forwarding alias retirement: original LBU X/Z and LH support height at caller"))
FIXES.setdefault("F00908", []).append(
    ("func_800A3C00(object, 1)",
     "VS::Field::LiveFindActorOccupyingTile((u_char)object->tileX, "
     "(u_char)object->tileZ, object->positionY, 1)",
     "F00910 forwarding alias retirement: original LBU X/Z and LH support height at caller"))
FIXES["F00782"] = FIXES.get("F00782", []) + [
    ("VS::Field::LiveSampleActorSupportHeight(&s1->positionX, 0, 0, 1)",
     "VS::Field::LiveSampleSupportHeightAt(s1->positionX, s1->positionY, s1->positionZ, 0, 0, 1)",
     "30DB0.c:373 samples from the gimmick's three position halfwords through a short* prototype"),
]
FIXES.setdefault("F00139", []).extend([
    ("Field_interactionFocus.actorQuery.location.position.vx", "Field_interactionFocus.actor_query.location.position.vx", "287 owned actor observation naming"),
    ("Field_interactionFocus.actorQuery.location.position.vy", "Field_interactionFocus.actor_query.location.position.vy", "287 owned actor observation naming"),
    ("Field_interactionFocus.actorQuery.location.position.vz", "Field_interactionFocus.actor_query.location.position.vz", "287 owned actor observation naming"),
])
FIXES.setdefault("F00140", []).extend([
    ("Field_interactionFocus.actorQuery.location.position.vx", "Field_interactionFocus.actor_query.location.position.vx", "287 owned actor observation naming"),
    ("Field_interactionFocus.actorQuery.location.position.vy", "Field_interactionFocus.actor_query.location.position.vy", "287 owned actor observation naming"),
    ("Field_interactionFocus.actorQuery.location.position.vz", "Field_interactionFocus.actor_query.location.position.vz", "287 owned actor observation naming"),
])

FIXES.setdefault("F00853", []).append(
    ("VS::Field::LiveSampleActorSupportHeight(&temp_s0->positionX, 0, 0, 0)",
     "VS::Field::LiveSampleSupportHeightAt(temp_s0->positionX, temp_s0->positionY, temp_s0->positionZ, 0, 0, 0)",
     "30DB0.c:3680 samples from the gimmick's three position halfwords through a short* prototype"))

FIXES.setdefault("F00158", []).append(
    ("    Field_MpdDoorRecord destination;\n",
     "    Field_MpdDoorRecord destination{};\n",
     "#323: the scripted-fall producer leaves unlock/id/reserved fields unassigned, but DoorRecordOf copies them"))

FIXES.setdefault("F00086", []).append(
    ("    Effects_ActionEffectRequest request;\n",
     "    Effects_ActionEffectRequest request{};\n",
     "#324: the trap-tile producer leaves actor endpoint part/padding unassigned, but EndpointOf reads and copies them"))

# #288: these readers dereference func_8008B764's result unchecked, so off the grid, without a cell table or on a
# missing platform layer the original reads PS1 RAM word 0. They read a never-null cell that is kAbsentCellWord then.
_CELL_OR_ABSENT = ("VS::Field::LiveCollisionCellAt(", "VS::Field::LiveCollisionCellOrAbsent(")
for _row, _decl, _where in (
        ("F00158", ("    Field_MpdTileCell* tile;\n", "    const Field_MpdTileCell* tile;\n"),
         "146C.c:5390 reads hasEventTrigger and hasRoomTrigger"),
        ("F00159", ("    Field_MpdTileCell* temp_s4;\n", "    const Field_MpdTileCell* temp_s4;\n"),
         "146C.c:5531 reads hasRoomTrigger"),
        ("F00547", None, "146C.c:15556 reads hasRoomTrigger"),
        ("F00596", ("        Field_MpdTileCell* cell;\n", "        const Field_MpdTileCell* cell;\n"),
         "146C.c:16717 reads isPit and surfaceKind"),
        ("F01308", ("Field_MpdTileCell* room = ", "const Field_MpdTileCell* room = "),
         "58578.c:1060 reads blocksSphereTerrain, doorPassability and occlusionAndFloorFlags")):
    _evidence = f"#288: {_where} of the cell lookup without a NULL check"
    FIXES.setdefault(_row, []).extend(
        ([(*_decl, _evidence)] if _decl else []) + [(*_CELL_OR_ABSENT, _evidence)])

_SCALAR_MATRIX_CALLS = ("MultiplyMatrixIntoRight|MultiplyMatrixIntoLeft|MultiplyRotationMatrixInPlace|"
                        "PremultiplyRotationMatrix|BuildEulerRotationMatrix(?:Quadrant)?|ScaleMatrixColumns")

MOVEMENT_BINDING = ("the movement binding supplies the actor AI state, battle actors, actor records, patrol routes"
                    " and frame terrain")
CLEANUP_BINDING = (
    "the cleanup binding supplies the actor AI state table, room AI state, tile flag grid, battle actors,"
    " actor records and heap")
_GROUND_BINDING = "the ground binding supplies the actor AI state, AI frame maps, actor list and movement code"

REGEX_FIXES: list[tuple[str, str, str, str]] = [
    *(
        (r"", rf"\bVS::Core::Math::({_SCALAR_MATRIX_CALLS})\({first}, {second}\);",
         rf"VS::Core::Math::Apply\1({first_out}, {second_out});",
         "F00761/F00763/F01018/F01019/F03403/F03406/F03415 #46 typed promotion: matrix records are passed by "
         "reference to the Mat3 adapters instead of as raw MATRIX/SVECTOR/VECTOR pointers")
        for first, first_out, second, second_out in (
            (r"&([^,;]+)", r"\2", r"&([^;]+)", r"\3"),
            (r"&([^,;]+)", r"\2", r"([^&;][^;]*)", r"*\3"),
            (r"([^&,;][^,;]*)", r"*\2", r"&([^;]+)", r"\3"),
            (r"([^&,;][^,;]*)", r"*\2", r"([^&;][^;]*)", r"*\3"),
        )
    ),
    (r"", r"\bVS::Core::Math::(InterpolateLiveSevenBit(?:Vector|Plane))\(&(\w+->\w+\[\d+\])\[0\]",
     r"VS::Core::Math::\1(\2",
     "F01458/F01459/F01460 #276 A3: a pair range is passed as its fixed-size short array, so the binding's span "
     "keeps its extent, instead of as a pointer to its first halfword"),
    (r"", r"\bVS::Core::Math::SetLiveIdentityRotation\(\(void\*\)&(\w+->facing)\)",
     r"VS::Core::Math::SetLiveIdentityRotation(&\1)",
     "F04132 #276 A3: effectTransformState.facing is a MATRIX, so the record binding takes it without the "
     "decomp's untyped cast"),
    (r"^Battle::Ai::", r"\bVS::Field::GroundHeight(Flags)?AtPackedTile\(((?:[^()]|\([^()]*\))*)\)",
     r"VS::Field::GroundHeight\1AtPackedTile(VS::Battle::Ai::CurrentTerrain(), "
     r"VS::Core::Math::UnpackTileCoordinate(static_cast<std::uint32_t>(\2)))",
     "F01760/F01762 #46 typed promotion: the AI tile word is decoded by the caller and the frame's terrain grid "
     "is passed explicitly instead of read from a hidden global"),
    (r"^Battle::Ai::", r"\bVS::Field::(GroundHeight(?:Flags)?At(?:Tile|WorldPosition)|IsPositionAboveGround)\(",
     r"VS::Field::\1(VS::Battle::Ai::CurrentTerrain(), ",
     "F01759/F01761/F01763/F01764/F01767 #46 typed promotion: the frame's terrain grid is passed explicitly"),
    (r"^Battle::Ai::", r"\bVS::Field::IsTerrainSegmentBlocked\((\w+), (\w+), ",
     r"VS::Field::IsTerrainSegmentBlocked(VS::Battle::Ai::CurrentTerrain(), VS::Core::Math::PositionOf(*\1), "
     r"VS::Core::Math::PositionOf(*\2), ",
     "F01768 #46 typed promotion: endpoints are read as Position3 values instead of SVECTOR pointers"),
    (r"^(Battle::Ai|Field)::", r"\bVS::Battle::Ai::(InitializeRoomAiMap|FreeRoomAiState)\(\)",
     r"VS::Battle::Ai::\1(VS::Battle::Ai::BattleRoomAiRuntime())",
     "F01837/F01838 #46 typed promotion: the room AI lifecycle takes the runtime that owns the published map "
     "instead of reaching hidden room and actor globals"),
    (r"^Battle::Ai::", r"\bVS::Battle::Ai::LowestReachableGroundHeight\(\(void\s*\*\)(\w+), ",
     r"VS::Battle::Ai::LowestReachableGroundHeight(VS::Battle::Ai::ActorFlightNeighborhood("
     r"*(Battle_Ai_ActorAiState *)((void*)\1)), ",
     "F01731 #46 typed promotion: the flight-state view is bound once to the actor's neighbourhood (frame terrain, "
     "direction steps and cached flight test) instead of the query reading frame globals"),
    (r"^Battle::Ai::", r"\bVS::Battle::Ai::LowestReachableGroundHeight\((\w+), ",
     r"VS::Battle::Ai::LowestReachableGroundHeight(VS::Battle::Ai::ActorFlightNeighborhood(*\1), ",
     "F01731 #46 typed promotion: callers holding the actor AI state pass its neighbourhood explicitly"),
    (r"^Battle::Ai::", r"\bVS::Battle::Ai::MinimumAiGroundHeightAlongSegment\(",
     r"VS::Battle::Ai::MinimumAiGroundHeightAlongSegment(VS::Battle::Ai::CurrentTerrain(), ",
     "F01830 #46 typed promotion: the frame's terrain grid is passed explicitly"),
    (r"^Actors::", r"\bVS::Battle::Ai::RoomNavigationCeilingHeight\(\)",
     r"VS::Battle::Ai::RoomNavigationCeilingHeight(VS::Battle::Ai::BattleRoomAiRuntime())",
     "F01843 #46 typed promotion: the ceiling is read from the runtime that publishes the room AI map"),
    (r"^Battle::", r"\bVS::Battle::Ai::ResetActorAiReactionDelay\(",
     r"VS::Battle::Ai::ResetActorAiReactionDelay(VS::Battle::Ai::BattleActorReactionDelays(), ",
     "F01791 #46 typed promotion: the delay is written through the battle actor AI table adapter"),
    (r"", r"\bVS::Actors::(SetDirectMovement|QueueNavigationTileTarget)\(",
     r"VS::Actors::\1(VS::Actors::ActorRecordMovementRequests(), ",
     "F04180/F01025 #46 typed promotion: movement commands are written through the actor table adapter"),
    *(
        (r"", rf"\bVS::Actors::{core}\(", rf"VS::Actors::{_MOTION_REQUEST_LIVE[core]}(",
         f"{fid} #46 typed promotion: {why}")
        for fid, core, why in (
            ("F00989", "SaveActorGroundedTransform",
             "the actor binding stores the typed grounded history in the actor record"),
            ("F04178", "BeginTileJump",
             "the actor binding copies the packed tile target before the plain jump starts"),
            ("F04179", "BeginAirborneTileJump",
             "the actor binding copies the packed tile target before the airborne jump starts"),
            ("F01024", "BeginNavigationJump",
             "the actor binding copies the packed tile target before the navigation jump starts"),
            ("F04184", "BeginTeleport",
             "the actor binding resolves each companion through the bounded actor table"),
            ("F04185", "BeginTileTravel",
             "the actor binding copies the eight-byte travel route before the tile pose is set"),
            ("F04186", "BeginTimedTravel",
             "the actor binding copies the eight-byte travel route before the timed pose is set"),
            ("F04181", "QueueGroundOffset",
             "the actor binding reads the step as a typed vector and refuses a missing one"),
            ("F04182", "QueueMovementVector",
             "the actor binding reads the step as a typed vector and refuses a missing one"),
            ("F04183", "QueueFlightMovement",
             "the actor binding reads the step as a typed vector and refuses a missing one"),
            ("F01026", "TurnNavigationActor",
             "the actor binding picks the turn rate from the actor's collision radius"),
            ("F04187", "TurnActorAtSpeed",
             "the actor binding refuses a zero angular speed before the turn consumer divides by it"),
        )
    ),
    *(
        (r"", rf"\bVS::Menu::Files::{core}\(", rf"VS::Menu::{_FILE_MENU_LIVE[core]}(",
         f"{fid} #46 typed promotion: the picker binding runs one frame over the menu globals, controller and "
         f"native save files and keeps the original tri-state result")
        for fid, core in (
            ("F02764", "UpdateDataSaveMenu"),
            ("F02767", "UpdateDataLoadMenu"),
            ("F03628", "UpdateLoadMenu"),
            ("F03634", "UpdateSaveMenu"),
        )
    ),
    (r"^Battle::", r"\bVS::Battle::Ai::CaptureActorConditionTable\((\w+)\);",
     r"VS::Battle::Ai::CaptureBattleActorConditions("
     r"std::span<Battle_Ai_ActionTiming, VS::Battle::Ai::kBattleActorSlotCount>("
     r"\1, VS::Battle::Ai::kBattleActorSlotCount));",
     "F01689 #46 typed promotion: the planner rows are bound as an explicit 16-slot span and the live "
     "battle actor list is read by the binding"),
    (r"", r"\bVS::Field::SampleCollisionHeightCode\(",
     r"VS::Field::SampleCollisionHeightCode(VS::Field::LiveRoomCollisionMap(), ",
     "F00772 #46 typed promotion: height samples read the live room collision map as checked bytes"),
    (r"", r"\bVS::Audio::Seq::ReleaseSavedVoiceConflicts\(\)",
     r"VS::Audio::Seq::ReleaseLiveSavedVoiceConflicts()",
     "F03198 #46 typed promotion: the sequencer binding supplies the live music layers and keys off "
     "the released voices"),
    (r"", r"\bVS::Audio::Driver::ResetPlaybackState\(\)",
     r"VS::Audio::Driver::ResetLivePlaybackState()",
     "F03160 #46 typed promotion: the driver binding applies the reset to the live sequencer, mixer, stream and channel tables"),
    (r"", r"\bVS::Audio::Driver::KeyOnVoices\(", r"VS::Audio::Driver::KeyOnLiveVoices(",
     "F03163 #46 typed promotion: the voice binding keys on the native voice engine's voices in the 24-voice mask"),
    (r"", r"\bVS::Audio::Driver::KeyOffVoices\(", r"VS::Audio::Driver::KeyOffLiveVoices(",
     "F03164 #46 typed promotion: the voice binding keys off the native voice engine's voices in the 24-voice mask"),
    (r"", r"\bVS::Audio::Driver::SetVoiceReverbMask\(", r"VS::Audio::Driver::SetLiveVoiceReverbMask(",
     "F03165 #46 typed promotion: the voice binding routes the masked voices to the native reverb"),
    (r"", r"\bVS::Audio::Driver::SetVoiceNoiseMask\(", r"VS::Audio::Driver::SetLiveVoiceNoiseMask(",
     "F03166 #46 typed promotion: the voice binding switches the masked voices to the native noise source"),
    (r"", r"\bVS::Audio::Driver::SetVoiceFrequencyModulationMask\(",
     r"VS::Audio::Driver::SetLiveVoiceFrequencyModulationMask(",
     "F03167 #46 typed promotion: the voice binding modulates the masked voices except voice 0"),
    (r"", r"\bVS::Audio::Driver::SetVoiceVolume\(", r"VS::Audio::Driver::SetLiveVoiceVolume(",
     "F03168 #46 typed promotion: the voice binding refuses a voice outside the 24 SPU voices"),
    (r"", r"\bVS::Audio::Driver::SetVoiceSampleRate\(", r"VS::Audio::Driver::SetLiveVoiceSampleRate(",
     "F03169 #46 typed promotion: the voice binding refuses a voice outside the 24 SPU voices or a rate outside "
     "the 16-bit pitch register"),
    (r"", r"\bVS::Audio::Driver::SetVoiceReleaseRateAndMode\(",
     r"VS::Audio::Driver::SetLiveVoiceReleaseRateAndMode(",
     "F03178 #46 typed promotion: the voice binding refuses a voice outside the 24 SPU voices or a release rate "
     "or mode outside its ADSR field"),
    (r"", r"\bVS::Audio::Samples::UploadBank\(", r"VS::Audio::Samples::UploadLiveBank(",
     "F03159 #46 typed promotion: the bank binding reads the 32-byte AKAO header and binds the decoded bank "
     "assets to the validated instrument run"),
    (r"", r"\bVS::Audio::UnbindRoomCutsceneStream\(\s*\)", r"VS::Audio::UnbindLiveRoomCutsceneStream()",
     "F03546 #46 typed promotion: the stream binding stops the room stream's voices before clearing its slot"),
    (r"", r"\bVS::Effects::UpdateTrailEmitter\b", r"VS::Effects::UpdateLivePlg074TrailEmitter",
     "F01994 #46 typed promotion: the renderer table and plugin update bind PLG074's slot entry, which checks "
     "the emitter against the current package's recorded extents before running the typed lifecycle (#271)"),
    (r"", r"\bVS::Actors::AdvanceSkeletalAnimation\((\w+), &\1->pose, ",
     r"VS::Actors::AdvanceLiveSkeletalAnimation(*\1, ",
     "F04128 #46 typed promotion: the actor binding steps the actor's own pose and bounds its SEQ reads by the "
     "bank's recorded extent (#261)"),
    (r"", r"\bVS::Actors::BlendActorPose\(\s*&(\w+)->blendPose, &\1->pose, ",
     r"VS::Actors::BlendLiveActorPose(\1->blendPose, \1->pose, ",
     "F01016 #46 typed promotion: the pose binding blends typed keys and skips zero-length blends (#261)"),
    (r"", r"\bVS::Render::QueueDefaultSprite\(", r"VS::Render::QueueLiveDefaultSprite(",
     "F04190 #46 typed promotion: the sprite binding bounds packets by the frame primitive buffer (#265)"),
    (r"", r"\bVS::Render::QueueSpriteWithTexturePage\(", r"VS::Render::QueueLiveSpriteWithTexturePage(",
     "F04191 #46 typed promotion: the sprite binding bounds packets by the frame primitive buffer (#265)"),
    (r"", r"\bVS::Render::QueueSprite\(", r"VS::Render::QueueLiveSprite(",
     "F04192 #46 typed promotion: the sprite binding bounds packets by the frame primitive buffer (#265)"),
    (r"", r"\bVS::Render::UpdateRoomGroupTransforms\((\w+)\)",
     r"VS::Render::UpdateLiveRoomGroupTransforms(\1, Field_roomData.header.geometryLength)",
     "F00758 #46 typed promotion: the room binding rebuilds group views inside the geometry section's recorded "
     "length from the frame camera"),
    (r"", r"\bVS::Render::DrawRoomGeometry\((\w+), ",
     r"VS::Render::DrawLiveRoomGeometry(\1, Field_roomData.header.geometryLength, ",
     "F00744 #46 typed promotion: the room binding draws inside the geometry section's recorded length and a "
     "64 KiB frame-buffer batch"),
    (r"", r"\bVS::Render::TintRoomGeometryPrimitives\(([^,;]+), ([^,;]+), &(\w+), ",
     r"VS::Render::TintLiveRoomGeometryPrimitives(\1, \2, \3, ",
     "F00769 #46 typed promotion: the room binding tints only packets inside the current frame buffer"),
    (r"", r"\bVS::Render::GetProjectionScreenOffset\(&(\w+)\);",
     r"\1 = VS::Render::GetLiveProjectionScreenOffset();",
     "F00773 #46 typed promotion: the screen offset is returned by value"),
    (r"", r"\bVS::Render::SetProjectionScreenOffset\(&(\w+)\)",
     r"VS::Render::SetLiveProjectionScreenOffset(\1)",
     "F00775 #46 typed promotion: the screen offset is passed by reference"),
    (r"", r"\bVS::Render::BuildScaleCompensatedRoomView\(&(\w+), (\w+), &(\w+)\)",
     r"VS::Render::BuildLiveScaleCompensatedRoomView(\1, *\2, \3)",
     "F00760 #46 typed promotion: the room binding builds the view from the frame camera"),
    (r"", r"\bVS::Render::BuildRoomGeometryViewMatrix\(&(\w+), (\w+), &(\w+)\)",
     r"VS::Render::BuildLiveRoomGeometryViewMatrix(\1, *\2, \3)",
     "F00759 #46 typed promotion: the room binding builds the view from the frame camera"),
    (r"", r"\bVS::Render::DrawSkyDome\((\w+)\)", r"VS::Render::DrawLiveSkyDome(\1, sizeof \1)",
     "F00757 #46 typed promotion: the sky is read inside its declared module array"),
    (r"", r"\bVS::Render::(SetRoomPolygonNearClip|SetRoomPolygonFarClip|SetRoomTextureLodDepth)\(",
     r"VS::Render::\1(VS::Render::LiveRoomClipState(), ",
     "F00753/F00754/F00755 #46 typed promotion: room clip settings live in the binding's clip state instead of "
     "module statics"),
    (r"", r"\bVS::Core::Memory::CopyAlignedWords\(", r"VS::Core::Memory::CopyLiveAlignedWords(",
     "F03572 #46 typed promotion: the word-copy binding bounds array arguments by their declared extent and copies "
     "forward without memcpy overlap (#268)"),
    (r"", r"\bVS::Core::Memory::ZeroAlignedWords\(", r"VS::Core::Memory::ZeroLiveAlignedWords(",
     "F03573 #46 typed promotion: the word-copy binding bounds array arguments by their declared extent and copies "
     "forward without memcpy overlap (#268)"),
    (r"", r"\bVS::Core::Memory::ClearAlignedWords\(", r"VS::Core::Memory::ClearLiveAlignedWords(",
     "F01758 #46 typed promotion: the word-copy binding bounds array arguments by their declared extent and copies "
     "forward without memcpy overlap (#268)"),
    (r"", r"\bVS::Core::Memory::CopyHalfwords\(", r"VS::Core::Memory::CopyLiveHalfwords(",
     "F04188 #46 typed promotion: the word-copy binding bounds array arguments by their declared extent and copies "
     "forward without memcpy overlap (#268)"),
    (r"", r"\bVS::Core::Memory::CopyHalfwordBlocks\(", r"VS::Core::Memory::CopyLiveHalfwordBlocks(",
     "F04189 #46 typed promotion: the word-copy binding bounds array arguments by their declared extent and copies "
     "forward without memcpy overlap (#268)"),
    *[(r"", rf"\bVS::Core::Memory::{name}\(", rf"VS::Core::Memory::{live}(",
       f"{fid} #276 typed promotion: the copy binding bounds array arguments by their declared extent and refuses "
       "the sizes the original ran away on (#277)")
      for fid, name, live in (("F01273", "ReadUnalignedShort", "ReadLiveUnalignedShort"),
                              ("F01401", "ZeroWordsBackward", "ZeroLiveWordsBackward"),
                              ("F01402", "CopyBytesBackward", "CopyLiveBytesBackward"),
                              ("F03263", "CopyWords", "CopyLiveWords"),
                              ("F03264", "SwapWords", "SwapLiveWords"))],
    *[(r"", rf"\bVS::Core::Random::{name}\(", rf"VS::Core::{live}(",
       f"{fid} #276 typed promotion: the engine's rand() state, keystream and shuffle table are bound by "
       "RandomLive.hpp")
      for fid, name, live in (("F01404", "NextInterfaceRandomBits", "NextLiveInterfaceRandomBits"),
                              ("F01455", "RandomRangeExclusive", "LiveRandomRangeExclusive"),
                              ("F03433", "InitializeShuffleTable", "InitializeLiveRandomShuffleTable"),
                              ("F03434", "NextBounded", "NextLiveRandomBounded"),
                              ("F03435", "NextAveraged", "NextLiveRandomAveraged"))],
    *[(r"", rf"\bVS::Core::String::{name}\(", rf"VS::Core::String::{live}(",
       f"{fid} #276 typed promotion: the game-text binding bounds array arguments by their declared extent and "
       "encodes the percent suffix (#277)")
      for fid, name, live in (("F02822", "CopyGameText", "CopyLiveGameText"),
                              ("F02826", "FormatGameInteger", "FormatLiveGameInteger"),
                              ("F02827", "FormatGamePercent", "FormatLiveGamePercent"))],
    *[(r"", rf"\bVS::Core::Memory::{name}\(", rf"VS::Core::Memory::{live}(",
       f"{fid} #276 typed promotion: the game heap keeps its block tables out of the arena, refuses stray and "
       "repeated frees and reports exhaustion")
      for fid, name, live in (("F03463", "FreeFromHighEnd", "FreeLiveFromHighEnd"),
                              ("F03464", "FreeFromLowEnd", "FreeLiveFromLowEnd"),
                              ("F03465", "AllocateFromHighEnd", "AllocateLiveFromHighEnd"),
                              ("F03466", "AllocateFromLowEnd", "AllocateLiveFromLowEnd"))],
    (r"", r"\bVS::Core::IgnoreCallback\(", r"VS::Core::IgnoreLiveActorCallback(",
     "F00023 #276 typed promotion: the actor-stats callback keeps its original argument"),
    (r"^Menu::(Container|Inventory)::SortEquipment$",
     r"CopyLiveHalfwordBlocks\((\w+), (\w+), (\w+( \* 2)?)\)",
     r"CopyLiveHalfwordBlocks(\1, &\2[0], \3)",
     "F04189 #46 typed promotion: the sort buffers are variable-length arrays sized to cover the copy, which a "
     "template cannot deduce"),
    (r"^Menu::(Container|Inventory)::SortEquipment$",
     r"ZeroLiveWordsBackward\((\w+), ",
     r"ZeroLiveWordsBackward(std::as_writable_bytes(std::span(&\1[0], sizeof \1 / sizeof \1[0])), ",
     "F01401 #276 typed promotion: the sort buffers are variable-length arrays, which a template cannot deduce, "
     "so the clear is bounded by an explicit span of the buffer"),
    (r"^Content(::|_)ParseActorModel$", r"CopyLiveAlignedWords\(actor->textureOverlays, data, 0xE8\)",
     r"CopyLiveAlignedWords(VS::Core::Memory::RecordTail(*actor, actor->textureOverlays), data, 0xE8)",
     "F03572 #46 typed promotion: the SHP header block fills the texture overlays and the fields after them "
     "in one copy, so it is bounded by the actor record rather than the first array (#268)"),
    (r"", r"\bVS::Battle::Ai::DispatchMovementBehavior\(", r"VS::Battle::Ai::DispatchLiveMovementBehavior(",
     "F01670 #46 typed promotion: the decision binding runs the actor's behaviour through its AI state"),
    (r"", r"\bVS::Battle::Ai::UpdateActorLocomotionMode\(", r"VS::Battle::Ai::UpdateLiveActorLocomotionMode(",
     "F01669 #46 typed promotion: the decision binding issues the actor's locomotion command"),
    (r"", r"\bVS::Battle::Ai::TickActorDecisionTimers\(", r"VS::Battle::Ai::TickLiveActorDecisionTimers(",
     "F01765 #46 typed promotion: the frame-update binding ticks the actor's decision delays"),
    (r"", r"\bVS::Battle::Ai::InitializeActorLocomotionMode\(", r"VS::Battle::Ai::InitializeLiveActorLocomotionMode(",
     "F01824 #46 typed promotion: the frame-update binding stores the selected locomotion mode"),
    (r"", r"\bVS::Battle::Ai::SetActorVerticalMovementSteps\(", r"VS::Battle::Ai::SetLiveActorVerticalMovementSteps(",
     "F01825 #46 typed promotion: the frame-update binding refuses an actor ID outside the AI table"),
    (r"", r"\bVS::Battle::Ai::RunActorAiUpdateStages\(\)", r"VS::Battle::Ai::RunLiveActorAiUpdateStages()",
     "F01800 #46 typed promotion: the frame-update binding refuses a frame without a room or linked actors"),
    (r"", r"\bVS::Boot::JumpToTitle\([^()]*\)", r"VS::Boot::JumpToLiveTitle()",
     "F04200 #46 typed promotion: the session binding owns the transition boundary and refuses transitions "
     "while a stack-bound scope is live (#266)"),
    (r"", r"\bVS::Boot::JumpToGameLoop\([^()]*\)", r"VS::Boot::JumpToLiveGameLoop()",
     "F04201 #46 typed promotion: the session binding owns the transition boundary and refuses transitions "
     "while a stack-bound scope is live (#266)"),
    (r"", r"\bVS::Render::LoadFrameViewMatrix\(", r"VS::Render::LoadLiveFrameViewMatrix(",
     "F04193 #46 typed promotion: the sprite binding bounds packets by the frame primitive buffer (#265)"),
    (r"", r"\bVS::Save::MemoryCard::InitializeMenuResourcesStep\(", r"VS::Save::MemoryCard::StepLiveMenuResources(",
     "F02737 #46 typed promotion: the memory-card asset binding keeps one explicit load state (#264)"),
    (r"", r"\bVS::Save::MemoryCard::ReleaseMenuResources\(", r"VS::Save::MemoryCard::ReleaseLiveMenuResources(",
     "F02738 #46 typed promotion: the memory-card asset binding keeps one explicit load state (#264)"),
    (r"", r"\bVS::Save::MemoryCard::InitializeResourcesStep\(", r"VS::Save::MemoryCard::StepLiveTitleResources(",
     "F03604 #46 typed promotion: the memory-card asset binding keeps one explicit load state (#264)"),
    (r"", r"\bVS::Save::MemoryCard::ReleaseResources\(", r"VS::Save::MemoryCard::ReleaseLiveTitleResources(",
     "F03605 #46 typed promotion: the memory-card asset binding keeps one explicit load state (#264)"),
    (r"", r"\bVS::Actors::ProjectActorBoneEndpoint\(",
     r"VS::Actors::ProjectLiveActorBoneEndpoint(",
     "F00878 #46 typed promotion: the projection binding bounds the bone by the pose matrices and bone table (#263)"),
    (r"", r"\bVS::Actors::DrawActorModel\((\w+), (\w+), ",
     r"VS::Actors::DrawLiveActorModel(*\1, *\2, ",
     "F01023 #46 typed promotion: the actor binding builds the pose and draws the selected geometry through the "
     "typed draw order"),
    (r"^Battle::", r"\bVS::Battle::DrawSphereTerrainIntersection\((\w+), (\w+), ",
     r"VS::Battle::DrawLiveSphereTerrainIntersection(*\1, *\2, ",
     "F01309 #46 typed promotion: the sphere binding supplies the GTE, target sphere tables and frame packets"),
    (r"^Battle::", r"\bVS::Battle::SetSphereTargets\((\w+), (Battle_actionState->previews), ",
     r"VS::Battle::SetLiveSphereTargets(VS::Battle::CheckedTargetPreviews(\2, \1), ",
     "F01296 #46 typed promotion: the action previews are bound to the live sphere as a span checked against their capacity"),
    (r"", r"\bVS::Battle::Ai::RebuildActorActionPlans\(",
     r"VS::Battle::Ai::RebuildLiveActorActionPlans(",
     "F01718 #46 typed promotion: the planning binding supplies call-local storage, the live actor list and the "
     "queued Battle_ActionRequest (#255)"),
    (r"", r"\bVS::Battle::Ai::InitializeActorAiRelations\(",
     r"VS::Battle::Ai::InitializeLiveActorAiRelations(",
     "F01841 #46 typed promotion: the relations binding supplies the battle actor list and the registered AI state table"),
    (r"", r"\bVS::Battle::Ai::ApplySavedActorAiRelations\(\)",
     r"VS::Battle::Ai::ApplyLiveSavedActorAiRelations()",
     "F01840 #46 typed promotion: the relations binding supplies the battle actor list and the AI state table"),
    (r"", r"\bVS::Battle::Ai::(End|TryBegin)AshleyAlliance\(",
     r"VS::Battle::Ai::\1LiveAshleyAlliance(",
     "F01681/F01682 #46 typed promotion: the alliance binding supplies the reaction state and owns the decision "
     "callback swap"),
    (r"^Battle::Ai::UpdateActorPerception$",
     r"\bVS::Battle::Ai::UpdateLiveEngagementBuildup\(\(struct Battle_Ai_EngagementView\*\)state\)",
     r"VS::Battle::Ai::UpdateLiveEngagementBuildup((Battle_Ai_ActorAiState*)state)",
     "F01679 #276 typed promotion: the buildup binding takes the actor's AI state, which the perception view "
     "aliases"),
    (r"", r"\bVS::Battle::Ai::IssueActorCommand\(",
     r"VS::Battle::Ai::IssueLiveActorCommand(",
     "F01720 #46 typed promotion: the command binding snapshots the AI state and supplies the frame terrain, actor model, "
     "selected candidate and tile reservations"),
    *(
        (r"", rf"\bVS::Battle::Ai::{core}\(", rf"VS::Battle::Ai::{live}(",
         f"{fid} #46 typed promotion: {binding}")
        for fid, core, live, binding in (
            ("F01635", "TryEngageActor", "TryEngageLiveActor",
             "the engagement binding supplies the room engaged mask and the actor's AI timers"),
            ("F01628", "SetActorAwarenessIndicator", "SetLiveActorAwarenessIndicator",
             "the awareness binding supplies the frame actor table and the battle marker models"),
            ("F01634", "DisengageActor", "DisengageLiveActor",
             "the engagement binding supplies the room engaged mask and the actor's AI timers"),
            ("F01631", "IsEngagementReady", "IsLiveEngagementReady",
             "the engagement binding reads the actor's engagement timers"),
            ("F01629", "ResetActorAwarenessIndicator", "ResetLiveActorAwarenessIndicator",
             "the awareness binding supplies the frame actor table and the battle marker models"),
            ("F01633", "DisengageActorById", "DisengageLiveActorById",
             "the engagement binding bounds the actor id by the frame AI table"),
            ("F01632", "UpdateTemporaryAwarenessIndicator", "UpdateLiveTemporaryAwarenessIndicator",
             "the awareness binding supplies the perception timer and the battle marker models"),
            ("F01674", "DoesActionRequireCooldown", "DoesLiveActionRequireCooldown",
             "the decision binding supplies the candidate's action definition"),
            ("F01675", "IsActionCooldownReady", "IsLiveActionCooldownReady",
             "the decision binding supplies the action definition and the actor's cooldown timers"),
            ("F01677", "CanExecuteActionCandidate", "CanExecuteLiveActionCandidate",
             "the decision binding supplies the action definition, target placement and actor distances"),
            ("F01683", "ScoreActorCondition", "ScoreLiveActorCondition",
             "the decision binding forwards the four condition scores"),
            ("F01690", "IsActionHarmfulToActor", "IsLiveActionHarmfulToActor",
             "the decision binding supplies the action definition and the planning stats snapshot"),
            ("F01691", "IsActionBeneficialToActor", "IsLiveActionBeneficialToActor",
             "the decision binding supplies the action definition and the planning stats snapshot"),
            ("F01692", "CacheFallbackActionCandidate", "CacheLiveFallbackActionCandidate",
             "the decision binding supplies the fallback candidate cache"),
            ("F01693", "PromoteRandomizedActionCandidate", "PromoteLiveRandomizedActionCandidate",
             "the decision binding ranks the planning candidates and draws from the actor random state"),
            ("F01770", "NextActorRandomMasked", "NextLiveActorRandomMasked",
             "the random binding supplies the frame random accumulator and budget step"),
            ("F01797", "IsAiTargetBeyondNearRadius", "IsLiveAiTargetBeyondNearRadius",
             "the radius binding supplies the frame squared distance to the target"),
            ("F01671", "CanApproachEngagedTarget", "CanApproachLiveEngagedTarget",
             "the approach binding supplies the engagement timers, room engaged mask and target activity"),
            ("F01792", "RememberAiActionTarget", "RememberLiveAiActionTarget",
             "the planning binding supplies the target placement tile and the target memory"),
            ("F01684", "CopyActionCandidate", "CopyLiveActionCandidate",
             "the planning binding copies the legacy candidate words"),
            ("F01685", "ApplySimulatedStatDelta", "ApplyLiveSimulatedStatDelta",
             "the planning binding supplies the simulated stat and its maximum"),
            ("F01686", "AppendActionCandidate", "AppendLiveActionCandidate",
             "the planning binding supplies the actor's candidate list and exclusions"),
            ("F01687", "SortActionCandidates", "SortLiveActionCandidates",
             "the planning binding ranks the pass candidates and writes the sorted order back"),
            ("F01688", "CopyPlanningActorStats", "CopyLivePlanningActorStats",
             "the planning binding supplies the actor stats records"),
            ("F01794", "CanAiActionAffectActor", "CanLiveAiActionAffectActor",
             "the planning binding supplies the action definition and actor placement"),
            ("F01795", "RefreshAiActionUsability", "RefreshLiveAiActionUsability",
             "the planning binding supplies the limb action slots, stats and action definitions"),
            ("F01796", "RefreshAiResourceRestrictions", "RefreshLiveAiResourceRestrictions",
             "the planning binding supplies the actor stats and resource flags"),
        )
    ),
    *(
        (r"", rf"\bVS::Battle::Ai::{core}\b", rf"VS::Battle::Ai::{live}",
         f"{fid} #46 typed promotion: {binding}")
        for fid, core, live, binding in (
            ("F01648", "SelectActionMovementMode", "SelectLiveActionMovementMode", MOVEMENT_BINDING),
            ("F01661", "ApproachActor", "ApproachLiveActor", MOVEMENT_BINDING),
            ("F01662", "SetMovementBehavior", "SetLiveMovementBehavior", MOVEMENT_BINDING),
            ("F01663", "WanderAroundSavedCenter", "WanderAroundLiveSavedCenter", MOVEMENT_BINDING),
            ("F01664", "WanderAroundTile", "WanderAroundLiveTile", MOVEMENT_BINDING),
            ("F01665", "MoveToTile", "MoveLiveActorToTile", MOVEMENT_BINDING),
            ("F01666", "PatrolWaypoints", "PatrolLiveWaypoints", MOVEMENT_BINDING),
            ("F01667", "FleeVisibleEnemies", "FleeLiveVisibleEnemies", MOVEMENT_BINDING),
            ("F01668", "TrackActorPosition", "TrackLiveActorPosition", MOVEMENT_BINDING),
            ("F01673", "FindActorByRelationship", "FindLiveActorByRelationship", MOVEMENT_BINDING),
            ("F01696", "ApplyActionRelationshipChanges", "ApplyLiveActionRelationshipChanges", MOVEMENT_BINDING),
            ("F01766", "TargetFacingCosine", "LiveTargetFacingCosine", MOVEMENT_BINDING),
            ("F01779", "RefreshAiTargetTileWhenTracking", "RefreshLiveAiTargetTileWhenTracking", MOVEMENT_BINDING),
            ("F01780", "RefreshAiTargetTile", "RefreshLiveAiTargetTile", MOVEMENT_BINDING),
            ("F01786", "RefreshAiMovementCooldown", "RefreshLiveAiMovementCooldown", MOVEMENT_BINDING),
            ("F01695", "MakeTargetHostile", "MakeLiveTargetHostile", CLEANUP_BINDING),
            ("F01778", "SetActorAiReactionWithDelay", "SetLiveActorAiReactionWithDelay", CLEANUP_BINDING),
            ("F01790", "ReleaseAiDestinationTile", "ReleaseLiveAiDestinationTile", CLEANUP_BINDING),
            ("F01804", "FreeActorNavigationOverride", "FreeLiveActorNavigationOverride", CLEANUP_BINDING),
            ("F01820", "PackActorAiPersistenceFlags", "PackLiveActorAiPersistenceFlags", CLEANUP_BINDING),
            ("F01821", "RefreshActorAiReservations", "RefreshLiveActorAiReservations", CLEANUP_BINDING),
            ("F01822", "ForceReleaseActorAiReservations", "ForceReleaseLiveActorAiReservations", CLEANUP_BINDING),
            ("F01823", "ReleaseActorAiReservations", "ReleaseLiveActorAiReservations", CLEANUP_BINDING),
            ("F01831", "ClearActorAiNavigationReset", "ClearLiveActorAiNavigationReset", CLEANUP_BINDING),
            ("F01835", "InitializeActorAiExtensionHook", "InitializeLiveActorAiExtensionHook", CLEANUP_BINDING),
            ("F01836", "ReleaseActorAiContactOccupancy", "ReleaseLiveActorAiContactOccupancy", CLEANUP_BINDING),
            ("F01842", "FreeActorAiState", "FreeLiveActorAiState", CLEANUP_BINDING),
            ("F01844", "ClearActorAiLinksAndPlan", "ClearLiveActorAiLinksAndPlan", CLEANUP_BINDING),
            ("F01845", "AllocateOrbitingBossState", "AllocateLiveOrbitingBossState", CLEANUP_BINDING),
            ("F01694", "UpdateRouteGateAvailability", "UpdateLiveRouteGateAvailability",
             "the route binding supplies the room route table and the gate availability words"),
            ("F01775", "IsAdjacentTileStepBlocked", "IsLiveAdjacentTileStepBlocked",
             "the navigation binding supplies the room AI height and occupancy maps"),
            ("F01789", "ReserveAiDestinationTile", "ReserveLiveAiDestinationTile",
             "the navigation binding supplies the room AI occupancy map"),
            ("F01801", "ActorNavigationOverrideStatus", "LiveActorNavigationOverrideStatus",
             "the override binding supplies the actor AI state table"),
            ("F01802", "HasActorNavigationState", "HasLiveActorNavigationState",
             "the override binding supplies the actor AI state table"),
            ("F01803", "CopyActorNavigationOverrideState", "CopyLiveActorNavigationOverrideState",
             "the override binding supplies the actor AI state table"),
            ("F01805", "SetActorNavigationOverride", "SetLiveActorNavigationOverride",
             "the override binding supplies the actor AI state table and the scripted path"),
            ("F01806", "UpdateActorNavigationOverrides", "UpdateLiveActorNavigationOverrides",
             "the override binding supplies the actor AI state table and the actor list"),
            ("F01826", "AddAiTileOccupant", "AddLiveAiTileOccupant",
             "the occupancy binding supplies the room AI maps and the gimmick tile prefix"),
            ("F01827", "RemoveAiTileOccupant", "RemoveLiveAiTileOccupant",
             "the occupancy binding supplies the room AI maps and the gimmick tile prefix"),
            ("F01828", "MarkAiTileDynamicObstacle", "MarkLiveAiTileDynamicObstacle",
             "the occupancy binding supplies the room AI maps"),
            ("F01832", "BuildAiTerrainHeightsWithWorkspace", "BuildLiveAiTerrainHeightsWithWorkspace",
             "the terrain binding supplies the room collision data and the AI height maps"),
            ("F01645", "DestinationOccupancy", "LiveDestinationOccupancy", _GROUND_BINDING),
            ("F01646", "CanTraverseTileEdge", "CanTraverseLiveTileEdge", _GROUND_BINDING),
            ("F01647", "DirectionalBlockingActor", "LiveDirectionalBlockingActor", _GROUND_BINDING),
            ("F04202", "CanWalkInDirection", "CanLiveActorWalkInDirection", _GROUND_BINDING),
            ("F01672", "FindGapLandingTile", "FindLiveGapLandingTile", _GROUND_BINDING),
            ("F01752", "ResolveDirectionalMovement", "ResolveLiveDirectionalMovement", _GROUND_BINDING),
            ("F01753", "SetDetourBoundary", "SetLiveDetourBoundary", _GROUND_BINDING),
            ("F01754", "RefreshDetourProgress", "RefreshLiveDetourProgress", _GROUND_BINDING),
            ("F01755", "CardinalPassabilityMask", "LiveCardinalPassabilityMask", _GROUND_BINDING),
            ("F01756", "InitializeObstacleDetour", "InitializeLiveObstacleDetour", _GROUND_BINDING),
            ("F01757", "UpdateHesitationTurn", "UpdateLiveHesitationTurn", _GROUND_BINDING),
            ("F01776", "SelectActorVerticalMovementStep", "SelectLiveActorVerticalMovementStep", _GROUND_BINDING),
            ("F01777", "IsTileBehindActorTarget", "IsTileBehindLiveActorTarget", _GROUND_BINDING),
            ("F01781", "FindAiDescentTile", "FindLiveAiDescentTile", _GROUND_BINDING),
            ("F01782", "BeginAiTerrainMovement", "BeginLiveAiTerrainMovement", _GROUND_BINDING),
            ("F01783", "BeginAiAlternateTerrainMovement", "BeginLiveAiAlternateTerrainMovement", _GROUND_BINDING),
            ("F01784", "MoveActorTowardDestinationTile", "MoveLiveActorTowardDestinationTile", _GROUND_BINDING),
            ("F01785", "BeginAiGroundMovement", "BeginLiveAiGroundMovement", _GROUND_BINDING),
            ("F01787", "ClearAiContactOccupancy", "ClearLiveAiContactOccupancy", _GROUND_BINDING),
            ("F01788", "SetAiContactOccupancy", "SetLiveAiContactOccupancy", _GROUND_BINDING),
            ("F01798", "BeginAiMovementTowardPoint", "BeginLiveAiMovementTowardPoint", _GROUND_BINDING),
            ("F01799", "DispatchAiLocomotionMode", "DispatchLiveAiLocomotionMode", _GROUND_BINDING),
        )
    ),
    (r"^(Field::RefreshAshleyInteractionTile)$",
     r"\b(VS::Battle::Ai::ReleaseLiveAiDestinationTile\([^,;()]+,[^,;()]+),[^,;()]+,[^,;()]+\);",
     r"\1);",
     "F00828 calls func_800E4C28 with four arguments, but 7BDD4.s reads only a0/a1 (tile X/Z); the extra "
     "offsets were dead registers, so the typed two-argument release drops them (F00829 is promoted in row 20b)"),
    (r"", r"\bVS::Effects::ScreenDistortion::WarpPolygonVertices\(\s*\)",
     r"VS::Effects::ScreenDistortion::WarpLivePolygonVertices()",
     "F02458 #46 typed promotion: the warp binding supplies the overlay warp words, the resident root table and a "
     "checked ordering-table packet cursor"),
    (r"", r"\bVS::Battle::DrawSphereVolumeWireframe\(",
     r"VS::Battle::DrawLiveSphereVolumeWireframe(",
     "F01288 #46 typed promotion: the wireframe binding supplies the target sphere wire state, the volume transform, "
     "the frame view projection and the wire line primitive"),
    (r"", r"\bVS::Battle::ApplyActionTargetEffects\(",
     r"VS::Battle::ApplyLiveActionTargetEffects(",
     "F00062 #46 typed promotion: the target effects binding supplies the action queue, hit application, reaction "
     "animations and world labels"),
    (r"", r"\bVS::Field::TransitionDoorsAtTile\(",
     r"VS::Field::TransitionLiveDoorsAtTile(",
     "F00579 #46 typed promotion: the door binding supplies the room door records, the raw RNG byte and the borrowed "
     "query list"),
    (r"", r"\bVS::Field::FindMovingPlatformById\(",
     r"VS::Field::FindLiveMovingPlatformById(",
     "F00920/F00938 #46 typed promotion: the platform binding supplies the room platforms, bounce frames and "
     "offsets and returns the contact by value"),
    (r"", r"\bVS::Audio::Control::SetDiscMixVolume\(",
     r"VS::Audio::Control::SetLiveDiscMixVolume(",
     "F03144 #46 typed promotion: the XA binding supplies the live output mode and publishes the CdlATV routes to the "
     "native XA source"),
    (r"", r"\bVS::Audio::Seq::WriteDiscVolume\(\s*\)",
     r"VS::Audio::Seq::WriteLiveDiscVolume()",
     "F03262 #46 typed promotion: the XA binding reads the sequencer disc volume and publishes it with the mix"),
    (r"", r"\bVS::Field::DrawRoomTriggerMarkers\(\s*\)",
     r"VS::Field::DrawLiveRoomTriggerMarkers()",
     "F00617 #46 typed promotion: the marker binding supplies the room floor triggers, frame timers, collision "
     "floor, VRAM animation moves and the BATTLE.PRG marker definition"),
    (r"", r"\bVS::Event::ScriptSetAshleyBattleMode\b",
     r"Event_ScriptSetAshleyBattleModeOpcode",
     "F01130 #46 typed promotion: the event VM opcode slot holds the adapter that decodes the command bytes"),
    (r"", rf"\bVS::Event::({'|'.join(name for _, _, name in EVENT_SCRIPT_CONTROL_OPCODES)})\b", r"Event_\1Opcode",
     "#276 row 33 typed promotion: control slots use guarded native invocation bindings and typed flow"),
    (r"", rf"\bVS::Event::({'|'.join(name for _, _, name in EVENT_SCRIPT_ACTOR_OPCODES)})\b", r"Event_\1Opcode",
     "#276 row 34 typed promotion: each actor opcode slot holds the C binding that decodes the command bytes and runs "
     "the typed handler over the live host"),
    (r"", rf"\bVS::Event::({'|'.join(name for _, _, name in EVENT_SCRIPT_MEDIA_OPCODES)})\b", r"Event_\1Opcode",
     "#276 row 36 typed promotion: each sound, dialog and effect opcode slot holds the C binding that decodes the "
     "command bytes and runs the typed handler over the live host"),
    (r"^Menu::Results::", r"\bVS::Render::DrawTexture(TintFade|FlashFade|Wipe)\(\s*",
     r"VS::Render::DrawLiveTexture\1(Menu_Results_disMap, ",
     "MENUF uses its own _disMap G01780 (3B8.c:26), not the shorter ENDING table; issue #244."),
    (r"^Menu::Results::",
     r"&(Menu_Results_(?:foodChain|evolveOrDie|congratulations|timeAttackHeading)Colors)"
     r"\[1\]",
     r"std::span(\1).subspan(1)",
     "The MENUF pop-ins pass the second of three colors onward; a subspan keeps the pair's extent "
     "checked (issue #259)"),
    (r"^Actors::", r"\bField_gimmickInstances\[([A-Za-z_]\w*)\](?!\s*=(?!=))",
     r"Field_FindGimmickForActorSlot(\1)",
     "renderer slots and gimmick slots are distinct domains; reserved renderer16 "
     "must preserve the existing no-owner query result instead of reading gimmick16"),
    (r"^Field::(?:Find|Show|Hide|Disable)RoomGeometryGroup$|^Field::AddRoomGeometryDepthBias$",
     r"\barg0\b", "group_index",
     "2842C.c:550-606 verified geometry group index from event/gimmick/render callers"),
    (r"^Field::(?:Show|Hide|Disable)RoomGeometryGroup$", r"\btemp_v0\b", "group",
     "2842C.c:562/572/606 points at the geometry group's complete flags word"),
    *[(r"^Field::FindRoomGeometryGroup$", rf"\b{old}\b", new,
       "2842C.c:550 verified counted array of sixteen-word geometry group headers")
      for old, new in (("v1", "group_count"), ("a1", "groups"))],
    *[(r"^Field::SwapRoomGeometryGroupFlags$", rf"\b{old}\b", new,
       "2842C.c:582 verified two group indices and whole-word flag swap")
      for old, new in (("arg0", "first_group_index"),
                       ("arg1", "second_group_index"),
                       ("temp_s0", "first_group"), ("temp_a1", "second_group"),
                       ("temp_a0", "saved_flags"))],
    *[(r"^Field::AddRoomGeometryDepthBias$", rf"\b{old}\b", new,
       "2842C.c:596 and event signed-byte operand verify upper-halfword depth bias")
      for old, new in (("arg1", "depth_bias"), ("temp_v0", "group_halfwords"))],
    (r"^Effects::ClearParticleTextureState$", r"\barg0\b", "storage",
     "5BF94.c:8501 existing C entry receives native texture-state storage"),
    (r"^Effects::(?:Initialize|Advance)ParticleTextureAnimation$",
     r"\barg0\b", "animation",
     "5BF94.c:8513/8521 verified texture-animation state ownership"),
    (r"^Effects::InitializeParticleTextureAnimation$", r"\barg2\b", "programIndex",
     "5BF94.c:8513 verified block2 halfword program-offset selection"),
    (r"^Effects::AdvanceParticleTextureAnimation$", r"\bw\b", "command",
     "5BF94.c:8521 verified frame/offset/loop/end command word"),
    (r"^Effects::(?:SetEffectScriptRegister|BranchIfEffectRegisterEqual)$",
     r"\barg0\b", "instance",
     "5BF94.c:7633/7639 verified effect-instance stream and register ownership"),
    *[(r"^Menu::Score::QueueEnemyModelRead$", rf"\b{old}\b", new,
       "592C.c:171 verified source-file and palette-variant selection meanings")
      for old, new in (("id", "zudId"), ("cdFile", "sourceFile"),
                       ("i", "variantIndex"))],
    (r"^Menu::Score::LoadEnemyModelParts$", r"\bslot\b", "request",
     "592C.c:203 the local is an asset request, not a queue slot"),
    *[(r"^Event::(?:Script|Command)", rf"\b{old}\b", new,
       "573B8.c:67-142 dispatches the current instruction and thread index through both opcode tables")
      for old, new in (("arg0", "instruction"), ("arg1", "thread_index"))],
    (r"^Audio::", r"\bSpuSetNoiseClock\b", "Audio_Native_SetNoiseClock",
     "native noise clock controls replace the SDK register setter"),
    (r"^Audio::", r"\bSpuSetReverbModeDepth\b", "Audio_Native_SetReverbDepth",
     "native wet gains replace the SDK reverb depth setter"),
    (r"^Audio::", r"SpuGetReverbModeType\(&curMode\)", "curMode = Audio_Native_ReverbMode()",
     "read initialized native preset state instead of the unported SDK getter"),
    (r"^Audio::", r"SpuSetReverbModeType\(in_ReverbMode \| SPU_REV_MODE_CLEAR_WA\)",
     "Audio_Native_SetReverbMode(in_ReverbMode)", "native preset changes clear owned delay state"),
    (r"^Audio::", r"\bSpuSetReverb\b", "Audio_Native_EnableReverb",
     "native reverb processing enable replaces the SDK register setter"),
    (r"^Boot::", r"SsUtReverbOff\(\)", "Audio_Native_EnableReverb(0)",
     "boot suspension freezes reverb writes through the native output API"),
    (r"^Boot::", r"SsUtReverbOn\(\)", "Audio_Native_EnableReverb(1)",
     "boot resume enables the native effect processor"),
    (r"^Menu::", r"&Menu_textBoxOrderingTables\b", "Menu_textBoxOrderingTables",
     "textbox ordering storage is a bounded array, not a scalar address"),
    (r"^(Actors|Render)::", r"\(void\*\)\(\(uintptr_t\)Core_ScratchBase\(\) \+ 0x35C\)",
     "Render_ModelWorkspace()", "native model workspace cannot fit in the original scratchpad tail"),
    (r"^Actors::", r"\bRender_ModelScratchpad(?:AnglesView|BoneView)\b",
     "Render_ModelScratchpad", "model workspace consumers share the native pointer-bearing layout"),
    # Flag tests through the 32-bit word at 0x5AC of an actor record (`*(int*)((char*)actor + 0x5AC) & 0x800000`):
    # the word is a run of 1-bit bitfields, whose x64 offset differs; each mask is the named flag at that bit
    # (tools/port/field_at.py Actors_ActorRecord 0x5AC:<bit>).
    *[(r"^(Actors|Content)::",
       r"(?:\(\*\((?:u_)?int\s*\*\)\s*\(\((?:u_)?char\s*\*\)\s*actor\s*\+\s*0x5AC\)\)"
       r"|\*\((?:u_)?int\s*\*\)\s*\(\((?:u_)?char\s*\*\)\s*actor\s*\+\s*0x5AC\))" + rf"\s*&\s*{mask}\b",
       f"actor->{name}", f"actor flag bit {bit} ({name})")
      for bit, name, mask in [(b, n, f"0x0*{1 << b:X}") for b, n in (
          (27, "textureHeaderBit"), (26, "landingPending"), (24, "animationLocked"), (23, "poseOverrideActive"),
          (18, "advanceRequested"), (16, "animationEventFlag"), (15, "isRunning"))]],
    (r"^Actors::", r"\*\(u_int\s*\*\)\(\(char\s*\*\)\s*actor\s*\+\s*0x5AC\)\s*&=\s*~0x4000000;",
     "actor->landingPending = 0;", "actor flag bit 26 (landingPending)"),
    (r"^Actors::", r"\*\(u_int\s*\*\)\(\(char\s*\*\)\s*actor\s*\+\s*0x5AC\)\s*\|=\s*0x4000000;",
     "actor->landingPending = 1;", "actor flag bit 26 (landingPending)"),
    # The same word in a gimmick instance (Field::GimmickInstance, 40564.c:1769): bit 27 is bit 2 of a reserved run.
    (r"^Actors::UpdateActorAppearanceTransition$",
     r"\*\(int\s*\*\)\(\(char\s*\*\)\s*arg0\s*\+\s*0x5AC\)\s*&\s*0x08000000", "(arg0->reserved5ACBits25 & 4)",
     "gimmick flag bit 27"),
    (r"^Actors::UpdateActorAppearanceTransition$",
     r"\*\(int\s*\*\)\(\(char\s*\*\)\s*arg0\s*\+\s*0x5AC\)\s*&\s*8\b", "arg0->actionPoseActive", "gimmick flag bit 3"),
    # 5BF94.c particle code indexes the motion vectors as int[3] (`unk1C[0]`); the upstream-merged
    # Effects::ParticleMotion has them as Effects::FixedVector {vx, vy, vz}.
    (r"^Effects::", r"\bmotion\.(position|velocity|force|acceleration)\[0\]", r"motion.\1.vx",
     "merged particle motion vectors"),
    (r"^Effects::", r"\bmotion\.(position|velocity|force|acceleration)\[1\]", r"motion.\1.vy",
     "merged particle motion vectors"),
    (r"^Effects::", r"\bmotion\.(position|velocity|force|acceleration)\[2\]", r"motion.\1.vz",
     "merged particle motion vectors"),
]

MACRO_REPLACEMENTS: dict[str, str] = {
    # src/include/insertTPage.h: inline assembly that links a three-word texture-page packet into ordering-table
    # slot `after`, through the frame scratch block (primitive cursor at +0, ordering table at +4), and advances
    # the cursor by 12 bytes. Same steps in C, through the block's fields so 8-byte pointers keep working, with
    # the packet's link as a handle (Legacy/Render/LibGpu.h).
    "_insertTPage": (
        "#define _insertTPage(after, texture_page_mode)                                                              \\\n"
        "    do {                                                                                        \\\n"
        "        u_long* ot_ = (u_long*)Render_frameScratch.orderingTable + (after);                    \\\n"
        "        u_long* p_ = (u_long*)Render_frameScratch.primitiveCursor;                             \\\n"
        "        p_[1] = 0xE1000200 | ((texture_page_mode) & 0x1FF);                                                 \\\n"
        "        p_[2] = 0;                                                                             \\\n"
        "        p_[0] = 0x02000000 | (*ot_ & 0xFFFFFF);                                                \\\n"
        "        *ot_ = (*ot_ & 0xFF000000) | LegacyGpu_Handle(p_);                                     \\\n"
        "        Render_frameScratch.primitiveCursor = p_ + 3;                                          \\\n"
        "    } while (0)"
    ),
}

NESTED_SAME_AS: dict[str, str] = {
    # TITLE 22C.c primBuf_t (T00586) is same-as MENU7 260.c primBuf_t (T00585): the copy writes MENU7's, so the
    # unnamed unions nested in it are MENU7's too, member for member (64-byte record, same order).
    "T00794": "T00784",   # Title::FileMenuPrimitivePayload -> Menu::Files::FilePrimitive
    "T00838": "T00836",   # Title::TileAndGradientQuad -> Menu::Files::TileAndGradientQuad
}

# Pointer members of on-disc structs (and other views of the same loaded bytes) that the PS1 relocated in place: the
# file holds a 32-bit offset from the struct's start and the loader added the struct's address. Natively they stay
# 32-bit offsets (`u_int`, the file layout), every use `E->m` becomes VS_ONDISC(E, m, T) = (T)((char*)E + E->m), and
# the relocation statements are removed by the FIXES above. field id -> what it is
ONDISC_OFFSETS: dict[str, str] = {
    "T00663.001": "map room payload, resolved as an offset from its 12-byte room record",
    "T00252.001": "SHP sound (AKAO) section",
    "T00252.002": "SHP group section",
    "T00252.003": "SHP vertex section",
    "T00252.004": "SHP polygon section",
    "T00460.000": "SEQ first section (animation data)",
    "T00460.001": "SEQ second section (animation map)",
    "T00474.000": "SEQ animation data (the first section, through Actors::SeqAnimationBank)",
    "T00474.001": "SEQ animation map (Actors::SeqAnimationBank views a SEQ buffer)",
}

# Reviewed fixes to copied type definitions: type id -> [(old, new, reason)]. Checked by Native\Tests\src\LayoutAsserts.cpp.
TYPE_FIXES: dict[str, list[tuple[str, str, str]]] = {
    "T00255": [
        ("u_char pulsedPalette[32];", "u_short pulsedPalette[16];",
         "3A1A0.h:521 / 40564.c:1710; T00255.072 and T00469.006 agree on sixteen 16-bit CLUT entries"),
    ],
    "T00225": [
        ("Battle_TargetPreview previews[23];", "Battle_TargetPreview previews[255];",
         "Native preview indices reserve FF for cancellation; keep all actor/limb candidates outside the PS1-sized inline limit"),
    ],
    "T00226": [
        ("Battle_HitResult targets[30];", "Battle_HitResult targets[255];",
         "Execution/result requests must accommodate the same native target domain as candidate collection"),
    ],
    "T00218": [
        ("char offsetY;", "u_char offsetY;",
         "The target-row producer stores unsigned pixel offsets including rows below 127"),
    ],
    "T00271": [
        ("char reservedB[9];",
         "union {\n        char reservedB[9];\n        struct {\n"
         "            u_char reservedBPrefix[2];\n            u_char engagementLimit;\n"
         "            u_char reservedEPrefix[2];\n            u_char engaged_mask_bytes[2];\n"
         "            u_char engagedCount;\n            u_char reserved13;\n        };\n    };",
         "func_800D836C/82CC use the encounter limit, membership mask and member count; byte aliases preserve native layout"),
    ],
    "T00532": [
        ("char reservedC8[8];",
         "union {\n        char reservedC8[8];\n        struct {\n"
         "            int wander_center_radius;\n"
         "            Battle_Ai_TilePosition wander_center;\n        };\n    };",
         "72B70.s restores the configured wander radius and packed center"),
        ("char reserved10D[11];",
         "char reserved10D[3];\n    Battle_Ai_TilePosition flee_tile;\n"
         "    u_short flee_timer;\n    char reserved116[2];",
         "72B70.s remembers the running hostile center and its 90-frame timeout"),
        ("char reserved6;",
         "union { char reserved6; u_char ignore_floor_mask; };",
         "70D4C.s tests the floor-mask override byte at original offset 6"),
        ("char reserved31[3];",
         "union {\n        char reserved31[3];\n        struct {\n"
         "            u_char reserved31Prefix;\n            u_char terrain_reject_mask;\n"
         "            u_char reserved33;\n        };\n    };",
         "70D4C.s uses actor-specific high collision bits at original offset 0x32"),
        ("char reserved94[4];",
         "union {\n        char reserved94[4];\n        struct {\n"
         "            u_short collision_radius;\n            u_char reserved96[2];\n"
         "        };\n    };",
         "70D4C.s uses the native collision radius initialized by actor creation"),
        ("char reserved132[2];",
         "union {\n        char reserved132[2];\n        struct {\n"
         "            u_char ground_area;\n            u_char reserved133;\n"
         "        };\n    };",
         "70D4C.s compares the destination area against original byte 0x132"),
        ("char reserved22[4];",
         "union {\n        char reserved22[4];\n        struct {\n"
         "            u_char reserved22Prefix;\n            signed char movementEnabled;\n"
         "            u_char reserved24[2];\n        };\n    };",
         "func_800E4BB8 uses the signed movement-enabled byte at original offset 0x23"),
        ("char reserved89[3];",
         "union {\n        char reserved89[3];\n        struct {\n"
         "            u_char requestedMovementMode;\n            u_char currentMovementMode;\n"
         "            u_char reserved8B;\n        };\n    };",
         "func_800DB4AC records the requested and current movement modes at 0x89/0x8A"),
        ("char reservedFC[12];",
         "union {\n        char reservedFC[12];\n        struct {\n"
         "            u_char reservedFCPrefix[4];\n            u_char patrolIndex;\n"
         "            u_char patrolRow;\n            u_char patrolRate;\n"
         "            signed char patrolDirection;\n            u_char patrolCooldown;\n"
         "            u_char reserved105[3];\n        };\n    };",
         "func_800DB4AC selects the closest flagged entry in one of four patrol rows"),
        ("char reserved474[13];",
         "union {\n        char reserved474[13];\n"
         "        u_char movementModeCounters[13];\n    };",
         "func_800DB4AC clears one byte per two-byte movement-mode record at 0x474"),
        ("char reserved12B[2];",
         "union {\n        char reserved12B[2];\n        struct {\n"
         "            u_char reserved12BPrefix;\n            u_char engagementRequest;\n"
         "        };\n    };",
         "func_800D836C clears the engagement request at original offset 0x12C"),
        ("char reserved9C[23];",
         "union {\n        char reserved9C[23];\n        struct {\n"
         "            u_char last_slide_tile_bytes[4];\n            u_char reservedA0[16];\n"
         "            u_char step_height_bytes[2];\n            u_char reservedB2;\n"
         "        };\n    };",
         "func_800DEEFC uses the slide cache at 0x9C and step threshold at 0xB0; byte aliases retain native view layout"),
        ("char reserved174[8];",
         "union {\n        char reserved174[8];\n        SVECTOR nextPosition;\n    };",
         "func_800DEEFC stages the destination point at original offset 0x174"),
        ("char reserved90[2];",
         "union {\n        char reserved90[2];\n        u_short actionDelay;\n    };",
         "func_800DBCB4 checks the action gate halfword at original offset 0x90"),
        ("char reserved10[3];",
         "union {\n        char reserved10[3];\n        struct {\n"
         "            u_char resourceStatusRestricted;\n            u_char lowMp;\n"
         "            u_char lowHp;\n        };\n    };",
         "func_800E4F14 writes status/MP/HP condition bytes; func_800E4CF4 consumes their low bits"),
        ("char reserved3C[8];",
         "union {\n        char reserved3C[8];\n        struct {\n"
         "            u_int rememberedTargetTile;\n            u_short targetMemoryFrames;\n"
         "            short savedRelationOptions;\n"
         "        };\n    };",
         "func_800E4C8C stores target tile/memory at0x3C/0x40; func_800E72D0 restores options at0x42"),
        ("char reserved120[10];",
         "union {\n        char reserved120[10];\n        struct {\n"
         "            u_char fine_x;\n            u_char fine_layer;\n"
         "            u_char fine_z;\n            u_char fine_status;\n"
         "            u_char reserved124;\n            u_char locomotionMode;\n"
         "            u_char reserved126[4];\n        };\n    };",
         "func_800E6828 selects the movement dispatcher at original offset 0x125"),
        ("char reserved154[4];",
         "union {\n        char reserved154[4];\n        struct {\n"
         "            u_char reserved154Area;\n            u_char decisionDelay;\n"
         "            union { u_char reserved156[2]; u_short movement_wait; };\n"
         "        };\n    };",
         "func_800E4358 decrements the decision timer at original offset 0x155"),
        ("char reserved224[4];",
         "union {\n        char reserved224[4];\n        struct {\n"
         "            u_short locomotionFlags;\n            u_char walkStep;\n"
         "            u_char runStep;\n        };\n    };",
         "func_800E6828/685C use locomotion flags and the paired movement rates"),
    ],
    "T00214": [
        ("    int targetList0;", "    Battle_TargetPreview* targetList0;", "native target-list pointer"),
        ("    int targetList1;", "    Battle_TargetPreview* targetList1;", "native secondary target-list pointer"),
    ],
    "T00237": [("u_char workArea[0x1FD0];",
                 "int projectedVertices[768];\n    int vertexDepths[768];\n"
                 "    unsigned projectedVertexCount;\n    u_char reserved1804[0x7CC];",
                 "4B16C.s:func_800B3A68 screen/depth arrays as 32-bit words the typed projection writes; retain "
                 "the surrounding context extent")],
    "T00239": [
        ("    int animationHeader;", "    intptr_t animationHeader;", "native SEQ bank cache"),
        ("    u_char textureRowOffsets[2];\n    short momentumDistance;",
         "    union {\n        u_char textureRowOffsets[3];\n        struct {\n"
         "            u_char reservedTextureRows[2];\n            short momentumDistance;\n"
         "        };\n    };",
         "func_8009CE9C reads three bytes at actor+0x183E; bank 2 aliases momentumDistance at +0x1840"),
        ("    u_short previousTargetZ;", "    u_short previousTargetZ;\n    unsigned modelDataSize;",
         "native SHP allocation extent for validated model traversal"),
    ],
    # 146C.h:621: an actor's record is placed at core.model and runs into reservedB2C (the PS1 slot was 0x1900 bytes,
    # the record 0x18DC). The x64 record is 68 bytes larger than the PS1 one; the reserve grows so the record still
    # ends before the weapon model slots.
    "T00634": [("char reservedB2C[0x1788];", "char reservedB2C[0x1788 + 80]; /* x64: the actor record is larger */",
                "room for the x64 actor record")],
}

FIXES["F03437"] = FIXES["F03437"] + [
    ("    VS::Content::InitializeLiveDiscQueue();",
     "    Battle_Ai_InvalidateSessionBindings();\n    VS::Content::InitializeLiveDiscQueue();",
     "native AI bindings must not outlive the game-arena reset in _sysReinit"),
    ("VS::Content::ResetDiscDrive();", "VS::Content::ResetLiveDiscDrive();",
     "F03479 typed core takes a DiscDriveHost; the generated caller uses the live binding"),
    ("void Boot_InitializeTitleSession(void)\n{",
     "void Boot_InitializeTitleSession(void)\n{\n    Save_NativeResetSession();",
     "main.c:8309 discard staged native state before heap/module initialization"),
]

# #282: vs_main_allocateCdQueueSlot (main.c:9411) returned no value when all 32 disc requests were busy; its binding
# returns null then. A caller that enqueues a refused request reads its file without the queue instead and keeps the
# completed record CompleteLiveRefusedDiscRequest returns, which its polls and release accept (CdDriveLive.hpp). A
# granted request takes the original path unchanged.
_REFUSED_DISC_REQUEST_WHY = ("main.c:9411 the request queue can be full (#282): read the file without the queue and "
                             "keep a completed record for the caller's polls")


def _refused_disc_request(indent: str, slot: str, destination: str, disc_file: str, priority: bool = False,
                          enqueue: str | None = None, prefix: str = "") -> tuple[str, str, str]:
    call = "EnqueueLivePriorityDiscRequest" if priority else "EnqueueLiveDiscRequest"
    original = enqueue if enqueue is not None else f"{indent}VS::Content::{call}({slot}, {destination});"
    return (prefix + original,
            f"{prefix}{indent}if ({slot} != NULL) {{\n"
            f"{indent}    VS::Content::{call}({slot}, {destination});\n"
            f"{indent}}} else {{\n"
            f"{indent}    {slot} = VS::Content::CompleteLiveRefusedDiscRequest({disc_file}, {destination});\n"
            f"{indent}}}",
            _REFUSED_DISC_REQUEST_WHY)


_EVENT_FILE_REQUEST = "(*(Content_DiscQueueSlot* (*))&Event_eventFileDiscRequest)"
for _row, _fixup in (
    ("F03417", _refused_disc_request(" " * 4, "slot", "Core_overlaySlots[0]", "&cdFile", priority=True)),
    ("F03417", _refused_disc_request(" " * 4, "slot", "Core_overlaySlots[1]", "&cdFile", priority=True)),
    ("F03418", _refused_disc_request(" " * 4, "slot", "Core_overlaySlots[0]", "&cdFile", priority=True)),
    ("F03419", _refused_disc_request(" " * 4, "slot", "Core_overlaySlots[0]", "&cdFile", priority=True)),
    ("F01388", _refused_disc_request(" " * 8, "p->slot", "*new_var", "&file")),
    ("F01277", _refused_disc_request(
        " " * 4, _EVENT_FILE_REQUEST, "arg1", "&Event_eventFileDiscLocation")),
    ("F02503", _refused_disc_request(
        " " * 8, "Menu_Framework_loadItemTextItemNamesCdQueueSlot", "Menu_Framework_itemNames", "&cdFile")),
    ("F04163", _refused_disc_request(
        " " * 12, "Menu_Manual_helpAssetsCdQueue", "Menu_Manual_helpAssets",
        "&Menu_Manual_kHelpFileCdFiles[selectedRow * 2]")),
    ("F04163", _refused_disc_request(
        " " * 12, "Menu_Manual_helpTextCdQueue", "Menu_Manual_helpText",
        "&Menu_Manual_kHelpFileCdFiles[selectedRow * 2 + 1]")),
    ("F04148", _refused_disc_request(" " * 8, "Menu_Map_loadBackgroundSlot", "Menu_Map_backgroundImage", "&cdFile")),
    ("F04150", _refused_disc_request(
        " " * 8, "Menu_Map_sceneCdQueueSlot", "Battle_unusedSceneBuffer",
        "&Menu_Map_kSceneArmFiles[Menu_Map_currentScene]")),
    ("F04151", _refused_disc_request(
        " " * 12, "Menu_Map_sceneCdQueueSlot", "Menu_Map_armFileBuff",
        "&Menu_Map_kSceneArmFiles[Menu_Map_currentScene]")),
    ("F03046", _refused_disc_request(
        " " * 8, "Menu_Results_initializeCubePuzzleStartIqDisCdSlot", "Menu_Results_initializeCubePuzzleStartIqDisData",
        "&Menu_Results_kDisFiles[Menu_Results_DisFile_Iq]")),
    ("F03048", _refused_disc_request(
        " " * 8, "Menu_Results_initializeCubePuzzleQuitEscDisCdSLot", "Menu_Results_initializeCubePuzzleQuitEscDisData",
        "&Menu_Results_kDisFiles[Menu_Results_DisFile_Esc]")),
    ("F03010", _refused_disc_request(
        " " * 8, "Menu_Results_initializeGameClearCdSlot", "Menu_Results_initializeGameClearTimData",
        "&Menu_Results_kDisFiles[Menu_Results_DisFile_Rank]")),
    ("F03011", _refused_disc_request(
        " " * 8, "Menu_Results_initializeTimeAttackResultsCdSlot", "Menu_Results_initializeTimeAttackResultsTimData",
        "&Menu_Results_kDisFiles[1]")),
    ("F03012", _refused_disc_request(
        " " * 8, "Menu_Results_initializeTimeAttackStartCdSlot", "Menu_Results_initializeTimeAttackStartTimData",
        "&Menu_Results_kDisFiles[Menu_Results_DisFile_Attack]")),
    ("F02839", _refused_disc_request(
        " " * 4, "Menu_Score_zudCdSlot", "Menu_Score_zudData", "&sourceFile", priority=True)),
    ("F02803", _refused_disc_request(
        " " * 12, "Menu_Score_monBinCdQueueSlot", "Menu_Score_monBinData", "&Menu_Score_kMonBinFile")),
):
    FIXES.setdefault(_row, []).append(_fixup)

# The primary effects' menu-overlay reload request reads nothing (size 0, no destination) and case 9 already skips a
# null request, so a refused one is only kept from the queue (#282).
FIXES.setdefault("F01425", []).append((
    "            VS::Content::EnqueueLiveDiscRequest(Effects_menuReloadSlot, NULL);",
    "            if (Effects_menuReloadSlot != NULL) {\n"
    "                VS::Content::EnqueueLiveDiscRequest(Effects_menuReloadSlot, NULL);\n"
    "            }",
    "main.c:9411 a refused (null) request is not enqueued; case 9 skips it (#282)"))

# 5BF94.c:7885 LoadEffectTimeline: the P-file binding refuses a section or track outside the timeline block with a
# warning and null (PFileLive.hpp). A refused section keeps the effect's current timeline; a refused track ends the
# timeline's tracks before it.
FIXES.setdefault("F01557", []).extend([
    ("    int i;\n\n    if (arg0->context.timeline != NULL) {",
     "    int i;\n\n    if (entry == NULL) {\n        return;\n    }\n\n"
     "    if (arg0->context.timeline != NULL) {",
     "PFileTimelineSection refuses a section outside the timeline block"),
    ("        p->tracks[i].header = entry->tracks[i].header;",
     "        if (p->tracks[i].keys == NULL) {\n            p->trackCount = i;\n            break;\n        }\n"
     "        p->tracks[i].header = entry->tracks[i].header;",
     "PFileTimelineTrackData refuses a track outside the timeline block"),
])

for _row, _variable in (
    ("F00946", "actor"),
    ("F00948", "temp_s0"),
    ("F00949", "new_var"),
    ("F00950", "temp_s1"),
    ("F00952", "temp_s1"),
    ("F00953", "temp_s1"),
):
    _declaration = f"    Actors_ActorRecord* {_variable} = Actors_actorRecords[arg0];"
    FIXES.setdefault(_row, []).append((
        _declaration,
        _declaration + f"\n    if ({_variable} == NULL) {{\n"
        '        Core_Debugf("actors", "movement wrapper skips absent actor=%d", arg0);\n'
        "        return;\n    }",
        "retain the guarded movement primitive's optional actor contract in its rate wrapper",
    ))

# The WEP header (Actors::WeaponModelData, 30DB0.c:598): four section offsets, relocated in place by _parseWep and
# re-based when a loaded model is copied to another slot.
ONDISC_OFFSETS.update({
    "T00344.001": "WEP texture section",
    "T00344.002": "WEP vertex group section",
    "T00344.003": "WEP vertex section",
    "T00344.004": "WEP polygon section",
})
# Field::RoomRecord (146C.c): the 24 section slots after the header are filled as one array of pointers
# (`(&var_s0->geometry)[i]`, _loadMpdRoomSection); seven were declared int in the decomp's view.
TYPE_FIXES["T00329"] = [(f"    int {m};", f"    void* {m}; /* a section pointer (filled with the others) */",
                         "section slot") for m in ("lighting", "section6", "section7", "section8", "section15",
                                                    "section16", "section17")]
# Accessors that return those section pointers as int.
SIG_RETYPE: dict[str, dict] = {
    "F01281": {3: "Battle_TargetPreview*"},
}

# All 182 targets of BATTLE's 256 normal opcodes and INITBTL 12AC.c's skip table. The excluded rows are
# helpers, not handlers. Keep even control-code-only handlers compatible with the pointer-returning branches.
EVENT_OPCODE_HANDLERS = frozenset(
    {f"F{i:05}" for i in range(1027, 1208)} - {"F01033", "F01039", "F01186"}
    | {"F01221", "F01226", "F01229", "F01232"})
SIG_RETYPE.update({fid: {"ret": "intptr_t"} for fid in EVENT_OPCODE_HANDLERS})
# Opcode handlers promoted to typed VS:: C++ (#46) keep their table slot as a C binding with the dispatch ABI; its
# header is in NATIVE_HELPER_HEADERS and MODULE_BINDING_ENTRIES maps the module-image address to it.
EVENT_OPCODE_BINDINGS: dict[str, str] = {
    "F01130": "Event_ScriptSetAshleyBattleModeOpcode",
    **{fid: f"Event_{name}Opcode" for fid, _, name in EVENT_SCRIPT_CONTROL_OPCODES},
    **{fid: f"Event_{name}Opcode" for fid, _, name in EVENT_SCRIPT_ACTOR_OPCODES},
    **{fid: f"Event_{name}Opcode" for fid, _, name in EVENT_SCRIPT_MEDIA_OPCODES},
}

# This normal-only handler omits its unused thread argument in 4A0A8.c. It has no direct C callers,
# but the dispatch ABI still requires both arguments (check_event_dispatch.py checks all table targets).
for fid in ("F01122",):
    FIXES.setdefault(fid, []).append(("(u_char* arg0)", "(u_char* arg0, short arg1)", "consistent opcode signature"))

# The MPD section E header (Field::MpdSectionE, 146C.c:15700): two array pointers into its own payload, set by
# the room section load (VS::Field::LoadRoomSections) and read only by assembly-era code; offsets from the header natively.
ONDISC_OFFSETS.update({"T00337.003": "MPD section E first array", "T00337.004": "MPD section E second array"})


# 146C.c:4810-4816 (func_800738E4) resumes the room's music slot through the D_800F18AC view of the zone context;
# #276 slice 25c retired the view, so the slot is read from the zone context directly.
FIXES.setdefault("F00131", []).append(
    ("Field_roomMusicSlot", "Field_zoneContext.musicSlot", "D_800F18AC aliases _zoneContext.musicSlot (146C.h:719)"))


# Individually reviewed event bindings; structural fixups run first.
REGEX_FIXES += [
    (rf"^{name}$", rf"\b{old}\b", new, evidence)
    for name, bindings, evidence in (
        ("Event::EndFieldCutscene",
         (('sp10', 'sound_override_reset'),),
         "F00131 146C.c:4777-4821 sets sp10.unk0=0 and passes its address with selector -1 to func_8009F990 after the follow-on-event early return. Consumer 30DB0.c:3401-3412 dispatches the same override to every actor. This names the reset request, not a fully initialized object; untouched bytes stay untouched."),
        ("Event::RestoreScriptTextureRectangle",
         (('sp10', 'source_rectangle'), ('temp_s0', 'move_primitive')),
         "F00972 40564.c:1280-1299 fills sp10 with source rectangle(470,384,18,36), loads temp_s0 from primitive allocator, initializes move to(16,400), appends it, and advances allocation by one DR_MOVE. new_var is separately retained with its complete write-only evidence."),
        ("Event::FinishSkippedCutscene",
         (('arg0', 'scan_cursor'),),
         "F01033 4A0A8.c:618-640 scans arg0 until opcode0x0F, compares four bytes against the sound-restore marker, and advances by D_800E9C30[*arg0]. Both ordinary/jump-end handlers pass their current instruction when previously skipping. scan_cursor covers the advancing local pointer, not caller cursor output."),
        ("Event::ScriptEndCutsceneAndJump",
         (('temp_v1', 'previous_skip_state'),),
         "F01034 4A0A8.c:642-651 snapshots D_800F4C2C into temp_v1 before clearing the global. Saved state selects cleanup/yield versus normal relative jump. Do not call it current state: current global has already been reset."),
        ("Event::ScriptEndCutscene",
         (('temp_v1', 'previous_skip_state'),),
         "F01035 4A0A8.c:653-662 separately snapshots D_800F4C2C into temp_v1, clears global, and uses old state to choose cleanup/yield versus normal completion. This binding was individually reviewed, not assumed equivalent merely from F01034 spelling."),
        ("Event::ScriptConfigureSplashLayer",
         (('new_var', 'splash_tween'),),
         "F01044 4A0A8.c:857-877 early-outs without a GIM set, clears the encoded layer's D_800F4B30 tween duration for immediate configuration, then sets the layer's routine/offset/scale/brightness and activates it. #276 row 6 moved the layer writes to VS::Render::ConfigureLiveGimLayer; new_var only survives if the tween pointer is reintroduced."),
        ("Event::ScriptTweenSplashLayer",
         (('temp_a1', 'splash_tween'), ('temp_a2', 'layer_index')),
         "F01045 4A0A8.c:879-903 derives temp_a2=byte5>>6 and chooses temp_a1 in the tween array; #276 row 6 reads the layer's typed placement (splash_layer) instead of offsetting a PS1 record. It waits for idle tween, captures start/delta values from the placement, and resets elapsed."),
        ("Event::ScriptBranchOnFlagImmediate",
         (('temp_a0', 'flag_value'), ('temp_a1', 'immediate_value')),
         "F01085 4A0A8.c:1543-1587 gets temp_a0 from the packed flag index and temp_a1 from byte3. Every compare/bit-test uses these same operands; true comparison returns0, other path returns the relative target. Preserve char signedness and branch polarity; names do not endorse older ledger wording about which condition branches."),
        ("Event::ScriptBranchOnFlagPair",
         (('temp_s0', 'left_flag_value'), ('temp_a0', 'right_flag_value')),
         "F01086 4A0A8.c:1589-1628 reads temp_s0 from the first packed flag index and temp_a0 from the second. All six relational tests use that left/right ordering, returning0 for matching comparison and branch target otherwise. Char types/polarity stay unchanged."),
        ("Event::ScriptApplyImmediateFlagOperation",
         (('temp_a1', 'operation_and_flag_bits'), ('temp_s1', 'immediate_value'), ('temp_s0', 'target_flag_id')),
         "F01096 4A0A8.c:1685-1718 reads temp_a1=byte1, using high nibble for operation and low nibble to form temp_s0 with byte2>>2. temp_s1 is byte3, used for arithmetic/bitwise/assignment. Packed-byte name covers both temp_a1 roles, not merely opcode."),
        ("Event::ScriptStoreButtonState",
         (('temp_v0', 'flag_and_button_source'),),
         "F01102 4A0A8.c:1798-1805 reads temp_v0 as short+1, uses low10bits as destination flag and bit15 to choose current pressed low byte or prior button state. Name covers both parts rather than falsely treating all bits as flag id."),
        ("Event::ScriptConfigureEasingCurve",
         (('new_var', 'curve_table'), ('temp_a1', 'curve_bytes'), ('var_v1', 'encoded_power')),
         "F01111 4A0A8.c:1948-1964 assigns the u_short table base to new_var and derives temp_a1 by adding the two-bit curve index before casting to bytes. It stores family in byte0 and var_v1 (low3bits, optionally OR0xF8) in byte1. Original573B8.c:168-220 reads byte1 as signed power; TYPE_LEDGER T00256 establishes two-byte InterpolationCurve. Stride and encoded sign extension remain unchanged."),
        ("Event::ScriptOverrideDepthCueDistance",
         (('temp_v0', 'depth_cue_operand'),),
         "F01113 4A0A8.c:1972-1980 reads temp_v0 as short+1. Its sign controls D_800F4C6A, while low11bits feed func_80098194 only when enabled. The name covers the packed flag-and-distance operand, not simply distance; no sign/mask change."),
        ("Event::ScriptChangeRoom",
         (('sp10', 'door_record'),),
         "F01120 4A0A8.c:2018-2047 fills sp10 as _mpdRoomDoorSection_t from room/zone, entry tile/direction/elevation, and flags, then passes it to func_8007C580 or func_8007C654. State0 creates the transition, state2 acknowledges it. Native Field_MpdDoorRecord preserves this relationship and partially initialized layout."),
        ("Event::ScriptFadeOutAndWait",
         (('temp_v0', 'transition_phase'),),
         "F01121 4A0A8.c:2049-2059 reads temp_v0 from func_8007C398; phase0 initiates fade, phase5 completes waiting, all other phases keep waiting. Native Field_ScreenTransitionPhase and F01121 ledger corroborate the phase interpretation."),
        ("Event::ScriptTransformPalettes",
         (('sp18', 'actor_selection'), ('temp_a2', 'actor_id')),
         "F01128 4A0A8.c:2122-2149 builds/advances sp18 only for the actor-palette case; temp_a2 is the actor passed to func_8007B29C until sentinel1. Room/shared palette branches do not use them. F01208/F01209 establish selection contract; never substitute these spellings globally."),
        ("Event::ScriptControlRoomAnimation",
         (('temp_s0', 'room_effect_id'),),
         "F01138 4A0A8.c:2238-2254 reads temp_s0 from byte2. Start mode optionally finishes an active effect while skipping, then starts that effect; toggle mode passes the same id to func_800936F8. Native room-effect consumers interpret it as selected room effect, not animation frame."),
        ("Event::CommandSpecialOperation",
         (('sp10', 'diagnostic_message'),),
         "F01187 4A0A8.c:2651-2731 declares sp10[32] and uses it only as sprintf destination for the undefined-special-operation diagnostic. Other switch branches never access it. This names an existing diagnostic staging role without adding output or changing side effects."),
        ("Event::CommandSetCameraFollowing",
         (('new_var', 'eye_follow_bit'),),
         "F01194 4A0A8.c:2767-2772 masks byte1 with2 into new_var, then booleanizes it for the second argument of func_800BE36C. At 4018-4024 that argument becomes eye.follow, while the first becomes lookAt.follow. It is the raw eye-follow bit, not already a bool."),
        ("Event::CommandWaitScriptContext",
         (('temp_a0', 'target_thread_index'),),
         "F01202 4A0A8.c:2827-2835 masks byte1 with3 into temp_a0, then clears that cursor if bit7 is set or returns whether it remains active. This is the operand-selected thread, not the handler calling thread_index parameter, which stays meaningful and unchanged."),
        ("Event::CommandFinishEvent",
         (('var_s1', 'pending_text_box_count'),),
         "F01206 4A0A8.c:2854-2872 initializes var_s1=0, increments for each nonzero dismissal result among eight boxes, and only restores tick speed/returns VM completion when zero. This counts boxes still pending dismissal, not total boxes or a generic boolean."),
        ("Event::InitializeEventCamera",
         (('sp20', 'field_eye_position'), ('sp30', 'field_look_at')),
         "F01214 4A0A8.c:3014-3080 fills sp20 through vs_battle_getCameraPosition and sp30 through vs_battle_getCameraLookAt. Each is copied into its own current position and the opposite track target/partner position before resets. The unused underscore vector is outside the inventory and unchanged."),
        ("Event::SetEventFieldCameraOverride",
         (('arg0', 'field_camera_override'),),
         "F01216 4A0A8.c:3084 assigns arg0 to D_800F4BA0. Consumers: vs_battle_applyCameraState at 3117 onward continuously replaces event eye/look-at from field camera when nonzero; func_800BC1CC at 3173-3184 clears it and skips one scripted return. Name covers both uses instead of claiming only a one-shot skip."),
        ("Event::SetEventCameraOriginActor",
         (('arg0', 'origin_actor'),),
         "F01217 4A0A8.c:3086-3099 compares arg0 with0x2000, preserves world positions when leaving an actor-relative origin, stores it into D_800F4BA4->unk1E8, then refreshes the offset. F01218 consumes that actor/sentinel. origin_actor keeps the existing sentinel contract; no type change."),
        ("Event::UpdateEventCameraOrigin",
         (('sp10', 'origin_actor_location'),),
         "F01218 4A0A8.c:3101-3115 clears the offset for0x2000, otherwise fills sp10 using func_800A1108(originActor) and multiplies its position components by ONE. F01217 produces the selected origin. sp10 is the placement query, not the resulting fixed-point offset."),
        ("Event::RestoreEventFieldCamera",
         (('arg0', 'field_facing'), ('arg1', 'field_distance_mode'), ('sp10', 'field_eye_position'), ('sp20', 'field_look_at'), ('temp_s0', 'eye_travel_distance'), ('temp_v0', 'return_duration_frames')),
         "F01220 4A0A8.c:3173-3198 passes arg0 and narrowed arg1 into vs_battle_initialiseCameraFromSpherical, producing sp10/sp20 and destination tracks. Original 146C.c:9071-9105 names inputs facing/mode, with modes1/2 selecting distances0x600/0x900. temp_s0 is eye displacement magnitude; temp_v0 sums eye/look-at travel divided by0x28000 and is written into both camera-motion command durations. Override early return/narrowing are preserved."),
        ("Event::EventCameraGroundClearance",
         (('arg0', 'camera_transform'),),
         "F01225 4A0A8.c:3556-3599 installs arg0 as GTE rotation and translation, transforms four view-space offsets and samples terrain clearance. Native CommandCameraMotion/AdvanceTerrainCameraTrack pass the camera matrix scratch. Name describes a complete transform, not just rotation."),
        ("Event::CommandTweenCameraRoll",
         (('var_s0', 'roll_tween'),),
         "F01226 4A0A8.c:3601-3645 selects var_s0 from camera roll for0xE2 or captured-screen roll for0xE3, then initializes duration/current/start/delta/easing fields. F01227 advances the same typed state. roll_tween covers either target; missing-default behavior stays untouched."),
        ("Event::UpdateEventAngleTween",
         (('arg0', 'angle_tween'),),
         "F01227 4A0A8.c:3647-3661 advances arg0 elapsed time, computes start+delta*eased-fraction, clears duration at completion, and marks changed. Event-camera updater passes both camera and captured-screen roll. No member token is named angle_tween."),
        ("Event::EventCameraCompletionMask",
         (('temp_a2', 'completion_mask'),),
         "F01228 4A0A8.c:3663-3689 initializes temp_a2 from look-at idle state and ORs eye/roll/lens/effect/shake completion bits through0x80. Opcode 2788-2791 compares its requested mask with this result. Preserve char type/signedness; naming is not a completion-logic fix."),
        ("Event::UpdateEventScreenTransforms",
         (('sp10', 'screen_transform'), ('sp20', 'screen_tint')),
         "F01230 4A0A8.c:3812-3836 uses sp10 for both two-component scale and translation outputs and sp20 for RGB tint, only when respective tweens update. screen_transform deliberately avoids assigning sp10 solely a scale or solely an offset role."),
        ("Event::UpdateEventTripleTween",
         (('arg0', 'triple_tween'), ('a0', 'fraction_scratch'), ('v0', 'eased_fraction')),
         "F01231 4A0A8.c:3838-3860 advances arg0 and interpolates its three values. v0 is func_800BFEBC result; a0 points at0x1F800088 and stores/reads that fraction. Native Event_EasedFractionScratch.easedFraction remains a member and is not renamed. Snake-case eased_fraction is absent from the current body and differs from that member."),
        ("Event::UpdateEventLensTween",
         (('arg0', 'lens_tween'), ('new_var', 'delta_value_ptr')),
         "F01233 4A0A8.c:3927-3943 advances/clamps arg0 elapsed time and updates value/change state. new_var is specifically &arg0->unk6, dereferenced in start+delta*curve interpolation. Native LensTween identifies that member as deltaValue; pointer naming changes no member or scratch arithmetic."),
        ("Event::UpdateEventShakeOscillator",
         (('arg0', 'shake_oscillator'), ('temp_s1', 'total_phase_angle')),
         "F01234 4A0A8.c:3945-3980 computes temp_s1=arg0->unk4<<12 and uses it in rsin(temp_s1*elapsed/duration), times amplitude and a separate envelope. Setup at3862-3925 writes cycles from the instruction and resets elapsed; update clears duration. total_phase_angle means cycles times one4096-unit turn, not instantaneous frequency/frame count."),
        ("Event::SetEventCameraFollowing",
         (('arg0', 'look_at_follow'), ('arg1', 'eye_follow')),
         "F01236 4A0A8.c:4018-4024 assigns arg0 to lookAt.follow, arg1 to eye.follow, and clears both following latches. F01194 independently decodes instruction bits0/1 into these arguments; native track update consumes followRequest/isFollowing."),
        ("Event::StartEventCameraOrbitReturn",
         (('arg0', 'orbit_side_selector'), ('sp10', 'start_look_at'), ('sp20', 'destination_look_at'), ('sp30', 'start_eye_position'), ('sp40', 'destination_eye_position'), ('temp_s0', 'distance_mode_index'), ('temp_s3', 'saved_facing'), ('new_var', 'preceding_facing')),
         "F01238 4A0A8.c:4039-4072 reads temp_s3 from flag0xAF and temp_s0 from0xB0; new_var=temp_s3-1. arg0 modifies start facing as(new_var+arg0*2)&7. sp30/sp10 receive start eye/look-at and height adjustment; sp40/sp20 receive final eye/look-at. Producer146C.c:4720-4734 saves facing/512 and distance index0 for0x600, otherwise1; consumer146C.c:9071-9105 uses temp_s0+1 as distance mode. Do not assert selector is restricted to bool."),
        ("Event::ScaleEventCameraTrackDuration",
         (('arg0', 'camera_track'), ('sp10', 'remaining_displacement')),
         "F01239 4A0A8.c:4074-4080 subtracts arg0 destination from current position into sp10, takes fixed magnitude, converts to world units, and scales track duration by/1000. sp10 is displacement, not an absolute point; sign/fixed-point conversion stay untouched."),
        ("Event::ScaleEventCameraDuration",
         (('arg0', 'track_selector'), ('temp_s0', 'look_at_track'), ('temp_s1', 'eye_track'), ('var_a0', 'selected_track')),
         "F01240 4A0A8.c:4082-4104 binds temp_s0 to lookAt and temp_s1 to eye. arg0==2 scales both then averages durations; otherwise var_a0 selects lookAt for zero or eye for nonzero before one scaling call. Names cover the multi-valued selector and both-track path."),
        ("Event::ReadScriptFixedVector",
         (('arg0', 'destination'), ('arg1', 'operand_bytes')),
         "F01246 4A0A8.c:4148-4153 reads signed little-endian shorts from arg1 offsets0/2/4, multiplies by ONE, writes and returns arg0. Native camera-motion callers pass destination/displacement or scratch vectors with script+1/+4. operand_bytes is the six-byte payload, not the opcode instruction start."),
        ("Event::EvaluateRoomEventTriggers",
         (('arg0', 'trigger_request'), ('arg1', 'event_or_actor_id'), ('arg2', 'trigger_context'), ('arg3', 'context_kind')),
         "F01261 4A0A8.c:4302-4584 copies arg0 to type, supporting trigger mask plus requests0xFF/0x40; arg1 supplies direct event id for0x40 and first actor comparison in predicate6. arg2 is compared with second actor id when mode0, but dereferenced as tile bytes[0]/[2] when mode1. arg3 selects mode. Original146C.c:5260-5268/native Field_TryTriggerFacingTileEvent pass tile buffer/mode1; GameLoop passes actor number/mode0. Broad names cover both uses and preserve existing pointer/integer compatibility contract, not silently fix it."),
        ("Event::ExecuteScriptThread",
         (('arg0', 'thread_cursor'), ('arg1', 'thread_index')),
         "F01271 573B8.c:67-131 dereferences arg0 to get script, writes null through it on result3, and persists cursor on wait/yield. arg1 selects main-thread skip initiation and is passed as second argument to both opcode tables and native debug logging. Callers pass Event_threadCursors or &Event_threadCursors[i] with corresponding index. Already-published handler parameters are not repeated."),
        ("Event::OpcodeSize",
         (('arg0', 'instruction'),),
         "F01274 573B8.c:154 indexes D_800E9C30 with arg0[0]. F01271 passes its instruction pointer when advancing after ordinary/yielding instructions. This helper parameter was not covered by the published opcode-handler batch."),
        ("Event::ResolveScriptActorReference",
         (('arg0', 'actor_reference'),),
         "F01275 573B8.c:156-166 switches on arg0>>12: model lookup for type0, direct slot low5bits for type1, unchanged special reference for type2. Reviewed opcode producers pass encoded shorts. actor_reference describes all alternatives without claiming input is already a slot."),
        ("Event::QueueEventScriptLoad",
         (('arg1', 'destination_buffer'),),
         "F01277 573B8.c:223-229 computes LBA from global _evtFile, allocates a disc queue slot, and supplies arg1 only as destination to vs_main_cdEnqueue. Native callers pass Event_scriptBuffer or Event_chainedEventBuffer; first arguments mirror Event_eventId but are not consumed here."),
    )
    for old, new in bindings
]


REGEX_FIXES += [
    (rf"^{name}$", rf"\b{old}\b", new, evidence)
    for name, bindings, evidence in (
        ("Actors::EnableActorModel",
         (('arg0', 'actor_id'),),
         "F00245 146C.c:8623: arg0 indexes vs_battle_actors and forwards to display flag getter/setter; enables bit 0."),
        ("Actors::DisableActorModel",
         (('arg0', 'actor_id'),),
         "F00247 146C.c:8639: arg0 indexes vs_battle_actors and forwards to display flag getter/setter; clears bit 0."),
        ("Actors::SetActorHostile",
         (('arg0', 'actor_id'), ('arg1', 'hostile')),
         "F00251 146C.c:8686: arg0 is actor slot; nonzero arg1 selects kind 4, zero selects kind 8."),
        ("Actors::ActorHpPercentage",
         (('arg0', 'actor_id'), ('temp_a0', 'stats'), ('var_v1', 'hp_percentage')),
         "F00252 146C.c:8697: stats from actor->unk3C, currentHP*100/maxHP with positive HP clamped to at least 1 percent."),
        ("Actors::RestoreActorVitals",
         (('arg0', 'actor_id'), ('temp_a1', 'stats')),
         "F00253 146C.c:8714: actor slot resolves stats, resets risk/recovery and restores HP/MP and six limb HP values, removes statuses."),
        ("Actors::FindActorByModelId",
         (('arg0', 'model_id'),),
         "F00293 146C.c:9127 compares each actor unk18/modelId to arg0; returns matching slot."),
        ("Actors::CountActorsByDisposition",
         (('arg0', 'actor_kind'), ('var_a2', 'matching_actor_count')),
         "F00295 146C.c:9141: count begins at zero and increments for actor->unk1C/kind equal to arg0."),
        ("Actors::IsActorRetainedAfterDefeat",
         (('arg0', 'actor_id'),),
         "F00296 146C.c:9155 returns actor slot unk26/retainAfterDefeat, or zero for absent actor."),
        ("Actors::SetActorLogicEnabled",
         (('arg0', 'actor_id'), ('arg1', 'enabled')),
         "F00299 146C.c:9179: nonzero arg1 resumes commandState from 0x20 and clears kind high bit; zero sets suspended command/high bit and markers."),
        ("Actors::SetOtherActorLogicEnabled",
         (('arg0', 'enabled'),),
         "F00300 146C.c:9199 forwards arg0 to F00299 for slots 1..15 and returns the enable value."),
        ("Actors::SetActorDetached",
         (('arg0', 'actor_id'), ('arg1', 'detached'), ('temp_a0', 'actor'), ('temp_s0', 'list_changed')),
         "F00301 146C.c:9209: nonzero arg1 unlinks actor and locks animation, zero relinks after Ashley. temp_s0 is either link/unlink result gating AI rebuild."),
        ("Actors::ResetActorMotionState",
         (('arg0', 'actor_id'), ('sp10', 'sound_override'), ('s0', 'shadow_blend'), ('temp_s1', 'actor'), ('var_a2', 'initial_animation_id')),
         "F00808 30DB0.c:2073-2145 resets actor record, sp10.unk0=0 clears sound override via func_8009F990, var_a2 selects animation 1 for Ashley or 0x62, s0 points to shadow blend state and copies radius rates."),
        ("Actors::SetActorCompanion",
         (('arg0', 'actor_id'), ('arg1', 'companion_slot'), ('arg2', 'companion_state'), ('arg3', 'companion_color'), ('temp_t0', 'actor')),
         "F00841 30DB0.c:3388 writes companion slot unk17FD, packed state unk6F4 and color unk6F8 from the two input color-code-sized objects."),
        ("Actors::AttachActorToParentBone",
         (('arg0', 'actor_id'), ('arg1', 'parent_actor_id'), ('arg2', 'parent_bone_index'), ('temp_a0', 'actor')),
         "F00851 30DB0.c:3613 writes attachParent unk12 and attachBone unk13, decrements prior parent child count, clears transform and sets attachment flag. arg3 has no use."),
        ("Actors::ReleaseActorAttachmentLock",
         (('arg0', 'actor_id'), ('temp_a0', 'actor'), ('temp_v1', 'parent_actor_id')),
         "F00852 30DB0.c:3635 reads attachParent; 0xFF, parent index and parent bone count validate link before clearing isAttached. arg1 is unused."),
        ("Actors::SetActorDisplayFlags",
         (('arg1', 'display_flags'), ('temp_t0', 'actor'), ('temp_a0', 'gimmick'), ('new_var', 'equipment_display_bit')),
         "F00864 30DB0.c:3950-3984: arg1 bits control skip/visibility/freeze/sound; new_var carries bit11 then bit12 to weapon/shield model unk8_4. Name describes exact display-flag extraction without claiming undocumented bit semantics."),
        ("Actors::ActorStateFlags",
         (('temp_a1', 'actor'), ('temp_v1', 'gimmick'), ('a2', 'animation_lock_mask')),
         "F00872 30DB0.c:4065-4180 reads actor or gimmick. At 4129 a2=0x1000000 tests packed actor word at unk5B4-8=unk5AC, bit24; actor header/field ledger names this animationLocked, contributing output bit0x2000."),
        ("Actors::ActorPlacement",
         (('arg0', 'actor_id'), ('arg1', 'placement_out'), ('var_v1', 'placed_object')),
         "F00873 30DB0.c:4182 writes packed tile, position words and facing to caller output; source is actor or layout-compatible gimmick fallback, so not misleadingly actor-only."),
        ("Actors::FindActorAtFlightPosition",
         (('arg0', 'actor_id'), ('arg1', 'position'), ('temp_a2', 'other_actor')),
         "F00902 3A1A0.c:350 checks slots 2..<arg0, locked flight actors, packed XY and Z equality with position. arg0 is both actor id and exclusive predecessor limit."),
        ("Actors::SeparateActorFlightPlacement",
         (('arg0', 'actor_id'), ('arg1', 'position')),
         "F00903 3A1A0.c:365: resolves actor collision height and repeatedly tests adjusted Y via F00902, writes nonoverlapping height back to input position."),
        ("Actors::ActorSavedPlacement",
         (('arg0', 'actor_id'), ('arg1', 'movement_class'), ('arg2', 'placement_out'), ('temp_a0', 'actor')),
         "F00906 3A1A0.c:448 returns saved placement, substitutes current tile when flight-locked and class 0x10/0x20, writes current facing/16. Same class values select flight placement in F00901."),
        ("Actors::FindNearestActorAhead",
         (('arg0', 'actor_id'), ('arg1', 'horizontal_range'), ('arg2', 'vertical_range')),
         "F00907 3A1A0.c:460: source slot arg0, horizontal squared radius arg1*arg1 and vertical interval [-arg2,+arg2]; facing cone filters nearest actor. Original 146C.c:7648 calls (0,0xA0,0x80)."),
        ("Actors::QueueActorBaseSequenceLoad",
         (('arg1', 'actor_id'), ('sp10', 'asset_request')),
         "F00170 146C.c:6444 creates type7/base sequence request for slot arg1; arg0 explicitly __attribute__((unused)) in original."),
        ("Actors::QueueActorWeaponSequenceLoad",
         (('arg0', 'actor_bytes'), ('arg1', 'actor_id'), ('arg2', 'weapon_category'), ('arg3', 'spawn_flags'), ('sp10', 'asset_request')),
         "F00172 146C.c:6480 and caller F00173: arg0 is the actor allocation prefix; byte37 is equippedWeaponCategory (native typed access). arg2 receives that category; arg3 low2 bits gate hostile sequence load; arg1 indexes request slot."),
        ("Actors::InterruptActorAction",
         (('arg0', 'actor_id'), ('arg1', 'animate')),
         "F00255 146C.c:8738 forwards actor id and animation gate to F00836/BeginActorDeath; body inspected in ActorState.c:344-381."),
        ("Actors::QueueSharedSequenceWithTextures",
         (('arg2', 'texture_slot'),),
         "F00280 146C.c:9010 assigns objData.unk11/textureSlot in type7, subkind2 request."),
        ("Actors::QueueSequenceEffectTextures",
         (('arg2', 'texture_slot'),),
         "F00281 146C.c:9022 assigns objData.unk11/textureSlot in type9 ETM request."),
        ("Actors::ReleaseSequenceEffectTextures",
         (('arg0', 'bank_id'),),
         "F00282 146C.c:9033 forwards to func_8009D208; Content::ReleaseSequenceEffectTexture validates bank 1..3 and indexes shared texture pages at bank_id-1 (ActorAssets.c:1682)."),
        ("Actors::SetSharedSequenceBank",
         (('arg0', 'bank_id'),),
         "F00283 146C.c:9035 forwards to F00801, whose valid bank range is one-based 1..3."),
        ("Actors::ReturnActorToIdle",
         (('arg0', 'actor_id'),),
         "F00291 146C.c:9115 forwards actor slot to RestoreActorIdlePose; all-actor caller loops slots."),
        ("Actors::SelectSharedSequenceBank",
         (('arg0', 'bank_id'), ('temp_s0', 'actor')),
         "F00801 30DB0.c:1894 validates arg0-1<3; each actor texture rows are compared between old/current and requested bank, then global bank index becomes arg0-1."),
        ("Actors::RequestActorAnimationAdvance",
         (('arg0', 'actor_id'),),
         "F00811 30DB0.c:2188 indexes actor record and sets unk5AC_18/advanceRequested."),
        ("Actors::SetActorWeaponDrawn",
         (('arg0', 'actor_id'), ('arg1', 'weapon_drawn'), ('arg2', 'transition_mode'), ('temp_s0', 'actor'), ('temp_s1', 'draw_sound_base'), ('temp_v1', 'equipment_model'), ('var_s2', 'blend_frames')),
         "F00813 30DB0.c:2219-2293: mode2 sets state/model flags directly; modes0/1 choose draw/sheathe animation pairs and 5/16 blend frames. temp_s1 table base feeds PlayActorSoundPair (+1 for sheathe); temp_v1 iterates blade/shield slots."),
        ("Actors::PlayActorWeaponDrawSound",
         (('arg0', 'actor_id'), ('arg1', 'sound_offset'), ('temp_a0', 'actor')),
         "F00814 30DB0.c:2296 indexes actor and adds arg1 to drawn-weapon sound table base before sound playback."),
        ("Actors::PlayActorActionPose",
         (('arg0', 'actor_id'), ('arg1', 'facing_target'), ('arg2', 'pose_kind'), ('temp_s0', 'actor'), ('var_s1', 'animation_id')),
         "F00820 30DB0.c:2451 dispatches pose kind1..7 into animation ids; computes facing delta toward arg1 vector then calls PlayActorAnimation with selected var_s1."),
        ("Actors::IsActorActionAnimationBusy",
         (('arg0', 'actor_id'), ('temp_a1', 'actor'), ('temp_a0', 'base_animation_id')),
         "F00825 30DB0.c:2627 reads actor/gimmick existence and busy action state. temp_a0 starts animationId, removes drawn-weapon +100 variant before busy-range checks."),
        ("Actors::RestoreActorIdlePose",
         (('arg0', 'actor_id'),),
         "F00826 30DB0.c:2685 avoids special transition animations 0x63/0xC7; passes actor id to RestoreActorBasePose animation1."),
        ("Actors::RestoreActorNeutralPose",
         (('arg0', 'actor_id'),),
         "F00827 30DB0.c:2693 forwards actor id to RestoreActorBasePose animation6."),
        ("Actors::AnimationBattleWindowFrame",
         (('arg0', 'actor_id'), ('s0', 'pose'), ('temp_s1', 'saved_event_frame'), ('temp_s2', 'saved_event_script'), ('temp_v0', 'battle_window_frame')),
         "F00840 30DB0.c:3373 reads current pose event frame/script, invokes RunAnimationEvents mode3 (query battle-window frame), restores both saved values and returns query result. Original event consumer44F14.c:185-205: mode3 skips all opcodes except2 and returns state->unk542/nextEventFrame, confirming the returned battle-window frame."),
        ("Actors::SetActorSoundOverrides",
         (('arg0', 'actor_id'), ('arg1', 'sound_override')),
         "F00842 30DB0.c:3401 forwards sound override to ApplyActorSoundOverrides, with actor id -1 applying to all17 slots."),
        ("Actors::ApplyActorSoundOverrides",
         (('arg0', 'actor_id'), ('arg1', 'sound_override'), ('temp_s0', 'actor'), ('temp_a2', 'override_kind'), ('temp_v1', 'previous_kind'), ('sp10', 'pan'), ('sp14', 'volume')),
         "F00843 30DB0.c:3414-3469 reads incoming/current override kinds, initializes/targets sound state. func_800AD62C provides sp10/sp14. Native Audio/ActorSound.c:29-58 and Audio/Sfx.c:152-198 prove first output is screen-X-derived pan (neutral128), second depth-attenuated volume. Original callee verification: 40564.c:2789-2819 func_800AD62C splits high16 into first output and low16 into second. SLUS_010.40/main.c:9879-9924 vs_main_computeSfxPan returns (screen-X pan<<16)+(127-depth_attenuation), confirming sp10=pan/sp14=volume independently of native names."),
        ("Actors::SetActorTravelAnimation",
         (('arg0', 'actor_id'), ('arg1', 'animation_id'), ('temp_a0', 'actor')),
         "F00845 30DB0.c:3480 writes travel animation unk6E5; requested -1 selects default0xD."),
        ("Actors::ActorTravelAnimation",
         (('arg0', 'actor_id'), ('arg1', 'animation_out'), ('temp_a0', 'actor')),
         "F00846 30DB0.c:3498 returns travel animation unk6E5 through int pointer."),
        ("Actors::QueueActorAnimation",
         (('arg0', 'actor_id'), ('arg1', 'animation_id'), ('arg2', 'blend_frames'), ('arg3', 'start_frame'), ('temp_a0', 'actor')),
         "F00855 30DB0.c:3710 stores queued animation/parameters. Original 44F14.c:1252 skeletal advancement and registered-owned SequencePlayback.c:140-143 pass queuedParameterA/B to PlayActorAnimationAtFrame as blend frames/start frame. FIELD_LEDGER T00239.092/.093 hints agree; no field rename/promotion included."),
        ("Actors::PlayActorAnimationAtFrame",
         (('arg0', 'actor_id'), ('arg1', 'animation_id'), ('arg2', 'blend_frames'), ('arg3', 'start_frame'), ('temp_s0', 'actor'), ('v0', 'play_result')),
         "F00857 30DB0.c:3850 forwards third argument as PlayActorAnimation fourth/blend argument; start frame subtracts tickSpeed/2 and temporarily becomes framesPerStep for initial advancement. v0 propagates failure."),
        ("Actors::SetActorTransitionMode",
         (('arg0', 'control_byte'),),
         "F00863 30DB0.c:3948 only assigns D_800E8FC0/Actors_reservedControlByte; control_byte names the proven destination, not an unproved transition mode."),
        ("Actors::SetActorAnimationLocked",
         (('arg0', 'actor_id'), ('arg1', 'locked')),
         "F00865 30DB0.c:3987 sets unk5AC_24/animationLocked; consumed by PlayActorAnimation guard."),
        ("Actors::PlayActorTurnAnimation",
         (('arg0', 'actor_id'), ('arg1', 'target_facing'), ('temp_a3', 'actor'), ('var_a1', 'facing_delta'), ('var_a2', 'turn_animation_id'), ('v1', 'current_animation_id')),
         "F00890 38C1C.c:578 normalizes target-current facing to +/-0x800; selects jump turn0x1F or left/right0x11/0x12, checks current animation and queues selected turn animation."),
        ("Actors::SetActorBonePoseLock",
         (('arg0', 'actor'), ('arg1', 'bone_index'), ('arg2', 'lock_pose'), ('sp10', 'bone_length'), ('temp_s2', 'bone')),
         "F00984 40564.c:2199 resolves armature by bone index; lock gate reserves a saved matrix slot, unlock clears poseLockSlot. ActorAnchorMatrix forwards fourth output to ActorBoneWorldMatrix, which writes model->bones[index].length (38C1C.c:195-244); sp10 receives that unused length, not an arbitrary scratch flag."),
        ("Actors::ClearActorBonePoseLocks",
         (('arg0', 'actor'),),
         "F00985 40564.c:2221 clears both lockedBone entries and each model bone poseLockSlot."),
        ("Actors::DecodeInitialPoseVector",
         (('arg0', 'pose_key'), ('arg1', 'encoded_vector'), ('arg2', 'base_animation_index'), ('a3', 'component_value')),
         "F00990 40564.c:2561-2590 and inline alias2592-2621: three big-endian16 components doubled into pose key; pad6 for standalone base-animation index0xFF, else0. Caller InitializeActorAnimationPose at2623 uses header byte2 as referenced/base animation index. Preserve raw pointer arithmetic including [339]; no layout change."),
        ("Actors::FindActorAnimationHeader",
         (('arg0', 'actor_context'), ('arg1', 'animation_id'), ('arg2', 'header_out')),
         "F00993 40564.c:2719: void* context casts to actor, animation id selects owned/common/battle/shared bank and mapped header index; header_out is null on absent mapping, bank returned separately."),
        ("Actors::RestartActorAnimationEvents",
         (('arg0', 'actor'), ('sp10', 'animation_header'), ('v0', 'animation_bank'), ('temp_a3', 'event_data'), ('var_v1', 'next_event_frame'), ('var_a3', 'event_script')),
         "F00998 44F14.c:610-635: header[2] points to event data; first byte or FF plus next byte decodes next event frame; remaining bytes become blendPose.eventScript. Names preserve native checked offset resolution and event restart mode2."),
        ("Actors::UpdateActorAnimationBlend",
         (('arg0', 'actor'), ('v5b2', 'frames_per_step')),
         "F01009 44F14.c:1146: actor supplies pose, blend counters, bone count and motion/look-at updates across blend and nonblend paths."),
        ("Actors::UpdateMotionAndLookAt",
         (('arg1', 'skip_motion_update'),),
         "F04130 44F14.c:1486,1507-1537 onward: nonzero arg1 skips the scale/turn/root-motion countdown advancement block while root-offset and look-at handling remain; no inference that it disables all animation."),
        ("Actors::UpdateAshleyMovementInput",
         (('unksp10', 'placement'), ('temp_a0', 'stick_y'), ('temp_a1', 'stick_x'), ('temp_s0', 'stats'), ('temp_s1', 'state_flags')),
         "F00079 146C.c:2837-2894: placement comes from ActorPlacement and feeds camera tile constraint; stats from Ashley actor; state flags gate carrying/jump movement. Analog rStickX/Y subtract midpoint128 then deadzone64; axes written into move-input slots1/3, with X negated."),
        ("Actors::ConsumeActorMovementOffset",
         (('arg0', 'actor'), ('arg1', 'offset_out')),
         "F00160 146C.c:5577 consumes signed fixed-point commandArg0/1/2 as X/Z/Y offsets, limiting each emitted component to +/-8 and decrementing accumulated commands by0x8000 or clearing remainder."),
        ("Actors::BeginScriptedActorPath",
         (('arg0', 'actor_id'), ('arg1', 'target_position')),
         "F00297 146C.c:9163 sets collision bypass bit2 and calls navigation override with target vector X/Z."),
        ("Actors::IsScriptedActorPathFinished",
         (('arg0', 'actor_id'), ('temp_v0', 'navigation_status')),
         "F00298 146C.c:9169 queries navigation status; zero clears bypass bit2 and returns finished, otherwise compares status2. Native query now accepts actor id; preserve that adaptation."),
        ("Actors::ResetActorMovement",
         (('arg0', 'actor_id'), ('temp_a1', 'actor')),
         "F00302 146C.c:9232 resets battle actor commandState except suspended0x20, then stops corresponding actor motion."),
        ("Actors::StopActorMotion",
         (('arg0', 'actor_id'), ('temp_s0', 'actor')),
         "F00815 30DB0.c:2304 actor lookup, clears movement modes, animation/turn counters and packed root motion."),
        ("Actors::ClearActorMovementModes",
         (('arg0', 'actor_id'), ('temp_a3', 'actor')),
         "F00816 30DB0.c:2315 clears jumpCountdown, movementMode, weaponPoseActive, poseState, locomotionState and movementPending on indexed actor."),
        ("Actors::ActorFacingDeltaToPoint",
         (('arg0', 'actor'), ('arg1', 'target_point'), ('temp_a0', 'delta_z'), ('temp_a1', 'delta_x'), ('var_v1', 'heading_delta_work'), ('var_v0', 'wrapped_facing_delta')),
         "F00821 30DB0.c:2491: X/Z target deltas feed atan2; var_v1 holds heading then heading minus facing; var_v0 wraps difference to [-0x800,0x800]. Coincident/null targets retain existing behavior."),
        ("Actors::ResetActorRunMomentum",
         (('arg0', 'actor_id'), ('temp_a0', 'actor')),
         "F00868 30DB0.c:4015 clears actor momentumDistance/runFrames/isRunning."),
        ("Actors::SetActorWalkRunSpeeds",
         (('arg0', 'actor_id'), ('arg1', 'walk_speed_fixed'), ('arg2', 'run_speed_fixed'), ('temp_a0', 'actor')),
         "F00869 30DB0.c:4025 divides incoming speeds by ONE into actor walk/run fields; sets runSpeedFF when walk>=run. Preserve fixed-point scaling and sentinel."),
        ("Actors::SetActorTravelStepCounts",
         (('arg0', 'actor_id'), ('arg1', 'move_steps_per_tile'), ('arg2', 'turn_steps_per_revolution'), ('temp_a0', 'actor')),
         "F00870 30DB0.c:4037 writes moveRate/turnRate (negative inputs become1). Original40564.c:934-945 computes ceil(distance*moveRate/128);1001-1013 computes ceil(abs(angle)*turnRate/ONE), proving these are duration scales, not world/angular speeds."),
        ("Actors::ActorTravelStepCounts",
         (('arg0', 'actor_id'), ('arg1', 'move_steps_per_tile_out'), ('arg2', 'turn_steps_per_revolution_out'), ('temp_a0', 'actor')),
         "F00871 30DB0.c:4056 exports moveRate and turnRate through distinct outputs. Units established by F00958/F00960 arithmetic, not guessed from field spelling."),
        ("Actors::ApplyActorMovementVector",
         (('arg0', 'actor'),),
         "F00894 38C1C.c:712 adds queued movement vector to actor position, recomputes tile and turns toward request facing."),
        ("Actors::UpdateActorTeleportTransition",
         (('arg0', 'actor'),),
         "F00896 3A1A0.c:86-151 consumes/advances actor reactionState teleport phases, animation/appearance, tile translation and companion completion."),
        ("Actors::CancelActorTeleportTransition",
         (('arg0', 'actor'),),
         "F00897 3A1A0.c:153 restores actor tile from saved route-compatible storage, clears queued animation, sets reactionState4 and recursively cancels companion. Naming does not certify the legacy sharedSequences[4] view."),
        ("Actors::UpdateActorTravelTransition",
         (('arg0', 'actor'),),
         "F00898 3A1A0.c:164-237 advances actor poseState travel transitions using route target, turn/move durations, root-motion transitionFrames and support contact."),
        ("Actors::StepActorTransitionDisplacement",
         (('arg0', 'actor'), ('temp_a1', 'component_step')),
         "F00899 3A1A0.c:239-254: temp_a1 reused per XYZ root-motion residual divided by transitionFrames, subtracted from residual and added to position; countdown decremented once."),
        ("Actors::SetActorFallDestination",
         (('arg0', 'actor'), ('arg1', 'fall_direction'), ('var_a2', 'offset_x'), ('var_a3', 'offset_z')),
         "F00900 3A1A0.c:256-272: each signed direction selects +/-0x3F offset, added to current X/Z destination; destinationMode1."),
        ("Actors::ClearActorMovementRequest",
         (('arg0', 'actor'),),
         "F00925 3A1A0.c:1336 resets turn request mode(default1, Ashley0), direction, distance and facing."),
        ("Actors::UpdateActorSteering",
         (('arg0', 'actor'), ('arg1', 'target_facing'), ('arg2', 'turn_request')),
         "F00927 3A1A0.c:1920: target facing drives wrapped angle, request.restartTurn determines whether to adjust remaining turn or restart12 steps; large actor radius enforces angle/32 minimum duration."),
        ("Actors::MoveActorToTileAtRate",
         (('arg0', 'actor_id'), ('arg1', 'tile_target'), ('arg2', 'move_steps_per_tile'), ('arg3', 'turn_steps_per_revolution')),
         "F00946 40564.c:522 sets route move/turn rate scales (default fields for -1); immediate zero/zero updates placement/facing. Tile target encodes X/layer/Z/facing and becomes world routeTarget."),
        ("Actors::MoveActorToPositionAtRate",
         (('arg2', 'move_steps_per_tile'), ('arg3', 'turn_steps_per_revolution')),
         "F00947 40564.c:574 assigns routeMoveRate/routeTurnRate or default fields for -1, uses turn rate to face world target, routeUsesDuration0; zero pair moves immediately. Existing unrelated double-X support query retained."),
        ("Actors::TranslateActorToTileAtRate",
         (('arg0', 'actor_id'), ('arg1', 'tile_target'), ('arg2', 'move_steps_per_tile'), ('temp_s0', 'actor')),
         "F00948 40564.c:661 forwards tile translation duration sentinel-1 and selected moveRate, then records current target tile fields."),
        ("Actors::TurnActorTowardTileAtRate",
         (('arg0', 'actor_id'), ('arg1', 'tile_target'), ('arg2', 'turn_steps_per_revolution'), ('new_var', 'actor')),
         "F00949 40564.c:676 forwards turn duration sentinel-1 and selected actor turnRate to SetActorTileTurn."),
        ("Actors::TranslateActorByAtRate",
         (('arg0', 'actor_id'), ('arg1', 'translation'), ('arg2', 'move_steps_per_tile'), ('temp_s0', 'saved_move_rate'), ('temp_s1', 'actor')),
         "F00950 40564.c:687: vector is relative translation; save actor moveRate, override when arg2!=-1, call SetActorTranslation duration-1 and restore. Saved value is not actor index."),
        ("Actors::TranslateActorToAtRate",
         (('arg0', 'actor_id'), ('arg1', 'target_position'), ('arg2', 'move_steps_per_tile'), ('sp10', 'translation'), ('temp_s0', 'saved_move_rate'), ('temp_s1', 'actor')),
         "F00951 40564.c:705 computes target-current XYZ translation, saves/overrides/restores moveRate around SetActorTranslation duration-1."),
        ("Actors::TurnActorByAtRate",
         (('arg0', 'actor_id'), ('arg1', 'yaw_delta'), ('arg2', 'turn_steps_per_revolution'), ('temp_s0', 'saved_turn_rate'), ('temp_s1', 'actor')),
         "F00952 40564.c:728 saves/overrides/restores turnRate around relative-yaw SetActorYawTurn duration-1."),
        ("Actors::TurnActorToAtRate",
         (('arg0', 'actor_id'), ('arg1', 'target_yaw'), ('arg2', 'turn_steps_per_revolution'), ('temp_s0', 'saved_turn_rate'), ('temp_s1', 'actor')),
         "F00953 40564.c:746 arg1 begins absolute yaw then is mutated to wrapped delta; save/override/restore turnRate around yaw setter. Name denotes public input, with mutation explicitly documented here."),
        ("Actors::RotateActorByAtRate",
         (('arg0', 'actor_id'), ('arg1', 'rotation_delta'), ('arg2', 'turn_steps_per_revolution'), ('temp_s0', 'saved_turn_rate'), ('temp_s1', 'actor')),
         "F00954 40564.c:774 forwards relative XYZ rotation while temporarily overriding turnRate; -1 keeps default."),
        ("Actors::RotateActorToAtRate",
         (('arg0', 'actor_id'), ('arg1', 'target_rotation'), ('arg2', 'turn_steps_per_revolution'), ('sp10', 'rotation_delta'), ('temp_s0', 'saved_turn_rate'), ('temp_s1', 'actor')),
         "F00955 40564.c:793 computes wrapped target-current XYZ rotation delta; save/override/restore turnRate around SetActorRotation duration-1."),
        ("Actors::MoveActorToTileOverTime",
         (('arg0', 'actor_id'), ('arg1', 'tile_target'), ('arg2', 'move_steps'), ('arg3', 'turn_steps'), ('temp_s0', 'actor'), ('temp_a1', 'yaw_delta')),
         "F00956 40564.c:842 routeUsesDuration1 means arg2 is literal later translation steps, arg3 passed as literal tile-turn steps. Zero pair immediately translates and turns; local target facing*16-current is yaw delta."),
        ("Actors::TranslateActorToTileOverTime",
         (('arg0', 'actor_id'), ('arg1', 'tile_target'), ('arg2', 'move_steps'), ('temp_s0', 'actor')),
         "F00957 40564.c:874 forwards literal move steps with actor moveRate fallback to SetActorTileTranslation and stores current tile."),
        ("Actors::SetActorTileTranslation",
         (('arg0', 'actor_id'), ('arg1', 'tile_target'), ('arg2', 'move_steps'), ('arg3', 'move_steps_per_tile')),
         "F00958 40564.c:886-946: tile centers=(coordinate*128)+64; move_steps0 immediate, -1 computes ceil(Euclidean distance*arg3/128), otherwise sets currentAnimation countdown directly."),
        ("Actors::TurnActorTowardTileOverTime",
         (('arg0', 'actor_id'), ('arg1', 'tile_target'), ('arg2', 'turn_steps')),
         "F00959 40564.c:948 forwards tile target, duration and default turnRate to SetActorTileTurn."),
        ("Actors::SetActorTileTurn",
         (('arg0', 'actor_id'), ('arg1', 'tile_target'), ('arg2', 'turn_steps'), ('arg3', 'turn_steps_per_revolution')),
         "F00960 40564.c:953-1014: computes target tile heading, normalizes delta, duration0 immediate; -1 computes ceil(abs(angle)*arg3/ONE), otherwise uses literal turn steps."),
        ("Actors::SetActorMotion",
         (('arg0', 'actor_id'), ('arg1', 'translation'), ('arg2', 'move_steps')),
         "F00961 40564.c:1016 direct wrapper of SetActorTranslation (relative vector and step count with0/-1 sentinels)."),
        ("Actors::TranslateActorToOverTime",
         (('arg0', 'actor_id'), ('arg1', 'target_position'), ('arg2', 'move_steps'), ('sp10', 'translation'), ('temp_a3', 'actor')),
         "F00962 40564.c:1018 computes XYZ target-current delta and forwards relative vector and duration to SetActorTranslation."),
        ("Actors::SetActorTranslation",
         (('arg0', 'actor_id'), ('arg1', 'translation'), ('arg2', 'move_steps')),
         "F00963 40564.c:1032 copies relative translation to rootMotion;0 adds immediately; -1 computes ceil(length*moveRate/128); other values are countdown. No change to signed/overflow behavior."),
        ("Actors::SetActorYawMotion",
         (('arg0', 'actor_id'), ('arg1', 'yaw_delta'), ('arg2', 'turn_steps')),
         "F00964 40564.c:1089 direct wrapper of SetActorYawTurn with relative yaw and step count."),
        ("Actors::TurnActorToOverTime",
         (('arg0', 'actor_id'), ('arg1', 'target_yaw'), ('arg2', 'turn_steps'), ('temp_a1', 'yaw_delta'), ('var_v1', 'wrapped_yaw_delta'), ('temp_v0', 'actor')),
         "F00965 40564.c:1091 computes short yaw delta then adjusts by ONE for shortest arc before SetActorYawTurn duration."),
        ("Actors::TurnActorToAtAngularSpeed",
         (('arg0', 'actor_id'), ('arg1', 'target_yaw'), ('arg2', 'angular_speed'), ('temp_v0', 'actor'), ('var_v0', 'absolute_yaw_delta'), ('arg2_2', 'turn_steps')),
         "F00966 40564.c:1115-1147: arg1 becomes wrapped relative yaw; absolute angle divided by absolute angular speed, rounded up, produces arg2_2 duration. The latter is a manually discovered supplemental candidate excluded by the inventory regex."),
        ("Actors::SetActorYawTurn",
         (('arg0', 'actor_id'), ('arg1', 'yaw_delta'), ('arg2', 'turn_steps'), ('speed', 'turn_step_numerator')),
         "F00967 40564.c:1149-1193: local speed is NOT a speed: abs(yaw_delta)*turnRate is divided by ONE and rounded up to turnFrames. Rename numerator rather than blanket-retain spelling speed. Other arguments are actor slot, relative yaw, duration with0/-1 sentinels."),
        ("Actors::SetActorRotationMotion",
         (('arg0', 'actor_id'), ('arg1', 'rotation_delta'), ('arg2', 'turn_steps')),
         "F00968 40564.c:1195 direct wrapper of relative XYZ SetActorRotation and step count."),
        ("Actors::SetActorRotation",
         (('arg0', 'actor_id'), ('arg1', 'rotation_delta'), ('arg2', 'turn_steps')),
         "F00969 40564.c:1197: stores relative XYZ rotation; immediate0, default-rate -1 uses largest absolute component*turnRate/ONE rounded up, otherwise duration."),
        ("Actors::ScaleActorToOverTime",
         (('arg0', 'actor_id'), ('arg1', 'target_scale'), ('arg2', 'scale_steps'), ('temp_a3', 'actor')),
         "F00970 40564.c:1244:0 writes XYZ scales immediately; otherwise saves scale deltas and scaleFrames countdown, short-circuiting zero delta."),
        ("Actors::UpdateActorRoute",
         (('arg0', 'actor_id'), ('arg1', 'actor'), ('temp_a1', 'target_facing')),
         "F00986 40564.c:2233: route phases turn then translate, choosing AtRate or OverTime from routeUsesDuration. temp_a1 reads routeTarget.pad, rejects0x8000 sentinel and masks0xFFF for final facing."),
        ("Actors::UpdateActorJumpAndFall",
         (('arg0', 'actor_id'),),
         "F00988 40564.c:2297 and2398-2434: actor id feeds sounds, animations and Ashley-specific checks. speed is reused for signed X then Z root-motion component during destination overshoot test, not turn-duration numerator as in F00967."),
        ("Actors::UpdateActorAuxiliaryMotion",
         (('arg0', 'gimmick'), ('a2', 'shadow_blend'), ('temp_lo', 'blend_step')),
         "F01011 44F14.c:1225-1250: gimmick shadow blend state at unk1864; quotient temp_lo reused for level/expand/shrink residual divided by remaining frames, updates value and decrements associated countdown."),
    )
    for old, new in bindings
]


# Individually reviewed field bindings; structural fixups run first.

REGEX_FIXES += [
    (rf"^{name}$", rf"\b{old}\b", new, evidence)
    for name, bindings, evidence in (
        ("Field::DisableTrapAtPosition",
         (('arg0', 'trap_position'),),
         "F00065 World X/Z divided by128 select room trigger; marks it enabled/disabled via activation1 and updates AI obstacle tile."),
        ("Field::ApplyUnlockEffectToRoomObject",
         (('arg0', 'target_position'), ('sp10', 'room_object_count'), ('temp_v0', 'door_records'), ('temp_v0_2', 'chest_records'), ('new_var2', 'lock_level_upper_exclusive'), ('new_var3', 'minimum_breakable_lock_level')),
         "F00067 Input position.pad encodes object class/index. Door/chest tables return count through one stack integer; lock levels2..3 can be broken by risk roll, upper bound4 exclusive."),
        ("Field::InteractWithLockedRoomObject",
         (('arg0', 'door_trigger'), ('sp10', 'interaction_target_position')),
         "F00134 Lock-trigger record drives inventory key/sigil handling and notifications; short vector's X/Z are tile centers passed to actor pose5."),
        ("Field::TriggerActorTrap",
         (('arg0', 'trap_trigger'), ('arg1', 'victim_placement')),
         "F00135 Trigger kind chooses trap action/effect; actor location query supplies range origin; existing targetActor parameter selects victim."),
        ("Field::TryOpenChest",
         (('arg0', 'chest_trigger'), ('sp10', 'interaction_target_position'), ('var_s1', 'can_open')),
         "F00136 Trigger supplies target center and key state; boolean indicates unlock success and selects actor pose2/3. Second formal is never read, while dispatch caller passes actorQuery.actorIndex."),
        ("Field::BeginTrapInteraction",
         (('arg0', 'trap_trigger'), ('arg1', 'victim_location'), ('arg2', 'victim_actor_index')),
         "F00137 Stores trigger, complete actor location and actor index into interactionFocus, resets idle poses, and optionally displays trap-kind notification."),
        ("Field::BeginChestInteraction",
         (('arg0', 'chest_trigger'), ('arg1', 'interacting_actor_index'), ('sp10', 'interaction_target_position'), ('var_v0', 'target_tile_coordinate')),
         "F00138 Saves trigger and queried interacting actor. Reused scalar reads tileX then tileZ to form centered world vector for Ashley opening pose."),
        ("Field::WaitForChestCameraFocus",
         (('sp10', 'interaction_camera_target'),),
         "F00139 Vector copied from interactionFocus actor position is passed to camera step, then arrival/actor-state gates are evaluated."),
        ("Field::StepTrapCameraFocus",
         (('sp10', 'interaction_camera_target'),),
         "F00140 Vector copied from interactionFocus actor position is passed directly to camera step."),
        ("Field::FinishChestInteraction",
         (('sp10', 'interaction_target_position'),),
         "F00144 X/Z vector coordinates are chest tile center, passed to Ashley pose4 when finishing chest animation."),
        ("Field::TrackInteractionCameraToAshley",
         (('sp18', 'ashley_placement_words'), ('sp10', 'ashley_camera_target'), ('temp_s0', 'camera_tracking_speed')),
         "F00146 short[6] aliases actor-placement query storage; slots2/3/4 provide XYZ. Derived vector offsets Y by90 and tracks camera; return is camera tracking speed unless transition pose blocks."),
        ("Field::OpenChestContents",
         (('temp_v1', 'chest_lock_state'), ('arg1', 'notification_template'), ('temp_v1_2', 'loot_or_storage_index')),
         "F00147 keyOrSigil is read as chest lock state and maps1/2/3 to notification2/1/3. linkedLockIndex below64 selects inline loot; >=64 selects storage index after subtracting64. arg1 is a local, not a formal."),
        ("Field::UpdateChestContentsSelection",
         (('temp_v0', 'action_selection'),),
         "F00148 ActionSelectionState result supplies menuStatus and selectionKind, determining opened flag or switch toggle."),
        ("Field::TileInFrontOfActor",
         (('arg0', 'actor_placement'), ('arg1', 'output_facing_tile'), ('var_v1', 'rounded_facing_or_quadrant')),
         "F00152 Actor placement facing is rounded by0x200, masked, shifted10 into quadrant; output byte order is X/layer/Z and one coordinate is stepped."),
        ("Field::TryTriggerFacingTileEvent",
         (('arg0', 'actor_index'), ('sp10', 'actor_placement'), ('sp20', 'facing_tile')),
         "F00153 Actor id selects placement; output facing tile feeds room event predicate. Comment mentioning arg2 is excluded from lexical inventory."),
        ("Field::FindFacingRoomObject",
         (('arg0', 'actor_index'), ('sp10', 'actor_placement'), ('sp20', 'facing_tile'), ('s0', 'facing_tile_bytes'), ('temp_s1', 'facing_door')),
         "F00154 ActorPlacement and TileInFrontOfActor produce facing tile; byte alias indexes X/Z/layer for door lookup and height acceptance; result pointer may be cleared."),
        ("Field::FindFacingUnlootedChest",
         (('arg0', 'actor_index'), ('sp10', 'actor_placement'), ('sp20', 'facing_tile'), ('s1', 'facing_tile_bytes'), ('temp_s2', 'facing_quadrant'), ('temp_v0', 'facing_chest')),
         "F00155 Facing query returns quadrant and tile; matches chest facing, unlooted state and exact floor+96 height before return."),
        ("Field::CheckActorTrapAndFallTriggers",
         (('arg0', 'actor_index'), ('arg1', 'actor_location'), ('s2', 'location_alias'), ('temp_v0', 'actor_state_flags'), ('temp_a2', 'platform_layer_flags'), ('temp_s3', 'trigger_tile_mode'), ('temp_s4', 'collision_cell'), ('temp_s0', 'battle_actor'), ('temp_v0_2', 'floor_trigger')),
         "F00159 Actor id/location feed tile-state and trap/fall checks. State bit14 is cached trigger tile mode; bits16..18 override collision layer. Pointers distinguish location alias, battle actor, collision tile, and floor trigger."),
        ("Field::RollRiskLockBreak",
         (('var_s0', 'risk_derived_percent'),),
         "F00350 Risk-derived percent=(risk+150)*100/256, capped100, then used with+20 against bounded random100."),
        ("Field::CalculateTrapPanelDamage",
         (('sp10', 'damage_calculation'), ('new_var2', 'trap_attack_value')),
         "F00420 Local damage-calculation record accumulates attack/armor/shield factors; new_var2 caches final attackStat before subtracting defense. Second formal is explicitly unused in trap damage handler."),
        ("Field::CalculateHealingPanelRecovery",
         (('sp10', 'recovery_calculation'), ('new_var2', 'panel_attack_value')),
         "F00422 Healing-panel calculation uses same damage-calculation factors, cached attackStat minus defense and optional random modifier; second formal unused."),
        ("Field::CurrentSavePointId",
         (('sp10', 'ashley_placement'), ('temp_a2', 'platform_layer_flags'), ('temp_v0_2', 'ashley_state_flags'), ('var_a0', 'collision_tile_x'), ('var_a1', 'collision_tile_z'), ('var_a2', 'collision_layer'), ('s0', 'ashley_tile')),
         "F00547 Ashley placement supplies target tile. Actor state &0x70000 selects platform layer and zero coordinates, else tile target coordinates. Flags also guard trigger kind16 save point."),
        ("Field::SetMinimapVisible",
         (('arg0', 'visible'),),
         "F00566 Direct stores and returns minimap visibility flag."),
        ("Field::BuildMinimapRotation",
         (('arg0', 'output_rotation'), ('arg1', 'yaw_angle'), ('temp_s0', 'yaw_sine'), ('temp_v0', 'yaw_cosine'), ('a2', 'negative_yaw_sine')),
         "F00569 Sin/cos of input angle populate output3x3 minimap rotation with fixed tilt constants; a2 is negative sine."),
        ("Field::CollisionCellAt",
         (('arg2', 'collision_layer'),),
         "F00575 Layer >=2 addresses movingPlatforms[layer-2].tileCell; lower layers use the room tile grid."),
        ("Field::SetCollisionCellBlocked",
         (('arg0', 'tile_x'), ('arg1', 'tile_z'), ('arg2', 'collision_layer'), ('arg3', 'blocked_passability'), ('temp_a0', 'collision_cell')),
         "F00576 Arguments index X/Z/layer and control doorPassability, with nonzero blocked input converted to value 5; same tile-cell pointer covers grid/platform paths."),
        ("Field::SetEntranceDoorRecord",
         (('arg0', 'entrance_record'),),
         "F00577 Copies optional twelve-byte entrance record and translates byte8 door id, or clears state for NULL."),
        ("Field::FindEntranceDoorNearTile",
         (('arg0', 'query_tile_x'), ('arg1', 'query_tile_z'), ('arg2', 'query_floor_layer'), ('temp_t4', 'door_count_or_scene_door_id'), ('var_s0', 'entrance_door_cursor'), ('new_var', 'neighbor_tile_radius')),
         "F00578 Checks neighboring 3x3 tile neighborhood on matching layer; layer>=2 matches directly. Cursor selects door. Shared integer begins as doorsLength/12 then is overwritten with SceneDoorId result."),
        ("Field::InteractiveDoorAtTile",
         (('arg0', 'query_tile_x'), ('arg1', 'query_tile_z'), ('arg2', 'query_floor_layer'), ('temp_t2', 'lock_trigger_count'), ('var_t0', 'lock_trigger_cursor')),
         "F00580 Scans lock-trigger records of length20 for tile/layer or directional neighbor for kinds4..7."),
        ("Field::ToggleDoorCollision",
         (('arg0', 'door_trigger'), ('var_a0', 'collision_cell')),
         "F00581 Door trigger locates primary and direction-adjacent collision cells, XORing passability with5."),
        ("Field::BeginDoorToggle",
         (('arg0', 'door_trigger'), ('temp_a0', 'key_or_sigil_id')),
         "F00582 Door record guards isTriggered; unsigned short keyOrSigil is range-tested against0x1CA..0x1D0 then converted to sigil index; opening delay is queued or animation starts."),
        ("Field::StartDoorAnimation",
         (('arg0', 'door_trigger'), ('temp_s0', 'lock_trigger_table')),
         "F00583 Door record drives effect/collision/queue; linkedLockIndex indexes the room's lock-trigger table."),
        ("Field::QueueGimmickAnimation",
         (('arg0', 'queued_job_kind'), ('arg1', 'lock_trigger')),
         "F00584 Job kind and lock-trigger pointer populate first free eight-slot gimmick animation job."),
        ("Field::UpdateGimmickAnimationQueue",
         (('temp_a0', 'queued_trigger'),),
         "F00585 Same trigger pointer covers switch countdown jobs and delayed door-opening jobs, read from queue.trigger."),
        ("Field::FinishDoorAnimation",
         (('arg0', 'door_trigger'),),
         "F00586 Waits for room effect, toggles collision/open/triggered flags including linked door, then reports completion."),
        ("Field::SetMovingPlatformActor",
         (('arg0', 'platform_index'), ('arg1', 'platform_control_state')),
         "F00591 Platform table index selects controlState; nonnegative controller input resets bounce frames when not frozen."),
        ("Field::ClearMovingPlatformActor",
         (('arg0', 'platform_index'),),
         "F00592 Forwards platform table index to SetMovingPlatformActor(index,-1)."),
        ("Field::InitializeMovingPlatforms",
         (('var_s2', 'platform_cursor'), ('temp_s3', 'platform_count'), ('temp_v0', 'geometry_group_words'), ('temp_a0', 'entrance_platform_layer')),
         "F00593 Initializes pause/freeze/bounce; iterates movingPlatformsLength/184 and writes platform positions into found geometry group. Entrance flags>>13 encode platform layer; subtracts2 before reset."),
        ("Field::ResetMovingPlatformWaypoint",
         (('arg0', 'platform_index'), ('arg1', 'waypoint_index'), ('temp_s0', 'platform_record'), ('temp_v0_5', 'geometry_group_words')),
         "F00594 Platform index selects record; waypoint -1 selects terminalWaypoint. Resets movement state and copies waypoint position into platform and corresponding geometry group."),
        ("Field::CollectMovingPlatformPositions",
         (('arg0', 'output_contacts'), ('a0', 'contact_cursor'), ('var_t0', 'platform_cursor'), ('var_t1', 'contact_count'), ('temp_t3', 'platform_count')),
         "F00597 Enumerates nonhidden platform records into caller array, converts fixed-point positions and bounce height, stores platformIndex, and returns written count."),
        ("Field::RoomTriggerAtTile",
         (('arg0', 'query_tile_x'), ('arg1', 'query_tile_z'), ('arg2', 'query_floor_layer'), ('var_t0', 'trigger_cursor'), ('temp_t1', 'trigger_count')),
         "F00602 Scans floorTriggersLength/record size; compares tileX/tileZ/floorLayer; may suppress kinds<16 for special global modes."),
        ("Field::SetMovingPlatformsPaused",
         (('arg0', 'paused'),),
         "F00604 Direct moving-platform pause flag store."),
        ("Field::FreezeMovingPlatforms",
         (('arg0', 'freeze_frames'),),
         "F00605 Input is stored as platformFreezeFrames and its nonzero value enables freeze appearance."),
        ("Field::BeginSwitchToggle",
         (('arg0', 'switch_trigger'),),
         "F00606 Lock-trigger switch markerFade prevents reentry; activation becomes20-frame queued delay or switch toggles immediately."),
        ("Field::ToggleRoomSwitch",
         (('arg0', 'switch_trigger'), ('temp_v0', 'collision_cell')),
         "F00607 Switch value selects SFX, toggles lockState.switchValue and drives collision-cell passability=value*5."),
        ("Field::SceneDoorId",
         (('temp_v1', 'scene_section_word_cursor'),),
         "F00610 Advances int cursor over room records and variable-length minimap sections to door count/table; returns byte1 of selected door record."),
        ("Field::MarkEntranceDoorOnSceneMap",
         (('arg0', 'entered_door_index'), ('temp_a3', 'scene_door_count'), ('temp_v1', 'scene_section_word_cursor')),
         "F00611 Stores entered door id; parses same minimap sections, obtains door count and ORs record byte2 bit0x10 for matching index."),
        ("Field::InterpolatedFloorHeight",
         (('arg0', 'world_x_or_sample_x'), ('arg1', 'world_z_or_sample_z'), ('temp_fp', 'sample_offset_z'), ('temp_s7', 'sample_offset_x'), ('temp_s6', 'height_code_or_packed_height'), ('var_s5', 'base_or_interpolated_height'), ('var_s3', 'neighbor_sample_x_or_height'), ('var_s1', 'neighbor_sample_z_or_height'), ('var_v0', 'neighbor_height_delta'), ('var_s0', 'centered_sample_offset_x'), ('var_v0_3', 'centered_sample_offset_z'), ('temp_s0_2', 'packed_absolute_sample_offsets'), ('temp_s0_3', 'neighbor_z_weight'), ('temp_v0', 'neighbor_x_weight'), ('temp_v0_2', 'center_weight_or_weighted_height')),
         "F00612 World X/Z become 32-unit sample coordinates; low five bits select neighboring sample side. Neighbor coordinates are overwritten with decoded/clamped heights. NormalClip yields barycentric weights; center remainder is then reused for weighted-height sum. Packed result retains sample bit7 as bit15. Names explicitly describe merged lifetimes rather than inventing single roles."),
        ("Field::FloorHeightAt",
         (('arg0', 'world_x'), ('arg1', 'world_z'), ('var_a2', 'height_code_or_packed_floor')),
         "F00613 World X/Z bounds-check at >>7, sample at >>5; one integer holds sentinel, source byte, then packed signed-15-bit height and flag."),
        ("Field::CeilingHeightAt",
         (('arg0', 'world_x'), ('arg1', 'world_z'), ('var_a2', 'height_code_or_packed_ceiling'), ('temp_v1', 'decoded_ceiling_height')),
         "F00614 Samples ceiling plane 1 and decodes heightOrigin/code; positive ceiling uses 0xF810 fallback. Constant 8 is reused as height multiplier and flag left-shift count."),
        ("Field::FloorAndCeilingHeightsAt",
         (('arg0', 'world_x'), ('arg1', 'world_z'), ('sp10', 'packed_floor_and_ceiling'), ('new_var', 'height_halfwords'), ('temp_a3', 'height_code_or_packed_plane'), ('temp_v1', 'ceiling_height_work')),
         "F00615 Initial 0x80008000 holds two missing-height sentinels. short view writes floor half then ceiling half. Shared sample integer holds raw and packed forms of both planes; ceiling scratch is scaled then masked."),
        ("Field::SetRoomTriggerEnabled",
         (('arg0', 'selected_trigger'), ('arg1', 'activation_value'), ('var_a2', 'trigger_cursor'), ('temp_a3', 'trigger_count')),
         "F00616 Writes activation input to one trigger or, for NULL input, all floor-trigger records counted by section length/12."),
        ("Field::SetMovingPlatformVisibility",
         (('arg0', 'platform_index'), ('arg1', 'visibility_value'), ('temp_s0', 'platform_record'), ('temp_v0', 'geometry_group_words')),
         "F00619 Platform index selects record; visibility input minus1 becomes isHidden and corresponding geometry bit0x100 is updated."),
        ("Field::MovingPlatformTargetWaypoint",
         (('arg0', 'platform_index'), ('temp_v1', 'platform_record')),
         "F00620 Platform table index selects record; returns record+8+currentWaypoint*16."),
        ("Field::MovingPlatformDwellTicks",
         (('arg0', 'platform_index'), ('temp_v1', 'platform_record')),
         "F00621 Platform table index selects record; waiting state1 returns motionTicks, otherwise -1."),
        ("Field::RoomInteractiveDoors",
         (('arg0', 'output_door_count'),),
         "F00623 Output pointer receives lockTriggersLength/record size or0; returns table orNULL."),
        ("Field::ActiveRoomTriggers",
         (('arg0', 'output_trigger_count'),),
         "F00624 Output count receives floorTriggersLength/12, or zero when absent/suppressed."),
        ("Field::SetPlatformFreezeAppearance",
         (('arg0', 'freeze_appearance_enabled'), ('var_s0', 'platform_cursor'), ('temp_s2', 'platform_count')),
         "F00629 Requested freeze state selects palette; scans platforms and swaps group R/G where configured, toggles optional environment freeze effect, then flips active tint flag."),
        ("Field::SigilDoorEndpointCoordinate",
         (('arg0', 'door_pair_index'), ('arg1', 'endpoint_component')),
         "F00632 Indexes locked-door endpoint rows; callers use components0/1 for zone/map at first endpoint and2/3 for second."),
        ("Field::MarkSigilDoorUnlocked",
         (('arg0', 'sigil_index'),),
         "F00633 Sigil index selects range in lockedDoorIndexRanges; matches current room zone/map in endpoint pairs then marks matching unlock state."),
        ("Field::IsRoomSigilDoorUnlocked",
         (('arg0', 'zone_id'), ('arg1', 'map_id'), ('arg2', 'sigil_index')),
         "F00634 Zone/map arguments compare both endpoint pairs; sigil argument selects index range then returns unlock bit."),
        ("Field::AdvanceRoomTextureFrames",
         (('arg0', 'texture_frame_effect'),),
         "F00677 Texture-frame record advances frame/delay and writes UV coordinates into polygon UV view at geometryOffset."),
        ("Field::ScrollRoomTextureCoordinates",
         (('arg0', 'texture_scroll_effect'),),
         "F00678 Texture-scroll record advances/wraps current UV window then writes UV members of triangle/quad polygon view."),
        ("Field::AnimateRoomTextureImages",
         (('arg0', 'texture_region_effect'),),
         "F00679 Input texture-region effect updates ticks/frame/scroll, converts source rectangle to texture-page space, performs full/half-resolution MoveImage then restores rectangle origin."),
        ("Field::UpdateRoomRotationEffect",
         (('arg0', 'swing_effect'),),
         "F00680 Swing record owns angle/velocity/acceleration/rest/start/reset state and geometry offset; axis chooses geometry rotation word; endpoint swap resets next swing."),
        ("Field::CreateRoomEffectObjects",
         (('arg0', 'effect_instance'),),
         "F00683 Room effect instance's object count/currentData allocate object records, parse id list/aligned script, capture geometry transforms and disable missing groups."),
        ("Field::StartRoomEffect",
         (('arg0', 'effect_index'),),
         "F00685 Effect index addresses texture effects below64 or script effects above; frees previous allocation before recreating active script objects."),
        ("Field::ExecuteRoomEffectCommands",
         (('arg0', 'effect_object_cursor'), ('arg1', 'object_count'), ('temp_s0', 'command_header')),
         "F00687 Walks object-count entries, reads command header size/opcode, dispatches handlers and advances byte cursor; third formal unused by original/native command loop."),
        ("Field::SetRoomEffectRadialVelocity",
         (('arg0', 'effect_object'), ('arg1', 'effect_command')),
         "F00688 Effect object position and command operands derive signed radial velocities and angular velocity; damping metadata copied from command."),
        ("Field::SetRoomEffectVelocity",
         (('arg0', 'effect_object'), ('arg1', 'effect_command')),
         "F00689 Command halfwords set effect object velocity/angularVelocity and pad damping values."),
        ("Field::SetRoomEffectTransform",
         (('arg0', 'effect_object'), ('arg1', 'effect_command')),
         "F00690 Command words set object position/rotation then synchronize geometry transform."),
        ("Field::WriteRoomEffectTransform",
         (('arg0', 'effect_object'), ('temp_v1', 'geometry_group_words')),
         "F00691 Effect object's geometry sceneEntry gets position/16 in words1..3 and rotation in4..6."),
        ("Field::HideRoomEffectObject",
         (('arg0', 'effect_object'),),
         "F00693 Object sceneEntry word0 is set0x100; command parameter is explicitly unused in opcode-handler ABI."),
        ("Field::ExecuteRoomEffectShowGroup",
         (('arg1', 'effect_command'),),
         "F00694 Command operand0 chooses geometry group to show; object formal is never read."),
        ("Field::AdvanceRoomEffectMotion",
         (('arg0', 'effect_object'), ('arg1', 'effect_command'), ('new_var', 'motion_command'), ('var_a0', 'motion_still_active')),
         "F00695 Command alias selects gravity/damped step and duration/sentinel0x8000. Motion result gates waiting; object position.pad tracks elapsed ticks."),
        ("Field::StepRoomEffectGravity",
         (('arg0', 'effect_object'),),
         "F00696 Effect object integrates linear/angular velocities, probes floor, bounces or completes and applies damping/gravity before geometry sync."),
        ("Field::StepRoomEffectDampedMotion",
         (('arg0', 'effect_object'),),
         "F00697 Effect object integrates motion and proportional damping, then synchronizes geometry."),
        ("Field::ExecuteRoomEffectPlaySound",
         (('arg1', 'effect_command'),),
         "F00698 Command halfwords select sound policy/id; global room bank and effect sound id used, object formal unread."),
        ("Field::ExecuteRoomEffectStopSound",
         (('arg1', 'effect_command'),),
         "F00699 Command operand selects sound to stop in room bank and clears effect sound id; object formal unread."),
        ("Field::ExecuteRoomEffectStartEffect",
         (('arg1', 'effect_command'),),
         "F00700 Command operand0 selects StartRoomEffect; explicitly unused object formal."),
        ("Field::ExecuteRoomEffectHideGroup",
         (('arg1', 'effect_command'),),
         "F00701 Command operand0 selects hidden geometry group; unk0 is explicitly unused object formal, not structure member."),
        ("Field::ExecuteRoomEffectStopScript",
         (('arg1', 'effect_command'),),
         "F00702 Command operand0+64 selects script instance whose allocation is freed/reset and active flag cleared; object formal never read."),
        ("Field::ToggleRoomEffectUpdates",
         (('arg0', 'effect_index'),),
         "F00704 Validated texture/script effect index toggles isUpdateEnabled."),
        ("Field::IsRoomEffectActive",
         (('arg0', 'effect_index'),),
         "F00705 Indexes effect instance and returns isActive."),
        ("Field::FinishRoomDoorRotation",
         (('arg0', 'effect_index'), ('temp_a2', 'swing_effect'), ('a0', 'room_geometry_base'), ('temp_v1', 'geometry_group_words'), ('temp_v0', 'reversed_initial_velocity'), ('temp_a1', 'previous_start_angle')),
         "F00706 Effect index selects active kind6 swing. Geometry base+offset yields group words; restAngle is applied, initial velocity negated and old startAngle retained while endpoints swap."),
        ("Field::FinishRoomTiltRotation",
         (('arg0', 'effect_index'), ('temp_a2', 'swing_effect'), ('temp_a0', 'geometry_group_words'), ('temp_v0', 'reversed_initial_velocity'), ('temp_a1', 'previous_start_angle')),
         "F00707 Effect index supplies swing record; geometry group gets rest angle in axis-specific word, then negated velocity and swapped endpoints."),
        ("Field::DuckRoomEffectSound",
         (('arg0', 'duck_enabled'),),
         "F00708 Nonzero input fades room-effect sound to0x20, zero to0x7F."),
        ("Field::SwapRoomGeometryRedGreen",
         (('arg0', 'geometry_group_id'),),
         "F00709 FindRoomGeometryGroup argument selects group whose triangle/quad corner red and green bytes are swapped."),
        ("Field::FindSharedGimmickModel",
         (('arg0', 'asset_request'),),
         "F00780 Actor asset request carries targetSlot/subKind; searches other live gimmicks with matching objectId and writes shared slot to request.dataAddress."),
        ("Field::SetGimmickPaletteTable",
         (('arg0', 'palette_data'),),
         "F00781 Copies0xE0 palette bytes from input into global gimmick palette colors."),
        ("Field::CreateGimmickInstance",
         (('arg0', 'asset_request'), ('s1', 'created_gimmick'), ('temp_a0', 'supporting_gimmick'), ('temp_s0', 'model_recolor_count'), ('var_v0_2', 'has_shared_model'), ('var_s0', 'base_palette_colors'), ('temp_v1_2', 'template_or_polygon_section')),
         "F00782 Asset request supplies destination/slot/kind/placement. s1 is created instance; temp_a0 is earlier supporting gimmick. Shared model search yields boolean; palette cursor is basePalette; template pointer is later overwritten by model polygon section, so compound name records this distinct lifetime."),
        ("Field::ReleaseGimmickInstance",
         (('arg0', 'gimmick_slot'), ('sp10', 'shared_model_query'), ('a2', 'shared_model_query_alias'), ('temp_s0', 'released_gimmick')),
         "F00783 Slot selects instance; local asset request and its pointer alias encode shared-model search. Model memory is released only when no other matching objectId exists; occupancy and instance slot cleared."),
        ("Field::RefreshAshleyInteractionTile",
         (('arg0', 'actor_record'), ('temp_v1_2', 'facing_quadrant'), ('var_a0', 'signed_interaction_reach'), ('var_a2', 'tile_offset_x'), ('var_a3', 'tile_offset_z')),
         "F00828 Only Ashley interaction animations are accepted. Animation range selects1/2-tile reach, then rounded facing quadrant signs and routes offset into X or Z for ReleaseAiDestinationTile."),
        ("Field::DropUnsupportedGimmickStack",
         (('arg0', 'removed_gimmick_slot'), ('temp_s0', 'removed_or_scanned_gimmick'), ('temp_s1', 'falling_base_gimmick'), ('temp_s4', 'top_gimmick_slot'), ('temp_v0', 'falling_base_slot'), ('temp_v0_2', 'supported_actor')),
         "F00836 Input selects removed gimmick. temp_s0 is reused for removed record then stack scan candidates; FindGimmickBelow yields falling base index/record; FindTopGimmickAtTile gives top slot. Actor iterator above that stack receives falling movement state."),
        ("Field::FindGimmickBelow",
         (('arg0', 'reference_gimmick'), ('t1', 'reference_packed_tile'), ('temp_v1', 'candidate_gimmick'), ('temp_v1_2', 'candidate_height'), ('var_a3', 'best_height'), ('var_t0', 'best_gimmick_slot')),
         "F00837 Packed four-byte tile identity filters eligible gimmicks; picks largest positionY <= input Y, starting bestY=-30000/index=-1."),
        ("Field::GimmickSubtype",
         (('arg0', 'gimmick_slot'), ('temp_a0', 'gimmick_record')),
         "F00838 Slot selects gimmick; returns subtype only for objectId1, else -1."),
        ("Field::GimmickPlacement",
         (('arg0', 'gimmick_slot'), ('arg1', 'output_snapshot'), ('temp_s0', 'gimmick_record'), ('var_v0_2', 'support_height_difference')),
         "F00853 Slot selects gimmick, destination receives placement/orientation and position. Height difference is adjusted by127 for negative values then shifted7. Preexisting uninitialized pointer check remains untouched."),
        ("Field::SetActorCollisionBypass",
         (('arg0', 'actor_index'), ('arg1', 'collision_bypass_flags'), ('temp_a0', 'actor_record')),
         "F00866 Actor array index selects record; input low bit and >>1 supply bypassActorCollision and ignoredByCollision."),
        ("Field::ActorCollisionBypass",
         (('arg0', 'actor_index'), ('temp_a0', 'actor_record'), ('var_v1', 'collision_bypass_flags')),
         "F00867 Reads selected actor's bypass bit then ORs 2 when ignoredByCollision is set."),
        ("Field::ActorSupportHeight",
         (('arg0', 'world_x'), ('arg1', 'world_z')),
         "F00904 World X/Z passed to InterpolatedFloorHeight then compared with each gimmick's positionX/Z for top support at positionY-128."),
        ("Field::RoomCeilingHeight",
         (('arg0', 'world_x'), ('arg1', 'world_z'), ('temp_v0', 'packed_ceiling_height')),
         "F00909 CeilingHeightAt produces a flagged 16-bit value; nonzero result is sign-extended from its low 15 bits."),
        ("Field::FindActorAboveGimmick",
         (('arg0', 'support_gimmick'), ('arg1', 'occupancy_mode_or_first_actor')),
         "F00910 Original func_800A3C00 in3A1A0.c:661 forwards gimmick tileX/tileZ/positionY and occupancy mode/first-actor argument to func_800A3C34. The stack-drop consumer advances that argument through actor slots; preserve mode0/1 semantics of the callee."),
        ("Field::FindActorOccupyingTile",
         (('arg0', 'query_tile_x'), ('arg1', 'query_tile_z'), ('arg2', 'minimum_actor_height'), ('arg3', 'occupancy_mode_or_first_actor'), ('temp_v1', 'support_gimmick_record')),
         "F00911 X/Z compare to current/route tiles. Input Y is reduced by 192 to a lower bound. Mode 0/1 starts at actor0, mode>=2 also selects first actor; mode1 checks pending route tile. Ashley support index selects a gimmick tile."),
        ("Field::AreGimmicksMoving",
         (('temp_v1', 'gimmick_record'),),
         "F00917 Iterates live gimmicks, checking isSkipped/motionState/mode before reporting activity."),
        ("Field::FindMovingPlatformById",
         (('arg0', 'platform_layer_or_index'), ('sp10', 'platform_contacts'), ('temp_v0', 'platform_contact_count')),
         "F00920 Subtracts layer bias 2, collects four platform contacts, compares each platformIndex and returns its address; existing returned-stack-address defect is not changed by naming."),
        ("Field::IsPointOnMovingPlatform",
         (('arg0', 'platform_layer_or_index'), ('arg1', 'world_position'), ('sp10', 'platform_contacts')),
         "F00924 Converts layer to platform index by subtracting 2; queries platform contacts and compares input world X/Z against selected platform +/-64 extents."),
        ("Field::HasActorLedgeSideSupport",
         (('arg0', 'actor_record'),),
         "F00929 Actor collisionRadius determines lateral ledge-support probe offset; all argument uses select this actor radius."),
        ("Field::SampleActorSupportHeight",
         (('arg0', 'origin_position'), ('arg1', 'offset_x'), ('arg2', 'offset_z'), ('arg3', 'support_probe_mode')),
         "F00931 Adds X/Z offsets to input position. Mode selects discrete/interpolated floor, below-height gimmicks, moving-platform inclusion and unsupported fallback behavior; modes 0..4 are not a boolean."),
        ("Field::ClassifyActorTerrainProbe",
         (('arg0', 'probe_position'), ('arg1', 'sample_sector')),
         "F00935 Mutable probe vector supplies world X/Z, receives support height and packed platform/gimmick ids. Input sample index &7 selects scratch sampleHeights baseline."),
        ("Field::FindCollisionCellAtPosition",
         (('arg0', 'world_position'), ('arg1', 'collision_layer'), ('temp_s1', 'tile_x_or_local_x'), ('temp_s0', 'tile_z_or_local_z'), ('temp_v0', 'collision_cell')),
         "F00939 World position and collision layer select cell. X/Z locals begin as tile quotients then are overwritten with within-tile remainders for triangular passability tests."),
        ("Field::FindTopGimmickAtTile",
         (('arg0', 'query_tile_x'), ('arg1', 'query_tile_z'), ('arg2', 'require_idle_mode'), ('temp_v1', 'candidate_gimmick'), ('var_t2', 'minimum_height'), ('var_t3', 'top_gimmick_slot')),
         "F00942 Tile coordinates filter eligible unattached/un-carried objects; optional mode restricts mode==0. Tracks smallest positionY and its slot."),
        ("Field::GimmickSupportHeight",
         (('arg0', 'query_tile_x'), ('arg1', 'query_tile_z'), ('temp_v1', 'gimmick_record'), ('var_t0', 'minimum_gimmick_y_or_support_height')),
         "F00943 Searches matching X/Z tile for minimum gimmick positionY; accumulator starts at 3000, then subtracts 128 and clamps positive result to zero."),
        ("Field::MovingPlatformContactHeight",
         (('arg0', 'world_x'), ('arg1', 'world_y'), ('arg2', 'world_z'), ('arg3', 'contact_search_mode'), ('sp10', 'platform_contacts')),
         "F00944 Arguments are world X/Y/Z and mode 0..3 controlling vertical band and return offset; contact array generated by CollectMovingPlatformPositions and publishes platform layer+2."),
        ("Field::GimmickSupportHeightBelow",
         (('arg0', 'query_tile_x'), ('arg1', 'query_tile_z'), ('arg2', 'reference_height'), ('temp_v1', 'gimmick_record'), ('temp_v1_2', 'gimmick_height'), ('var_a3', 'minimum_gimmick_y_or_support_height')),
         "F01015 X/Z tile match; accepts gimmick Y strictly greater than reference height and below current minimum. Subtracts 128 then clamps at zero."),
        ("Field::MeasureTileStack",
         (('arg0', 'query_tile_x'), ('arg1', 'query_tile_z'), ('arg2', 'output_stack_height'), ('var_t2', 'stack_height'), ('var_t0', 'gimmick_table'), ('temp_v1', 'gimmick_record')),
         "F02468 Filters stationary visible gimmicks at tile; interaction2 sets return flag, other matches decrement output stack height by128. Table alias and per-record pointer are separate."),
        ("Field::IsGimmickWithinHeightBand",
         (('arg0', 'query_tile_x'), ('arg1', 'query_tile_z'), ('arg2', 'minimum_height'), ('new_var', 'maximum_height'), ('var_t0', 'gimmick_table'), ('temp_v1', 'gimmick_record')),
         "F02469 Checks visible stationary gimmicks at tile with Y in inclusive [inputHeight,inputHeight+128]."),
    )
    for old, new in bindings
]


# Individually reviewed menu bindings; structural fixups run first.
REGEX_FIXES += [
    (rf"^{name}$", rf"\b{old}\b", new, evidence)
    for name, bindings, evidence in (
        ("Menu::SelectTextBoxChoice",
         (('v0', 'previous_choice_index'),),
         "F01330 src\\BATTLE\\BATTLE.PRG\\5BF94.c:1559-1650 (vs_battle_selectTextBox). Named roles: previous choice index."),
        ("Menu::Framework::LoadMenuOverlay",
         (('arg0', 'packed_menu_state'),),
         "F01342 src\\BATTLE\\BATTLE.PRG\\5BF94.c:2576-2651 (vs_battle_loadMenuPrg). Named roles: packed menu state."),
        ("Menu::Framework::SetMenuItem",
         (('var_a0', 'text_destination_row'),),
         "F01345 src\\BATTLE\\BATTLE.PRG\\5BF94.c:2660-2696 (vs_battle_setMenuItem). Named roles: text destination row."),
        ("Menu::Framework::DrawMenuItem",
         (('temp_v1', 'row_left_x'),),
         "F01348 src\\BATTLE\\BATTLE.PRG\\5BF94.c:2737-2827 (vs_battle_renderMenuItem). Named roles: row left x. Retained: var_s2: Merged glyph pen-X accumulator and later packed gradient-stop color written to primitive words 4/8."),
        ("Menu::Framework::UpdateMenuOverlay",
         (('temp_s1', 'selected_menu_id'), ('var_s0', 'packed_menu_state'), ('var_s2', 'menu_row_cursor')),
         "F01350 src\\BATTLE\\BATTLE.PRG\\5BF94.c:2966-3019 (func_800C97BC). Named roles: selected menu id, packed menu state, menu row cursor."),
        ("Menu::Framework::InitGameInterface",
         (('arg0', 'interface_texture_data'),),
         "F01364 src\\BATTLE\\BATTLE.PRG\\5BF94.c:3484-3516 (func_800CA9C0). Named roles: interface texture data."),
        ("Menu::Framework::ActionRestrictionFlags",
         (('arg0', 'menu_invocation_mode'), ('temp_v0', 'ashley_state_flags'), ('var_a0', 'actor_restriction_flags')),
         "F01366 src\\BATTLE\\BATTLE.PRG\\5BF94.c:3535-3573 (func_800CABE0). Named roles: menu invocation mode, ashley state flags, actor restriction flags."),
        ("Menu::Framework::OpenGameMenu",
         (('arg1', 'menu_invocation_mode'), ('var_a0', 'ability_action_id'), ('var_a1', 'enabled_menu_flags')),
         "F01367 src\\BATTLE\\BATTLE.PRG\\5BF94.c:3575-3629 (func_800CACD0). Named roles: menu invocation mode, ability action id, enabled menu flags."),
        ("Menu::Framework::OpenLootListMenu",
         (('arg0', 'loot_list_head'),),
         "F01373 src\\BATTLE\\BATTLE.PRG\\5BF94.c:3723-3728 (func_800CB158). Named roles: loot list head."),
        ("Menu::Framework::OpenLootEntryMenu",
         (('arg0', 'loot_entry'),),
         "F01374 src\\BATTLE\\BATTLE.PRG\\5BF94.c:3730-3735 (func_800CB18C). Named roles: loot entry."),
        ("Menu::Framework::OpenChestStorageMenu",
         (('arg0', 'chest_location_id'),),
         "F01375 src\\BATTLE\\BATTLE.PRG\\5BF94.c:3737-3742 (func_800CB1C0). Named roles: chest location id."),
        ("Menu::DismissAllTextBoxes",
         (('var_s1', 'all_text_boxes_dismissed'),),
         "F01378 src\\BATTLE\\BATTLE.PRG\\5BF94.c:3757-3769 (_dismissAllTextBoxes). Named roles: all text boxes dismissed."),
        ("Menu::ShowSceneNotification",
         (('arg0', 'notification_kind'), ('arg1', 'notification_subject_id'), ('arg2', 'timed_notification')),
         "F01379 src\\BATTLE\\BATTLE.PRG\\5BF94.c:3771-3800 (vs_battle_displaySceneMessage). Named roles: notification kind, notification subject id, timed notification."),
        ("Menu::TextBoxAnchor",
         (('arg0', 'anchor_preset_index'), ('arg1', 'orientation_flags'), ('arg2', 'text_column_count'), ('arg3', 'text_line_count'), ('unk24', 'anchor_alignment')),
         "F01410 src\\BATTLE\\BATTLE.PRG\\5BF94.c:4546-4565 (func_800CCE10). Named roles: anchor preset index, orientation flags, text column count, text line count, anchor alignment."),
        ("Menu::DrawInformationBoxHeader",
         (('arg0', 'header_y'), ('arg1', 'header_kind')),
         "F01413 src\\BATTLE\\BATTLE.PRG\\5BF94.c:4633-4639 (func_800CD0FC). Named roles: header y, header kind."),
        ("Menu::FlipTextBoxShapePoint",
         (('arg0', 'packed_shape_point'), ('arg1', 'flip_axes'), ('var_v1', 'flipped_shape_point')),
         "F01415 src\\BATTLE\\BATTLE.PRG\\5BF94.c:4697-4710 (func_800CD3A0). Named roles: packed shape point, flip axes, flipped shape point."),
        ("Menu::Framework::LoadActorPreview",
         (('arg0', 'source_actor_slot'),),
         "F02463 src\\MENU\\MAINMENU.PRG\\224.c:10-18 (func_800F9A24). Named roles: source actor slot."),
        ("Menu::Framework::CloneActorPreview",
         (('arg0', 'source_actor_slot'), ('sp10', 'asset_request'), ('temp_s2', 'source_actor'), ('temp_s3', 'preview_actor'), ('temp_v0', 'cloned_asset_allocation')),
         "F02464 src\\MENU\\MAINMENU.PRG\\224.c:20-91 (func_800F9A78). Named roles: source actor slot, asset request, source actor, preview actor, cloned asset allocation. Retained: temp_s0: Two distinct local bindings: outer Actors_ActorRecord pointer for companion and inner Actors_WeaponModel pointer for equipped weapon."),
        ("Menu::Framework::ResetActorPreviewPose",
         (('sp10', 'preview_color'), ('sp18', 'companion_state_bytes'), ('temp_s0', 'preview_actor')),
         "F02465 src\\MENU\\MAINMENU.PRG\\224.c:100-147 (func_800F9CB0). Named roles: preview color, companion state bytes, preview actor. Retained: new_var: Original func_800F9CB0 declares this int but never reads or writes it; retain the unused decompiler declaration, not a fictional pose field."),
        ("Menu::Framework::ReleaseActorPreview",
         (('sp10', 'preview_weapon_allocations'), ('temp_s1', 'preview_actor_allocation')),
         "F02466 src\\MENU\\MAINMENU.PRG\\224.c:149-175 (func_800F9E0C). Named roles: preview weapon allocations, preview actor allocation."),
        ("Menu::Framework::DrawActorPreview",
         (('arg0', 'preview_camera_matrix'), ('new_var2', 'companion_ordering_table_advance_bytes'), ('new_var3', 'companion_ordering_table_restore_bytes'), ('temp_a0', 'preview_weapon_model'), ('temp_s1', 'preview_actor')),
         "F02467 src\\MENU\\MAINMENU.PRG\\224.c:177-259 (func_800F9EB8). Named roles: preview camera matrix, companion ordering table advance bytes, companion ordering table restore bytes, preview weapon model, preview actor."),
        ("Menu::Framework::ResetAshleyEquipmentPose",
         (('sp10', 'animation_length_address'), ('temp_s1', 'ashley_actor'), ('var_a0', 'equipped_weapon_model')),
         "F02470 src\\MENU\\MAINMENU.PRG\\224.c:306-350 (func_800FA2CC). Named roles: animation length address, ashley actor, equipped weapon model."),
        ("Menu::Framework::QueueAshleyWeaponAnimations",
         (('arg0', 'sequence_model_id'), ('sp10', 'sequence_request')),
         "F02471 src\\MENU\\MAINMENU.PRG\\224.c:352-364 (func_800FA3FC). Named roles: sequence model id, sequence request."),
        ("Menu::Framework::SetGemStats",
         (('temp_a2', 'selected_gem'),),
         "F02490 src\\MENU\\MAINMENU.PRG\\2D10.c:531-548 (vs_mainMenu_setStatsFromGem). Named roles: selected gem."),
        ("Menu::Framework::SelectShieldDetailRow",
         (('arg0', 'selected_detail_row'), ('temp_s2', 'saved_status_view_item')),
         "F02494 src\\MENU\\MAINMENU.PRG\\2D10.c:656-695 (_shieldStatusView). Named roles: selected detail row, saved status view item."),
        ("Menu::Framework::IsItemDetailElementReachable",
         (('var_a0', 'element_reachability'),),
         "F02498 src\\MENU\\MAINMENU.PRG\\2D10.c:750-821 (_isUiElementReachable). Named roles: element reachability."),
        ("Menu::Framework::DrawButtonPromptBackground",
         (('temp_v1', 'background_ordering_table'), ('var_t2', 'background_primitive_cursor')),
         "F02536 src\\MENU\\MAINMENU.PRG\\58EC.c:718-737 (vs_mainmenu_renderButtonBackground). Named roles: background ordering table, background primitive cursor."),
        ("Menu::Framework::LoadBackgroundTexture",
         (('temp_a0', 'packed_run_lengths'), ('temp_t1', 'encoded_texture_words'), ('var_a2', 'decoded_word_index'), ('var_v1', 'run_words_remaining')),
         "F02538 src\\MENU\\MAINMENU.PRG\\58EC.c:751-787 (vs_mainMenu_unpackMenubg). Named roles: packed run lengths, encoded texture words, decoded word index, run words remaining."),
        ("Menu::Framework::CreateMenuRows",
         (('temp_v1_2', 'maximum_page_offset'), ('var_v0', 'has_following_page')),
         "F02539 src\\MENU\\MAINMENU.PRG\\58EC.c:798-890 (vs_mainmenu_setMenuRows). Named roles: maximum page offset, has following page. Retained: var_a0: Only assigned rowCount<<7 and never read."),
        ("Menu::Framework::CreateListRow",
         (('temp_v1', 'invert_gradient_flag'),),
         "F02543 src\\MENU\\MAINMENU.PRG\\58EC.c:910-945 (_initItemRow). Named roles: invert gradient flag."),
        ("Menu::Framework::UpdateRowSelection",
         (('temp_v1', 'visible_row_capacity'), ('var_s1', 'visible_row_count')),
         "F02544 src\\MENU\\MAINMENU.PRG\\58EC.c:947-1164 (vs_mainMenu_renderItemsList). Named roles: visible row capacity, visible row count."),
        ("Menu::Framework::DrawBackground",
         (('new_var', 'background_texture_page_flags'),),
         "F02545 src\\MENU\\MAINMENU.PRG\\58EC.c:1172-1212 (_renderBackground). Named roles: background texture page flags."),
        ("Menu::Framework::DrawFeatheredRowCharacter",
         (('new_var', 'non_printing_character_threshold'), ('s7', 'scroll_direction_sign')),
         "F02546 src\\MENU\\MAINMENU.PRG\\58EC.c:1215-1254 (vs_mainMenu_renderRowLabel). Named roles: non printing character threshold, scroll direction sign."),
        ("Menu::Framework::DrawScrollableRow",
         (('sp14', 'texture_bank_x'), ('temp_a0', 'texture_v'), ('temp_v1_2', 'texture_u')),
         "F02548 src\\MENU\\MAINMENU.PRG\\58EC.c:1309-1452 (_renderScrollableRow). Named roles: texture bank x, texture v, texture u. Retained: var_s3: Merged arrow Y animation offset, decoded glyph, gradient intensity and final texture-page X origin."),
        ("Menu::Framework::UpdateAndDrawRows",
         (('var_v1', 'row_marker_x'),),
         "F02549 src\\MENU\\MAINMENU.PRG\\58EC.c:1454-1707 (_renderMenuItems). Named roles: row marker x. Retained: temp_a1: Declaration-only int in UpdateAndDrawRows with no producer/use; retain it as unused decompiler scaffolding, not an invented animation value."),
        ("Menu::Framework::OffsetStatBarCoordinate",
         (('a0', 'panel_x'), ('a1', 'scanline_offset'), ('a2', 'edge_base_x')),
         "F02554 src\\MENU\\MAINMENU.PRG\\C48.c:668-668 (_subtract). Named roles: panel x, scanline offset, edge base x."),
        ("Menu::Framework::DrawEquipmentStatPage",
         (('a0', 'equipment_subtype'), ('sp14', 'stat_bar_origin_x'), ('sp18', 'stat_rows_height'), ('sp1C', 'highest_affinity_value'), ('sp20', 'stat_color_thresholds'), ('sp24', 'comparison_stat_values'), ('sp28', 'stat_number_x'), ('temp_a0', 'background_scanline_offset'), ('temp_a0_2', 'packed_background_scanline_y'), ('temp_v0_2', 'packed_positive_bar_y'), ('temp_v0_3', 'packed_negative_bar_y'), ('temp_v1_3', 'background_right_inset'), ('var_s6', 'stat_row_y_offset'), ('var_s7', 'displayed_stat_values')),
         "F02555 src\\MENU\\MAINMENU.PRG\\C48.c:673-897 (_renderStatPage). Named roles: equipment subtype; statistic-bank and color-threshold pointers; affinity maximum; stat-row height/Y, number/bar X, and packed background/bar coordinates."),
        ("Menu::Framework::UpdateEquipmentStatPages",
         (('arg0', 'stat_page_panel_command'), ('var_s1', 'panel_x_offset')),
         "F02558 src\\MENU\\MAINMENU.PRG\\C48.c:920-971 (vs_mainMenu_drawClassAffinityType). Named roles: stat page panel command, panel x offset."),
        ("Menu::Framework::SetRangeAndRisk",
         (('arg3', 'show_range_risk'),),
         "F02559 src\\MENU\\MAINMENU.PRG\\C48.c:973-979 (vs_mainMenu_setRangeRisk). Named roles: show range risk. Retained: arg2: Stores only rangeRiskData[2]; every reviewed caller supplies zero, while UpdatePrimaryStatPanel consumes range/risk [0..1] and visibility [3]."),
        ("Menu::Framework::SetActorComparisonStats",
         (('temp_t0', 'actor_stats'),),
         "F02561 src\\MENU\\MAINMENU.PRG\\C48.c:992-1018 (vs_mainMenu_populateBaseStats). Named roles: actor stats."),
        ("Menu::Framework::UpdatePrimaryStatPanel",
         (('new_var2', 'compared_stat_total'), ('temp_s4', 'panel_ordering_table')),
         "F02562 src\\MENU\\MAINMENU.PRG\\C48.c:1025-1118 (vs_mainMenu_renderEquipStats). Named roles: compared stat total, panel ordering table. Retained: var_a3: Merged values-known low-bit predicate and later accessory stat contribution."),
        ("Menu::Framework::UpdateDpPpBars",
         (('temp_s1', 'packed_bar_panel_y'),),
         "F02564 src\\MENU\\MAINMENU.PRG\\C48.c:1134-1215 (vs_mainMenu_renderDpPpBars). Named roles: packed bar panel y."),
        ("Menu::Inventory::UpdateItemDetails",
         (('new_var', 'has_initial_page'),),
         "F02581 src\\MENU\\items1.h:640-688 (_itemStatusDispatcher). Named roles: has initial page."),
        ("Menu::Inventory::UpdateItemList",
         (('var_s2', 'base_item_category'),),
         "F02597 src\\MENU\\items2.h:201-965 (_itemNavigation). Named roles: base item category. Retained: sp310: Two distinct block bindings: heap-backed item-row text pointer and action-menu local text array[12]; var_a1_2: Merged packed row-creation options, category-navigation button masks, swapped inventory ID and parent equipment ID."),
        ("Menu::Magic::UpdateTeleportMenu",
         (('a0', 'packed_destination_tile_z'), ('a2', 'selected_save_point_index'), ('v0', 'packed_location_and_tile_x')),
         "F02598 src\\MENU\\MENU0.PRG\\15AC.c:22-143 (_teleportMenu). Named roles: packed destination tile z, selected save point index, packed location and tile x. Retained: v1: Merged packed location field (bits0..8), tile-X field (bits9..13), and MP-cost field (bits19..31)."),
        ("Menu::Magic::UpdateWarlockMenu",
         (('temp_s6', 'spell_level_ordering_table'), ('var_s7_2', 'level_locked')),
         "F02604 src\\MENU\\MENU0.PRG\\84.c:164-498 (_warlockMagicMenu). Named roles: spell level ordering table, level locked."),
        ("Menu::BattleAbilities::UpdateBreakArtUnlock",
         (('new_var', 'learned_arts_by_weapon_category'),),
         "F02614 src\\MENU\\MENU2.PRG\\143C.c:27-71 (_breakArtUnlocked). Named roles: learned arts by weapon category."),
        ("Menu::BattleAbilities::UpdateDefenseAbilityMenu",
         (('arg0', 'reset_menu'),),
         "F02623 src\\MENU\\MENU2.PRG\\64.c:553-605 (_defenseAbilityMenu). Named roles: reset menu."),
        ("Menu::Inventory::UpdateEquipmentChoices",
         (('temp_s4', 'selected_absolute_row'), ('temp_s6', 'previous_first_row')),
         "F02628 src\\MENU\\MENU3.PRG\\16C.c:111-309 (_processEquipSubMenu). Named roles: selected absolute row, previous first row. Retained: temp_s0: Merged packed menu-row flags and temporary visible-row loop bound capped at six."),
        ("Menu::Inventory::Update",
         (('temp_a1', 'item_use_return_menu'), ('temp_a1_2', 'equipment_return_menu')),
         "F02636 src\\MENU\\MENU3.PRG\\16C.c:1236-1548 (vs_menu3_exec). Named roles: item use return menu, equipment return menu."),
        ("Menu::Inventory::ReloadEquipmentModels",
         (('temp_s1', 'preview_actor'),),
         "F02637 src\\MENU\\MENU3.PRG\\68C4.c:12-85 (_loadObjectData). Named roles: preview actor."),
        ("Menu::Inventory::QueueEquipmentModel",
         (('var_a3', 'equipment_texture_slot'),),
         "F02639 src\\MENU\\MENU3.PRG\\68C4.c:100-128 (_populateDataSlot). Named roles: equipment texture slot."),
        ("Menu::Status::ArmorDescription",
         (('arg0', 'armor_loadout'),),
         "F02652 src\\MENU\\MENU4.PRG\\120.c:368-376 (_drawArmorInfoRow). Named roles: armor loadout."),
        ("Menu::Status::AccessoryDescription",
         (('arg0', 'accessory_loadout'),),
         "F02653 src\\MENU\\MENU4.PRG\\120.c:383-393 (_drawAccessoryInfoRow). Named roles: accessory loadout."),
        ("Menu::Status::IsDetailElementSelectable",
         (('v0', 'maximum_component_selection'), ('v1', 'component_selection_index')),
         "F02654 src\\MENU\\MENU4.PRG\\120.c:401-454 (_equipmentDetailElementSelectable). Named roles: maximum component selection, component selection index."),
        ("Menu::Status::FindNextInspectableActor",
         (('temp_a0', 'candidate_actor_state_flags'),),
         "F02657 src\\MENU\\MENU4.PRG\\120.c:548-565 (_getNextValidActor). Named roles: candidate actor state flags."),
        ("Menu::Status::DrawLimbConditionIndicator",
         (('temp_t1', 'limb_indicator_ordering_table'),),
         "F02662 src\\MENU\\MENU4.PRG\\120.c:847-865 (_drawStatusIndicator). Named roles: limb indicator ordering table."),
        ("Menu::Status::DrawLimbLeaderLine",
         (('temp_t0', 'leader_line_ordering_table'), ('temp_t2', 'limb_row_y'), ('temp_v0', 'packed_leader_elbow')),
         "F02663 src\\MENU\\MENU4.PRG\\120.c:870-907 (_drawLimbLeaderLine). Named roles: leader line ordering table, limb row y, packed leader elbow. Retained: arg2: Explicitly unused third parameter in the original limb leader-line renderer."),
        ("Menu::Status::NavigateStatusEffects",
         (('arg1', 'selection_active'),),
         "F02665 src\\MENU\\MENU4.PRG\\120.c:961-1109 (_navigateStatusModifiers). Named roles: selection active."),
        ("Menu::Status::UpdateLimbPanel",
         (('sp20', 'highlighted_limb_index'), ('sp24', 'highlight_dimming_shift'), ('temp_s6', 'panel_dimming_shift')),
         "F02668 src\\MENU\\MENU4.PRG\\120.c:1146-1294 (_renderLimbUi). Named roles: highlighted limb index, highlight dimming shift, panel dimming shift."),
        ("Menu::Status::UpdateEquipmentDetails",
         (('a1', 'previous_equipment_row'), ('sp18', 'equipment_row_text'), ('sp48', 'equipment_row_flags'), ('var_a0_5', 'dp_pp_panel_command'), ('var_s0_2', 'equipment_row_advance'), ('var_s2', 'selected_equipment_row'), ('var_s3', 'detail_help_text')),
         "F02675 src\\MENU\\MENU4.PRG\\120.c:1482-1868 (_equipmentDetailScreen). Named roles: previous equipment row, equipment row text, equipment row flags, dp pp panel command, equipment row advance, selected equipment row, detail help text."),
        ("Menu::Status::UpdateEquipmentList",
         (('new_var', 'weapon_row_style_flags'), ('sp3E0', 'accessory_inventory_view'), ('temp_s6', 'selected_actor_stats')),
         "F02676 src\\MENU\\MENU4.PRG\\120.c:1875-2076 (_equipmentScreen). Named roles: weapon row style flags, accessory inventory view, selected actor stats. Retained: temp_s1_2: Merged packed shield-row style 0xF200 and saved cursor-memory preference; temp_s5: Merged unanalyzed-equipment disabled bit, selected limb index and actor-switch transition flags."),
        ("Menu::Status::DrawModelSlideFade",
         (('temp_t1', 'model_fade_ordering_table'),),
         "F02677 src\\MENU\\MENU4.PRG\\120.c:2081-2116 (_fadeScreenLeft). Named roles: model fade ordering table."),
        ("Menu::Status::DrawBackground",
         (('temp_a0', 'ordering_table_base'), ('var_t9', 'background_ordering_table_entry')),
         "F02678 src\\MENU\\MENU4.PRG\\120.c:2121-2185 (_drawMenuBackground). Named roles: ordering table base, background ordering table entry."),
        ("Menu::Status::DrawFrame",
         (('temp_lo_5', 'camera_orbit_z_offset'),),
         "F02679 src\\MENU\\MENU4.PRG\\120.c:2190-2223 (_drawScreen). Named roles: camera orbit z offset."),
        ("Menu::Status::Update",
         (('temp_s0_2', 'selected_element_index'), ('var_s5', 'actor_details_visible'), ('var_s6', 'limb_status_available')),
         "F02682 src\\MENU\\MENU4.PRG\\120.c:2270-2856 (vs_menu4_exec). Named roles: selected element index, actor details visible, limb status available. Retained: new_var: Two separate char declarations in distinct limb-navigation branches, both decrement bias one."),
        ("Menu::Map::ApplyCamera",
         (('sp10', 'negative_camera_position'), ('sp18', 'view_rotation')),
         "F02690 src\\MENU\\MENU5.PRG\\4D8.c:948-971 (_setCamera). Named roles: negative camera position, view rotation. Retained: sp38: MATRIX initially contains pitch rotation, then its translation words receive the transformed negative camera position for SetTransMatrix."),
        ("Menu::Framework::BuildYawMatrix",
         (('arg0', 'camera_matrix'), ('arg1', 'angle'), ('temp_s0', 'sine'), ('temp_v0', 'cosine')),
         "F02697 src\\MENU\\MENU5.PRG\\4D8.c:1756-1770 (_buildYawMatrix). Named roles: camera matrix, angle, sine, cosine."),
        ("Menu::Framework::BuildPitchMatrix",
         (('arg0', 'pitch_matrix'), ('arg1', 'angle'), ('temp_s0', 'sine'), ('temp_v0', 'cosine')),
         "F02698 src\\MENU\\MENU5.PRG\\4D8.c:1772-1786 (_buildPitchMatrix). Named roles: pitch matrix, angle, sine, cosine."),
        ("Menu::Map::DrawPalingDistortion",
         (('new_var', 'legacy_render_scratch'),),
         "F02702 src\\MENU\\MENU5.PRG\\4D8.c:1880-1963 (_applyPalingScreenEffect). Named roles: legacy render scratch."),
        ("Menu::Map::DrawControls",
         (('s1', 'legacy_button_render_scratch'),),
         "F02704 src\\MENU\\MENU5.PRG\\4D8.c:1991-2151 (_drawUIControls). Named roles: legacy button render scratch. Retained: s0: Six distinct block-local aliases of original address 0x1F800000, each reading ordering-table slot 1 for a control state."),
        ("Menu::Map::DrawDoorStateMarker",
         (('new_var', 'legacy_render_scratch'), ('s6', 'zero_based_marker_id')),
         "F02706 src\\MENU\\MENU5.PRG\\4D8.c:2180-2222 (func_80107630). Named roles: legacy render scratch, zero based marker id."),
        ("Menu::Map::DrawCompass",
         (('new_var2', 'legacy_primitive_scratch'), ('new_var3', 'legacy_ordering_table_scratch'), ('new_var4', 'compass_texture_right_u')),
         "F02709 src\\MENU\\MENU5.PRG\\4D8.c:2272-2375 (_renderCompass). Named roles: legacy primitive scratch, legacy ordering table scratch, compass texture right u."),
        ("Menu::Map::ComputeCompassCorners",
         (('arg0', 'compass_center_x'), ('arg1', 'compass_center_y'), ('arg2', 'compass_half_width'), ('arg3', 'compass_half_height'), ('arg4', 'compass_angle'), ('temp_s1', 'angle_sine'), ('temp_v0', 'angle_cosine')),
         "F02710 src\\MENU\\MENU5.PRG\\4D8.c:2377-2398 (_computeRotatedRect). Named roles: compass center x, compass center y, compass half width, compass half height, compass angle, angle sine, angle cosine."),
        ("Menu::Map::ShouldDrawDoorStateMarker",
         (('arg0', 'door_state_id'), ('arg1', 'zone_id'), ('arg2', 'map_id'), ('temp_a2', 'sigil_door_kind')),
         "F02714 src\\MENU\\MENU5.PRG\\4D8.c:2471-2479 (func_8010839C). Named roles: door state id, zone id, map id, sigil door kind."),
        ("Menu::Files::DrawHpMpSummary",
         (('new_var4', 'value_right_edge_offset'),),
         "F02743 src\\MENU\\MENU7.PRG\\260.c:1270-1344 (_drawHPMP). Named roles: value right edge offset."),
        ("Menu::Files::DrawElement",
         (('new_var', 'thumbnail_texture_page_x'), ('new_var3', 'screen_width_pixels'), ('v0', 'thumbnail_texture_page_column'), ('var_a3', 'thumbnail_page_flags_and_dimming')),
         "F02756 src\\MENU\\MENU7.PRG\\260.c:1765-1946 (_drawFileMenuElement). Named roles: thumbnail texture page x, screen width pixels, thumbnail texture page column, thumbnail page flags and dimming."),
        ("Menu::Files::UpdateConfirmation",
         (('arg0', 'initialize_confirmation'), ('temp_v0', 'confirmation_element')),
         "F02759 src\\MENU\\MENU7.PRG\\260.c:2092-2153 (_promptConfirm). Named roles: initialize confirmation, confirmation element."),
        ("Menu::Files::UpdateOverwritePrompt",
         (('arg0', 'initialize_overwrite_prompt'), ('temp_v0', 'confirmation_result')),
         "F02760 src\\MENU\\MENU7.PRG\\260.c:2161-2191 (_promptOverwrite). Named roles: initialize overwrite prompt, confirmation result."),
        ("Menu::Files::UpdateLoadFiles",
         (('new_var', 'load_port_and_read_flags'),),
         "F02765 src\\MENU\\MENU7.PRG\\260.c:2878-3055 (_showLoadFilesMenu). Named roles: load port and read flags."),
        ("Menu::Files::LoadGameOverAssetsStep",
         (('arg0', 'begin_asset_load'),),
         "F02769 src\\MENU\\MENU7.PRG\\260.c:3395-3429 (_initGameOver). Named roles: begin asset load."),
        ("Menu::Rename::DrawFrame",
         (('a1', 'frame_overlay_ordering_table'), ('new_var3', 'name_character_x'), ('sp18', 'frame_ordering_table'), ('temp_s8', 'character_grid_origin_x'), ('v1', 'background_right_inset')),
         "F02781 src\\MENU\\MENU8.PRG\\21A0.c:133-313 (_renderRenameMenu). Named roles: frame overlay ordering table, name character x, frame ordering table, character grid origin x, background right inset."),
        ("Menu::Rename::DrawSelectionBox",
         (('temp_t0', 'selection_box_primitive'), ('temp_t4', 'selection_box_ordering_table')),
         "F02782 src\\MENU\\MENU8.PRG\\21A0.c:318-339 (_highlightSelection). Named roles: selection box primitive, selection box ordering table."),
        ("Menu::Rename::Update",
         (('v1', 'encoded_space'), ('var_v0', 'name_padding_cursor'), ('var_v1', 'weapon_name_cursor')),
         "F02787 src\\MENU\\MENU8.PRG\\21A0.c:820-903 (vs_menu8_execRename). Named roles: encoded space, name padding cursor, weapon name cursor."),
        ("Menu::Options::Update",
         (('temp_v1_2', 'settings_with_puzzle_mode'), ('var_v1', 'updated_settings_word')),
         "F02797 src\\MENU\\MENU8.PRG\\88.c:551-826 (vs_menu8_exec). Named roles: settings with puzzle mode, updated settings word."),
        ("Menu::Score::RenderScrollableRow",
         (('s5', 'gradient_intensity_offset'), ('temp_a0', 'texture_v'), ('temp_v1_2', 'texture_u')),
         "F02799 src\\MENU\\MENU9.PRG\\238.c:54-190 (_renderScrollableMenuItem). Named roles: gradient intensity offset, texture v, texture u. Retained: var_s0: Merged arrow animation Y, decoded text character, gradient intensity and texture-page X origin."),
        ("Menu::Score::Update",
         (('temp_v1_10', 'reselected_enemy_camera_distance'), ('temp_v1_4', 'initial_enemy_camera_distance'), ('var_s0', 'gazette_row_cursor')),
         "F02803 src\\MENU\\MENU9.PRG\\238.c:548-1397 (_menuInput). Named roles: reselected enemy camera distance, initial enemy camera distance, gazette row cursor."),
        ("Menu::Score::DrawEnemyBanner",
         (('var_s7', 'scanline_width'),),
         "F02810 src\\MENU\\MENU9.PRG\\238.c:1552-1589 (_renderEnemyInfoBg). Named roles: scanline width."),
        ("Menu::Score::DrawGradientPanel",
         (('a2', 'bottom_y'), ('a3', 'right_x')),
         "F02811 src\\MENU\\MENU9.PRG\\238.c:1594-1639 (_renderGradientQuad). Named roles: bottom y, right x."),
        ("Menu::Score::RenderOverview",
         (('sp18', 'saved_projection_offset'), ('sp20', 'overview_projection_offset'), ('sp28', 'score_text'), ('spA8', 'viewer_camera_matrix')),
         "F02813 src\\MENU\\MENU9.PRG\\238.c:1679-1717 (_renderTopMenu). Named roles: saved projection offset, overview projection offset, score text, viewer camera matrix."),
        ("Menu::Score::CreateTitleRows",
         (('new_var', 'title_flag_bit'),),
         "F02814 src\\MENU\\MENU9.PRG\\238.c:1722-1747 (_setTitleRows). Named roles: title flag bit."),
        ("Menu::Score::RenderEnemyList",
         (('sp18', 'list_slide_y_offset'),),
         "F02819 src\\MENU\\MENU9.PRG\\238.c:1972-2064 (_renderEnemyMenu). Named roles: list slide y offset."),
        ("Menu::Score::RenderEnemyViewer",
         (('sp30', 'viewer_camera_matrix'), ('sp50', 'saved_projection_offset'), ('sp58', 'viewer_projection_offset'), ('sp60', 'enemy_number_text'), ('temp_s0', 'control_label_x'), ('temp_s2', 'name_panel_slide_x'), ('temp_s4', 'controls_slide_x'), ('temp_s6', 'control_banner_x')),
         "F02820 src\\MENU\\MENU9.PRG\\238.c:2069-2141 (_renderEnemyDetailScreen). Named roles: viewer camera matrix, saved projection offset, viewer projection offset, enemy number text, control label x, name panel slide x, controls slide x, control banner x."),
        ("Menu::Score::RenderStatistics",
         (('new_var', 'record_cursor_memory'), ('s5', 'value_right_edge_x'), ('sp18', 'aligned_value_text'), ('temp_v1_4', 'selected_record_index'), ('var_s4', 'record_row_y')),
         "F02825 src\\MENU\\MENU9.PRG\\238.c:2226-2343 (_renderBossRushDetails). Named roles: record cursor memory, value right edge x, aligned value text, selected record index, record row y. Retained: v1: Merged record-statistics row ID, adjusted weapon-category ID and numeric-text left X."),
        ("Menu::Score::RenderTimeTrialRecords",
         (('sp28', 'record_value_text'), ('sp38', 'selected_trial_index'), ('sp3C', 'selected_trial_row'), ('var_fp', 'packed_trial_cursor_position'), ('var_s1', 'trial_row_cursor'), ('var_s7', 'settled_row_count')),
         "F02831 src\\MENU\\MENU9.PRG\\238.c:2507-2628 (_renderBossRushRecords). Named roles: record value text, selected trial index, selected trial row, packed trial cursor position, trial row cursor, settled row count."),
        ("Menu::Score::UpdateEnemyNavigation",
         (('new_var', 'arrow_base_x'),),
         "F02834 src\\MENU\\MENU9.PRG\\238.c:2782-2836 (_renderEnemyNavigation). Named roles: arrow base x."),
        ("Menu::Loot::UpdateStackConsolidation",
         (('v0', 'remaining_stack_capacity'),),
         "F02845 src\\MENU\\MENUB.PRG\\260.c:458-704 (_consolidateMiscItems). Named roles: remaining stack capacity. Retained: var_v0: Assigned zero but never consumed in original _consolidateMiscItems."),
        ("Menu::Loot::StageWeapon",
         (('arg0', 'looted_weapon'),),
         "F02849 src\\MENU\\MENUB.PRG\\260.c:792-820 (_copyWeaponToInventory). Named roles: looted weapon."),
        ("Menu::Loot::StageShield",
         (('arg0', 'looted_shield'),),
         "F02851 src\\MENU\\MENUB.PRG\\260.c:852-879 (_copyShieldToInventory). Named roles: looted shield."),
        ("Menu::Loot::StageMiscItem",
         (('arg0', 'looted_item'),),
         "F02852 src\\MENU\\MENUB.PRG\\260.c:886-902 (_copyMiscToInventory). Named roles: looted item."),
        ("Menu::Loot::CountContainedCategory",
         (('arg0', 'counted_category'), ('arg1', 'loot_list_item')),
         "F02855 src\\MENU\\MENUB.PRG\\260.c:968-1010 (_itemIncludesItemsInCategory). Named roles: counted category, loot list item."),
        ("Menu::Loot::RenderBagCapacities",
         (('var_a2', 'all_command_color'), ('var_fp', 'all_command_availability')),
         "F02856 src\\MENU\\MENUB.PRG\\260.c:1015-1105 (_displayInventoryCapacities). Named roles: all command color, all command availability."),
        ("Menu::Loot::IsDropAllowed",
         (('arg0', 'drop_difficulty_flags'),),
         "F02857 src\\MENU\\MENUB.PRG\\260.c:1113-1116 (_droppableUnderCurrentDifficulty). Named roles: drop difficulty flags."),
        ("Menu::Loot::StageDropRecord",
         (('arg0', 'drop_record'),),
         "F02858 src\\MENU\\MENUB.PRG\\260.c:1124-1168 (_applyLootList). Named roles: drop record."),
        ("Menu::Loot::RenderLootRows",
         (('temp_v1_3', 'loot_row_state'),),
         "F02860 src\\MENU\\MENUB.PRG\\260.c:1192-1313 (_displayCurrentLoot). Named roles: loot row state."),
        ("Menu::Loot::UpdateLeaveConfirmation",
         (('var_v1', 'leave_confirmation_result'),),
         "F02862 src\\MENU\\MENUB.PRG\\260.c:1377-1500 (_discardItems). Named roles: leave confirmation result."),
        ("Menu::Loot::UpdateLootSelection",
         (('var_v0_4', 'nested_menu_result'),),
         "F02864 src\\MENU\\MENUB.PRG\\260.c:1622-1943 (_processLootMenu). Named roles: nested menu result. Retained: temp_s2: Merged previous lootClaimed accumulator and later absolute selected-loot index."),
        ("Menu::Loot::RecordClaimedKey",
         (('arg0', 'item_id'),),
         "F02866 src\\MENU\\MENUB.PRG\\260.c:2108-2113 (_setKeyFlag). Named roles: item id."),
        ("Menu::Setup::UpdateConfirmationPrompt",
         (('arg0', 'initialize_confirmation'),),
         "F02873 src\\MENU\\MENUC.PRG\\168.c:155-217 (_confirmationPrompt). Named roles: initialize confirmation."),
        ("Menu::Setup::UpdateThreeChoicePrompt",
         (('arg0', 'cancel_combine_row'),),
         "F02874 src\\MENU\\MENUC.PRG\\168.c:225-304 (_confirmCombine). Named roles: cancel combine row."),
        ("Menu::Setup::LeaveItemSelection",
         (('arg0', 'hide_equipment_panels'),),
         "F02879 src\\MENU\\MENUC.PRG\\168.c:419-435 (_leaveItemSlotSelection). Named roles: hide equipment panels."),
        ("Menu::Setup::BuildItemList",
         (('temp_v1', 'encoded_row_text'),),
         "F02880 src\\MENU\\MENUC.PRG\\168.c:450-505 (_populateItemsList). Named roles: encoded row text."),
        ("Menu::Setup::UpdateItemList",
         (('arg0', 'list_update_flags'), ('unksp18', 'saved_row_gradient_states'), ('v1', 'outset_icon_code')),
         "F02882 src\\MENU\\MENUC.PRG\\168.c:518-724 (_navigateItemsList). Named roles: list update flags, saved row gradient states, outset icon code. Retained: v0: Two native local bindings: outer packed-row outset-icon code and inner item-list absolute row/index."),
        ("Menu::Setup::SetAssemblyStats",
         (('var_v1', 'assembly_part_mask'),),
         "F02883 src\\MENU\\MENUC.PRG\\168.c:730-757 (_setUiStats). Named roles: assembly part mask."),
        ("Menu::Setup::AnimateItemHeader",
         (('arg0', 'menu_row_id'),),
         "F02885 src\\MENU\\MENUC.PRG\\168.c:784-790 (_initMenuHeader). Named roles: menu row id."),
        ("Menu::Setup::UpdateAssembly",
         (('a0', 'packed_grip_selection'), ('arg0', 'initialize_assembly'), ('sp20', 'formatted_row_flags')),
         "F02890 src\\MENU\\MENUC.PRG\\168.c:1164-1594 (_assembleMenu). Named roles: packed grip selection, initialize assembly, formatted row flags. Retained: temp_v1: Declaration-only int in the assembly controller, with no assignments or reads in original/native body."),
        ("Menu::Setup::CreateEmptyGemSlotRow",
         (('arg1', 'gem_row_index'), ('temp_v0', 'empty_gem_row')),
         "F02891 src\\MENU\\MENUC.PRG\\168.c:1599-1607 (_addGemSelector). Named roles: gem row index, empty gem row. Retained: arg0: Explicitly unused first parameter of empty-gem-slot row helper; actual row is entirely determined by arg1."),
        ("Menu::Setup::UpdateGemAttachment",
         (('arg0', 'equipment_category_selection'), ('new_var', 'equipment_description_buffer'), ('new_var2', 'gem_row_base_id'), ('new_var3', 'gem_component_base'), ('var_a2', 'equipment_row_icon'), ('var_s2', 'equipment_id')),
         "F02893 src\\MENU\\MENUC.PRG\\168.c:1666-2009 (_attachGemsMenu). Named roles: equipment category selection, equipment description buffer, gem row base id, gem component base, equipment row icon, equipment id."),
        ("Menu::Setup::UpdateGemCategoryMenu",
         (('arg0', 'initialize_gem_category_menu'),),
         "F02894 src\\MENU\\MENUC.PRG\\168.c:2016-2091 (_attachGemsTopMenu). Named roles: initialize gem category menu."),
        ("Menu::Setup::UpdateDisassembly",
         (('arg0', 'equipment_category_selection'),),
         "F02895 src\\MENU\\MENUC.PRG\\168.c:2100-2278 (_disassembleMenu). Named roles: equipment category selection."),
        ("Menu::Setup::UpdateDisassemblyCategory",
         (('arg0', 'initialize_disassembly_category'),),
         "F02896 src\\MENU\\MENUC.PRG\\168.c:2285-2366 (_disassembleTopMenu). Named roles: initialize disassembly category."),
        ("Menu::Setup::UpdateWeaponRename",
         (('arg0', 'initialize_weapon_rename'),),
         "F02897 src\\MENU\\MENUC.PRG\\168.c:2374-2501 (_renameWeaponMenu). Named roles: initialize weapon rename."),
        ("Menu::Setup::UpdateRepair",
         (('arg0', 'initialize_repair'), ('new_var', 'repair_item_id')),
         "F02898 src\\MENU\\MENUC.PRG\\168.c:2509-2648 (_repairMenu). Named roles: initialize repair, repair item id."),
        ("Menu::Setup::PreviewBladeCombination",
         (('arg0', 'preview_selection'), ('temp_s2', 'second_blade_id'), ('temp_v1', 'selected_ingredient_index'), ('var_s3', 'ingredient_presence_mask')),
         "F02902 src\\MENU\\MENUC.PRG\\168.c:2735-2793 (_setCombineBladeUi). Named roles: preview selection, second blade id, selected ingredient index, ingredient presence mask."),
        ("Menu::Setup::SelectCombinationBlade",
         (('arg0', 'ingredient_slot'), ('new_var', 'available_blade_id'), ('temp_a0', 'parent_weapon_id')),
         "F02903 src\\MENU\\MENUC.PRG\\168.c:2801-2914 (_selectBlade). Named roles: ingredient slot, available blade id, parent weapon id."),
        ("Menu::Setup::UpdateBladeCombination",
         (('arg0', 'initialize_blade_combination'), ('var_a0_2', 'combination_help_text')),
         "F02905 src\\MENU\\MENUC.PRG\\168.c:2939-3312 (_combineBladeMenu). Named roles: initialize blade combination, combination help text. Retained: temp_v0_2: Merged popped ingredient slot and later three-choice confirmation return code; var_s1: Merged ingredient-presence boolean, result blade type, inventory blade ID, row loop index, packed cursor position, history slot, equipment/gem warning flags and parent weapon ID."),
        ("Menu::Setup::PreviewShieldCombination",
         (('arg0', 'preview_selection'), ('temp_v1', 'selected_ingredient_index'), ('var_s2', 'ingredient_presence_mask')),
         "F02907 src\\MENU\\MENUC.PRG\\168.c:3353-3399 (_initCombineShields). Named roles: preview selection, selected ingredient index, ingredient presence mask."),
        ("Menu::Setup::SelectCombinationShield",
         (('arg0', 'ingredient_slot'),),
         "F02909 src\\MENU\\MENUC.PRG\\168.c:3421-3543 (_selectShields). Named roles: ingredient slot."),
        ("Menu::Setup::UpdateShieldCombination",
         (('arg0', 'initialize_shield_combination'), ('temp_v0', 'combination_menu_row'), ('temp_v1_2', 'shield_gem_slot_count'), ('var_a0', 'gem_slot_index'), ('var_a0_2', 'combination_help_text')),
         "F02911 src\\MENU\\MENUC.PRG\\168.c:3562-3932 (_combineShieldMenu). Named roles: initialize shield combination, combination menu row, shield gem slot count, gem slot index, combination help text. Retained: temp_s1: result type versus inventory slot; temp_s2: history/UI row, item, confirmation and state result; var_s1: presence/result, row/item/gem data, packed cursor, history and equipment warnings. Preserve these merged lifetimes."),
        ("Menu::Setup::PreviewArmorCombination",
         (('arg0', 'preview_selection'), ('temp_s3', 'second_armor_id'), ('temp_v1', 'selected_ingredient_index'), ('var_s2', 'ingredient_presence_mask')),
         "F02912 src\\MENU\\MENUC.PRG\\168.c:3947-4011 (_initCombineArmor). Named roles: preview selection, second armor id, selected ingredient index, ingredient presence mask. Retained: var_a0: Only a declaration and +=2 inside the four-entry type-stat copy loop; no initialization or downstream consumer."),
        ("Menu::Setup::SelectCombinationArmor",
         (('arg0', 'ingredient_slot'), ('sp10', 'armor_row_text'), ('temp_v0', 'armor_text_allocation'), ('var_s2', 'armor_item'), ('var_s4', 'armor_row_count')),
         "F02913 src\\MENU\\MENUC.PRG\\168.c:4019-4121 (_initUiArmor). Named roles: ingredient slot, armor row text, armor text allocation, armor item, armor row count."),
        ("Menu::Setup::UpdateArmorCombination",
         (('arg0', 'initialize_armor_combination'), ('temp_a0', 'result_menu_row'), ('temp_v0', 'ingredient_menu_row'), ('temp_v0_2', 'restored_ingredient_slot'), ('temp_v0_6', 'confirmation_result'), ('var_a0_2', 'combination_help_text')),
         "F02915 src\\MENU\\MENUC.PRG\\168.c:4140-4422 (_combineArmorMenu). Named roles: initialize armor combination, result menu row, ingredient menu row, restored ingredient slot, confirmation result, combination help text. Retained: var_s1: Merged ingredient presence, result armor type, inventory armor slot, UI row loop, packed cursor position, history slot and equipped-limb warning/index."),
        ("Menu::Setup::CountCombinationIngredients",
         (('var_v1', 'is_combinable_armor'),),
         "F02916 src\\MENU\\MENUC.PRG\\168.c:4428-4447 (_countItems). Named roles: is combinable armor."),
        ("Menu::Setup::UpdateCombinationCategory",
         (('arg0', 'initialize_combination_category'), ('temp_s1', 'saved_cursor_memory')),
         "F02917 src\\MENU\\MENUC.PRG\\168.c:4455-4542 (_combineTopMenu). Named roles: initialize combination category, saved cursor memory."),
        ("Menu::Setup::ReloadAshleyEquipment",
         (('arg0', 'equipment_reload_mode'), ('new_var', 'ashley_actor')),
         "F02924 src\\MENU\\MENUC.PRG\\9258.c:7-43 (_loadObject). Named roles: equipment reload mode, ashley actor."),
        ("Menu::Container::PreviewTransferredMisc",
         (('var_a2', 'packed_transfer_item'),),
         "F02934 src\\MENU\\MENUD.PRG\\234.c:259-274 (func_801031A0). Named roles: packed transfer item."),
        ("Menu::Container::RebuildCategoryIndices",
         (('var_a1', 'next_occupied_index'),),
         "F02937 src\\MENU\\MENUD.PRG\\234.c:498-553 (_defragmentContainerItems). Named roles: next occupied index."),
        ("Menu::Container::SetPanelVisibility",
         (('arg0', 'panel_visibility'),),
         "F02942 src\\MENU\\MENUD.PRG\\234.c:659-664 (func_80103D50). Named roles: panel visibility."),
        ("Menu::Container::PollDetailsPaging",
         (('temp_a0', 'item_count_for_wrap'),),
         "F02943 src\\MENU\\MENUD.PRG\\234.c:666-698 (_pollItemNavigation). Named roles: item count for wrap."),
        ("Menu::Container::DrawCategoryIcons",
         (('arg1', 'details_view_phase'), ('temp_s0', 'icon_x'), ('temp_s4', 'selected_category_icon'), ('temp_v0', 'details_menu_row'), ('var_v1', 'icon_sprite_packet')),
         "F02944 src\\MENU\\MENUD.PRG\\234.c:700-733 (func_80103E24). Named roles: details view phase, icon x, selected category icon, details menu row, icon sprite packet. Retained: arg0: Merged input categoryIconsSlideStep and subsequently computed pixel Y=(step*8)-16."),
        ("Menu::Container::BeginDetailsTransition",
         (('arg0', 'menu_row_id'),),
         "F02945 src\\MENU\\MENUD.PRG\\234.c:735-751 (func_80103F64). Named roles: menu row id."),
        ("Menu::Container::InitializeDetailsLayout",
         (('arg0', 'selected_menu_row_id'), ('arg1', 'stat_page_panel_command')),
         "F02947 src\\MENU\\MENUD.PRG\\234.c:767-774 (func_80104034). Named roles: selected menu row id, stat page panel command."),
        ("Menu::Container::SelectDetailsItem",
         (('arg0', 'category_id'), ('arg1', 'category_item_index')),
         "F02949 src\\MENU\\MENUD.PRG\\234.c:789-794 (func_80104114). Named roles: category id, category item index."),
        ("Menu::Container::ExitDetailsView",
         (('arg0', 'hide_dp_pp_bars'),),
         "F02950 src\\MENU\\MENUD.PRG\\234.c:796-808 (func_80104170). Named roles: hide dp pp bars."),
        ("Menu::Container::UpdateBladeDetails",
         (('sp18', 'row_type'), ('temp_v0_3', 'blade_id')),
         "F02954 src\\MENU\\MENUD.PRG\\234.c:902-963 (_bladeNavigation). Named roles: row type, blade id."),
        ("Menu::Container::UpdateGripDetails",
         (('arg0', 'packed_details_selection'), ('sp10', 'row_text'), ('sp18', 'row_type'), ('temp_v0_2', 'selected_grip_index'), ('temp_v0_3', 'grip_id')),
         "F02955 src\\MENU\\MENUD.PRG\\234.c:965-1035 (_gripNavigation). Named roles: packed details selection, row text, row type, selected grip index, grip id."),
        ("Menu::Container::UpdateShieldDetails",
         (('arg0', 'packed_details_selection'), ('sp18', 'row_text'), ('sp20', 'row_type'), ('temp_v0_2', 'selected_shield_index')),
         "F02956 src\\MENU\\MENUD.PRG\\234.c:1037-1108 (_shieldNavigation). Named roles: packed details selection, row text, row type, selected shield index."),
        ("Menu::Container::UpdateArmorDetails",
         (('arg0', 'packed_details_selection'), ('sp10', 'row_text'), ('sp18', 'row_type'), ('temp_v0_2', 'selected_armor_index'), ('temp_v0_3', 'armor_id')),
         "F02957 src\\MENU\\MENUD.PRG\\234.c:1110-1174 (_armorNavigation). Named roles: packed details selection, row text, row type, selected armor index, armor id."),
        ("Menu::Container::UpdateGemDetails",
         (('arg0', 'packed_details_selection'), ('sp10', 'row_text'), ('sp18', 'row_type'), ('temp_v0_2', 'selected_gem_index'), ('temp_v0_3', 'gem_id')),
         "F02958 src\\MENU\\MENUD.PRG\\234.c:1176-1233 (_gemNavigation). Named roles: packed details selection, row text, row type, selected gem index, gem id."),
        ("Menu::Container::UpdateItemDetails",
         (('arg0', 'packed_category_selection'), ('temp_v0', 'detail_handler_result'), ('var_s0', 'packed_details_selection')),
         "F02959 src\\MENU\\MENUD.PRG\\234.c:1235-1271 (func_80105008). Named roles: packed category selection, detail handler result, packed details selection."),
        ("Menu::Container::SortShields",
         (('arg0', 'sort_stat_id'), ('sp10', 'shield_stats'), ('sp178', 'sorted_shield_indices'), ('temp_s0', 'shield_id'), ('temp_s5', 'shield_items'), ('temp_s6', 'shield_indices'), ('temp_v0', 'shield_sort_value'), ('var_s3', 'highest_remaining_sort_value'), ('var_s4', 'sorted_shield_count')),
         "F02963 src\\MENU\\MENUD.PRG\\234.c:1388-1435 (_sortShieldsByStat). Named roles: sort stat id, shield stats, sorted shield indices, shield id, shield items, shield indices, shield sort value, highest remaining sort value, sorted shield count."),
        ("Menu::Container::SortMisc",
         (('arg0', 'sort_stat_id'), ('sp10', 'sorted_misc_indices'), ('temp_s0', 'misc_item_id'), ('temp_s7', 'misc_indices'), ('temp_v0', 'misc_sort_value'), ('var_s4', 'highest_remaining_sort_value'), ('var_s5', 'sorted_misc_count')),
         "F02965 src\\MENU\\MENUD.PRG\\234.c:1445-1487 (_sortItemsByStat). Named roles: sort stat id, sorted misc indices, misc item id, misc indices, misc sort value, highest remaining sort value, sorted misc count."),
        ("Menu::Container::SortEquipment",
         (('arg0', 'category_id'), ('arg1', 'sort_stat_id'), ('sp10', 'equipment_stats'), ('sp40', 'sorted_equipment_indices'), ('temp_s0', 'equipment_id'), ('temp_s5', 'category_capacity'), ('temp_s7', 'category_indices'), ('temp_v0', 'equipment_sort_value'), ('var_s4', 'highest_remaining_sort_value'), ('var_s6', 'sorted_equipment_count')),
         "F02968 src\\MENU\\MENUD.PRG\\234.c:1549-1599 (_copyEquipmentByStat). Named roles: category id, sort stat id, equipment stats, sorted equipment indices, equipment id, category capacity, category indices, equipment sort value, highest remaining sort value, sorted equipment count."),
        ("Menu::Container::UpdateSortMenu",
         (('arg0', 'category_selection'), ('new_var', 'sort_stat_ids'), ('sp10', 'sort_row_text'), ('spD0', 'sort_row_types')),
         "F02969 src\\MENU\\MENUD.PRG\\234.c:1601-1678 (_displaySortMenu). Named roles: category selection, sort stat ids, sort row text, sort row types."),
        ("Menu::Container::UpdateDiscardMenu",
         (('a2', 'previous_discard_count'), ('arg0', 'packed_discard_selection'), ('temp_v0_7', 'packed_discard_digits')),
         "F02970 src\\MENU\\MENUD.PRG\\234.c:1680-1919 (_discardMenu). Named roles: previous discard count, packed discard selection, packed discard digits. Retained: a0: Merged candidate discard quantity and previous discard-step selector; var_v0: Declared and assigned zero without any read in _discardMenu; retain the decompiler dead temporary rather than invent a UI role or remove it."),
        ("Menu::Container::DrawCapacityCount",
         (('arg0', 'capacity_limit'), ('arg1', 'packed_count_position'), ('arg2', 'occupied_count'), ('temp_s0', 'packed_used_count_position'), ('var_s2', 'used_count_color')),
         "F02971 src\\MENU\\MENUD.PRG\\234.c:1925-1938 (func_80106464). Named roles: capacity limit, packed count position, occupied count, packed used count position, used count color."),
        ("Menu::Container::CountWeaponGems",
         (('arg0', 'weapon_index'),),
         "F02973 src\\MENU\\MENUD.PRG\\234.c:1959-1969 (_getWeaponsubItemCount). Named roles: weapon index."),
        ("Menu::Container::CountShieldGems",
         (('arg0', 'shield_index'),),
         "F02974 src\\MENU\\MENUD.PRG\\234.c:1971-1981 (_getShieldsubItemCount). Named roles: shield index."),
        ("Menu::Container::WouldExceedCapacity",
         (('arg0', 'category_id'), ('arg1', 'additional_item_count'), ('temp_s0', 'destination_item_count'), ('temp_s1', 'projected_item_count')),
         "F02975 src\\MENU\\MENUD.PRG\\234.c:1983-1993 (func_801066DC). Named roles: category id, additional item count, destination item count, projected item count."),
        ("Menu::Container::TransferCapacityError",
         (('arg0', 'category_id'), ('arg1', 'item_index')),
         "F02976 src\\MENU\\MENUD.PRG\\234.c:1995-2038 (func_80106784). Named roles: category id, item index."),
        ("Menu::Container::UndoPendingTransfer",
         (('arg0', 'pending_transfer_index'), ('temp_s0', 'packed_transfer_item'), ('temp_s0_2', 'staged_item_index'), ('temp_s2', 'category_id')),
         "F02977 src\\MENU\\MENUD.PRG\\234.c:2040-2052 (func_801068BC). Named roles: pending transfer index, packed transfer item, staged item index, category id."),
        ("Menu::Container::StageItemTransfer",
         (('arg0', 'category_id'), ('arg1', 'item_index')),
         "F02978 src\\MENU\\MENUD.PRG\\234.c:2054-2059 (func_80106948). Named roles: category id, item index."),
        ("Menu::Container::FindPendingTransfer",
         (('arg0', 'category_id'), ('arg1', 'item_index')),
         "F02979 src\\MENU\\MENUD.PRG\\234.c:2061-2071 (func_801069B0). Named roles: category id, item index."),
        ("Menu::Container::AppendPendingTransfer",
         (('arg0', 'category_id'), ('arg1', 'item_index'), ('var_v1', 'pending_transfer_items')),
         "F02980 src\\MENU\\MENUD.PRG\\234.c:2073-2086 (func_80106A04). Named roles: category id, item index, pending transfer items."),
        ("Menu::Container::RebuildPendingTransfers",
         (('temp_s2', 'pending_transfer_items'), ('var_a1', 'next_occupied_transfer_index')),
         "F02981 src\\MENU\\MENUD.PRG\\234.c:2088-2143 (func_80106A50). Named roles: pending transfer items, next occupied transfer index."),
        ("Menu::Container::RestoreItemRows",
         (('arg0', 'category_id'), ('temp_v0', 'restored_menu_row'), ('temp_v0_2', 'selected_row_mismatch')),
         "F02983 src\\MENU\\MENUD.PRG\\234.c:2309-2330 (func_801071D8). Named roles: category id, restored menu row, selected row mismatch."),
        ("Menu::Container::UpdateTransferConfirmation",
         (('arg0', 'initialize_confirmation'), ('sp10', 'confirmation_row_text'), ('sp20', 'confirmation_row_types'), ('temp_s0', 'confirmation_step')),
         "F02984 src\\MENU\\MENUD.PRG\\234.c:2332-2391 (func_801072B0). Named roles: initialize confirmation, confirmation row text, confirmation row types, confirmation step."),
        ("Menu::Container::UpdateItemList",
         (('arg0', 'transfer_direction_selection'), ('s6', 'details_index_bias'), ('sp10', 'action_row_text'), ('temp_s0_2', 'rebuilt_row_text'), ('temp_s1_3', 'rebuilt_row_types'), ('temp_s1_4', 'pending_row_types'), ('temp_s3_7', 'pending_row_text'), ('temp_s5', 'initial_row_text'), ('temp_s6', 'initial_row_types'), ('temp_v0_11', 'category_title_row'), ('temp_v0_17', 'parent_item_row'), ('temp_v0_18', 'category_cursor_memory_offset'), ('temp_v0_19', 'pending_rows_allocation'), ('temp_v0_7', 'rebuilt_rows_allocation'), ('temp_v0_8', 'previous_category_buttons_held'), ('var_s4', 'selected_category_id'), ('var_v0_3', 'next_category_buttons_repeated')),
         "F02985 src\\MENU\\MENUD.PRG\\234.c:2393-3035 (func_80107490). Named roles: transfer-direction input; action text; initial/rebuilt/pending row text, types and allocations; category/title/cursor state and button inputs. Retained: temp_a0/temp_s0: separate shadowed counters, scrolling and item-state locals; temp_s1: category indices versus allocated row buffer; temp_s2/temp_s3/var_a2: preserved count/input/item/selection/result lifetimes; var_a1: unread writes."),
        ("Menu::Container::UpdateTransferDirection",
         (('arg0', 'initialize_direction_menu'), ('a0', 'direction_help_text'), ('temp_v0_2', 'selected_direction_row'), ('var_s1', 'selected_direction')),
         "F02986 src\\MENU\\MENUD.PRG\\234.c:3037-3126 (func_801089BC). Named roles: initialize direction menu, direction help text, selected direction row, selected direction."),
        ("Menu::Container::UpdateScreen",
         (('arg0', 'screen_step'), ('temp_a2', 'projected_category_count'), ('temp_s0', 'control_icon_x'), ('temp_s0_2', 'destination_category_count'), ('var_a0', 'category_capacity'), ('var_s6', 'packed_capacity_row_position'), ('var_v0', 'capacity_slide_offset')),
         "F02987 src\\MENU\\MENUD.PRG\\234.c:3130-3352 (vs_menuD_exec). Named roles: screen step, projected category count, control icon x, destination category count, category capacity, packed capacity row position, capacity slide offset. Retained: s3: Merged control-label X coordinate and later category loop index 0..6."),
        ("Menu::Manual::CopyEncodedLine",
         (('arg0', 'destination_text'), ('arg1', 'source_text')),
         "F02996 src\\MENU\\MENUE.PRG\\494.c:561-575 (_vsStrcpy). Named roles: destination text, source text."),
        ("Menu::Manual::MinTileExtent",
         (('arg0', 'remaining_extent'), ('arg1', 'tile_extent_limit')),
         "F02998 src\\MENU\\MENUE.PRG\\494.c:619-625 (_min). Named roles: remaining extent, tile extent limit."),
        ("Menu::Manual::AddSpriteOffset",
         (('arg0', 'sprite_origin'), ('arg1', 'tile_offset')),
         "F02999 src\\MENU\\MENUE.PRG\\494.c:627-627 (_add_int). Named roles: sprite origin, tile offset."),
        ("Menu::Manual::DrawUpperScrollFade",
         (('var_v0', 'transparency_source'), ('var_v1', 'transparency_destination')),
         "F03002 src\\MENU\\MENUE.PRG\\494.c:740-789 (_fadeMenuUpper). Named roles: transparency source, transparency destination."),
        ("Menu::Manual::DrawLowerScrollFade",
         (('var_v0', 'transparency_source'), ('var_v1', 'transparency_destination')),
         "F03003 src\\MENU\\MENUE.PRG\\494.c:791-842 (_fadeMenuLower). Named roles: transparency source, transparency destination."),
        ("Menu::Manual::DrawPageArrow",
         (('v0', 'upper_arrow_base_y'),),
         "F03004 src\\MENU\\MENUE.PRG\\494.c:844-881 (_drawPaginationArrow). Named roles: upper arrow base y."),
        ("Menu::Results::UpdateGameClear",
         (('temp_a2_2', 'prompt_slide_step'), ('temp_v1', 'updated_reel_phase')),
         "F03015 src\\MENU\\MENUF.PRG\\3B8.c:755-893 (_renderCongratulationsScreen). Named roles: prompt slide step, updated reel phase."),
        ("Menu::Results::UpdateTimeAttackResults",
         (('var_v0', 'palette_pulse_sine'),),
         "F03016 src\\MENU\\MENUF.PRG\\3B8.c:905-1006 (_renderTimeAttackEnd). Named roles: palette pulse sine."),
        ("Menu::Results::UpdateTimeAttackStart",
         (('temp_v1_2', 'countdown_interval'),),
         "F03017 src\\MENU\\MENUF.PRG\\3B8.c:1014-1144 (_renderTimeAttackStart). Named roles: countdown interval."),
        ("Menu::Results::DrawCourseNumber",
         (('new_var', 'course_number_base_x'),),
         "F03019 src\\MENU\\MENUF.PRG\\3B8.c:1175-1193 (_renderCourse). Named roles: course number base x."),
        ("Menu::Results::DrawBestTime",
         (('new_var', 'best_time_base_x'),),
         "F03021 src\\MENU\\MENUF.PRG\\3B8.c:1226-1246 (_renderBestTimeHeader). Named roles: best time base x."),
        ("Menu::Results::DrawStartCountdown",
         (('temp_a2', 'countdown_texture_id'),),
         "F03022 src\\MENU\\MENUF.PRG\\3B8.c:1248-1254 (_renderStartCountdown). Named roles: countdown texture id."),
        ("Menu::Results::PulseTimeAttackClut",
         (('arg0', 'blend_weight'), ('new_var3', 'packed_blue'), ('new_var4', 'packed_red_green'), ('var_a1', 'weighted_red')),
         "F03023 src\\MENU\\MENUF.PRG\\3B8.c:1256-1292 (_pulseTimeAttackEnd). Named roles: blend weight, packed blue, packed red green, weighted red."),
        ("Menu::Results::PulseCubePuzzleClut",
         (('arg0', 'blend_weight'), ('new_var3', 'packed_blue'), ('new_var4', 'packed_red_green'), ('var_a1', 'weighted_red'), ('var_a2', 'weighted_green')),
         "F03024 src\\MENU\\MENUF.PRG\\3B8.c:1294-1330 (_pulseCubePuzzleEnd). Named roles: blend weight, packed blue, packed red green, weighted red, weighted green."),
        ("Menu::Results::DrawScore",
         (('new_var', 'score_label_and_colon_width'),),
         "F03026 src\\MENU\\MENUF.PRG\\3B8.c:1357-1407 (_renderScore). Named roles: score label and colon width. Retained: arg3: Explicitly unused fourth results-render parameter; callers pass screen state for the shared rendering convention."),
        ("Menu::Results::DrawCountingScore",
         (('temp_s2', 'wipe_edge_x'),),
         "F03027 src\\MENU\\MENUF.PRG\\3B8.c:1409-1484 (_renderIncrementalScore). Named roles: wipe edge x. Retained: arg3: Explicitly unused screen-state slot in the common results rendering call shape."),
        ("Menu::Results::DrawCountingMapCompletion",
         (('arg0', 'draw_x'), ('arg1', 'draw_y'), ('arg2', 'animation_timer'), ('temp_s4', 'wipe_edge_x')),
         "F03029 src\\MENU\\MENUF.PRG\\3B8.c:1527-1596 (_renderIncrementalMapCompletion). Named roles: draw x, draw y, animation timer, wipe edge x. Retained: arg3: Explicitly unused fourth results-render parameter."),
        ("Menu::Results::DrawTimeAttackRating",
         (('arg2', 'fade_timer'), ('var_a2', 'rating_texture_offset')),
         "F03033 src\\MENU\\MENUF.PRG\\3B8.c:1678-1706 (_renderTimeAttackRating). Named roles: fade timer, rating texture offset."),
        ("Menu::Results::DrawTimeAttackRatingWipe",
         (('arg3', 'wipe_edge_x'),),
         "F03034 src\\MENU\\MENUF.PRG\\3B8.c:1708-1741 (_renderTimeAttackRatingWipe). Named roles: wipe edge x. Retained: arg2: Merged input fade/wipe timer clamped to 0..64 and later rating texture offset 0..4."),
        ("Menu::Results::DrawTimeAttackTime",
         (('arg0', 'draw_x'), ('arg1', 'draw_y'), ('arg2', 'fade_timer')),
         "F03035 src\\MENU\\MENUF.PRG\\3B8.c:1743-1760 (_renderHeaderAndTime). Named roles: draw x, draw y, fade timer."),
        ("Menu::Results::DrawCountingTimeAttackTime",
         (('new_var4', 'time_accumulator_address'), ('temp_a3', 'total_centiseconds')),
         "F03036 src\\MENU\\MENUF.PRG\\3B8.c:1762-1799 (_renderHeaderAndIncrementalTime). Named roles: time accumulator address, total centiseconds. Retained: var_a3: Merged interpolated total centiseconds, remainder below a minute, remainder below a second and final packed minutes/seconds/centiseconds display word."),
        ("Menu::Results::DrawLeaderboard",
         (('var_s3', 'leaderboard_clut'),),
         "F03038 src\\MENU\\MENUF.PRG\\3B8.c:1816-1847 (_renderAllStoredTimes). Named roles: leaderboard clut."),
        ("Menu::Results::DrawTime",
         (('s2', 'time_digits_clut'),),
         "F03042 src\\MENU\\MENUF.PRG\\3B8.c:1951-1992 (_renderTime). Named roles: time digits clut."),
        ("Menu::Results::DrawBonusReel",
         (('a0', 'reel_origin_x'), ('arg3', 'results_screen_state'), ('new_var', 'reel_row_offset'), ('new_var2', 'reel_base_clip_height'), ('sp18', 'reel_clip_rect'), ('v1', 'reel_entry_index')),
         "F03043 src\\MENU\\MENUF.PRG\\3B8.c:1996-2115 (_renderStatWheel). Named roles: reel origin x, results screen state, reel row offset, reel base clip height, reel clip rect, reel entry index."),
        ("Menu::Results::DrawPressButtonPrompt",
         (('arg2', 'slide_timer'), ('arg3', 'bounce_timer')),
         "F03044 src\\MENU\\MENUF.PRG\\3B8.c:2117-2164 (_renderPressButtonPrompt). Named roles: slide timer, bounce timer. Retained: v1: Merged pre-slide X origin and two bounce Y baselines."),
        ("Menu::Results::UpdateCubePuzzleResults",
         (('var_v0', 'rank_index_bias'),),
         "F03050 src\\MENU\\MENUF.PRG\\3B8.c:2477-2561 (_renderCubePuzzleEnd). Named roles: rank index bias."),
        ("Menu::Results::DrawGivingUp",
         (('arg0', 'draw_y'), ('arg1', 'fade_timer')),
         "F03052 src\\MENU\\MENUF.PRG\\3B8.c:2610-2622 (_renderGivingUp). Named roles: draw y, fade timer."),
        ("Menu::Results::DrawChicken",
         (('arg0', 'draw_y'), ('arg1', 'fade_timer')),
         "F03053 src\\MENU\\MENUF.PRG\\3B8.c:2624-2638 (_renderChicken). Named roles: draw y, fade timer."),
        ("Menu::Results::DrawCubePuzzleParTime",
         (('new_var', 'seconds_shifted_for_packing'),),
         "F03057 src\\MENU\\MENUF.PRG\\3B8.c:2699-2710 (_renderCubePuzzleTime). Named roles: seconds shifted for packing."),
        ("Menu::Results::DrawCountingCubePuzzleTime",
         (('new_var2', 'display_minutes'), ('new_var4', 'time_accumulator_address')),
         "F03059 src\\MENU\\MENUF.PRG\\3B8.c:2732-2772 (_renderCubePuzzleIncrementalTime). Named roles: display minutes, time accumulator address. Retained: temp_a3: Merged raw puzzle seconds, full duration in centiseconds, and interpolated seconds component."),
        ("Menu::Results::ApplyHealingScoreBonus",
         (('new_var', 'score_before_healing_penalty'),),
         "F03064 src\\MENU\\MENUF.PRG\\3B8.c:2865-2876 (_addHealBonus). Named roles: score before healing penalty."),
        ("Menu::Framework::ReturnFromSubmenu",
         (('arg0', 'packed_menu_state'),),
         "F04145 src\\MENU\\MAINMENU.PRG\\C48.c:316-332 (func_800FAA5C). Named roles: packed menu state."),
        ("Menu::Framework::UpdateMainMenu",
         (('arg0', 'packed_menu_state'),),
         "F04146 src\\MENU\\MAINMENU.PRG\\C48.c:334-478 (vs_mainMenu_exec). Named roles: packed menu state."),
        ("Menu::Map::LoadBackground",
         (('arg0', 'begin_background_load'),),
         "F04148 src\\MENU\\MENU5.PRG\\4D8.c:206-238 (_loadMapBackground). Named roles: begin background load."),
        ("Menu::Map::UpdateMapMenu",
         (('new_var2', 'area_selection_result'), ('var_s2', 'controls_state')),
         "F04151 src\\MENU\\MENU5.PRG\\4D8.c:523-893 (_menuManager). Named roles: area selection result, controls state."),
        ("Menu::Map::DrawVisitedRooms",
         (('new_var', 'room_count_address'),),
         "F04152 src\\MENU\\MENU5.PRG\\4D8.c:1081-1110 (_renderMap). Named roles: room count address."),
        ("Menu::Map::DrawRoom",
         (('arg2', 'zone_id'), ('arg3', 'map_id'), ('s2', 'legacy_render_scratch'), ('sp28', 'available_marker_kinds'), ('v0', 'projected_depth'), ('var_s1', 'projected_vertex_rows'), ('var_s3', 'room_element_cursor')),
         "F04153 src\\MENU\\MENU5.PRG\\4D8.c:1112-1509 (_renderRoom). Named roles: zone id, map id, legacy render scratch, available marker kinds, projected depth, projected vertex rows, room element cursor. Retained: new_var: Merged const-int pointer to serialized element count and later Ashley-icon brightness table; sp38: Merged RotTransPers depth-cue output (stored in projected vertex word 3) and later polygon/line/tile ordering-table depth index (from word 2); sp3C: Merged RotTransPers projection flags and later room-feature highlight/draw predicate."),
        ("Menu::Manual::UpdateHelpPages",
         (('s0', 'frame_scratch'),),
         "F04163 src\\MENU\\MENUE.PRG\\494.c:245-448 (_showMenu). Named roles: frame scratch."),
    )
    for old, new in bindings
]


# Individually reviewed battle bindings; structural fixups run first.
# Separate shadowed locals and cast-prototype parameters before naming outer bindings.
for identifier, old, new, evidence in (
    ("F00523",
     "int arg0, int arg1, u_int arg2, int arg3",
     "int actor_slot, int limb_tag, u_int encoded_amount, int label_flags",
     "Independent parameters inside QueueActorValueLabel function-pointer cast, not enclosing locals. F00714 resolves actor/limb and encodes amount sign plus label color/visibility flags. Preserve types and ABI."),
    ("F01355",
     "                    int v0 = arg2 - 8;\n                    arg2 = v0 + var_s1;\n",
     "                    int clipped_icon_left = arg2 - 8;\n                    arg2 = clipped_icon_left + var_s1;\n",
     "First block v0 is arg2-8, a temporary left screen X used to anchor the clipped lower strip."),
    ("F01355",
     "                int v0 = (arg0 & 3) * 8 + 0x3070;\n                var_s2 = v0 - var_s1;\n",
     "                int overlay_texture_end = (arg0 & 3) * 8 + 0x3070;\n                var_s2 = overlay_texture_end - var_s1;\n",
     "Separate block v0 is atlas packed UV endpoint (arg0&3)*8+0x3070; subtracting clipped_width chooses strip UV. Not the screen-X local above."),
):
    FIXES.setdefault(identifier, []).append((old, new, evidence))

REGEX_FIXES += [
    (rf"^{name}$", rf"\b{old}\b", new, evidence)
    for name, bindings, evidence in (
        ("Battle::ResolveEnemyDefeat",
         (('arg0', 'source_actor_slot'), ('arg2', 'count_kill')),
         "F00048 F00048 Battle::ResolveEnemyDefeat: original _enemyDefeated at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:1824 (function lines 1824-1840). arg0 -> source_actor_slot (source actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:1824-1832. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:87-95. arg2 -> count_kill (count kill). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:1824-1832. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:87-95. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::PlayWeaponHitSounds",
         (('arg0', 'source_actor_slot'), ('sp10', 'target_placement'), ('sp20', 'hit_position'), ('temp_s3', 'queued_action')),
         "F00059 F00059 Battle::PlayWeaponHitSounds: original func_8006C5AC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:1955 (function lines 1955-2042). arg0 -> source_actor_slot (source actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:1955-1978. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:106-129. sp10 -> target_placement (target placement). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:1957-1971. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:108-122. sp20 -> hit_position (hit position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:1958-2032. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:109-183. temp_s3 -> queued_action (queued action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:1959-1973. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:110-124. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::ApplyActionSourceEffects",
         (('arg0', 'source_actor_slot'), ('temp_s0', 'queued_action')),
         "F00061 F00061 Battle::ApplyActionSourceEffects: original func_8006CA20 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:2106 (function lines 2106-2139). arg0 -> source_actor_slot (source actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:2106-2130. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:196-220. arg1 intentionally retained: Original func_8006CA20 SVECTOR formal has no reads/writes. Effects Timeline forwards NULL or an effect source position, but the callback ignores it. Retain unused interface slot rather than impose an active role. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:2106. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:196. temp_s0 -> queued_action (queued action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:2108-2137. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:198-227. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::BeginPostActionRecovery",
         (('arg0', 'recovery_reward_mode'),),
         "F00096 F00096 Battle::BeginPostActionRecovery: original func_8006FBCC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3388 (function lines 3388-3406). arg0 -> recovery_reward_mode (recovery reward mode). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3388-3403. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:240-255. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::BeginActionTrailFade",
         (('temp_s0', 'initial_action'),),
         "F00097 F00097 Battle::BeginActionTrailFade: original func_8006FCBC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3408 (function lines 3408-3416). temp_s0 -> initial_action (initial action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3410-3415. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:263-268. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::StartActionAnimation",
         (('temp_s0', 'queued_action'), ('temp_a3', 'chain_index')),
         "F00100 F00100 Battle::StartActionAnimation: original func_80070278 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3510 (function lines 3510-3570). temp_a3 -> chain_index (chain index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3513-3555. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:278-320. temp_s0 -> queued_action (queued action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3512-3560. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:277-325. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::CommitSelectedAction",
         (('arg0', 'target_selection_mode'), ('temp_s0', 'selected_preview'), ('temp_v1_2', 'action_id')),
         "F00108 F00108 Battle::CommitSelectedAction: original func_80070F28 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3775 (function lines 3775-3904). arg0 -> target_selection_mode (target selection mode). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3775-3826. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:345-396. temp_s0 -> selected_preview (selected preview). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3779-3790. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:349-360. temp_v1_2 -> action_id (action id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3778-3836. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:348-406. var_a0 intentionally retained: Original func_80070F28 and native definition contain only int var_a0; no initializer, read or write. Unused decompiler declaration has no proven runtime role. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:3780. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:350. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::BeginActorDefeatAnimation",
         (('arg0', 'actor_slot'),),
         "F00156 F00156 Battle::BeginActorDefeatAnimation: original func_80074A20 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:5315 (function lines 5315-5337). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:5315-5330. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:677-692. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::ActionTargetLimbPosition",
         (('arg0', 'actor_slot'), ('arg1', 'limb_tag'), ('arg2', 'position_out'), ('var_v0', 'bone_index')),
         "F00179 F00179 Battle::ActionTargetLimbPosition: original func_80077F14 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6821 (function lines 6821-6831). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6821-6830. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:37-46. arg1 -> limb_tag (limb tag). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6821-6828. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:37-44. arg2 -> position_out (position out). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6821-6830. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:37-46. var_v0 -> bone_index (bone index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6823-6830. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:39-46. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::ActionTargetDistance",
         (('sp10', 'source_position'), ('sp18', 'target_position'), ('temp_s0', 'queued_action'), ('temp_v0', 'delta_x'), ('temp_v0_2', 'delta_z'), ('temp_v0_3', 'horizontal_distance'), ('temp_v1', 'delta_y')),
         "F00180 F00180 Battle::ActionTargetDistance: original func_80077F70 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6833 (function lines 6833-6861). sp10 -> source_position (source position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6835-6858. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:52-75. sp18 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6836-6858. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:53-75. temp_s0 -> queued_action (queued action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6837-6853. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:54-70. temp_v0 -> delta_x (delta x). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6838-6857. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:55-74. temp_v0_2 -> delta_z (delta z). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6839-6857. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:56-74. temp_v0_3 -> horizontal_distance (horizontal distance). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6840-6860. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:57-77. temp_v1 -> delta_y (delta y). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6841-6860. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:58-77. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::ActionCameraMidpoint",
         (('arg0', 'midpoint_out'), ('sp10', 'source_position'), ('sp18', 'target_position'), ('temp_s0', 'queued_action')),
         "F00181 F00181 Battle::ActionCameraMidpoint: original func_800780A8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6863 (function lines 6863-6884). arg0 -> midpoint_out (midpoint out). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6863-6883. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:81-101. sp10 -> source_position (source position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6865-6883. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:83-101. sp18 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6866-6883. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:84-101. temp_s0 -> queued_action (queued action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6867-6879. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:85-97. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::StepCameraTowardActionMidpoint",
         (('arg0', 'update_camera'), ('sp10', 'action_midpoint')),
         "F00182 F00182 Battle::StepCameraTowardActionMidpoint: original func_8007820C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6886 (function lines 6886-6894). arg0 -> update_camera (update camera). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6886-6891. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:105-110. sp10 -> action_midpoint (action midpoint). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6888-6892. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:107-111. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::UpdateActionCameraZoom",
         (('sp18', 'action_midpoint'), ('sp20', 'camera_displacement'), ('sp30', 'camera_step')),
         "F00186 F00186 Battle::UpdateActionCameraZoom: original _handleCombatCameraZoom at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6958 (function lines 6958-7023). sp18 -> action_midpoint (action midpoint). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6960-7009. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:183-238. sp20 -> camera_displacement (camera displacement). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6961-7010. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:184-240. sp30 -> camera_step (camera step). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:6962-7009. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:185-238. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::WaitForActionEffectReady",
         (('temp_s0', 'queued_action'),),
         "F00187 F00187 Battle::WaitForActionEffectReady: original func_80078748 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:7025 (function lines 7025-7033). temp_s0 -> queued_action (queued action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:7027-7029. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:258-260. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::WaitForTargetRecovery",
         (('var_s3', 'queued_action'),),
         "F00193 F00193 Battle::WaitForTargetRecovery: original func_800790BC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:7364 (function lines 7364-7411). var_s3 -> queued_action (queued action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:7366-7405. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:290-329. var_s4 intentionally retained: Intentionally merged original local: initially boolean all target animations idle, then recovery/reward mode 0/1/2 sent to BeginPostActionRecovery after defeat processing. Preserve one binding; do not falsely name both phases as one boolean or split lifetime. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:7369-7409. Native references: Native\\Game\\src\\Battle\\ActionExecution.cpp:293-333. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::ResolveActorDefeat",
         (('arg0', 'source_actor_slot'), ('arg1', 'target_actor_slot'), ('arg2', 'count_kill')),
         "F00194 F00194 Battle::ResolveActorDefeat: original func_800792E4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:7413 (function lines 7413-7432). arg0 -> source_actor_slot (source actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:7413-7418. Native references: Native\\Game\\src\\Battle\\Damage.cpp:47-52. arg1 -> target_actor_slot (target actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:7413-7428. Native references: Native\\Game\\src\\Battle\\Damage.cpp:47-62. arg2 -> count_kill (count kill). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:7413-7418. Native references: Native\\Game\\src\\Battle\\Damage.cpp:47-52. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::ShowPendingRewardUnlock",
         (('temp_v0_2', 'pending_unlocks'),),
         "F00220 F00220 Battle::ShowPendingRewardUnlock: original func_8007B63C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:8346 (function lines 8346-8364). temp_v0_2 -> pending_unlocks (pending unlocks). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:8348-8363. Native references: Native\\Game\\src\\Battle\\Rewards.cpp:129-144. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::SetGameplayHudHidden",
         (('arg0', 'hidden'),),
         "F00235 F00235 Battle::SetGameplayHudHidden: original func_8007BD8C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:8561 (function lines 8561-8571). arg0 -> hidden (hidden). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:8561-8570. Native references: Native\\Game\\src\\Battle\\Hud.cpp:131-140. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::IgnoreHitCallback",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00341 F00341 Battle::IgnoreHitCallback: original _invalidHitFunction at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9860 (function lines 9860-9865). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9860. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:174. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9861. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:175. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9862. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:176. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9862. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:176. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9863. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:177. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::PreviousHitSuccessThreshold",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00342 F00342 Battle::PreviousHitSuccessThreshold: original func_8007E97C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9867 (function lines 9867-9873). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9867. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:182. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9868. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:183. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9869. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:184. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9869. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:184. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9870. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:185. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::GemHitChance",
         (('arg4', 'execute_action'),),
         "F00343 F00343 Battle::GemHitChance: original _getHitGemModifier at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9875 (function lines 9875-9888). arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9876. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:192. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::AgilityHitChance",
         (('v1', 'variance_base_chance'),),
         "F00344 F00344 Battle::AgilityHitChance: original _getAgilityDifference at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9890 (function lines 9890-9939). v1 -> variance_base_chance (variance base chance). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9925-9926. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:242-243. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved. v1 is the chance-minus-five base before adding NextAveraged(11) variance; name deliberately describes this intermediate, not a fully calculated random chance."),
        ("Battle::IntelligenceHitChance",
         (('v1', 'variance_base_chance'),),
         "F00345 F00345 Battle::IntelligenceHitChance: original _getIntelligenceDifference at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9941 (function lines 9941-9990). v1 -> variance_base_chance (variance base chance). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9976-9977. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:294-295. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved. v1 is the chance-minus-five base before adding NextAveraged(11) variance; name deliberately describes this intermediate, not a fully calculated random chance."),
        ("Battle::CostAdjustedAgilityHitChance",
         (('v1', 'variance_base_chance'),),
         "F00346 F00346 Battle::CostAdjustedAgilityHitChance: original _getAgilityDifference2 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:9992 (function lines 9992-10043). v1 -> variance_base_chance (variance base chance). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10029-10030. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:348-349. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved. v1 is the chance-minus-five base before adding NextAveraged(11) variance; name deliberately describes this intermediate, not a fully calculated random chance."),
        ("Battle::RiskHitChance",
         (('arg0', 'action_definition'), ('arg2', 'target_hit'), ('arg4', 'execute_action')),
         "F00347 F00347 Battle::RiskHitChance: original _getRiskModifier at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10045 (function lines 10045-10055). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10045. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:365. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10046. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:366. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10047. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:367. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ChainHitChance",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00348 F00348 Battle::ChainHitChance: original _getChainEvasionModifier at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10057 (function lines 10057-10073). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10057. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:378. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10058. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:379. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10059. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:380. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10059. Native references: Native\\Game\\src\\Battle\\HitChance.cpp:380. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ApplyDamageGemMultiplier",
         (('arg1', 'damage_amount'), ('arg2', 'source_stats'), ('arg3', 'target_stats'), ('var_a0', 'has_modifier_gem'), ('var_t0', 'damage_percent')),
         "F00394 F00394 Battle::ApplyDamageGemMultiplier: original func_8007FE5C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10534 (function lines 10534-10592). arg1 -> damage_amount (damage amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10535-10591. Native references: Native\\Game\\src\\Battle\\Damage.cpp:70-126. arg2 -> source_stats (source stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10535-10569. Native references: Native\\Game\\src\\Battle\\Damage.cpp:70-104. arg3 -> target_stats (target stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10535-10581. Native references: Native\\Game\\src\\Battle\\Damage.cpp:70-116. var_a0 -> has_modifier_gem (has modifier gem). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10537-10587. Native references: Native\\Game\\src\\Battle\\Damage.cpp:72-122. var_t0 -> damage_percent (damage percent). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10538-10591. Native references: Native\\Game\\src\\Battle\\Damage.cpp:73-126. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::DistributeLimbRecovery",
         (('temp_lo', 'limb_recovery_amount'),),
         "F00397 F00397 Battle::DistributeLimbRecovery: original func_800801E0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10664 (function lines 10664-10687). temp_lo -> limb_recovery_amount (limb recovery amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:10679-10683. Native references: Native\\Game\\src\\Battle\\Damage.cpp:198-202. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::QueueApplicableStatusChanges",
         (('arg0', 'status_id'), ('temp_a2', 'target_stats'), ('temp_t0', 'current_statuses')),
         "F00405 F00405 Battle::QueueApplicableStatusChanges: original _setApplicableStatuses at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11195 (function lines 11195-11215). arg0 -> status_id (status id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11195-11199. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:67-71. temp_a2 -> target_stats (target stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11200-11209. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:72-81. temp_t0 -> current_statuses (current statuses). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11204-11207. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:76-79. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::QueueStatusRemovalMask",
         (('arg0', 'status_id'),),
         "F00406 F00406 Battle::QueueStatusRemovalMask: original _setInflictedStatuses at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11217 (function lines 11217-11228). arg0 -> status_id (status id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11217-11220. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:90-93. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::IgnoreStatCalculator",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00407 F00407 Battle::IgnoreStatCalculator: original _statCalculatorNop at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11230 (function lines 11230-11235). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11230. Native references: Native\\Game\\src\\Battle\\Damage.cpp:209. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11231. Native references: Native\\Game\\src\\Battle\\Damage.cpp:210. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11232. Native references: Native\\Game\\src\\Battle\\Damage.cpp:211. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11232. Native references: Native\\Game\\src\\Battle\\Damage.cpp:211. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11233. Native references: Native\\Game\\src\\Battle\\Damage.cpp:212. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::LastActionStatValue",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00408 F00408 Battle::LastActionStatValue: original _getLastValue at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11237 (function lines 11237-11243). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11237. Native references: Native\\Game\\src\\Battle\\Damage.cpp:217. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11238. Native references: Native\\Game\\src\\Battle\\Damage.cpp:218. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11239. Native references: Native\\Game\\src\\Battle\\Damage.cpp:219. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11239. Native references: Native\\Game\\src\\Battle\\Damage.cpp:219. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11240. Native references: Native\\Game\\src\\Battle\\Damage.cpp:220. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::CalculatePhysicalHitDamage",
         (('sp18', 'calculation'),),
         "F00409 F00409 Battle::CalculatePhysicalHitDamage: original _getPhysicalAttackDamage at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11271 (function lines 11271-11465). sp18 -> calculation (calculation). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11274-11459. Native references: Native\\Game\\src\\Battle\\Damage.cpp:229-414. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::CalculateSpellHitDamage",
         (('sp18', 'calculation'),),
         "F00411 F00411 Battle::CalculateSpellHitDamage: original _getMagicAttackDamage at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11473 (function lines 11473-11638). sp18 -> calculation (calculation). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11476-11631. Native references: Native\\Game\\src\\Battle\\Damage.cpp:433-588. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScalePreviousTargetHit",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00412 F00412 Battle::ScalePreviousTargetHit: original _previousHitFactor0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11640 (function lines 11640-11648). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11640-11645. Native references: Native\\Game\\src\\Battle\\Damage.cpp:598-603. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11640. Native references: Native\\Game\\src\\Battle\\Damage.cpp:598. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11641. Native references: Native\\Game\\src\\Battle\\Damage.cpp:599. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11641-11645. Native references: Native\\Game\\src\\Battle\\Damage.cpp:599-603. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11642. Native references: Native\\Game\\src\\Battle\\Damage.cpp:600. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScaleSourceMissingHp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00413 F00413 Battle::ScaleSourceMissingHp: original _hpLostFactor at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11650 (function lines 11650-11657). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11650-11655. Native references: Native\\Game\\src\\Battle\\Damage.cpp:609-614. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11650-11654. Native references: Native\\Game\\src\\Battle\\Damage.cpp:609-613. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11651. Native references: Native\\Game\\src\\Battle\\Damage.cpp:610. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11651-11655. Native references: Native\\Game\\src\\Battle\\Damage.cpp:610-614. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11652. Native references: Native\\Game\\src\\Battle\\Damage.cpp:611. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScaleTargetCurrentHp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00415 F00415 Battle::ScaleTargetCurrentHp: original _factorCurrentHp at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11669 (function lines 11669-11676). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11669-11674. Native references: Native\\Game\\src\\Battle\\Damage.cpp:619-624. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11669. Native references: Native\\Game\\src\\Battle\\Damage.cpp:619. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11670-11673. Native references: Native\\Game\\src\\Battle\\Damage.cpp:620-623. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11670-11674. Native references: Native\\Game\\src\\Battle\\Damage.cpp:620-624. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11671. Native references: Native\\Game\\src\\Battle\\Damage.cpp:621. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::WeaponPpWithChainOffset",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00416 F00416 Battle::WeaponPpWithChainOffset: original _getWeaponPp at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11678 (function lines 11678-11684). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11678. Native references: Native\\Game\\src\\Battle\\Damage.cpp:629. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11678-11682. Native references: Native\\Game\\src\\Battle\\Damage.cpp:629-633. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11679. Native references: Native\\Game\\src\\Battle\\Damage.cpp:630. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11679. Native references: Native\\Game\\src\\Battle\\Damage.cpp:630. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11680. Native references: Native\\Game\\src\\Battle\\Damage.cpp:631. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScaleIncomingSpellDamage",
         (('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('var_a2', 'scaled_amount')),
         "F00417 F00417 Battle::ScaleIncomingSpellDamage: original _factorMagicDamageReceived at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11686 (function lines 11686-11701). arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11687. Native references: Native\\Game\\src\\Battle\\Damage.cpp:639. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11688. Native references: Native\\Game\\src\\Battle\\Damage.cpp:640. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11688-11696. Native references: Native\\Game\\src\\Battle\\Damage.cpp:640-648. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11689. Native references: Native\\Game\\src\\Battle\\Damage.cpp:641. var_a2 -> scaled_amount (scaled amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11691-11700. Native references: Native\\Game\\src\\Battle\\Damage.cpp:643-652. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScaleIncomingPhysicalDamage",
         (('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('var_a2', 'scaled_amount')),
         "F00418 F00418 Battle::ScaleIncomingPhysicalDamage: original _factorPhysicalDamageReceived0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11703 (function lines 11703-11718). arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11704. Native references: Native\\Game\\src\\Battle\\Damage.cpp:657. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11705. Native references: Native\\Game\\src\\Battle\\Damage.cpp:658. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11705-11713. Native references: Native\\Game\\src\\Battle\\Damage.cpp:658-666. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11706. Native references: Native\\Game\\src\\Battle\\Damage.cpp:659. var_a2 -> scaled_amount (scaled amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11708-11717. Native references: Native\\Game\\src\\Battle\\Damage.cpp:661-670. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScaleSourceMissingMp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00419 F00419 Battle::ScaleSourceMissingMp: original _mpLostFactor at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11720 (function lines 11720-11726). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11721-11724. Native references: Native\\Game\\src\\Battle\\Damage.cpp:675-678. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11721-11723. Native references: Native\\Game\\src\\Battle\\Damage.cpp:675-677. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11721. Native references: Native\\Game\\src\\Battle\\Damage.cpp:675. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11721-11724. Native references: Native\\Game\\src\\Battle\\Damage.cpp:675-678. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11721. Native references: Native\\Game\\src\\Battle\\Damage.cpp:675. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::CalculateSpellHealing",
         (('sp18', 'calculation'),),
         "F00421 F00421 Battle::CalculateSpellHealing: original _hpHealed at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11850 (function lines 11850-11990). sp18 -> calculation (calculation). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:11853-11984. Native references: Native\\Game\\src\\Battle\\Damage.cpp:686-817. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::TargetMissingHp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00424 F00424 Battle::TargetMissingHp: original _getRecoverableHp at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12130 (function lines 12130-12136). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12130. Native references: Native\\Game\\src\\Battle\\Damage.cpp:826. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12131. Native references: Native\\Game\\src\\Battle\\Damage.cpp:827. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12131-12135. Native references: Native\\Game\\src\\Battle\\Damage.cpp:827-831. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12132. Native references: Native\\Game\\src\\Battle\\Damage.cpp:828. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12132. Native references: Native\\Game\\src\\Battle\\Damage.cpp:828. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::TargetMissingMp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00425 F00425 Battle::TargetMissingMp: original _getRecoverableMp at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12138 (function lines 12138-12144). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12138. Native references: Native\\Game\\src\\Battle\\Damage.cpp:835. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12139. Native references: Native\\Game\\src\\Battle\\Damage.cpp:836. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12139-12143. Native references: Native\\Game\\src\\Battle\\Damage.cpp:836-840. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12140. Native references: Native\\Game\\src\\Battle\\Damage.cpp:837. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12140. Native references: Native\\Game\\src\\Battle\\Damage.cpp:837. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScaleIncomingSpellEffect",
         (('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('var_a2', 'scaled_amount')),
         "F00427 F00427 Battle::ScaleIncomingSpellEffect: original _factorMagicDamageReceivedOfType at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12156 (function lines 12156-12172). arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12157. Native references: Native\\Game\\src\\Battle\\Damage.cpp:845. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12158. Native references: Native\\Game\\src\\Battle\\Damage.cpp:846. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12158-12167. Native references: Native\\Game\\src\\Battle\\Damage.cpp:846-855. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12159. Native references: Native\\Game\\src\\Battle\\Damage.cpp:847. var_a2 -> scaled_amount (scaled amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12161-12171. Native references: Native\\Game\\src\\Battle\\Damage.cpp:849-859. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScaleIncomingSpellCost",
         (('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('var_v0', 'scaled_spell_cost')),
         "F00429 F00429 Battle::ScaleIncomingSpellCost: original _factorSpellCost at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12191 (function lines 12191-12206). arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12191. Native references: Native\\Game\\src\\Battle\\Damage.cpp:863. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12192. Native references: Native\\Game\\src\\Battle\\Damage.cpp:864. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12192-12199. Native references: Native\\Game\\src\\Battle\\Damage.cpp:864-871. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12193. Native references: Native\\Game\\src\\Battle\\Damage.cpp:865. var_v0 -> scaled_spell_cost (scaled spell cost). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12195-12205. Native references: Native\\Game\\src\\Battle\\Damage.cpp:867-877. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScaleIncomingDamage",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('var_a2', 'scaled_amount')),
         "F00430 F00430 Battle::ScaleIncomingDamage: original _factorDamageReceived at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12208 (function lines 12208-12219). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12208-12215. Native references: Native\\Game\\src\\Battle\\Damage.cpp:881-888. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12208. Native references: Native\\Game\\src\\Battle\\Damage.cpp:881. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12209. Native references: Native\\Game\\src\\Battle\\Damage.cpp:882. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12209-12215. Native references: Native\\Game\\src\\Battle\\Damage.cpp:882-888. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12210. Native references: Native\\Game\\src\\Battle\\Damage.cpp:883. var_a2 -> scaled_amount (scaled amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12212-12218. Native references: Native\\Game\\src\\Battle\\Damage.cpp:885-891. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::SourceShieldPp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00431 F00431 Battle::SourceShieldPp: original _getEquippedShieldDp at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12221 (function lines 12221-12226). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12221. Native references: Native\\Game\\src\\Battle\\Damage.cpp:895. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12221-12225. Native references: Native\\Game\\src\\Battle\\Damage.cpp:895-899. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12222. Native references: Native\\Game\\src\\Battle\\Damage.cpp:896. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12222. Native references: Native\\Game\\src\\Battle\\Damage.cpp:896. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12223. Native references: Native\\Game\\src\\Battle\\Damage.cpp:897. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::ScaleLastActionStatValue",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00432 F00432 Battle::ScaleLastActionStatValue: original _factorLastValue at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12228 (function lines 12228-12233). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12228-12232. Native references: Native\\Game\\src\\Battle\\Damage.cpp:903-907. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12228. Native references: Native\\Game\\src\\Battle\\Damage.cpp:903. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12229. Native references: Native\\Game\\src\\Battle\\Damage.cpp:904. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12229-12232. Native references: Native\\Game\\src\\Battle\\Damage.cpp:904-907. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12230. Native references: Native\\Game\\src\\Battle\\Damage.cpp:905. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::PhysicalDamageWithDebuffs",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00433 F00433 Battle::PhysicalDamageWithDebuffs: original _getPhysicalAttackDamageWithStatusModifiers at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12235 (function lines 12235-12239). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12236-12238. Native references: Native\\Game\\src\\Battle\\Damage.cpp:912-914. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12236-12238. Native references: Native\\Game\\src\\Battle\\Damage.cpp:912-914. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12236-12238. Native references: Native\\Game\\src\\Battle\\Damage.cpp:912-914. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12236-12238. Native references: Native\\Game\\src\\Battle\\Damage.cpp:912-914. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12236-12238. Native references: Native\\Game\\src\\Battle\\Damage.cpp:912-914. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::TargetRisk",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00434 F00434 Battle::TargetRisk: original _getRisk at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12241 (function lines 12241-12246). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12241. Native references: Native\\Game\\src\\Battle\\Damage.cpp:918. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12242. Native references: Native\\Game\\src\\Battle\\Damage.cpp:919. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12242-12245. Native references: Native\\Game\\src\\Battle\\Damage.cpp:919-922. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12243. Native references: Native\\Game\\src\\Battle\\Damage.cpp:920. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12243. Native references: Native\\Game\\src\\Battle\\Damage.cpp:920. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::CalculateActionStatChange",
         (('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg4', 'execute_action')),
         "F00435 F00435 Battle::CalculateActionStatChange: original _calculateStatChange at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12248 (function lines 12248-12263). arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12249-12258. Native references: Native\\Game\\src\\Battle\\Damage.cpp:927-936. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12249-12258. Native references: Native\\Game\\src\\Battle\\Damage.cpp:927-936. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12249-12258. Native references: Native\\Game\\src\\Battle\\Damage.cpp:927-936. Review context: Original individual callback body and dispatch reviewed. CalculateActionStatChange and RollActionHit pass definition/source/target/hit index/execution flag; that flag enables variance and execution-only rating updates. Formula locals hold the actual damage/healing calculation record, scaled amount, or chance-minus-five prior to adding uniform variance. Existing types, signedness and unused interface slots preserved."),
        ("Battle::QueueStrengthDown",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00437 F00437 Battle::QueueStrengthDown: original func_80083998 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12273 (function lines 12273-12280). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12273. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:104. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12274. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:105. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12274-12278. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:105-109. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12275. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:106. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12275. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:106. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueIntelligenceDown",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00438 F00438 Battle::QueueIntelligenceDown: original func_800839E8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12282 (function lines 12282-12289). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12282. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:114. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12283. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:115. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12283-12287. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:115-119. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12284. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:116. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12284. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:116. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueAgilityDown",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00439 F00439 Battle::QueueAgilityDown: original func_80083A38 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12291 (function lines 12291-12298). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12291. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:124. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12292. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:125. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12292-12296. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:125-129. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12293. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:126. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12293. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:126. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueEquipmentDown",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00440 F00440 Battle::QueueEquipmentDown: original func_80083A88 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12300 (function lines 12300-12307). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12300. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:134. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12301. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:135. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12301-12305. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:135-139. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12302. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:136. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12302. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:136. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueHalfMovement",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00441 F00441 Battle::QueueHalfMovement: original func_80083AD8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12309 (function lines 12309-12316). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12309. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:144. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12310. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:145. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12310-12314. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:145-149. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12311. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:146. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12311. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:146. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueStrengthUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00442 F00442 Battle::QueueStrengthUp: original func_80083B28 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12318 (function lines 12318-12325). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12318. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:154. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12319. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:155. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12319-12323. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:155-159. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12320. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:156. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12320. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:156. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueIntelligenceUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00443 F00443 Battle::QueueIntelligenceUp: original func_80083B78 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12327 (function lines 12327-12334). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12327. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:164. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12328. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:165. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12328-12332. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:165-169. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12329. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:166. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12329. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:166. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueAgilityUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00444 F00444 Battle::QueueAgilityUp: original func_80083BC8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12336 (function lines 12336-12343). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12336. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:174. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12337. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:175. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12337-12341. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:175-179. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12338. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:176. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12338. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:176. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueEquipmentUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00445 F00445 Battle::QueueEquipmentUp: original func_80083C18 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12345 (function lines 12345-12352). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12345. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:184. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12346. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:185. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12346-12350. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:185-189. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12347. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:186. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12347. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:186. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueQuicken",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00446 F00446 Battle::QueueQuicken: original func_80083C68 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12354 (function lines 12354-12361). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12354. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:194. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12355. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:195. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12355-12359. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:195-199. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12356. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:196. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12356. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:196. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueSilence",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00447 F00447 Battle::QueueSilence: original func_80083CB8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12363 (function lines 12363-12370). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12363. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:204. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12364. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:205. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12364-12368. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:205-209. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12365. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:206. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12365. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:206. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueMagicWard",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00448 F00448 Battle::QueueMagicWard: original func_80083D08 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12372 (function lines 12372-12378). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12372. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:214. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12373. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:215. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12374-12377. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:216-219. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12374. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:216. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12375. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:217. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueRegeneration",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00449 F00449 Battle::QueueRegeneration: original func_80083D2C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12380 (function lines 12380-12387). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12380. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:223. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12381. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:224. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12381-12385. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:224-228. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12382. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:225. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12382. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:225. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueBanish",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00450 F00450 Battle::QueueBanish: original func_80083D7C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12389 (function lines 12389-12397). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12389. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:233. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12390. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:234. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12390-12395. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:234-239. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12391. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:235. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12391-12394. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:235-238. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueParalysis",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00451 F00451 Battle::QueueParalysis: original func_80083DEC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12399 (function lines 12399-12406). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12399. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:244. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12400. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:245. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12400-12404. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:245-249. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12401. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:246. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12401. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:246. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueuePoison",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00452 F00452 Battle::QueuePoison: original func_80083E3C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12408 (function lines 12408-12415). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12408. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:254. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12409. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:255. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12409-12413. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:255-259. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12410. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:256. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12410. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:256. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueNumbness",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00453 F00453 Battle::QueueNumbness: original func_80083E8C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12417 (function lines 12417-12424). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12417. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:264. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12418. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:265. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12418-12422. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:265-269. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12419. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:266. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12419. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:266. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueCurse",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00454 F00454 Battle::QueueCurse: original func_80083EDC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12426 (function lines 12426-12433). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12426. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:274. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12427. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:275. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12427-12431. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:275-279. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12428. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:276. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12428. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:276. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueAirAttackUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00455 F00455 Battle::QueueAirAttackUp: original func_80083F2C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12435 (function lines 12435-12440). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12435. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:284. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12436. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:285. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12436-12439. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:285-288. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12437. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:286. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12437. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:286. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueFireAttackUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00456 F00456 Battle::QueueFireAttackUp: original func_80083F50 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12442 (function lines 12442-12447). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12442. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:292. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12443. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:293. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12443-12446. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:293-296. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12444. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:294. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12444. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:294. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueEarthAttackUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00457 F00457 Battle::QueueEarthAttackUp: original func_80083F74 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12449 (function lines 12449-12454). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12449. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:300. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12450. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:301. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12450-12453. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:301-304. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12451. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:302. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12451. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:302. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueWaterAttackUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00458 F00458 Battle::QueueWaterAttackUp: original func_80083F98 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12456 (function lines 12456-12461). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12456. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:308. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12457. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:309. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12457-12460. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:309-312. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12458. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:310. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12458. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:310. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueAirDefenseUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00459 F00459 Battle::QueueAirDefenseUp: original func_80083FBC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12463 (function lines 12463-12468). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12463. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:316. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12464. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:317. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12464-12467. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:317-320. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12465. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:318. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12465. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:318. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueFireDefenseUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00460 F00460 Battle::QueueFireDefenseUp: original func_80083FE0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12470 (function lines 12470-12475). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12470. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:324. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12471. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:325. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12471-12474. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:325-328. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12472. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:326. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12472. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:326. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueEarthDefenseUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00461 F00461 Battle::QueueEarthDefenseUp: original func_80084004 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12477 (function lines 12477-12482). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12477. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:332. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12478. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:333. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12478-12481. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:333-336. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12479. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:334. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12479. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:334. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueWaterDefenseUp",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00462 F00462 Battle::QueueWaterDefenseUp: original func_80084028 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12484 (function lines 12484-12489). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12484. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:340. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12485. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:341. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12485-12488. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:341-344. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12486. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:342. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12486. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:342. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueParalysisCure",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00463 F00463 Battle::QueueParalysisCure: original func_8008404C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12491 (function lines 12491-12498). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12491. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:348. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12492. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:349. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12492-12496. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:349-353. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12493. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:350. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12493. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:350. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueuePoisonCure",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00464 F00464 Battle::QueuePoisonCure: original func_8008409C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12500 (function lines 12500-12507). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12500. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:358. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12501. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:359. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12501-12505. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:359-363. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12502. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:360. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12502. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:360. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueCurseCure",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00465 F00465 Battle::QueueCurseCure: original func_800840EC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12509 (function lines 12509-12516). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12509. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:368. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12510. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:369. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12510-12514. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:369-373. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12511. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:370. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12511. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:370. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueNumbnessCure",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00466 F00466 Battle::QueueNumbnessCure: original func_8008413C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12518 (function lines 12518-12525). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12518. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:378. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12519. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:379. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12519-12523. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:379-383. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12520. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:380. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12520. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:380. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueDispel",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00467 F00467 Battle::QueueDispel: original func_8008418C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12527 (function lines 12527-12533). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12527. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:388. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12528. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:389. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12528-12532. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:389-393. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12529. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:390. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12529. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:390. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::QueueNegativeStatusCure",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('temp_v1', 'negative_status_mask')),
         "F00468 F00468 Battle::QueueNegativeStatusCure: original func_800841C8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12535 (function lines 12535-12545). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12535. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:397. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12536. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:398. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12536-12543. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:398-405. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12537. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:399. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12537. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:399. temp_v1 -> negative_status_mask (negative status mask). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12539-12542. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:401-404. Review context: Individually read original mediator body including each distinct status ID/mask, application/cure helper, effect polarity and Banish execution-only isAnalyzed write. All five callback argument roles verified from ApplyActionHitEffect mediator invocation; unused parameters preserve positional ABI."),
        ("Battle::MarkSourceActionSuccess",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00469 F00469 Battle::MarkSourceActionSuccess: original func_80084228 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12547 (function lines 12547-12552). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12547. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:27. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12547-12551. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:27-31. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12548. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:28. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12548. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:28. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12549. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:29. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueInstantDeath",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00470 F00470 Battle::QueueInstantDeath: original func_80084248 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12554 (function lines 12554-12561). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12554. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:35. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12555. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:36. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12555-12560. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:36-41. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12556. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:37. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12556. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:37. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueDamageBasedArmorWear",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00471 F00471 Battle::QueueDamageBasedArmorWear: original func_800842AC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12563 (function lines 12563-12570). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12563. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:45. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12564. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:46. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12564-12569. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:46-51. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12565. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:47. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12565. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:47. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::SetActionFlag64",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00473 F00473 Battle::SetActionFlag64: original func_80084310 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12579 (function lines 12579-12587). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12579. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:55. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12580. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:56. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12581. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:57. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12581. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:57. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12582-12584. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:58-60. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::SetActionFlag32",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00474 F00474 Battle::SetActionFlag32: original func_80084340 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12589 (function lines 12589-12597). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12589. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:66. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12590. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:67. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12591. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:68. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12591. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:68. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12592-12594. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:69-71. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueSpellTriggeredRecovery",
         (('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00476 F00476 Battle::QueueSpellTriggeredRecovery: original func_80084390 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12606 (function lines 12606-12615). arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12607-12610. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:78-81. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12607-12613. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:78-84. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12607-12610. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:78-81. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12607-12610. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:78-81. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueuePhysicalTriggeredRecovery",
         (('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00477 F00477 Battle::QueuePhysicalTriggeredRecovery: original func_80084440 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12617 (function lines 12617-12626). arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12618-12621. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:90-93. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12618-12624. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:90-96. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12618-12621. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:90-93. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12618-12621. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:90-93. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueuePhysicalAffinityRecovery",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00478 F00478 Battle::QueuePhysicalAffinityRecovery: original func_800844F0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12628 (function lines 12628-12637). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12629-12635. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:102-108. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12629-12632. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:102-105. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12629-12635. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:102-108. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12629-12632. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:102-105. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12629-12632. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:102-105. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueAirAffinityRecovery",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00479 F00479 Battle::QueueAirAffinityRecovery: original func_80084570 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12639 (function lines 12639-12648). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12640-12646. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:114-120. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12640-12643. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:114-117. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12640-12646. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:114-120. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12640-12643. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:114-117. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12640-12643. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:114-117. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueFireAffinityRecovery",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00480 F00480 Battle::QueueFireAffinityRecovery: original func_800845F0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12650 (function lines 12650-12659). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12651-12657. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:126-132. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12651-12654. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:126-129. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12651-12657. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:126-132. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12651-12654. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:126-129. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12651-12654. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:126-129. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueEarthAffinityRecovery",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00481 F00481 Battle::QueueEarthAffinityRecovery: original func_80084670 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12661 (function lines 12661-12670). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12662-12668. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:138-144. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12662-12665. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:138-141. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12662-12668. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:138-144. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12662-12665. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:138-141. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12662-12665. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:138-141. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueWaterAffinityRecovery",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00482 F00482 Battle::QueueWaterAffinityRecovery: original func_800846F0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12672 (function lines 12672-12681). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12673-12679. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:150-156. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12673-12676. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:150-153. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12673-12679. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:150-156. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12673-12676. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:150-153. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12673-12676. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:150-153. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueLightAffinityRecovery",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00483 F00483 Battle::QueueLightAffinityRecovery: original func_80084770 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12683 (function lines 12683-12692). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12684-12690. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:162-168. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12684-12687. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:162-165. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12684-12690. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:162-168. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12684-12687. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:162-165. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12684-12687. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:162-165. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueDarkAffinityRecovery",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00484 F00484 Battle::QueueDarkAffinityRecovery: original func_800847F0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12694 (function lines 12694-12703). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12695-12701. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:174-180. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12695-12698. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:174-177. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12695-12701. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:174-180. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12695-12698. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:174-177. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12695-12698. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:174-177. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueDamageTriggeredRecovery",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('temp_v1', 'incoming_hp_effect')),
         "F00485 F00485 Battle::QueueDamageTriggeredRecovery: original func_80084870 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12705 (function lines 12705-12715). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12706-12713. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:186-193. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12706-12710. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:186-190. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12706-12713. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:186-193. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12706-12710. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:186-190. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12706-12710. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:186-190. temp_v1 -> incoming_hp_effect (incoming hp effect). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12708-12709. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:188-189. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueRandomStrengthIncrease",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00486 F00486 Battle::QueueRandomStrengthIncrease: original _increaseStrengthRand at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12717 (function lines 12717-12724). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12717. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:198. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12718. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:199. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12718-12723. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:199-204. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12719. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:200. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12719. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:200. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueRandomIntelligenceIncrease",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00487 F00487 Battle::QueueRandomIntelligenceIncrease: original _increaseIntelligenceRand at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12726 (function lines 12726-12733). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12726. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:208. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12727. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:209. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12727-12732. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:209-214. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12728. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:210. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12728. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:210. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueRandomAgilityIncrease",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00488 F00488 Battle::QueueRandomAgilityIncrease: original _increaseAgilityRand at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12735 (function lines 12735-12742). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12735. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:218. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12736. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:219. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12736-12741. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:219-224. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12737. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:220. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12737. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:220. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueRandomMaximumHpIncrease",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00489 F00489 Battle::QueueRandomMaximumHpIncrease: original _increaseMaxHpRand at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12744 (function lines 12744-12751). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12744. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:228. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12745. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:229. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12745-12750. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:229-234. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12746. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:230. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12746. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:230. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueRandomMaximumMpIncrease",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00490 F00490 Battle::QueueRandomMaximumMpIncrease: original _increaseMaxMpRand at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12753 (function lines 12753-12760). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12753. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:238. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12754. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:239. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12754-12759. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:239-244. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12755. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:240. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12755. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:240. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueRandomStrengthDecrease",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00491 F00491 Battle::QueueRandomStrengthDecrease: original _decreaseStrengthRand at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12762 (function lines 12762-12769). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12762. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:248. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12763. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:249. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12763-12768. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:249-254. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12764. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:250. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12764. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:250. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueRandomIntelligenceDecrease",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00492 F00492 Battle::QueueRandomIntelligenceDecrease: original _decreaseIntelligenceRand at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12771 (function lines 12771-12778). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12771. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:258. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12772. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:259. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12772-12777. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:259-264. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12773. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:260. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12773. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:260. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueRandomAgilityDecrease",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00493 F00493 Battle::QueueRandomAgilityDecrease: original _decreaseAglityRand at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12780 (function lines 12780-12787). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12780. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:268. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12781. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:269. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12781-12786. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:269-274. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12782. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:270. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12782. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:270. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::QueueHitDamageAndEquipmentWear",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('temp_v0', 'damage_amount'), ('temp_s4', 'source_stats'), ('temp_s2', 'target_stats')),
         "F00494 F00494 Battle::QueueHitDamageAndEquipmentWear: original func_80084B70 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12789 (function lines 12789-12818). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12790-12815. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:279-304. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12790-12796. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:279-285. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12790-12810. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:279-299. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12790-12796. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:279-285. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12790-12813. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:279-302. temp_s2 -> target_stats (target stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12794-12815. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:283-304. temp_s4 -> source_stats (source stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12793-12815. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:282-304. temp_v0 -> damage_amount (damage amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12792-12815. Native references: Native\\Game\\src\\Battle\\ActionEffects.cpp:281-304. Review context: Each callback body reviewed individually: sourceResult writes affect source_hit; target hp/stat/effect writes affect target_hit; affinity/recovery predicates forward the same five arguments. ApplyActionHitEffect (146C.c:13012) invokes effectId mediator(action,source,target,hit,execution mode); ResolveActionTargets supplies 0 for preview and 1 for execution. Unused callback formals retain their positional ABI, now named by that verified contract."),
        ("Battle::AccumulateMpDamage",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('temp_s0', 'target_stats')),
         "F00495 F00495 Battle::AccumulateMpDamage: original func_80084CAC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12820 (function lines 12820-12833). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12821-12824. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:48-51. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12821-12824. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:48-51. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12821-12832. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:48-59. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12821-12824. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:48-51. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12821-12824. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:48-51. temp_s0 -> target_stats (target stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12823-12828. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:50-55. Review context: Individually reviewed accumulator/no-op body. Five mediator arguments come from ApplyActionHitEffect; CalculateActionStatChange produces signed short amount, applied to this callback specific target stat. Risk reduction tests its original shifted amount without changing signedness or width."),
        ("Battle::AccumulateWeaponPpDrain",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00496 F00496 Battle::AccumulateWeaponPpDrain: original func_80084D44 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12835 (function lines 12835-12845). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12835. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:63. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12836. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:64. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12836-12843. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:64-71. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12837. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:65. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12837. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:65. Review context: Individually reviewed accumulator/no-op body. Five mediator arguments come from ApplyActionHitEffect; CalculateActionStatChange produces signed short amount, applied to this callback specific target stat. Risk reduction tests its original shifted amount without changing signedness or width."),
        ("Battle::AccumulateShieldPpDrain",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00497 F00497 Battle::AccumulateShieldPpDrain: original func_80084DA8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12847 (function lines 12847-12860). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12847. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:76. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12848. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:77. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12848-12857. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:77-86. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12849. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:78. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12849. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:78. Review context: Individually reviewed accumulator/no-op body. Five mediator arguments come from ApplyActionHitEffect; CalculateActionStatChange produces signed short amount, applied to this callback specific target stat. Risk reduction tests its original shifted amount without changing signedness or width."),
        ("Battle::AccumulateHpRestoration",
         (('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00498 F00498 Battle::AccumulateHpRestoration: original func_80084E28 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12862 (function lines 12862-12872). arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12863-12865. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:93-95. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12863-12870. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:93-100. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12863-12865. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:93-95. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12863-12865. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:93-95. Review context: Individually reviewed accumulator/no-op body. Five mediator arguments come from ApplyActionHitEffect; CalculateActionStatChange produces signed short amount, applied to this callback specific target stat. Risk reduction tests its original shifted amount without changing signedness or width."),
        ("Battle::AccumulateMpRestoration",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('temp_v0', 'restoration_amount')),
         "F00499 F00499 Battle::AccumulateMpRestoration: original func_80084EA0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12874 (function lines 12874-12880). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12875-12877. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:106-108. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12875-12877. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:106-108. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12875-12879. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:106-110. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12875-12877. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:106-108. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12875-12877. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:106-108. temp_v0 -> restoration_amount (restoration amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12877-12878. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:108-109. Review context: Individually reviewed accumulator/no-op body. Five mediator arguments come from ApplyActionHitEffect; CalculateActionStatChange produces signed short amount, applied to this callback specific target stat. Risk reduction tests its original shifted amount without changing signedness or width."),
        ("Battle::AccumulateWeaponPpRestoration",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('temp_v0', 'restoration_amount')),
         "F00500 F00500 Battle::AccumulateWeaponPpRestoration: original func_80084EEC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12882 (function lines 12882-12888). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12883-12885. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:115-117. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12883-12885. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:115-117. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12883-12887. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:115-119. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12883-12885. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:115-117. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12883-12885. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:115-117. temp_v0 -> restoration_amount (restoration amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12885-12886. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:117-118. Review context: Individually reviewed accumulator/no-op body. Five mediator arguments come from ApplyActionHitEffect; CalculateActionStatChange produces signed short amount, applied to this callback specific target stat. Risk reduction tests its original shifted amount without changing signedness or width."),
        ("Battle::AccumulateWeaponDpRestoration",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('temp_v0', 'restoration_amount')),
         "F00501 F00501 Battle::AccumulateWeaponDpRestoration: original func_80084F40 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12890 (function lines 12890-12896). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12891-12893. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:124-126. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12891-12893. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:124-126. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12891-12895. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:124-128. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12891-12893. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:124-126. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12891-12893. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:124-126. temp_v0 -> restoration_amount (restoration amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12893-12894. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:126-127. Review context: Individually reviewed accumulator/no-op body. Five mediator arguments come from ApplyActionHitEffect; CalculateActionStatChange produces signed short amount, applied to this callback specific target stat. Risk reduction tests its original shifted amount without changing signedness or width."),
        ("Battle::AccumulateRiskReduction",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action'), ('temp_v0', 'restoration_amount'), ('var_v0', 'shifted_restoration_amount')),
         "F00502 F00502 Battle::AccumulateRiskReduction: original func_80084F94 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12898 (function lines 12898-12907). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12899-12901. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:133-135. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12899-12901. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:133-135. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12899-12905. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:133-139. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12899-12901. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:133-135. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12899-12901. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:133-135. temp_v0 -> restoration_amount (restoration amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12901-12904. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:135-138. var_v0 -> shifted_restoration_amount (shifted restoration amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12902-12903. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:136-137. Review context: Individually reviewed accumulator/no-op body. Five mediator arguments come from ApplyActionHitEffect; CalculateActionStatChange produces signed short amount, applied to this callback specific target stat. Risk reduction tests its original shifted amount without changing signedness or width."),
        ("Battle::IgnoreActionEffect",
         (('arg0', 'action_definition'), ('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'hit_index'), ('arg4', 'execute_action')),
         "F00503 F00503 Battle::IgnoreActionEffect: original func_80084FF0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12909 (function lines 12909-12914). arg0 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12909. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:144. arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12910. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:145. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12911. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:146. arg3 -> hit_index (hit index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12911. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:146. arg4 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12912. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:147. Review context: Individually reviewed accumulator/no-op body. Five mediator arguments come from ApplyActionHitEffect; CalculateActionStatChange produces signed short amount, applied to this callback specific target stat. Risk reduction tests its original shifted amount without changing signedness or width."),
        ("Battle::DeriveLimbInjuryStatuses",
         (('arg0', 'hit_result'), ('temp_s3', 'target_stats'), ('var_a2', 'projected_limb_hp'), ('var_a1', 'projected_secondary_mobility_hp'), ('var_v1', 'limb_health_test')),
         "F00506 F00506 Battle::DeriveLimbInjuryStatuses: original func_80085008 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12930 (function lines 12930-13010). arg0 -> hit_result (hit result). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12930-13005. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:152-227. temp_s3 -> target_stats (target stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12936-12996. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:158-218. var_a1 -> projected_secondary_mobility_hp (projected secondary mobility hp). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12934-12974. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:156-196. var_a2 -> projected_limb_hp (projected limb hp). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12935-13003. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:157-225. var_v1 -> limb_health_test (limb health test). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:12933-13004. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:155-226. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::ApplyActionHitEffect",
         (('arg1', 'source_hit'), ('arg2', 'target_hit'), ('arg3', 'execute_action'), ('temp_s0', 'weapon_category')),
         "F00507 F00507 Battle::ApplyActionHitEffect: original func_80085390 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13012 (function lines 13012-13056). arg1 -> source_hit (source hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13013-13037. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:236-260. arg2 -> target_hit (target hit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13013-13046. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:236-269. arg3 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13013-13037. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:236-260. temp_s0 -> weapon_category (weapon category). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13045-13053. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:268-276. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::ClearActionResult",
         (('arg0', 'action_result'),),
         "F00508 F00508 Battle::ClearActionResult: original func_800856F8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13058 (function lines 13058-13058). arg0 -> action_result (action result). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13058. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:282. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::InitializeHitResult",
         (('arg0', 'hit_result'),),
         "F00509 F00509 Battle::InitializeHitResult: original func_80085718 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13060 (function lines 13060-13065). arg0 -> hit_result (hit result). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13060-13064. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:285-289. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::PreviewActorActionCost",
         (('arg0', 'action_id'), ('arg1', 'actor_slot')),
         "F00511 F00511 Battle::PreviewActorActionCost: original func_80085978 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13167 (function lines 13167-13170). arg0 -> action_id (action id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13167-13169. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:386-388. arg1 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13167-13169. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:386-388. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::AddActionRecoveryDelay",
         (('arg0', 'action_id'), ('arg1', 'actor_stats'), ('arg2', 'execute_action'), ('new_var', 'action_definition'), ('new_var2', 'recovery_delay')),
         "F00512 F00512 Battle::AddActionRecoveryDelay: original func_800859B4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13172 (function lines 13172-13184). arg0 -> action_id (action id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13172-13176. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:392-396. arg1 -> actor_stats (actor stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13172-13181. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:392-401. arg2 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13172-13176. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:392-396. new_var -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13174-13180. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:394-400. new_var2 -> recovery_delay (recovery delay). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13180-13181. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:400-401. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::FinalizeHitOutcome",
         (('arg0', 'hit_status'), ('var_a1', 'remaining_effect_flags')),
         "F00513 F00513 Battle::FinalizeHitOutcome: original func_80085A34 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13186 (function lines 13186-13217). arg0 -> hit_status (hit status). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13186-13213. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:407-434. var_a1 -> remaining_effect_flags (remaining effect flags). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13189-13199. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:410-420. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::ResolveActionTargets",
         (('arg3', 'execute_action'), ('spec', 'action_definition')),
         "F00514 F00514 Battle::ResolveActionTargets: original func_80085B10 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13221 (function lines 13221-13419). arg3 -> execute_action (execute action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13221-13415. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:441-635. spec -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13226-13380. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:446-600. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::PreviewActionAgainstLimb",
         (('arg0', 'action_id'), ('arg1', 'source_actor_slot'), ('arg4', 'result_out'), ('sp10', 'preview_request')),
         "F00515 F00515 Battle::PreviewActionAgainstLimb: original func_8008631C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13421 (function lines 13421-13434). arg0 -> action_id (action id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13421-13433. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:642-654. arg1 -> source_actor_slot (source actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13421-13433. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:642-654. arg4 -> result_out (result out). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13421-13432. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:642-653. sp10 -> preview_request (preview request). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13423-13432. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:644-653. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::PreviewActionAgainstActor",
         (('arg0', 'action_id'), ('arg1', 'source_actor_slot'), ('arg3', 'target_limb'), ('arg4', 'source_position'), ('arg5', 'target_position'), ('arg6', 'result_out'), ('sp10', 'preview_request'), ('sp860', 'target_volume'), ('sp870', 'limb_position'), ('var_s2', 'target_count'), ('temp_s0', 'delta_z'), ('temp_s0_2', 'delta_y'), ('temp_s1', 'delta_x'), ('temp_v0', 'action_category')),
         "F00516 F00516 Battle::PreviewActionAgainstActor: original func_800863A4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13436 (function lines 13436-13530). arg0 -> action_id (action id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13436-13529. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:658-751. arg1 -> source_actor_slot (source actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13436-13529. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:658-751. arg3 -> target_limb (target limb). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13436-13513. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:658-735. arg4 -> source_position (source position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13436-13471. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:658-693. arg5 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13437-13485. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:659-707. arg6 -> result_out (result out). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13437-13528. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:659-750. sp10 -> preview_request (preview request). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13439-13528. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:661-750. sp860 -> target_volume (target volume). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13440-13498. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:662-720. sp870 -> limb_position (limb position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13441-13498. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:663-720. temp_s0 -> delta_z (delta z). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13443-13468. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:665-690. temp_s0_2 -> delta_y (delta y). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13444-13468. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:666-690. temp_s1 -> delta_x (delta x). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13445-13468. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:667-690. temp_v0 -> action_category (action category). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13446-13455. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:668-677. var_s2 -> target_count (target count). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13442-13506. Native references: Native\\Game\\src\\Battle\\ActionResolution.cpp:664-728. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
        ("Battle::RemoveActorStatuses",
         (('arg0', 'status_mask'), ('arg1', 'actor_stats'), ('temp_v1_2', 'element_status_mask')),
         "F00517 F00517 Battle::RemoveActorStatuses: original func_80086754 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13532 (function lines 13532-13799). arg0 -> status_mask (status mask). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13532-13705. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:410-583. arg1 -> actor_stats (actor stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13532-13793. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:410-671. temp_v1_2 -> element_status_mask (element status mask). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13537-13709. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:415-587. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::ApplyWeaponElementEnchantment",
         (('arg0', 'actor_stats'), ('arg1', 'increased_affinity'), ('arg2', 'decreased_affinity'), ('arg3', 'modifier_amount')),
         "F00518 F00518 Battle::ApplyWeaponElementEnchantment: original func_80086EA4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13801 (function lines 13801-13809). arg0 -> actor_stats (actor stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13801-13807. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:680-686. arg1 -> increased_affinity (increased affinity). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13801-13806. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:680-685. arg2 -> decreased_affinity (decreased affinity). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13801-13807. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:680-686. arg3 -> modifier_amount (modifier amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13801-13807. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:680-686. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::ApplyArmorElementEnchantment",
         (('arg0', 'actor_stats'), ('arg1', 'increased_affinity'), ('arg2', 'decreased_affinity'), ('arg3', 'modifier_amount')),
         "F00519 F00519 Battle::ApplyArmorElementEnchantment: original func_80086EF4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13811 (function lines 13811-13830). arg0 -> actor_stats (actor stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13811-13827. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:691-707. arg1 -> increased_affinity (increased affinity). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13811-13826. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:691-706. arg2 -> decreased_affinity (decreased affinity). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13811-13827. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:691-707. arg3 -> modifier_amount (modifier amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13811-13827. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:691-707. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::InflictActorStatuses",
         (('arg0', 'status_mask'), ('arg1', 'actor_stats'), ('temp_a0', 'element_status_mask'), ('var_a3', 'equipment_modifier_amount')),
         "F00520 F00520 Battle::InflictActorStatuses: original func_80086FA8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13832 (function lines 13832-14009). arg0 -> status_mask (status mask). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13832-13984. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:713-865. arg1 -> actor_stats (actor stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13832-14005. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:713-886. temp_a0 -> element_status_mask (element status mask). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13836-13994. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:717-875. var_a3 -> equipment_modifier_amount (equipment modifier amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:13835-14005. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:716-886. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::ApplyActorActionResult",
         (('arg0', 'hit_result'),),
         "F00522 F00522 Battle::ApplyActorActionResult: original _applyBattleAbilityEffect at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14018 (function lines 14018-14374). arg0 -> hit_result (hit result). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14018-14368. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:901-1251. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::UpdateActorStatusTimers",
         (('var_a0_2', 'limb_recovered'), ('var_v1', 'healing_dividend')),
         "F00523 F00523 Battle::UpdateActorStatusTimers: original func_80087EF4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14376 (function lines 14376-14497). arg0: independent scoped binding(s); exact mappings are listed below. Original references: no original named binding; see native-only prototype explanation. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1307-1336. Native-only named cast-prototype parameters; original caller forwards equivalent values without this named prototype. Callee contract evidence is in secondary_bindings. arg1: independent scoped binding(s); exact mappings are listed below. Original references: no original named binding; see native-only prototype explanation. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1307-1336. Native-only named cast-prototype parameters; original caller forwards equivalent values without this named prototype. Callee contract evidence is in secondary_bindings. arg2: independent scoped binding(s); exact mappings are listed below. Original references: no original named binding; see native-only prototype explanation. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1307-1336. Native-only named cast-prototype parameters; original caller forwards equivalent values without this named prototype. Callee contract evidence is in secondary_bindings. arg3: independent scoped binding(s); exact mappings are listed below. Original references: no original named binding; see native-only prototype explanation. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1307-1336. Native-only named cast-prototype parameters; original caller forwards equivalent values without this named prototype. Callee contract evidence is in secondary_bindings. var_a0_2 -> limb_recovered (limb recovered). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14378-14485. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1262-1369. var_v1 -> healing_dividend (healing dividend). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14438-14444. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1322-1328. secondary_bindings at Native\\Game\\src\\Battle\\StatusEffects.cpp:1307-1307: arg0 -> actor_slot, arg1 -> limb_tag, arg2 -> encoded_amount, arg3 -> label_flags. Independent parameters inside QueueActorValueLabel function-pointer cast, not enclosing locals. F00714 resolves actor/limb and encodes amount sign plus label color/visibility flags. Preserve types and ABI. secondary_bindings at Native\\Game\\src\\Battle\\StatusEffects.cpp:1336-1336: arg0 -> actor_slot, arg1 -> limb_tag, arg2 -> encoded_amount, arg3 -> label_flags. Independent parameters inside QueueActorValueLabel function-pointer cast, not enclosing locals. F00714 resolves actor/limb and encodes amount sign plus label color/visibility flags. Preserve types and ABI. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change. Two named cast prototypes have separate arg0..arg3 bindings and neither belongs to enclosing actor parameter; originals call func_80093B68 directly. Healing dividend rounds negative hp before >>3; limb_recovered tests projected/actual HP >=2 without changing signed arithmetic."),
        ("Battle::UpdateActorVitals",
         (('temp_s0', 'actor_stats'), ('temp_s3', 'battle_actor'), ('temp_v0', 'actor_state_flags'), ('var_s1', 'update_result'), ('var_s5', 'pending_event_result')),
         "F00525 F00525 Battle::UpdateActorVitals: original func_80088554 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14560 (function lines 14560-14741). temp_s0 -> actor_stats (actor stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14592-14697. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1416-1521. temp_s3 -> battle_actor (battle actor). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14568-14699. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1392-1523. temp_v0 -> actor_state_flags (actor state flags). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14573-14590. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1397-1414. var_s1 -> update_result (update result). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14562-14737. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1386-1561. var_s5 -> pending_event_result (pending event result). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\146C.c:14564-14740. Native references: Native\\Game\\src\\Battle\\StatusEffects.cpp:1388-1564. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::QueueWorldLockedLabel",
         (('arg0', 'world_position'), ('temp_v0', 'label_spec')),
         "F00713 F00713 Battle::QueueWorldLockedLabel: original func_80093B04 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1475 (function lines 1475-1487). arg0 -> world_position (world position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1475-1484. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:135-144. temp_v0 -> label_spec (label spec). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1477-1484. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:137-144. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::QueueActorValueLabel",
         (('arg0', 'actor_slot'), ('arg1', 'limb_tag'), ('arg2', 'encoded_amount'), ('arg3', 'label_flags')),
         "F00714 F00714 Battle::QueueActorValueLabel: original func_80093B68 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1544 (function lines 1544-1641). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1544-1626. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:150-232. arg1 -> limb_tag (limb tag). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1544-1557. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:150-163. arg2 -> encoded_amount (encoded amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1544-1634. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:150-240. arg3 -> label_flags (label flags). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1544-1628. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:150-234. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::QueueAbilityTimingResult",
         (('arg0', 'timing_result'),),
         "F00715 F00715 Battle::QueueAbilityTimingResult: original vs_battle_renderBattleAbilityTimingResult at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1643 (function lines 1643-1664). arg0 -> timing_result (timing result). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1643-1660. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:250-267. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::QueueChainCount",
         (('arg0', 'chain_count'),),
         "F00716 F00716 Battle::QueueChainCount: original func_80093F24 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1666 (function lines 1666-1693). arg0 -> chain_count (chain count). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1666-1687. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:274-295. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::QueueActorStatusLabels",
         (('arg0', 'actor_slot'), ('arg2', 'status_mask'), ('arg3', 'label_flags'), ('var_s1', 'status_bit')),
         "F00717 F00717 Battle::QueueActorStatusLabels: original func_80093FEC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1695 (function lines 1695-1706). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1695-1702. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:304-311. arg1 intentionally retained: Original func_80093FEC formal never used; each emitted status label hard-codes bone tag 0xFB instead. Keep ignored slot, not a purported active limb selector. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1695. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:304. arg2 -> status_mask (status mask). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1695-1701. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:304-310. arg3 -> label_flags (label flags). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1695-1702. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:304-311. var_s1 -> status_bit (status bit). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1698-1701. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:307-310. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::QueueActorStatusLabel",
         (('arg0', 'actor_slot'), ('arg1', 'bone_tag'), ('arg2', 'status_index'), ('arg3', 'label_flags'), ('sp10', 'screen_position')),
         "F00718 F00718 Battle::QueueActorStatusLabel: original func_8009406C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1708 (function lines 1708-1755). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1708-1749. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:318-359. arg1 -> bone_tag (bone tag). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1708-1750. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:318-360. arg2 -> status_index (status index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1708-1752. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:318-362. arg3 -> label_flags (label flags). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1708-1751. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:318-361. sp10 -> screen_position (screen position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:1710-1735. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:320-345. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::AdvanceFloatingLabelQueue",
         (('arg0', 'label'),),
         "F00721 F00721 Battle::AdvanceFloatingLabelQueue: original func_80094AF8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2067 (function lines 2067-2071). arg0 -> label (label). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2067-2070. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:680-683. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::DrawLockedFloatingLabel",
         (('arg0', 'screen_x'), ('arg1', 'screen_y'), ('arg2', 'label')),
         "F00724 F00724 Battle::DrawLockedFloatingLabel: original func_8009506C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2167 (function lines 2167-2208). arg0 -> screen_x (screen x). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2167-2204. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:783-820. arg1 -> screen_y (screen y). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2167-2185. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:783-801. arg2 -> label (label). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2167-2178. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:783-794. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::DrawStatusRemovalMarker",
         (('arg0', 'screen_x'), ('arg1', 'screen_y')),
         "F00726 F00726 Battle::DrawStatusRemovalMarker: original func_800957D0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2312 (function lines 2312-2330). arg0 -> screen_x (screen x). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2312-2322. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:930-940. arg1 -> screen_y (screen y). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2312-2322. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:930-940. arg2 intentionally retained: Original func_800957D0 uses incoming removal-marker animation index then shifts it left four into texture-U displacement. The same parameter binding holds both representations; preserve it rather than claim a pure index or pixel coordinate. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2312-2324. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:930-942. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::SetActorMarkersHidden",
         (('arg0', 'hidden'),),
         "F00729 F00729 Battle::SetActorMarkersHidden: original func_80095B70 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2379 (function lines 2379-2379). arg0 -> hidden (hidden). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2379. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:1000. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::SetActorMarker",
         (('arg0', 'actor_slot'), ('arg1', 'marker_flags')),
         "F00730 F00730 Battle::SetActorMarker: original func_80095B7C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2381 (function lines 2381-2397). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2381-2393. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:1003-1015. arg1 -> marker_flags (marker flags). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2381-2396. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:1003-1018. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::CompactFloatingLabels",
         (('arg0', 'removal_count'),),
         "F00735 F00735 Battle::CompactFloatingLabels: original func_8009639C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2613 (function lines 2613-2626). arg0 -> removal_count (removal count). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2613-2618. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:1233-1238. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::RemoveFloatingLabel",
         (('arg0', 'label_index'),),
         "F00736 F00736 Battle::RemoveFloatingLabel: original func_80096444 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2628 (function lines 2628-2640). arg0 -> label_index (label index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2628-2634. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:1249-1255. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::QueueEquipmentStatChange",
         (('arg1', 'stat_label_kind'), ('arg2', 'encoded_amount')),
         "F00738 F00738 Battle::QueueEquipmentStatChange: original func_80096768 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2694 (function lines 2694-2763). arg0 intentionally retained: Original func_80096768 receives equipment/bone tag (including 240 weapon, 241 shield), then reassigns it to display icon index 0..6 before storing colorFlags. Intentionally merged tag/index lifetime, not a single unchanged equipment identifier. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2694-2748. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:1317-1371. arg1 -> stat_label_kind (stat label kind). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2694-2749. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:1317-1372. arg2 -> encoded_amount (encoded amount). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2694-2754. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:1317-1377. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::ShowAbilityTimingCue",
         (('arg0', 'cue_duration_frames'),),
         "F00740 F00740 Battle::ShowAbilityTimingCue: original func_80096FF0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2838 (function lines 2838-2862). arg0 -> cue_duration_frames (cue duration frames). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\2842C.c:2838-2861. Native references: Native\\Game\\src\\Battle\\Feedback.cpp:1463-1486. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names. Producer UpdateBattleAbilityTiming sets cue lifetime from (margin*2)/State_tickSpeed, not elapsed time. ResetFloatingLabels zeroes it; UpdateFloatingLabels decrements once per draw. Original 2842C.c:1468,1764-1766,2861 verifies duration_frames."),
        ("Battle::PlayMeleeAttackAnimation",
         (('arg0', 'actor_slot'), ('arg1', 'target_position'), ('arg2', 'animation_parameter'), ('temp_s0', 'actor'), ('temp_v0', 'target_pitch'), ('var_a1', 'animation_id')),
         "F00817 F00817 Battle::PlayMeleeAttackAnimation: original func_8009DD00 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2331 (function lines 2331-2363). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2331-2360. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:27-56. arg1 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2331-2346. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:27-42. arg2 -> animation_parameter (animation parameter). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2331-2352. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:27-48. temp_s0 -> actor (actor). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2334-2361. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:30-57. temp_v0 -> target_pitch (target pitch). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2335-2352. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:31-48. var_a1 -> animation_id (animation id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2336-2360. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:32-56. Review context: Reviewed original function and native producers/consumers. Actor array indices, position/pitch calculations, animation header out-parameter and PlayActorAnimation blend argument preserve the original contracts; Abilities queue source, mapped input slots and unlock bit packing follow their actual consumers."),
        ("Battle::PlayDefenseAnimation",
         (('arg0', 'actor_slot'), ('arg1', 'target_position'), ('temp_s0', 'actor'), ('temp_v0', 'target_pitch'), ('var_a1', 'animation_id'), ('var_s1', 'blend_frames')),
         "F00818 F00818 Battle::PlayDefenseAnimation: original func_8009DDF4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2365 (function lines 2365-2406). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2365-2396. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:62-93. arg1 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2365-2386. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:62-83. temp_s0 -> actor (actor). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2368-2404. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:65-101. temp_v0 -> target_pitch (target pitch). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2376-2391. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:73-88. var_a1 -> animation_id (animation id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2375-2396. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:72-93. var_s1 -> blend_frames (blend frames). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2374-2396. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:71-93. Review context: Reviewed original function and native producers/consumers. Actor array indices, position/pitch calculations, animation header out-parameter and PlayActorAnimation blend argument preserve the original contracts; Abilities queue source, mapped input slots and unlock bit packing follow their actual consumers."),
        ("Battle::SetEnemyCombatPose",
         (('arg0', 'actor_slot'), ('arg1', 'pose_mode'), ('sp10', 'animation_header'), ('temp_s2', 'actor'), ('var_s0', 'animation_id'), ('var_s1', 'blend_frames')),
         "F00819 F00819 Battle::SetEnemyCombatPose: original func_8009DF3C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2408 (function lines 2408-2449). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2408-2445. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:106-143. arg1 -> pose_mode (pose mode). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2408-2446. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:106-144. sp10 -> animation_header (animation header). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2410-2438. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:108-136. temp_s2 -> actor (actor). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2411-2447. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:109-145. var_s0 -> animation_id (animation id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2413-2445. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:111-143. var_s1 -> blend_frames (blend frames). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2414-2445. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:112-143. Review context: Reviewed original function and native producers/consumers. Actor array indices, position/pitch calculations, animation header out-parameter and PlayActorAnimation blend argument preserve the original contracts; Abilities queue source, mapped input slots and unlock bit packing follow their actual consumers."),
        ("Battle::PlaySpellCastAnimation",
         (('arg0', 'actor_slot'), ('arg1', 'target_position'), ('arg2', 'cast_phase'), ('sp10', 'cast_animations'), ('sp18', 'fallback_cast_animations'), ('sp20', 'animation_header')),
         "F00823 F00823 Battle::PlaySpellCastAnimation: original func_8009E2E0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2554 (function lines 2554-2617). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2554-2615. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:150-211. arg1 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2554-2571. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:150-167. arg2 -> cast_phase (cast phase). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2554-2609. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:150-205. sp10 -> cast_animations (cast animations). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2556-2600. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:152-196. sp18 -> fallback_cast_animations (fallback cast animations). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2557-2609. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:153-205. sp20 -> animation_header (animation header). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2558-2608. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:154-204. Review context: Reviewed original function and native producers/consumers. Actor array indices, position/pitch calculations, animation header out-parameter and PlayActorAnimation blend argument preserve the original contracts; Abilities queue source, mapped input slots and unlock bit packing follow their actual consumers."),
        ("Battle::PlayActorDamageReaction",
         (('arg0', 'actor_slot'), ('arg1', 'source_position'), ('sp10', 'bone_position'), ('temp_a0', 'source_delta_z'), ('temp_a1', 'source_delta_x'), ('temp_s1', 'actor'), ('temp_v1', 'distance_delta_z'), ('temp_v1_2', 'source_facing_delta'), ('var_s4', 'blend_frames'), ('var_v0', 'biased_facing_delta'), ('var_v1', 'source_pitch'), ('var_v1_2', 'distance_z_squared')),
         "F00830 F00830 Battle::PlayActorDamageReaction: original func_8009EA14 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2837 (function lines 2837-2954). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2837-2951. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:216-330. arg1 -> source_position (source position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2837-2900. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:216-279. sp10 -> bone_position (bone position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2839-2882. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:218-261. temp_a0 -> source_delta_z (source delta z). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2841-2905. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:220-284. temp_a1 -> source_delta_x (source delta x). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2842-2905. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:221-284. temp_s1 -> actor (actor). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2840-2952. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:219-331. temp_v1 -> distance_delta_z (distance delta z). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2844-2874. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:223-253. temp_v1_2 -> source_facing_delta (source facing delta). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2845-2919. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:224-298. var_s0 intentionally retained: Intentional merged original local: 0x5B/0x45/0x44/0x40/0x42/0x3B animation ID, but also x delta, x squared and horizontal distance before ratan2 in original 30DB0.c:2856-2883. No single semantic role; splitting lifetimes is outside identifier-only review. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2843-2951. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:222-330. var_s4 -> blend_frames (blend frames). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2846-2951. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:225-330. var_v0 -> biased_facing_delta (biased facing delta). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2849-2922. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:228-301. var_v1 -> source_pitch (source pitch). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2847-2889. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:226-268. var_v1_2 -> distance_z_squared (distance z squared). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2848-2880. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:227-259. Review context: Reviewed original function and native producers/consumers. Actor array indices, position/pitch calculations, animation header out-parameter and PlayActorAnimation blend argument preserve the original contracts; Abilities queue source, mapped input slots and unlock bit packing follow their actual consumers."),
        ("Battle::PlayChainAbilityAnimation",
         (('arg0', 'actor_slot'), ('arg1', 'target_position'), ('arg2', 'chain_ability_index'), ('arg3', 'chain_index'), ('sp10', 'bone_position')),
         "F00831 F00831 Battle::PlayChainAbilityAnimation: original func_8009EC9C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2956 (function lines 2956-3038). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2956-3037. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:336-417. arg1 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2956-3017. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:336-397. arg2 -> chain_ability_index (chain ability index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2956-2974. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:336-354. arg3 -> chain_index (chain index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2956-3007. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:336-387. sp10 -> bone_position (bone position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:2958-2995. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:338-375. Review context: Reviewed original function and native producers/consumers. Actor array indices, position/pitch calculations, animation header out-parameter and PlayActorAnimation blend argument preserve the original contracts; Abilities queue source, mapped input slots and unlock bit packing follow their actual consumers."),
        ("Battle::PlayDefenseAbilityAnimation",
         (('arg0', 'actor_slot'), ('arg1', 'target_position'), ('temp_s0', 'actor'), ('temp_v0', 'target_pitch'), ('var_a1', 'animation_id'), ('var_s1', 'blend_frames')),
         "F00832 F00832 Battle::PlayDefenseAbilityAnimation: original func_8009EE9C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3040 (function lines 3040-3083). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3040-3082. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:421-463. arg1 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3040-3061. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:421-442. arg2 intentionally retained: Original 30DB0.c:3040 formal is never read, written or forwarded; sole Battle_ActionFlow call passes literal 0. Keep unused legacy parameter identity rather than invent a mode; signature retained. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3040. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:421. temp_s0 -> actor (actor). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3043-3079. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:424-460. temp_v0 -> target_pitch (target pitch). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3049-3066. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:430-447. var_a1 -> animation_id (animation id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3050-3071. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:431-452. var_s1 -> blend_frames (blend frames). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3051-3071. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:432-452. Review context: Reviewed original function and native producers/consumers. Actor array indices, position/pitch calculations, animation header out-parameter and PlayActorAnimation blend argument preserve the original contracts; Abilities queue source, mapped input slots and unlock bit packing follow their actual consumers."),
        ("Battle::PlayBreakArtAnimation",
         (('arg0', 'actor_slot'), ('arg1', 'target_position'), ('arg2', 'action_phase'), ('sp10', 'animation_header')),
         "F00834 F00834 Battle::PlayBreakArtAnimation: original func_8009F298 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3189 (function lines 3189-3200). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3189-3197. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:570-578. arg1 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3189-3193. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:570-574. arg2 -> action_phase (action phase). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3189-3194. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:570-575. sp10 -> animation_header (animation header). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\30DB0.c:3191-3196. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:572-577. Review context: Reviewed original function and native producers/consumers. Actor array indices, position/pitch calculations, animation header out-parameter and PlayActorAnimation blend argument preserve the original contracts; Abilities queue source, mapped input slots and unlock bit packing follow their actual consumers."),
        ("Battle::FinishEnemyCombatPose",
         (('arg0', 'actor_slot'), ('arg1', 'facing'), ('temp_s1', 'actor')),
         "F00891 F00891 Battle::FinishEnemyCombatPose: original func_800A2574 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\38C1C.c:625 (function lines 625-631). arg0 -> actor_slot (actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\38C1C.c:625-629. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:584-588. arg1 -> facing (facing). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\38C1C.c:625-628. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:584-587. temp_s1 -> actor (actor). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\38C1C.c:627-630. Native references: Native\\Game\\src\\Battle\\ActionAnimation.cpp:586-589. Review context: Reviewed original function and native producers/consumers. Actor array indices, position/pitch calculations, animation header out-parameter and PlayActorAnimation blend argument preserve the original contracts; Abilities queue source, mapped input slots and unlock bit packing follow their actual consumers."),
        ("Battle::SetSphereVolumes",
         (('arg0', 'primary_volume'), ('arg1', 'secondary_volume'), ('arg2', 'sphere_mode'), ('arg3', 'target_previews')),
         "F01281 F01281 Battle::SetSphereVolumes: original func_800C05EC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\573B8.c:444 (function lines 444-473). arg0 -> primary_volume (primary volume). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\573B8.c:444-448. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:153-157. arg1 -> secondary_volume (secondary volume). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\573B8.c:444-465. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:153-174. arg2 -> sphere_mode (sphere mode). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\573B8.c:444-467. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:153-176. arg3 -> target_previews (target previews). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\573B8.c:444-468. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:153-177. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::SetSecondarySphereVolume",
         (('arg0', 'secondary_volume'),),
         "F01283 F01283 Battle::SetSecondarySphereVolume: original func_800C0700 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\573B8.c:482 (function lines 482-486). arg0 -> secondary_volume (secondary volume). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\573B8.c:482-484. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:193-195. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::BuildTargetVolumeTestTransform",
         (('arg0', 'volume'), ('arg1', 'test_transform'), ('arg2', 'rotation_matrix'), ('new_var', 'extent_component')),
         "F01290 F01290 Battle::BuildTargetVolumeTestTransform: original func_800C0FA8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:170 (function lines 170-186). arg0 -> volume (volume). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:170-182. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:419-431. arg1 -> test_transform (test transform). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:170-185. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:419-434. arg2 -> rotation_matrix (rotation matrix). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:170-185. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:419-434. new_var -> extent_component (extent component). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:174-178. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:423-427. temp_v0 intentionally retained: Original func_800C0FA8 and current native body contain only int temp_v0; never initialized, read or written. Retain unused decompiler declaration without fabricated purpose. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:172. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:421. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::IsPointInTargetEllipsoid",
         (('arg0', 'volume'), ('arg1', 'point_components')),
         "F01291 F01291 Battle::IsPointInTargetEllipsoid: original func_800C1034 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:188 (function lines 188-240). arg0 -> volume (volume). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:188-196. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:438-446. arg1 -> point_components (point components). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:188-209. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:438-459. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::IsPointInTargetCylinder",
         (('arg0', 'volume'), ('arg1', 'point_components'), ('arg2', 'shift_height_origin')),
         "F01292 F01292 Battle::IsPointInTargetCylinder: original func_800C110C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:242 (function lines 242-297). arg0 -> volume (volume). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:242-277. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:493-528. arg1 -> point_components (point components). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:242-263. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:493-514. arg2 -> shift_height_origin (shift height origin). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:242-273. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:493-524. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::IsPointInTargetCone",
         (('arg0', 'volume'), ('arg1', 'point_components'), ('arg2', 'shift_height_origin')),
         "F01293 F01293 Battle::IsPointInTargetCone: original func_800C123C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:299 (function lines 299-346). arg0 -> volume (volume). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:299-336. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:551-588. arg1 -> point_components (point components). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:299-322. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:551-574. arg2 -> shift_height_origin (shift height origin). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:299-331. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:551-583. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::IsPointInTargetVolume",
         (('arg0', 'volume'), ('arg1', 'point_components')),
         "F01295 F01295 Battle::IsPointInTargetVolume: original func_800C1564 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:411 (function lines 411-444). arg0 -> volume (volume). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:411-442. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:665-696. arg1 -> point_components (point components). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:411-438. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:665-692. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::DrawSphereTargetLabel",
         (('arg0', 'label'),),
         "F01301 F01301 Battle::DrawSphereTargetLabel: original func_800C1DC4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:656 (function lines 656-748). arg0 -> label (label). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:656-744. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:892-980. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::UpdateSphereTargetLabels",
         (('temp_a0', 'screen_x'), ('var_s3', 'label_index'), ('var_v1', 'animation_step'), ('var_s1', 'label'), ('var_s0', 'screen_x_ptr'), ('v1', 'label_state')),
         "F01302 F01302 Battle::UpdateSphereTargetLabels: original func_800C20B4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:750 (function lines 750-819). temp_a0 -> screen_x (screen x). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:752-808. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:989-1046. v1 -> label_state (label state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:759-765. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:997-1003. var_s0 -> screen_x_ptr (screen x ptr). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:756-813. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:993-1051. var_s1 -> label (label). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:755-816. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:992-1054. var_s3 -> label_index (label index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:753-758. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:990-995. var_v1 -> animation_step (animation step). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:754-802. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:991-1040. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::BuildSphereCameraBasis",
         (('temp_a0', 'yaw_sine'), ('temp_s2', 'yaw_cosine')),
         "F01306 F01306 Battle::BuildSphereCameraBasis: original func_800C282C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:965 (function lines 965-982). temp_a0 -> yaw_sine (yaw sine). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:968-978. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:1210-1220. temp_s2 -> yaw_cosine (yaw cosine). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:969-979. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:1211-1221. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::DrawSpherePositionCursor",
         (('arg1', 'lock_pitch'),),
         "F01307 F01307 Battle::DrawSpherePositionCursor: original func_800C28AC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:984 (function lines 984-1034). arg1 -> lock_pitch (lock pitch). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\58578.c:984-1027. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:1227-1270. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::UpdateSphereTargetHighlights",
         (('arg0', 'target_lines'), ('arg1', 'target_count')),
         "F01310 F01310 Battle::UpdateSphereTargetHighlights: original func_800C4650 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BE50.c:65 (function lines 65-80). arg0 -> target_lines (target lines). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BE50.c:65-76. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:1365-1376. arg1 -> target_count (target count). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BE50.c:65-70. Native references: Native\\Game\\src\\Battle\\Sphere.cpp:1365-1370. Review context: Original and native individual candidate dataflow reviewed. Names follow actual status mask/stat writes, equipment affinity indices, target-volume geometry, source/target preview records, label animation, pulse API parameters or range test consumers; no layout or behavioral change."),
        ("Battle::DrawSlidingStatusIcon",
         (('arg0', 'status_icon_id'), ('arg1', 'slide_offset'), ('var_s1', 'clipped_width'), ('var_s2', 'texture_offset'), ('temp_a0', 'sprite_packet')),
         "F01355 F01355 Battle::DrawSlidingStatusIcon: original func_800C9CB4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3112 (function lines 3112-3156). arg0 -> status_icon_id (status icon id). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3112-3148. Native references: Native\\Game\\src\\Battle\\Hud.cpp:169-205. arg1 -> slide_offset (slide offset). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3112-3128. Native references: Native\\Game\\src\\Battle\\Hud.cpp:169-185. arg2 intentionally retained: Original func_800C9CB4 incoming icon slot is multiplied by 16 and biased by 160, then clipped/shifted as screen X. Retain the explicitly reviewed merged slot/pixel binding; do not split lifetime or mislabel the original formal. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3112-3152. Native references: Native\\Game\\src\\Battle\\Hud.cpp:169-209. temp_a0 -> sprite_packet (sprite packet). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3116-3134. Native references: Native\\Game\\src\\Battle\\Hud.cpp:173-191. v0: independent scoped binding(s); exact mappings are listed below. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3143-3149. Native references: Native\\Game\\src\\Battle\\Hud.cpp:200-206. var_s1 -> clipped_width (clipped width). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3114-3152. Native references: Native\\Game\\src\\Battle\\Hud.cpp:171-209. var_s2 -> texture_offset (texture offset). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3115-3153. Native references: Native\\Game\\src\\Battle\\Hud.cpp:172-210. scoped_renames at Native\\Game\\src\\Battle\\Hud.cpp:200-201: v0 -> clipped_icon_left. First block v0 is arg2-8, a temporary left screen X used to anchor the clipped lower strip. scoped_renames at Native\\Game\\src\\Battle\\Hud.cpp:205-206: v0 -> overlay_texture_end. Separate block v0 is atlas packed UV endpoint (arg0&3)*8+0x3070; subtracting clipped_width chooses strip UV. Not the screen-X local above. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::StatusIconFlags",
         (('arg0', 'actor_stats'), ('temp_v1', 'remaining_status_bits'), ('var_a2', 'icon_flags')),
         "F01356 F01356 Battle::StatusIconFlags: original vs_battle_getStatusFlags at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3158 (function lines 3158-3172). arg0 -> actor_stats (actor stats). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3158-3161. Native references: Native\\Game\\src\\Battle\\Hud.cpp:216-219. temp_v1 -> remaining_status_bits (remaining status bits). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3161-3166. Native references: Native\\Game\\src\\Battle\\Hud.cpp:219-224. var_a2 -> icon_flags (icon flags). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3162-3171. Native references: Native\\Game\\src\\Battle\\Hud.cpp:220-229. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::ShortcutIconBrightness",
         (('arg0', 'shortcut_index'), ('temp_s0', 'restriction_flags'), ('var_s2', 'brightness_transparency')),
         "F01358 F01358 Battle::ShortcutIconBrightness: original func_800C9EB8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3197 (function lines 3197-3237). arg0 -> shortcut_index (shortcut index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3197-3221. Native references: Native\\Game\\src\\Battle\\Hud.cpp:257-281. temp_s0 -> restriction_flags (restriction flags). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3199-3229. Native references: Native\\Game\\src\\Battle\\Hud.cpp:259-289. var_s2 -> brightness_transparency (brightness transparency). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3200-3236. Native references: Native\\Game\\src\\Battle\\Hud.cpp:260-296. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names. Consumer Render_QueueSpriteWithTexturePage has packed brightness_transparency argument: low byte grayscale, bit8 semi-transparency. Original 573B8.c:291-299 masks a0 by 0xFF and separately extracts (a0>>7)&2. Retain the flags in the name, not merely brightness; values 0x180/0x140 preserve transparency."),
        ("Battle::DrawShortcutButtonMarker",
         (('arg0', 'packed_screen_position'), ('arg1', 'brightness_transparency'), ('temp_v0', 'sprite_packet')),
         "F01360 F01360 Battle::DrawShortcutButtonMarker: original func_800C9F98 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3241 (function lines 3241-3247). arg0 -> packed_screen_position (packed screen position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3241-3244. Native references: Native\\Game\\src\\Battle\\Hud.cpp:303-306. arg1 -> brightness_transparency (brightness transparency). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3241-3244. Native references: Native\\Game\\src\\Battle\\Hud.cpp:303-306. temp_v0 -> sprite_packet (sprite packet). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3243-3246. Native references: Native\\Game\\src\\Battle\\Hud.cpp:305-308. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names. Consumer Render_QueueSpriteWithTexturePage has packed brightness_transparency argument: low byte grayscale, bit8 semi-transparency. Original 573B8.c:291-299 masks a0 by 0xFF and separately extracts (a0>>7)&2. Retain the flags in the name, not merely brightness; values 0x180/0x140 preserve transparency."),
        ("Battle::DrawShortcutHud",
         (('temp_s3', 'panel_x_offset'), ('temp_v0', 'icon_brightness_transparency'), ('temp_v0_2', 'sprite_packet')),
         "F01361 F01361 Battle::DrawShortcutHud: original func_800C9FE8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3249 (function lines 3249-3304). temp_s3 -> panel_x_offset (panel x offset). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3251-3299. Native references: Native\\Game\\src\\Battle\\Hud.cpp:314-362. temp_v0 -> icon_brightness_transparency (icon brightness transparency). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3252-3292. Native references: Native\\Game\\src\\Battle\\Hud.cpp:315-355. temp_v0_2 -> sprite_packet (sprite packet). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3254-3297. Native references: Native\\Game\\src\\Battle\\Hud.cpp:317-360. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names. Consumer Render_QueueSpriteWithTexturePage has packed brightness_transparency argument: low byte grayscale, bit8 semi-transparency. Original 573B8.c:291-299 masks a0 by 0xFF and separately extracts (a0>>7)&2. Retain the flags in the name, not merely brightness; values 0x180/0x140 preserve transparency."),
        ("Battle::DrawPuzzleTimer",
         (('sp10', 'timer_components'),),
         "F01384 F01384 Battle::DrawPuzzleTimer: original _renderTimer at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3844 (function lines 3844-3865). sp10 -> timer_components (timer components). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3846-3859. Native references: Native\\Game\\src\\Battle\\Hud.cpp:549-561. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names. sp10 contains numeric time components {hundredths,seconds,minutes,hours}, not individual decimal digits. The loop later splits each component into BCD digits. Preserve char widths and clamp behavior."),
        ("Battle::SetHudHidden",
         (('arg0', 'hidden'),),
         "F01385 F01385 Battle::SetHudHidden: original func_800CB654 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3867 (function lines 3867-3867). arg0 -> hidden (hidden). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3867. Native references: Native\\Game\\src\\Battle\\Hud.cpp:572. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::SetHudBattleMode",
         (('arg0', 'battle_mode'),),
         "F01386 F01386 Battle::SetHudBattleMode: original func_800CB660 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3869 (function lines 3869-3869). arg0 -> battle_mode (battle mode). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\5BF94.c:3869. Native references: Native\\Game\\src\\Battle\\Hud.cpp:575. Review context: Original/native candidate statements individually reviewed: actual label key, bone tag, queue count, packed amount/flags, sprite XY/UV/brightness, HUD restriction result and timer component consumers establish these names."),
        ("Battle::Ai::SetTemporaryAwarenessIndicator",
         (('arg0', 'ai_state'), ('arg1', 'awareness_marker'), ('arg2', 'duration_frames')),
         "F01630 F01630 Battle::Ai::SetTemporaryAwarenessIndicator: original func_800D8260 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\6FA60.c:407 (function lines 407-411). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\6FA60.c:407-410. Native references: Native\\Game\\src\\Battle\\Ai\\Perception.cpp:49-52. arg1 -> awareness_marker (awareness marker). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\6FA60.c:407-409. Native references: Native\\Game\\src\\Battle\\Ai\\Perception.cpp:49-51. arg2 -> duration_frames (duration frames). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\6FA60.c:407-410. Native references: Native\\Game\\src\\Battle\\Ai\\Perception.cpp:49-52. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::UpdateCombatPursuit",
         (('arg0', 'ai_state'),),
         "F01650 F01650 Battle::Ai::UpdateCombatPursuit: original func_800DA1D4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:610 (function lines 610-709). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:610-701. Native references: Native\\Game\\src\\Battle\\Ai\\Behavior.cpp:137-228. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::TryApproachPlannedTarget",
         (('arg0', 'ai_state'), ('arg1', 'candidate')),
         "F01651 F01651 Battle::Ai::TryApproachPlannedTarget: original func_800DA4BC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:788 (function lines 788-812). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:788-806. Native references: Native\\Game\\src\\Battle\\Ai\\Behavior.cpp:239-257. arg1 -> candidate (candidate). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:788-798. Native references: Native\\Game\\src\\Battle\\Ai\\Behavior.cpp:239-249. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::TryExecuteActionCandidate",
         (('arg0', 'ai_state'), ('arg1', 'candidate')),
         "F01657 F01657 Battle::Ai::TryExecuteActionCandidate: original func_800DAC80 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:1006 (function lines 1006-1036). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:1006-1029. Native references: Native\\Game\\src\\Battle\\Ai\\Behavior.cpp:439-462. arg1 -> candidate (candidate). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:1006-1028. Native references: Native\\Game\\src\\Battle\\Ai\\Behavior.cpp:439-461. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::TryExecutePlannedActions",
         (('arg0', 'ai_state'),),
         "F01658 F01658 Battle::Ai::TryExecutePlannedActions: original func_800DAD9C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:1093 (function lines 1093-1127). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\71618.c:1093-1119. Native references: Native\\Game\\src\\Battle\\Ai\\Behavior.cpp:472-498. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::UnlinkEngagedActor",
         (('arg0', 'ai_state'),),
         "F01678 F01678 Battle::Ai::UnlinkEngagedActor: original func_800DBF00 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\73700.c:414 (function lines 414-442). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\73700.c:414-433. Native references: Native\\Game\\src\\Battle\\Ai\\Perception.cpp:474-493. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::UpdateEngagementBuildup",
         (('arg0', 'ai_state'),),
         "F01679 F01679 Battle::Ai::UpdateEngagementBuildup: original func_800DBFE4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\73700.c:463 (function lines 463-498). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\73700.c:463-496. Native references: Native\\Game\\src\\Battle\\Ai\\Perception.cpp:505-538. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::ApplySimulatedActionResults",
         (('arg0', 'simulated_actor_rows'),),
         "F01697 F01697 Battle::Ai::ApplySimulatedActionResults: original func_800DC888 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:473 (function lines 473-511). arg0 -> simulated_actor_rows (simulated actor rows). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:473-502. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:117-146. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::SimulateActionCandidate",
         (('arg0', 'ai_state'), ('arg1', 'simulated_actor_rows'), ('arg2', 'candidate'), ('arg3', 'simulation_mode'), ('sp20', 'simulated_source_position'), ('sp28', 'target_position'), ('temp_s0', 'target_actor_slot'), ('temp_v0', 'action_available')),
         "F01698 F01698 Battle::Ai::SimulateActionCandidate: original func_800DCAA0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:513 (function lines 513-537). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:514-532. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:159-177. arg1 -> simulated_actor_rows (simulated actor rows). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:514-535. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:159-180. arg2 -> candidate (candidate). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:514-532. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:159-177. arg3 -> simulation_mode (simulation mode). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:514-521. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:159-166. sp20 -> simulated_source_position (simulated source position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:516-532. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:161-177. sp28 -> target_position (target position). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:517-532. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:162-177. temp_s0 -> target_actor_slot (target actor slot). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:518-532. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:163-177. temp_v0 -> action_available (action available). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:519-534. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:164-179. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::UpdateCandidateHitWeight",
         (('arg0', 'candidate'),),
         "F01699 F01699 Battle::Ai::UpdateCandidateHitWeight: original func_800DCBD8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:539 (function lines 539-554). arg0 -> candidate (candidate). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:539-553. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:185-199. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::EvaluateActionCandidate",
         (('arg0', 'ai_state'), ('arg1', 'candidate'), ('arg2', 'simulation_mode'), ('arg3', 'action_definition')),
         "F01704 F01704 Battle::Ai::EvaluateActionCandidate: original func_800DCC94 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:620 (function lines 620-674). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:621-652. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:267-298. arg1 -> candidate (candidate). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:621-673. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:267-319. arg2 -> simulation_mode (simulation mode). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:621-646. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:267-292. arg3 -> action_definition (action definition). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:621-629. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:267-275. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::SelectFallbackAction",
         (('arg0', 'ai_state'),),
         "F01707 F01707 Battle::Ai::SelectFallbackAction: original func_800DD604 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:899 (function lines 899-936). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:899-913. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:323-337. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::EnumerateActionCandidates",
         (('arg0', 'ai_state'), ('arg1', 'readiness_threshold')),
         "F01712 F01712 Battle::Ai::EnumerateActionCandidates: original func_800DD918 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1007 (function lines 1007-1154). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1007-1147. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:363-503. arg1 -> readiness_threshold (readiness threshold). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1007-1025. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:363-381. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::AdvancePlanningSimulation",
         (('arg0', 'simulation_time_step'),),
         "F01713 F01713 Battle::Ai::AdvancePlanningSimulation: original func_800DE030 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1167 (function lines 1167-1262). arg0 -> simulation_time_step (simulation time step). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1167-1210. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:513-556. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::ReconcileCompletedActionPlan",
         (('arg0', 'completed_action'), ('temp_t1', 'command_actor'), ('temp_a2', 'candidate')),
         "F01717 F01717 Battle::Ai::ReconcileCompletedActionPlan: original func_800DEB10 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1458 (function lines 1458-1507). arg0 -> completed_action (completed action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1458-1492. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:780-814. temp_a2 -> candidate (candidate). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1486-1490. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:808-812. temp_t1 -> command_actor (command actor). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1463-1489. Native references: Native\\Game\\src\\Battle\\Ai\\ActionPlanning.cpp:785-811. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::NotifyActionRelationshipChanges",
         (('arg0', 'completed_action'),),
         "F01719 F01719 Battle::Ai::NotifyActionRelationshipChanges: original func_800DEEA4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1596 (function lines 1596-1609). arg0 -> completed_action (completed action). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\74088.c:1596-1607. Native references: Native\\Game\\src\\Battle\\Ai\\Perception.cpp:701-712. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::ResolveRouteGateExit",
         (('arg0', 'entry_tile'), ('arg1', 'exit_tile_out')),
         "F01721 F01721 Battle::Ai::ResolveRouteGateExit: original func_800DF9A8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:459 (function lines 459-478). arg0 -> entry_tile (entry tile). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:459-477. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:168-186. arg1 -> exit_tile_out (exit tile out). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:459-477. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:168-186. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::FindNearestRouteGateEntry",
         (('arg0', 'platform_index'), ('arg3', 'entry_tile_out')),
         "F01722 F01722 Battle::Ai::FindNearestRouteGateEntry: original func_800DFA54 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:480 (function lines 480-496). arg0 -> platform_index (platform index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:480-492. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:190-202. arg1 intentionally retained: Original func_800DFA54 parameter is fixed-point world X, shifted right 19 in place to tile X before gate matching. Intentionally merged coordinate representations; no lifetime split. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:480-487. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:190-197. arg2 intentionally retained: Original func_800DFA54 parameter is fixed-point world Z, shifted right 19 in place to tile Z before gate matching. Intentionally merged coordinate representations; no lifetime split. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:480-488. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:190-198. arg3 -> entry_tile_out (entry tile out). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:480-495. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:190-205. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::FindRouteGateAtTile",
         (('arg1', 'packed_tile'), ('arg2', 'match_entry_end'), ('sp10', 'platform_positions')),
         "F01723 F01723 Battle::Ai::FindRouteGateAtTile: original func_800DFAF8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:498 (function lines 498-521). arg0 intentionally retained: Original func_800DFAF8 first intptr_t/int argument is wholly unused; gate search uses packed tile, endpoint selector and globally collected platform positions. Do not invent a context role. Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:498. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:209. arg1 -> packed_tile (packed tile). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:498-512. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:209-223. arg2 -> match_entry_end (match entry end). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:498-509. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:209-220. sp10 -> platform_positions (platform positions). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:500-513. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:211-224. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::UpdateFlightDirectionalMovement",
         (('arg0', 'ai_state'), ('arg1', 'movement_angle'), ('sp10', 'vertical_step'), ('sp14', 'movement_command')),
         "F01734 F01734 Battle::Ai::UpdateFlightDirectionalMovement: original func_800E1238 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:1278 (function lines 1278-1328). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:1278-1326. Native references: Native\\Game\\src\\Battle\\Ai\\Movement.cpp:531-579. arg1 -> movement_angle (movement angle). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:1278-1312. Native references: Native\\Game\\src\\Battle\\Ai\\Movement.cpp:531-565. sp10 -> vertical_step (vertical step). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:1280-1290. Native references: Native\\Game\\src\\Battle\\Ai\\Movement.cpp:533-543. sp14 -> movement_command (movement command). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:1281-1326. Native references: Native\\Game\\src\\Battle\\Ai\\Movement.cpp:534-579. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::FilterDirectionsByGroundHeight",
         (('arg0', 'direction_mask'), ('arg1', 'packed_tile'), ('arg2', 'maximum_ground_height')),
         "F01738 F01738 Battle::Ai::FilterDirectionsByGroundHeight: original func_800E1850 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:1455 (function lines 1455-1471). arg0 -> direction_mask (direction mask). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:1455-1470. Native references: Native\\Game\\src\\Battle\\Ai\\Movement.cpp:687-702. arg1 -> packed_tile (packed tile). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:1455-1458. Native references: Native\\Game\\src\\Battle\\Ai\\Movement.cpp:687-690. arg2 -> maximum_ground_height (maximum ground height). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\771A8.c:1455-1465. Native references: Native\\Game\\src\\Battle\\Ai\\Movement.cpp:687-697. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::ForgetAiActionTarget",
         (('arg0', 'ai_state'),),
         "F01793 F01793 Battle::Ai::ForgetAiActionTarget: original func_800E4CE8 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7C4E8.c:407 (function lines 407-407). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7C4E8.c:407. Native references: Native\\Game\\src\\Battle\\Ai\\ActionSelection.cpp:20. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::ApplySavedAiSettingsFlags",
         (('arg0', 'settings_word'), ('arg1', 'saved_settings')),
         "F01812 F01812 Battle::Ai::ApplySavedAiSettingsFlags: original func_800E5A74 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:595 (function lines 595-598). arg0 -> settings_word (settings word). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:595-597. Native references: Native\\Game\\src\\Battle\\Ai\\ActorState.cpp:87-89. arg1 -> saved_settings (saved settings). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:595-597. Native references: Native\\Game\\src\\Battle\\Ai\\ActorState.cpp:87-89. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::FloodFillAiTerrainHeights",
         (('arg0', 'tile_x'), ('arg1', 'tile_z'), ('arg2', 'height_value')),
         "F01814 F01814 Battle::Ai::FloodFillAiTerrainHeights: original func_800E5EC0 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:760 (function lines 760-777). arg0 -> tile_x (tile x). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:760-769. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:646-655. arg1 -> tile_z (tile z). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:760-770. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:646-656. arg2 -> height_value (height value). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:760-774. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:646-660. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::FindAdjacentAiGroundHeight",
         (('arg0', 'tile_x'), ('arg1', 'tile_z')),
         "F01815 F01815 Battle::Ai::FindAdjacentAiGroundHeight: original func_800E5FDC at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:779 (function lines 779-793). arg0 -> tile_x (tile x). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:779-786. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:666-673. arg1 -> tile_z (tile z). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7CD68.c:779-787. Native references: Native\\Game\\src\\Battle\\Ai\\Navigation.cpp:666-674. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::IsBossActionReady",
         (('arg0', 'ai_state'),),
         "F01847 F01847 Battle::Ai::IsBossActionReady: original func_800E78F4 at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7EE98.c:553 (function lines 553-565). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7EE98.c:553-560. Native references: Native\\Game\\src\\Battle\\Ai\\OrbitingBoss.cpp:89-96. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::Ai::UpdateOrbitingBossBehavior",
         (('arg0', 'ai_state'), ('temp_s0', 'orbit_state'), ('temp_a1', 'command_actor')),
         "F01849 F01849 Battle::Ai::UpdateOrbitingBossBehavior: original func_800E7F8C at resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7EE98.c:753 (function lines 753-825). arg0 -> ai_state (ai state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7EE98.c:753-810. Native references: Native\\Game\\src\\Battle\\Ai\\OrbitingBoss.cpp:259-316. temp_a1 -> command_actor (command actor). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7EE98.c:796-806. Native references: Native\\Game\\src\\Battle\\Ai\\OrbitingBoss.cpp:302-312. temp_s0 -> orbit_state (orbit state). Original references: resources\\rood-reverseWin\\src\\BATTLE\\BATTLE.PRG\\7EE98.c:755-817. Native references: Native\\Game\\src\\Battle\\Ai\\OrbitingBoss.cpp:261-323. Review context: Original individual AI function reviewed against current body and actual callers. Names describe actor-state/candidate/result ownership, simulation threshold/delay, geometry, gate index and packed tile roles. SelectActorVerticalMovementStep returns walk/run step and command 9/10; SetTemporaryAwarenessIndicator writes marker and duration fields (FIELD_LEDGER T00532.090/T00399.045)."),
        ("Battle::ApplyLearnedActionUnlocks",
         (('var_v0', 'learned_flag_index'),),
         "F01851 F01851 Battle::ApplyLearnedActionUnlocks: original func_800F9818 at resources\\rood-reverseWin\\src\\BATTLE\\INITBTL.PRG\\18.c:240 (function lines 240-256). var_v0 -> learned_flag_index (learned flag index). Original references: resources\\rood-reverseWin\\src\\BATTLE\\INITBTL.PRG\\18.c:243-250. Native references: Native\\Game\\src\\Battle\\ActionFlow.cpp:705-712. Review context: Reviewed complete candidate use chains against original function. Names follow queue access, actor/stat indices, result output writes, geometry differences and the actual called helper contracts; no split local lifetimes or behavioral edits."),
    )
    for old, new in bindings
]


REGEX_FIXES += [
    (rf"^{name}$", rf"\b{old}\b", new, evidence)
    for name, bindings, evidence in (
        ("State::StartSceneTimer",
         (('arg0', 'timer_mode'), ('arg1', 'reset_scope'), ('arg2', 'packed_time')),
         "F00232 Original func_8007BBD8 at 146C.c:8511 writes D_80050468.unk0 and unk2, clears time, and for mode !=1 extracts frames/seconds/minutes/hours from successive bytes of arg2. It publishes flags A0-A3 and shows the puzzle timer. Native Event/ScriptVm.c supplies Event_kSceneTimerModes and Event_kSceneTimerResetScopes, corroborating mode/reset-scope meanings; State_SceneTimer layout names those exact fields."),
        ("State::SetSceneTimerPaused",
         (('arg0', 'paused'), ('temp_v1', 'active_mode')),
         "F00234 Original func_8007BD3C at 146C.c:8548 saves the active timer mode in unk1, writes mode zero and hides the timer when arg0 is nonzero; the other branch restores mode and shows it. Native Event/ScriptVm.c passes the pause operand and the reset/start paths establish mode."),
        ("State::SetTickSpeed",
         (('arg0', 'requested_tick_speed'),),
         "F00256 Original func_8007C36C at 146C.c:8740 accepts only 2 or 4 into vs_gametime_tickspeed and returns the resulting value. State_UpdateEventTimer and State_UpdatePlayTimeAndLoading consume that global as frame increments; this is a requested speed, not an unconditional setter value."),
        ("State::UpdateEventTimer",
         (('var_s1', 'timer_event_triggered'),),
         "F00524 Original func_800882F4 at 146C.c:14499 returns zero for inactive mode, updates count-up/count-down time and flags A0-A3, then sets var_s1=1 only if the frames reach zero and func_800BEC58(16,0,0,0)==1. Native Battle/StatusEffects.c consumes that return as the timer-event trigger result. This is not a timer-expired boolean: it depends on event evaluation."),
        ("State::ResetZoneScriptFlags",
         (('arg0', 'preserve_zone_flags'),),
         "F01256 Original func_800BEB9C at 4A0A8.c:4263 resets flags 64..127 only for zero. Native Field/RoomLoading.c passes one for gameEntryMode==1 and zero otherwise, under the existing resume guard. The name describes the actual suppression condition without altering the resume path."),
        ("State::ScriptFlag",
         (('arg0', 'flag_id'),),
         "F01258 Original vs_battle_getStateFlag at 4A0A8.c:4275 casts the entire state-flags object to char* and indexes it with the short parameter. The adjacent setter already calls that same index id. Preserve signedness, bounds behavior and the byte-backed field layout."),
    )
    for old, new in bindings
]


# Individually reviewed effect bindings; structural fixups run first.
# Separate shadowed locals and cast-prototype parameters before naming outer bindings.
for identifier, old, new, evidence in (
    ("F01589",
     "    Effects_TimelineTrack* temp_a1;",
     "    Effects_TimelineTrack* unused_track_declaration;",
     "The uninitialized outer pointer is declaration-only. A distinct inner pointer is initialized to tracks[track_index] and passed to every handler. Prepare the outer declaration first, then rename only the remaining inner temp_a1 binding."),
):
    FIXES.setdefault(identifier, []).append((old, new, evidence))

REGEX_FIXES += [
    (rf"^{name}$", rf"\b{old}\b", new, evidence)
    for name, bindings, evidence in (
        ("Effects::StartWeaponTrail",
         (('arg0', 'actor_index'), ('arg1', 'bone_index')),
         "F00056 Forwarded to SpawnBoneTrail, whose packed bytes become BoneTrailConfig.actor and .bone."),
        ("Effects::QueueActionHitEffect",
         (('arg1', 'hit_result'), ('arg0', 'effect_id'), ('sp10', 'hit_effect_request'), ('temp_s0', 'queued_action')),
         "F00058 Source builds the overlay hit request from current action source, hit target/position and limb; opaque0 overlays request.effectId."),
        ("Effects::QueueActionImpactEffects",
         (('arg0', 'attacker_index'), ('temp_s0', 'queued_action')),
         "F00060 Current action queue drives target selection; arg0 is passed unchanged to Battle_PlayWeaponHitSounds, which resolves the attacker model and weapon."),
        ("Effects::SignalActorEffectAnimation",
         (('arg0', 'actor_index'),),
         "F00064 Passed to actor animation advance or stored as tinted actor and passed to palette actor transform."),
        ("Effects::TintActionActorRed",
         (('arg0', 'actor_index'),),
         "F00068 Passed to actor animation advance or stored as tinted actor and passed to palette actor transform."),
        ("Effects::SpawnGroundPlaneImpactEffect",
         (('arg0', 'world_position_components'),),
         "F00085 Unsigned-short source elements 0 and 2 become target vx/vz; target vy is forced to zero. No signedness or layout change."),
        ("Effects::SpawnTrapTileEffect",
         (('arg0', 'actor_location'),),
         "F00086 Actor-location tile XZ becomes tile-center world coordinates (*128+64), position.vy supplies height; busy bit2 gates effect33."),
        ("Effects::BeginAshleyStandaloneEffect",
         (('arg0', 'effect_id'),),
         "F00119 Assigned to request.effectId before production BeginActionEffect."),
        ("Effects::StartScriptedActorEffect",
         (('arg0', 'effect_id'), ('arg1', 'target_actor_index')),
         "F00238 Becomes targets[0].actor and is excluded from the separately built others list."),
        ("Effects::ReleaseScriptedEffectTrigger",
         (('arg0', 'track_index'),),
         "F00239 Indexes pendingTracks bits; StartEffectTrack multiplies index by 3 for script entry offset."),
        ("Effects::InitializeRoomWeather",
         (('arg0', 'suppress_rain'), ('new_var2', 'room_environment'), ('new_var4', 'selected_environment')),
         "F00638 Only weatherKind==1 is suppressed when arg0 is nonzero; source environment pointers remain distinct bindings including redundant reload."),
        ("Effects::RoomWeatherState",
         (('arg0', 'weather_kind_output'), ('arg1', 'intensity_output')),
         "F00641 Writes roomWeatherKind and weatherIntensity through distinct output pointers."),
        ("Effects::InitializeFallingWeatherParticles",
         (('arg0', 'particle_count'), ('arg1', 'weather_kind'), ('var_s1', 'particle_cursor')),
         "F00648 Count initializes capacity and active count; kind selects weather mode; cursor advances through the particle array."),
        ("Effects::InitializeScreenWeatherParticles",
         (('arg0', 'particle_count'), ('arg1', 'weather_kind'), ('var_s1', 'particle_cursor')),
         "F00649 Count initializes capacity and active count; kind selects weather mode; cursor advances through the particle array."),
        ("Effects::InitializeRisingEmberParticles",
         (('arg0', 'particle_count'), ('arg1', 'weather_kind'), ('var_s1', 'particle_cursor')),
         "F00651 Count initializes capacity and active count; kind selects weather mode; cursor advances through the particle array."),
        ("Effects::ResetRisingEmberParticle",
         (('arg0', 'weather_particle'), ('temp_s0', 'room_environment')),
         "F00652 Mpd environment weatherArea determines XZ ranges; native particle fields set spawn Y and weather-specific state."),
        ("Effects::ResetFallingWeatherParticle",
         (('arg0', 'weather_particle'), ('temp_s0', 'room_environment')),
         "F00656 Mpd environment weatherArea determines XZ ranges; native particle fields set spawn Y and weather-specific state."),
        ("Effects::RecalculateWeatherVelocity",
         (('temp_v1', 'elevation_cosine'), ('var_s0', 'horizontal_direction_product'), ('var_v0', 'direction_component_product')),
         "F00657 Signed intermediate products compute sin/cos heading times magnitude, apply elevation, then vertical sine; +0xFFF before >>12 preserves truncation toward zero."),
        ("Effects::LoadScreenEffectOverlay",
         (('arg0', 'begin_load'), ('new_var', 'overlay_destination_slot')),
         "F01388 Nonzero begins SCREFF2 disc read into Core_overlaySlots[1], zero polls/completes existing request."),
        ("Effects::DrawScreenEffectOverlay",
         (('temp_s0', 'screen_effect_state'),),
         "F01389 Nullable overlay state gates load polling, radial warp, feedback and vignette consumers."),
        ("Effects::IgnoreRuntimeDiagnostic",
         (('arg0', 'diagnostic_code'),),
         "F01419 Call sites pass 0x14 for pool exhaustion and 0x1E for invalid opcode; original empty diagnostic hook remains empty."),
        ("Effects::InitEffectInstance",
         (('arg1', 'effect_instance'), ('arg0', 'spawn_request')),
         "F01422 Spawn-request producer fields parent/priority/scriptOffset/script/token and four attachment references initialize the instance; particles/childCount/frame/renderers/timeline reset. Distinct input/output pointer names preserve aliasing and zeroing."),
        ("Effects::CreatePrimaryEffectInstance",
         (('arg0', 'spawn_request'),),
         "F01423 Incoming spawn request initializes the newly allocated instance and supplies priority for sorted insertion into primaryInstances."),
        ("Effects::DestroyPrimaryEffectInstance",
         (('arg0', 'effect_instance'), ('var_s1', 'renderer_cursor'), ('var_v1', 'instance_cursor'), ('temp_s0', 'next_renderer')),
         "F01424 Unlinks instance; walks renderer list caching next before destroy command4 and detach; particle list returned then instance freed."),
        ("Effects::UpdatePrimaryEffects",
         (('sp38', 'menu_reload_file'), ('temp_s1', 'next_instance'), ('var_s0', 'instance_cursor'), ('var_s2', 'primary_busy')),
         "F01425 Disc request reloads main-menu overlay; update loop caches next before script-driven destruction; busy return tracks phases0..9."),
        ("Effects::UpdateSecondaryEffects",
         (('temp_s4', 'next_instance'), ('var_s1', 'instance_cursor'), ('var_s0', 'renderer_cursor'), ('var_s3', 'secondary_busy_bits'), ('temp_s2', 'next_renderer')),
         "F01426 Current/next pointers kept separate at each list level; ORs busyBits. This names existing post-destroy access without certifying its safety or changing behavior."),
        ("Effects::CanStartEffectTrack",
         (('var_a1', 'can_start_track'),),
         "F01427 Boolean requires phase4/5, unstarted track count and nonzero pending bitmap."),
        ("Effects::PrimaryEffectTrackCount",
         (('var_v1', 'active_track_count'),),
         "F01428 Returns count in phase4/5, otherwise zero."),
        ("Effects::RequeueEffectTrack",
         (('arg0', 'track_index'),),
         "F01429 Indexes pendingTracks bits; StartEffectTrack multiplies index by 3 for script entry offset."),
        ("Effects::StartEffectTrack",
         (('arg0', 'track_index'), ('sp10', 'spawn_request'), ('a1', 'script_bytes')),
         "F01431 Builds primary spawn request from action source/target and package block8 bytecode."),
        ("Effects::UpdateEffectRuntime",
         (('temp_s0', 'secondary_busy_bits'), ('temp_s1', 'primary_busy')),
         "F01432 Switches resource contexts around primary and secondary update then ORs their status."),
        ("Effects::BeginActionEffect",
         (('arg0', 'effect_request'), ('arg1', 'action_variant'), ('arg2', 'variant_argument')),
         "F01433 Copies action request; only nonzero variant with actor source stores both actionVariant and actionVariantArgument."),
        ("Effects::ClearPrimaryEffects",
         (('var_s0', 'instance_cursor'), ('temp_s1', 'next_instance')),
         "F01434 Caches next before freeing timeline and destroying each primary instance."),
        ("Effects::SpawnSecondaryEffects",
         (('arg0', 'effect_request'), ('arg2', 'renderer_parameters')),
         "F01435 Passed through wrapper; child context.parameter is consumed by ScriptAttachRenderer as override."),
        ("Effects::BeginSecondaryEffects",
         (('arg0', 'effect_request'), ('arg1', 'renderer_parameters')),
         "F01436 Calls SpawnSecondaryEffects(request,0,arg1), becoming child context.parameter."),
        ("Effects::SpawnSecondaryEffectRequest",
         (('arg0', 'effect_request'), ('arg2', 'renderer_parameters')),
         "F01437 Passed through wrapper; child context.parameter is consumed by ScriptAttachRenderer as override."),
        ("Effects::SetPrimaryEffectPhase",
         (('arg0', 'effect_phase'),),
         "F01438 Stored in global primaryPhase or selected secondary context.phase, consumed by runtime switch."),
        ("Effects::SetSecondaryEffectPhase",
         (('arg1', 'effect_instance'), ('arg0', 'effect_phase')),
         "F01439 Stored in global primaryPhase or selected secondary context.phase, consumed by runtime switch."),
        ("Effects::AllocateEffectToken",
         (('temp_a1', 'allocated_bits'), ('var_t2', 'allocated_token'), ('var_a2', 'bitmap_base')),
         "F01441 Scans four 32-bit bitmaps bits1..31, marks free bit and encodes (word<<5)+bit. bitmap_base is a distinct unused alias, not a live traversal pointer."),
        ("Effects::ReleaseEffectToken",
         (('arg0', 'effect_token'), ('var_v1', 'bitmap_word_index')),
         "F01442 Signed division correction followed by >>5 computes bitmap word; bit offset clears allocated token."),
        ("Effects::CreateSecondaryEffectInstance",
         (('arg0', 'spawn_request'), ('var_a1', 'instance_cursor'), ('temp_v0', 'new_instance'), ('temp_a0', 'next_instance')),
         "F01443 Allocates and initializes new instance, inserts in priority order, retaining distinct predecessor/successor/new pointers."),
        ("Effects::DestroySecondaryEffectInstance",
         (('arg0', 'effect_instance'), ('var_v1', 'instance_cursor')),
         "F01444 Unlinks secondary instance, returns particles, releases token and frees instance."),
        ("Effects::AttachEffectRenderer",
         (('arg0', 'effect_instance'), ('arg2', 'renderer_parameters'), ('temp_v0', 'new_renderer')),
         "F01445 Allocates slot, links to instance, sets function/state, invokes command1 with packed parameters."),
        ("Effects::DetachEffectRenderer",
         (('arg0', 'effect_instance'), ('arg1', 'renderer_slot'), ('var_v1', 'renderer_cursor')),
         "F01446 Finds predecessor and unlinks slot; owned rendererState and slot are freed."),
        ("Effects::SpawnBoneTrail",
         (('arg0', 'actor_index'), ('arg1', 'bone_index'), ('arg2', 'texture_index'), ('arg3', 'color_index')),
         "F01447 Packed bytes/nibbles match BoneTrailConfig actor/bone/texture/color and life FF; consumer DrawTrailSegment indexes texture and color tables."),
        ("Effects::FadeBoneTrails",
         (('var_s1', 'instance_cursor'), ('var_s0', 'renderer_cursor')),
         "F01448 FadeBoneTrails enumerates every secondary renderer and sends command 3; both formal integers are unused, including the forwarded wrapper inputs."),
        ("Effects::ClampTrailTexturePoint",
         (('arg0', 'u_offset'), ('arg1', 'v_offset'), ('arg2', 'packed_screen_xy'), ('arg3', 'packed_uv_output'), ('var_v1', 'clamped_u'), ('temp_a1', 'clamped_v')),
         "F01451 Adds X/Y offsets to packed screen halves, clamps each to1..254 and packs U|V<<8."),
        ("Effects::MapTrailFramebufferTexture",
         (('arg0', 'trail_packet'), ('arg1', 'u_offset'), ('arg2', 'v_offset'), ('arg3', 'texture_page_flags')),
         "F01452 Finds left screen page, combines page and supplied flags, subtracts left for U offset then clamps four packet UVs."),
        ("Effects::StepParticleEndpointTowardStart",
         (('arg0', 'particle_endpoints'),),
         "F01453 Source compares packed endpoints and changes startX/startY toward the endpoint; no attempted behavioral correction of existing function wording."),
        ("Effects::InterpolateTransformPosition",
         (('arg0', 'transform_definition'), ('arg1', 'transform_motion')),
         "F01463 Definition mode selects saved-versus-world start/end and linear/cosine/curve interpolation; motion state supplies frame, receives position."),
        ("Effects::InterpolateTransformSpline",
         (('arg0', 'transform_definition'), ('arg1', 'transform_motion')),
         "F01464 Definition mode/limit/curve determines Q12 spline argument; motion state's saved/world endpoints and control points produce its position."),
        ("Effects::AdvanceTransformMotion",
         (('arg0', 'transform_definition'), ('arg1', 'transform_motion')),
         "F01465 Definition flags select linear or spline position update, then motion flags/definition limit gate frame increment."),
        ("Effects::BuildTransformSplineControls",
         (('arg0', 'transform_definition'), ('arg1', 'transform_motion')),
         "F01466 Definition radius/jitter/shape fields determine two local control points, transformed along saved/world endpoint direction and stored in motion state. Fixed-point >>12 and radius*32 preserved."),
        ("Effects::BuildCameraAttachment",
         (('arg0', 'effect_instance'), ('arg1', 'transform_motion'), ('arg2', 'offset_toward_camera')),
         "F01467 Nonzero branch displaces camera-lookAt attachment along lookAt-to-camera distance minus 0x600; effect instance itself unused."),
        ("Effects::BuildEffectOriginAttachment",
         (('arg0', 'attachment_matrix'),),
         "F01468 Translation matrix output receives global effectOrigin."),
        ("Effects::BuildAnimatedLocalTransform",
         (('arg0', 'transform_definition'), ('arg2', 'local_transform'), ('arg1', 'rotation_angles')),
         "F01469 Mutable SVECTOR rotation accumulates sampled offsets before RotMatrix; range translation and scaling build local matrix."),
        ("Effects::ResolveTransformAttachment",
         (('arg0', 'effect_instance'), ('arg1', 'transform_motion'), ('sp10', 'actor_placement'), ('sp20', 'bone_length_output')),
         "F01470 ActorAnchorMatrix -> ActorBoneWorldMatrix (original38C1C.c:195-244) writes only the first int of the existing two-int sp20 output as bone length; preserve both-int allocation and unused tail. ActorPlacement supplies location.position.pad copied to saved.pad."),
        ("Effects::UpdateEffectTransforms",
         (('arg0', 'effect_instance'),),
         "F01471 Effect instance owns transforms; package definitions drive local animation, target/attachment resolution, spline controls, motion, world/view composition and optional actor motion. Parameter identifies owner, not one transform."),
        ("Effects::SampleAllEffectCurves",
         (('arg0', 'sample_distance'), ('temp_a2', 'curve_block')),
         "F01472 Each curve indexed by sample distance modulo its own size; source writes curveSamples[i+1]."),
        ("Effects::SampleSegmentedEffectCurve",
         (('arg0', 'curve_index'), ('arg1', 'segment_distance')),
         "F01474 Index selects block11 offset; distance is repeatedly reduced by segment durations and used in linear segment interpolation, not a byte offset."),
        ("Effects::SegmentedEffectCurveDuration",
         (('arg0', 'curve_index'),),
         "F01475 Selects block11 offset and sums segment durations until zero terminator, then adds one."),
        ("Effects::CaptureEffectCamera",
         (('arg0', 'camera_state'),),
         "F01490 Destination camera receives Field camera position/lookAt/roll and Render near/projection/far clip values."),
        ("Effects::EnableCameraShakeChannels",
         (('arg0', 'channel_mask'),),
         "F01492 Bits 0..3 select lateral/vertical/depth/roll shake channels; zero disable mask clears all."),
        ("Effects::DisableCameraShakeChannels",
         (('arg0', 'channel_mask'),),
         "F01493 Bits 0..3 select lateral/vertical/depth/roll shake channels; zero disable mask clears all."),
        ("Effects::SetEffectCameraTransitionMode",
         (('arg0', 'transition_mode'),),
         "F01494 Stored in cameraTransitionMode and dispatched by UpdateEffectCameraMotion as 0/1/2/3."),
        ("Effects::CameraShakePhaseStep",
         (('arg0', 'shake_channel'), ('var_v0', 'phase_step')),
         "F01495 Either samples stepCurve at channel phase or returns its literal value."),
        ("Effects::RandomCameraShakePhaseStep",
         (('arg0', 'shake_channel'), ('var_s0', 'random_step_limit')),
         "F01496 Curve-or-literal limit is zero-guarded then used as rand modulo divisor; exclusive random upper bound."),
        ("Effects::StartCameraShakeChannels",
         (('arg0', 'channel_mask'), ('arg1', 'shape_curve'), ('arg2', 'amplitude'), ('arg3', 'step_uses_curve'), ('arg4', 'step_curve_or_value'), ('arg5', 'random_step_uses_curve'), ('arg6', 'random_step_curve_or_value')),
         "F01497 Each selected channel receives these seven values; phase reset and random initial angle use the two curve/literal selectors. UpdateCameraShakeChannels proves amplitude scaling; no new units are inferred."),
        ("Effects::StopCameraShakeChannels",
         (('arg0', 'channel_mask'),),
         "F01498 Bits 0..3 select lateral/vertical/depth/roll shake channels; zero disable mask clears all."),
        ("Effects::UpdateEffectCameraMotion",
         (('temp_s0', 'had_camera_transition'),),
         "F01503 Snapshots nonzero transition mode before ticking; forces final camera application even if transition ends this tick."),
        ("Effects::AllocateEffectParticle",
         (('arg0', 'effect_instance'),),
         "F01507 Effect instance receives popped free-list particle at particles head and incremented particleCount; pool counter and texture reset unchanged."),
        ("Effects::ReleaseEffectParticle",
         (('arg1', 'effect_instance'), ('arg0', 'particle_node')),
         "F01508 Particle node is detached through previous/next; instance parameter fixes head and count. Node returns to global free list."),
        ("Effects::ReleaseAllEffectParticles",
         (('arg0', 'effect_instance'), ('temp_a1', 'particle_list_head'), ('var_a0', 'particle_list_tail')),
         "F01509 Walk to tail, splice existing free list, set free head and subtract the instance particle count."),
        ("Effects::SetParticleSpawnPosition",
         (('arg1', 'particle_node'), ('arg0', 'emitter_scratch')),
         "F01514 Emitter scratch vector is optionally rotated into aimPoint; node position receives scratch.position plus aimPoint*ONE. Q12 scaling preserved."),
        ("Effects::EmitParticleEvent",
         (('arg0', 'effect_instance'), ('arg2', 'transparency_override'), ('arg3', 'particle_variant')),
         "F01515 Nonzero arg2 overrides selected transparency; arg3 defaults from parent variant low nibble then becomes node opaque61Bit0. No interpretation of variant values beyond that proven propagation."),
        ("Effects::BranchEffectScriptIf",
         (('arg1', 'effect_instance'), ('arg0', 'condition')),
         "F01516 True consumes branch halfword as script offset; false skips two operand bytes."),
        ("Effects::YieldEffectScriptAtOffset",
         (('arg0', 'effect_instance'),),
         "F01517 Reads branch halfword from instance bytecode, assigns scriptOffset, returns0 to yield."),
        ("Effects::JumpEffectScript",
         (('arg0', 'effect_instance'),),
         "F01518 Reads branch halfword from instance bytecode, assigns scriptOffset, returns1 to continue."),
        ("Effects::SpawnEffectScript",
         (('arg0', 'effect_instance'), ('sp10', 'spawn_request')),
         "F01519 Copies script and source into spawn request and reads new entry offset from bytecode."),
        ("Effects::EndEffectScript",
         (('arg0', 'effect_instance'),),
         "F01520 Formal retains EffectInstance type and callback slot although unused; returns2 consumed by RunEffectScript as completion. Name conveys the callback object, not an access claim."),
        ("Effects::SetEffectScriptSelector",
         (('arg0', 'effect_instance'),),
         "F01521 Consumes one byte from instance bytecode into global scriptSelector."),
        ("Effects::ScriptAttachRenderer",
         (('arg0', 'effect_instance'), ('temp_s0', 'renderer_parameters'), ('temp_s2', 'renderer_index')),
         "F01522 Reads renderer table byte then low/high halfwords; context.parameter can replace packed value before attach."),
        ("Effects::BranchIfEffectRenderersFinished",
         (('arg0', 'effect_instance'),),
         "F01523 Tests instance renderer-list nullness and passes same instance to conditional bytecode branch."),
        ("Effects::LoadEffectTimelineImmediate",
         (('arg0', 'effect_instance'),),
         "F01524 Consumes timeline byte, invokes LoadEffectTimeline(instance,index,0), then sets this instance scriptOffset0xB7."),
        ("Effects::BranchIfActionVariantCleared",
         (('arg0', 'effect_instance'),),
         "F01525 Tests global actionVariant zero but branches through the passed instance's bytecode operand."),
        ("Effects::TickEffectRenderers",
         (('arg0', 'effect_instance'), ('var_s0', 'renderer_cursor'), ('s1', 'next_renderer')),
         "F01526 Caches next before command2 may detach current slot; not an instance next pointer."),
        ("Effects::TickEffectTransforms",
         (('arg0', 'effect_instance'),),
         "F01527 Forwards same instance to UpdateEffectTransforms, preserving opcode return1."),
        ("Effects::SetActionEffectVerticalOffset",
         (('arg0', 'effect_instance'),),
         "F01528 Uses instance effectId/register3 to select +/-0x80 transform Y offsets for all package transforms."),
        ("Effects::InitializeScriptEffectTransforms",
         (('arg0', 'effect_instance'),),
         "F01529 Forwards opcode instance to InitializeTransformStates and returns1."),
        ("Effects::LoadActionVariantRegister",
         (('arg0', 'effect_instance'),),
         "F01530 Global actionVariant<4 selects value copied to instance register3."),
        ("Effects::BranchIfParticleCountEqual",
         (('arg0', 'effect_instance'),),
         "F01533 Compares instance particleCount to consumed byte and conditionally branches same instance."),
        ("Effects::EmitParticlesFromScriptRegister",
         (('arg0', 'effect_instance'),),
         "F01534 Bytecode byte selects an instance register; its value becomes particle definition/event index with zero overrides."),
        ("Effects::TickEffectParticles",
         (('arg0', 'effect_instance'),),
         "F01535 Walks instance particle list caching next, invokes owned particle renderer, emits child events using parent motion/context, and releases completed particles through the same instance."),
        ("Effects::RefreshScriptParticleEnvironment",
         (('arg0', 'effect_instance'),),
         "F01536 Unused EffectInstance callback formal preserved; global package gravity/drag refresh and return1 unchanged."),
        ("Effects::SignalSourceActorFromScript",
         (('arg0', 'effect_instance'),),
         "F01537 Actor-typed instance source becomes animation-advance actor index."),
        ("Effects::ClearScriptActionVariant",
         (('arg0', 'effect_instance'),),
         "F01538 Unused EffectInstance callback formal preserved; only global actionVariant is cleared and return1 supplied."),
        ("Effects::ContinueEffectScript",
         (('arg0', 'effect_instance'),),
         "F01539 Unused EffectInstance callback formal preserved; constant return1 is VM continue status."),
        ("Effects::LoadEffectTimelineFromRegister",
         (('arg0', 'effect_instance'),),
         "F01540 Instance register selects timeline; root instance optionally remaps through block9 timelineIds before loader call. Parent/null and table bounds retain current behavior."),
        ("Effects::TickEffectTimelineFromScript",
         (('arg0', 'effect_instance'),),
         "F01541 Reads halfword from bytecode and forwards it as child script entry offset to TickEffectTimeline."),
        ("Effects::BranchIfEffectTimelineFinished",
         (('arg0', 'effect_instance'),),
         "F01542 Tests instance timeline activeTracks zero then consumes conditional branch operand from same instance."),
        ("Effects::FreeEffectScriptTimeline",
         (('arg0', 'effect_instance'),),
         "F01543 Frees nullable instance timeline and clears pointer, retaining opcode return1."),
        ("Effects::SetParticleColorCurvesFromScript",
         (('arg0', 'effect_instance'),),
         "F01544 Consumes three packed operand bytes through instance stream and writes selected particle definition R/G/B curve fields."),
        ("Effects::ApplyPairedParticleColorPreset",
         (('arg0', 'effect_instance'),),
         "F01545 Instance target colorPreset indexes paired color curves for definitions0/1."),
        ("Effects::CompleteChildEffectScript",
         (('arg0', 'effect_instance'),),
         "F01546 Instance parent, if nonnull, has childCount decremented; the formal is not itself the parent."),
        ("Effects::ApplyParticleColorPresetTable",
         (('arg0', 'effect_instance'),),
         "F01547 Instance target colorPreset selects five definition IDs and their RGB curve presets."),
        ("Effects::ReportInvalidEffectInstruction",
         (('arg0', 'effect_instance'),),
         "F01548 Unused EffectInstance callback formal preserved; invokes existing diagnostic hook with0x1E."),
        ("Effects::ReadScriptByte",
         (('arg0', 'effect_instance'),),
         "F01549 Returns instance script[scriptOffset++] byte."),
        ("Effects::ReadScriptHalfword",
         (('arg0', 'effect_instance'),),
         "F01550 Reads low then high bytes from same instance and combines lo|(hi<<8); endian and advancement preserved."),
        ("Effects::RunEffectScript",
         (('arg0', 'effect_instance'),),
         "F01551 Opcode byte indexes callback table, callback receives same instance; status1 loops,2 completes,other yields."),
        ("Effects::SetParticleGravity",
         (('arg0', 'gravity_vector'),),
         "F01552 Copies pointed gravity vector into global particleGravity; no component reinterpretation or unit conversion."),
        ("Effects::SetParticleDrag",
         (('arg0', 'drag_value'),),
         "F01553 Copies pointed integer to global particleDrag."),
        ("Effects::LoadEffectTimeline",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_index')),
         "F01557 Index selects PFile timeline section and each track data; initializes header/key index/timer and active track bitmap."),
        ("Effects::InitializeParticleTimelineKey",
         (('arg0', 'timeline_track'),),
         "F01558 Track keyIndex selects particle key; keyParameter receives opaque2Bit4 and keyTimer receives duration."),
        ("Effects::InitializeShakeTimelineKey",
         (('arg0', 'timeline_track'),),
         "F01559 Track's indexed shake curve drives segmented-curve duration stored in keyTimer."),
        ("Effects::AdvanceEffectTimelineKey",
         (('arg0', 'timeline_track'),),
         "F01560 Bounded track keyIndex advances; kind1 and kind13 initialize particle/shake key state; zero means exhausted."),
        ("Effects::TickParticleEmissionTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track')),
         "F01561 Instance timeline frame gates current particle-track key; curveFrame samples all curves, packed key fields feed EmitParticleEvent; hold flag gates timer decrement and next-key advance."),
        ("Effects::TickEffectSoundTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track')),
         "F01562 Instance frame gates current track word; packed mode chooses stream/centered/world sound and optional instance transform position; next-key advance uses same track."),
        ("Effects::TickChildEffectTrack",
         (('arg0', 'effect_instance'), ('arg1', 'child_track'), ('arg2', 'child_script_offset')),
         "F01563 Child spawn scriptOffset is taken from the third dispatcher argument and used for each target/final child."),
        ("Effects::TickEffectRendererTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track'), ('temp_a3', 'renderer_key')),
         "F01564 Current eight-byte key frame and renderer index/packed arguments drive AttachEffectRenderer."),
        ("Effects::TickSourceActorColorTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track'), ('temp_s0', 'color_key')),
         "F01566 Current key frame, mode/duration and centered RGB feed source actor color transform."),
        ("Effects::TickTargetActorColorTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track'), ('arg2', 'apply_to_all_targets')),
         "F01567 Nonzero iterates actionRequest targets, zero uses this instance emitTo only."),
        ("Effects::TickOtherActorColorTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track'), ('temp_s2', 'color_key')),
         "F01568 Current key drives each actor from actionRequest.others list."),
        ("Effects::TickSceneColorTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track'), ('temp_a3', 'color_key')),
         "F01569 Current frame key's centered RGB/mode/duration passed to scene or background palette transform."),
        ("Effects::TickBackgroundColorTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track'), ('temp_a3', 'color_key')),
         "F01570 Current frame key's centered RGB/mode/duration passed to scene or background palette transform."),
        ("Effects::ApplyTargetActionImpact",
         (('arg0', 'effect_instance'),),
         "F01571 Actor target passes actor index and null position; position target passes pad index and address to Battle target-effect consumer."),
        ("Effects::ApplySourceActionImpact",
         (('arg0', 'effect_instance'),),
         "F01572 Source actor/position tag selects arguments to Battle source-effect consumer."),
        ("Effects::ActivateTargetRoomTile",
         (('arg0', 'effect_instance'),),
         "F01573 Position-tagged instance target supplies world position to trap disable consumer."),
        ("Effects::SpawnTransformWeaponTrail",
         (('arg0', 'effect_instance'), ('arg1', 'transform_index'), ('arg2', 'packed_trail_style')),
         "F01574 Selected transform supplies actor/bone; decimal remainder and quotient produce texture index and color index."),
        ("Effects::ResumeEffectTransformMotion",
         (('arg0', 'transform_controls'), ('arg1', 'transform_index')),
         "F01576 Advances existing transform-control pointer by index and updates motionResumed or attachmentMode. Does not repair or reinterpret caller cast."),
        ("Effects::HoldEffectTimelineFrame",
         (('arg0', 'effect_instance'),),
         "F01577 Sets instance timeline flags bit1 consumed by timer/frame advancement."),
        ("Effects::SignalSourceActorFromTimeline",
         (('arg0', 'effect_instance'),),
         "F01579 Instance source actor index passed to animation-advance signal, same contract as script opcode."),
        ("Effects::InvalidateEffectTransformAttachments",
         (('arg0', 'transform_controls'), ('arg1', 'transform_index')),
         "F01580 Advances existing transform-control pointer by index and updates motionResumed or attachmentMode. Does not repair or reinterpret caller cast."),
        ("Effects::SetSourceWeaponBrightness",
         (('arg0', 'effect_instance'), ('arg1', 'brightness_override')),
         "F01581 Nonzero enables explicit brightness, zero clears override."),
        ("Effects::PlaySourceActorReactionSound",
         (('arg0', 'effect_instance'), ('arg1', 'reaction_sound_variant')),
         "F01582 Values<4 forwarded directly; larger values select nonrepeating random0..2."),
        ("Effects::ActivateTargetGimmick",
         (('arg0', 'effect_instance'),),
         "F01583 Position-tagged instance target supplies world position to room-object unlock consumer."),
        ("Effects::FocusCameraOnEffectTransform",
         (('arg0', 'effect_instance'), ('arg1', 'transform_index'), ('arg2', 'duration_frames')),
         "F01584 Reads selected world transform translation, multiplies by ONE, sets motion duration (zero coerced to one), chooses cosine mode."),
        ("Effects::RestoreActionEffectCamera",
         (('arg0', 'effect_instance'), ('arg1', 'duration_frames')),
         "F01585 Restores saved end pose; zero duration coerced to one. Effect-instance slot is unused but part of the action-control call shape."),
        ("Effects::PrepareAshleyTargetImpact",
         (('arg0', 'effect_instance'),),
         "F01586 Actor-tagged emitTo index opens defense-ability timing window; no assumption that all callers are Ashley."),
        ("Effects::TickActionControlTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track'), ('temp_a2', 'control_key'), ('temp_a1_2', 'control_operand0'), ('temp_a2_2', 'control_operand1')),
         "F01587 Current key low9 frame bits gate action, high bits dispatch opcode. Operand bytes have opcode-dependent transform/brightness/sound/duration/style roles; names intentionally describe bytecode operands rather than falsely one gameplay role."),
        ("Effects::TickCameraShakeTrack",
         (('arg0', 'effect_instance'), ('arg1', 'timeline_track')),
         "F01588 Instance timeline frame gates track key; channel mask/curve/amplitude/phase-step fields feed StartCameraShakeChannels; timer counts to next key."),
        ("Effects::TickEffectTimeline",
         (('arg0', 'effect_instance'), ('arg1', 'child_script_offset'), ('var_s1', 'track_index'), ('temp_a1', 'timeline_track')),
         "F01589 Active tracks dispatch by kind; inner temp_a1 is current track. Unused outer temp_a1 and temp_a0 are separately accounted for."),
        ("Effects::SelectEffectResourceContext",
         (('arg0', 'use_primary_context'),),
         "F01596 Zero selects secondaryPackage; nonzero selects actionPackage."),
        ("Effects::UpdateParticleGravityAndDrag",
         (('sp10', 'sampled_gravity'), ('sp20', 'sampled_drag')),
         "F01600 Interpolate package gravity/drag ranges using their curve samples, then copy to runtime gravity/drag setters; fixed-point values preserved."),
        ("Effects::BindEffectPackageParameters",
         (('arg0', 'package_parameters'),),
         "F01604 Incoming block9 structure pointer becomes package block9Data; gravity/drag, transforms and timeline consumers read this same block."),
        ("Effects::BindEffectTransformDefinitions",
         (('arg0', 'block_data'),),
         "F01605 Stores the incoming block pointer in package block10Data or block11Data respectively; no new format interpretation."),
        ("Effects::ParticleAnimationData",
         (('arg0', 'animation_index'),),
         "F01608 Selects block2 halfword offset at index+2 and adds it to block base."),
        ("Effects::BindEffectSoundSequences",
         (('arg0', 'block_data'),),
         "F01609 Stores the incoming block pointer in package block10Data or block11Data respectively; no new format interpretation."),
        ("Effects::BindEffectSoundBank",
         (('arg0', 'sound_bank_data'),),
         "F01610 Stored as block12Data and passed to Audio_BindRoomSfxBank."),
        ("Effects::BindEffectAuxiliarySoundBank",
         (('arg0', 'stream_data'),),
         "F01611 Stored as block13Data and supplied with unchanged byte_count to owned cutscene stream binding."),
        ("Effects::UpdateEffectTextureLoading",
         (('sp50', 'upload_rectangle'), ('temp_s1', 'load_state'), ('var_s0', 'loading_pending')),
         "F01616 RECT describes texture page or CLUT upload; load-state snapshot controls poll/upload/free; return stays nonzero through final upload then zero on cleanup."),
        ("Effects::SetEffectTextureAssetCount",
         (('arg0', 'asset_count'),),
         "F01621 Stored in textureLoads.assetCount, compared against completed load index."),
        ("Effects::SetEffectTextureLoadState",
         (('arg0', 'load_state'),),
         "F01623 Sets the respective loader state consumed by its switch, no enum/signature change."),
        ("Effects::SetEffectPluginLoadState",
         (('arg0', 'load_state'),),
         "F01625 Sets the respective loader state consumed by its switch, no enum/signature change."),
        ("Effects::BindEffectPackageBlocks",
         (('arg0', 'package_block'),),
         "F01626 Cursor walks meta low16 byte sizes and dispatches high16 block tags to typed binders; texture/stream bounded byte counts remain native adaptations, no pointer-width/layout change."),
        ("Effects::Templates::ClosedRing",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('speed', 'angular_step')),
         "F01861 Three-row closed ring: allocate state, sample ranges, increment angle, project ring and emit textured packets; each macro instance keeps its color table. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\closedRings.h:41; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::Grid",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters')),
         "F01862 Nine-by-nine deforming grid: allocate state, sample extents/forces and emit projected textured cells; lifecycle values select allocation/update/expiration. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\grids.h:23; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::Mesh10",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters')),
         "F01863 Ten-vertex mesh: allocate state and retain per-instance vertex binding; definition/lifetime packed parameters drive update. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\meshes10.h:29; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::MirrorSurface",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters')),
         "F01864 Mirrored surface renderer: definition/lifetime prefix and mirrored vertex layout remain unchanged; concrete vertex-table macro retained. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\mirroredSurfaces.h:34; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::ParticleDispatch",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('a2', 'emitter_scratch'), ('s3', 'renderer_alive'), ('temp_s0', 'effect_instance'), ('temp_s1', 'particle_set'), ('temp_s2', 'particle_definition'), ('var_a0', 'control_point_capacity'), ('var_v0', 'max_interval_endpoint')),
         "F01865 Allocates particle-set prefix (untextured) or full set, allocates <=8 four-point splines, then calls Origin/Target/Control/Render with definition,scratch,currentInstance,state; destruction frees points. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\particleDispatch.h:3; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::ParticleLineRender",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'particle_set')),
         "F01866 Uses same definition/scratch/instance/control-point argument order but emits untextured line packets; dispatch VS_PARTICLE_UNTEXTURED determines state prefix allocation. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\particleLineRender.h:13; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::ParticleRender",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'particle_set')),
         "F01867 Uses definition color/width curves, emitter transform flags, current instance view and particle control-point set to emit textured spline strips. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\particleRender.h:17; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::ParticleOrigin",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'particle_set'), ('sp10', 'ring_edge_start_x'), ('temp_a1', 'next_corner_index'), ('temp_lo', 'ring_angle_step'), ('temp_s3', 'ring_x'), ('temp_v0', 'random_angle'), ('temp_v0_4', 'edge_interpolation_q12'), ('var_v1', 'corner_y_bits'), ('var_v1_2', 'corner_z_bits'), ('temp_s0', 'emitter_world_matrix'), ('temp_s1', 'origin_control_point'), ('var_fp', 'ring_edge_start_z'), ('var_a0_2', 'corner_x_bits')),
         "F01868 Samples emitter origin/extent; chooses sphere/box/ring origins and writes first point of each four-point control array; current instance supplies attachment matrix. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\particleSetup.h:36; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::ParticleTarget",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'particle_set'), ('temp_s0', 'target_control_point')),
         "F01869 Samples attraction offset/jitter and optional target transform; writes fourth spline point for every particle control array. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\particleSetup.h:262; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::ParticleControl",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'particle_set'), ('sp10', 'first_local_control'), ('sp18', 'second_local_control'), ('temp_s0', 'spline_span_length'), ('temp_s5', 'spline_control_points'), ('var_s0', 'first_control_angle'), ('var_s1', 'second_control_angle'), ('var_s3', 'first_control_radius'), ('var_s4', 'second_control_radius'), ('var_v0_2', 'first_longitudinal_factor_256'), ('var_v0_3', 'second_longitudinal_factor_256')),
         "F01870 Uses first/fourth spline points to form look-at basis; sampled longitudinal factors/angles/radii create second/third spline points; instance formal unused. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\particleSetup.h:298; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::Ring",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('speed', 'angular_step')),
         "F01871 Ring renderer: random angular step added to prior angle; definition/lifetime prefix and texture mapping preserved. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\rings.h:26; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::RotatingSurface",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('speed', 'angular_step')),
         "F01872 Surface renderer: sampled angular step updates state.angle before vertex rotation; instance vertices remain bound by macro. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\rotatingSurfaces.h:34; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::Surface",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters')),
         "F01873 Surface renderer: definition/lifetime prefix and per-instance surface vertices; no shared storage or callback change. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\surfaces.h:34; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TiledRing",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('speed', 'angular_step')),
         "F01874 Tiled ring: sampled angular step updates ring angle; lifetime/definition packed prefix, tiled texture contract retained. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\tiledRings.h:35; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailOrigin",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set'), ('sp10', 'ring_edge_start_x'), ('temp_a1', 'next_corner_index'), ('temp_lo', 'ring_angle_step'), ('temp_s3', 'ring_x'), ('temp_v0', 'random_angle'), ('temp_v0_4', 'edge_interpolation_q12'), ('var_v1', 'corner_y_bits'), ('var_v1_2', 'corner_z_bits'), ('sp14', 'ring_edge_start_z'), ('var_a0', 'corner_x_bits')),
         "F01875 Five-sample trail origins are first spline points; sphere/box/ring geometry uses per-plugin emission cursor and effect-instance transform. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails5.h:27; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailTarget",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set')),
         "F01876 Five-sample trail target is fourth spline point from attraction offset/jitter, optional transform or action position. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails5.h:243; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailControl",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set'), ('sp10', 'first_local_control'), ('sp18', 'second_local_control'), ('temp_s0', 'spline_span_length'), ('temp_s5', 'spline_control_points'), ('var_s0', 'first_control_angle'), ('var_s1', 'second_control_angle'), ('var_s3', 'first_control_radius'), ('var_s4', 'second_control_radius'), ('var_v0_2', 'first_longitudinal_factor_256'), ('var_v0_3', 'second_longitudinal_factor_256')),
         "F01877 Five-sample middle control points built in first-to-fourth look-at basis; sampled /256 longitudinal scales and independent radii/angles preserved. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails5.h:278; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailInit",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set')),
         "F01878 Five-sample trail reset: stepCount random range (zero->1), sample count1/collapse4/index0, origin sample, zero progress and high-resolution spline speed. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails5.h:380; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailUpdate",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set'), ('temp_s0', 'trail')),
         "F01879 Five-sample circular history: collapse after stepCount, otherwise update spline progress/speed then sample with (progress>>12)-ONE. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails5.h:407; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailRender",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set'), ('sp10', 'strip_packet'), ('sp50', 'screen_points'), ('sp78', 'projected_depths'), ('spA0', 'segment_normal_angles'), ('spC0', 'sampled_color'), ('spF0', 'screen_point_base'), ('spF8', 'texture_v_start'), ('spFC', 'texture_v_end'), ('temp_a1_2', 'normal_offset_y'), ('temp_a3', 'output_packet'), ('temp_s1', 'trail'), ('temp_s7', 'texture_frame'), ('var_s0_2', 'joint_normal_angle')),
         "F01880 Five-sample renderer projects history, constructs normal-offset strip vertices, modulates color by collapse samples and emits GT4 packets; preserved depth/coordinate mixed register reuse. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails5.h:449; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailDispatch",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('sp18', 'child_particle_context'), ('s4', 'emitter_scratch'), ('temp_s0', 'trail_set'), ('temp_s3', 'particle_definition'), ('temp_s5', 'effect_instance'), ('temp_v0', 'new_trail_set'), ('var_s6', 'renderer_alive')),
         "F01881 Five-sample dispatcher allocates <=16 trails, initializes curve samples5 and calls six stages with definition,scratch,instance,trail set; emits child particles from current spline sample and frees trail storage. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails5.h:677; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailNineOrigin",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set'), ('sp10', 'ring_edge_start_x'), ('temp_a1', 'next_corner_index'), ('temp_lo', 'ring_angle_step'), ('temp_s3', 'ring_x'), ('temp_v0', 'random_angle'), ('temp_v0_4', 'edge_interpolation_q12'), ('var_v1', 'corner_y_bits'), ('var_v1_2', 'corner_z_bits'), ('sp14', 'ring_edge_start_z'), ('var_a0', 'corner_x_bits')),
         "F01882 Nine-sample trail origins independently checked against trails9.h; same origin geometry, separate trail9 type/capacity and plugin emission cursor. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails9.h:27; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailNineTarget",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set')),
         "F01883 Nine-sample fourth control point uses independently typed trails9 state and same offset/jitter/transform rules. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails9.h:243; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailNineControl",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set'), ('sp10', 'first_local_control'), ('sp18', 'second_local_control'), ('temp_s0', 'spline_span_length'), ('temp_s5', 'spline_control_points'), ('var_s0', 'first_control_angle'), ('var_s1', 'second_control_angle'), ('var_s3', 'first_control_radius'), ('var_s4', 'second_control_radius'), ('var_v0_2', 'first_longitudinal_factor_256'), ('var_v0_3', 'second_longitudinal_factor_256')),
         "F01884 Nine-sample middle control-point math independently checked, including unused instance formal and do-while wrapper; no storage/type substitution. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails9.h:278; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailNineInit",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set')),
         "F01885 Nine-sample reset independently checked: sample count1/collapse8/index0, origin sample, zero progress and high-resolution spline speed. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails9.h:387; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailNineUpdate",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set'), ('temp_s0', 'trail')),
         "F01886 Nine-sample circular history independently checked: modulo9/cap9 and collapse8; same high-resolution progress recurrence. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails9.h:414; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailNineRender",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg0', 'particle_definition'), ('arg3', 'trail_set'), ('sp10', 'strip_packet'), ('sp50', 'screen_points'), ('sp78', 'projected_depths'), ('spA0', 'segment_normal_angles'), ('spC0', 'sampled_color'), ('spF0', 'screen_point_base'), ('spF8', 'texture_v_start'), ('spFC', 'texture_v_end'), ('temp_a1_2', 'normal_offset_y'), ('temp_a3', 'output_packet'), ('temp_s1', 'trail'), ('temp_s7', 'texture_frame'), ('var_s0_2', 'joint_normal_angle')),
         "F01887 Nine-sample renderer independently checked: arrays10/8, sample traversal+8 modulo9, texture interpolation divisor8; GT4 SDK fields preserved. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails9.h:456; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::Templates::TrailNineDispatch",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('sp18', 'child_particle_context'), ('s4', 'emitter_scratch'), ('temp_s0', 'trail_set'), ('temp_s3', 'particle_definition'), ('temp_s5', 'effect_instance'), ('temp_v0', 'new_trail_set'), ('s6', 'renderer_alive')),
         "F01888 Nine-sample dispatcher independently checked: <=8 trails, samples9 and native trail9 type; same callback stage order, distinct plugin cursor. Shared body provenance resources\\rood-reverseWin\\src\\EFFECT\\trails9.h:685; concrete macro bindings recorded separately, no assumption that copier owns owned instantiations."),
        ("Effects::ConfigureTextureLoads",
         (('a2', 'clut_slot_entry'), ('new_var', 'uses_texture_assets'), ('var_s2', 'first_texture_asset')),
         "F01891 Prefix sum of prior FBT counts selects current FBT records; FBC slot entry configures CLUT request, usesTextures controls additional counted asset."),
        ("Effects::Plg005::UpdateAnalyzeGrid",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('sp5A0', 'effect_instance'), ('s6', 'emitter_scratch'), ('s7', 'particle_definition'), ('var_a0', 'texture_axis_origin'), ('var_a1', 'z_spacing'), ('var_a2', 'x_spacing'), ('var_a3', 'z_spacing_increment'), ('var_t0', 'x_spacing_increment')),
         "F01893 Renderer slot receives allocated grid state on command1; parameters encode lifetime/definition. Spacing increments expand symmetric grid arrays; texture_axis_origin is used sequentially for U then V, not a position. Commands2/3/4 update/expire/finish."),
        ("Effects::Plg052::UpdateRepeatingRing",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('speed', 'angular_step')),
         "F01920 Each independently inspected ring renderer stores parameter low byte lifetime and upper byte definition; random sampled speed is added to state.angle before Sin/Cos. This is angular increment, not translation speed. Packet v0..v3 remain SDK V-coordinate members. Native provenance explicitly takes C from PLG077 func_800FA324 (F01998, src/EFFECT/PLG077.BIN/18.c:37); that original C body and angular-step assignment at148-162 were read. PLG052 INCLUDE_ASM declaration is not misrepresented as a readable C body or freshly proven binary equivalence."),
        ("Effects::Plg128::BuildCameraOffsetTransform",
         (('arg0', 'camera_effect'), ('arg1', 'translation')),
         "F02064 Builds look-at orientation from camera state and writes input VECTOR XYZ to matrix translation."),
        ("Effects::Plg128::ResolveCameraEndpoints",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg3', 'particle_definition'), ('arg0', 'camera_effect')),
         "F02065 Particle definition indexes source/attraction transforms in current instance; scratch position/attraction offsets form camera-effect lookAt/eye. Case flags0..2/0x80/0xC0 retain actual source conventions."),
        ("Effects::Plg128::ResolveActorFacingCamera",
         (('arg2', 'effect_instance'), ('arg1', 'emitter_scratch'), ('arg3', 'particle_definition'), ('arg0', 'camera_effect')),
         "F02066 Definition transform selects instance placement/facing; scratch position and attractionJitter.vx yield effect eye and sin/cos-offset lookAt. No guessed eye/lookAt reversal fix."),
        ("Effects::Plg128::StoreCameraEndpoints",
         (('arg0', 'camera_effect'),),
         "F02067 Effect state's lookAt becomes endCamera.position<<12 while eye becomes endCamera.lookAt<<12, exactly as original; scratch formal already named and unused."),
        ("Effects::Plg128::UpdateEffectCamera",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters')),
         "F02068 Low byte lifetime/duration, next byte definition, upper bits camera mode; command2 reads curves and camera endpoint helpers, without changing fixed-point shifts."),
        ("Effects::Plg162::UpdateTransposedRing",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('speed', 'angular_step')),
         "F02209 Each independently inspected ring renderer stores parameter low byte lifetime and upper byte definition; random sampled speed is added to state.angle before Sin/Cos. This is angular increment, not translation speed. Packet v0..v3 remain SDK V-coordinate members."),
        ("Effects::Plg300::UpdateForegroundRepeatingRing",
         (('arg0', 'renderer_slot'), ('arg1', 'lifecycle_command'), ('arg2', 'packed_parameters'), ('speed', 'angular_step')),
         "F02398 Each independently inspected ring renderer stores parameter low byte lifetime and upper byte definition; random sampled speed is added to state.angle before Sin/Cos. This is angular increment, not translation speed. Packet v0..v3 remain SDK V-coordinate members."),
        ("Effects::ScreenDistortion::SetFeedbackLayer",
         (('arg0', 'ordering_layer'),),
         "F02455 Stored as feedbackLayer, used as ordering-table index minus3; zero releases texture reservation."),
        ("Effects::ScreenDistortion::DrawFeedbackTiles",
         (('temp_v0', 'texture_page_bits'), ('var_s2', 'tile_strip_x'), ('temp_s4', 'ordering_table'), ('var_t2', 'packet_words'), ('s5', 'texture_page_mode')),
         "F02456 Strip loop X coordinate determines framebuffer texture page; packet_words successively refers to sprite, draw-area and reset packets."),
        ("Effects::ScreenDistortion::SetRadialWarp",
         (('arg0', 'initial_scale_q12'), ('arg1', 'radial_growth_q12')),
         "F02457 Original 3D8.s loads offset4 unsigned then <<4 for scale seed; offset6 signed is added to0x1000 and multiplied into each next radius scale with >>12. Retains incoming short types and wrap behavior."),
        ("Effects::ScreenDistortion::SetVignetteEnabled",
         (('arg0', 'enabled'),),
         "F02459 Stored in vignetteEnabled gate used by DrawScreenEffectOverlay."),
        ("Effects::ScreenDistortion::EllipsePoint",
         (('arg0', 'angular_step'), ('arg1', 'radius'), ('var_s1', 'radial_modulation_q6')),
         "F02460 64 angular steps per revolution (<<6); modulation=64+abs(8-((step+8)&15)); returns packed XY centered160,112."),
        ("Effects::ScreenDistortion::AppendVignetteCorner",
         (('arg0', 'packed_corner_xy'), ('arg1', 'first_angular_step'), ('arg2', 'packet_words'), ('arg3', 'ordering_head'), ('var_s2', 'angular_step')),
         "F02461 Writes four corner-fan triangles from fixed corner to adjacent ellipse points and advances packet output."),
        ("Effects::ScreenDistortion::DrawVignette",
         (('var_s2', 'packet_words'), ('temp_s4', 'overlay_ordering_table')),
         "F02462 Four corner fans and64 rim quads linked into overlay table; packet cursor advances by exact word counts."),
        ("Effects::InitializeTransformStates",
         (('arg0', 'effect_instance'),),
         "F04134 Resets each transform state from block10 definition and initializes source/target attachment references from this instance context, retaining actor sentinels and copied position storage."),
    )
    for old, new in bindings
]


# Individually reviewed render-direct bindings; structural fixups run first.
REGEX_FIXES += [
    (rf"^{name}$", rf"\b{old}\b", new, evidence)
    for name, bindings, evidence in (
        ("Render::SetCameraNearClip",
         (('arg0', 'near_clip'),),
         "F00286 146C.c:9041-9045 forwards the same near clipping threshold to the room polygon consumer."),
        ("Render::SetCameraFarClip",
         (('arg1', 'preserve_texture_lod'),),
         "F00288 146C.c:9053-9071 always sets camera farClip; only zero arg1 recalculates/clamps room texture LOD depth. Cutscene exit passes1 and scripted cameras pass backdrop-enabled state."),
        ("Render::ConfigureFieldDisplay",
         (('arg0', 'frame_width'), ('arg1', 'frame_height'), ('arg2', 'projection_distance'), ('arg3', 'clear_red'), ('arg4', 'clear_green'), ('arg5', 'clear_blue'), ('temp_s3', 'drawable_height')),
         "F00162 146C.c:5959-5986 builds both draw/display environments, subtracts16 from height, sets projection distance and RGB clear values. Signed rounding and unsigned height storage remain unchanged."),
        ("Render::PresentFieldFrame",
         (('temp_a0', 'second_port_pressed_buttons'),),
         "F00163 146C.c:5988-6032 takes the high halfword of button edges for the second controller's pause/step controls."),
        ("Render::AppendOverlayPrimitive",
         (('arg0', 'primitive'),),
         "F00200 146C.c:8055 forwards the primitive to the overlay ordering-table splice."),
        ("Render::SplicePrimitiveList",
         (('arg0', 'ordering_link'), ('arg1', 'primitive'), ('a3', 'link_mask'), ('temp_t0', 'sentinel_handle')),
         "F01324 5BF94.c:1332-1343 walks links to the previous ordering-table word, preserves length bits and splices the primitive. Native handles remain handles, not physical pointers."),
        ("Render::BlendRgbWithBrightness",
         (('temp_lo', 'weighted_channel'),),
         "F01346 5BF94.c:2704-2720 mixes the current RGB component with an eighth-scale blend, multiplies brightness, and shifts10."),
        ("Render::DrawIntegerRightAligned",
         (('new_var', 'base_advanced_xy'),),
         "F01352 5BF94.c:3033-3052 subtracts the five-pixel base glyph step from packed XY before the font-specific extra step. Keep packed-coordinate arithmetic intact."),
        ("Render::ReserveTexturePageRange",
         (('arg0', 'first_page'), ('arg1', 'page_count'), ('arg2', 'owner_tag'), ('var_a3', 'range_occupied')),
         "F00331 146C.c:9509-9530 checks pages25..32 for any owner before filling the requested range with the supplied owner tag."),
        ("Render::RetireTexturePageRange",
         (('arg0', 'first_page'), ('arg1', 'page_count'), ('arg2', 'owner_tag'), ('var_s0', 'owner_mismatch')),
         "F00332 146C.c:9532-9560 rejects a differing owner and marks the matching range for deferred retirement; return is the inverse mismatch flag."),
        ("Render::ParseTextureImage",
         (('arg0', 'image_cursor'),),
         "F00609 146C.c:16935-16952 advances through TIM mode, optional CLUT block and image block in serialized words."),
        ("Render::QueueTextureRectangleMove",
         (('arg0', 'source_rectangle'), ('arg1', 'destination_x'), ('arg2', 'destination_y')),
         "F00971 40564.c:1272-1278 passes rectangle and destination coordinates to SetDrawMove, links one DR_MOVE and advances the cursor."),
        ("Render::QueueGimImage",
         (('arg1', 'first_texture_page'), ('var_s0', 'layer_cursor')),
         "F01338 5BF94.c:2347-2375 initializes three GIM layers with a shared first page and advances the layer pointer while constructing the disc request."),
        ("Render::ActivateGimLayer",
         (('arg0', 'layer_index'),),
         "F01339 5BF94.c:2377-2420 selects one GIM layer; scanline mode builds randomized wipe widths and blend mode reserves a workspace page."),
        ("Render::ReleaseGimImages",
         (('var_s0', 'layer_cursor'),),
         "F01340 5BF94.c:2422-2438 advances through all three layers while retiring pages; the actual base pointer, not advanced cursor, is freed."),
        ("Render::DrawGimFramebufferFade",
         (('arg0', 'image'), ('arg1', 'clut'), ('arg2', 'ordering_table'), ('temp_s0', 'brightness'), ('var_s1', 'other_framebuffer_x'), ('new_var', 'blend_texture_mode')),
         "F01394 5BF94.c:4192-4218 forwards image/CLUT/table to tile drawing then blends the other framebuffer;0x120 remains the exact texture-mode bits."),
        ("Render::SetGimWorkspaceDrawArea",
         (('arg0', 'ordering_link'), ('arg1', 'page_x')),
         "F01396 5BF94.c:4319-4323 inserts draw-area and offset commands for a64-wide workspace."),
        ("Render::SetGimFramebufferDrawArea",
         (('arg0', 'ordering_link'), ('arg1', 'page_x')),
         "F01397 5BF94.c:4325-4329 inserts draw-area and offset commands for a320-wide framebuffer."),
        ("Render::DrawGimLayer",
         (('arg0', 'layer_index'),),
         "F01399 5BF94.c:4429-4452 selects the layer and its palette and ordering-table offset before the typed draw-routine dispatch."),
        ("Render::UploadPackedRectangle",
         (('arg1', 'pixels'),),
         "F02717 MENU7/260.c:199-210 and TITLE/22C.c:82-93 decode packed XY/WH and pass this pixel buffer to LoadImage; shared native definition."),
        ("Render::DrawTextureFlashFade",
         (('temp_a0', 'flash_blend_weight'),),
         "F03677 renderTextures.h:60-128 cd10..13 subtracts10 and forwards the weight to _adjust for every RGB component."),
        ("Render::DrawTextureWipe",
         (('arg3', 'colors'), ('arg4', 'wipe_edge_x'), ('temp_a1', 'strip_right_x'), ('var_a0', 'strip_width_or_fade_weight'), ('var_s6', 'texture_u')),
         "F03678 renderTextures.h:130-235 emits12-pixel strips; var_a0 first holds final strip width then separately clamped0..64 edge fade weights. char U wrap, mixed lifetime and all packet operations remain unchanged."),
        ("Render::TransformRoomPalettes",
         (('arg0', 'mode'), ('arg1', 'duration'), ('arg2', 'red_offset'), ('arg3', 'green_offset'), ('arg4', 'blue_offset')),
         "F00215 146C.c:8241-8268 forwards mode/duration/RGB offsets to the corresponding CLUT/model/background transforms; main.c:10595-10776 establishes argument roles."),
        ("Render::TransformSceneColors",
         (('arg0', 'mode'), ('arg1', 'duration'), ('arg2', 'red_offset'), ('arg3', 'green_offset'), ('arg4', 'blue_offset')),
         "F00216 146C.c:8241-8268 forwards mode/duration/RGB offsets to the corresponding CLUT/model/background transforms; main.c:10595-10776 establishes argument roles."),
        ("Render::TransformBackgroundColors",
         (('arg0', 'mode'), ('arg1', 'duration'), ('arg2', 'red_offset'), ('arg3', 'green_offset'), ('arg4', 'blue_offset')),
         "F00218 146C.c:8241-8268 forwards mode/duration/RGB offsets to the corresponding CLUT/model/background transforms; main.c:10595-10776 establishes argument roles."),
        ("Render::TransformActorColors",
         (('arg0', 'mode'), ('arg1', 'duration'), ('arg2', 'actor_index'), ('arg3', 'red_offset'), ('arg4', 'green_offset'), ('arg5', 'blue_offset')),
         "F00217 146C.c:8257-8263 routes actor index+4 to CLUT and index+1 to model-color storage."),
        ("Render::LoadRoomPaletteTransition",
         (('arg0', 'duration'), ('arg1', 'palette_colors'), ('arg3', 'clut_offset')),
         "F00219 146C.c:8270-8278 and main.c:10553-10560 establish duration/palette/16-color offset. Existing omitted fourth arguments on two unused calls are not repaired by naming."),
        ("Render::StartSharedPaletteTransition",
         (('arg0', 'duration'), ('arg1', 'shared_slot'), ('arg2', 'palette_colors'), ('arg3', 'clut_offset')),
         "F04121 146C.c:8280-8283 adds4 to shared slot and forwards duration/palette/CLUT offset unchanged."),
        ("Render::ApplyBackgroundGradientColors",
         (('arg0', 'duration'), ('arg1', 'colors')),
         "F04122 146C.c:8285 forwards exactly the duration and five RGB colors to main.c:10780."),
        ("Render::StepRgbTowardTarget",
         (('arg0', 'current_channels'), ('arg1', 'target_channels'), ('arg2', 'signed_step'), ('var_t0', 'changed')),
         "F00981 40564.c:2003-2031 iterates three channels, applies a signed step with target clamping, and reports any changed channel."),
        ("Render::ToFixed16",
         (('arg0', 'value'),),
         "F04168 main.c:10778 shifts the input value16 bits without changing width or overflow behavior."),
        ("Render::SetBackgroundGradientColors",
         (('arg0', 'duration'), ('arg1', 'colors'), ('sp10', 'packed_colors'), ('temp_t6', 'color_components_fixed')),
         "F04169 main.c:10780-10820 holds five six-word RGB/current-and-delta rows, forms Q16 channels, then packs immediate RGB or starts a2*duration transition."),
        ("Render::SetBackgroundGradientBase",
         (('arg0', 'duration'), ('arg1', 'colors')),
         "F04170 main.c:10822-10832 sets the current gradient and copies its five base colors."),
        ("Render::TransformBackgroundGradient",
         (('arg0', 'mode'), ('arg1', 'duration'), ('arg2', 'red_offset'), ('arg3', 'green_offset'), ('arg4', 'blue_offset'), ('sp10', 'packed_colors'), ('temp_v0', 'luminance_or_channel_fixed'), ('var_t1', 'red_target_fixed'), ('var_t0', 'green_target_fixed'), ('var_a1', 'blue_target_or_green_fixed'), ('v0', 'base_luminance_work'), ('v1', 'base_green_work'), ('a0', 'base_blue_work'), ('temp_a1', 'red_byte'), ('var_v0', 'rounded_green_fixed'), ('new_var', 'packed_red_green')),
         "F04171 main.c:10834-10988: targets are Q16 RGB; blue-target storage briefly holds green's Q16 contribution. Both independently reviewed case6/7 blocks give v0/v1/a0 the same base-luminance/green/blue work roles. temp_v0 is reused for half luminance and rounded R/B packing. Preserve all lifetimes and division order."),
        ("Render::RequestBackgroundGradientTransform",
         (('arg0', 'mode'), ('arg1', 'duration'), ('arg2', 'red_offset'), ('arg3', 'green_offset'), ('arg4', 'blue_offset')),
         "F04172 main.c:10990-10995 gates and forwards the exact background mode/duration/RGB transform."),
        ("Render::SetModelTint",
         (('arg0', 'duration'), ('arg1', 'tint_slot'), ('arg2', 'color'), ('sp10', 'tint_code'), ('temp_t0', 'fade_state'), ('temp_a3', 'color_components_fixed'), ('temp_a1', 'red_fixed'), ('temp_t1', 'green_fixed'), ('temp_t2', 'blue_fixed')),
         "F04173 main.c:10997-11045 stores six-word Q16 color/delta state; slot0 targets room tint and slot n targets actor n-1."),
        ("Render::SetModelTintBase",
         (('arg0', 'duration'), ('arg1', 'tint_slot'), ('arg2', 'color')),
         "F04174 main.c:11047-11053 applies the tint and saves the same slot's base RGB."),
        ("Render::TransformModelTint",
         (('arg0', 'mode'), ('arg1', 'duration'), ('arg2', 'tint_slot'), ('arg3', 'red_offset'), ('arg4', 'green_offset'), ('arg5', 'blue_offset'), ('sp10', 'tint_code'), ('var_t4', 'blend_target'), ('var_a1', 'red_target_fixed'), ('var_t0', 'green_target_fixed'), ('var_a2', 'blue_target_or_green_fixed'), ('var_v0', 'channel_value'), ('var_v1', 'luminance_or_rounding_fixed'), ('t1', 'fade_state'), ('t2', 'color_components_fixed'), ('v0', 'base_luminance_work'), ('v1', 'base_green_work'), ('a0', 'base_blue_work')),
         "F04175 main.c:11055-11220: Q16 RGB targets, blend target0/4000, duration-scaled deltas and slot-specific publication. Both case6/7 block locals have the same independently reviewed color-work roles; var_v0's unused seed1 is retained before channel conversion, and var_v1 is reused for luminance/rounding."),
        ("Render::RequestModelTintTransform",
         (('arg0', 'mode'), ('arg1', 'duration'), ('arg2', 'tint_slot'), ('arg3', 'red_offset'), ('arg4', 'green_offset'), ('arg5', 'blue_offset')),
         "F04176 main.c:11222-11227 gates and forwards the same model tint transform."),
        ("Render::UpdateBackgroundGradientTransition",
         (('sp18', 'packed_colors'), ('var_v0', 'rounded_green_fixed'), ('new_var', 'packed_red_green'), ('new_var2', 'red_byte'), ('var_v1', 'rounded_red_or_blue_fixed'), ('t1', 'color_components_fixed')),
         "F04177 main.c:11229-11285 advances five Q16 rows, rounds/packs RGB, and retains alternating mode9/10 pulse timing."),
        ("Render::UpdateModelColorTransitions",
         (('sp18', 'tint_code'), ('var_v0', 'rounded_channel_fixed'), ('a2', 'color_components_fixed')),
         "F03558 main.c:11287-11343 advances17 color slots, publishes room/actor tint and preserves pulse/restore transitions."),
        ("Render::UpdateClutTransitions",
         (('var_t5', 'frame_divisor'),),
         "F03559 main.c:11345-11466 selects16/8/4/2-step delta tables;1/2/4/8 divides16 to determine the frame limit."),
        ("Render::StopClutTransition",
         (('arg0', 'slot'),),
         "F03560 main.c:11468 clears only the selected CLUT slot active flag."),
        ("Render::TransformClutBase",
         (('arg6', 'update_current_colors'), ('new_var', 'source_red'), ('new_var2', 'source_green'), ('new_var3', 'source_blue')),
         "F03562 main.c:11499-11596 extracts RGB555 channels for luminance; final flag additionally updates current colors/inactive channel state while base colors always change."),
        ("Render::OffsetClutBase",
         (('arg5', 'update_current_colors'),),
         "F03563 main.c:11598-11602 forwards the current-color update flag with mode8 and RGB offsets."),
        ("Render::CaptureRoomBackgroundColors",
         (('var_v1', 'packed_room_colors'),),
         "F03564 main.c:11604-11619 reads five packed RGB words and expands each channel into base/Q16 state."),
    )
    for old, new in bindings
]
