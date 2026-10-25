"""Game_Battle_03 types and fields from code read directly. Citations are resolved by exact text search (common.Gen).

Group A: the AI actor state (T00532, upstream func_800E78F4_t) and its remaining fork views, by offset (ai_state.TABLE).
Group B: canonical AI entry types named from their already-named fork views (same bits), and other battle types by
common.Gen.generic_specs (used members keep meaningful fork names or become opaque; unread members are reserved).
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ai_state  # noqa: E402
from common import Gen  # noqa: E402

g = Gen("Game_Battle_03")
c, T = g.c, g.T
L, M, H = "low", "med", "high"
B = "Native\\Game\\src\\Battle\\"
AI = B + "Ai\\"
STATE_RECV = r"\b(?:state|arg0|arg1|s|other|firstOther|allyState|self|context|target|motion)|D_800F5878\[[^\]]*\]\)?"
ANYR = r"[\w\]\)]+"
VIEW = ";view of the AI actor state T00532 (upstream func_800E78F4_t); fold into it when copying"

# ---------------------------------------------------------------- group A: AI actor state and views
T("T00532", "State", "Battle::Ai::ActorAiState", AI + "AiState.h",
  "Holds one actor's AI state: settings, tile and movement state, perception history and relations, action slots, "
  "candidates and plan, allocated per actor when its AI starts.", "high",
  c(("7CD68.c", "state = vs_main_allocHeap(0x484);"), ("upstream:6E644.c", "switch (arg0->unk12E) {"))
  + ";names by offset from sym_gen/ai_state.py, shared with every fork view", ai_state.view_specs(g, "T00532"),
  recv=STATE_RECV)
for _tid, _name, _purpose in (
        ("T00412", "Battle::Ai::EscortView", "Views the AI actor state for escort and close-follow behavior."),
        ("T00414", "Battle::Ai::EscortRelationsView", "Views the AI actor state's relation words for escort behavior."),
        ("T00514", "Battle::Ai::MoveTargetView", "Views the first 364 bytes of the AI actor state when a tile move is chosen."),
        ("T00524", "Battle::Ai::ActionExecutionView", "Views the AI actor state while an action is executed."),
        ("T00526", "Battle::Ai::FloorHeightView", "Views the AI actor state while floor heights are sampled."),
        ("T00554", "Battle::Ai::MovementCheckView", "Views the AI actor state while a direction is checked."),
        ("T00555", "Battle::Ai::MovementDecisionView", "Views the AI actor state while movement is decided."),
        ("T00556", "Battle::Ai::MovementDestinationView", "Views the AI actor state while a destination is chosen."),
        ("T00557", "Battle::Ai::MovementPathView", "Views the AI actor state while an obstacle detour is followed."),
        ("T00559", "Battle::Ai::MovementRecoveryView", "Views the AI actor state while blocked movement is recovered."),
        ("T00561", "Battle::Ai::MovementStepView", "Views the AI actor state while it steps or flies."),
        ("T00582", "Battle::Ai::PatrolView", "Views the AI actor state while it patrols waypoints."),
        ("T00416", "Battle::Ai::DirectionCacheView", "Views the AI actor state's tiles and flight height for the direction cache."),
        ("T00564", "Battle::Ai::WaypointSearchView", "Views the AI actor state's position during a waypoint search."),
        ("T00569", "Battle::Ai::ObstructionView", "Views the AI actor state for obstruction tests.")):
    T(_tid, "State", _name, AI + "AiState.h", _purpose, M,
      "declaration-only;" + g.decl(_tid, g.members[_tid][0]["name"]) + VIEW, ai_state.view_specs(g, _tid), recv=STATE_RECV)

# ---------------------------------------------------------------- group B: canonical entries from their views
T("T00517", "State", "Battle::Ai::ActionCandidate", AI + "AiState.h",
  "Scores one action candidate: action slot, readiness threshold, squared distance, target, weight and range.", "med",
  "declaration-only;" + g.decl("T00517", "unk0") + ";names from the same bits of T00350 (Battle::Ai::CandidateEntryView)",
  g.generic_specs("T00517", g.names_from_views("T00517", ["T00350"])), recv=ANYR)
T("T00520", "State", "Battle::Ai::ActionSlotMetadata", AI + "AiState.h",
  "Holds an action slot's AI metadata: kind, range class, special flag, radius and squared range.", "med",
  "declaration-only;" + g.decl("T00520", "unk4") + ";names from the same bits of T00366 (Battle::Ai::ActionMetadataEntry)",
  g.generic_specs("T00520", g.names_from_views("T00520", ["T00366"])), recv=ANYR)
T("T00515", "State", "Battle::Ai::RelationWord", AI + "Perception.h",
  "Holds what an AI actor perceives about one other actor (upstream bit split of the relation word).", "med",
  "declaration-only;" + g.decl("T00515", "unk0_0") + ";names from the same bits of T00136 (Battle::Ai::TargetRelationBits)",
  g.generic_specs("T00515", g.names_from_views("T00515", ["T00136"])), recv=ANYR)

# ---------------------------------------------------------------- group B: other battle and AI types
UNION = {"raw": ("word", "Whole word", M), "p": ("tile", "Tile fields", M), "bits": ("bits", "Bit fields", M),
         "u8": ("bytes", "Bytes of the word", M), "s32": ("word", "Whole word", M), "packed": ("word", "Whole word", M),
         "fields": ("fields", "Field view", M), "vec": ("vector", "Vector view", L), "words": ("words", "Word view", L)}
OTHER = [
    ("T00413", "Battle::Ai::EscortRelationBits", "Views a relation word's mode and category bits for escort behavior.", None),
    ("T00415", "Battle::Ai::DirectionCacheBits", "Holds tested and open direction bits of the direction cache.", None),
    ("T00805", "Battle::Ai::DirectionCacheTile", "Holds the cached tile word as a word or tile bytes.", UNION),
    ("T00418", "Battle::EffectDefinitionView", "Views an effect definition record while effect transforms are set up.", {
        "unk0": ("flags", "Bit 1 attaches to an actor (bits 3-4 pick the source), bit 7 targets an actor (bits 10-11 pick the source)", M,
                 c(("5BF94.c", "switch (definition->unk0 & 0x18) {"))),
        "unk2": ("attachment", "Attached actor in byte 0 and bone in byte 1", M, c(("5BF94.c", "state->actor = (u_char)definition->unk2;"))),
        "unk8": ("rotationXAndTarget", "Initial rotation x in the low half; target actor (low 5 bits) and bone in bytes 2-3", M,
                 c(("5BF94.c", "state->targetActor = ((u_char*)&definition->unk8)[2] & 31;"))),
        "unkC": ("rotationY", "Initial rotation y", M, c(("5BF94.c", "state->rotation[1] = definition->unkC;"))),
        "unk10": ("rotationZ", "Initial rotation z", M, c(("5BF94.c", "state->rotation[2] = definition->unk10;"))),
    }),
    ("T00420", "Battle::EffectTrailConfig", "Gives an effect trail's actor, bone, color, texture and lifetime.", None),
    ("T00422", "Battle::EffectTransformState", "Holds one effect transform: rotation, frame, position, attachment and targets.", None),
    ("T00423", "Battle::EnvironmentTarget", "Describes one room object an action can target: position, radius and kind.", None),
    ("T00424", "Battle::EnvironmentTargetList", "Lists the room objects an action can target.", None),
    ("T00429", "Audio::SoundReleaseView", "Views a sound record while it is released; not read.", None),
    ("T00436", "Actors::ActorLocationWords", "Holds an actor location as three words; not read.", None),
    ("T00806", "Actors::AuxiliaryActorWord", "Holds an auxiliary actor setup word as bytes or a word; not read.", UNION),
    ("T00440", "Actors::SpawnScratch", "Holds a 16-byte spawn scratch value; not read.", None),
    ("T00441", "Actors::SpawnScratchPair", "Holds two spawn scratch blocks; not read.", None),
    ("T00442", "Battle::HitStatusView", "Views a hit result's threshold and status words (upstream merged it into Battle::HitResult).", None),
    ("T00459", "Battle::FloatingLabelSpec", "Gives the layout parameters of a floating battle label.", None),
    ("T00470", "Actors::JumpContactView", "Views a jump's contact word; not read.", None),
    ("T00471", "Actors::JumpFlagsView", "Views a jump's flag word; not read.", None),
    ("T00481", "Battle::TargetVolumeMatrix", "Holds the transform used to test points against a target volume.", None),
    ("T00482", "Battle::TargetVolumeFlags", "Holds a target volume's shape flags.", None),
    ("T00483", "Battle::TargetVolume", "Describes a target volume: shape, kind, facing, extents and origin.", None),
    ("T00484", "Battle::TargetLabelLine", "Holds one line of a target label.", None),
    ("T00807", "Battle::VectorWords", "Views a vector as words; not read.", UNION),
    ("T00485", "Battle::TargetSphereLabelView", "Views Battle::TargetSphere with its label text buffer.", None),
    ("T00486", "Battle::TargetSphereLabelEntry", "Holds one target label entry: actor, limb, values and limb name.", None),
    ("T00487", "Battle::TargetInfoLine", "Holds one target information line.", None),
    ("T00491", "Battle::EffectScriptWord", "Holds an effect script word; not read.", None),
    ("T00492", "Battle::EffectTransformDefinition", "Gives an effect transform definition; not read through this view.", None),
    ("T00496", "Battle::EffectRendererState", "Holds an effect renderer's state; not read.", None),
    ("T00808", "Effects::ParticleColorWord", "Holds a particle's color curves as a word or fields.", UNION),
    ("T00508", "Battle::EffectTransformBlock", "Holds an effect transform block; not read.", None),
    ("T00516", "Battle::Ai::ActionTiming", "Holds the timing and target of an AI action being executed.", None),
    ("T00518", "Battle::Ai::CandidateTable", "Holds the AI candidate table; not read.", None),
    ("T00519", "Battle::Ai::CandidateFlags", "Holds the flag bytes of an AI action candidate.", None),
    ("T00521", "Battle::Ai::EngagementActor", "Views a battle actor while the engagement list is managed.", None),
    ("T00522", "Battle::Ai::EngagementData", "Holds an actor's engagement data.", None),
    ("T00523", "Battle::Ai::ReactionRecord", "Holds a reaction record handed to the AI.", None),
    ("T00525", "Battle::Ai::RouteGateTile", "Holds a route gate tile: X, Z and gate.", None),
    ("T00527", "Battle::Ai::AiModelView", "Views a model record as the AI uses it (upstream layout).", None),
    ("T00528", "Battle::Ai::ActionRangeShape", "Gives an action's range extents, shape and angle.", None),
    ("T00529", "Battle::Ai::ActionRangeView", "Views a record holding an action's range.", None),
    ("T00530", "Battle::Ai::ActionRangeView2", "Views a second record holding an action's range.", None),
    ("T00531", "Battle::Ai::SavedSettingsPair", "Holds saved AI settings words.", None),
    ("T00533", "Battle::Ai::WordPair", "Holds two words; not read.", None),
    ("T00541", "Battle::EffectParameterBytes", "Holds effect parameter bytes; not read.", None),
    ("T00544", "Core::IntPair", "Holds two integers; not read.", None),
    ("T00545", "Battle::TargetLabelPrimitive", "Holds the primitive tag and box behind a target label.", None),
    ("T00546", "Battle::LimbEffects", "Holds two limb effect values.", None),
    ("T00551", "Battle::ShapeVertex", "Holds one wireframe vertex and its flags.", None),
    ("T00552", "Battle::Ai::ActorPositionView", "Views an actor's X and Z position.", None),
    ("T00553", "Battle::Ai::CheckObjectView", "Views an object's flags while a direction is checked.", None),
    ("T00809", "Battle::Ai::DetourWord", "Holds the obstacle detour state as a word or bits.", UNION),
    ("T00558", "Battle::Ai::RecoveryModelView", "Views a model record's flags and angles during movement recovery.", None),
    ("T00560", "Battle::Ai::StepActorView", "Views an actor record's height and tile during step movement.", None),
    ("T00810", "Battle::Ai::StepPositionWord", "Holds a step position as a word or X/Y halves.", UNION),
    ("T00563", "Battle::Ai::WaypointTile", "Holds a waypoint tile: X, Z and flags.", None),
    ("T00567", "Battle::Ai::ObstructionCache", "Caches an obstruction test result.", None),
    ("T00568", "Battle::Ai::ObstructionModel", "Views a model record's position for obstruction tests.", None),
    ("T00570", "Battle::Ai::ObstructionVector", "Holds an obstruction test vector.", None),
]
for _tid, _name, _purpose, _ov in OTHER:
    loc = ("Native\\Engine\\src\\Audio\\SoundBanks.h" if _name.startswith("Audio::") else
           "Native\\Game\\src\\Actors\\Spawn.h" if _name.startswith("Actors::") else
           "Native\\Engine\\include\\Core\\Core\\IntPair.h" if _name.startswith("Core::") else
           "Native\\Game\\src\\Effects\\Particles.h" if _name.startswith("Effects::") else
           AI + "AiViews.h" if _name.startswith("Battle::Ai::") else B + "BattleTypes.h")
    T(_tid, "State", _name, loc, _purpose, L if "not read" in _purpose else M,
      "declaration-only;" + g.decl(_tid, g.members[_tid][0]["name"]), g.generic_specs(_tid, _ov), recv=ANYR)
g.TYPES.append(["T00744", "Value", "Battle::Ai::PathTest", AI + "Navigation.h",
                "Is the function type that tests whether a detour may step in a direction.", "med",
                c(("771A8.c", "} movementPathTest;")) if any("} movementPathTest;" in source_line for source_line in g.trees[""].lines(g.trees[""].rel("771A8.c")))
                else "declaration-only;" + c(("771A8.c", "movementPathTest"))])

if __name__ == "__main__":
    g.write(partial=True)
