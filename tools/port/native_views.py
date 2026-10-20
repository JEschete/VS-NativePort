"""Rebase selected runtime views by exposing pointers otherwise hidden in PS1 padding.

These are in-memory objects, never file layouts. Each family uses the same pointer slots;
letting the C compiler align those slots preserves native pointers without packing or handles.
Existing semantic fields and decomp provenance remain in the generated declarations.
"""
from __future__ import annotations

import re

# BATTLE 7CD68.c actorInitializationState/actorSettingsState and the merged func_800E78F4_t.
AI_VIEWS = frozenset("""
T00351 T00356 T00362 T00368 T00370 T00373 T00381 T00385 T00389 T00392 T00394 T00399
T00406 T00412 T00414 T00416 T00514 T00524 T00526 T00529 T00532 T00554 T00555 T00556 T00557
T00559 T00561 T00564 T00569 T00582 T00590 T00591 T00593 T00608 T00613 T00615 T00623 T00740
""".split())
AI_POINTERS = (0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x1A0, 0x22C, *range(0x430, 0x470, 4))

# Views of vs_battle_actor (146C.h), including the engagement data used by the canonical AI state.
ACTOR_VIEWS = frozenset("""
T00358 T00379 T00380 T00383 T00388 T00390 T00396 T00408 T00522 T00610 T00614
""".split())
ACTOR_POINTERS = (0, 0x3C, 0x44, 0x48, 0x4C)

# D_800F4538_t / D_800F4538_unk0 (3A1A0.h). More distant model fields are validated against ActorRecord.
MODEL_VIEWS = frozenset("""
T00360 T00384 T00387 T00411 T00527 T00553 T00558 T00560 T00568 T00588 T00589 T00612 T00622 T00739
""".split())
MODEL_POINTERS = (0x68, *range(0x5D0, 0x5EC, 4))

GIMMICK_VIEWS = frozenset(("T00565", "T00566"))
GIMMICK_POINTERS = (0x68,)

GIM_VIEWS = frozenset(("T00219",))
GIM_POINTERS = (0x10, 0x14)

SPHERE_VIEWS = frozenset(("T00214", "T00485"))
SPHERE_POINTERS = (0x40, 0x44, *(0x990 + index * 20 + 16 for index in range(24)))


def pointer_slots(tid: str) -> tuple[int, ...]:
    if tid in AI_VIEWS:
        return AI_POINTERS
    if tid in ACTOR_VIEWS:
        return ACTOR_POINTERS
    if tid in MODEL_VIEWS:
        return MODEL_POINTERS
    if tid in GIMMICK_VIEWS:
        return GIMMICK_POINTERS
    if tid in GIM_VIEWS:
        return GIM_POINTERS
    if tid in SPHERE_VIEWS:
        return SPHERE_POINTERS
    return ()


def rebase(tid: str, text: str, fields: list[tuple[str, dict]]) -> str:
    """Split byte-padding declarations at missing pointer slots; reject ambiguous overlays.

    `fields` contains final field names and the original ledger layout. Actual pointer fields
    are retained, including function pointers. Slots outside a partial view's extent are ignored.
    """
    slots = pointer_slots(tid)
    if not slots:
        return text
    # GCC packed these full-byte bitfields; MSVC reserves the entire declared short.
    if tid in ('T00532', 'T00412'):
        text, count = re.subn(r'(?:int|short) linkedActor\s*:\s*8;', 'signed char linkedActor;', text)
        if count != 1:
            raise ValueError(f'{tid}: linkedActor byte not found')
    # 74088.c:1547 tests the actor's model slot through a partial integer view.
    if tid == 'T00380':
        text, count = re.subn(r'int opaque44;', 'void* opaque44;', text)
        if count != 1:
            raise ValueError('T00380: model slot not found')
    for name, field in fields:
        offset = field['offset'] // 8
        padding = re.fullmatch(r'(?:u_char|(?:unsigned )?char)\[(\d+)\]', field['type'])
        if not padding or not name.startswith(('reserved', 'padding')):
            continue
        end = offset + int(padding[1])
        hidden = [s for s in slots if offset <= s and s + 4 <= end]
        if not hidden:
            continue
        pieces = []
        cursor = offset
        for slot in hidden:
            if cursor < slot:
                pieces.append(f'    char {name}_{cursor:X}[{slot - cursor}];')
            pieces.append(f'    void* nativePointer{slot:X}; /* PS1 pointer slot 0x{slot:X} */')
            cursor = slot + 4
        if cursor < end:
            pieces.append(f'    char {name}_{cursor:X}[{end - cursor}];')
        pattern = rf'(?m)^\s*(?:/\*[^\n]*?\*/\s*)?(?:u_char|(?:unsigned\s+)?char)\s+{re.escape(name)}\s*\[[^\]]+\];'
        text, count = re.subn(pattern, '\n'.join(pieces), text, count=1)
        if count != 1:
            raise ValueError(f'{tid}.{name}: cannot expose native pointer slots')
    return text
