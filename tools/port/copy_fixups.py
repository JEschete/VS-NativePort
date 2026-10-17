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
        ("uintptr_t temp_t0 = ((uintptr_t)(arg0 - 1) & a3);", "u_long temp_t0 = LegacyGpu_Handle(arg0 - 1);",
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
}

REGEX_FIXES: list[tuple[str, str, str, str]] = [
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
