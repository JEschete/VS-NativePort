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
    "F03397": ("Core_Gte_RotMatrixYXZ", r"Native\Engine\src\Core\Gte.cpp"),
    "F03398": ("Core_Gte_RotMatrix", r"Native\Engine\src\Core\Gte.cpp"),
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

MODULE_DATA_BLOB_TYPES = {"Content_residentLfoSamples": ("short", 2)}
MODULE_SAMPLE_POINTERS = {"G01840": "Content_residentLfoSamples"}

MODULE_FUNCTION_ENTRIES = (
    ("BATTLE/BATTLE.PRG", 0x800DBA4C, "F04203"),
    ("BATTLE/BATTLE.PRG", 0x800DBA88, "F04204"),
    ("BATTLE/BATTLE.PRG", 0x800DBB98, "F04205"),
    ("BATTLE/BATTLE.PRG", 0x800DBC0C, "F04206"),
)

FIXES: dict[str, list[tuple[str, str, str]]] = {
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
