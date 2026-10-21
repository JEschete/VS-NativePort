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
    **{name: r"Native\Game\src\Battle\EncounterHooks.h"
       for name in (
           "Boot_EncounterRoomPlacements",
           "Boot_EncounterUsesRoomEvents",
           "Boot_EncounterLoadRoomEvents",
           "Boot_EncounterPollStop",
       )},
    "Boot_InitializePlayerSettings": r"Native\Game\src\Boot\PlayerSettings.h",
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
    ("SLUS_010.40", 0x2019C, 0x8002F99C, 0x804, "Content_residentLfoSamples",
     "25AC.c:SoundVM_B4_Vibrato; sixteen short waveform sequences precede g_Sound_LfoTable"),
)

MODULE_FUNCTION_ENTRIES = (
    ("BATTLE/BATTLE.PRG", 0x800DBA4C, "F04203"),
    ("BATTLE/BATTLE.PRG", 0x800DBA88, "F04204"),
    ("BATTLE/BATTLE.PRG", 0x800DBB98, "F04205"),
    ("BATTLE/BATTLE.PRG", 0x800DBC0C, "F04206"),
)

FIXES: dict[str, list[tuple[str, str, str]]] = {
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
    ],
    "F01535": [
        ("    /* The original caller supplies arguments to this argument-free wrapper. */\n", "",
         "the retained assembly renderer now has an explicit native particle-list contract"),
        ("((void (*)())Effects_DrawEffectParticleList)(arg0->particles, (void*)((uintptr_t)Core_ScratchBase() + 0x15C));",
         "Effects_DrawEffectParticleList(arg0->particles);",
         "forward native particle ownership through a type-checked call"),
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
    "F02553": [
        (r'"MISC\00000|0xF8|"',
         r'"\x16\x12\x1C\x0C\xE7\x00\x00\xF8"',
         "C48.c:485 pragma vsstring encodes MISC then E7 termination; preserve reset bytes and avoid runaway glyph packets"),
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
    "F00539": [
        ("    if (dataLen != 0) {",
         "    Boot_EncounterRoomPlacements(&data, &dataLen);\n"
         "    if (dataLen != 0) {",
         "explicit encounter definitions replace room placements before the production spawn/cache lifecycle"),
    ],
    "F00543": [
        ("        temp_s4 = Event_LoadRoomEventSections((void*)(arg0),",
         "        if (!Boot_EncounterUsesRoomEvents()) {\n"
         "            temp_s4 = Boot_EncounterLoadRoomEvents();\n"
         "        } else {\n"
         "        temp_s4 = Event_LoadRoomEventSections((void*)(arg0),",
         "isolated encounters declare an empty story/door-event set; normal room loading remains unchanged"),
        ("            Boot_gameEntryMode == new_var);",
         "            Boot_gameEntryMode == new_var);\n        }",
         "close the explicit room-event policy branch"),
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
    "F02535": [
        ("((u_long)poly << 8) >> 8", "LegacyGpu_Handle(poly)",
         "action-cost panel packets retain native handles"),
    ],
    "F02536": [
        ("((uintptr_t)var_t2 << 8) >> 8", "LegacyGpu_Handle(var_t2)",
         "button-prompt background at ordering entry15 must preserve traversal to submenu labels below it"),
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
                 '    if (actor == NULL) Core_Debugf("actors", "movement references missing actor %d", id);',
                 "identify a failed script actor binding before the required movement operation")],
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
    "F03423": [("    while (DsControlB(DslPause, NULL, NULL) == 0)\n        ;\n", "",
                 "main.c:8119; native extracted-file reads have no CD transport to pause")],
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
    ],
    # func_800AEF94 / 44F14.c:923: room actor slots are sparse, including an absent companion in slot 1.
    "F01008": [
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
    ],
        "F01006": [("*(MATRIX*)&source", "*source", "44F14.c:853; copy the supplied camera matrix, not pointer-stack bytes")],
    # func_800AE47C / 44F14.c:610: the SEQ header starts with offsets, not a native pointer.
    "F00998": [
        ("u_char** v0 = ((u_char** (*)(Actors_ActorRecord*, u_char, u_short**))Actors_FindActorAnimationHeader)(arg0, arg0->animationId, &sp10);",
         "Actors_SeqAnimationBank* v0 = (Actors_SeqAnimationBank*)Actors_FindActorAnimationHeader(arg0, arg0->animationId, &sp10);",
         "typed on-disc sequence header"),
        ("u_char* temp_a3 = *v0;", "u_char* temp_a3 = VS_ONDISC(v0, animationData, u_char*);",
         "resolve the event stream's 32-bit offset"),
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
    "G00574": [
        ("Effects_effectRenderers[]", "Effects_effectRenderers[8]",
         "eight native function pointers occupy the original table range 0x800F56A8..0x800F56C8"),
    ],
    "G00579": [("Battle_Ai_actorAiStates[]", "Battle_Ai_actorAiStates[16]", "AI actor pointer table extent")],
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
        ("G01840", "Audio_Seq_kLfoWaveforms", 16,
         "25AC.c:LFO selection; sixteen waveform addresses precede D_800301E0"),
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

# Reviewed fixes to copied type definitions: type id -> [(old, new, reason)]. Checked by Native\Tests\src\LayoutAsserts.c.
TYPE_FIXES: dict[str, list[tuple[str, str, str]]] = {
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
         "            u_char reserved3CPrefix[6];\n            short savedRelationOptions;\n"
         "        };\n    };",
         "func_800E72D0 restores relationship options from original offset 0x42"),
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
