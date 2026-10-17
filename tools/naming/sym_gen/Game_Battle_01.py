"""Game_Battle_01 types and fields from code read directly. Citations are resolved by exact text search (common.Gen).

Group A: anonymous unions and structs nested in battle, room and AI types, with their per-overlay duplicates.
Group B: small named battle types.
Group C: the battle-state records (D_800EB9B8_t, D_800F19CC_t, _hitEntity_t, AI scratch tables).
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen  # noqa: E402

g = Gen("Game_Battle_01")
c, T, D = g.c, g.T, g.D
LOW, MED, HIGH = "low", "med", "high"
B = "Native\\Game\\src\\Battle\\"
BI = "Native\\Game\\include\\Battle\\"
AI = B + "Ai\\"
FD = "Native\\Game\\src\\Field\\"
TU = "the same translation unit's header included by another overlay"


def twin(tid: str, applied: str, why: str) -> None:
    """Type row identical to an already-applied twin (same names, copied evidence)."""
    t, fs = g.applied(applied)
    g.TYPES.append([tid, t["role"], t["final_name"], t["final_location"], t["purpose"], t["confidence"],
                    f"same-as:{applied};{why};{t['notes']}"])
    for m, f in zip(g.members[tid], fs):
        g.FIELDS.append([m["id"], f["final_name"], f["meaning"], f["confidence"], f["notes"]])


# ---------------------------------------------------------------- group A: battle and room unions
hp = {f["index"]: f for f in g.applied("T00153")[1]}
T("T00125", "Def", "Battle::ActionHitParams", BI + "Action.h",
  "Gives one hit of an action: effect, prerequisite, hit and stat functions, stat factor, damage type and affinity.",
  "high", c(("146C.c", "threshold = _hitFunctions[action->hitParams[hitNumber].hitFunction]("))
  + ";field names match T00153 (same declaration through another include path)", [
      ("effect", hp["0"]["final_name"], "Hit effect; indexes the effect mediators", HIGH,
       c(("146C.c", "_actionEffectMediators[action->hitParams[hit].effect]("))),
      ("prerequisiteFunction", hp["1"]["final_name"], "Prerequisite check selected for the hit", HIGH,
       c(("146C.c", "switch (spec->hitParams[hit].prerequisiteFunction) {"))),
      ("hitFunction", hp["2"]["final_name"], "Hit-chance function index", HIGH,
       c(("146C.c", "threshold = _hitFunctions[action->hitParams[hitNumber].hitFunction]("))),
      ("statCalculator", hp["3"]["final_name"], "Stat-change calculator index", HIGH,
       c(("146C.c", "amount = _statCalculators[action->hitParams[hit].statCalculator]("))),
      ("statFactor", hp["4"]["final_name"], "Factor applied by the stat calculation", MED,
       c(("146C.c", "* arg0->hitParams[arg3].statFactor)"))),
      ("type", hp["5"]["final_name"], "Damage type; 0 is physical", MED, c(("146C.c", "if (action->hitParams[nHit].type == 0) {"))),
      ("affinity", hp["6"]["final_name"], "Affinity + 1; 0 for none", HIGH,
       c(("146C.c", "affinity = action->hitParams[0].affinity - 1;"))),
  ])
T("T00747", "State", "Battle::HitLimbResult", B + "Hit.h",
  "Holds one limb's hit result: its two effect values, or both read as one word.", "med",
  c(("146C.c", "entity->limbs[i].effects.value0 += _armorDpAdjustmentAmounts[i];")), [
      ("effects", "effects", "Effect values for the limb", HIGH),
      ("hp", "packed", "Both effect values as one word; summed across entities", LOW),
  ], recv=r"limbs\[[^\]]+\]")
T("T00126", "OnDisc", "Field::MpdSectionLengths", FD + "Room.h",
  "Gives the byte length of each MPD room section, in the order of Field::RoomRecord's section pointers.", "high",
  c(("146C.c", "temp_t4 = vs_battle_roomData.header.doorSectionLen / 12;")) + ";names follow T00329 section pointers", [
      ("geometrySectionLen", "geometryLength", "Geometry section length", MED),
      ("collisionMapSectionLen", "collisionMapLength", "Collision map section length", MED),
      ("section3Len", "tileCellsLength", "Tile cell section length", MED),
      ("doorSectionLen", "doorsLength", "Door section length; 12-byte records", HIGH),
      ("lightingSectionLen", "lightingLength", "Lighting section length", MED),
      ("section6Len", "section6Length", "Section 6 length", LOW),
      ("section7Len", "section7Length", "Section 7 length", LOW),
      ("section8Len", "section8Length", "Section 8 length", LOW),
      ("section9Len", "floorTriggersLength", "Floor trigger section length; 12-byte records", HIGH),
      ("sectionALen", "lockTriggersLength", "Lock trigger section length; 20-byte records", HIGH),
      ("sectionBLen", "environmentLength", "Environment section length", MED),
      ("textureEffectsSectionLen", "textureEffectsLength", "Texture effect section length", HIGH),
      ("sectionDLen", "effectScriptsLength", "Effect script section length", HIGH),
      ("sectionELen", "sectionELength", "Section E length", LOW),
      ("sectionFLen", "minimapSceneLength", "Minimap scene section length", MED),
      ("section10Len", "tiledBackgroundLength", "Tiled background section length", MED),
      ("section11Len", "viewTriggersLength", "View trigger section length", HIGH),
      ("section12Len", "movingPlatformsLength", "Moving platform section length; 184-byte records", HIGH),
      ("section13Len", "containersLength", "Treasure chest section length", HIGH),
      ("akaoSectionLen", "soundDataLength", "AKAO sound section length", MED),
      ("section15Len", "section15Length", "Section 15 length", LOW),
      ("section16Len", "section16Length", "Section 16 length", LOW),
      ("section17Len", "section17Length", "Section 17 length", LOW),
      ("section18Len", "zoneBoundsLength", "Zone bounds section length", MED),
  ], recv=r"header")
D("T00126", ["T00154", "T00166", "T00182"], TU)
T("T00749", "State", "Battle::ActorTargetWord", B + "BattleActor.h",
  "Holds an actor word used as a pending Z movement offset, and as the last targeted actor and limb as halves.", "low",
  c(("146C.c", "if (arg0->unk10.s32 < 0) {"), ("146C.c", "D_800F19CC->unk29C0[0].unk9 = temp_a0->unk10.u8[0];")), [
      ("u8", "bytes", "Bytes 0 and 2 seed the target selection", MED,
       c(("146C.c", "D_800F19CC->unk29C0[0].unk9 = temp_a0->unk10.u8[0];"))),
      ("u16", "halves", "Last targeted actor and limb", LOW),
      ("s32", "word", "Pending Z movement in 20.12 fixed point, consumed 8 units per step", MED,
       c(("146C.c", "arg0->unk10.s32 += 0x8000;"))),
  ], recv=r"unk10")
D("T00749", ["T00767", "T00777", "T00788"], TU)
T("T00750", "State", "Battle::ActorTriggerTile", B + "BattleActor.h",
  "Remembers the tile word and tile mode of the last floor trigger an actor fired.", "med",
  c(("146C.c", "actor->unk2C.s32 = *(u_int*)position;"), ("146C.c", "actor->unk2C.u8[3] = tileMode;")), [
      ("u8", "bytes", "Byte 3 is the tile mode", MED, c(("146C.c", "actor->unk2C.u8[3] = tileMode;"))),
      ("s32", "word", "Tile word (X, layer, Z) of the last trigger", MED, c(("146C.c", "actor->unk2C.s32 = *(u_int*)position;"))),
  ])
D("T00750", ["T00768", "T00778", "T00789"], TU)
T("T00127", "State", "Actors::ActorAndStats", "Native\\Game\\src\\Actors\\ActorAllocation.h",
  "Places a battle actor and its statistics block together; the actor's statistics pointer points at the block.",
  "high", c(("146C.c", "temp_v0->unk0.unk0.unk3C = &temp_v0->unk0.unk50;")), [
      ("unk0", "actor", "Battle actor", HIGH, c(("146C.c", "vs_main_bzero(temp_v0, sizeof temp_v0->unk0.unk0);"))),
      ("unk50", "stats", "Statistics block", HIGH, c(("146C.c", "temp_v0->unk0.unk0.unk3C = &temp_v0->unk0.unk50;"))),
  ])
D("T00127", ["T00155", "T00167", "T00183"], TU)
T("T00751", "State", "Battle::ValueLabelKey", B + "ValueLabels.h",
  "Identifies a queued value label: its kind byte and the actor it belongs to.", "med",
  c(("2842C.c", "if ((p->unk8.u8[0] == 1) && (p->unk8.u8[1] == arg0) && (p->unk0.pad >= 0)) {")), [
      ("u8", "bytes", "Byte 0 is the label kind, byte 1 the actor", MED),
      ("u16", "halfword", "Kind and actor as one value; compared with a kind when the actor is 0", MED),
  ], recv=r"\w->unk8")
D("T00751", ["T00769", "T00779", "T00790"], TU)
T("T00752", "OnDisc", "Field::MpdLockState", FD + "Mpd.h",
  "Holds a floor trigger's lock or switch state: a toggled value, or a lock state byte and its door rotation.", "med",
  c(("146C.c", "switch (arg0->unk0.unk8.lock.state) {"), ("146C.c", "arg0->unk0.unk8.s16 = arg0->unk0.unk8.s16 == 0;")), [
      ("s16", "switchValue", "Switch position; toggled between 0 and 1", MED),
      ("lock", "lock", "Lock state and door rotation", MED),
  ], recv=r"unk8")
D("T00752", ["T00770", "T00780", "T00791"], TU)
T("T00128", "Def", "Battle::StatusLabelLayout", B + "ValueLabels.h",
  "Gives the label sprites shown for one actor status label and the gap used between them.", "med",
  c(("2842C.c", "for (i = 0; i < D_800690B4[p->unkC].count; ++i) {")), [
      ("count", "count", "Number of label sprites; 0 for none", HIGH),
      ("labels", "labels", "Label sprite indices", HIGH),
      ("color", "spacing", "Read as the width offset between sprites", LOW, c(("2842C.c", "width = D_800690B4[p->unkC].color;"))),
  ], recv=r"D_800690B4\[p->unkC\]")
T("T00754", "State", "Event::ShakeEnvelope", "Native\\Game\\src\\Event\\Camera.h",
  "Holds a camera shake oscillator's envelope: shape byte from the script, or the remaining count.", "med",
  c(("4A0A8.c", "switch (arg0->unk0.u8[1]) {")), [
      ("u8", "bytes", "Byte 1 is the envelope shape", MED, c(("4A0A8.c", "entry->unk0.u8[1] = script[4];"))),
      ("s16", "count", "Remaining shake count", MED, c(("4A0A8.c", "D_800F4BA4->unk1A4.unk0.s16 = count;"))),
  ])
T("T00130", "State", "Battle::PrismTestPolygon", B + "TargetVolume.h",
  "Holds the projected prism corners and normal-clip result while testing a point against a target prism.", "high",
  c(("58578.c", "gte_stopz(&polygon.clip);")), [
      ("corners", "corners", "Projected corners", HIGH),
      ("clip", "normalClip", "Normal-clip result of the corners", HIGH),
  ], recv=r"polygon")
T("T00131", "State", "Battle::PrismTestWork", B + "TargetVolume.h",
  "Holds the local and rotated points while testing a point against a target prism.", "high",
  c(("58578.c", "ApplyMatrixSV(&matrix, (SVECTOR*)work.local, &work.rotated);")), [
      ("local", "localPoint", "Point in shape space", HIGH),
      ("rotated", "rotatedPoint", "Point rotated into world space", HIGH),
      ("origin", "origin", "Shape origin", HIGH),
  ], recv=r"work")
T("T00132", "State", "Battle::TargetLabelBox", B + "TargetVolume.h",
  "Holds the primitives that draw the box behind a target label.", "med",
  c(("58578.c", "prim->box.tile.r0g0b0code = vs_getRGB0(primTile, 0, 0, 0);")), [
      ("tile", "tile", "Box tile", HIGH),
      ("poly", "shadePoly", "Shaded polygon", HIGH),
      ("tpage", "texturePageCommand", "Texture-page command word", LOW, "declaration-only;" + g.decl("T00132", "tpage")),
  ], recv=r"box")
T("T00755", "Value", "Battle::TargetShapeExtent", B + "TargetVolume.h",
  "Gives a target volume's size bytes, or its shape flags.", "med",
  c(("146C.c", "sp860.unk4.values[0] = vs_main_actions[arg0].rangeX;")), [
      ("flags", "flags", "Shape flags", MED, c(("58578.c", "func_800C1564_flags saved_unk4 = arg0->unk4.flags;"))),
      ("values", "extents", "Size along each axis; byte 0 is the action's X range", MED),
  ], recv=r"unk4")
T("T00756", "State", "Battle::HudHighlightWord", B + "Hud.h",
  "Selects which HUD entry is highlighted; reset to 0xFFFFFF for none.", "low",
  c(("5BF94.c", "D_800EB9D0.s32 = 0xFFFFFF;")), [
      ("s32", "word", "All bytes; 0xFFFFFF clears the highlight", MED),
      ("u8", "bytes", "Byte 1 is the highlighted entry", LOW),
  ], recv=r"D_800EB9D0")
T("T00133", "State", "Menu::ItemListCursors", "Native\\Game\\src\\Menu\\ItemList.h",
  "Remembers the selected item category and the list cursor of each category.", "high",
  c(("16C.c", "int category = (D_800F4EE8.unk3A.currentItemCategory - 1) & 7;")), [
      ("currentItemCategory", "currentCategory", "Selected category + 1", HIGH),
      ("unk1", "reserved1", "Not read", LOW),
      ("unk2", "categoryCursors", "List cursor of each category", HIGH),
  ], recv=r"unk3A\)?")
D("T00133", ["T00156", "T00168"], TU)
T("T00757", "State", "Battle::PendingAbilityCommand", BI + "Ability.h",
  "Holds the command chosen in a battle menu: packed ability fields, item id halves, or the whole word.", "med",
  c(("146C.c", "func_80072EC4(0, arg0->executeAbility.s32);")), [
      ("fields", "bits", "Packed ability fields", MED),
      ("u16", "halves", "Half 0 is the chosen item; half 1 the consumed item", MED,
       c(("146C.c", "vs_battle_decreaseMiscCount(D_800F18EC->executeAbility.u16[1]);"))),
      ("s32", "word", "Whole command word", MED),
  ], recv=r"executeAbility")
D("T00757", ["T00771", "T00781"], TU)
T("T00134", "Def", "Effects::ParticleColorCurves", "Native\\Game\\src\\Effects\\Particles.h",
  "Selects the red, green and blue fade curves of a particle and its age.", "med",
  c(("5BF94.c", "scratch->unk154.fields.age = node->unk77;")), [
      ("rCurve", "redCurve", "Red fade curve", MED),
      ("gCurve", "greenCurve", "Green fade curve", MED),
      ("bCurve", "blueCurve", "Blue fade curve", MED),
      ("age", "age", "Particle age", HIGH),
  ], recv=r"fields")
D("T00134", ["T00157", "T00169"], TU)
T("T00758", "Value", "Battle::EffectTargetValue", B + "Effects.h",
  "Targets a battle effect at an actor part or at a world position.", "high",
  c(("146C.c", "request.source.value.actor.actor = 0;"), ("146C.c", "request.targets[0].value.position.vx = arg0[0];")), [
      ("actor", "actor", "Actor and part", HIGH),
      ("position", "position", "World position", HIGH),
  ], recv=r"value")
D("T00758", ["T00772", "T00782"], TU)
T("T00764", "Value", "Actors::PackedTileTarget", "Native\\Game\\src\\Actors\\Location.h",
  "Holds a tile target as fields or as one word.", "high",
  c(("3A1A0.c", "arg2->unk0.unk0.value = temp_a0->unk1868;")), [
      ("fields", "target", "Tile and facing fields", HIGH),
      ("value", "word", "Whole tile word", HIGH),
  ], recv=r"unk0(?:\.unk0)?")
D("T00764", ["T00773", "T00783", "T00792"], TU)
for _t in ("T00766", "T00776", "T00787"):
    twin(_t, "T00748", TU)

# ---------------------------------------------------------------- group A: AI tile words and relation bits
TILE_WHY = "the same packed tile word declared again in another AI translation unit"
T("T00144", "Value", "Battle::Ai::TileWord", AI + "Navigation.h",
  "Packs a tile's X, an extra byte, Z and a search status into one word.", "high",
  c(("771A8.c", "dest.p.x = (int)(source.p.x * 128 + x) >> 7;"))
  + ";upstream vs_battle_movementPosition has the same four bytes;status is cited from the identical 6FC00.c copy", [
      ("x", "tileX", "Tile X", HIGH),
      ("y", "tag", "Extra byte: gate index in routes, layer in placements, height in steps", LOW,
       c(("771A8.c", "candidate.p.y = i + 2;"))),
      ("z", "tileZ", "Tile Z", HIGH),
      ("status", "searchStatus", "Search state of the tile (1 searched, 2 accepted)", LOW,
       c(("6FC00.c", "dest.p.status = 1;"))),
  ], recv=r"\bp")
D("T00144", ["T00135", "T00138", "T00149"], TILE_WHY)
D("T00144", ["T00137", "T00142", "T00145", "T00148"], TILE_WHY + "; byte members, different member spellings")
T("T00141", "Value", "Battle::Ai::StepPosition", AI + "Navigation.h",
  "Holds an actor's world X and Y as two halves of one word during step movement.", "med",
  c(("771A8.c", "angle = ratan2(state->position.p.x - x, state->z - z);")), [
      ("x", "positionX", "World X", MED, c(("771A8.c", "angle = ratan2(state->position.p.x - x, state->z - z);"))),
      ("y", "positionY", "World Y", MED, c(("771A8.c", "rise = selectedHeight - state->position.p.y;"))),
  ])
T("T00759", "State", "Battle::Ai::PerceivedObjectFlags", AI + "Perception.h",
  "Holds a perceived object's flag word, or its bit fields.", "low",
  c(("6FC00.c", "if ((targetObject->flags.raw & 0x180000)")), [
      ("raw", "word", "Whole flag word", MED),
      ("bits", "bits", "Bit fields", LOW),
  ], recv=r"flags")
T("T00136", "State", "Battle::Ai::TargetRelationBits", AI + "Perception.h",
  "Holds what an AI actor perceives about one other actor: activity, visibility, reach, classification and timers.",
  "med", c(("6FC00.c", "relation.bits.active = 1;")) + ";upstream func_800DA1D4_t is the same word", [
      ("active", "active", "Relation is live this frame", MED),
      ("previous", "wasActive", "Value of active in the previous update", LOW),
      ("hidden", "hidden", "Target is hidden from the actor", MED),
      ("inView", "inView", "Target is inside the view cone", MED),
      ("unk4", "reserved4", "Not read", LOW),
      ("visible", "visible", "Target is visible", MED),
      ("kind", "classification", "Relation classification", MED),
      ("counter", "counter", "Classification counter", LOW),
      ("timer", "timer", "Relation timer", LOW),
      ("kindTimer", "classificationTimer", "Frames before the classification reverts", LOW),
      ("blocked", "blocked", "Path to the target is blocked (not read by decompiled code)", LOW),
      ("previousKind", "previousClassification", "Classification restored when the timer ends", MED),
      ("outOfReach", "outOfReach", "Target is beyond reach", MED),
      ("unk31", "onMarkedTile", "Target stands on a tile flagged 0x2000", LOW,
       c(("6FC00.c", "relation.bits.unk31 = 1;"))),
  ], recv=r"bits")
T("T00139", "State", "Battle::Ai::TargetRelationReactionView", AI + "Perception.h",
  "Views the per-target relation word as the reaction code uses it.", "low",
  c(("74088.c", "state->targets[id].p.classification = classification;"))
  + ";view of the word in T00136 with a different bit split; reconcile when copying", [
      ("unk0", "reserved0Bits", "Not read here", LOW),
      ("classification", "classification", "Relation classification", MED),
      ("unk8", "reserved8Bits", "Not read here", LOW),
      ("timer", "timer", "Relation timer; set to 28 on reaction", LOW),
      ("counter", "counter", "Classification counter", LOW),
      ("unk27", "reserved27Bit", "Not read here", LOW),
      ("previous", "previousClassification", "Classification restored later", LOW),
      ("unk30", "reserved30Bits", "Not read here", LOW),
  ], recv=r"\.p")
T("T00140", "State", "Battle::Ai::TargetRelationBytes", AI + "Perception.h",
  "Views the per-target relation word as bytes; the top two bits of byte 0 are the classification.", "low",
  c(("74088.c", "if ((firstOther->targets[0].b.flags >> 6) == 3) {")), [
      ("flags", "firstByte", "First byte; bits 6-7 are the classification", LOW),
      ("pad", "reserved1", "Remaining bytes", LOW),
  ], recv=r"\.b")
T("T00760", "State", "Battle::Ai::PositionWord", AI + "Perception.h",
  "Holds an actor position word, or its bytes.", "low",
  c(("6FC00.c", "state->unk38.value = state->unk64->position;")), [
      ("value", "word", "Position word", MED),
      ("b", "bytes", "Position bytes", LOW, "declaration-only;" + g.decl("T00760", "b")),
  ], recv=r"unk38")
T("T00761", "State", "Battle::Ai::MovementHistory", AI + "Perception.h",
  "Keeps eight recent coordinate bytes of an AI actor, compared as two words to detect repeating movement.", "high",
  c(("6FC00.c", "state->historyX.b[state->historyIndex] = state->unk4C;")), [
      ("b", "samples", "Recent coordinate bytes", HIGH, c(("6FC00.c", "state->historyX.b[state->historyIndex] = state->unk4C;"))),
      ("w", "words", "Samples compared as two words", HIGH, c(("6FC00.c", "return state->historyX.w[0] == state->historyX.w[1]"))),
  ])
D("T00761", ["T00762"], "the Z twin of the X history with the same layout")
T("T00143", "State", "Battle::Ai::DetourState", AI + "Navigation.h",
  "Tracks an AI actor walking around an obstacle: heading, turn side, steps, visited cells and bounds.", "med",
  c(("771A8.c", "heading = path.bits.direction;")), [
      ("unk0", "reserved0", "Not read", LOW),
      ("direction", "heading", "Heading in eighth turns", HIGH),
      ("clockwise", "clockwise", "Turns clockwise around the obstacle", HIGH),
      ("count", "steps", "Steps left; 0 ends the detour", MED),
      ("visited", "visited", "Visited mask, cleared on start", LOW),
      ("stop", "stop", "Ends the detour", MED),
      ("changed", "changed", "Heading changed this step", LOW),
      ("cell", "cell", "Cell the detour started from", LOW),
      ("boundX", "boundX", "X bound in tiles", MED),
      ("boundZ", "boundZ", "Z bound in tiles", MED),
  ], recv=r"path\.bits")
T("T00763", "State", "Battle::Ai::SavedSettingsWord", AI + "AiState.h",
  "Holds an actor's saved AI settings word, or its fields; the 2-bit field selects a mode.", "low",
  c(("7CD68.c", "temp = actor->flags34.bits.mode;")), [
      ("raw", "word", "Whole settings word; bit 14 selects a range table", LOW,
       c(("7CD68.c", "state->f102 = D_80069C08[(actor->flags34.raw >> 14) & 1];"))),
      ("bits", "bits", "Settings fields", LOW),
  ])
T("T00146", "State", "Battle::Ai::ActorStateFlagBytes", AI + "AiState.h",
  "Views an actor's state flag word as four bytes.", "low",
  c(("7CD68.c", "actor->flags.bytes.b1 = 0;")), [
      ("b0", "byte0", "Byte 0", LOW), ("b1", "byte1", "Byte 1; cleared when the AI state is created", LOW),
      ("b2", "byte2", "Byte 2", LOW), ("b3", "byte3", "Byte 3", LOW),
  ], recv=r"bytes")
T("T00147", "State", "Battle::Ai::ActorStateFlagBits", AI + "AiState.h",
  "Views an actor's state flag word as bit fields; two 2-bit fields are cleared when the AI state is created.", "low",
  c(("7CD68.c", "actor->flags.bits.f24 = 0;")), [
      ("low", "lowBits", "Bits 0-23", LOW), ("f24", "bits24", "Bits 24-25", LOW),
      ("f26", "bits26", "Bits 26-27", LOW), ("high", "highBits", "Bits 28-31", LOW),
  ], recv=r"flags\.bits")

# ---------------------------------------------------------------- group B: small named battle types
T("T00200", "State", "Actors::KeyframeRangeScratch", "Native\\Game\\src\\Actors\\Animation.h",
  "Holds the frame range and track data pointer while one animation track is decoded on the scratchpad.", "med",
  c(("44F14.c", "for (frame = scratch->first; frame <= scratch->last; ++frame) {")), [
      ("unk0", "reserved0", "Not read", LOW),
      ("first", "firstFrame", "First frame decoded", HIGH),
      ("last", "lastFrame", "Last frame decoded", HIGH),
      ("data", "trackData", "Track data being decoded", MED),
  ], recv=r"scratch")
T("T00217", "State", "Battle::TargetSphereSpec", B + "TargetVolume.h",
  "Describes one targeting sphere volume; set as a whole, and only its kind byte is read by decompiled code.", "low",
  c(("573B8.c", "D_800EB9B8->unk0.unk0 |= 0x80;")), [
      ("unk0", "kind", "Volume kind; bit 7 marks it active", MED, c(("573B8.c", "D_800EB9B8->unk0.unk0 |= 0x80;"))),
      ("unk1", "reserved1", "Copied with the volume; not read by decompiled code", LOW),
      ("unk2", "reserved2", "Copied with the volume; not read by decompiled code", LOW),
      ("unk3", "reserved3", "Copied with the volume; not read by decompiled code", LOW),
      ("unk4", "reserved4", "Copied with the volume; not read by decompiled code", LOW),
      ("unk8", "reserved8", "Copied with the volume; not read by decompiled code", LOW),
      ("unkC", "reservedC", "Copied with the volume; not read by decompiled code", LOW),
  ])
T("T00218", "State", "Battle::SphereTargetLabel", B + "TargetVolume.h",
  "Holds one floating label shown on a targeting sphere: state, depth, gradient, screen slide, color and text.", "med",
  c(("58578.c", "vs_battle_renderTextRawColor(arg0->unk10, textXy, color, nextPrim);")), [
      ("unk0", "state", "0 inactive, 2 sliding in; above 1 is not settled", MED, c(("58578.c", "if (D_800EB9B8->unk990[i].unk0 > 1) {"))),
      ("unk1", "depth", "Ordering-table depth of the label", MED, c(("58578.c", "u_long* nextPrim = vs_scratch.unk8 - (arg0->unk1 * 4 - 8);"))),
      ("unk2", "gradient", "Gradient style", MED, c(("58578.c", "int gradient = arg0->unk2;"))),
      ("unk3", "ringSize", "Ring size passed to the ring draw", LOW, c(("58578.c", "func_800C1A40(sxy, ringXy, arg0->unk3 - 2);"))),
      ("unk4", "screenX", "Screen X; slides toward its row", MED, c(("58578.c", "x = arg0->unk4 & 0xFFFF;"))),
      ("unk6", "offsetY", "Y offset added to the label row", MED, c(("58578.c", "y = arg0->unk6 + D_800F4CB4;"))),
      ("unk7", "fade", "Color fade step", MED, c(("58578.c", "color0 = vs_battle_uiGradientStop(color0, arg0->unk7 * 8, 0x80);"))),
      ("unk8", "worldPosition", "World position projected for the label", HIGH, c(("58578.c", "gte_ldv0(&arg0->unk8);"))),
      ("unk10", "text", "Label text", HIGH, c(("58578.c", "vs_battle_renderTextRawColor(arg0->unk10, textXy, color, nextPrim);"))),
  ])
T("T00220", "Def", "Battle::Ai::DirectionDelta", AI + "Navigation.h",
  "Gives the tile X and Z step of one movement direction.", "high",
  c(("6FC00.c", "dest.p.x = source.p.x + ((D_800F16EC_t*)0x1F8003EC)[direction].dx;")), [
      ("dx", "dx", "X step (-1, 0 or 1)", HIGH),
      ("unk0_2", "reserved2Bits", "Not read", LOW),
      ("dz", "dz", "Z step (-1, 0 or 1)", HIGH),
      ("unk0_6", "reserved6Bits", "Not read", LOW),
  ], recv=r"\]")
T("T00227", "State", "Battle::TargetPreview", B + "Targeting.h",
  "Describes one selectable target and its preview: position, actor and limb, previewed effect and amounts.", "med",
  c(("146C.c", "arg0->unkE = arg2->unk0.hitThreshold;"), ("146C.c", "D_800F19CC->unk29C0[count].unk0.vz = room[j].unk2 * 128 + 64;")), [
      ("unk0", "position", "World position of the target", HIGH,
       c(("146C.c", "D_800F19CC->unk29C0[count].unk0.vz = room[j].unk2 * 128 + 64;"))),
      ("unk8", "kind", "Target kind; 16 for chests", LOW, c(("146C.c", "target->unk8 = 16;"))),
      ("unk9", "actorId", "Actor id; chest index | 0x20 for chests", MED,
       c(("146C.c", "if (D_800F19CC->unk29C0[i].unk9 < 0x10) {"))),
      ("unkA", "limb", "Target limb", HIGH,
       c(("146C.c", "D_800F19CC->unk8.unk4C[i].unk0.targetLimb = D_800F19CC->unk29C0[i].unkA;"))),
      ("effect", "effect", "Previewed hit effect", HIGH, c(("146C.c", "arg0->effect = effect;"))),
      ("unkC", "previewAmount", "Current HP or MP shown by the preview", MED, c(("146C.c", "arg0->unkC = var_v0;"))),
      ("unkE", "hitChance", "Hit chance; for chests a risk-based trap chance", MED,
       c(("146C.c", "arg0->unkE = arg2->unk0.hitThreshold;"))),
      ("unk10", "removedStatuses", "Statuses the action removes", HIGH, c(("146C.c", "arg0->unk10 = arg2->removeStatuses & 0x1FFFFFE0;"))),
      ("unk14", "actionKind", "Kind byte of the action", MED, c(("146C.c", "arg0->unk14 = vs_main_actions[arg1->actionIndex].unk2_4;"))),
      ("unk15", "opaque15", "Copied from the action request; meaning not established", LOW, c(("146C.c", "arg0->unk15 = arg1->unk3;"))),
      ("unk16", "opaque16", "Copied from the action request; meaning not established", LOW, c(("146C.c", "arg0->unk16 = arg1->unk2;"))),
      ("enemyClass", "enemyClass", "Enemy class of the target", HIGH),
  ], recv=r"arg0|target")
T("T00230", "State", "Battle::FloatingLabel", B + "ValueLabels.h",
  "Holds one floating battle label (damage value, status, chain count or timing result): position, key, timing, "
  "scale and text.", "med",
  c(("2842C.c", "sprintf(p->displayText, \"%d\", p->unkC & 0x7FFF);")), [
      ("unk0", "position", "World position; pad is the remaining lifetime", MED,
       c(("2842C.c", "if (label->unk8.u8[0] == 6 && --label->unk0.pad == -1) {"))),
      ("unk8", "key", "Label kind and actor", HIGH),
      ("unkA", "limb", "Limb or sprite index", MED, c(("2842C.c", "p->unkA = arg1;"))),
      ("unkB", "frame", "Animation frame", MED, c(("2842C.c", "frame = p->unkB;"))),
      ("unkC", "value", "Displayed value; bit 15 marks healing", MED, c(("2842C.c", "p->unkC = 999 | ((arg2 >> 16) & 0x8000);"))),
      ("unkE", "colorFlags", "Color and flags; bit 7 is cleared once shown", LOW, c(("2842C.c", "p->unkE = arg3 | flags;"))),
      ("displayTextLen", "textLength", "Length of the text", HIGH),
      ("unk10", "scaleX", "Horizontal scale (ONE = 1.0)", MED, c(("2842C.c", "p->unk10 = ONE;"))),
      ("unk12", "scaleY", "Vertical scale; grows by 10 per frame while rising", MED,
       c(("2842C.c", "(*D_800F1BAC)[p].unk12 += 10;"))),
      ("displayText", "text", "Text drawn as digit sprites", HIGH),
  ], recv=r"\bp|label|q")
T("T00240", "State", "Field::GimmickObjectInfo", FD + "Gimmicks.h",
  "Holds a gimmick's object identity, stacking state, kind, material, carrier and support count.", "med",
  c(("30DB0.c", "&& (D_800F45E0[i]->unk16C.actorId == arg0->actorId))) {")), [
      ("actorId", "objectId", "Object id matched when models are shared", HIGH),
      ("unk0_3", "isFalling", "Set when the stack under it is removed", LOW, c(("30DB0.c", "temp_s0->unk16C.unk0_3 = 1;"))),
      ("unk0_4", "interaction", "Interaction kind; 1 is interactable ahead, 2 drops", LOW,
       c(("3A1A0.c", "if (object->unk16C.unk0_4 == 1) {"))),
      ("unk0_7", "reservedBits7", "Not read", LOW),
      ("unk1", "subtype", "Gimmick subtype", HIGH, c(("30DB0.c", "return temp_a0->unk16C.unk1;"))),
      ("unk2", "material", "Material; also the orientation table index", MED, c(("30DB0.c", "obj->unk16C.unk2 = material;"))),
      ("unk3", "carrier", "Gimmick it rests on; 0xFF when none", MED, c(("30DB0.c", "s1->unk16C.unk3 = 0xFF;"))),
      ("unk4", "supportIndex", "First support slot scanned when moving", LOW, c(("3A1A0.c", "for (i = object->unk16C.unk4; i < 15; i++) {"))),
      ("unk5", "reserved5", "Not read", LOW),
      ("unk6", "reserved6", "Not read", LOW),
      ("unk7", "reserved7", "Not read", LOW),
      ("unk8", "reserved8", "Not read", LOW),
  ])
T("T00244", "State", "Actors::MovementRequest", "Native\\Game\\src\\Actors\\Movement.h",
  "Holds an actor's pending movement: mode, direction, facing, distance and step vector.", "med",
  c(("3A1A0.c", "mode = actor->unk1848.unk0;")), [
      ("unk0", "mode", "Movement mode (0 none, 1, 2, 6 turn, 7 jump)", MED, c(("3A1A0.c", "actor->unk1848.unk0 = 7;"))),
      ("unk1", "reserved1", "Not read", LOW),
      ("unk2", "reserved2", "Not read", LOW),
      ("unk3", "reserved3", "Not read", LOW),
      ("unk4", "direction", "Movement direction", MED, c(("3A1A0.c", "actor->unk1848.unk4 = direction;"))),
      ("unk6", "facing", "Facing to turn to", MED, c(("38C1C.c", "func_800AA850(actor->unk0.unkF, actor->unk1848.unk6, 12);"))),
      ("unk8", "distance", "Distance or speed of the move", MED, c(("3A1A0.c", "actor->unk1848.unk8 = distance;"))),
      ("unkC", "reservedC", "Not read", LOW),
      ("unk10", "step", "Movement vector added to the position", HIGH,
       c(("38C1C.c", "addVector(&arg0->unk0.position, &arg0->unk1848.unk10);"))),
  ])
T("T00246", "State", "Actors::CompanionOffset", "Native\\Game\\src\\Actors\\ActorRecord.h",
  "Offsets a linked companion actor from its anchor and biases its draw depth.", "med",
  c(("44F14.c", "frame->depth += actor->unk6F4.unk3 * 4;")), [
      ("unk0", "offsetX", "X offset added to the position", MED, c(("44F14.c", "+ (signed char)(u_char)actor->unk6F4.unk0;"))),
      ("unk1", "offsetY", "Y offset added to the position", MED, c(("44F14.c", "+ (signed char)(u_char)actor->unk6F4.unk1;"))),
      ("unk2", "offsetZ", "Z offset added to the position", MED, c(("44F14.c", "+ (signed char)(u_char)actor->unk6F4.unk2;"))),
      ("unk3", "depthBias", "Draw depth bias in units of 4; 8 by default", MED, c(("44F14.c", "frame->depth += actor->unk6F4.unk3 * 4;"))),
  ])
T("T00264", "State", "Battle::EffectInstanceTargets", B + "Effects.h",
  "Holds a running effect's four target slots, its timeline, renderer, effect id, phase and parameter.", "med",
  c(("5BF94.c", "arg1->unkD1C.unk0 = *arg0->unk0;"), ("5BF94.c", "spawn.unk4 = &arg0->targets[i];")), [
      ("unk0", "source", "Effect source", MED, c(("5BF94.c", "arg1->unkD1C.unk0 = *arg0->unk0;"))),
      ("unkC", "emitFrom", "Where the effect starts: the source, or the target for reversed effects", LOW,
       c(("5BF94.c", "arg1->unkD1C.unkC = *arg0->unk4;"))),
      ("unk18", "target", "Effect target; byte 2 also selects a color preset", LOW, c(("5BF94.c", "arg1->unkD1C.unk18 = *arg0->unk8;"))),
      ("unk24", "emitTo", "Where the effect ends: the target, or the source for reversed effects", LOW,
       c(("5BF94.c", "arg1->unkD1C.unk24 = *arg0->unkC;"))),
      ("unk30", "timeline", "Effect timeline", MED, c(("5BF94.c", "if (var_s0->unkD1C.unk30 != NULL) {"))),
      ("unk34", "renderer", "Renderer attached to the effect", MED, c(("5BF94.c", "var_s1 = arg0->unkD1C.unk34;"))),
      ("unk38", "effectId", "Effect id", HIGH, c(("5BF94.c", "child->unkD1C.unk38 = arg0->effectId;"))),
      ("unk3A", "phase", "Secondary effect phase", MED, c(("5BF94.c", "switch (var_s1->unkD1C.unk3A) {"))),
      ("unk3C", "parameter", "Spawn parameter; nonzero attaches a renderer", LOW, c(("5BF94.c", "child->unkD1C.unk3C = arg2;"))),
  ])
T("T00272", "OnDisc", "Battle::Ai::RouteGateEnd", AI + "Navigation.h",
  "Gives the tile of one end of a route gate; ends are stored in linked pairs.", "med",
  c(("771A8.c", "if (D_800F5910[i * 2].unk0_0 == arg0->unk0")), [
      ("unk0_0", "tileX", "Tile X", MED, c(("771A8.c", "if (D_800F5910[i * 2].unk0_0 == arg0->unk0"))),
      ("unk0_5", "tileZ", "Tile Z", MED, c(("771A8.c", "&& D_800F5910[i * 2].unk0_5 == arg0->unk2) {"))),
      ("unk0_10", "reservedBits10", "Not read", LOW),
  ])
T("T00295", "Value", "Render::PolyFt4", "Native\\Engine\\src\\Render\\Primitives.h",
  "Is a flat textured quad GPU packet: color code, then XY and UV words with the palette and texture page.", "med",
  "declaration-only;" + c(("gpu.h", "} VS_POLY_FT4;")) + ";standard PSX POLY_FT4 word layout", [
      ("r0g0b0code", "colorCode", "Color and command", MED), ("x0y0", "xy0", "Vertex 0", MED),
      ("u0v0clut", "uv0Palette", "UV 0 and palette", MED), ("x1y1", "xy1", "Vertex 1", MED),
      ("u1v1tpage", "uv1TexturePage", "UV 1 and texture page", MED), ("x2y2", "xy2", "Vertex 2", MED),
      ("u2v2", "uv2", "UV 2", MED), ("x3y3", "xy3", "Vertex 3", MED), ("u3v3", "uv3", "UV 3", MED),
  ])

# ---------------------------------------------------------------- group C: battle-state records
SPHERE = "D_800EB9B8"
T("T00215", "State", "Battle::TargetSphere", B + "TargetVolume.h",
  "Holds the targeting sphere: its two volumes, wireframe animation, selected targets, labels and terrain cache.",
  "med", c(("58578.c", "menuShapeVertex* vertices = D_800EB9B8->unk48;"), ("58578.c", "D_800EB9B8_unk990* var_s1 = D_800EB9B8->unk990;")), [
      ("unk0", "volumes", "Primary and secondary target volumes", HIGH, c(("58578.c", "*shape = D_800EB9B8->unk0[i];"))),
      ("unk20", "depth", "Average screen depth of the sphere", MED, c(("58578.c", "gte_stszotz(&D_800EB9B8->unk20);"))),
      ("unk22", "cameraYaw", "Camera yaw used to orient the sphere", MED, c(("58578.c", "D_800EB9B8->unk22 = yaw;"))),
      ("unk24", "wirePhaseA", "Wireframe animation phase advanced by 3 per shape step", LOW),
      ("unk26", "wirePhaseB", "Wireframe animation phase advanced by 7 per shape step", LOW),
      ("unk28", "wirePhaseC", "Wireframe animation phase advanced by 11 per shape step", LOW),
      ("unk2A", "wireShape", "Wireframe shape index; 15 when set up", MED, c(("58578.c", "int shape = D_800EB9B8->unk2A;"))),
      ("unk2C", "targetCount", "Number of selectable targets", MED),
      ("unk2D", "selectedTarget", "Selected target; clamped to the count", LOW),
      ("unk2E", "sphereMode", "Mode passed when the volumes are set", LOW),
      ("unk2F", "reserved2F", "Not read", LOW),
      ("unk30", "reserved30", "Not read", LOW),
      ("unk31", "targetMode", "Mode passed when the targets are set", LOW),
      ("unk32", "vertexCount", "Wireframe vertices built", LOW),
      ("unk34", "reserved34", "Not read", LOW),
      ("unk38", "reserved38", "Cleared on setup; not read", LOW),
      ("unk3C", "reserved3C", "Cleared on setup; not read", LOW),
      ("unk3E", "inputLocked", "Nonzero blocks sphere interaction unless a button is held", LOW),
      ("unk40", "targetLists", "Target preview lists shown on the sphere", MED, c(("58578.c", "D_800EB9B8->unk40[0] = arg1;"))),
      ("unk48", "wireVertices", "Wireframe vertices", HIGH),
      ("unk990", "labels", "Floating target labels", HIGH),
      ("unkB70", "reservedB70", "Not read by decompiled code", LOW),
      ("unk4370", "terrainHeights", "Cached terrain heights around the sphere (9x9)", MED),
      ("unk44B4", "wireEdges", "Wireframe edge list", MED, c(("573B8.c", "edges = D_800EB9B8->unk44B4;"))),
      ("unk4AB8", "terrainRowFlags", "Valid flags of the cached terrain (9x9)", MED),
  ], recv=SPHERE + r"\s*")
D("T00215", ["T00216"], "the same declaration in another translation unit")
T("T00214", "State", "Battle::TargetSphereSetupView", B + "TargetVolume.h",
  "Views Battle::TargetSphere while its volumes are set up, with the two volumes as separate members.", "low",
  c(("573B8.c", "D_800EB9B8->unk10 = *arg0;")) + ";coarser view of T00215 (same storage)", [
      ("unk0", "primaryVolume", "Primary target volume", HIGH, c(("573B8.c", "D_800EB9B8->unk0 = *arg0;"))),
      ("unk10", "secondaryVolume", "Secondary target volume", HIGH, c(("573B8.c", "D_800EB9B8->unk10 = *arg0;"))),
      ("unk20", "depthAndYaw", "Depth and camera yaw as one word", LOW),
      ("unk24", "wirePhaseA", "Wireframe animation phase", LOW, c(("573B8.c", "D_800EB9B8->unk24 = 0;"))),
      ("unk26", "wirePhaseB", "Wireframe animation phase", LOW),
      ("unk28", "wirePhaseC", "Wireframe animation phase", LOW, c(("573B8.c", "D_800EB9B8->unk28 = 0;"))),
      ("unk2A", "wireShape", "Wireframe shape index", MED, c(("573B8.c", "D_800EB9B8->unk2A = 0xF;"))),
      ("unk2C", "targetCount", "Number of selectable targets", LOW),
      ("unk2D", "selectedTarget", "Selected target; -1 after init", LOW, c(("573B8.c", "D_800EB9B8->unk2D = -1;"))),
      ("unk2E", "sphereMode", "Mode passed when the volumes are set", LOW, c(("573B8.c", "D_800EB9B8->unk2E = arg2;"))),
      ("unk2F", "reserved2F", "Not read", LOW),
      ("unk30", "reserved30", "Not read", LOW),
      ("unk34", "reserved34", "Not read", LOW),
      ("unk38", "reserved38", "Cleared on setup; not read", LOW, c(("573B8.c", "D_800EB9B8->unk38 = 0;"))),
      ("unk3C", "reserved3C", "Cleared on setup; not read", LOW, c(("573B8.c", "D_800EB9B8->unk3C = 0;"))),
      ("unk40", "targetList0", "First target list", LOW),
      ("unk44", "targetList1", "Second target list; set from a setup parameter", LOW, c(("573B8.c", "D_800EB9B8->unk44 = arg3;"))),
      ("unk48", "wireVertices", "Wireframe vertices", HIGH, c(("573B8.c", "menuShapeVertex* vertex = &D_800EB9B8->unk48[index];"))),
      ("unk990", "labelArea", "Label and terrain storage", LOW),
      ("unk44B4", "wireEdges", "Wireframe edge list", MED, c(("573B8.c", "edges = D_800EB9B8->unk44B4;"))),
      ("unk4AB8", "terrainRowFlags", "Valid flags of the cached terrain", LOW),
  ])
T("T00226", "State", "Battle::ActionRequest", B + "Action.h",
  "Holds one action being chosen or executed: action id, variant, damage type, primary and other targets, and "
  "focus position.", "high",
  c(("146C.c", "D_800F19CC->unk8.actionIndex = arg1;"), ("146C.c", "D_800F19CC->unk8.unk4A = D_800F19CC->unk2C00;")), [
      ("actionIndex", "actionId", "Action", HIGH),
      ("unk2", "effectVariant", "Variant passed to the effect presentation", LOW, c(("146C.c", "func_800CF0E8(&request, mode, action->unk2);"))),
      ("unk3", "damageType", "Damage type of the weapon used", MED, c(("146C.c", "result->unk3 = attacker->weapon.damageType;"))),
      ("unk4", "primaryTarget", "Primary target and its hit result", HIGH,
       c(("146C.c", "D_800F19CC->unk8.unk4.unk0.targetActor = arg0;"))),
      ("unk48", "animationParameter", "Parameter passed to the action animation", LOW, c(("146C.c", "result->unk48 = 1;"))),
      ("unk4A", "targetCount", "Number of targets", HIGH, c(("146C.c", "D_800F19CC->unk8.unk4A = D_800F19CC->unk2C00;"))),
      ("unk4C", "targets", "Targets and their hit results", HIGH,
       c(("146C.c", "D_800F19CC->unk8.unk4C[i].unk0.targetLimb = D_800F19CC->unk29C0[i].unkA;"))),
      ("unk844", "focusPosition", "Position the action and camera focus on", MED,
       c(("146C.c", "D_800F19CC->unk8.unk844 = D_800F19CC->unk2990;"))),
  ])
T("T00311", "State", "Battle::HitResult", B + "Hit.h",
  "Holds the queued outcome of an action on one target: stat changes, statuses, per-stat effect directions, limb "
  "results and equipment wear.", "high",
  c(("146C.c", "switch (arg0->strengthEffect) {"), ("146C.c", "arg2->strength += amount;"))
  + ";upstream merged func_80085A34_t into this type", [
      ("unk0", "target", "Target actor and limb with HP, MP and hit chance", HIGH,
       c(("146C.c", "D_800F19CC->unk8.unk4.unk0.targetActor = arg0;"))),
      ("risk", "riskChange", "Risk change", HIGH, c(("146C.c", "arg2->risk += temp_v0;"))),
      ("strength", "strengthChange", "Strength change", HIGH, c(("146C.c", "arg2->strength += amount;"))),
      ("intelligence", "intelligenceChange", "Intelligence change", HIGH, c(("146C.c", "arg2->intelligence += amount;"))),
      ("agility", "agilityChange", "Agility change", HIGH, c(("146C.c", "arg2->agility += amount;"))),
      ("maxHp", "maxHpChange", "Maximum HP change", HIGH, c(("146C.c", "arg2->maxHp += amount;"))),
      ("maxMp", "maxMpChange", "Maximum MP change", HIGH, c(("146C.c", "arg2->maxMp += amount;"))),
      ("removeStatuses", "removeStatuses", "Statuses removed", HIGH, c(("146C.c", "entity->removeStatuses |= remove;"))),
      ("inflictStatuses", "inflictStatuses", "Statuses inflicted", HIGH, c(("146C.c", "entity->inflictStatuses |= inflict;"))),
      ("hpEffect", "hpEffect", "HP effect: 1 damage, 2 restore, 3 other", HIGH),
      ("mpEffect", "mpEffect", "MP effect: 1 drain, 2 restore", HIGH, c(("146C.c", "arg2->mpEffect = 1;"))),
      ("riskEffect", "riskEffect", "Risk effect direction", HIGH, c(("146C.c", "arg2->riskEffect = 2;"))),
      ("strengthEffect", "strengthEffect", "Strength effect: 1 down, 2 up", HIGH, c(("146C.c", "arg2->strengthEffect = 2;"))),
      ("intelligenceEffect", "intelligenceEffect", "Intelligence effect: 1 down, 2 up", HIGH,
       c(("146C.c", "arg2->intelligenceEffect = 2;"))),
      ("agilityEffect", "agilityEffect", "Agility effect: 1 down, 2 up", HIGH, c(("146C.c", "arg2->agilityEffect = 2;"))),
      ("maxHpEffect", "maxHpEffect", "Maximum HP effect direction", HIGH, c(("146C.c", "arg2->maxHpEffect = 2;"))),
      ("maxMpEffect", "maxMpEffect", "Maximum MP effect direction", HIGH, c(("146C.c", "arg2->maxMpEffect = 2;"))),
      ("unk1C_16", "statStatusEffect", "Stat status change: 1 down, 2 up (Strength/Intelligence/Agility down/up)", MED,
       c(("146C.c", "arg2->unk1C_16 = 1;"))),
      ("weaponDpEffect", "weaponDpEffect", "Weapon DP effect direction", HIGH, c(("146C.c", "arg2->weaponDpEffect = 2;"))),
      ("weaponPpEffect", "weaponPpEffect", "Weapon PP effect: 1 drain, 2 restore", HIGH, c(("146C.c", "arg2->weaponPpEffect = 1;"))),
      ("armorDpEffect", "armorDpEffect", "Armor DP effect direction", HIGH, c(("146C.c", "arg2->armorDpEffect = 1;"))),
      ("shieldPpEffect", "shieldPpEffect", "Shield PP effect direction", HIGH, c(("146C.c", "arg2->shieldPpEffect = 1;"))),
      ("unk1C_26", "sourceResult", "Set to 2 when the source action succeeds", LOW, c(("146C.c", "arg1->unk1C_26 = 2;"))),
      ("unk1C_28", "reservedBits28", "Not read", LOW),
      ("limbs", "limbs", "Per-limb results", HIGH, c(("146C.c", "entity->limbs[i].effects.value0 += _armorDpAdjustmentAmounts[i];"))),
      ("weaponDp", "weaponDpChange", "Weapon DP change", HIGH, c(("146C.c", "actor->weapon.currentDp += arg0->weaponDp;"))),
      ("weaponPp", "weaponPpChange", "Weapon PP change", HIGH, c(("146C.c", "arg2->weaponPp += amount;"))),
      ("armorDp", "armorDpChange", "Armor DP change", HIGH, c(("146C.c", "arg2->armorDp = arg2->unk0.hp * 2;"))),
      ("shieldPp", "shieldPpChange", "Shield PP change", HIGH, c(("146C.c", "arg2->shieldPp += amount;"))),
      ("unk40", "targetsPosition", "Nonzero when the target is a position rather than an actor", MED,
       c(("146C.c", "if (arg1->unk40 == 0) {"))),
  ])
T("T00225", "State", "Battle::ActionState", B + "Action.h",
  "Holds the battle action system: the action being chosen, the four-entry execution queue, target selection volumes "
  "and previews, chain-ability input and action camera settings.", "high",
  c(("146C.c", "D_800F19CC_t2* temp_s0 = &D_800F19CC->unk854[D_800F19CC->unk0 & 3];")), [
      ("unk0", "queueIndex", "Index of the executing entry (low two bits)", HIGH,
       c(("146C.c", "D_800F19CC_t2* temp_s0 = &D_800F19CC->unk854[D_800F19CC->unk0 & 3];"))),
      ("unk4", "chainStep", "Chain ability step; 1 when an action is committed", MED, c(("146C.c", "++D_800F19CC->unk4;"))),
      ("unk8", "selection", "Action being chosen", HIGH, c(("146C.c", "D_800F19CC->unk8.actionIndex = arg1;"))),
      ("unk854", "queue", "Actions queued for execution", HIGH),
      ("unk2984", "isPlayerAction", "1 when Ashley chooses the action, 0 for other actors", MED, c(("146C.c", "D_800F19CC->unk2984 = 1;"))),
      ("unk2988", "sourceActorId", "Actor whose targets are collected", LOW, c(("146C.c", "D_800F19CC->unk2988 = arg0;"))),
      ("unk298C", "selectedPreview", "Selected target preview; 0xFF none", MED, c(("146C.c", "temp_s0 = D_800F19CC->unk298C;"))),
      ("unk298D", "cameraPreview", "Preview the camera follows; 0xFF none", MED,
       c(("146C.c", "if ((D_800F19CC->unk298D != 0xFF) && (D_800F19CC->unk2C00 != 0)) {"))),
      ("unk298E", "selectionFrames", "Frames spent selecting", MED, c(("146C.c", "if (++D_800F19CC->unk298E >= 0x1F) {"))),
      ("unk298F", "reserved298F", "Not read", LOW),
      ("unk2990", "savedAreaCenter", "Area center saved before filtering", MED, c(("146C.c", "D_800F19CC->unk2990 = D_800F19CC->unk2998;"))),
      ("unk2998", "areaCenter", "Area-of-effect center", MED, c(("146C.c", "targetPos.vx = D_800F19CC->unk2998.vx;"))),
      ("unk29A0", "rangeShape", "Shape of the range volume", MED, c(("146C.c", "D_800F19CC->unk29A0 = vs_main_actions[action].shape;"))),
      ("unk29A1", "rangeKind", "Range volume kind: 1 direct, 2 ground, 3 default", MED, c(("146C.c", "D_800F19CC->unk29A1 = 2;"))),
      ("unk29A2", "rangeFacing", "Facing of the range volume", MED, c(("146C.c", "D_800F19CC->unk29A2 = state.unk0.unk4.pad;"))),
      ("unk29A4", "rangeX", "Range volume X size", HIGH, c(("146C.c", "D_800F19CC->unk29A4 = spec->rangeX;"))),
      ("unk29A5", "rangeY", "Range volume Y size", HIGH, c(("146C.c", "D_800F19CC->unk29A5 = spec->rangeY;"))),
      ("unk29A6", "rangeZ", "Range volume Z size", HIGH, c(("146C.c", "D_800F19CC->unk29A6 = spec->rangeZ;"))),
      ("unk29A7", "rangeAngle", "Range volume shape angle (x8)", HIGH, c(("146C.c", "D_800F19CC->unk29A7 = spec->angle * 8;"))),
      ("unk29A8", "rangeOrigin", "Origin of the range volume", HIGH,
       c(("146C.c", "func_800A190C(0, 250, &D_800F19CC->unk29A8, 0);"))),
      ("unk29B0", "areaShape", "Shape of the area-of-effect volume", MED, c(("146C.c", "D_800F19CC->unk29B0 = spec->shape;"))),
      ("unk29B1", "areaKind", "Area volume kind", LOW, c(("146C.c", "D_800F19CC->unk29B1 = 1;"))),
      ("unk29B2", "areaFacing", "Facing of the area volume", MED, c(("146C.c", "D_800F19CC->unk29B2 = ratan2(dx, dz);"))),
      ("unk29B4", "areaX", "Area volume X size", HIGH, c(("146C.c", "D_800F19CC->unk29B4 = spec->aoe_0;"))),
      ("unk29B5", "areaY", "Area volume Y size", HIGH, c(("146C.c", "D_800F19CC->unk29B5 = spec->aoe_8;"))),
      ("unk29B6", "areaZ", "Area volume Z size", HIGH, c(("146C.c", "D_800F19CC->unk29B6 = spec->aoe_16;"))),
      ("unk29B7", "areaAngle", "Area volume shape angle or pitch", MED, c(("146C.c", "D_800F19CC->unk29B7 = spec->aoe_24 & 0xF8;"))),
      ("unk29B8", "areaOrigin", "Origin of the area volume", HIGH, c(("146C.c", "D_800F19CC->unk29B8 = D_800F19CC->unk2990;"))),
      ("unk29C0", "previews", "Selectable target previews", HIGH),
      ("unk2BE8", "reserved2BE8", "Not read", LOW),
      ("unk2BEC", "reserved2BEC", "Not read", LOW),
      ("unk2BF0", "reserved2BF0", "Not read", LOW),
      ("unk2BF4", "reserved2BF4", "Not read", LOW),
      ("unk2BF8", "reserved2BF8", "Not read", LOW),
      ("unk2BFC", "reserved2BFC", "Not read", LOW),
      ("unk2C00", "previewCount", "Number of target previews", HIGH, c(("146C.c", "for (i = 0; i < D_800F19CC->unk2C00; ++i) {"))),
      ("unk2C02", "previousChainButtons", "Chain buttons held last frame", LOW, c(("146C.c", "D_800F19CC->unk2C02 = 0;"))),
      ("unk2C03", "chainAbilitySlot", "Mapped chain ability slot + 1", HIGH,
       c(("146C.c", "vs_main_settings.mappedChainAbilities[D_800F19CC->unk2C03 - 1] - 0x16,"))),
      ("previousChainInput", "previousChainInput", "Chain button pressed last", HIGH),
      ("battleAbilityInputSuccessful", "abilityInputSucceeded", "Timed ability input succeeded", HIGH),
      ("battleAbilityInputAttempted", "abilityInputAttempted", "Timed ability input attempted", HIGH),
      ("unk2C07", "chainContinued", "Set once a chain ability continues the action", LOW, c(("146C.c", "D_800F19CC->unk2C07 = 1;"))),
      ("battleAbilityTimer", "abilityTimer", "Timer of the ability input window", HIGH),
      ("battleAbilityTargetWindow", "abilityWindow", "Ability input window; 0 when closed", HIGH),
      ("unk2C0C", "preferredCameraDistance", "Camera distance kept after the action", MED, c(("146C.c", "D_800F19CC->unk2C0C = 0x600;"))),
      ("unk2C0E", "cameraZoomState", "Action camera zoom state", LOW, c(("146C.c", "if (D_800F19CC->unk2C0E == 0) {"))),
  ], recv=r"D_800F19CC")
T("T00271", "State", "Battle::Ai::RoomAiState", AI + "AiState.h",
  "Holds room-wide AI state: tile bounds, action thresholds, flight ceiling, engaged-actor list and special actors.",
  "med", c(("771A8.c", "if (dest.p.x < D_800F58BC->minX) {")), [
      ("maxX", "maxTileX", "Largest tile X", HIGH), ("maxZ", "maxTileZ", "Largest tile Z", HIGH),
      ("minX", "minTileX", "Smallest tile X", HIGH), ("minZ", "minTileZ", "Smallest tile Z", HIGH),
      ("unk4", "reserved4", "Not read", LOW),
      ("unk8", "defaultActionDelay", "Action delay used when an actor has none of its own", LOW,
       c(("71618.c", "arg0->unk13F = arg0->unk13 == 0 ? D_800F58BC->unk8 : 50;"))),
      ("unk9", "reserved9", "Not read", LOW),
      ("unkA", "allowsNonSpellActions", "Marks non-spell action entries usable", LOW,
       c(("upstream:6E644.c", "entry->unk0_6 = D_800F58BC->unkA && action->type != actionTypeSpell"))),
      ("unkB", "reservedB", "Not read", LOW),
      ("unk14", "flightCeiling", "Height limit for flying actors; 0 when none", MED,
       c(("771A8.c", "} else if (D_800F58BC->unk14 && D_800F58BC->unk14 >= actorY) {"))),
      ("unk18", "readinessLevel", "Threshold an action entry must not exceed to be ready", LOW,
       c(("6FC00.c", "if (entry->weight && entry->threshold <= D_800F58BC->unk18) {"))),
      ("unk1A", "reserved1A", "Not read", LOW),
      ("unk1C", "engagedHead", "First engaged actor", MED, c(("73700.c", "actorTimerState* head = (actorTimerState*)D_800F58BC->unk1C;"))),
      ("unk20", "engagedTail", "Last engaged actor", LOW, c(("73700.c", "if (arg0 == D_800F58BC->unk20) {"))),
      ("unk24", "approachMoves", "Approach moves made this update", LOW, c(("71618.c", "++D_800F58BC->unk24;"))),
      ("unk28", "specialActors", "Actors whose models are of kind 2 or 4", MED,
       c(("71618.c", "vs_battle_actor* actor = D_800F58BC->unk28[j];"))),
  ])
T("T00273", "State", "Battle::Ai::ArenaOrbitState", AI + "Movement.h",
  "Drives a boss circling the arena centre (0x7C0, 0x7C0): heading, phase, path step, cooldown and saved plan.",
  "low", c(("upstream:6E644.c", "switch (motion->unk2) {"), ("7EE98.c", "temp_s0->unkC = 900;")), [
      ("unk0", "heading", "Heading toward the arena centre", LOW, c(("upstream:6E644.c", "motion->unk0 = (-angle + ONE * 3 / 4) & 0xFFF;"))),
      ("unk2", "phase", "Orbit phase", MED, c(("upstream:6E644.c", "switch (motion->unk2) {"))),
      ("unk3", "distanceCached", "Set once the distance is measured", LOW, c(("upstream:6E644.c", "motion->unk3 = 0;"))),
      ("unk4", "distanceSquared", "Squared distance to the arena centre", MED,
       c(("upstream:6E644.c", "motion->unk4 = func_800E4660(0x7C0 - arg0->unk4C.vx, 0, 0x7C0 - arg0->unk4C.vz);"))),
      ("unk8", "reserved8", "Not read", LOW),
      ("unkA", "isOrbiting", "Set while orbiting", LOW, c(("7EE98.c", "temp_s0->unkA = 0;"))),
      ("unkB", "pathStep", "Step in the orbit path table", MED, c(("upstream:6E644.c", "++motion->unkB;"))),
      ("unkC", "cooldown", "Frames before the orbit can restart; 900 when started", MED, c(("7EE98.c", "temp_s0->unkC = 900;"))),
      ("unkE", "requested", "Orbit requested", LOW, c(("7EE98.c", "temp_s0->unkE = 0;"))),
      ("unk10", "orbitAngle", "Angle around the centre", MED, c(("upstream:6E644.c", "motion->unk10 = (-angle + ONE * 3 / 4) & 0xFFF;"))),
      ("unk12", "savedHeight", "Height saved when the orbit starts", LOW, c(("7EE98.c", "temp_s0->unk12 = arg0->unk4E;"))),
      ("unk14", "healthPhase", "Advances when the boss's HP ratio falls below 3/4, then below 1/4; each advance requests an orbit",
       MED, c(("upstream:6E644.c", "if ((!motion->unk14 && ratio < 0xC000)"))),
      ("unk15", "reserved15", "Not read", LOW),
      ("unk16", "planSaved", "The actor's plan count is saved", LOW, c(("7EE98.c", "temp_s0->unk16 = 1;"))),
      ("unk17", "savedPlanCount", "Plan count restored when the orbit ends", LOW, c(("7EE98.c", "temp_s0->unk17 = arg0->unk470;"))),
      ("unk18", "startX", "X where the orbit started", MED, c(("upstream:6E644.c", "motion->unk18 = arg0->unk4C.vx;"))),
      ("unk1A", "startZ", "Z where the orbit started", MED, c(("upstream:6E644.c", "motion->unk1A = arg0->unk4C.vz;"))),
      ("unk1C", "started", "Set when the orbit begins", LOW, c(("upstream:6E644.c", "motion->unk1C = 1;"))),
  ])

if __name__ == "__main__":
    g.write()
