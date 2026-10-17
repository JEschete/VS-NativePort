"""Nested_Types: unnamed structs/unions nested two levels deep, named from read uses.

Added 2026-10-17 when tools\\port\\decomp_index.py began recording nested definitions at any depth. Canonical rows
are the BATTLE.PRG (or own-file) spellings; the same declarations reached through INITBTL, MENU3 or SLUS include paths
are same-as rows.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen  # noqa: E402

B = "BATTLE/BATTLE.PRG"
H, M, L = "high", "med", "low"
g = Gen("Nested_Types")
_T = g.T


def T(tid, role, name, loc, purpose, conf, notes, specs):
    """Type rows cite the declaration of their first member as the type's own evidence."""
    _T(tid, role, name, loc, purpose, conf, f"{notes};{g.decl(tid, specs[0][0])}", specs)


g.T = T

g.T("T00822", "OnDisc", "Field::MpdLockBytes", r"Native\Game\src\Field\Mpd.h",
    "Splits a floor trigger's lock word into its lock state and the room effect it drives.", H,
    "nested in Field::MpdLockState (T00752)", [
        ("state", "state", "Lock state: 0 open, 1 or 3 locked by a key or sigil; cleared when opened", H,
         g.c((f"{B}/146C.c", "switch (arg0->unk0.unk8.lock.state) {"))),
        ("unk1", "roomEffect", "Room effect started or tested for this trigger", H,
         g.c((f"{B}/146C.c", "func_8009291C(arg0->unk0.unk8.lock.unk1);"))),
    ])
g.D("T00822", ["T00828", "T00832", "T00837"])

g.T("T00823", "Value", "Menu::ItemListCursorPosition", r"Native\Game\src\Menu\ItemList.h",
    "Holds one remembered item-list cursor as a row index and a page.", M,
    "nested in Menu::ItemListCursor (T00839)", [
        ("index", "row", "Remembered cursor row", M),
        ("page", "page", "Remembered list page", M),
    ])
g.D("T00823", ["T00829", "T00833"])

g.T("T00839", "Value", "Menu::ItemListCursor", r"Native\Game\src\Menu\ItemList.h",
    "Holds one remembered item-list cursor, as row and page or as one word.", M,
    "element of Menu::ItemListCursors.categoryCursors (T00133)", [
        ("cursor", "position", "Row and page", M),
        ("value", "word", "Both bytes as one word", M),
    ])
g.D("T00839", ["T00840", "T00841"])

g.T("T00824", "Value", "Battle::TeleportCommandFields", r"Native\Game\include\Battle\Ability.h",
    "Packs a pending teleport: destination location, tile X and Z, and the MP it costs.", H,
    "nested in Battle::PendingAbilityCommand (T00757); read by ExecuteActionSelection case 10", [
        ("unk0_0", "location", "Teleport destination location index", H,
         g.c((f"{B}/146C.c", "func_8007C694(arg0->executeAbility.fields.unk0_0,"))),
        ("unk0_9", "tileX", "Destination tile X", H,
         g.c((f"{B}/146C.c", "arg0->executeAbility.fields.unk0_9, arg0->executeAbility.fields.unk0_14, 0,"))),
        ("unk0_14", "tileZ", "Destination tile Z", H,
         g.c((f"{B}/146C.c", "arg0->executeAbility.fields.unk0_9, arg0->executeAbility.fields.unk0_14, 0,"))),
        ("unk0_19", "mpCost", "MP subtracted from Ashley when the teleport runs", H,
         g.c((f"{B}/146C.c", ">= arg0->executeAbility.fields.unk0_19) {"))),
    ])
g.D("T00824", ["T00830", "T00834"])

g.T("T00825", "Value", "Battle::EffectTargetActor", r"Native\Game\src\Battle\Effects.h",
    "Identifies an effect target by actor, event actor slot and body part.", H,
    "nested in Battle::EffectTargetValue (T00758)", [
        ("actor", "actor", "Target actor index", H,
         g.c((f"{B}/146C.c", "request.source.value.actor.actor = action->unk4.unk0.targetActor;"))),
        ("unk1", "eventActorSlot", "Slot in the event's actor list; battle code sets 0", M,
         g.c((f"{B}/5BF94.c", "child->unk10[1] = event->actors[spawn.unk8->value.actor.unk1];"))),
        ("part", "part", "Targeted body part; 0xFD is a special marker", M,
         g.c((f"{B}/146C.c", "request.targets[0].value.actor.part = 0xFD;"))),
        ("unk3", "reserved3", "Not read", L),
    ])
g.D("T00825", ["T00831", "T00835"])

g.T("T00826", "State", "Battle::Ai::PerceivedObjectFlagBits", r"Native\Game\src\Battle\Ai\Perception.h",
    "Splits a perceived object's flag word into the bits perception tests.", L,
    "nested in Battle::Ai::PerceivedObjectFlags (T00759)", [
        ("unk0", "reserved0", "Not read through this view", L),
        ("unk16", "skipStateA", "Nonzero skips the object's perception update (with skipStateB)", M,
         g.c((f"{B}/6FC00.c", "|| (object->flags.bits.unk16 | object->flags.bits.unk19 | object->unk5AC_15)"))),
        ("unk19", "skipStateB", "Nonzero skips the object's perception update (with skipStateA)", M,
         g.c((f"{B}/6FC00.c", "|| (object->flags.bits.unk16 | object->flags.bits.unk19 | object->unk5AC_15)"))),
        ("unk21", "reserved21", "Not read", L),
        ("unk31", "forcesPerception", "Set: the object's target perception is updated immediately", M,
         g.c((f"{B}/6FC00.c", "if (object->flags.bits.unk31) {"))),
    ])

g.T("T00827", "State", "Battle::Ai::SavedSettingsBits", r"Native\Game\src\Battle\Ai\AiState.h",
    "Splits the actor's saved settings word into the two-bit mode the AI reads.", L,
    "nested in Battle::Ai::SavedSettingsWord (T00763)", [
        ("low", "reserved0", "Not read through this view", L),
        ("mode", "mode", "Two-bit mode: bit 1 and bit 0 become separate AI state flags", M,
         g.c((f"{B}/7CD68.c", "temp = actor->flags34.bits.mode;"))),
        ("high", "upperBits", "Bits 14-31; bit 14 is read through the word", L),
    ])

g.T("T00836", "Packet", "Menu::Files::TileAndGradientQuad", r"Native\Game\src\Menu\Files.h",
    "Is a textured tile followed by a gradient quad in one primitive buffer.", H,
    "nested in Menu::Files::FilePrimitive (T00784)", [
        ("tile", "tileTpage", "Textured tile with its texture page", H,
         g.c(("MENU/MENU7.PRG/260.c", "_primBuf.prim.tilePoly.tile.tpage ="))),
        ("polyG4", "polyG4", "Gradient quad", H,
         g.c(("MENU/MENU7.PRG/260.c", "_primBuf.prim.tilePoly.polyG4.r0g0b0code = vs_getRGB0Raw(primPolyG4, rgb1);"))),
    ])

g.T("T00838", "Packet", "Title::TileAndGradientQuad", r"Native\Game\src\Title\SaveFiles.h",
    "Is a textured tile followed by a gradient quad in one primitive buffer.", H,
    "nested in Title::FileMenuPrimitivePayload (T00794)", [
        ("tile", "texturedTile", "Textured tile with its texture page", H,
         g.c(("TITLE/TITLE.PRG/22C.c", "_primBuf.prim.tilePoly.tile.tpage ="))),
        ("polyG4", "gradientQuad", "Gradient quad", M),
    ])

if __name__ == "__main__":
    g.write()
