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

NATIVE_HELPER_HEADERS = {
    "Battle_Ai_ActorNavigationOverrideStatus":
        r"Native\Game\src\Battle\Ai\NavigationPrimitives.h",
    "Audio_UpdateActorSoundTransition": r"Native\Game\src\Actors\SoundOverrides.h",
    "Actors_ResolveAttachmentParentMatrix": r"Native\Game\src\Actors\ModelDrawing.h",
    "Actors_SaveActorGroundedTransform": r"Native\Game\src\Actors\MotionRequests.h",
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
    **{name: r"Native\Game\src\Battle\EncounterHooks.h"
       for name in (
           "Boot_EncounterRoomPlacements",
           "Boot_EncounterUsesRoomEvents",
           "Boot_EncounterLoadRoomEvents",
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
}

NATIVE_FUNCTION_REDIRECTS = {
    "F03404": ("Core_Gte_DpqColor", r"Native\Engine\src\Core\Gte.cpp"),
    "F03408": ("Core_Gte_SetFogNear", r"Native\Engine\src\Core\Gte.cpp"),
    "F03399": ("Core_Math_LookupRotationSineCosine",
               r"Native\Engine\src\Core\Math\ScalarMath.c"),
    "F01017": ("Core_Math_LookupRotationSineCosine",
               r"Native\Engine\src\Core\Math\ScalarMath.c"),
    "F03568": ("Core_IgnoreCall", r"Native\Engine\src\Core\Runtime.c"),
    "F00764": ("Core_Gte_Cos", r"Native\Engine\src\Core\Gte.cpp"),
    "F00765": ("Core_Gte_Sin", r"Native\Engine\src\Core\Gte.cpp"),
    "F03397": ("Core_Gte_RotMatrixYXZ", r"Native\Engine\src\Core\Gte.cpp"),
    "F03398": ("Core_Gte_RotMatrix", r"Native\Engine\src\Core\Gte.cpp"),
    "F03400": ("Core_Gte_Cos", r"Native\Engine\src\Core\Gte.cpp"),
    "F03401": ("Core_Gte_Sin", r"Native\Engine\src\Core\Gte.cpp"),
    "F03407": ("Core_Gte_OuterProduct0", r"Native\Engine\src\Core\Gte.cpp"),
    "F03410": ("Core_Gte_SquareRoot12", r"Native\Engine\src\Core\Gte.cpp"),
    "F03411": ("Core_Gte_VectorNormal", r"Native\Engine\src\Core\Gte.cpp"),
    "F03412": ("Core_Gte_VectorNormalS", r"Native\Engine\src\Core\Gte.cpp"),
    "F03413": ("Core_Gte_VectorNormalSS", r"Native\Engine\src\Core\Gte.cpp"),
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
    ("BATTLE/BATTLE.PRG", 0x1060, 0x80069860, 0x98, "Content_battleLimbNameBytes",
     "58578.c INCLUDE_RODATA D_80069860; strings end at jtbl_800698F8"),
    ("BATTLE/BATTLE.PRG", 0x80B0C, 0x800E930C, 0x71C, "Content_battleGimmickTemplateBytes",
     "30DB0.c:_loadGimmick; three 16-byte headers plus modelDataSize payloads"),
    ("BATTLE/BATTLE.PRG", 0x823AC, 0x800EABAC, 0x9DC, "Content_battleTextboxFrameBytes",
     "5BF94.c:textbox frame vertices; nine styles precede D_800EB588"),
    ("BATTLE/BATTLE.PRG", 0x7FD48, 0x800E8548, 116, "Content_battleTriggerMarkerBytes",
     "146C.c:func_8008E19C complete one-group/one-quad marker definition; G00039-G00041 are interior aliases"),
    ("SLUS_010.40", 0x800, 0x80010000, 0x52000, "Content_residentLfoSamples",
     "25AC.c:LFO selectors include source-code bytes as sample/control data; original PS-X EXE payload, never host code"),
)

MODULE_DATA_BLOB_TYPES = {
    "Content_residentLfoSamples": ("short", 2),
    "Content_plg074TrailEmissionCounter": ("int", 4),
}
MODULE_SAMPLE_POINTERS = {"G01840": "Content_residentLfoSamples"}

MODULE_FUNCTION_ENTRIES = (
    ("BATTLE/BATTLE.PRG", 0x800E527C, "F01805"),
    ("BATTLE/BATTLE.PRG", 0x800E5308, "F01806"),
    ("BATTLE/BATTLE.PRG", 0x800DC638, "F01694"),
    ("BATTLE/BATTLE.PRG", 0x800E6EB0, "F01836"),
    ("EFFECT/PLG074.BIN", 0x800F98F8, "F01986"),
    ("EFFECT/PLG074.BIN", 0x800FA7F0, "F01987"),
    ("EFFECT/PLG074.BIN", 0x800FB414, "F01988"),
    ("EFFECT/PLG074.BIN", 0x800FBBB0, "F01989"),
    ("EFFECT/PLG074.BIN", 0x800FBDC8, "F01990"),
    ("EFFECT/PLG074.BIN", 0x800FC28C, "F01991"),
    ("EFFECT/PLG074.BIN", 0x800FC404, "F01992"),
    ("EFFECT/PLG074.BIN", 0x800FC590, "F01993"),
    ("EFFECT/PLG074.BIN", 0x800FCFEC, "F01994"),
    ("EFFECT/PLG074.BIN", 0x800FD534, "F01995"),
    ("BATTLE/BATTLE.PRG", 0x800980F8, "F00752"),
    ("BATTLE/BATTLE.PRG", 0x80097AEC, "F00748"),
    ("BATTLE/BATTLE.PRG", 0x80097CF0, "F00749"),
    ("BATTLE/BATTLE.PRG", 0x8009797C, "F00746"),
    ("SLUS_010.40", 0x80041A00, "F03404"),
    ("SLUS_010.40", 0x80041954, "F03408"),
    ("SLUS_010.40", 0x80041A1C, "F03405"),
    ("SLUS_010.40", 0x80041AF4, "F03414"),
    ("SLUS_010.40", 0x80041104, "F03399"),
    ("BATTLE/BATTLE.PRG", 0x800E4604, "F01772"),
    ("BATTLE/BATTLE.PRG", 0x800E6B4C, "F01832"),
    ("BATTLE/BATTLE.PRG", 0x800E6BA0, "F01833"),
    ("BATTLE/BATTLE.PRG", 0x800E6C34, "F01834"),
    ("GIM/SCREFF2.PRG", 0x800F9BD8, "F02458"),
    ("BATTLE/BATTLE.PRG", 0x800DBA4C, "F04203"),
    ("BATTLE/BATTLE.PRG", 0x800DBA88, "F04204"),
    ("BATTLE/BATTLE.PRG", 0x800DBB98, "F04205"),
    ("BATTLE/BATTLE.PRG", 0x800DBC0C, "F04206"),
)

FIXES: dict[str, list[tuple[str, str, str]]] = {
    "F01013": [
        ("Battle_Ai_AddAiTileOccupant(object);",
         "Battle_Ai_AddAiTileOccupant((const Field_GimmickInstance*)object);",
         "the rebased physics view aliases the native owning Gimmick; pass its checked tile prefix explicitly"),
    ],
    "F00514": [
        ("*(int*)&spec->recoveryDelay & 0x4000",
         "spec->resetsHitResults",
         "Action byte13 bit6 is the typed source interception flag; do not alias four bytes starting at recoveryDelay"),
        ("&origin, &position, &impact, result->primaryTarget.target.targetActor);",
         "&origin, &position, &impact, result->primaryTarget.target.targetActor) != NULL;",
         "Native segment trace returns an output pointer; this caller consumes only collision truth"),
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
    "F00896": [
        ("((u_int (*)(u_char, void*, int))Actors_TranslateActorToTileAtRate)(arg0->instance.slot, &arg0->routeTarget, 0);",
         "Actors_TranslateActorToTileAtRate(arg0->instance.slot, (Actors_TileTarget*)&arg0->routeTarget, 0);",
         "40564.c:661 takes an int actor ID; the conflicting narrow 3A1A0.c prototype leaves upper argument bits undefined on x64"),
    ],
    "F00897": [
        ("((u_int (*)(u_char, void*, int))Actors_TranslateActorToTileAtRate)(arg0->instance.slot, &arg0->sharedSequences[4], 0);",
         "Actors_TranslateActorToTileAtRate(arg0->instance.slot, (Actors_TileTarget*)&arg0->sharedSequences[4], 0);",
         "Cancellation must use the same full-width typed tile-translation ABI as the actual implementation"),
    ],
    "F00898": [
        ("((u_int (*)(u_char, void*, int))Actors_TranslateActorToTileAtRate)(arg0->instance.slot, &arg0->routeTarget, -1);",
         "Actors_TranslateActorToTileAtRate(arg0->instance.slot, (Actors_TileTarget*)&arg0->routeTarget, -1);",
         "Travel must promote the slot to the actual int parameter rather than call through an incompatible u_char function pointer"),
    ],
    "F00298": [
        ("Battle_Ai_ActorNavigationOverrideStatus()",
         "Battle_Ai_ActorNavigationOverrideStatus(arg0)",
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
    "F00099": [
        ("""    if ((Battle_actionState->abilityInputSucceeded == 1)
        && (Battle_actors[action->targets[0].target.targetActor]->stats->currentHp != 0)) {
        ((void (*)(Battle_ActionRequest*))Battle_Ai_NotifyActionRelationshipChanges)(action);
        Battle_actionFlowMode = 8;
        ++Battle_actionState->chainStep;
        ++Battle_actionState->queueIndex;
        Battle_ResolveActionTargets(Input_settings.mappedChainAbilities[Battle_actionState->chainAbilitySlot - 1],
            &Battle_actionState->queue[Battle_actionState->queueIndex & 3], &Battle_actionState->selection, 1);
        Battle_QueueChainCount(Battle_actionState->queueIndex);
        if (Audio_Seq_unusedScoredata.maxChain < Battle_actionState->queueIndex) {
            Audio_Seq_unusedScoredata.maxChain = Battle_actionState->queueIndex;
        }""",
         """    if (Battle_actionState->abilityInputSucceeded == 1) {
        if (Battle_actors[action->targets[0].target.targetActor]->stats->currentHp != 0) {
            ((void (*)(Battle_ActionRequest*))Battle_Ai_NotifyActionRelationshipChanges)(action);
            Battle_actionFlowMode = 8;
            ++Battle_actionState->chainStep;
            ++Battle_actionState->queueIndex;
            Battle_ResolveActionTargets(Input_settings.mappedChainAbilities[Battle_actionState->chainAbilitySlot - 1],
                &Battle_actionState->queue[Battle_actionState->queueIndex & 3], &Battle_actionState->selection, 1);
            Battle_QueueChainCount(Battle_actionState->queueIndex);
            if (Audio_Seq_unusedScoredata.maxChain < Battle_actionState->queueIndex) {
                Audio_Seq_unusedScoredata.maxChain = Battle_actionState->queueIndex;
            }
        } else {
            Battle_FinishBattleAbilitySequence();
        }""",
         "Owner rule #183: accepted lethal chain timing earns the existing point, but never queues an attack on a corpse; defense and kill-gated learning stay unchanged"),
        ("""(Audio_Seq_unusedArtsStatus.kills.totalKills
                < Battle_battleAbilityPointRequirements[Audio_Seq_unusedArtsStatus.kills
                        .battleAbilitiesUnlocked])""",
         "(Audio_Seq_unusedArtsStatus.kills.totalKills < 0xFFFF)",
         "Owner rule #183 banks every successful chain/defense toward later abilities while learning waits for a death; saturate the persistent ushort instead of discarding threshold overflow"),
    ],
    "F00545": [
        ("void Field_UnloadCurrentRoom(void)\n{",
         "void Field_UnloadCurrentRoom(void)\n{\n    Boot_ResetGameplayInspection();",
         "146C.c:15489 revoke diagnostic readiness before any teardown yield/free"),
    ],
    "F00786": [
        ("    arg0[12] += (u_long)arg0;\n"
         "    arg0[13] += (u_long)arg0;\n"
         "    arg0[14] += (u_long)arg0;\n"
         "    arg0[15] += (u_long)arg0;\n",
         "",
         "SHP section members remain serialized offsets; VS_ONDISC resolves them from the native model base"),
    ],
    "F00782": [
        ("memset(s1, 0, 0x178);",
         "memset(s1, 0, offsetof(Field_GimmickInstance, reserved178));",
         "Clear the native runtime prefix, not the PS1 prefix or the larger actor-compatibility view"),
        ("Core_Memory_CopyAlignedWords(s1, Field_gimmickInstances[arg0->dataAddress], 0x178);",
         "Core_Memory_CopyAlignedWords(s1, Field_gimmickInstances[arg0->dataAddress], "
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
        ("    Core_Memory_ZeroWordsBackward(Menu_Loot_inventory, sizeof *Menu_Loot_inventory);",
         "    Menu_Loot_BeginPaging(node);\n"
         "    Core_Memory_ZeroWordsBackward(Menu_Loot_inventory, sizeof *Menu_Loot_inventory);",
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
    Core_Memory_ZeroAlignedWords(&Menu_Loot_paging, sizeof Menu_Loot_paging);
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
    return Items_Inventory_CountItems(category, Menu_Loot_inventory) + quantity
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
        ("        Core_Memory_FreeFromHighEnd(Menu_Loot_inventory);",
         "        Menu_Loot_ResetPaging();\n"
         "        Core_Memory_FreeFromHighEnd(Menu_Loot_inventory);",
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
    "F00549": [
        ("    Items_LootListNode* var_a0 = Items_lootListHead;",
         "    Items_LootListNode* var_a0 = Items_lootListHead;\n\n"
         "    Menu_Loot_ResetPaging();",
         "Room exit and Combat Lab stop invalidate page references before freeing their source nodes."),
    ],
    "F01867": [
        ("            curveSamples[segment] = segment * 0x600 + Core_Random_Rand() % width - width / 2;",
         """            int sampleJitter = Core_Random_Rand();
            // PLG045 func_800FA760, 0x800FAA74: DIV by zero leaves the dividend in HI.
            if (width != 0) {
                sampleJitter %= width;
            }
            curveSamples[segment] = segment * 0x600 + sampleJitter - width / 2;""",
         "Preserve the original nontrapping remainder and RNG consumption when the "
         "width reused from the previous ribbon becomes zero; do not clamp or reload it."),
    ],
    "F01856": [
        ("    int bladeMaterial;\n    int shieldMaterial;",
         "    int bladeMaterial = 0;\n    int shieldMaterial = 0;",
         "INITBTL/18.c:func_800F9CCC passes both materials even when their "
         "model IDs are zero; initialize empty slots without changing equipped ones."),
    ],
    "F03210": [
        ("""    if (in_SwapWithSavedState == 0) {
        Core_Memory_CopyWords(&Audio_Seq_savedMusicConfig, &Audio_Seq_primaryMusicConfig, 0x80);
        Core_Memory_CopyWords(Audio_Seq_savedMusicChannels, Audio_Seq_musicChannels, 0x2200);
    } else {
        Core_Memory_SwapWords(&Audio_Seq_savedMusicConfig, &Audio_Seq_primaryMusicConfig, 0x80);
        Core_Memory_SwapWords(Audio_Seq_savedMusicChannels, Audio_Seq_musicChannels, 0x2200);
    }""", "    Audio_Native_RestoreMusicSnapshot(in_SwapWithSavedState);",
         "restore complete native objects instead of PS1-sized byte ranges"),
    ],
    "F03213": [
        ("""Core_Memory_CopyWords(&Audio_Seq_primaryMusicConfig, &Audio_Seq_savedMusicConfig, 0x80);
        Core_Memory_CopyWords(Audio_Seq_musicChannels, (Audio_MusicConfig*)Audio_Seq_savedMusicChannels, 0x2200);""",
         "Audio_Native_SaveMusicSnapshot();",
         "save every native channel and config field"),
    ],
    "F03214": [
        ("""Core_Memory_CopyWords(&Audio_Seq_primaryMusicConfig, &Audio_Seq_savedMusicConfig, 0x80);
        Core_Memory_CopyWords(Audio_Seq_musicChannels,
            (Audio_MusicConfig*)Audio_Seq_secondaryMusicChannels, 0x2200);""",
         "Audio_Native_SaveMusicSnapshot();",
         "layer snapshots use the same complete native state"),
    ],
    "F03215": [
        ("""Core_Memory_CopyWords(&Audio_Seq_primaryMusicConfig, &Audio_Seq_savedMusicConfig, 0x80);
            Core_Memory_CopyWords(Audio_Seq_musicChannels,
                (Audio_MusicConfig*)Audio_Seq_secondaryMusicChannels, 0x2200);""",
         "Audio_Native_SaveMusicSnapshot();",
         "crossfade snapshots use the same complete native state"),
    ],
    "F03246": [
        ("*(int*)&Audio_Seq_pendingMusicData.param1 = 0;",
         "Audio_Seq_pendingMusicData.param1 = 0;",
         "25AC.c:3591 cancels the entire sequence pointer; native param1 is uintptr_t, not a PS1 int"),
    ],
    "F02438": [
        (" // Partial playback?", "",
         "the XA request argument is initial CD volume rather than a byte count"),
    ],
    "F01611": [
        ("(void* arg0)", "(void* arg0, size_t byte_count)",
         "carry the containing effect block extent into stream validation"),
        ("Audio_BindRoomCutsceneStream((uintptr_t)arg0)",
         "Audio_BindRoomCutsceneStream(arg0, byte_count)",
         "bind native stream assets instead of a PS1 array-offset alias"),
    ],
    "F01626": [
        ("Effects_BindEffectTextureTables((Effects_PackageBlock1*)arg0->blocks);",
         "Effects_BindEffectTextureTables(arg0->blocks, "
         "(unsigned)(meta & 0xFFFF) - sizeof(arg0->meta));",
         "pass the actual block payload extent to the native material binding validator"),
        ("Effects_BindEffectAuxiliarySoundBank(arg0->blocks);",
         "Effects_BindEffectAuxiliarySoundBank(\n"
         "                arg0->blocks, (meta & 0xFFFF) - sizeof(u_int));",
         "the stream payload excludes the four-byte package block header"),
    ],
    "F03265": [
        ("""    if ((Audio_Cutscene_streamState.voiceMask != 0)
        && (Audio_Cutscene_streamState.volumeFadeTicksLeft != 0)) {
        --Audio_Cutscene_streamState.volumeFadeTicksLeft;
        temp_s1 = Audio_Cutscene_streamState.volume
                + Audio_Cutscene_streamState.volumeFadeStep;
        if ((temp_s1 & 0xFF00) != (Audio_Cutscene_streamState.volume & 0xFF00)) {
            if (Audio_Driver_mixBehavior & 2) {
                int temp_s0 = (Audio_Cutscene_streamState.volume * Audio_Mixer_kCenterPanGain) >> 0x10;
                Audio_Driver_SetVoiceVolume(
                    Audio_Cutscene_streamState.voiceIndex, temp_s0, temp_s0, 0);
                Audio_Driver_SetVoiceVolume(
                    Audio_Cutscene_streamState.voiceIndex + 1, temp_s0, temp_s0, 0);
            } else {
                int temp_s0 = (temp_s1 << 0xF) >> 0x10;
                Audio_Driver_SetVoiceVolume(Audio_Cutscene_streamState.voiceIndex, temp_s0, 0, 0);
                Audio_Driver_SetVoiceVolume(
                    Audio_Cutscene_streamState.voiceIndex + 1, 0, temp_s0, 0);
            }
        }
        Audio_Cutscene_streamVolume = temp_s1 & 0xFFFF;
    }""", "    Audio_Native_UpdateCutsceneStream();",
         "native completion and fades preserve mono/stereo layout and exact targets"),
    ],
    "F03554": [
        ("void Audio_ResetSoundSlots(void)\n{",
         "void Audio_ResetSoundSlots(void)\n{\n"
         "    Audio_Native_ResetCutsceneStream();",
         "slot reset must release native voices and borrowed stream bindings"),
    ],
    "F03161": [
        ("    SpuStart();\n"
         "    SpuSetTransferStartAddr(0x1010);\n"
         "    Audio_Driver_WriteSampleMemory(Audio_Driver_legacySpuFlushBytes, 64);\n"
         "    Audio_Driver_WaitForSampleTransfer();\n", "",
         "native voice reset owns silence; no SPU sample-memory initialization"),
    ],
    "F03162": [
        ("    if (Audio_Cutscene_legacySpuTransferBusy == 1) {\n"
         "        Audio_Driver_WriteSampleMemory(Audio_Driver_legacySpuFlushBytes, 64);\n"
         "        Audio_Driver_WaitForSampleTransfer();\n"
         "    }\n", "",
         "native bank loads are synchronous asset bindings, not hardware transfers"),
        ("    Audio_Driver_KeyOffVoices(0xFFFFFF);", "    Audio_Native_Reset();",
         "teardown must clear release tails and queued native output"),
    ],
    "F03186": [
        ("Audio_Driver_WriteVoiceParameters(in_pChannel->voice.assignedVoice,\n"
         "                            &in_pChannel->voice,\n"
         "                            in_pChannel->voice.volumeScale);",
         "Audio_Native_ConfigureChannel(in_pChannel);",
         "pass the musical instrument identity and controls to native output"),
        ("Audio_Driver_WriteDirtyVoiceParameters(\n"
         "                            in_pChannel->voice.assignedVoice,\n"
         "                            &in_pChannel->voice, in_pChannel->updateFlags);",
         "Audio_Native_ConfigureChannel(in_pChannel);",
         "native control updates replace register writes"),
    ],
    "F03189": [
        ("SpuGetVoiceEnvelope(i, &pVoice->envelopeLevel);",
         "pVoice->envelopeLevel = Audio_Native_Envelope(i);",
         "voice stealing observes the actual native envelope"),
    ],
    "F03269": [
        ("    return var_s5;", "    Audio_Native_Tick();\n    return var_s5;",
         "commit 200-frame control ticks; native output renders on device demand"),
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
        ("((void (*)())Effects_DrawEffectParticleList)(arg0->particles, (void*)((uintptr_t)Core_ScratchBase() + 0x15C));",
         "Effects_DrawEffectParticleList(arg0->particles);",
         "forward native particle ownership through a type-checked call"),
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
    "F00876": [
        ("(MATRIX*)((uintptr_t)Core_ScratchBase() + 0x014)",
         "&((Render_FrameScratch*)Core_ScratchBase())->viewMatrix",
         "the actor overhead projection must use the rebased native frame matrix"),
    ],
    "F00731": [
        ("*ot = ((u_long)prim << 8) >> 8;", "*ot = LegacyGpu_Handle(prim);",
         "overhead awareness frame links must retain native packet ownership, not truncated addresses"),
    ],
    "F02771": [
        ("*nextPrim = ((u_long)prim << 8) >> 8;", "*nextPrim = LegacyGpu_Handle(prim);",
         "260.c:3504 game-over fade links must store the native packet handle, not a truncated host address"),
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
        ("Core_Memory_AllocateFromLowEnd(0x3200)",
         "Core_Memory_AllocateFromLowEnd(2 * sizeof(Actors_ActorRecord))",
         "592C.c:171 companion previews own two native actor records"),
        ("Core_Memory_AllocateFromLowEnd(0x24D0)",
         "Core_Memory_AllocateFromLowEnd(sizeof(Actors_ActorRecord) + "
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
        ("Core_Memory_AllocateFromHighEnd(0x1B00)",
         "Core_Memory_AllocateFromHighEnd(0x1900 + 64 * 2 * sizeof(*menuText))",
         "C48.c:546 scratch ends with 128 text pointers; reserve native pointer width, not the PS1 512 bytes"),
    ],
    "F03669": [
        ("    memset(&Input_settings, 0, sizeof Input_settings);\n"
         "    Input_settings.menuFlags = 0x2D8;\n"
         "    Input_settings.information = 1;\n"
         "    Input_settings.simpleMap = 3;\n"
         "    Input_settings.containerEmpty = 1;\n"
         "    Input_settings.abilityTimingDisplay = 1;\n"
         "    Input_settings.cursorMemory = 1;",
         "    Boot_InitializePlayerSettings();",
         "22C.c:4463 share real settings defaults with direct native sessions without title presentation"),
    ],
    "F00199": [
        ("                    ot = Render_legacyOrderingTables[Render_frameBuf];",
         "                    Boot_EncounterPollStop();\n"
         "                    ot = Render_legacyOrderingTables[Render_frameBuf];",
         "native encounter hosts can stop at a production frame boundary; inactive for normal sessions"),
    ],
    "F00077": [
        ("void Field_UpdateCameraOrbitInput(void)",
         '#include "Battle/TargetPages.h"\n\nvoid Field_UpdateCameraOrbitInput(void)',
         "Paged target lists explicitly own shoulder buttons while the ordinary field camera remains unchanged"),
        ("    if (Field_sphericalCamera.delta.yaw == 0) {\n        if (!CAMERA_INPUT_LOCKED()) {",
         "    if (Field_sphericalCamera.delta.yaw == 0) {\n"
         "        if (!CAMERA_INPUT_LOCKED() && !Battle_TargetPagesUseShoulders()) {",
         "Do not initiate orbit rotation from a target-page command"),
        ("&& !CAMERA_INPUT_LOCKED())", "&& !CAMERA_INPUT_LOCKED() && !Battle_TargetPagesUseShoulders())",
         "Do not extend an existing orbit from held page-navigation buttons"),
    ],
    "F00112": [
        ("previews[count]", "previews[Battle_ValidateTargetPreviewIndex(count)]",
         "Check native candidate capacity before every direct actor/limb/chest preview write"),
    ],
    "F00116": [
        ("&Battle_actionState->selection, 0x84C",
         "&Battle_actionState->selection, sizeof Battle_actionState->selection",
         "Clear the complete native action request after widening native target capacity"),
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
    "F00111": [
        ("void Battle_CollectSourceAreaActionTargets(",
         '#include "Battle/TargetPages.h"\n\nvoid Battle_CollectSourceAreaActionTargets(',
         "Expose the shared native target-capacity and paging API to the target collector translation unit"),
        ("previews[count]", "previews[Battle_ValidateTargetPreviewIndex(count)]",
         "Check native candidate capacity before source-area preview writes"),
    ],
    "F00113": [
        ("previews[count]", "previews[Battle_ValidateTargetPreviewIndex(count)]",
         "Check native candidate capacity before ground-area preview writes"),
    ],
    "F01323": [
        ("        case 19:",
         "        case 19:\n"
         "            if (Battle_HandleTargetPageButtons()) {\n"
         "                *target = 0xFF;\n"
         "                return 0;\n"
         "            }",
         "L1/R1 change whole-actor pages only after the original target-list animation has settled"),
        ("*target = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D;",
         "*target = Battle_MapTargetPageIndex((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D);",
         "Return the absolute preview index while each original visible page keeps local indices"),
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
    "F00103": [
        ("    ((void (*)(void*))Battle_SetSecondarySphereVolume)(&Battle_actionState->areaShape);",
         "    if (Battle_actionState->isPlayerAction != 0) {\n"
         "        ((void (*)(void*))Battle_SetSecondarySphereVolume)(&Battle_actionState->areaShape);\n"
         "    }",
         "NPC area filtering retains action geometry without requiring the optional player sphere renderer"),
    ],
    "F01297": [
        ("    if ((*(Battle_TargetSphere* (*))&Battle_targetSphere) != NULL) {",
         "    Battle_ResetTargetPages();\n    if ((*(Battle_TargetSphere* (*))&Battle_targetSphere) != NULL) {",
         "Clearing target selection invalidates the native page view"),
    ],
    "F00539": [
        ("    if (dataLen != 0) {",
         "    Boot_EncounterRoomPlacements(&data, &dataLen);\n"
         "    if (dataLen != 0) {",
         "explicit encounter definitions replace room placements before the production spawn/cache lifecycle"),
    ],
    "F00543": [
        ("void Field_FinishRoomLoad(int arg0)\n{",
         "void Field_FinishRoomLoad(int arg0)\n{\n    Boot_ResetGameplayInspection();",
         "146C.c:15272 do not inspect partially replaced room actors/sections"),
        ("        temp_s4 = Event_LoadRoomEventSections((void*)(arg0),",
         "        if (!Boot_EncounterUsesRoomEvents()) {\n"
         "            temp_s4 = Boot_EncounterLoadRoomEvents();\n"
         "        } else {\n"
         "        temp_s4 = Event_LoadRoomEventSections((void*)(arg0),",
         "isolated encounters declare an empty story/door-event set; normal room loading remains unchanged"),
        ("            Boot_gameEntryMode == new_var);",
         "            Boot_gameEntryMode == new_var);\n        }",
         "close the explicit room-event policy branch"),
        ("    if ((temp_s4 << 0x10) != 0) {",
         "    if (Save_NativeResumePending() && !Save_NativeFinishResume()) {\n"
         "        Boot_JumpToTitle(NULL);\n"
         "        return;\n"
         "    }\n\n"
         "    if ((temp_s4 << 0x10) != 0) {",
         "146C.c:15272 restore the native checkpoint after camera/display setup, before event dispatch"),
    ],
    "F00533": [
        ("    if (Boot_gameEntryMode == 1) {\n"
         "        State_ResetZoneScriptFlags(1);\n"
         "    } else {\n"
         "        State_ResetZoneScriptFlags(0);\n"
         "    }",
         "    if (!Save_NativeResumePending()) {\n"
         "        if (Boot_gameEntryMode == 1) {\n"
         "            State_ResetZoneScriptFlags(1);\n"
         "        } else {\n"
         "            State_ResetZoneScriptFlags(0);\n"
         "        }\n"
         "    }",
         "146C.c:14826 preserve restored zone flags only during the initial native resume"),
    ],
    "F02777": [
        ('static char* Menu_Files_playTime = "00:00:00:00";',
         'static char Menu_Files_playTime[] = "00:00:00:00";',
         "260.c:4157 the clock overwrites its digit positions; own mutable storage rather than a read-only literal"),
        ("        // BUG: invalid write to .rodata\n", "",
         "the native clock buffer is now mutable and reset by value"),
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
    "F00551": [
        ("Field_ResolveSceneRoomData(Field_roomData.minimapScene);",
         "Field_ResolveSceneRoomData(Field_roomData.minimapScene, Field_roomData.header.minimapSceneLength);",
         "bound scene decoding to its MPD subsection, retaining 12-byte room records"),
    ],
    "F00556": [
        ("Field_DrawMinimapScene(Field_roomData.minimapScene);",
         "Field_DrawMinimapScene(Field_roomData.minimapScene, Field_roomData.header.minimapSceneLength);",
         "bound minimap vertices and primitive tables to the loaded scene"),
        ("    Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);\n    drawArea =",
         "    Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);\n"
         "    Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);\n    drawArea =",
         "restore the field transform after the minimap's explicit translation"),
    ],
    **{fid: [
        ("Field_ResolveSceneRoomData(Battle_unusedSceneBuffer)",
         "Field_ResolveSceneRoomData(Battle_unusedSceneBuffer, Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size)",
         "bound ARM scene decoding to its loaded file extent"),
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
    "F01342": [
        ("        Boot_WaitForOverlayReady();",
         "        Boot_WaitForOverlayReady();\n"
         "        if (subMenu == 0) {\n"
         "            Boot_ResetMainMenuStatics();\n"
         "        }",
         "5BF94.c:vs_battle_loadMenuPrg MAINMENU reload restores C48.c:_quitDelay and other initialized menu statics"),
    ],
    "F02511": [
        ("        int count = source->count;\n\n        Items_Inventory_RebuildDisplayIndices(6);",
         "        int count = source->count;\n\n"
         "        /* Probe before merging: rejected claims retain their whole loot row. */\n"
         "        if ((write != 0) && (Items_Inventory_CopyItem(Items_ItemCategory_Misc,\n"
         "                targetInventory, sourceItemIndex, sourceInventory) == 0)) {\n"
         "            return 0;\n"
         "        }\n\n"
         "        Items_Inventory_RebuildDisplayIndices(6);",
         "misc-item writes must preserve inventory when the whole stack cannot fit; reuse the non-writing capacity query before mutation"),
    ],
    "F02535": [
        ("((u_long)poly << 8) >> 8", "LegacyGpu_Handle(poly)",
         "action-cost panel packets retain native handles"),
    ],
    "F02536": [
        ("((uintptr_t)var_t2 << 8) >> 8", "LegacyGpu_Handle(var_t2)",
         "button-prompt background at ordering entry15 must preserve traversal to submenu labels below it"),
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
    "F03533": [
        ("Audio_soundState.musicHandles[16]", "Audio_soundState.sfxBankData[2]",
         "main.c:10161 indexes unk14 beyond its array to alias sfxData[2] at PS1 offset 0x54; use the native field"),
        ("Audio_soundState.sfxBankData[2] = arg0;",
         "Audio_soundState.sfxBankData[2] = (void*)arg0;",
         "the room sound bank is an externally owned native pointer, not a 32-bit music handle"),
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
        ("Core_Memory_AllocateFromLowEnd(0x1900)", "Core_Memory_AllocateFromLowEnd(sizeof(Actors_ActorRecord))",
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
    "F02781": [
        ("((u_long)prim << 8) >> 8", "LegacyGpu_Handle(prim)",
         "MENU8/21A0.c rename commands and backdrops must preserve native packet links"),
    ],
    "F02782": [
        ("((u_long)temp_t0 << 8) >> 8", "LegacyGpu_Handle(temp_t0)",
         "MENU8/21A0.c name and grid highlights must preserve the lower UI chain"),
    ],
    "F02886": [
        ("char** menuText;", "char* menuText[2 * 17];",
         "MENUC assembly blade choices need two native pointers per stock slot plus the current row"),
        ("            menuText = textBuf + 0x6A4;", "",
         "Keep native pointers out of the unaligned PS1-sized temporary text allocation"),
    ],
    "F02887": [
        ("char** menuText;", "char* menuText[2 * 17];",
         "MENUC assembly grip choices need two native pointers per stock slot plus the current row"),
        ("            menuText = textBuf + 0x6A4;", "",
         "Keep native pointers out of the unaligned PS1-sized temporary text allocation"),
    ],
    "F02888": [
        ("char** menuText;", "char* menuText[2 * 49];",
         "MENUC gem choices need two native pointers per stock slot plus the current row"),
        ("            menuText = textBuf + 0x1324;", "",
         "BuildItemList copies gem text before this native pointer array leaves scope"),
    ],
    "F02903": [
        ("char** menuText;", "char* menuText[2 * 17];",
         "MENUC combination blade choices need a native pointer array for all stock slots"),
        ("        menuText = (char**)(textBuf + 0x6A4);", "",
         "BuildItemList copies text before the native pointer array leaves scope"),
    ],
    "F02909": [
        ("char** menuText;", "char* menuText[2 * 9];",
         "MENUC combination shield choices need a native pointer array for all stock slots"),
        ("            menuText = stringBuf + 0x384;", "",
         "BuildItemList copies text before the native pointer array leaves scope"),
    ],
    "F02913": [
        ("char** sp10;", "char* sp10[2 * 17];",
         "MENUC combination armor choices need a native pointer array for all stock slots"),
        ("            sp10 = (char**)(temp_v0 + 0x6A4);", "",
         "BuildItemList copies text before the native pointer array leaves scope"),
    ],
    "F00195": [
        ("Battle_cameraEaseMode", "Field_sphericalCamera.delta.mode",
         "D_800F19EC aliases the spherical camera mode at +0x1C"),
        ("Battle_settledCameraDistance", "Field_sphericalCamera.values.distance",
         "D_800F19D8 aliases the spherical camera distance at +0x8"),
    ],
    "F00190": [
        ("    abilityTimer = Battle_actionState->abilityTimer;",
         "    abilityTimer = Battle_actionState->abilityTimer;\n"
         "    const Actors_ActorRecord* actingActor = Actors_actorRecords[temp_s3->primaryTarget.target.targetActor];\n"
         '    Core_Debugf("combat-flow", "animation id=%u state=%u active=%u held=%u timer=%d window=%u effects=%u action=%u",\n'
         "        (unsigned)actingActor->animationId, (unsigned)actingActor->instance.actionAnimationState,\n"
         "        (unsigned)actingActor->instance.currentAnimation, (unsigned)actingActor->pose.holdingLastFrame,\n"
         "        abilityTimer, (unsigned)Battle_actionState->abilityWindow,\n"
         "        (unsigned)Effects_effectBusyFlags, (unsigned)temp_s3->actionId);",
         "temporary action animation completion trace"),
    ],
    "F00186": [
        ("    VECTOR sp30;",
         "    VECTOR sp30;\n"
         '    Core_Debugf("combat-flow", "camera mode=%d zoom=%d yaw=%d distance=%d action=%u targets=%u",\n'
         "        Field_sphericalCamera.delta.mode, Battle_actionState->cameraZoomState,\n"
         "        Field_sphericalCamera.delta.yaw, Field_sphericalCamera.values.distance,\n"
         "        (unsigned)Battle_actionState->selection.actionId, (unsigned)Battle_actionState->selection.targetCount);",
         "temporary combat camera gate trace"),
        ("Field_ComputeCameraTrackingStep(&sp18, &sp30, &sp20, 1, 0x28);",
         "Field_ComputeCameraTrackingStep(&sp18, &sp30, &sp20, 1, 0x28);\n"
         '                Core_Debugf("combat-flow", "tracking error=%ld,%ld,%ld", sp20.vx, sp20.vy, sp20.vz);',
         "temporary camera convergence trace"),
    ],
    "F00826": [
        ("    if ((Actors_actorRecords[arg0]->animationId != 0x63)\n"
         "        && (Actors_actorRecords[arg0]->animationId != 0xC7)) {",
         "    if (Actors_actorRecords[arg0] == NULL\n"
         "        || ((Actors_actorRecords[arg0]->animationId != 0x63)\n"
         "            && (Actors_actorRecords[arg0]->animationId != 0xC7))) {",
         "all-slot pose resets must reach the base helper's empty-actor and gimmick handling"),
    ],
    "F01364": [
        ("    Core_Memory_ZeroWordsBackward(&Menu_Framework_actionSelection, sizeof Menu_Framework_actionSelection);",
         "    Core_Memory_ZeroWordsBackward(&Menu_Framework_actionSelection, sizeof Menu_Framework_actionSelection);\n"
         "    Battle_actionSelection = Menu_Framework_ActionSelectionState();",
         "basic attacks may precede the first menu open; bind the interface-owned selection state at initialization"),
    ],
    "F01327": [
        ("*str = '\\0'", "*str = Core_TextCode_Terminator", "game text terminates with E7, not a zero glyph"),
    ],
    "F01328": [
        ("c = *src++;", "c = (u_char)*src++;", "unsigned game text and formatting controls"),
        ("c != '\\0'", "c != Core_TextCode_Terminator", "stop before reading past the encoded template"),
        ("*dest++ = '-';", "*dest++ = 0xA7;", "vsString minus glyph"),
        ("*dest++ = '0';", "*dest++ = 0;", "vsString zero glyph"),
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
    "F01710": [
        ("    int savedMap;", "    u_short (*savedMap)[32];", "cache a native grid pointer"),
        ("        int* scratch = (int*)((uintptr_t)Core_ScratchBase() + 0x3BC);\n"
         "        index = 0;\n        savedMap = *scratch;",
         "        index = 0;\n        savedMap = Battle_Ai_FrameState()->heightMap;",
         "save the owned AI grid without truncation"),
        ("*(int*)((uintptr_t)Core_ScratchBase() + 0x3BC) = (int)Battle_Ai_specialTerrainHeights;",
         "Battle_Ai_FrameState()->heightMap = Battle_Ai_specialTerrainHeights;",
         "select the full native height-grid pointer"),
        ("*(int*)((uintptr_t)Core_ScratchBase() + 0x3BC) = savedMap;",
         "Battle_Ai_FrameState()->heightMap = savedMap;", "restore the full native height-grid pointer"),
    ],
    "G00229": [
        ("Effects_PackageFile Effects_emptyPackage", "u_int Effects_emptyPackage[0x5200 / sizeof(u_int)]",
         "D_800EC4BC contains nine tagged blocks and the terminator at +0x51FC, not just a header"),
    ],
    "F01595": [
        ("Effects_BindEffectPackageBlocks(&Effects_emptyPackage);",
         "Effects_BindEffectPackageBlocks((Effects_PackageFile*)Effects_emptyPackage);",
         "bind the full embedded package stored at original address 0x800EC4BC"),
    ],
    "F00589": [("void Field_SampleRoomGroundColor(int arg0) { Render_SampleRoomLighting(arg0, 0); }",
                 "void Field_SampleRoomGroundColor(int arg0, int layer, Render_ColorCode* color)\n"
                 "{\n    (void)layer;\n    Render_SampleRoomLighting(arg0, 0, color);\n}",
                 "preserve the output-color argument consumed by the original assembly")],
    "G00582": [("int Battle_Ai_roomHasWaypoints", "void* Battle_Ai_roomHasWaypoints",
                 "func_800E6F9C stores the room auxiliary-data pointer, not a boolean")],
    "F01020": [
        ("&Actors_skeletonPose.scale", "&Actors_roomActorScale",
         "D_800F3C58+0x7E0 aliases D_800F4438; use the canonical initialized room scale"),
    ],
    "F03192": [
        (f"Audio_Seq_secondaryMusicConfig->{field}",
         f"(Audio_Seq_secondaryMusicConfig != NULL ? Audio_Seq_secondaryMusicConfig->{field} : 0)",
         "BuildVoiceModeMask accepts no saved layer; do not eagerly read the null optional config")
        for field in ("noiseChannels", "reverbChannels", "fmChannels")
    ] + [
        ("Audio_Driver_WriteDirtyVoiceParameters(pChannel->voice.assignedVoice,\n"
         "                        &pChannel->voice, pChannel->updateFlags);",
         "Audio_Native_ConfigureChannel(pChannel);",
         "sound effects share the same native voice engine as music"),
    ],
    "F00975": [
        ("    frame = Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex] & 15;",
         '    Core_Debugf("texture", "actor=%u model=%u bank=%u slot=%u clut=%u channel=%d frame=%d overlay=%u row=%u src=(%d,%d) dst=(%d,%d) size=(%d,%d)",\n'
         "        (unsigned)actor->instance.slot, (unsigned)actor->modelId, (unsigned)Actors_sharedSequenceBankIndex,\n"
         "        (unsigned)actor->textureSlot, (unsigned)actor->clutA, index, frame, (unsigned)animation->overlayIndex,\n"
         "        (unsigned)actor->textureRowOffsets[Actors_sharedSequenceBankIndex],\n"
         "        (Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex] & 15) * 64 + srcX, srcY + 256,\n"
         "        dstX + x * 64, y * 256 + row * 64 + py, width, height);\n"
         "    frame = Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex] & 15;",
         "toggleable live facial tile selection and copy rectangle diagnostics"),
    ],
    "F00801": [
        ("*((u_char*)temp_s0 + Actors_sharedSequenceBankIndex + 0x183E)",
         "temp_s0->textureRowOffsets[Actors_sharedSequenceBankIndex]",
         "func_8009CE9C: active facial bank row through the native actor layout"),
        ("*((u_char*)temp_s0 + arg0 + 0x183D)", "temp_s0->textureRowOffsets[arg0 - 1]",
         "func_8009CE9C: restore frame zero before selecting an unavailable facial bank"),
    ],
    "F00804": [
        ("    if (Actors_sequenceTexturePagesOneBased[arg0] != 0) {",
         "    if (arg0 < 1 || arg0 > 3) return -1;\n"
         "    if (Actors_sequenceTexturePagesOneBased[arg0] != 0) {", "one-based shared ETM slot bounds"),
        ("Actors_sequenceTexturePagesOneBased[arg0]", "Actors_sharedSequenceBankTexturePages[arg0 - 1]",
         "D_800F244F+slot and D_800F2450+bank are the same original byte"),
    ],
    "F00805": [
        ("    switch (Content_legacyEtmLoadPhase) {",
         "    if (objdata->targetSlot < 1 || objdata->targetSlot > 3) return -2;\n"
         "    switch (Content_legacyEtmLoadPhase) {", "one-based shared ETM slot bounds"),
        ("Actors_sequenceTexturePagesOneBased[objdata->targetSlot]",
         "Actors_sharedSequenceBankTexturePages[objdata->targetSlot - 1]",
         "publish the ETM page to the same bank table read by facial animation"),
    ],
    "F01221": [
        ("        c->durationFrames = script[2];",
         "        c->durationFrames = script[2];\n"
         '        Core_Debugf("event.camera", "start track=%s mode=%u easing=%u duration=%u",\n'
         '            c == &Event_camera->eye ? "eye" : "look_at",\n'
         "            (unsigned)c->motionMode, (unsigned)c->easingPrimary,\n"
         "            (unsigned)c->durationFrames);",
         "record actual camera-track producers when investigating zero-duration interpolation"),
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
        ("    Render_DrawCompactText(Battle_puzzleTimerText,",
         '    Core_Debugf("hud.timer", "text=%s seconds=%d minutes=%d hours=%d frames=%d",\n'
         "        Battle_puzzleTimerText, timer->seconds, timer->minutes, timer->hours, timer->frames);\n"
         "    Render_DrawCompactText(Battle_puzzleTimerText,",
         "retain exact per-render timer text in detailed gameplay diagnostics"),
    ],
    "F01392": [
        ("Battle_DrawPuzzleTimer(&Battle_puzzleTimer);",
         "Battle_DrawPuzzleTimer(&State_sceneTimer);",
         "SLUS 0x8005046C is the time-field subobject of the authoritative timer at 0x80050468, not independent native storage"),
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
    "F01865": [
        ("Core_Memory_AllocateFromHighEnd(0x10)",
         "Core_Memory_AllocateFromHighEnd(offsetof(Effects_ParticleQuadSet, textureAnimation))",
         "particleDispatch.h: allocate the complete native untextured prefix instead of its PS1 byte extent"),
    ],
    "F02351": [
        ("#define VS_PARTICLE_UNTEXTURED\n", "",
         "PLG265 original 0x800FFB7C allocates 0x30 and initializes texture state; do not inherit the preceding line dispatcher's option"),
        ("#undef VS_PARTICLE_UNTEXTURED\n", "",
         "remove the matching inherited option cleanup from the textured PLG265 instance"),
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
    "F00279": [
        ("void Actors_UnloadSharedSequence(int arg0) { Content_ReleaseActorSequence(arg0, 1); }",
         """int Actors_UnloadSharedSequence(int bankId)
{
    int slot;
    int result;
    if (bankId < 1 || bankId > 3) {
        Core_Errorf("actors.sequence", "invalid shared bank retirement=%d", bankId);
        return -1;
    }
    for (slot = 0; slot < 17; ++slot) {
        Actors_ActorRecord* actor = Actors_actorRecords[slot];
        u_short* entry;
        if (actor == NULL || actor->sharedSequences[bankId - 1] == NULL ||
            actor->animationHeader != (intptr_t)actor->sharedSequences[bankId - 1] ||
            !actor->poseOverrideActive) {
            continue;
        }
        if (Actors_FindActorAnimationHeader(actor, actor->animationId, &entry) != actor->animationHeader ||
            entry == NULL || entry[2] == 0) {
            Core_Errorf("actors.sequence", "bank=%d cannot unwind actor=%d animation=%d",
                bankId, slot, actor->animationId);
            return -1;
        }
    }
    for (slot = 0; slot < 17; ++slot) {
        Actors_ActorRecord* actor = Actors_actorRecords[slot];
        if (actor == NULL || actor->sharedSequences[bankId - 1] == NULL ||
            actor->animationHeader != (intptr_t)actor->sharedSequences[bankId - 1]) {
            continue;
        }
        if (actor->poseOverrideActive && Actors_RestartActorAnimationEvents(actor) < 0) {
            return -1;
        }
        actor->poseOverrideActive = 0;
        actor->animationHeader = 0;
        actor->animationSet = 0xFF;
        actor->pose.eventScript = NULL;
        actor->pose.nextEventFrame = 0xFFFF;
        actor->blendPose.eventScript = NULL;
        actor->blendPose.nextEventFrame = 0xFFFF;
    }
    result = Content_ReleaseActorSequence(bankId, 1);
    if (result < 0) {
        Core_Errorf("actors.sequence", "shared bank=%d retirement failed status=%d", bankId, result);
    }
    return result;
}""",
         "func_8007CBBC: undo live vertex edits and retire borrowed event/cache bindings before releasing a shared bank; retain the skeletal pose"),
    ],
    "F00836": [
        ("    i = 0;\n\n    while ((temp_v0_2 = Field_FindActorAboveGimmick",
         "    if (Field_gimmickInstances[temp_s4] == NULL) {\n"
         '        Core_Debugf("gimmick", "no eligible stack top while removing slot=%d", arg0);\n'
         "        return;\n    }\n\n    i = 0;\n\n"
         "    while ((temp_v0_2 = Field_FindActorAboveGimmick",
         "the top search can have no object; retain preceding stack updates before ending the actor scan"),
    ],
    "F00887": [
        ("    Actors_ShpModel* model = actor->instance.model;",
         "    Actors_ShpModel* model;",
         "func_800A1C10: the original low-address model read precedes the gimmick fallback; defer it natively"),
        ("        model = actor->instance.model;\n    }\n\n"
         "    matrix = Actors_ActorBoneWorldTransform",
         "    }\n\n    model = actor->instance.model;\n"
         "    matrix = Actors_ActorBoneWorldTransform",
         "resolve the actor or gimmick owner before the sole model read"),
    ],
    "F01170": [
        ("    Effects_StartScriptedActorEffect(\n"
         "        Core_Memory_ReadUnalignedShort(arg0 + 1), Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 3)));",
         "    const int effect = Core_Memory_ReadUnalignedShort(arg0 + 1);\n"
         "    const u_short selector = Core_Memory_ReadUnalignedShort(arg0 + 3);\n"
         "    const unsigned referenceType = selector >> 12;\n"
         "    const int actor = referenceType <= 1 ? Event_ResolveScriptActorReference(selector) : -1;\n"
         "    const Battle_Actor* target = (unsigned)actor < 16 ? Actors_ActorAtSlot(actor) : NULL;\n"
         "    if ((unsigned)actor >= 16 ||\n"
         "        (referenceType == 0 && (target == NULL || target->modelId != selector)) ||\n"
         "        (Actors_actorRecords[actor] == NULL && Field_gimmickInstances[actor] == NULL)) {\n"
         '        Core_Errorf("event", "effect=%d awaits unavailable actor selector=0x%04X slot=%d",\n'
         "            effect, (unsigned)selector, actor);\n"
         "        return 1;\n"
         "    }\n"
         "    Effects_StartScriptedActorEffect(effect, actor);",
         "func_800BA8BC: validate reference kind and model ownership; do not bind a missing-model sentinel or advance past the required effect"),
    ],
    "F00536": [("Field_isRoomActorSpawnEnabled", "Field_zoneContext.mpdEnemyCount",
                 "D_800F18E0 aliases _zoneContext +0x60 (mpdEnemyCount), not an independent enable flag")],
    "F00275": [("    if (arg0 < 16) {",
                 '    Core_Debugf("actors", "script spawn actor=%u model=%d existing=%p", arg0, arg1,\n'
                 "        arg0 < 16 ? (void*)Battle_actors[arg0] : NULL);\n    if (arg0 < 16) {",
                 "script-spawn selection and duplicate gate diagnostics")],
    "F00778": [("    if (ret == -1) {\n        return 1;\n    }",
                 "    if (ret == -1) {\n        return 1;\n    }\n"
                 '    Core_Debugf("actors", "loaded kind=%u actor=%u model=%d result=%d record=%p",\n'
                 "        (unsigned)slot->assetKind, (unsigned)slot->targetSlot, (int)slot->modelId, ret,\n"
                 "        slot->targetSlot < 17 ? (void*)Actors_actorRecords[slot->targetSlot] : NULL);",
                 "completed actor-load result and installed record diagnostics")],
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
    "F01640": [
        ("targetObject = (Battle_Ai_PerceivedObjectView*)Actors_actorRecords[id];",
         "targetObject = (Battle_Ai_PerceivedObjectView*)Battle_actors[id]->model;",
         "valid auxiliary AI targets own model prefixes without an actor-record table entry"),
        ("Battle_Ai_PerceivedObjectView* object = (Battle_Ai_PerceivedObjectView*)Actors_actorRecords[candidate];",
         "Battle_Ai_PerceivedObjectView* object = (Battle_Ai_PerceivedObjectView*)actor->model;",
         "resolve auxiliary occluders through the current Battle_Actor owner like distance/FOV paths"),
    ],
    "F03515": [
        ("    case 0xFF000:\n"
         "        var_s0_2 = Audio_soundState.sfxBankData[Audio_soundState.currentSfxBank - 1];",
         "    case 0xFF000:\n"
         "        if (Audio_soundState.currentSfxBank == 0) {\n"
         '            Core_Warnf("audio.sfx", "private SFX id=%ld skipped: no selected bank", id);\n'
         "            return;\n        }\n"
         "        if (Audio_soundState.currentSfxBank >\n"
         "            sizeof Audio_soundState.sfxBankData / sizeof Audio_soundState.sfxBankData[0]) {\n"
         '            Core_Errorf("audio.sfx", "invalid selected SFX bank=%u", Audio_soundState.currentSfxBank);\n'
         "            return;\n        }\n"
         "        var_s0_2 = Audio_soundState.sfxBankData[Audio_soundState.currentSfxBank - 1];\n"
         "        if (var_s0_2 == NULL || id < 0) {\n"
         '            Core_Errorf("audio.sfx", "unavailable SFX bank=%u or invalid id=%ld", Audio_soundState.currentSfxBank, id);\n'
         "            return;\n        }",
         "no selection is an optional disabled state; validate selection before unsigned indexing and report inconsistent banks"),
        ("    case 0xF00000:\n        var_s0_2 = Audio_soundState.sfxBankData[2];",
         "    case 0xF00000:\n        var_s0_2 = Audio_soundState.sfxBankData[2];\n"
         "        if (var_s0_2 == NULL) {\n"
         '            Core_Warnf("audio.sfx", "room SFX id=%ld skipped: no room bank", id);\n'
         "            return;\n        }\n"
         "        if (id < 0) {\n"
         '            Core_Errorf("audio.sfx", "invalid room SFX id=%ld", id);\n'
         "            return;\n        }",
         "room banks are optional and must be present before dereferencing their serialized program offsets"),
    ],
    "F01860": [(f"(*(int (*)[])&Actors_sharedSequenceData)[{index}] = 0;",
                  f"Actors_sharedSequenceData[{index}] = NULL;", "reset whole native SEQ bank pointers")
                 for index in range(3)],
    "F00777": [("    *dst = *objectData;",
                 '    Core_Debugf("actors", "queue kind=%u actor=%u model=%u phase=%u parse=%u",\n'
                 "        (unsigned)objectData->assetKind, (unsigned)objectData->targetSlot,\n"
                 "        (unsigned)objectData->modelId, (unsigned)Content_legacyModelLoadPhase,\n"
                 "        (unsigned)Content_modelParseResetFlag);\n    *dst = *objectData;",
                 "shared actor-load request and parser-state diagnostics")],
    "F01325": [
        ("((uintptr_t)(prim + 2) << 8) >> 8", "LegacyGpu_Handle(prim + 2)", "compact-text draw-mode link"),
        ("((uintptr_t)(prim + 5) << 8) >> 8", "LegacyGpu_Handle(prim + 5)", "compact-text glyph link"),
        ("((u_long)Render_frameScratch.primitiveCursor << 8) >> 8", "LegacyGpu_Handle(Render_frameScratch.primitiveCursor)",
         "compact-text chain head retains its native address"),
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
           ("Core_Memory_ZeroWordsBackward(Menu_Framework_menuItems, 0xB24)",
            "Core_Memory_ZeroWordsBackward(Menu_Framework_menuItems, sizeof(Menu_Framework_MenuRow) * 40 + 0x124)",
            "clear all forty native rows and the original 0x124-byte scratch tail"),
        ],
        "F01367": [
           ("Core_Memory_AllocateFromHighEnd(0xB24)",
            "Core_Memory_AllocateFromHighEnd(sizeof(Menu_Framework_MenuRow) * 40 + 0x124)",
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
    "F03673": [
        ("void Title_InitializeNewGameData(void)\n{",
         "void Title_InitializeNewGameData(void)\n{\n    Save_NativeResetSession();",
         "22C.c:4698 New Game abandons any staged native resume"),
    ],
    "F03670": [
        ("    menuItem = Save_MemoryCard_AnySaveFileExists();",
         "    menuItem = Save_NativeInitialTitleSelection();",
         "22C.c:4491 choose title focus from native slots without probing card BIOS"),
    ],
    "F02757": [
        ("    if (Menu_Files_fileMenuScreenFade != 0) {",
         "    Save_NativeDrawFileOverlay(0);\n"
         "    if (Menu_Files_fileMenuScreenFade != 0) {",
         "260.c:1951 draw typed native file cards after the message background and before the screen fade"),
    ],
    "F03624": [
        ("    if (Title_fileMenuScreenFade != 0) {",
         "    Save_NativeDrawFileOverlay(1);\n"
         "    if (Title_fileMenuScreenFade != 0) {",
         "22C.c:1784 Continue shares native cards without installing memory-card save headers"),
    ],
    "F00988": [
        ("Actors_SaveActorGroundedTransform((Field_GimmickInstance*)actor)",
         "Actors_SaveActorGroundedTransform(actor)",
         "40564.c:2519 landing snapshots use the typed actor history API rather than a gimmick view"),
    ],
    "F04130": [
        ("        *(int*)&actor->instance.previousTileX = *(int*)&actor->instance.currentTileX;\n"
         "        *(Core_IntPair*)&actor->instance.supportOffset = *(Core_IntPair*)&actor->instance.position;\n"
         "        if (actor->instance.supportLayer == 0) {\n"
         "            *(int*)&actor->savedPlacement = *(int*)&actor->instance.currentTileX;\n"
         "        }\n"
         "        actor->instance.previousFacing = actor->instance.facing;",
         "        Actors_SaveActorGroundedTransform(actor);",
         "44F14.c:1565-1571 reuse the same native grounded history contract under the original motion-state guard"),
    ],
    "G00497": [("u_long Menu_textBoxOrderingTables", "u_long Menu_textBoxOrderingTables[102]",
                 "5BF94.c:4044-4046; three 34-word textbox ordering tables")],
    "F01331": [
        ("c < '\\f'", "c < 0xE5", "vs_battle_getTextLineLength.s:0x800C702C"),
        ("c == '\\f'", "c == 0xE5", "encoded page boundary"),
        ("c == '\\0'", "c == 0xE7", "encoded terminator"),
        ("c == '\\n'", "c == 0xE8", "encoded newline"),
        ("c > '9'", "c > 9", "0x800C70B0 uses decimal operand count, not ASCII"),
        ("length += *str++;", "length += (u_char)*str++;", "unsigned spacing operand"),
    ],
    "F01337": [
        ("u_int currentChar = *str++;",
         "u_int currentChar = (u_char)*str++;",
         "800C81C8 loads an unsigned encoded glyph/control byte"),
        ("currentChar < '\\f'",
         "currentChar < Core_TextCode_NonPrinting",
         "800C81DC compares against encoded0xE5 not ASCII form-feed0x0C"),
        ("currentChar != ' '",
         "currentChar != Core_TextCode_Space",
         "800C81E4 compares against encoded space0x8F not ASCII0x20"),
        ("currentChar - '\\f'",
         "currentChar - Core_TextCode_NonPrinting",
         "800C8274 subtracts0xE5 so encoded0xE7 terminates before zero padding"),
        ("u_int opcodeParam = *str++;",
         "u_int opcodeParam = (u_char)*str++;",
         "transition control operands retain their original unsigned byte width"),
    ],
    "F01332": [("glyphId == ' '", "glyphId == 0x8F", "game-encoded space in the vsstring region")],
    "F01334": [
        ("currentChar = *currentString;", "currentChar = (u_char)*currentString;",
         "_printVariableWidthFont.s:0x800C7340 uses lbu"),
        ("opcodeParam = *currentString;", "opcodeParam = (u_char)*currentString;", "unsigned text operand"),
        ("currentChar < '\\f'", "currentChar < 0xE5", "0x800C7354 compares the game control boundary"),
    ],
    "F01335": [
        ("u_int currentChar = *str++;", "u_int currentChar = (u_char)*str++;",
         "_printFixedWidthFont.s:0x800C7CC4 uses lbu"),
        ("u_int opcodeParam = *str++;", "u_int opcodeParam = (u_char)*str++;", "unsigned text operand"),
        ("currentChar < '\\f'", "currentChar < 0xE5", "0x800C7CD8 compares the game control boundary"),
        ("currentChar - '\\f'", "currentChar - 0xE5", "0x800C7D00 dispatches E7 termination and E8 newline"),
    ],
    "F00557": [
        ("    Core_Gte_SetRotMatrix((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x014));",
         "    if (!Render_ValidateRoomGeometry(arg0, Field_roomData.header.geometryLength)) {\n"
         '        Core_Debugf("render", "Invalid room geometry extent");\n'
         "        return;\n    }\n"
         "    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);",
         "146C.c:15926; validate the MPD section and use the native frame matrix"),
        ("(MATRIX*)((uintptr_t)Core_ScratchBase() + 0x014)", "&Render_frameScratch.viewMatrix", "native frame translation source"),
        ("*(void**)((uintptr_t)Core_ScratchBase() + 0x000)", "Render_frameScratch.primitiveCursor", "native primitive cursor"),
    ],
    # _removeActorAtIndex / 146C.c:926: the linked list is stored in a sparse actor slot table.
    "F00010": [("if (Battle_actors[i]->next == actor)",
                "if (Battle_actors[i] != NULL && Battle_actors[i]->next == actor)",
                "ignore empty slots when unlinking a removed actor")],
    # Animation-header outputs are native pointers, not PS1-sized integers.
    "F00819": [
        ("    int sp10[2];", "    u_short* sp10;", "native animation-header output"),
        ("((intptr_t (*)(Actors_ActorRecord*, int, int*))Actors_FindActorAnimationHeader)(temp_s2, var_s0, sp10);",
         "Actors_FindActorAnimationHeader(temp_s2, var_s0, &sp10);", "use the declared output contract"),
        ("if (sp10[0] == 0)", "if (sp10 == NULL)", "test the complete native pointer"),
    ],
    "F00823": [
        ("    int sp20;", "    u_short* sp20;", "native animation-header output"),
        ("((intptr_t (*)(Actors_ActorRecord*, int, int*))Actors_FindActorAnimationHeader)(actor, anim, &sp20);",
         "Actors_FindActorAnimationHeader(actor, anim, &sp20);", "avoid overwriting a four-byte stack local"),
    ],
    "F00833": [
        ("    int animation;", "    u_short* animation;", "native animation-header output"),
        ("((intptr_t (*)(Actors_ActorRecord*, int, int*))Actors_FindActorAnimationHeader)",
         "Actors_FindActorAnimationHeader", "both special-action stages use the declared output contract"),
    ],
    "F00834": [
        ("    int sp10;", "    u_short* sp10;", "native animation-header output"),
        ("((intptr_t (*)(Actors_ActorRecord*, int, int*))Actors_FindActorAnimationHeader)",
         "Actors_FindActorAnimationHeader", "avoid overwriting a four-byte stack local"),
    ],
    "F00835": [
        ("((intptr_t (*)(Actors_ActorRecord*, int, int*))Actors_FindActorAnimationHeader)(actor, 0xABU, (int*)&animation);",
         "Actors_FindActorAnimationHeader(actor, 0xABU, &animation);", "preserve the declared native pointer output"),
    ],
    "F00930": [
        ("((intptr_t (*)())Actors_FindActorAnimationHeader)",
         "Actors_FindActorAnimationHeader", "type-check the native locomotion header output"),
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
    # func_800A0204 / 30DB0.c:3725 caches a native SEQ bank, not a 32-bit guest address.
    "F00856": [
        ("    int found;", "    intptr_t found;", "preserve the resolved bank pointer"),
        ("found = ((intptr_t (*)(Actors_ActorRecord*, int, int*))Actors_FindActorAnimationHeader)(actor, anim, (int*)&frames);",
         "found = Actors_FindActorAnimationHeader(actor, anim, &frames);", "use the declared header output type"),
        ("    actor->animationEventFlag = 0;\n"
         "    if ((*(int*)((char*)actor + 0x5AC)) & 0x800000) {\n"
         "        Actors_RestartActorAnimationEvents(actor);\n"
         "    }\n    actor->poseOverrideActive = 0;",
         "    if (actor->poseOverrideActive && Actors_RestartActorAnimationEvents(actor) < 0) {\n"
         "        return -1;\n    }\n"
         "    actor->animationEventFlag = 0;\n    actor->poseOverrideActive = 0;",
         "do not erase the outstanding vertex-override obligation when its event stream cannot be restored"),
    ],
    # func_800AEF94 / 44F14.c:923: room actor slots are sparse, including an absent companion in slot 1.
    "F01008": [
        ("((void (*)(void*, void*, int))Actors_UpdateActorPalettePulse)(actor, NULL, 0);",
         "Actors_UpdateActorPalettePulse(actor, NULL, 0);",
         "40564.c:1651 typed actor palette pulse entry"),
        ("((void (*)(void*, void*, int))Actors_UpdateActorPalettePulse)(attached, NULL, 0);",
         "Actors_UpdateActorPalettePulse(attached, NULL, 0);",
         "Attached actors retain the original actor palette path"),
        ("((void (*)(void*, void*, int))Actors_UpdateActorPalettePulse)(NULL, (Actors_ActorRecord*)object, 0);",
         "Actors_UpdateActorPalettePulse(NULL, object, 0);",
         "Use owning native gimmick palettes instead of a PS1-sized view"),
        ("((void (*)(void*))Actors_UpdateActorAppearanceTransition)(actor);",
         "Actors_UpdateActorAppearanceTransition(actor);",
         "44F14.c:923 use the owning native actor appearance API"),
        ("((void (*)(void*))Actors_UpdateActorAppearanceTransition)(attached);",
         "Actors_UpdateActorAppearanceTransition(attached);",
         "Attached actors share actor ownership, not a gimmick compatibility layout"),
        ("((void (*)(void*))Actors_UpdateActorAppearanceTransition)((Actors_ActorRecord*)object);",
         "Field_UpdateGimmickAppearanceTransition(object);",
         "Gimmicks retain a separate typed appearance entry point"),
        ("if ((*previous)->modelId == texture)", "if (*previous != NULL && (*previous)->modelId == texture)",
         "empty actor slots cannot share texture animation"),
        ("if ((*previousAttached)->modelId == texture)",
         "if (*previousAttached != NULL && (*previousAttached)->modelId == texture)",
         "attached-actor texture lookup also traverses sparse slots"),
           ("struct Render_DrawDepthScratch* frame;", "Render_FrameScratch* frame;", "native ordering-table view"),
           ("frame->depth += actor->companionState.depthBias * 4;",
            "frame->orderingTable = (u_char*)frame->orderingTable + actor->companionState.depthBias * 4;",
            "companion depth shifts the native ordering-table base"),
           ("frame->depth -= actor->companionState.depthBias * 4;",
            "frame->orderingTable = (u_char*)frame->orderingTable - actor->companionState.depthBias * 4;",
            "restore the native ordering-table base"),
           ("frame->depth += 0x10;", "frame->orderingTable = (u_char*)frame->orderingTable + 0x10;",
            "gimmick depth shifts the ordering table without touching the primitive pointer"),
           ("frame->depth = (int)(frame->depth - 0x10);",
            "frame->orderingTable = (u_char*)frame->orderingTable - 0x10;", "restore the gimmick ordering-table base"),
        ("                Actors_DrawActorModel(\n"
         "                    actor, &entries[actor->instance.attachParent]->boneMatrices[actor->instance.attachBone], 0U);",
         "                MATRIX* parent_matrix = Actors_ResolveAttachmentParentMatrix(actor);\n"
         "                if (parent_matrix != NULL) {\n"
         "                    Actors_DrawActorModel(actor, parent_matrix, 0U);\n"
         "                }",
         "44F14.c:1010 defer an independently owned prop draw while its borrowed parent matrix is unavailable"),
    ],
        "F01006": [("*(MATRIX*)&source", "*source", "44F14.c:853; copy the supplied camera matrix, not pointer-stack bytes")],
    # func_800AE47C / 44F14.c:610: the SEQ header starts with offsets, not a native pointer.
    "F00840": [
        ("    temp_s1 = s0->nextEventFrame;",
         "    if (s0->eventScript == NULL) {\n        return 0;\n    }\n"
         "    temp_s1 = s0->nextEventFrame;",
         "a pose whose bank was retired has no battle timing marker to scan"),
    ],
    "F00992": [
        ("    if (*(int*)((u_char*)actor + 0x5AC) & 0x800000) {\n"
         "        Actors_RestartActorAnimationEvents(actor);\n    }",
         "    if (actor->poseOverrideActive && Actors_RestartActorAnimationEvents(actor) < 0) {\n"
         "        return;\n    }",
         "keep the original pose and vertex-override obligation when event unwind is unavailable"),
    ],
    "F01081": [
        ("        Actors_UnloadSharedSequence(arg0[1]);",
         "        if (Actors_UnloadSharedSequence(arg0[1]) < 0) {\n"
         "            return 1;\n        }",
         "yield rather than advancing the script after a failed shared-bank retirement"),
    ],
    "F00998": [
        ("void Actors_RestartActorAnimationEvents(",
         "int Actors_RestartActorAnimationEvents(",
         "native event-unwind failure is explicit to all three callers"),
        ("u_char** v0 = ((u_char** (*)(Actors_ActorRecord*, u_char, u_short**))Actors_FindActorAnimationHeader)(arg0, arg0->animationId, &sp10);",
         "Actors_SeqAnimationBank* v0 = (Actors_SeqAnimationBank*)Actors_FindActorAnimationHeader(arg0, arg0->animationId, &sp10);",
         "typed on-disc sequence header"),
        ("u_char* temp_a3 = *v0;",
         "u_char* temp_a3;\n"
         "    if (v0 == NULL || sp10 == NULL || (sp10[2] == 0 && arg0->poseOverrideActive)) {\n"
         '        Core_Errorf("actors.sequence", "cannot unwind actor=%u animation=%u: unavailable events",\n'
         "            (unsigned)arg0->instance.slot, (unsigned)arg0->animationId);\n"
         "        return -1;\n    }\n"
         "    if (sp10[2] == 0) {\n        return 0;\n    }\n"
         "    temp_a3 = VS_ONDISC(v0, animationData, u_char*);",
         "validate the nullable lookup before resolving the event stream's on-disc offset"),
        ("((void (*)())Actors_RunAnimationEvents)(arg0, &arg0->blendPose, 2, var_a3);",
         "((void (*)())Actors_RunAnimationEvents)(arg0, &arg0->blendPose, 2, var_a3);\n"
         "    return 0;",
         "report successful event unwind without changing its original decoder mode"),
    ],
    # F01393 / BATTLE func_800CBBCC, 5BF94.c:4056: GIM grid coordinates are temporary, not guest memory.
    "F01393": [
        ("    short* columnX = (short*)(0x1F800400 - (columns + 1) * 2);\n"
         "    short* rowY = columnX - (rows + 1);",
         "    // Byte-sized dimensions need at most 256 boundary coordinates per axis.\n"
         "    short columnX[256];\n"
         "    short rowY[256];", "bounded native GIM coordinates, independent of shared scratch storage"),
        ("    for (i = 0; i <= columns; ++i) {",
         "    if (columns * rows > (int)(sizeof image->tileTable / sizeof image->tileTable[0]) - 4) {\n"
         '        Core_Debugf("render", "GIM grid %dx%d exceeds tile table capacity", columns, rows);\n'
         "        return;\n"
         "    }\n\n"
         "    for (i = 0; i <= columns; ++i) {", "reject grids beyond the embedded tile table before reading tiles"),
        ("((u_long)prim << 8) >> 8", "LegacyGpu_Handle(prim)", "scaled GIM quad links must retain native pointers"),
    ],
    # F01818 / BATTLE 7CD68.c:934: runtime views are rebased by native_views.py, not on-disc structs.
    "F01818": [
        ("    unsigned short result[4];", "    SVECTOR result;\n"
         '    Core_Debugf("ai", "create actor=%p nativeId=%d viewId=%d options=0x%X nativeModel=%p stats=%p",\n'
         "        (void*)actor, ((Battle_Actor*)actor)->id, id, (unsigned int)options,\n"
         "        (void*)((Battle_Actor*)actor)->model, (void*)((Battle_Actor*)actor)->stats);",
         "AI creation identity and native/view layout diagnosis"),
        ("    state->stats = actor->stats;", "    state->stats = actor->stats;\n"
         '    Core_Debugf("ai", "created state=%p id=%u viewModel=%p viewStats=%p",\n'
         "        (void*)state, (unsigned int)state->actorId, (void*)model, (void*)state->stats);",
         "AI state links before tagged bone lookup"),
        ("((int*)Battle_Ai_roomAiState)[10 + i]", "Battle_Ai_roomAiState->specialActors[i]",
         "room special actors are native pointers, not PS1 word offsets"),
        ("= (int)actor;", "= (Battle_Actor*)actor;", "preserve special actor pointer"),
        ("Core_Memory_AllocateFromLowEnd(0x484)", "Core_Memory_AllocateFromLowEnd(sizeof(Battle_Ai_ActorAiState))",
         "allocate full native state, never sizeof the partial initialization view"),
        ("Core_Memory_ClearAlignedWords(state, 0x484)", "Core_Memory_ClearAlignedWords(state, sizeof(Battle_Ai_ActorAiState))",
         "clear full native state including candidate pointer array"),
        ("((void (*)(unsigned char, int, void*, int))Actors_ActorTaggedBonePosition)(state->actorId, 250, result, 0)",
         "Actors_ActorTaggedBonePosition(state->actorId, 0xFA, &result, 0)", "use actual int/SVECTOR signature"),
        ("result[1]", "result.vy", "typed tagged bone height"),
        ("    return (short)options >= 0", '    Core_Debugf("ai", "initialized actor=%d state=%p bytes=%u radius=%u height=%u",\n'
         "        id, (void*)state, (unsigned int)sizeof(Battle_Ai_ActorAiState),\n"
         "        (unsigned int)state->radius, (unsigned int)state->pointHeight);\n"
         "    return (short)options >= 0", "successful initialization gate"),
    ],
    # Actor IDs are 0..15 (146C.c:8936); 0x10 is the linked-actor sentinel, not a table index.
    # 7E05C.s:541-575 identifies the next BSS symbol, tileFlagMap at 0x800F58B8: 0x40 bytes after D_800F5878.
    "F03317": [
        ("    in_pChannel->updateFlags |= 1;",
         "    short* nativeWave = Audio_Seq_LfoWaveform((u_char)in_pChannel->programCounter[2]);\n"
         "    if (nativeWave == NULL) {\n"
         "        in_pChannel->programCounter += 3;\n"
         "        return;\n"
         "    }\n"
         "    in_pChannel->updateFlags |= 1;",
         "validate imported sample/control data before changing vibrato state"),
        ("*in_pChannel->programCounter++", "(u_char)*in_pChannel->programCounter++",
         "SoundVM_B4 uses LBU for its byte operands"),
        ("Audio_Seq_kLfoWaveforms[in_pChannel->vibratoShape]", "nativeWave",
         "reuse the validated original-source waveform"),
    ],
    "F03321": [
        ("    in_pChannel->updateFlags |= SOUND_UPDATE_TREMOLO;",
         "    short* nativeWave = Audio_Seq_LfoWaveform((u_char)in_pChannel->programCounter[2]);\n"
         "    if (nativeWave == NULL) {\n"
         "        in_pChannel->programCounter += 3;\n"
         "        return;\n"
         "    }\n"
         "    in_pChannel->updateFlags |= SOUND_UPDATE_TREMOLO;",
         "validate imported sample/control data before changing tremolo state"),
        ("*in_pChannel->programCounter++", "(u_char)*in_pChannel->programCounter++",
         "SoundVM_B8 uses LBU for its byte operands"),
        ("Audio_Seq_kLfoWaveforms[in_pChannel->tremoloShape]", "nativeWave",
         "reuse the validated original-source waveform"),
    ],
    "F03325": [
        ("    in_pChannel->updateFlags |= SOUND_UPDATE_PAN_LFO;",
         "    short* nativeWave = Audio_Seq_LfoWaveform((u_char)in_pChannel->programCounter[1]);\n"
         "    if (nativeWave == NULL) {\n"
         "        in_pChannel->programCounter += 2;\n"
         "        return;\n"
         "    }\n"
         "    in_pChannel->updateFlags |= SOUND_UPDATE_PAN_LFO;",
         "validate imported sample/control data before changing auto-pan state"),
        ("*in_pChannel->programCounter++", "(u_char)*in_pChannel->programCounter++",
         "SoundVM_BC uses LBU for both byte operands"),
        ("Audio_Seq_kLfoWaveforms[in_pChannel->autoPanShape]", "nativeWave",
         "source selector65 is data, not an out-of-array native code pointer"),
    ],
    "F03276": [
        ("                    arg0->vibratoWave = Audio_Seq_kLfoWaveforms[arg0->vibratoShape];",
         "                    short* vibratoProgram = Audio_Seq_LfoWaveform(arg0->vibratoShape);\n"
         "                    if (vibratoProgram == NULL) return;\n"
         "                    arg0->vibratoWave = vibratoProgram;",
         "retrigger only a supported imported vibrato program"),
        ("                arg0->tremoloWave = Audio_Seq_kLfoWaveforms[arg0->tremoloShape];",
         "                short* tremoloProgram = Audio_Seq_LfoWaveform(arg0->tremoloShape);\n"
         "                if (tremoloProgram == NULL) return;\n"
         "                arg0->tremoloWave = tremoloProgram;",
         "retrigger only a supported imported tremolo program"),
    ],
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
        ("        switch (ret) {",
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
    "G00133": [
        ("int (*", "intptr_t (*", "normal opcode return contract"),
        ("Event_kOpcodeHandlers[]", "Event_kOpcodeHandlers[256]", "one handler per byte opcode"),
    ],
    **{rid: [(f"{name}[]", f"{name}[{count}]", evidence)] for rid, name, count, evidence in (
        ("G00029", "Battle_attackPrerequisiteFunctions", 40,
         "146C.c:_canPerformAttack; US table has forty functions before the hit-factor words"),
        ("G00032", "Battle_actionEffectMediators", 70,
         "146C.c:func_80085390; US table ends after the seventy effect mediators"),
        ("G00044", "Field_kTextureEffectHandlers", 7,
         "2842C.c:texture effect dispatch; seven handlers precede D_800E85E8"),
        ("G00045", "Field_kRoomEffectOpcodeHandlers", 13,
         "2842C.c:room effect dispatch; thirteen handlers precede numeric room-effect data"),
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
         "    Core_Memory_ZeroAlignedWords(Event_threadCursors, sizeof Event_threadCursors);\n"
         "    Core_Memory_ZeroAlignedWords(Event_threadWaitFrames, sizeof Event_threadWaitFrames);",
         "native globals survive overlay loads; a new gameplay session must not resume old event threads"),
    ],
    # BATTLE 4A0A8.c:2110 is called only through the opcode table; it ignores both dispatch arguments.
    "F01126": [("Event_ScriptWaitForScreenTransition(void)",
                 "Event_ScriptWaitForScreenTransition(u_char* arg0, short arg1)", "consistent opcode signature")],
    # 771A8.c:942 adds a direction delta to `arg0->unk34`, an int in the pinned view; the merged layout of
    # T00532 has the packed tile word as Battle::Ai::TilePosition (union of the word and its bytes).
    "F01731": [("arg0->tile + ", "arg0->tile.word + ", "packed tile word arithmetic (771A8.c:950)")],
    "F01728": [
        ("(*(u_char**)((uintptr_t)Core_ScratchBase() + 0x3C4))"
         "[position.tile.tileZ * *(u_char*)((uintptr_t)Core_ScratchBase() + 0x3DC) + position.tile.tileX]",
         "(*Battle_Ai_TerrainRegionCell(position.tile.tileX, position.tile.tileZ))",
         "771A8.c:587 validate route direction access against the authored payload"),
    ],
    "F01739": [
        ("&TERRAIN_TILE_MASK[state->tile.tile.tileZ * TERRAIN_MAP_WIDTH + state->tile.tile.tileX]",
         "Battle_Ai_TerrainRegionCell(state->tile.tile.tileX, state->tile.tile.tileZ)",
         "771A8.c:1517 retain bounded mutable direction-byte ownership"),
    ],
    "F01742": [
        ("(*(unsigned char**)((uintptr_t)Core_ScratchBase() + 0x3C4))"
         "[state->goal.tile.tileZ\n"
         "                                                       * (*(unsigned char*)((uintptr_t)Core_ScratchBase() + 0x3DC))\n"
         "                                                   + state->goal.tile.tileX]",
         "(*Battle_Ai_TerrainRegionCell(state->goal.tile.tileX, state->goal.tile.tileZ))",
         "771A8.c:1638 validate wander direction access against the authored payload"),
    ],
    # TITLE 22C.c _packageGameSaveData copies the inventory blocks with `savedata->unkXXX`, char arrays in its
    # view of the save record; the ledger type has them as Items::Inventory / Items::InventoryIndices structs.
    "F03601": [
        ("Core_Memory_CopyBytesBackward(savedata->inventory,", "Core_Memory_CopyBytesBackward(&savedata->inventory,",
         "array view of the inventory block (22C.c)"),
        ("Core_Memory_CopyBytesBackward(savedata->inventoryIndices,",
         "Core_Memory_CopyBytesBackward(&savedata->inventoryIndices,", "array view of the inventory indices (22C.c)"),
    ],
    # TITLE 22C.c _applyLoadedSaveFile: the same blocks copied back from the loaded save (22C.c:596).
    "F03600": [
        ("spmcimg[1].inventory,", "&spmcimg[1].inventory,", "array view of the inventory block (22C.c)"),
        ("spmcimg[1].inventoryIndices,", "&spmcimg[1].inventoryIndices,", "array view of the inventory indices (22C.c)"),
    ],
    # BATTLE 5BF94.c:1332 func_800C64D0 walks and splices an ordering-table list by raw 24-bit addresses (mask kept
    # in a variable, so the hazard index cannot see them as links): links are handles (Legacy/Render/LibGpu.h).
    "F01324": [
        ("intptr_t temp_t0 = ((uintptr_t)(arg0 - 1) & a3);", "u_long temp_t0 = LegacyGpu_Handle(arg0 - 1);",
         "link of the list's head entry"),
        ("arg0 = (u_long*)((*arg0 & a3) | 0x80000000);", "arg0 = (u_long*)LegacyGpu_Pointer(*arg0 & a3);",
         "follow a link"),
        ("((u_long)arg1 & a3)", "LegacyGpu_Handle(arg1)", "link to the spliced packet"),
    ],
    # The game heap is a fixed PS1 RAM range (0x8010C000, 0xF2000 bytes) in three start-up paths; natively it is
    # Core's arena (Core/Arena.h).
    **{fid: [("(Core_HeapBlockHeader*)0x8010C000, 0xF2000", "(Core_HeapBlockHeader*)Core_GameHeapArena(), "
              "CORE_GAME_HEAP_SIZE", "game heap base and size")] for fid in ("F01858", "F02435", "F03437")},
    # vs_overlay_slots (overlay.c) holds the PS1 load addresses of the overlay modules; natively they are offsets in
    # Core's overlay block (Core/Arena.h), which keeps the slots that overlap overlapping.
    "G02018": [(f"(void*)0x{a:08X}", f"Core_OverlayAddress(0x{a:08X}u)", "overlay load address")
               for a in (0x80068800, 0x800F9800, 0x80102800)],
    # The heap (main.c:8944-9018) counts in units of its 16-byte block header; on x64 the header holds two 8-byte
    # links and is 24 bytes, so the unit is the header's size.
    "F03465": [("((size + 15) >> 4) + 1", "((size + sizeof(Core_HeapBlockHeader) - 1) / sizeof(Core_HeapBlockHeader)) + 1",
                "heap unit is the block header")],
    "F03466": [("((size + 0xF) >> 4) + 1", "((size + sizeof(Core_HeapBlockHeader) - 1) / sizeof(Core_HeapBlockHeader)) + 1",
                "heap unit is the block header")],
    "F03467": [("value / 16 - 1", "value / sizeof(Core_HeapBlockHeader) - 1", "heap unit is the block header")],
    # main.c:8093 reaches the loading image's pixels as the words after its 4-byte size header (one rgb555Sized
    # asset in the module image); natively the pixels are their own global, filled from the same bytes.
    "F03421": [("(u_long*)&Render_legacyLoadingImageHeader + 1", "Render_loadingScreenPixels",
                "pixels follow the size header")],
    # The in-place relocations of on-disc pointers (ONDISC_OFFSETS below): natively the members keep their offsets.
    **{fid: [(f"    VS_ONDISC({b}, {m}, void *) = (char*)VS_ONDISC({b}, {m}, void *) + (intptr_t){base};\n", "",
              "on-disc offset kept (VS_ONDISC)") for b, m, base in rows]
       for fid, rows in {
           "F00793": [("model", m, "model") for m in ("soundSection", "groupSection", "vertexSection",
                                                       "polygonSection")],
           "F00798": [("seq", m, "seq") for m in ("firstSectionPointer", "secondSectionPointer")] +
                     [("seq", m, "(Actors_sharedSequenceAnimationData[slot][i] + 8)")
                      for m in ("firstSectionPointer", "secondSectionPointer")],
       }.items()},
    "F00797": [(f"    VS_ONDISC(arg0, {m}, void *) {op}= (intptr_t){b};\n", "", "on-disc offset kept (copy needs no fix)")
               for m in ("firstSectionPointer", "secondSectionPointer") for op, b in (("-", "arg1"), ("+", "arg0"))],
    # 146C.c:6480 reads the actor's weapon category as byte 37 of a char view of the Battle_Actor.
    "F00172": [("sp10.modelId = arg0[37];", "sp10.modelId = ((Battle_Actor*)arg0)->equippedWeaponCategory;",
                "Battle_Actor byte 37")],
    # 30DB0.c:1026 clears and copies actor records as 0x1900 bytes (the PS1 slot size): see FIXES["F00793"] below.
    # 146C.c:6578 lays out an enemy as one block by PS1 offsets: Battle_Actor, its stats, the model record at
    # 0x9B4 and two weapon model slots (0x22B4, 0x289C), or a second body record at 0x22B4. That block is
    # Actors::ActorAllocation; the spawn settings are the 8 bytes at 0x30 (a 32-bit flag run and reserved34).
    "F00176": [("actor = Core_Memory_AllocateFromLowEnd(0x3BB4);",
                "actor = Core_Memory_AllocateFromLowEnd(offsetof(Actors_ActorAllocation, modelSlots) "
                "+ sizeof(Actors_ActorRecord));", "allocation with a second body record"),
               ("actor = Core_Memory_AllocateFromLowEnd(0x2E84);",
                "actor = Core_Memory_AllocateFromLowEnd(sizeof(Actors_ActorAllocation));", "allocation"),
               ("actor->model = (void*)((char*)actor + 0x9B4);",
                "actor->model = (void*)&((Actors_ActorAllocation*)actor)->core.model;", "model record"),
               ("actor->equipmentModels[0] = (void*)((char*)actor + 0x22B4);",
                "actor->equipmentModels[0] = (void*)((Actors_ActorAllocation*)actor)->modelSlots;", "first slot"),
               ("actor->equipmentModels[1] = (void*)((char*)actor + 0x289C);",
                "actor->equipmentModels[1] = (void*)&((Actors_ActorAllocation*)actor)->modelSlots[1];",
                "second slot"),
               ("Actors_zoneCharacterInitData[zudIndex].characterKind",
                "Field_zoneContext.zndEnemies[zudIndex].recordBytes[2]",
                "D_800F188C aliases _zoneContext+0xC; use the same live table as the primary body"),
               ("*(Actors_SpawnSettings*)((char*)actor + 0x30) =",
                "*(Actors_SpawnSettings*)((char*)&actor->reserved34 - 4) =", "spawn settings at 0x30")],
    # 30DB0.c:1508 reads the actor's sequence table through `(char*)actor + 0x5F4` (+0x70 sectors, +0xA0 sizes).
    "F00796": [("sectors = *(u_short*)(sequences + 0xA0 + object->modelId * 2) >> 11;",
                "sectors = actor->chainAttackAnimationIds[object->modelId] >> 11;", "actor record 0x694"),
               ("Content_legacySequenceFileLocation.lba = *(u_int*)(sequences + 0x70 + object->modelId * 4);",
                "Content_legacySequenceFileLocation.lba = actor->battleSequenceLbas[object->modelId];",
                "actor record 0x664")],
    # 40564.c:1769 reads a gimmick instance through raw PS1 offsets: the flag byte at 8 (bit 7 transitionFlag), the
    # palettes at 0x12C/0x14C, and a 4-byte clear at 0x34 (slideX and velocityY).
    "F00980": [("flags = *(u_int*)((char*)arg0 + 8);", "flags = arg0->transitionFlag ? 0x80 : 0;", "flag byte at 8"),
               ("(void*)((char*)arg0 + 0x14C)", "(void*)arg0->pulsedPalette", "palette at 0x14C"),
               ("(void*)((char*)arg0 + 0x12C)", "(void*)arg0->basePalette", "palette at 0x12C"),
               ("(u_short*)((char*)object + 0x14C)", "(u_short*)object->pulsedPalette", "palette at 0x14C"),
               ("*(int*)((char*)object + 0x34) = 0;", "object->slideX = 0;\n            object->velocityY = 0;",
                "clear at 0x34")],
}

FIXES["F00798"] += [
    # _loadSeq / 30DB0.c:1766-1774: four-byte offsets relative to the payload after the offset table, not pointers.
    ("Actors_sharedSequenceAnimationData[slot][i] = *(char**)Content_legacySequenceParseData;",
     "Actors_sharedSequenceAnimationData[slot][i] = (char*)Content_legacySequenceParseData\n"
     "                + (count - i) * sizeof(u_int) + *(u_int*)Content_legacySequenceParseData;",
     "read one 32-bit file offset and resolve it relative to the shared sequence payload"),
    ("            Actors_sharedSequenceAnimationData[slot][i] += (intptr_t)Content_legacySequenceParseData;\n", "",
     "shared sequence entries already point into the payload"),
]

REGEX_FIXES: list[tuple[str, str, str, str]] = [
    *[(rf"^Content::{name}$", rf"\b{old}\b", new, evidence)
      for name, bindings, evidence in (
          ("AcquireOverlayWorkspace", (("arg0", "owner_tag"),),
           "F00333 146C.c:9562-9570 stores the claiming workspace owner tag"),
          ("ReleaseOverlayWorkspace", (("arg0", "owner_tag"),),
           "F00334 146C.c:9572-9580 compares the requesting owner tag before clearing ownership"),
          ("QueueAshleyWeaponSequence", (("arg0", "sequence_id"),),
           "F00779 30DB0.c:239-271;1474-1605 subkind3 queues this auxiliary SEQ selector through material"),
          ("UpdateWeaponModelLoad", (("arg0", "request"),),
           "F00787 30DB0.c:523-596 request fields drive model sharing and the staged WEP load"),
          ("ReleaseWeaponModel", (("var_v0", "shared_model_found"),),
           "F00790 30DB0.c:882-928 peer model match suppresses release of shared geometry"),
          ("FindSharedActorModel", (("arg0", "request"),),
           "F00791 30DB0.c:930-942 writes the matching peer slot into the asset request"),
          ("UpdateActorModelLoad", (("arg0", "request"),),
           "F00792 30DB0.c:944-1024 request fields drive model sharing and the staged SHP load"),
          ("FindSharedActorSequence",
           (("arg0", "request"), ("temp_v1", "candidate"),
            ("var_a2", "candidate_index"), ("t1", "model_id")),
           "F00794 30DB0.c:1433-1472 same SHP model and compatible SEQ identify the peer slot;F00795 is the identical alias"),
          ("CopyWeaponSequence",
           (("arg0", "destination"), ("arg1", "source")),
           "F00797 30DB0.c:1608-1615 native on-disc fixup preserves offsets during the fixed0x7800-byte copy"),
          ("BindSharedActorSequence",
           (("arg0", "actor"), ("arg1", "bank_index")),
           "F00799 30DB0.c:1795-1826 actor model selects a sequence from the zero-based shared bank"),
          ("ReleaseActorSequence",
           (("arg0", "target_id"), ("arg1", "sequence_kind")),
           "F00800 30DB0.c:1828-1892 kind selects zero-based actor slot or one-based shared bank;conversion stays in place"),
          ("ReleaseActorModel",
           (("arg0", "actor_index"), ("sp10", "sharing_request"),
            ("temp_s1", "actor"), ("a2", "request"),
            ("var_v0", "shared_model_found")),
           "F00803 30DB0.c:1920-1994 actor slot and peer-model search govern shared resource retirement"),
          ("ReleaseSequenceEffectTexture", (("arg0", "bank_id"),),
           "F00804 30DB0.c:1996-2006;native one-based1..3 bank gate precedes texture-frame and page release"),
          ("PFileTimelineSection",
           (("arg0", "timeline_block"), ("arg1", "section_index")),
           "F01555 5BF94.c:7874-7877 section index selects an offset relative to the timeline block"),
          ("PFileTimelineTrackData",
           (("arg0", "timeline_block"), ("arg1", "section_index"),
            ("arg2", "track_index")),
           "F01556 5BF94.c:7879-7883 track offset remains relative to the block rather than its section"),
      )
      for old, new in bindings],
    *[(rf"^Core::Math::{name}$", rf"\b{old}\b", new, evidence)
      for name, bindings, evidence in (
          ("InterpolateSevenBitScalar",
           (("arg0", "start"), ("arg1", "end"), ("arg2", "weight_q7")),
           "F01454 5BF94.c:5805-5808 signed delta product is shifted by seven before adding start"),
          ("InterpolateCosineScalar",
           (("arg0", "start"), ("arg1", "end"),
            ("arg2", "total_steps"), ("arg3", "current_step")),
           "F01457 5BF94.c:5859-5864 denominator precedes current step in the half-cycle cosine phase"),
          ("InterpolateShortPair",
           (("arg0", "endpoints"), ("arg1", "weight_q7"),
            ("new_var", "start")),
           "F01461 5BF94.c:5905-5909 short endpoint pair uses a seven-bit weight and signed shift"),
          ("InterpolateFixedPointRatio",
           (("arg0", "start"), ("arg1", "end"), ("arg2", "numerator"),
            ("arg3", "denominator"), ("var_v0", "scaled_delta")),
           "F01476 5BF94.c:6382-6386 quantizes the ratio to Q12 before multiplying the endpoint delta"),
          ("InterpolateLagrangeShortVector",
           (("temp_t3", "negative_phase"),
            ("temp_t2", "phase_minus_one"), ("temp_t1", "phase_plus_one"),
            ("temp_t0", "weight0_q12"), ("temp_t3_2", "weight1_q12"),
            ("temp_t2_2", "weight2_q12"), ("temp_v1_2", "weight3_q12")),
           "F01477 5BF94.c:6388-6410 factors and four Q12 control-point weights retain integer two-thirds rounding"),
          ("InterpolateLagrangeScalar",
           (("arg0", "value0"), ("arg1", "value1"), ("arg2", "value2"),
            ("arg3", "value3"), ("arg4", "phase_q12"),
            ("arg5", "destination")),
           "F01478 5BF94.c:6412-6422 four control values and a Q12 phase produce the pointed scalar result"),
          ("InterpolateThirteenBitScalar",
           (("arg0", "start"), ("arg1", "end"), ("arg2", "weight_q13"),
            ("var_v0", "scaled_delta")),
           "F01479 5BF94.c:6424-6428 divides the weighted delta by8192 with truncation toward zero"),
          ("InterpolateThirteenBitShortVector",
           (("arg0", "start"), ("arg1", "end"), ("arg2", "weight_q13"),
            ("arg3", "destination")),
           "F01480 5BF94.c:6430-6435 fourth argument receives three shared Q13 scalar interpolations"),
          ("InterpolateCosineShortVector",
           (("arg0", "start"), ("arg1", "end"), ("arg2", "total_steps"),
            ("arg3", "current_step"), ("arg4", "destination")),
           "F01481 5BF94.c:6437-6440;6020-6045 transform duration or curve extent precedes sampled progress"),
          ("InterpolateTimedShortVector",
           (("arg0", "start"), ("arg1", "end"), ("arg2", "total_steps"),
            ("arg3", "current_step"), ("arg4", "destination")),
           "F01482 5BF94.c:6442-6445;6020-6045 current progress is scaled by8192 then divided by total steps"),
          ("InterpolateThirteenBitWideVector",
           (("arg0", "start"), ("arg1", "end"), ("arg2", "weight_q13"),
            ("arg3", "destination")),
           "F01483 5BF94.c:6447-6452 fourth argument receives three shared Q13 scalar interpolations"),
          ("InterpolateCosineWideVector",
           (("arg0", "start"), ("arg1", "end"), ("arg2", "total_steps"),
            ("arg3", "current_step"), ("arg4", "destination")),
           "F01484 5BF94.c:6454-6457;6708-6733 effect camera passes duration then current frame"),
          ("InterpolateTimedWideVector",
           (("arg0", "start"), ("arg1", "end"), ("arg2", "total_steps"),
            ("arg3", "current_step"), ("arg4", "destination")),
           "F01485 5BF94.c:6459-6462;6681-6706 effect camera passes duration then current frame"),
          ("InterpolateCosineTimedScalar",
           (("arg0", "start"), ("arg1", "end"),
            ("arg2", "total_steps"), ("arg3", "current_step")),
           "F01486 5BF94.c:6464-6467;6708-6733 half-cycle cosine weight feeds the shared Q13 scalar"),
          ("InterpolateTimedScalar",
           (("arg0", "start"), ("arg1", "end"),
            ("arg2", "total_steps"), ("arg3", "current_step")),
           "F01487 5BF94.c:6469-6472;6681-6706 progress ratio feeds the shared Q13 scalar"),
          ("BuildLookAtOrientation",
           (("sp10", "direction"), ("sp20", "right_axis"),
            ("sp30", "forward_axis"), ("sp40", "up_axis")),
           "F00212 146C.c:8209-8236 direction and normalized cross products form the three rotation columns"),
          ("ClampSignedShort",
           (("arg0", "value"), ("arg1", "bound_a"), ("arg2", "bound_b"),
            ("var_t0", "lower"), ("var_v1", "upper")),
           "F00660 2842C.c:390-411 orders signed-short bounds before clamping the value"),
          ("AddPackedScreenCoordinates",
           (("arg0", "left_coordinates"), ("arg1", "right_coordinates")),
           "F00732 2842C.c:2576-2580 adds each packed coordinate without low-half carry"),
          ("AddVectors",
           (("arg0", "destination"), ("arg1", "left"), ("arg2", "right")),
           "F01241 4A0A8.c:4106-4110 first parameter receives the component sums"),
          ("SubtractVectors",
           (("arg0", "destination"), ("arg1", "left"), ("arg2", "right")),
           "F01242 4A0A8.c:4112-4116 first parameter receives left minus right"),
          ("ScaleFixedVector",
           (("arg0", "destination"), ("arg1", "source"),
            ("arg2", "scale_q12")),
           "F01243 4A0A8.c:4118-4123 scales each source component through the Q12 multiplier"),
          ("CopyVector", (("arg0", "destination"), ("arg1", "source")),
           "F01244 4A0A8.c:4125-4129 copies three components and returns the destination"),
          ("FixedVectorMagnitude",
           (("arg0", "source"), ("sp10", "components"),
            ("temp_lo", "x_squared"), ("temp_lo_2", "y_squared"),
            ("temp_v0", "z_squared")),
           "F01245 4A0A8.c:4131-4146 squares integer-downscaled components before restoring Q12 length"),
          ("ShortVectorToFixed",
           (("arg0", "destination"), ("arg1", "source")),
           "F01247 4A0A8.c:4155-4162 widens signed short components and multiplies them by ONE"),
          ("FixedAtan2", (("arg0", "y"), ("arg1", "x")),
           "F01249 4A0A8.c:4177 preserves ratan2 y/x argument order after shifting both by six"),
          ("DirectionRotationMatrix",
           (("arg0", "direction_q12"), ("arg1", "rotation"),
            ("sp10", "angles"), ("sp18", "integer_direction")),
           "F01250 4A0A8.c:4179-4191 downscales to SVECTOR then computes yaw/pitch with zero roll"),
          ("NormalizeFixedVector",
           (("arg0", "source"), ("arg1", "destination"),
            ("sp10", "integer_vector")),
           "F01251 4A0A8.c:4193-4199 downscales source components before VectorNormal writes the destination"),
          ("GreatestMagnitudeComponent",
           (("arg0", "first"), ("arg1", "second"), ("arg2", "third"),
            ("new_var", "zero"), ("var_t0", "first_magnitude"),
            ("var_a3", "second_magnitude"), ("var_v1", "third_magnitude"),
            ("var_v0", "select_third")),
           "F01456 5BF94.c:5827-5847 compares magnitudes but returns the selected signed component"),
          ("LargerMagnitude", (("arg0", "first"), ("arg1", "second")),
           "F04131 5BF94.c:5849-5858 keeps the first signed component on equal magnitudes"),
          ("SetIdentityRotation", (("arg0", "matrix"),),
           "F04132 5BF94.c:5922-5930 writes only the packed rotation words"),
          ("SetTranslationMatrix",
           (("arg0", "translation"), ("arg1", "matrix")),
           "F04133 5BF94.c:5932-5938 sets identity rotation and copies the supplied translation"),
          ("AddWideVectors",
           (("arg0", "left"), ("arg1", "right"), ("arg2", "destination")),
           "F01510 5BF94.c:6887-6892 third parameter receives the component sums"),
          ("AddWideAndShortVectors",
           (("arg0", "wide"), ("arg1", "narrow"), ("arg2", "destination")),
           "F01511 5BF94.c:6894-6899 third parameter receives wide plus signed-short components"),
          ("WidenShortVector",
           (("arg0", "source"), ("arg1", "destination")),
           "F01512 5BF94.c:6901-6906 copies signed-short components without fixed-point scaling"),
          ("NarrowWideVector",
           (("arg0", "source"), ("arg1", "destination")),
           "F01513 5BF94.c:6908-6913 narrows source components into the second parameter"),
      )
      for old, new in bindings],
    (r"^Input::MapStickDeadZone$", r"\barg0\b", "axis_byte",
     "5BE50.c:82-91 and 5BF94.c:667-668 map the raw right-stick axis byte outside64..191"),
    *[(r"^Input::PlayVibrationPreset$", rf"\b{old}\b", new,
       "main.c:139/8769-8785 four-byte directory entries hold little-endian pattern offsets after the count header")
      for old, new in (("arg0", "preset_index"),
                       ("new_var3", "directory_stride"),
                       ("new_var", "offset_byte1_index"),
                       ("new_var4", "offset_byte2_index"),
                       ("new_var2", "offset_byte3_index"))],
    *[(r"^Input::(?:SetMotorCommand|MotorCommand)$", rf"\b{old}\b", new,
       "main.c:8551-8562 actuator storage is indexed by controller port and motor")
      for old, new in (("port", "port_index"), ("pos", "motor_index"),
                       ("val", "command"))],
    *[(r"^Input::StartVibrationPattern$", rf"\b{old}\b", new,
       "main.c:8564-8593 and func_800433B4.s: slots are one-based; 255 requests a free slot")
      for old, new in (("arg0", "pattern"), ("arg1", "slot_request"),
                       ("arg2", "owner_tag"), ("i", "slot_index"),
                       ("ret", "slot_id"))],
    (r"^Input::(?:Clear|Pause|Resume)VibrationSlot$", r"\barg0\b", "slot_id",
     "main.c:8596/8630/8656 subtract one from the public slot ID before indexing storage"),
    (r"^Input::ClearVibrationSlot$", r"\bi\b", "slot_index",
     "main.c:8598 derives the zero-based slot index from the one-based ID"),
    (r"^Input::(?:Pause|Resume)VibrationSlot$", r"\bv0\b", "slot_index",
     "main.c:8632/8658 derive the zero-based slot index from the one-based ID"),
    (r"^Input::(?:Stop|Pause|Resume)VibrationPatterns$", r"\barg0\b", "slot_selector",
     "main.c:8607/8637/8663 accept one slot ID or zero to select all slots"),
    (r"^Input::StopVibrationPatterns$", r"\barg1\b", "owner_tag",
     "main.c:8616-8620 and 4A0A8.c:2523 match the owner tag assigned when starting a pattern"),
    (r"^Input::StopVibrationPatterns$", r"\bi\b", "slot_cursor",
     "main.c:8617-8625 uses a zero-based search cursor or increments it to the one-based clear ID"),
    (r"^Input::(?:Pause|Resume)VibrationPatterns$", r"\bi\b", "slot_id",
     "main.c:8650/8676 increments from zero before passing each one-based slot ID"),
    (r"^Input::ResetVibrationPatterns$", r"\bi\b", "slot_index",
     "main.c:8688 iterates the two stored vibration slots"),
    (r"^Input::RoundMotorIntensityTowardZero$", r"\barg\b", "intensity_q8",
     "main.c:8764-8765 and vs_main_setVibrateParams.s:8004387C-800438B4 round before shifting eight bits"),
    *[(r"^Input::UpdateVibrationPatterns$", rf"\b{old}\b", new,
       "main.c:8710-8756 and vs_main_setVibrateParams.s: slot/motor loops, bytecode cursor and six-word envelopes")
      for old, new in (("temp_a0", "pattern_offset"), ("i", "motor_index"),
                       ("j", "slot_index"), ("temp_t1", "pattern"),
                       ("a1", "motor_envelopes"),
                       ("new_var", "interpolate_intensity"))],
    (r"^Actors::UpdateAndDrawRoomActors$",
     r"(?s)if \(\((actor|attached)->soundEffect\.kind == 1\)\s*"
     r"&& \1->soundEffect\.parameter != 0\) \{.*?"
     r"\1->soundEffect\.parameter -= step;\s*\}",
     r"Audio_UpdateActorSoundTransition(\1);",
     "44F14.c:923 actor and companion inline copies share the source-backed typed sound transition"),
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
    *[(r"^Core::Memory::FreeFrom(?:High|Low)End$", r"\bvar_a1\b", "previous_free",
       "main.c:8884/8914 locate the free-list insertion predecessor before coalescing")],
    *[(r"^Core::Memory::AllocateFromHighEnd$", rf"\b{old}\b", new,
       "main.c:8944 searches backward and splits allocation units from the selected block tail")
      for old, new in (("var_a1", "allocation_block"),
                       ("var_a2", "following_free"),
                       ("blockSz", "allocation_units"))],
    *[(r"^Core::Memory::AllocateFromLowEnd$", rf"\b{old}\b", new,
       "main.c:8974 searches forward and preserves the split remainder as a free block")
      for old, new in (("var_a1", "allocation_block"),
                       ("var_v1", "previous_free"),
                       ("temp_v1", "remainder"),
                       ("blockSz", "allocation_units"))],
    (r"^Core::Memory::InitializeHeap$", r"\bvalue\b", "byte_count",
     "main.c:9008 divides the supplied arena byte extent by sizeof the native heap header"),
    (r"^Core::Memory::ReadUnalignedShort$", r"\barg0\b", "source",
     "573B8.c:146-152 reads two little-endian bytes from the supplied source address"),
    *[(r"^Core::Memory::ZeroWordsBackward$", rf"\b{old}\b", new,
       "5BF94.c:4476-4484 forms the end cursor from a byte extent then clears descending words")
      for old, new in (("arg0", "destination"), ("arg1", "byte_count"),
                       ("var_v0", "word_cursor"))],
    *[(r"^Core::Random::NextInterfaceRandomBits$", rf"\b{old}\b", new,
       "5BF94.c:4496-4501 returns high bits of the previous multiplicative state")
      for old, new in (("value", "bit_count"), ("temp_a1", "previous_state"))],
    *[(r"^Core::Random::RandomRangeExclusive$", rf"\b{old}\b", new,
       "5BF94.c:5810-5826 accepts either bound order and adds a modulo offset to the lesser")
      for old, new in (("arg0", "bound_a"), ("arg1", "bound_b"),
                       ("new_var", "offset"))],
    *[(r"^Core::Random::NextBounded$", rf"\b{old}\b", new,
       "main.c:8251-8275 scales the shuffle index then the complemented sample with signed rounding")
      for old, new in (("var_v0", "scaled_index"),
                       ("temp_s0", "picked_sample"),
                       ("var_a0", "scaled_result"))],
    (r"^Core::Random::NextAveraged$", r"\barg0\b", "max",
     "main.c:8277-8285 forwards the same maximum to twelve bounded draws before averaging"),
    *[(r"^Field::ClearSavedRoomEnemies$", rf"\b{old}\b", new,
       "146C.c:8509 forwards room and zone to the matching actor-state cache clear at146C.c:789")
      for old, new in (("arg0", "mapId"), ("arg1", "zoneId"))],
    *[(r"^Event::(?:Script|Command)", rf"\b{old}\b", new,
       "573B8.c:67-142 dispatches the current instruction and thread index through both opcode tables")
      for old, new in (("arg0", "instruction"), ("arg1", "thread_index"))],
    *[(r"^Content::CopyModelGeometry$", rf"\b{old}\b", new,
       "30DB0.c:507 verified geometry-copy parameter meaning")
      for old, new in (("arg0", "destination"), ("arg1", "source"),
                       ("arg2", "byteCount"))],
    *[(r"^Content::OffsetModelTextureCoordinates$", rf"\b{old}\b", new,
       "30DB0.c:445 verified flat/Gouraud UV-walker parameter and local meaning")
      for old, new in (("arg0", "polygonData"), ("arg1", "polygonCount"),
                       ("arg2", "colored"), ("arg3", "uOffset"),
                       ("arg4", "vOffset"), ("p", "cursor"),
                       ("i", "polygonIndex"), ("j", "vertexIndex"),
                       ("count", "remainingUvPairs"))],
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
        "#define _insertTPage(after, arg1)                                                              \\\n"
        "    do {                                                                                        \\\n"
        "        u_long* ot_ = (u_long*)Render_frameScratch.orderingTable + (after);                    \\\n"
        "        u_long* p_ = (u_long*)Render_frameScratch.primitiveCursor;                             \\\n"
        "        p_[1] = 0xE1000200 | ((arg1) & 0x1FF);                                                 \\\n"
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

FIXES["F01858"].append(
    ("0x2C10", "sizeof *Battle_actionState",
     "Allocate and initialize the actual native action state rather than its PS1 byte count")
)

# Reviewed fixes to copied type definitions: type id -> [(old, new, reason)]. Checked by Native\Tests\src\LayoutAsserts.c.
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
    "T00581": [
        ("int channels[10];",
         "int channels[12]; /* native: independent six-word envelopes for both motors */",
         "main.c:8584/8720 and MIPS 80043450/8004346C use an overlapping PS1 tail; native slots must not alias or overflow"),
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
    "T00456": [("int* reserved00;", "unsigned int reserved00;",
                 "room-effect commands begin with a four-byte size/opcode header, not a native pointer")],
    "T00453": [("int sceneEntry;", "int* sceneEntry;",
                 "runtime room-effect view shares the canonical native geometry pointer")],
    "T00542": [
        (f"u_int {field} :", f"u_short {field} :", "GIM flags occupy two bytes before vramPage and brightness")
        for field in ("drawEnabled", "loading", "textureMode", "drawRoutine", "loadPhase", "texturePageCount",
                      "ownsPalette", "reservedBits13")
    ],
    "T00219": [
        ("    int loaderAndImageData[0xFA];",
         "    union {\n        struct {\n            void* nativePointer10;\n"
         "            void* nativePointer14;\n            int reservedLoaderData[0xF8];\n"
         "        };\n        int loaderAndImageData[0xF8 + 2 * sizeof(void*) / sizeof(int)];\n    };",
         "event splash layers share the GIM renderer's native file/request pointer slots and array stride"),
    ],
    "T00237": [("u_char workArea[0x1FD0];",
                 "long projectedVertices[768];\n    int vertexDepths[768];\n"
                 "    unsigned projectedVertexCount;\n    u_char reserved1804[0x7CC];",
                 "4B16C.s:func_800B3A68 screen/depth arrays; retain the surrounding context extent")],
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

# 30DB0.c:1026 clears and copies actor records as 0x1900 bytes (the PS1 slot size).
FIXES["F00793"] = FIXES["F00793"] + [
    ("memset(actor, 0, 0x1900);", "memset(actor, 0, sizeof(Actors_ActorRecord));", "record size"),
    ("Core_Memory_CopyAlignedWords(actor, Actors_actorRecords[object->dataAddress], 0x1900);",
     "Core_Memory_CopyAlignedWords(actor, Actors_actorRecords[object->dataAddress], sizeof(Actors_ActorRecord));",
     "record size"),
    ("    model = Core_Memory_AllocateFromLowEnd(size);\n"
     "    memcpy(model, data, size);\n"
     "    actor->instance.model = model;\n"
     "    if (model == NULL) {\n        return -2;\n    }",
     "    if (size < (int)sizeof(Actors_ShpModel)) {\n        return -2;\n    }\n"
     "    model = Core_Memory_AllocateFromLowEnd(size);\n"
     "    if (model == NULL) {\n        return -2;\n    }\n"
     "    memcpy(model, data, size);\n"
     "    actor->instance.model = model;\n"
     "    actor->modelDataSize = (unsigned)size;",
     "retain allocation bounds and reject allocation failure before copying the SHP"),
]

FIXES["F03437"] = FIXES["F03437"] + [
    ("    Content_InitializeDiscQueue();",
     "    Battle_Ai_InvalidateSessionBindings();\n    Content_InitializeDiscQueue();",
     "native AI bindings must not outlive the game-arena reset in _sysReinit"),
    ("void Boot_InitializeTitleSession(void)\n{",
     "void Boot_InitializeTitleSession(void)\n{\n    Save_NativeResetSession();",
     "main.c:8309 discard staged native state before heap/module initialization"),
]

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
REGEX_FIXES.append((r"^Content::ParseWeaponModel$",
                    r"\n[ \t]*VS_ONDISC\((?:dst|buf), (?:textureSection|vertexGroupSection|vertexSection|polygonSection),"
                    r" void\*\) \+= \(u_long\)dst;", "", "on-disc offset kept (VS_ONDISC): relocation and re-basing"))
# Field::RoomRecord (146C.c): the 24 section slots after the header are filled as one array of pointers
# (`(&var_s0->geometry)[i]`, _loadMpdRoomSection); seven were declared int in the decomp's view.
TYPE_FIXES["T00329"] = [(f"    int {m};", f"    void* {m}; /* a section pointer (filled with the others) */",
                         "section slot") for m in ("lighting", "section6", "section7", "section8", "section15",
                                                    "section16", "section17")]
# Accessors that return those section pointers as int.
SIG_RETYPE: dict[str, dict] = {
    "F00572": {"ret": "intptr_t"},
    "F00590": {"ret": "intptr_t"},
    "F01281": {3: "Battle_TargetPreview*"},
}

# All 182 targets of BATTLE's 256 normal opcodes and INITBTL 12AC.c's skip table. The excluded rows are
# helpers, not handlers. Keep even control-code-only handlers compatible with the pointer-returning branches.
EVENT_OPCODE_HANDLERS = frozenset(
    {f"F{i:05}" for i in range(1027, 1208)} - {"F01033", "F01039", "F01186"}
    | {"F01221", "F01226", "F01229", "F01232"})
SIG_RETYPE.update({fid: {"ret": "intptr_t"} for fid in EVENT_OPCODE_HANDLERS})

# These normal-only handlers omit their unused thread argument in 4A0A8.c. They have no direct C callers,
# but the dispatch ABI still requires both arguments (check_event_dispatch.py checks all table targets).
for fid in ("F01055", "F01069", "F01071", "F01074", "F01075", "F01122", "F01143", "F01153", "F01178", "F01183"):
    FIXES.setdefault(fid, []).append(("(u_char* arg0)", "(u_char* arg0, short arg1)", "consistent opcode signature"))

# The MPD section E header (Field::MpdSectionE, 146C.c:15700): two array pointers into its own payload, set by
# _loadMpdRoomSection and read only by assembly-era code; offsets from the header natively.
ONDISC_OFFSETS.update({"T00337.003": "MPD section E first array", "T00337.004": "MPD section E second array"})
FIXES["F00551"] += [
    ("VS_ONDISC(sectionE, firstArray, int *) = (int*)(sectionE + 1);",
     "sectionE->firstArray = sizeof(*sectionE);", "offset of the payload"),
    ("VS_ONDISC(sectionE, secondArray, int *) = (int*)(sectionE + 1);",
     "sectionE->secondArray = sizeof(*sectionE);", "offset of the payload"),
    ("VS_ONDISC(sectionE, secondArray, int *) += sectionE->secondArrayOffset >> 2;",
     "sectionE->secondArray += (sectionE->secondArrayOffset >> 2) * sizeof(int);", "int-sized steps"),
    ("VS_ONDISC(sectionE, secondArray, int *) = NULL;", "sectionE->secondArray = 0;", "no second array"),
]


# Individually reviewed audio bindings; structural fixups run first.
REGEX_FIXES += [
    (rf"^{name}$", rf"\b{old}\b", new, evidence)
    for name, bindings, evidence in (
        ("Audio::PlayRandomActorImpactSound",
         (('arg0', 'sound_bank'), ('arg1', 'pan'), ('arg2', 'volume')),
         "F00063 146C.c:2144 forwards the model sound-section bank to func_800461CC, selects entries 8..10, and forwards pan/volume; 44F14.c:637 and main.c:10178 establish bank count/offset table and the two scalar meanings."),
        ("Audio::ActorSoundPanAndVolume",
         (('arg0', 'actor_index'), ('arg1', 'output_pan'), ('arg2', 'output_volume'), ('temp_s0', 'actor'), ('temp_v0', 'packed_pan_volume')),
         "F00994 40564.c:2789 indexes D_800F4538, projects its sound position, splits computeSfxPan's high/low halves into pan/volume and applies per-actor overrides; the volume output is nullable. main.c:9879 proves the packing."),
        ("Audio::PlayActorModelSound",
         (('arg0', 'model_instance'), ('arg1', 'sound_index')),
         "F00999 44F14.c:637 reads instance position, slot and model AKAO section; arg1 becomes type and selects a bank entry, with special randomized indices 4 and 8. This does not rename existing actor/type locals."),
        ("Audio::PlayActorModelSoundVariant",
         (('arg0', 'actor_index'), ('arg1', 'variant_index')),
         "F01000 44F14.c:685 indexes D_800F4538 and adds four to the requested bank sound variant before func_800AE4FC."),
        ("Audio::PlayActorSoundPair",
         (('arg0', 'source_actor'), ('arg1', 'first_effect_id'), ('arg2', 'second_effect_id'), ('sp10', 'pan_volume'), ('temp_s0', 'actor'), ('temp_v0', 'packed_pan_volume')),
         "F01001 44F14.c:690 resolves the actor by source instance slot, projects sound position, unpacks pan/volume into sp10[0]/[1], and plays one or two effect IDs; zero second ID suppresses that second call."),
        ("Audio::PlayGimmickSoundPair",
         (('arg0', 'gimmick'), ('arg1', 'first_effect_id'), ('arg2', 'second_effect_id'), ('temp_s0', 'sound_position')),
         "F01002 44F14.c:728 takes the SVECTOR at gimmick +0x1C and supplies the same position to one or two vs_main_panSfx calls. No assumption about ownership or pointer layout is added. Caller Actors/ActorState.c:375-380 obtains the object from Field_gimmickInstances[actorId], confirming the gimmick binding rather than assuming it from the function name."),
        ("Audio::PlayActorSurfaceContactSound",
         (('arg0', 'actor_index'), ('arg1', 'actor'), ('arg2', 'contact_sound_code'), ('sp10', 'weather_kind'), ('sp14', 'weather_intensity')),
         "F01003 44F14.c:739 reads actor tile/contact/surface, suppresses actor index 1, and selects banks by codes 0x41/0x42. Callers Animation.c:1524 (id,actor,op) and Movement.c:3121/3126 establish actor index and event/contact code. func_8008EB04 at 146C.c:17525 writes weather kind then intensity, used for the wet-surface +8 variant."),
        ("Audio::SetRoomAmbienceDucked",
         (('arg0', 'duck_enabled'),),
         "F00626 146C.c:17323 forwards the same duck flag to room-effect and weather-ambient controls; GameMode.c and Rewards.c pair 1 on entry with 0 on exit."),
        ("Audio::Control::SetMusicMuteMask",
         (('arg0', 'mute_mask'),),
         "F03100 25AC.c:459 writes the mask into command argument 1 and dispatches 0x90; its handler at 3648 assigns mutedMusicChannels and invalidates each channel volume. Preserve the void* legacy carrier."),
        ("Audio::Control::SetMusicBranchThreshold",
         (('arg0', 'jump_threshold'),),
         "F03101 25AC.c:465 dispatches 0x92 with argument 1; handler at 3660 stores the music jump comparison threshold, consumed by conditional bytecode jumps."),
        ("Audio::Control::PausePlaybackGroup",
         (('arg0', 'playback_group'),),
         "F03102 25AC.c:471 maps selector 1/2/3 to music/world-effects/cutscene pause commands 0x9B/0x9D/0x9F, defaulting to all groups 0x99."),
        ("Audio::Control::ResumePlaybackGroup",
         (('arg0', 'playback_group'),),
         "F03103 25AC.c:489 maps selector 1/2/3 to music/world-effects/cutscene resume commands 0x9A/0x9C/0x9E, defaulting to all groups 0x98."),
        ("Audio::Control::SetMixOptionLatch",
         (('arg0', 'enabled'),),
         "F03104 25AC.c:507 uses only zero/nonzero to clear/set mixBehavior bit 0x10. The deliberately neutral enable name does not assert an unverified option interpretation."),
        ("Audio::Control::SetSceneSoundEffectVolume",
         (('arg0', 'volume'),),
         "F03105 25AC.c:517 masks argument to 7-bit volume and dispatches 0xA8; handler 3302 stores scene-channel effectVolume at Q8 scale."),
        ("Audio::Control::FadeSceneSoundEffectVolume",
         (('arg0', 'fade_ticks'), ('arg1', 'target_volume')),
         "F03106 25AC.c:523 sends duration then 7-bit target to 0xA9; handler 3318 uses parameter 1 (or one) as divisor/remaining duration and parameter 2 as target effectVolume."),
        ("Audio::Control::SetSoundEffectVolume",
         (('arg0', 'effect_selector'), ('arg1', 'effect_tag_mask'), ('arg2', 'volume')),
         "F03107 25AC.c:530 sends selector, 24-bit tag mask and 7-bit volume to 0xA0; handler 3250 selects by intersecting tags or exact effectId, including pointer-valued IDs for blobs."),
        ("Audio::Control::FadeSoundEffectVolume",
         (('arg0', 'effect_selector'), ('arg1', 'effect_tag_mask'), ('arg2', 'fade_ticks'), ('arg3', 'target_volume')),
         "F03108 25AC.c:538 dispatches 0xA1; handler 3276 matches tags or effectId, uses parameter 3 as duration, and scales parameter 4 to target effectVolume."),
        ("Audio::Control::SetSceneSoundEffectPan",
         (('arg0', 'pan'),),
         "F03109 25AC.c:547 masks the byte sent to 0xAA; handler 3391 converts it through char to effectPan and invalidates volume. No signedness change is proposed."),
        ("Audio::Control::FadeSceneSoundEffectPan",
         (('arg0', 'fade_ticks'), ('arg1', 'target_pan')),
         "F03110 25AC.c:553 sends duration and masked pan byte to 0xAB; handler 3407 computes the effectPan slide with duration parameter 1 and signed-byte target parameter 2."),
        ("Audio::Control::SetSoundEffectPan",
         (('arg0', 'effect_selector'), ('arg1', 'effect_tag_mask'), ('arg2', 'pan')),
         "F03111 25AC.c:560 sends ID/blob selector, 24-bit tag mask and pan byte to 0xA2; handler 3339 matches tags or ID and sets effectPan."),
        ("Audio::Control::FadeSoundEffectPan",
         (('arg0', 'effect_selector'), ('arg1', 'effect_tag_mask'), ('arg2', 'fade_ticks'), ('arg3', 'target_pan')),
         "F03112 25AC.c:568 dispatches 0xA3; handler 3365 matches tags or ID, then uses parameter 3 duration and parameter 4 signed-byte pan target."),
        ("Audio::Control::SetSceneSoundEffectPitch",
         (('arg0', 'pitch_scale'),),
         "F03113 25AC.c:577 dispatches the masked pitch byte to 0xAC; handler 3481 sets effectPitchScale (not absolute note pitch) on non-interface channels."),
        ("Audio::Control::FadeSceneSoundEffectPitch",
         (('arg0', 'fade_ticks'), ('arg1', 'target_pitch_scale')),
         "F03114 25AC.c:583 sends duration and target byte to 0xAD; handler 3494 computes the non-interface-channel pitch-scale slide."),
        ("Audio::Control::SetSoundEffectPitch",
         (('arg0', 'effect_selector'), ('arg1', 'effect_tag_mask'), ('arg2', 'pitch_scale')),
         "F03115 25AC.c:590 dispatches 0xA4; handler 3427 matches tags or exact effectId and sets signed-char Q8 effectPitchScale."),
        ("Audio::Control::FadeSoundEffectPitch",
         (('arg0', 'effect_selector'), ('arg1', 'effect_tag_mask'), ('arg2', 'fade_ticks'), ('arg3', 'target_pitch_scale')),
         "F03116 25AC.c:598 dispatches 0xA5; handler 3453 matches tags or ID and divides target-minus-current pitch scale by duration parameter 3."),
        ("Audio::Control::SetMusicVolume",
         (('arg0', 'music_id'), ('arg1', 'volume')),
         "F03117 25AC.c:607 dispatches 0xC0; handler 3123 chooses active or secondary configuration by musicId (zero means active), then sets 7-bit masterVolume at Q16 scale."),
        ("Audio::Control::FadeMusicVolume",
         (('arg0', 'music_id'), ('arg1', 'fade_ticks'), ('arg2', 'target_volume')),
         "F03118 25AC.c:614 dispatches 0xC1; handler 3147 matches musicId, uses parameter 2 duration and parameter 3 target volume."),
        ("Audio::Control::FadeMusicVolumeFrom",
         (('arg0', 'music_id'), ('arg1', 'fade_ticks'), ('arg2', 'start_volume'), ('arg3', 'target_volume')),
         "F03119 25AC.c:622 dispatches 0xC2; handler 3184 resolves the selected music, writes parameter 3 as initial Q16 masterVolume and calculates the step toward parameter 4 over parameter 2 ticks."),
        ("Audio::Control::SetDiscVolume",
         (('arg0', 'volume'),),
         "F03120 25AC.c:631 dispatches 0xC8; handler 3216 clears the CD fade duration and stores the low 16-bit volume at Q16 scale before writing disc volume."),
        ("Audio::Control::FadeDiscVolume",
         (('arg0', 'fade_ticks'), ('arg1', 'target_volume')),
         "F03121 25AC.c:637 dispatches 0xC9; handler 3223 uses parameter 1 as duration and parameter 2 low half as the target CD volume."),
        ("Audio::Control::FadeDiscVolumeFrom",
         (('arg0', 'fade_ticks'), ('arg1', 'start_volume'), ('arg2', 'target_volume')),
         "F03122 25AC.c:644 dispatches 0xCA; handler 3237 writes parameter 2 as initial discVolume and computes a step toward parameter 3 over parameter 1 ticks."),
        ("Audio::Control::SetMusicTempoScale",
         (('arg0', 'tempo_scale'),),
         "F03123 25AC.c:652 dispatches masked byte through 0xD0; handler 3515 reads that byte signed and sets tempoScale shifted by 16, cancelling any slide."),
        ("Audio::Control::FadeMusicTempoScale",
         (('arg0', 'fade_ticks'), ('arg1', 'target_tempo_scale')),
         "F03124 25AC.c:658 dispatches 0xD1; handler 3521 uses duration parameter 1 and signed-byte target parameter 2."),
        ("Audio::Control::FadeMusicTempoScaleFrom",
         (('arg0', 'fade_ticks'), ('arg1', 'start_tempo_scale'), ('arg2', 'target_tempo_scale')),
         "F03125 25AC.c:665 dispatches 0xD2; handler 3532 sets scale from parameter 2 and step toward parameter 3. Its existing parameter-2 conditional for selecting the duration is preserved, not repaired."),
        ("Audio::Control::SetMusicPitchScale",
         (('arg0', 'pitch_scale'),),
         "F03126 25AC.c:673 dispatches masked byte through 0xD4; handler 3546 sets the signed-byte Q16 master pitch scale and clears its slide duration."),
        ("Audio::Control::FadeMusicPitchScale",
         (('arg0', 'fade_ticks'), ('arg1', 'target_pitch_scale')),
         "F03127 25AC.c:679 returns dispatch 0xD5; handler 3552 uses duration parameter 1 and signed-byte pitch-scale target parameter 2."),
        ("Audio::Control::FadeMusicPitchScaleFrom",
         (('arg0', 'fade_ticks'), ('arg1', 'start_pitch_scale'), ('arg2', 'target_pitch_scale')),
         "F03128 25AC.c:686 dispatches 0xD6; handler 3568 sets initial master pitch scale from parameter 2 and step toward parameter 3, preserving its existing duration-selection condition."),
        ("Audio::Control::SetMusicTempoAndPitch",
         (('arg0', 'tempo_pitch_scale'),),
         "F03129 25AC.c:694 dispatches 0xD8; dispatcher 3818 forwards the same byte to both 0xD0 tempo and 0xD4 pitch scale handlers."),
        ("Audio::Control::FadeMusicTempoAndPitch",
         (('arg0', 'fade_ticks'), ('arg1', 'target_tempo_pitch_scale')),
         "F03130 25AC.c:700 dispatches 0xD9; dispatcher 3818 copies the duration and target into both tempo (0xD1) and pitch (0xD5) slide handlers."),
        ("Audio::Control::FadeMusicTempoAndPitchFrom",
         (('arg0', 'fade_ticks'), ('arg1', 'start_tempo_pitch_scale'), ('arg2', 'target_tempo_pitch_scale')),
         "F03131 25AC.c:707 dispatches 0xDA; dispatcher 3818 forwards duration/start/target to tempo ramp 0xD2 and pitch ramp 0xD6 without changing either handler's existing behavior."),
        ("Audio::Control::StartCutsceneStream",
         (('arg0', 'stream_data'), ('arg1', 'pan'), ('arg2', 'reverb_enabled')),
         "F03145 25AC.c:928 checks AKAO magic and sends the stream pointer, pan byte shifted by 8 and third argument to 0xE0. Original StartStream at 6294 and native StreamPlayback.cpp:185 prove the third argument controls reverb, not looping or stereo format."),
        ("Audio::Control::SetCutsceneVolume",
         (('arg0', 'volume'),),
         "F03147 25AC.c:940 sends 7-bit volume shifted by 8 to 0xE4; original consumer 6513 and native SetStreamVolume assign the stream volume and cancel its fade."),
        ("Audio::Control::FadeCutsceneVolume",
         (('arg0', 'fade_ticks'), ('arg1', 'target_volume')),
         "F03148 25AC.c:946 sends duration and 7-bit target shifted by 8 to 0xE5; consumer 6525 uses duration (or one) as divisor and remaining ticks."),
        ("Audio::Control::SetCutscenePan",
         (('arg0', 'pan'),),
         "F03149 25AC.c:953 sends pan byte shifted by 8 to 0xE6; consumer 6537 writes the stream pan word. Native StreamPlayback preserves this packed-pan interface."),
        ("Audio::Control::SoundEffectChannelIds",
         (('arg0', 'output_effect_ids'), ('var_a1', 'channel')),
         "F03150 25AC.c:959 walks exactly twelve D_80035910 channels, stores each effectId through the incremented output pointer, and returns the scheduler active-voice mask. Field ledger T00278.012 identifies the IDs."),
        ("Audio::Cutscene::UploadSecondMonoBuffer",
         (('temp_s0', 'second_buffer_data'),),
         "F03381 25AC.c:6284 takes currentChunk+0x800 and writes exactly 0x800 bytes to sample address 0x2100 before the mono-start callback; the pointer is the second mono-buffer payload, not a voice index."),
        ("Audio::Cutscene::BeginStreamPlayback",
         (('var_s2', 'irq_sample_address'),),
         "F03384 25AC.c:6404 initializes the local from in_SampleAddr, substitutes silence address 0x1030 at end of data, and originally calls SpuSetIRQAddr(local+8). Native IRQ retirement leaves its assignments unused; name follows the proven original role, not a claim that the native dead local drives playback."),
        ("Audio::Driver::WriteDirtyVoiceParameters",
         (('arg2', 'unused_channel_flags'),),
         "F03180 25AC.c:1342 explicitly marks the third argument unused and obtains dirty flags from in_VoiceParams instead. Original callers at 1863 pass channel UpdateFlags; the new name records both supplied meaning and deliberate non-consumption, without dropping the ABI slot."),
        ("Audio::Mixer::SetSoundEffectPitchScale",
         (('arg0', 'command'), ('var_a1', 'channel'), ('var_a3', 'voice_bit')),
         "F03236 25AC.c:3427 handles command 0xA4: both tag-mask and exact-effectId branches walk twelve SFX channels with voice bits 12..23, set signed-byte Q8 effectPitchScale, clear its slide and mark pitch dirty. T00278.007/.012/.013 and T00289.000 corroborate those roles."),
        ("Audio::Mixer::SlideSoundEffectPitchScale",
         (('arg0', 'command'), ('var_a2', 'channel'), ('var_t0', 'voice_bit')),
         "F03237 25AC.c:3453 handles 0xA5: both selector branches walk the same channels/bits and use command duration/target fields to update effectPitchScaleSlideStep and remaining length."),
        ("Audio::Mixer::SetWorldSoundEffectPitchScale",
         (('arg0', 'command'), ('var_a1', 'channel')),
         "F03238 25AC.c:3481 walks twelve SFX records, excludes effectTags bit 0x02000000, and applies command parameter 1 as pitch scale. This particular loop does not consult the active-voice mask."),
        ("Audio::Mixer::SlideWorldSoundEffectPitchScale",
         (('arg0', 'command'), ('var_a1', 'fade_ticks'), ('var_a2', 'channel'), ('var_t0', 'voice_bit')),
         "F03239 25AC.c:3494 tests active SFX bits and excludes interface-tagged channels; var_a1 is command duration or one and is used as both pitch-step divisor and stored slide length."),
        ("Audio::Mixer::SetTempoScale",
         (('arg0', 'tempo_scale_argument'),),
         "F03240 25AC.c:3515 reads the first signed byte of the command parameter carrier, shifts it to Q16 tempoScale and clears slide length. Name does not turn the pointer into a scalar or a typed command struct."),
        ("Audio::Mixer::SlideTempoScale",
         (('arg0', 'command'), ('temp_v0', 'requested_fade_ticks'), ('var_a1', 'fade_ticks')),
         "F03241 25AC.c:3521 reads requested duration from param1, substitutes one only for zero, divides signed-byte target-minus-current tempoScale by that duration, and stores the length."),
        ("Audio::Mixer::SetTempoScaleRamp",
         (('arg0', 'command'), ('var_a1', 'fade_ticks')),
         "F03242 25AC.c:3532 starts duration at one, conditionally reads param1 when param2 is nonzero, sets initial tempoScale from param2 and steps toward param3. Existing conditional is preserved exactly."),
        ("Audio::Mixer::SetMusicPitchScale",
         (('arg0', 'pitch_scale_argument'),),
         "F03243 25AC.c:3546 reads the first signed byte of the command carrier into Q16 master pitch scale and clears its slide length."),
        ("Audio::Mixer::SlideMusicPitchScale",
         (('arg0', 'command'), ('temp_v0', 'requested_fade_ticks'), ('var_a1', 'fade_ticks')),
         "F03244 25AC.c:3552 reads requested duration, substitutes one for zero, divides the master-pitch-scale delta by it and stores the short duration."),
        ("Audio::Mixer::SetMusicPitchScaleRamp",
         (('arg0', 'command'), ('var_a1', 'fade_ticks')),
         "F03245 25AC.c:3568 uses the same source-specific param2 conditional for selecting the duration, sets initial pitch scale from param2 and calculates a step toward param3. Naming is not a duration-condition fix."),
        ("Audio::Mixer::StopWorldSoundEffects",
         (('var_s1', 'voice_bit'), ('var_s2', 'channel')),
         "F03248 25AC.c:3607 walks active SFX voice bits 12..23 alongside channels, skips interface-tagged effects, requests key-off, releases each scheduler bit and clears the channel update flags."),
        ("Audio::Mixer::SetMusicMuteMask",
         (('arg0', 'command'),),
         "F03251 25AC.c:3648 reads command param1 as the mute mask and marks all music-channel volumes dirty; the parameter is a command record, not a channel."),
        ("Audio::Mixer::SetMusicJumpThreshold",
         (('arg0', 'command'),),
         "F03252 25AC.c:3660 writes command param1 into active music JumpThresholdValue (T00279.030)."),
        ("Audio::Mixer::PauseMusic",
         (('arg0', 'unused_command'), ('temp_v1', 'paused_channel_mask'), ('v0', 'non_effect_voice_mask'), ('var_s0', 'voice_index'), ('var_s1', 'voices_to_pause'), ('var_s2', 'voice_bit')),
         "F03253 25AC.c:3665 explicitly ignores the command argument. v0 complements active SFX/cutscene masks; var_s1 clips it to 24 voices and removes handled bits, var_s2 walks each bit and var_s0 indexes driver calls. temp_v1 transfers activeChannels into pausedChannels before activeChannels is cleared."),
        ("Audio::Mixer::ResumeMusic",
         (('temp_v1', 'resumed_channel_mask'), ('var_a1', 'channel_bit'), ('var_a2', 'channels_to_resume')),
         "F03254 25AC.c:3698 walks paused music channel bits to invalidate voice parameters, then moves the saved mask back to activeChannels. The shrinking work mask and restored full mask are kept distinct."),
        ("Audio::Mixer::PauseWorldSoundEffects",
         (('var_a0', 'channel'), ('var_s0', 'voice_bit'), ('var_s1', 'voices_to_pause'), ('var_s2', 'voice_index')),
         "F03255 25AC.c:3722 first filters active SFX bits against interface-tagged channels, saves the world-effects mask, then visits physical voice indices starting at 12 to silence/pause them while consuming the mask."),
        ("Audio::Mixer::ResumeWorldSoundEffects",
         (('temp_a2', 'resumed_voice_mask'), ('var_a1', 'voice_bit'), ('var_a2', 'voices_to_resume')),
         "F03256 25AC.c:3761 walks D_800378A0 paused SFX bits to mark channels dirty, then transfers voiceScheduler.pausedEffectVoices back into activeVoices. Native Mixer/VoiceMixer.h defines pausedSoundEffectVoices as a view of that same pausedEffectVoices member; the shrinking work mask, bit cursor and saved full mask are distinct local lifetimes, not different underlying state."),
        ("Audio::Mixer::UpdateChannelSlidesAndDelays",
         (('var_lo', 'vibrato_amplitude'),),
         "F03181 25AC.c:1473-1488 computes the new pitch-scaled vibrato base from depth magnitude, selecting full-pitch or 15/256 scaling by the depth high bit; both assignments feed VibratoBase (T00278.050), not a register-specific operation."),
        ("Audio::Mixer::FindFreeVoice",
         (('arg0', 'search_control_or_voice_index'), ('var_v1', 'voice_envelope')),
         "F03185 25AC.c:1832 first interprets arg0 as the selector for searching from zero versus voiceSearchStart, then overwrites it with the cursor/returned free voice index (or 24). Compound name explicitly preserves the merged parameter/cursor lifetime. var_v1 walks envelope records and zero envelope means free (T00290.001)."),
        ("Audio::Mixer::ApplyMusicChannelVoices",
         (('sp10', 'channels_to_key_on'), ('temp_s0', 'keyed_channel_mask'), ('var_a0', 'voice_index'), ('var_s2', 'channel_bit'), ('var_s5', 'channel_index')),
         "F03186 25AC.c:1863 sp10 intersects selected channels with pendingKeyOn; the loop advances channel_bit/index together. temp_s0 intersects keyedVoices and feeds the free/steal allocator; var_a0 is its physical voice result or sentinel 24. Native ConfigureChannel replacement preserves these decisions."),
        ("Audio::Mixer::UnassignVoice",
         (('arg1', 'voice_index'),),
         "F03187 25AC.c:1926 scans primary and optional secondary channel arrays and resets any assignedVoice equal to arg1 to sentinel VOICE_COUNT."),
        ("Audio::Mixer::ApplySavedMusicMasterFade",
         (('arg1', 'unused_channels'),),
         "F03190 25AC.c:2006 explicitly ignores the channel-array parameter while applying the global master fade to the supplied config and saving its old volume. Original CommitPlaybackVoices passes the saved channel array; preserve the unused callback argument."),
        ("Audio::Mixer::RestoreSavedMusicVolume",
         (('arg1', 'unused_channels'),),
         "F03191 25AC.c:2020 explicitly ignores the channel-array parameter and restores only config masterVolume from masterFade.savedValue. Preserve the existing two-argument shape and distinct paired restore function."),
        ("Audio::SetZoneMusicSwitchEnabled",
         (('arg0', 'enabled'),),
         "F00237 146C.c:8579 directly assigns the zone-music-switch latch; Event/ScriptField.c passes the enable bit extracted from instruction[7]."),
        ("Audio::PlayActorScriptSound",
         (('arg0', 'actor_index'), ('arg1', 'sound_index')),
         "F00272 146C.c:8917 indexes vs_battle_actors, checks the index against 16, and passes its model plus the requested sound index to func_800AE4FC."),
        ("Audio::SetMusicJumpThreshold",
         (('arg0', 'jump_threshold'),),
         "F03489 main.c:9494 forwards the value to func_80012468; 25AC.c:465 dispatches command 0x92, whose handler at 3660 stores JumpThresholdValue (T00279.030)."),
        ("Audio::StopMusicSlotPlayback",
         (('new_var', 'empty_music_handle'), ('var_a1', 'slot_index')),
         "F03490 main.c:9496 uses 0xFFFF as the empty handle sentinel and walks the four music-handle slots from 3 to 0; all other stores clear the selected/current slot state."),
        ("Audio::ResumeMusicSlot",
         (('temp_v0', 'music_handle'),),
         "F03491 main.c:9525 queues the selected slot's sequence through func_800120E8 and stores its nonzero sequence ID result in the slot handle and playing handle (G01954/T00603)."),
        ("Audio::CrossfadeMusicSlot",
         (('arg1', 'target_volume'), ('arg2', 'fade_ticks'), ('temp_v0', 'music_handle')),
         "F03492 main.c:9545 passes data, arg2, arg1 to func_80012080. 25AC.c:318 and dispatcher 3818 move them to extParam1 duration/extParam2 target used at 3001; result is the sequence ID stored as a handle."),
        ("Audio::PlayMusicSlot",
         (('arg1', 'target_volume'), ('arg2', 'fade_ticks'), ('temp_v0', 'music_handle')),
         "F03493 main.c:9565 calls volume fade with (0,arg2,arg1), starts the slot sequence, stores its handle, and repeats the fade. 25AC.c:614/3147 prove ticks versus volume."),
        ("Audio::MusicSlotVolume",
         (('arg0', 'slot_id'),),
         "F03504 main.c:9733 subtracts one from a nonzero public slot ID, rejects the 0xFFFF handle, and queries its volume."),
        ("Audio::QueryMusicSlotVolume",
         (('arg0', 'slot_id'),),
         "F03505 main.c:9743 is a direct wrapper around func_800454B8; its sole argument remains the same one-based slot selector."),
        ("Audio::FadeMusicSlotVolume",
         (('arg1', 'target_volume'), ('arg2', 'fade_ticks')),
         "F03506 main.c:9745 checks the current slot and calls func_800128A0(playingHandle,arg2,arg1); the command consumer at 3147 divides target-current volume by duration."),
        ("Audio::FadeMusicSlotTempo",
         (('arg1', 'target_tempo_scale'), ('arg2', 'fade_ticks')),
         "F03507 main.c:9754 calls func_800129D0(arg2,arg1); 25AC.c:658/3521 proves duration first and signed-byte tempo scale second."),
        ("Audio::FadeMusicSlotPitch",
         (('arg1', 'target_pitch_scale'), ('arg2', 'fade_ticks')),
         "F03508 main.c:9763 calls func_80012A6C(arg2,arg1); 25AC.c:679/3552 proves duration first and signed-byte pitch scale second."),
        ("Audio::Playback::ActivePlaybackFlags",
         (('var_v1', 'playback_flags'),),
         "F03077 25AC.c:223 accumulates active primary/secondary music, queued music, SFX and cutscene flags (1,2,0x10,0x100,0x10000) and returns that bitset."),
        ("Audio::Playback::MusicVolume",
         (('arg0', 'music_id'), ('var_v1', 'music_config'), ('var_a1', 'volume')),
         "F03078 25AC.c:243 searches active then secondary configuration by musicId; only the selected config's Q16 masterVolume high half is returned, or zero when absent."),
        ("Audio::Playback::LongestMusicFade",
         (('arg0', 'fade_selector_mask'), ('var_a1', 'longest_fade_ticks')),
         "F03080 25AC.c:261 bits 1/2/4 select master-volume, tempo-scale and pitch-scale fades; the local retains the maximum remaining duration among selected fades."),
        ("Audio::Playback::StartMusic",
         (('arg0', 'sequence_data'),),
         "F03082 25AC.c:286 dispatches pointer through command 0x10; dispatcher 3818 validates its AKAO header and returns sequenceId, while the handler loads the sequence."),
        ("Audio::Playback::StopMusic",
         (('arg0', 'music_id'),),
         "F03083 25AC.c:292 sends ID through 0x11; StopMatchingMusic calls Sound_KillMusicConfig at 2506, where zero matches all and other values compare against musicId."),
        ("Audio::Playback::StartLayeredMusic",
         (('arg0', 'sequence_data'), ('arg1', 'volume'), ('temp_v0', 'music_id')),
         "F03086 25AC.c:308 starts via command 0x19, applies func_8001286C to the returned sequence ID with arg1 volume, and returns that ID. Do not repair the separate existing extended-parameter behavior."),
        ("Audio::Playback::CrossfadeMusic",
         (('arg0', 'sequence_data'), ('arg1', 'fade_ticks'), ('arg2', 'target_volume')),
         "F03087 25AC.c:318 supplies sequence, duration and 7-bit target to 0x1A. Dispatcher maps duration/target to extended parameters consumed by the crossfade handler at 3001."),
        ("Audio::Playback::StartMusicAtLoop",
         (('arg0', 'sequence_data'), ('arg1', 'loop_count')),
         "F03088 25AC.c:326 dispatches 0x12; dispatcher stores argument 2 as parameter 4, and handler 3034 derives musicLoopsRemaining from loop_count minus one when nonzero."),
        ("Audio::Playback::QueueMusicTransition",
         (('arg0', 'sequence_data'),),
         "F03089 25AC.c:333 queues a pointer through 0x14; dispatcher validates AKAO, marks pending music and returns its sequence ID."),
        ("Audio::Playback::StartMusicPair",
         (('arg0', 'primary_sequence'), ('arg1', 'secondary_sequence_address'), ('v0', 'packed_music_ids')),
         "F03090 25AC.c:339 command 0x16 consumes two sequence addresses; dispatcher 3818 loads the second into saved music and the first into active music, replaces both arguments with sequence IDs, and this wrapper packs secondary ID high / primary ID low. Preserve the legacy int and void* carriers."),
        ("Audio::Playback::PlaySoundEffect",
         (('arg0', 'effect_id'), ('arg1', 'effect_tag'), ('arg2', 'pan'), ('arg3', 'volume'), ('temp_s0', 'program_offset')),
         "F03092 25AC.c:352 masks effect ID to 10 bits, reads its program-offset entry, rejects -1, then dispatches tag/pan/volume masked to 24/8/7 bits through 0x20. The returned local remains the original offset, not a playback handle."),
        ("Audio::Playback::PlaySoundEffectBlob",
         (('arg0', 'program_blob'), ('arg1', 'effect_tag'), ('arg2', 'pan'), ('arg3', 'volume'), ('var_v0', 'program_or_failure')),
         "F03093 25AC.c:365 initializes return pointer to -1, validates AKAO, dispatches blob/tag/pan/volume through 0x24 and returns the original blob on success. Native uintptr_t preserves the same sentinel/address role."),
        ("Audio::Playback::StopSoundEffects",
         (('arg0', 'effect_selector'), ('arg1', 'effect_tag_mask')),
         "F03094 25AC.c:381 dispatches selector and 24-bit mask through 0x21; consumer 3118 forwards them to Sound_EvictSfxVoice. Selector can be ID, blob address, zero or special sentinel; effect_selector deliberately does not narrow that contract."),
        ("Audio::Playback::PlayInterfaceSound",
         (('arg0', 'effect_id'),),
         "F03095 25AC.c:388 masks the built-in effect ID to 10 bits and dispatches 0x30, which resolves its two program counters and applies fixed interface pan/volume."),
        ("Audio::Playback::ActiveSoundEffectTags",
         (('var_a0', 'empty_or_effect_tags'), ('var_a1', 'channel'), ('var_v1', 'voice_bit')),
         "F03096 25AC.c:394 first uses var_a0 for the no-active-voices early-exit test, then resets it to zero to OR effectTags from active SFX channels. var_v1 walks bits 12..23 alongside the channel pointer. The compound name preserves both lifetimes."),
        ("Audio::Playback::SoundEffectIdForTag",
         (('arg0', 'effect_tag'), ('var_a1', 'channel'), ('var_v1', 'voice_bit'), ('a2', 'active_voice_mask')),
         "F03097 25AC.c:421 snapshots active voices, walks the twelve SFX channels and tests exact tag equality, returning their effectId. It is not a tag-intersection mask API."),
        ("Audio::Playback::UsesSoundEffectTags",
         (('arg0', 'program_or_effect_id'),),
         "F03098 25AC.c:439 interprets a legacy high address as an SFX header and reads byte-9 bit 7, otherwise indexes metadata by numeric ID and reads bit 15. Name preserves the dual pointer/ID contract; no native pointer-threshold fix is claimed."),
        ("Audio::Playback::SetMonoOutput",
         (('arg0', 'mono_enabled'),),
         "F03099 25AC.c:450 dispatches mono 0x81 only for value 1, otherwise stereo 0x80; new name does not change this exact equality test."),
        ("Audio::Samples::LoadBank",
         (('arg0', 'bank_data'), ('arg1', 'wait_for_transfer')),
         "F03134 25AC.c:719 repeatedly calls the declared-bank loader with the same bank pointer and wait flag. The consumer func_800135D8 at 1033 forwards that flag to _waitTransferAvailable; native SequencerOutput keeps but ignores wait without changing the signature."),
        ("Audio::Samples::ChooseMusicBankDestination",
         (('arg0', 'bank_header'), ('arg1', 'output_first_instrument'), ('arg2', 'output_sample_address'), ('var_a3', 'use_alternate_bank')),
         "F03136 25AC.c:727 reads bank ID/header counts and sample length, selects primary or alternate music memory (instrument 0x40/0x60; sample address 0x35100/0x55100), writes the two outputs, updates T00201 IDs and returns the alternate-bank choice. All four bindings have distinct roles."),
        ("Audio::Samples::ChooseSoundEffectBankDestination",
         (('arg0', 'bank_header'), ('arg1', 'output_first_instrument'), ('arg2', 'output_sample_address'), ('arg3', 'bank_slot')),
         "F03137 25AC.c:768 removes duplicate bank IDs, chooses one of three SFX slots, writes instrument bases 0x80/0xA0/0xC0 and sample addresses 0xB100/0x13100/0x1B100, then returns the requested slot. Default handling remains unchanged."),
        ("Audio::Samples::SelectMusicBank",
         (('arg0', 'bank_id'),),
         "F03138 25AC.c:797 compares the requested ID against primary and alternate music bank IDs and adjusts mixBehavior bit 0x10000 accordingly."),
        ("Audio::Samples::LoadMusicBank",
         (('arg0', 'bank_data'), ('arg1', 'wait_for_transfer'), ('sp10', 'first_instrument'), ('sp14', 'sample_address'), ('temp_s0', 'alternate_bank_selected')),
         "F03142 25AC.c:894 obtains instrument/sample destinations from func_80012C14, forwards those and the wait flag to func_800135D8, and returns the saved alternate-bank selector. Native bank upload ownership changes do not change these bindings."),
        ("Audio::Samples::LoadSoundEffectBank",
         (('arg0', 'bank_data'), ('arg1', 'bank_slot'), ('arg2', 'wait_for_transfer'), ('sp10', 'first_instrument'), ('sp14', 'sample_address')),
         "F03143 25AC.c:903 obtains destinations using arg1 SFX bank slot, then uploads arg0 with arg2 wait flag and the two output destinations. This differs from LoadMusicBank's arg1 meaning."),
        ("Audio::Samples::LoadDeclaredBank",
         (('arg0', 'bank_data'), ('arg1', 'wait_for_transfer')),
         "F03158 25AC.c:1023 validates AKAO magic and uploads using header words 6/4 as instrument/sample destinations. Original upload at 1033 establishes arg1 wait semantics; returns 0/-1 independently of that flag."),
        ("Audio::Samples::SoundEffectBankSlot",
         (('var_v1', 'bank_slot'),),
         "F03211 25AC.c:2941 removes the metadata high bit, compares the ID against SFX bank IDs 1 and 2, and returns slot 0/1/2; the local is not a voice or instrument index."),
        ("Audio::Seq::LoadInstrument",
         (('var_s0', 'instrument_index'),),
         "F03306 25AC.c:5314 resolves the program byte through music or SFX bank mapping, indexes g_InstrumentInfo and assigns InstrumentIndex. Both branches yield the same instrument-index domain."),
        ("Audio::Seq::PlayEmbeddedSoundEffect",
         (('temp_v0', 'relative_program_offset'), ('var_a1', 'first_program'), ('var_a2', 'second_program')),
         "F03371 25AC.c:6162 separately decodes two little-endian relative offsets at pc and pc+2, maps zero to NULL, and passes the resulting two bytecode starts to Sound_PlaySfxProgram. The reused offset local has the same role both times."),
        ("Audio::Seq::SelectNoteRemapTable",
         (('temp_v0', 'remap_byte_offset'),),
         "F03373 25AC.c:6202 fetches one byte and takes &D_8002F66C[temp_v0] from a char table as the note remap base. Name is a byte offset, not a guessed table ordinal or pitch."),
        ("Audio::Seq::EnableVibrato",
         (('new_var', 'pitch_base_copy'),),
         "F03317 25AC.c:5462 copies the ushort PitchBase into int before both vibrato-base multiplication branches. Native waveform validation changes neither use; this is not the wave shape or depth value."),
        ("Audio::Seq::ResetSoundEffectChannel",
         (('arg0', 'channel'), ('arg1', 'program_start')),
         "F03199 25AC.c:2330 resets one SFX channel's volume, note/slide/LFO state, assigns its programCounter from arg1, and selects initial instrument zero. T00278.000 identifies programCounter."),
        ("Audio::Seq::AssignedVoiceMask",
         (('arg0', 'channel_cursor'), ('arg1', 'channel_mask'), ('var_a3', 'assigned_voice_mask')),
         "F03200 25AC.c:2363 walks 32 channel records, tests each selected channel bit, and ORs the physical assignedVoice bit only below sentinel 24. Input channel mask and returned voice mask are different domains."),
        ("Audio::Seq::ReleaseMusicConfiguration",
         (('arg2', 'music_id_filter'),),
         "F03202 25AC.c:2506 releases the supplied active config only if arg2 is zero or equals its musicId, resets channel programs and clears matching voice-owner records. No broader mask meaning is implied."),
        ("Audio::Seq::StartMusicLayer",
         (('arg0', 'command'),),
         "F03214 25AC.c:2983 receives sequence pointer/music ID/extended initial volume in the command, optionally saves active music, loads the new sequence and releases saved voice conflicts. Native snapshot replacement is preserved."),
        ("Audio::Seq::CrossfadeMusicLayers",
         (('arg0', 'command'), ('temp_a1', 'fade_ticks')),
         "F03215 25AC.c:3001 command carries sequence pointer/ID and extended duration/target volume. temp_a1 caches extParam1 for the saved layer's fade timer and negative fade step; active layer fades up using the same duration."),
        ("Audio::Seq::SetMusicVolume",
         (('arg0', 'command'), ('temp_v1', 'music_id'), ('var_a0', 'music_config'), ('var_a1', 'channels')),
         "F03222 25AC.c:3123 selects active music for zero/matching ID or secondary for its matching ID; config/channels stay paired in both branches, receive master volume and invalidate channel volumes."),
        ("Audio::Seq::FadeMusicVolume",
         (('arg0', 'command'), ('temp_a1', 'music_id'), ('var_a0', 'music_config'), ('var_a1', 'channels'), ('var_a2', 'fade_ticks'), ('var_v1', 'volume_step_work')),
         "F03223 25AC.c:3147 substitutes duration one for zero, pairs selected config/channels by music ID, and transforms var_v1 from target Q16 volume to delta then per-tick step before storing masterVolumeStep. The work name covers all three arithmetic stages."),
        ("Audio::Seq::FadeMusicVolumeFrom",
         (('arg0', 'command'), ('temp_v0', 'requested_fade_ticks'), ('temp_v1', 'music_id'), ('temp_v1_2', 'start_volume_q16'), ('var_a0', 'music_config'), ('var_a1', 'channels'), ('var_a2', 'fade_ticks')),
         "F03224 25AC.c:3184 resolves the selected active/secondary pair, initializes masterVolume from param3 low seven bits shifted by 16, and computes the step toward param4 using param2 duration or one. The two temp_v1 bindings have independently verified meanings."),
        ("Audio::Seq::SetDiscVolume",
         (('arg0', 'command'),),
         "F03225 25AC.c:3216 reads param1 low half as CD volume, shifts it by 16, clears the fade and updates disc output."),
        ("Audio::Seq::FadeDiscVolume",
         (('arg0', 'command'), ('temp_v0', 'requested_fade_ticks'), ('var_a1', 'fade_ticks')),
         "F03226 25AC.c:3223 reads duration parameter 1, substitutes one for zero, and computes discVolumeFadeStep toward parameter 2 low-half target at Q16 scale."),
        ("Audio::Seq::FadeDiscVolumeFrom",
         (('arg0', 'command'), ('new_var', 'target_volume_q16'), ('var_a1', 'fade_ticks')),
         "F03227 25AC.c:3237 creates target disc-volume Q16 from param3, starts from param2, and divides target-current by param1 duration or one."),
        ("Audio::Seq::UpdateGlobalSlides",
         (('temp_s1', 'next_slide_value'), ('var_a1_2', 'effect_channel'), ('var_a2', 'channel_count_or_voice_bit'), ('var_a3', 'effects_to_update'), ('var_v1', 'music_channel')),
         "F03265 25AC.c:4024 uses temp_s1 for each interpolated pitch/volume/pan value, compares integer portions and commits it. var_v1 walks all music channels during pitch invalidation; var_a1_2 walks SFX. var_a3 consumes active SFX bits. var_a2 is first the 32-channel countdown and later the SFX voice-bit cursor; compound name preserves both lifetimes. Native cutscene-update extraction removes that original branch, not these remaining roles."),
        ("Audio::Seq::AdvanceMusicTransition",
         (('temp_v1', 'packed_music_timer'),),
         "F03266 25AC.c:4138 combines lower count and upper count shifted by 16, writes musicTransitionTimer and tests its zero boundary before changing transition latches. It is not the top timer or a raw elapsed tick count."),
        ("Audio::Seq::UpdateMusicSequence",
         (('arg0', 'channels'), ('arg1', 'secondary_music'), ('var_a0', 'tempo_increment'), ('var_s1', 'channel_bit'), ('var_s2', 'channel_cursor'), ('var_s3', 'channels_to_update')),
         "F03267 25AC.c:4174 scales tempo high-half into the accumulator increment, then consumes active-channel bits while walking the supplied array. arg1 is 0 for primary and 1 for secondary in TickPlayback, gating reverb updates and primary loop countdown. Channel cursor is reset from arg0 on repeated loop ticks."),
        ("Audio::Seq::UpdateSavedMusicFade",
         (('arg0', 'music_config'), ('arg1', 'channels'), ('temp_s0', 'next_fade_value'), ('temp_v0', 'channels_to_release')),
         "F03268 25AC.c:4264 moves activeChannels into pendingKeyOff when the global saved fade expires; otherwise computes next masterFade.value and invalidates the supplied config/channel pair on integer-volume change."),
        ("Audio::Seq::TickPlayback",
         (('temp_v0', 'promoted_saved_config'), ('var_a0', 'transition_config'), ('var_s1', 'effect_voice_bit'), ('var_s2_2', 'channel_cursor'), ('var_s5', 'tick_counter_or_cost')),
         "F03269 25AC.c:4287 temp_v0 holds the secondary config being promoted/cleared; var_a0 selects which transition config loses resume-key bits. var_s2_2 walks new-music channels then SFX channels; var_s1 is SFX voice bits 12..23. var_s5 first saves RCntCNT2, then becomes the wrapped elapsed cost stored in recentTickCosts and returned. Compound timing name preserves both stages. Native Tick/ConfigureChannel adaptations are not modified."),
        ("Audio::Seq::ResolveSoundEffectInstrument",
         (('arg0', 'sample_bank_flags'), ('arg1', 'instrument_index')),
         "F03271 25AC.c:4550 maps instruments 0x80..0x9F by bank flag 2 or 4 to the second/third SFX bank. Producer at 2656 stores 1<<command.ExtParam1 (bank slot) in channel.unk38, whose ledger name is effectVoiceMask; no ledger rename is proposed."),
        ("Audio::Seq::ResolveMusicInstrument",
         (('arg0', 'music_status_flags'), ('arg1', 'instrument_index')),
         "F03272 25AC.c:4563 adds 0x20 for alternate-bank status bit 0x40 on indices 0x40..0x7F; otherwise returns the unchanged instrument index. Input flags come from active music status, unlike the SFX resolver."),
        ("Audio::Seq::SelectNotePatch",
         (('arg0', 'channel'), ('arg1', 'note_key'), ('temp_a0', 'instrument'), ('temp_s2', 'update_flags'), ('temp_v0', 'instrument_index'), ('var_s0', 'patch_entry')),
         "F03273 25AC.c:4571 scans eight-byte keymap/patch entries against the requested key and current key; resolves entry[0] to a music instrument; applies its sample addresses and envelope words under channel update flags; copies entry-specific ADSR and volume scale. Each pointer/index has a distinct domain, with no layout or search correction."),
        ("Audio::Seq::CalculateNotePitch",
         (('arg0', 'instrument'), ('arg1', 'note_cursor'), ('arg2', 'fine_tune'), ('arg3', 'output_fine_pitch_delta'), ('var_a0', 'semitone_or_octave_offset'), ('var_t0', 'pitch_work')),
         "F03274 25AC.c:4637 derives root-relative semitone and later octave offsets in var_a0, computes instrument-tuned pitch in var_t0, applies fine-tune contribution through arg3, scales both for octaves and finally shifts by eight. arg1 is the requested note then a cursor advanced by octaves below the root. Compound/work names cover all arithmetic stages without claiming one fixed Q format throughout."),
        ("Audio::Seq::StartDrumNote",
         (('arg0', 'channel'), ('arg1', 'channel_bit'), ('arg2', 'drum_key'), ('temp_a0', 'instrument'), ('temp_s1', 'drum_entry'), ('temp_s2', 'update_flags'), ('temp_v0', 'note_pitch'), ('temp_v0_2', 'instrument_index')),
         "F03275 25AC.c:4686 indexes an eight-byte drum keymap entry, resolves its instrument, applies sample/envelope data under channel update flags, computes pitch using entry[1], and sets pan/reverb. temp_v0 and temp_v0_2 are independently pitch and instrument index, respectively."),
        ("Audio::Seq::ExecuteChannelStep",
         (('arg0', 'channel'), ('arg1', 'channel_bit'), ('sp10', 'pitch_delta_work'), ('temp_a1', 'program_cursor'), ('temp_a1_2', 'note_update_flags'), ('temp_a2_2', 'opcode_or_pitch_work'), ('temp_s0', 'note_or_update_flags'), ('temp_v0', 'octave_semitones'), ('temp_v1', 'explicit_note_opcode_index'), ('temp_v1_6', 'vibrato_depth_magnitude'), ('var_s0', 'step_opcode'), ('var_v1', 'gate_ticks')),
         "F03276 25AC.c:4760-4952 full body reviewed: program_cursor fetches/increments the bytecode; step_opcode also normalizes explicit-length notes and stop commands. temp_v1 is opcode-0xF0; temp_v0 is octave*12; temp_a1_2 snapshots note update flags; var_v1 calculates gate duration. temp_v1_6 is the seven-bit vibrato-depth magnitude. Three merged lifetimes are deliberately named for every use: temp_a2_2 holds extended opcode, peeked next-note opcode, computed note pitch, and finally Q16 bend target; temp_s0 holds the selected/transposed note then modulation update flags; sp10 is random pitch deviation, then the otherwise-unused fine-pitch output for bend calculation. arg1 is the music-channel or SFX-voice bit supplied by both tick callers. All source/native LFO guard branches were read; no variable splitting, signedness changes, or opcode behavior edits are proposed."),
        ("Audio::Seq::ApplyInstrumentParameters",
         (('arg0', 'channel'), ('arg1', 'instrument'), ('arg2', 'sample_start_address')),
         "F03277 25AC.c:4953 copies sampleStart from argument 3, loop/envelope words from the instrument and marks the channel voice dirty. No copying of unrelated fields or pointer-size change is proposed."),
        ("Audio::Seq::SelectChannelInstrument",
         (('arg0', 'channel'), ('arg1', 'instrument_index')),
         "F03278 25AC.c:4963 stores the index and passes the indexed instrument plus its sampleStart to Sound_CopyInstrumentInfoToChannel."),
        ("Audio::Seq::ReleaseSoundEffectVoice",
         (('arg0', 'channel'), ('arg1', 'voice_bit'), ('temp_a1', 'keep_voice_mask')),
         "F03279 25AC.c:4970 complements the released bit, ANDs that keep mask into active/paused/noise/reverb/FM/key-on/keyed scheduler masks, and clears the supplied channel's effect tags/ID. pendingKeyOff is intentionally not added to this list."),
        ("Audio::PlaySfxOnPrimaryChannel",
         (('arg0', 'effect_id'),),
         "F03512 main.c:9785 masks the effect ID to 16 bits before passing it with group-1 tag, centered pan and maximum volume to func_8001217C."),
        ("Audio::PlayBuiltInSfxChannel",
         (('arg1', 'effect_tag'), ('arg2', 'pan'), ('arg3', 'volume')),
         "F03513 main.c:9794 compares tag 0x200's current effect ID to suppress a duplicate; all playback branches pass tag, pan and volume to 25AC.c:352, where those fields are masked to 24,8,7 bits respectively."),
        ("Audio::PlaySfxProgramChannel",
         (('arg1', 'effect_tag'), ('arg2', 'pan'), ('arg3', 'volume')),
         "F03514 main.c:9805 performs the same tag-0x200 identity check for a blob address, then forwards tag/pan/volume to func_800121F0 at 25AC.c:365."),
        ("Audio::PlaySfx",
         (('arg0', 'channel_group'), ('arg2', 'pan'), ('arg3', 'volume'), ('var_s0_2', 'bank_program')),
         "F03515 main.c:9816 switches group masks and rotates tags. For private/room groups the local first receives the bank base then advances by its indexed relative offset to the program. Native null/range guards do not change these roles. Pan/volume reach 25AC.c:352/365."),
        ("Audio::PlaySfxCentered",
         (('arg0', 'channel_group'),),
         "F03516 main.c:9877 forwards the group and ID with constant pan 0x80 and volume 0x7F."),
        ("Audio::SfxPanAndVolume",
         (('v0', 'coordinate_or_attenuation_work'), ('v1', 'depth_or_volume_limit'), ('new_var2', 'attenuated_volume')),
         "F03517 main.c:9879 uses v0 first for the signed packed-coordinate high half, then for distance attenuation shifted by 5; v1 first holds clamped z-768 and finally the maximum-volume constant 127. The compound names preserve these exact merged lifetimes, rather than incorrectly naming either variable solely pan or depth. new_var2 is 127 minus attenuation, returned in the low half."),
        ("Audio::PlayProjectedSfx",
         (('arg0', 'channel_group'), ('arg1', 'effect_id')),
         "F03518 main.c:9927 unpacks computed pan/volume from projected coordinates and forwards the unchanged group/effect ID only when volume is positive."),
        ("Audio::PlayWorldSfx",
         (('arg0', 'channel_group'), ('arg1', 'effect_id')),
         "F03519 main.c:9937 projects the supplied SVECTOR through the view matrix then calls _do_panSfx with unchanged group/effect ID."),
        ("Audio::ResolveSfxProgram",
         (('arg0', 'channel_group'), ('arg1', 'effect_id'), ('temp_v1', 'program_address')),
         "F03520 main.c:9950 returns zero for built-in groups; for private/room masks the local receives the selected bank address then adds the indexed offset. Its native intptr_t carrier and existing int casts remain unchanged."),
        ("Audio::FadeSfxVolume",
         (('arg0', 'channel_group'), ('arg1', 'effect_id'), ('arg2', 'target_volume'), ('arg3', 'fade_ticks'), ('temp_v0', 'program_address')),
         "F03521 main.c:9975 resolves a bank program if possible, otherwise uses the built-in ID, or selects by group when ID is zero. func_8001264C receives duration before target volume, verified at 25AC.c:538/3276."),
        ("Audio::FadeSfxPitch",
         (('arg0', 'channel_group'), ('arg1', 'effect_id'), ('arg2', 'target_pitch_scale'), ('arg3', 'fade_ticks'), ('temp_v0', 'program_address')),
         "F03522 main.c:9994 resolves program address or built-in ID, or selects a whole group; func_80012824 receives duration then target signed-byte pitch scale (25AC.c:598/3453)."),
        ("Audio::FadeSfxPan",
         (('arg0', 'group_or_program_selector'), ('arg1', 'effect_id'), ('arg2', 'target_pan'), ('arg3', 'fade_ticks')),
         "F03523 main.c:10012 reuses arg0: initially a group mask, then the resolved program address on the nonzero-ID branch. The compound selector name covers both actual uses. func_80012738 receives duration and target pan (25AC.c:568/3365)."),
        ("Audio::StopSfx",
         (('arg0', 'group_or_program_selector'), ('arg1', 'effect_id')),
         "F03524 main.c:10028 reuses arg0 from group mask to resolved program address before StopSoundEffects; ID zero keeps it as the tag mask. No pointer-width or dispatch correction is proposed."),
        ("Audio::PlayBankSfx",
         (('arg0', 'channel_group'), ('arg1', 'sound_bank'), ('arg2', 'sound_index'), ('arg3', 'pan'), ('arg4', 'volume'), ('var_s0', 'effect_tag'), ('temp_s1', 'bank_program')),
         "F03535 main.c:10178 checks the bank's count, resolves bank-relative offset[index+1], selects/rotates a group tag, and calls func_800456EC(program,tag,pan,volume). The local bank_program initially holds the base, then the selected entry."),
        ("Audio::PlayBankSfxCentered",
         (('arg0', 'channel_group'), ('arg1', 'sound_bank'), ('arg2', 'sound_index')),
         "F03536 main.c:10215 is the count/offset-table bank playback wrapper with constant pan 0x80 and volume 0x7F."),
        ("Audio::PlayProjectedBankSfx",
         (('arg0', 'channel_group'), ('arg1', 'sound_bank'), ('arg2', 'sound_index'), ('arg3', 'projection_or_pan'), ('arg4', 'projected_depth'), ('temp_t0', 'depth_or_volume'), ('var_v0', 'pan_work'), ('var_v1', 'coordinate_or_depth_work')),
         "F03537 main.c:10220: arg3 changes from packed screen coordinate to horizontal-coordinate-derived pan; var_v0 carries rounded high-half removal and the half-X calculation. var_v1 starts as packed coordinates and is reused for rounded depth. temp_t0 clamps depth to 0..1024 then becomes 127-depth/8. Compound names deliberately cover all uses before func_800461CC."),
        ("Audio::PlayWorldBankSfx",
         (('arg0', 'channel_group'), ('arg1', 'sound_bank'), ('arg2', 'sound_index')),
         "F03538 main.c:10272 projects a world SVECTOR and forwards group/bank/index unchanged to the projected bank playback helper."),
        ("Audio::StopBankSfx",
         (('arg0', 'channel_group'), ('arg1', 'sound_bank'), ('arg2', 'sound_index'), ('var_a0', 'program_address'), ('var_a1', 'effect_tag_mask')),
         "F03539 main.c:10285 computes the bank-relative program pointer; nonzero index selects by that pointer with zero tag mask, while zero index clears the pointer and uses the supplied group as mask."),
        ("Audio::FadeBankSfxVolume",
         (('arg0', 'channel_group'), ('arg1', 'sound_bank'), ('arg2', 'sound_index'), ('arg3', 'target_volume'), ('arg4', 'fade_ticks'), ('a0', 'program_address')),
         "F03540 main.c:10299 computes the bank-relative program address. Index zero selects the whole group; otherwise the address is used. func_8001264C receives arg4 low byte as duration and arg3 as target volume."),
        ("Audio::BindRoomSfxBank",
         (('arg0', 'bank_address'),),
         "F03533 main.c:10161 stores the address in the room-bank slot. Native copy fix corrects the original unk14[16] alias to sfxBankData[2]; no change to uintptr_t or ownership is proposed."),
        ("Audio::LoadWaveBankFile",
         (('temp_v1', 'packed_bank_ids'), ('var_a0', 'alternate_bank_id'), ('new_var', 'packed_ids_or_unused_one')),
         "F03547 main.c:10359 obtains packed current/alternate bank IDs from func_80011F08, extracts the signed-rounded upper half and compares both IDs before queueing. new_var first copies the packed IDs then receives an unused 1; compound name documents both lifetimes without removing that dead assignment."),
        ("Audio::UpdateWaveBankLoad",
         (('temp_v0', 'transfer_status'),),
         "F03549 main.c:10396 reads vs_sound_spuTransferring only in the dispatched state; 0 releases successful load resources, -1 releases failed load resources, and other values remain pending. No state/reset behavior changes."),
        ("Audio::SoundEffects::PlayInterfaceProgramPair",
         (('arg0', 'command'), ('temp_a1', 'first_program'), ('temp_a2', 'second_program')),
         "F03217 25AC.c:3046 saves two program pointers from command params 1/2, replaces those fields with the interface ID/tag and fixed pan/volume, then starts the saved pair. Both locals retain distinct bytecode starts throughout."),
        ("Audio::SoundEffects::PlayInterfaceProgram",
         (('arg0', 'command'), ('sp10', 'first_program'), ('sp14', 'second_program')),
         "F03218 25AC.c:3061 resolves two program counters for the requested effect through Sound_GetProgramCounters (2777), adds interface tag/fixed pan-volume and sample-bank slot, and starts that pair."),
        ("Audio::SoundEffects::PlaySharedProgram",
         (('arg0', 'command'),),
         "F03219 25AC.c:3074 reads effect ID from the command, resolves both shared program counters and bank metadata, and forwards the same command record to Sound_PlaySfxProgram."),
        ("Audio::SoundEffects::StopPrograms",
         (('arg0', 'command'),),
         "F03221 25AC.c:3118 forwards command params 1/2 as effect selector and tag mask to Sound_EvictSfxVoice. Existing selector/sentinel behavior is preserved."),
        ("Audio::SoundEffects::SetProgramVolume",
         (('arg0', 'command'), ('var_a1', 'channel'), ('var_a3', 'voice_bit')),
         "F03228 25AC.c:3250 both tag-mask and exact-ID branches walk twelve SFX channels with bits 12..23, set Q8 effectVolume from parameter 3, clear its fade and dirty voice volume. This binding was reviewed independently from pan and pitch handlers."),
        ("Audio::SoundEffects::FadeProgramVolume",
         (('arg0', 'command'), ('var_a2', 'channel'), ('var_t0', 'voice_bit')),
         "F03229 25AC.c:3276 walks both selector branches identically, using parameter 3 or one as duration and parameter 4 low seven bits as target effectVolume; channel and voice-bit cursors always advance together."),
        ("Audio::SoundEffects::SetSceneVolume",
         (('arg0', 'command'), ('var_a1', 'channel'), ('var_a3', 'voice_bit')),
         "F03230 25AC.c:3302 tests active SFX bits and excludes interface tag 0x02000000 before setting parameter-1 scene volume. Channel pointer and physical voice bit walk the twelve entries."),
        ("Audio::SoundEffects::FadeSceneVolume",
         (('arg0', 'command'), ('var_a1', 'fade_ticks'), ('var_a2', 'channel'), ('var_t0', 'voice_bit')),
         "F03231 25AC.c:3318 walks active non-interface effects; var_a1 is command duration or one and feeds both the volume-step divisor and slide length, not a channel pointer as in SetSceneVolume."),
        ("Audio::SoundEffects::SetProgramPan",
         (('arg0', 'command'), ('var_a1', 'channel'), ('var_a3', 'voice_bit')),
         "F03232 25AC.c:3339 selects active effects by tag intersection or exact ID, sets signed-char Q8 effectPan from parameter 3 and cancels its fade; both branches establish the same channel/bit roles."),
        ("Audio::SoundEffects::FadeProgramPan",
         (('arg0', 'command'), ('var_a2', 'channel'), ('var_t0', 'voice_bit')),
         "F03233 25AC.c:3365 selects active effects in both tag/ID branches and computes effectPanSlideStep from signed-byte parameter 4 over parameter 3 duration (or one)."),
        ("Audio::SoundEffects::SetScenePan",
         (('arg0', 'command'), ('var_a1', 'channel'), ('var_a3', 'voice_bit')),
         "F03234 25AC.c:3391 sets parameter-1 pan on active non-interface channels; var_a3 walks bits 12..23 while var_a1 walks SFX records."),
        ("Audio::SoundEffects::FadeScenePan",
         (('arg0', 'command'), ('var_a1', 'fade_ticks'), ('var_a2', 'channel'), ('var_t0', 'voice_bit')),
         "F03235 25AC.c:3407 computes scene pan fades only for active non-interface SFX channels; var_a1 is the checked duration, var_a2 the channel pointer and var_t0 the corresponding physical voice bit."),
        ("Audio::PlayCutsceneStreamSlot",
         (('arg0', 'slot_id'), ('arg1', 'pan'), ('arg2', 'volume')),
         "F03542 main.c:10312 validates a one-based slot (1..3), applies immediate volume, and starts its bound stream at the supplied pan with reverb enabled; 25AC.c:928/946 and 6294 confirm the parameter roles."),
        ("Audio::PlayCutsceneStreamSlotCentered",
         (('arg0', 'slot_id'),),
         "F03543 main.c:10324 forwards the one-based stream slot with pan 0x80 and volume 0x7F."),
        ("Audio::FreeCutsceneStreamSlot",
         (('arg0', 'slot_id'),),
         "F03544 main.c:10326 validates slot 1..3, stops the stream, frees its buffer and clears that same slot."),
    )
    for old, new in bindings
]
