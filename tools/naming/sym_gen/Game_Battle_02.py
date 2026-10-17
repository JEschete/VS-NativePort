"""Game_Battle_02 types and fields from code read directly. Citations are resolved by exact text search (common.Gen).

Group A: fork views of the AI actor state (upstream func_800E78F4_t), named by offset through ai_state.TABLE.
Group B: AI entry, record and helper types.
Group C: hit calculation and MPD geometry types.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen  # noqa: E402
import ai_state  # noqa: E402

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

if __name__ == "__main__":
    g.write(partial=True)
