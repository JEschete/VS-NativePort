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

FIXES: dict[str, list[tuple[str, str, str]]] = {
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

REGEX_FIXES: list[tuple[str, str, str, str]] = [
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
    # 146C.h:621: an actor's record is placed at core.model and runs into reservedB2C (the PS1 slot was 0x1900 bytes,
    # the record 0x18DC). The x64 record is 68 bytes larger than the PS1 one; the reserve grows so the record still
    # ends before the weapon model slots.
    "T00634": [("char reservedB2C[0x1788];", "char reservedB2C[0x1788 + 72]; /* x64: the actor record is larger */",
                "room for the x64 actor record")],
}

# 30DB0.c:1026 clears and copies actor records as 0x1900 bytes (the PS1 slot size).
FIXES["F00793"] = FIXES["F00793"] + [
    ("memset(actor, 0, 0x1900);", "memset(actor, 0, sizeof(Actors_ActorRecord));", "record size"),
    ("Core_Memory_CopyAlignedWords(actor, Actors_actorRecords[object->dataAddress], 0x1900);",
     "Core_Memory_CopyAlignedWords(actor, Actors_actorRecords[object->dataAddress], sizeof(Actors_ActorRecord));",
     "record size"),
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
SIG_RETYPE: dict[str, dict] = {"F00572": {"ret": "intptr_t"}, "F00590": {"ret": "intptr_t"}}

# The MPD section E header (Field::MpdSectionE, 146C.c:15700): two array pointers into its own payload, set by
# _loadMpdRoomSection and read only by assembly-era code; offsets from the header natively.
ONDISC_OFFSETS.update({"T00337.003": "MPD section E first array", "T00337.004": "MPD section E second array"})
FIXES["F00551"] = [
    ("VS_ONDISC(sectionE, firstArray, int *) = (int*)(sectionE + 1);",
     "sectionE->firstArray = sizeof(*sectionE);", "offset of the payload"),
    ("VS_ONDISC(sectionE, secondArray, int *) = (int*)(sectionE + 1);",
     "sectionE->secondArray = sizeof(*sectionE);", "offset of the payload"),
    ("VS_ONDISC(sectionE, secondArray, int *) += sectionE->secondArrayOffset >> 2;",
     "sectionE->secondArray += (sectionE->secondArrayOffset >> 2) * sizeof(int);", "int-sized steps"),
    ("VS_ONDISC(sectionE, secondArray, int *) = NULL;", "sectionE->secondArray = 0;", "no second array"),
]
