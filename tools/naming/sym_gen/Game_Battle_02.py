"""Game_Battle_02 types and fields from code read directly. Citations are resolved by exact text search (common.Gen).

Group A: fork views of the AI actor state (upstream func_800E78F4_t), named by offset through ai_state.TABLE.
Group B: AI entry, record and helper types.
Group C: hit calculation and MPD geometry types.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ai_state  # noqa: E402
from common import Gen  # noqa: E402

g = Gen("Game_Battle_02")
c, T = g.c, g.T
L, M, H = "low", "med", "high"
B = "Native\\Game\\src\\Battle\\"
AI = B + "Ai\\"
ANY = r"\b(?:state|arg0|arg1|s|other|firstOther|allyState|self|context|target)|D_800F5878\[[^\]]*\]\)?"
VIEW = ";view of the AI actor state T00532 (upstream func_800E78F4_t); fold into it when copying"


def V(tid, name, purpose, conf, cite, overrides=None):
    T(tid, "State", name, AI + "AiState.h", purpose, conf, cite + VIEW, ai_state.view_specs(g, tid, overrides), recv=ANY)


# ---------------------------------------------------------------- group A: AI actor state views
V("T00351", "Battle::Ai::CandidateReadinessView", "Views the AI actor state while candidate readiness is refreshed.", M,
  c(("6FC00.c", "for (i = 0; i < state->count; i++) {")))
V("T00356", "Battle::Ai::ActionChoiceView", "Views the first 752 bytes of the AI actor state while an action is chosen.", L,
  "declaration-only;" + g.decl("T00356", g.members["T00356"][0]["name"]))
V("T00362", "Battle::Ai::DecisionDispatchView", "Views the AI actor state while its decisions are dispatched.", M,
  c(("71618.c", "state->flags0 = 0;")))
V("T00368", "Battle::Ai::ActionMetadataView", "Views the AI actor state while action metadata is built.", M,
  c(("7CD68.c", "entry = &state->entries[row][column];")))
V("T00370", "Battle::Ai::ActionPlanView", "Views the AI actor state while an action plan is built.", M,
  c(("74088.c", "entry = &state->candidates[index];")))
V("T00373", "Battle::Ai::ReactionView", "Views the AI actor state while it reacts to another actor's action.", M,
  c(("74088.c", "if (source == 0 && state->flag13 == 0 && state->flag27) {")))
V("T00381", "Battle::Ai::PlanEvaluationView", "Views the AI actor state while action plans are rebuilt.", M,
  c(("74088.c", "state->fE7 = 0;")))
V("T00385", "Battle::Ai::AiCreationView", "Views the AI actor state while it is created and initialized.", M,
  c(("7CD68.c", "state->stepHeight = 32;")))
V("T00389", "Battle::Ai::PerceptionView", "Views the AI actor state while it perceives other actors.", M,
  c(("6FC00.c", "relation.bits.active = 1;")))
V("T00392", "Battle::Ai::SettingsView", "Views the AI actor state while the AI settings word is decoded.", M,
  c(("7CD68.c", "state->f20 = (settings->raw >> 26) & 1;")))
V("T00394", "Battle::Ai::EngagementView", "Views the AI actor state while the engagement list is animated.", M,
  c(("73700.c", "state = state->next;")))
V("T00399", "Battle::Ai::PerceptionUpdateView", "Views the AI actor state during its per-frame perception update.", M,
  c(("6FC00.c", "state->historyX.b[state->historyIndex] = state->unk4C;")))
V("T00406", "Battle::Ai::CandidateMovementView", "Views the AI actor state while it moves for an action candidate.", M,
  c(("71618.c", "switch (state->mode) {")))

# ---------------------------------------------------------------- group B: AI entry, record and helper types
ANYR = r"[\w\]\)]+"
UNION = {"raw": ("word", "Whole word", M), "bytes": ("bytes", "Bytes of the word", M), "bits": ("bits", "Bit fields", M),
         "half": ("halves", "Halves of the word", M), "byte": ("bytes", "Bytes of the word", M),
         "b": ("bytes", "Bytes of the word", M), "p": ("tile", "Tile fields", M)}
GROUP_B = [
    ("T00350", "Battle::Ai::CandidateEntryView", "Views an action candidate entry (upstream func_800DCAA0_t).", None),
    ("T00352", "Battle::Ai::RelationByteView", "Views a relation word as bytes; byte 0 bits 6-7 are the classification.", None),
    ("T00353", "Battle::Ai::ActionChoiceEntryView", "Views an action candidate entry while a choice is recorded; not read.", None),
    ("T00354", "Battle::Ai::ActionChoiceFlagsView", "Views the flag word of an action choice; not read.", None),
    ("T00355", "Battle::Ai::ActionChoiceInfoView", "Views an action definition while a choice is scored; not read.", None),
    ("T00357", "Battle::Ai::ActionCostView", "Views an action definition's flag word for its cost; not read.", None),
    ("T00358", "Battle::Ai::DispatchActorView", "Views a battle actor (list link, id, model) while decisions are dispatched.", None),
    ("T00359", "Battle::Ai::DispatchEntryView", "Views an action candidate entry while decisions are dispatched; not read.", None),
    ("T00360", "Battle::Ai::DispatchModelView", "Views a model record's flags and position while decisions are dispatched.", None),
    ("T00361", "Battle::Ai::DispatchRegistryView", "Views the room AI state's special-actor list; not read.", None),
    ("T00365", "Battle::Ai::MetadataActionView", "Views an action definition's flag words while action metadata is built.", None),
    ("T00366", "Battle::Ai::ActionMetadataEntry", "Holds an action slot's metadata: kind, range class and squared range.", None),
    ("T00367", "Battle::Ai::MetadataSlotView", "Views an action slot of the actor statistics.", None),
    ("T00369", "Battle::Ai::MetadataStatsView", "Views the action slots of the actor statistics.", None),
    ("T00797", "Battle::Ai::MetadataWord", "Holds an action definition flag word as a word or bytes.", UNION),
    ("T00371", "Battle::Ai::ReactionActionView", "Views an action definition's two hit words while reacting to it.", None),
    ("T00372", "Battle::Ai::ReactionOrbitView", "Views the arena orbit state's orbiting flag while reacting.",
     {"flag": ("isOrbiting", "Cleared when the actor reacts (same byte as Battle::Ai::ArenaOrbitState.isOrbiting)", M)}),
    ("T00798", "Battle::Ai::RelationWordView", "Views a relation word as a word, its reaction bit split, or bytes.",
     {**UNION, "p": ("reactionBits", "Bit split used by the reaction code", M)}),
    ("T00374", "Battle::Ai::ActorListNodeView", "Views a battle actor's list link and id.", None),
    ("T00375", "Battle::Ai::ActionSetupView", "Views the AI actor state while an action is set up; not read.", None),
    ("T00376", "Battle::Ai::ActionSlotConfigView", "Views an action slot's configuration; not read.", None),
    ("T00377", "Battle::Ai::StatApplicationView", "Views the actions and their stat changes applied to an actor.", None),
    ("T00378", "Battle::Ai::StatDeltaView", "Views a hit result while the AI reacts to its stat changes.", None),
    ("T00379", "Battle::Ai::EvaluationActorView", "Views a battle actor while action plans are rebuilt.", None),
    ("T00380", "Battle::Ai::EvaluationContextView", "Views a model record while action plans are rebuilt.", None),
    ("T00382", "Battle::Ai::EvaluationStatsView", "Views the actor statistics while action plans are rebuilt.", None),
    ("T00384", "Battle::Ai::CreationModelView", "Views a model record while the AI state is created.", None),
    ("T00386", "Battle::Ai::CreationStatsView", "Views the AI setting bytes of the statistics while the AI state is created.", None),
    ("T00387", "Battle::Ai::PerceivedObjectView", "Views a model record while it is perceived.", None),
    ("T00388", "Battle::Ai::EngagementActorView", "Views a battle actor's phase field while the engagement list is animated.", None),
    ("T00390", "Battle::Ai::SettingsActorView", "Views a battle actor's kind and setting words while AI settings are decoded.", None),
    ("T00391", "Battle::Ai::WanderTiming", "Gives the move period, rest period and rate of one wander setting.",
     {"first": ("movePeriod", "Wander move period", M), "second": ("restPeriod", "Wander rest period", M),
      "third": ("rate", "Wander chance exponent", M)}),
    ("T00396", "Battle::Ai::PerceptionActorView", "Views a battle actor during the perception update.", None),
    ("T00397", "Battle::Ai::HpPairView", "Views current and maximum HP.", None),
    ("T00398", "Battle::Ai::PositionLinkView", "Views a position word and its follower during the perception update.", None),
    ("T00400", "Battle::Ai::PerceptionStatsView", "Views the actor statistics' HP during the perception update.", None),
    ("T00405", "Render::ModelScratchpadCameraView", "Views the model scratchpad's matrix and scale; not read.", None),
    ("T00408", "Battle::Ai::EscortActorView", "Views a battle actor's list link and id for escort behavior.", None),
    ("T00409", "Battle::Ai::EscortEntryView", "Views an action candidate entry for escort behavior.", None),
    ("T00410", "Battle::Ai::EscortTargetView", "Views the target byte of an action candidate entry.", None),
    ("T00411", "Battle::Ai::GuardModelView", "Views a model record's position and tile for guard behavior.", None),
    ("T00799", "Battle::Ai::ActorStateFlags", "Holds an actor's state flag word as a word, bytes or bits.", UNION),
    ("T00800", "Battle::Ai::CreationTileWord", "Holds a model tile word as a word or tile bytes.", UNION),
    ("T00801", "Battle::Ai::TargetRelation", "Holds what an AI actor perceives about one other actor, as a word or bits.",
     {**UNION, "raw": ("word", "Whole relation word", M, c(("6FC00.c", "state->relations[id].raw |= 0x40000004;")))}),
    ("T00802", "Battle::Ai::SettingsWord", "Holds the AI settings word as a word, halves or bytes.", UNION),
    ("T00803", "Battle::Ai::CandidateTileWord", "Holds a tile word as a word or bytes.", UNION),
    ("T00804", "Battle::Ai::GuardTileWord", "Holds a tile word as a word or bytes.", UNION),
]
for tid, name, purpose, ov in GROUP_B:
    specs = g.generic_specs(tid, ov)
    T(tid, "State", name, ("Native\\Engine\\src\\Render\\ModelDraw.h" if name.startswith("Render::") else AI + "AiViews.h"),
      purpose, L if "not read" in purpose else M,
      "declaration-only;" + g.decl(tid, g.members[tid][0]["name"]) + ";fork-only view; fold into its canonical type",
      specs, recv=ANYR)
twin_t, twin_f = g.applied("T00200")
g.TYPES.append(["T00401", twin_t["role"], twin_t["final_name"], twin_t["final_location"], twin_t["purpose"],
                twin_t["confidence"], "same-as:T00200;identical scratch layout declared in another translation unit;"
                + twin_t["notes"]])
for _m, _f in zip(g.members["T00401"], twin_f, strict=True):
    g.FIELDS.append(
        [_m["id"], _f["final_name"], _f["meaning"], _f["confidence"], _f["notes"]]
    )


# ---------------------------------------------------------------- group C: hit calculation and MPD geometry
T("T00312", "State", "Battle::DamageCalculation", B + "Hit.h",
  "Holds the inputs of one damage or healing calculation: types, affinities, classes, stats and their factors.", "high",
  c(("146C.c", "sp18.mediatingStat = targetActor->strength;")),
  g.generic_specs("T00312", {"value": ("attackStat", "Attacker's base stat: strength for physical hits, intelligence for spells", H,
                                       c(("146C.c", "sp18.value = sourceActor->strength;"), ("146C.c", "sp18.value = sourceActor->intelligence;")))}),
  recv=r"sp18|arg\d")
FD = "Native\\Game\\src\\Field\\"
MPD = ";DataCrystal MPD files (GeometrySection)"


def poly_name(n: str) -> tuple[str, str]:
    import re as _re
    m = _re.fullmatch(r"p(\d)([xyz])", n)
    if m:
        k, ax = m.groups()
        return (f"vertex{k}{ax.upper()}" if k == "1" else f"vertex{k}Offset{ax.upper()}",
                f"Vertex {k} {ax.upper()}" + ("" if k == "1" else " relative to vertex 1"))
    m = _re.fullmatch(r"([rgb])(\d)", n)
    if m:
        col = {"r": "red", "g": "green", "b": "blue"}[m.group(1)]
        return f"{col}{m.group(2)}", f"Vertex {m.group(2)} {col}"
    if _re.fullmatch(r"[uv]\d", n):
        return n, f"Vertex {n[1]} texture {n[0].upper()}"
    return {"type": ("polygonType", "Polygon type"), "texelMask": ("texelMask", "Texel transparency mask"),
            "idTexture": ("textureId", "Texture id; changes at run time")}[n]


for _tid, _name, _what in (("T00324", "Field::MpdTriangle", "Gives one textured, Gouraud-shaded MPD triangle."),
                           ("T00325", "Field::MpdQuad", "Gives one textured, Gouraud-shaded MPD quad.")):
    T(_tid, "OnDisc", _name, FD + "Mpd.h", _what, "high", "declaration-only;" + g.decl(_tid, "p1x") + MPD,
      [(m["name"], *poly_name(m["name"]), M) for m in g.members[_tid]])
T("T00322", "OnDisc", "Field::MpdGeometrySection", FD + "Mpd.h",
  "Starts the MPD geometry section: the polygon group count and the group headers.", "high",
  "declaration-only;" + g.decl("T00322", "numGroups") + MPD, [
      ("numGroups", "groupCount", "Number of polygon groups", M),
      ("groups", "groupHeaders", "Polygon group headers", M),
  ])
T("T00321", "OnDisc", "Field::MpdPolygonGroupHeader", FD + "Mpd.h",
  "Is the 64-byte header of one MPD polygon group; its contents are not read by decompiled code.", "low",
  "declaration-only;" + g.decl("T00321", "unk0") + MPD + " documents the 0x40-byte size only",
  g.generic_specs("T00321"))

if __name__ == "__main__":
    g.write()
