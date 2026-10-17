"""Game_Battle_04 globals, types and fields from code read directly. Citations are resolved by exact text search
(common.Gen).

Globals: each name comes from the use sites printed by files/gu.py and read in their functions. Inventory sizes are
address gaps, not capacities. The three status-string globals are entries of the status string table addressed
directly; their entries were identified by u16 offset in BATTLE.PRG and assets/BATTLE/BATTLE.PRG/statusStrings.yaml.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen  # noqa: E402
import ai_state  # noqa: E402

g = Gen("Game_Battle_04")
c, T, G = g.c, g.T, g.G
L, M, H = "low", "med", "high"
B = "Native\\Game\\src\\Battle\\"
AI = B + "Ai\\"
GAP = ";inventory size is an address gap, not a capacity"

# ---------------------------------------------------------------- globals: AI
G("G00013", "constexpr", "Battle::Ai::kTimedStatusFlags", AI + "ActionPlanning.c",
  "Gives the five status flags the planning simulation clears once their status timers run out.", M,
  c(("74088.c", "if (statuses & D_80069BBC[i]) {"), ("74088.c", "statuses &= ~D_80069BBC[i];")) + ";five entries by loop bound" + GAP)
G("G00014", "constexpr", "Battle::Ai::kSettingsBits30Values", AI + "ActorState.c",
  "Gives the AI state value selected by bits 30-31 of the AI settings word (settingsBits30Value).", L,
  c(("7CD68.c", "state->f32 = D_80069C18[state->f31];")) + ";value meaning not established" + GAP)
G("G00015", "constexpr", "Battle::Ai::kPerceptionRangesSquared", AI + "ActorState.c",
  "Gives the squared perception range selected by bits 18-20 of the AI settings word.", H,
  c(("7CD68.c", "rangeSquared = D_80069C1C[(settings->half[1] & 28) >> 2];"), ("7CD68.c", "state->rangeSquared = rangeSquared;"))
  + ";stored at AI state offset 0xAC (farDistanceSquared)" + GAP)
G("G00016", "constexpr", "Battle::Ai::kWanderTimingPresets", AI + "ActorState.c",
  "Gives the wander move period, rest period and chance exponent selected by bits 4-6 of the AI settings word.", H,
  c(("7CD68.c", "state->fD6 = D_80069C3C[(settings->raw >> 4) & 7].first;"),
    ("7CD68.c", "state->fD8 = D_80069C3C[(settings->raw >> 4) & 7].third;")) + ";element type Battle::Ai::WanderTiming" + GAP)

# ---------------------------------------------------------------- globals: unreferenced
G("G00017", "drop", "Audio::legacyUnreferencedTable", "Native\\Audio\\src\\Legacy.c",
  "Reserves an unreferenced table declared beside the alternate sample bank remapping.", H,
  "replaced-by:nothing;no function references in either tree;" + c(("25AC.c", "extern int D_80094FAC[];")))
G("G00018", "drop", "Audio::legacyUnreferencedWord", "Native\\Audio\\src\\Legacy.c",
  "Reserves an unreferenced word declared beside the alternate sample bank remapping.", H,
  "replaced-by:nothing;no function references in either tree;" + c(("25AC.c", "extern int D_80094FFC;")))

# ---------------------------------------------------------------- globals: targeting by action category
TGT = B + "Targeting.c"
G("G00019", "catalog", "Battle::categoryNeedsTargetSelection", TGT,
  "Marks action categories whose targets the player picks (direct or ground-area targeting); others target the user's "
  "area or the environment.", M,
  c(("146C.c", "if (D_800E8184[category]) {"), ("146C.c", "if (D_800E8184[vs_main_actions[D_800F19CC->unk8.actionIndex].unk2_4]"))
  + ";indexed by the action's category" + GAP)
G("G00020", "catalog", "Battle::categoryIsAreaTarget", TGT,
  "Marks action categories that hit an area: around a picked ground point, or around the user when no target is picked.", M,
  c(("146C.c", "if (D_800E8194[category]) {"), ("146C.c", "} else if (D_800E8194[category]) {"))
  + ";with categoryNeedsTargetSelection selects CollectGroundAreaActionTargets, CollectDirectActionTargets, "
  "CollectSourceAreaActionTargets or CommitEnvironmentAction" + GAP)
G("G00021", "catalog", "Battle::categoryIncludesActingActor", TGT,
  "Marks action categories whose target lists may include the acting actor.", M,
  c(("146C.c", "|| D_800E81B4[vs_main_actions[action].unk2_4])")) + ";skips the actor at unk8.unk4.unk0 unless set" + GAP)
G("G00022", "catalog", "Battle::categoryTargetsBodyParts", TGT,
  "Marks action categories that target individual body parts instead of whole actors.", H,
  c(("146C.c", "if (D_800E81C4[vs_main_actions[action].unk2_4]) {"), ("146C.c", "if (!D_800E81C4[category]) {"))
  + ";when set each actor's limbs are listed" + GAP)
G("G00023", "catalog", "Battle::categoryTargetActorMasks", TGT,
  "Gives the actor kind bits an action category can target; bit 7 adds the room's target points.", M,
  c(("146C.c", "&& (vs_battle_actors[i]->unk1C & D_800E81D4[vs_main_actions[action].unk2_4])"),
    ("146C.c", "if (D_800E81D4[vs_main_actions[action].unk2_4] & 0x80) {")) + GAP)

# ---------------------------------------------------------------- globals: action resolution and status effects
G("G00025", "catalog", "Battle::limbInjuryStatusIds", B + "ActionResolution.c",
  "Gives the status effect each limb's injury applies or clears when its HP crosses 1.", M,
  c(("146C.c", "if (!(temp_s3->statuses & vs_main_statusEffectParams[D_800E8200[i]].unk0)) {"),
    ("146C.c", "_setInflictedStatuses(D_800E8200[i], arg0);")) + GAP)
G("G00026", "catalog", "Battle::limbStatusFlags", B + "StatusEffects.c",
  "Gives the status flag for each of the first four limbs; a set flag is kept only while that limb has at least 2 HP.", L,
  c(("146C.c", "if (actor->statuses & D_800E8204[i]) {"), ("146C.c", "statuses |= D_800E8204[i];")) + ";four entries by loop bound" + GAP)
G("G00028", "catalog", "Battle::hitFunctions", B + "HitChance.c",
  "Dispatches the hit-chance calculation selected by an action hit's hitFunction.", H,
  c(("146C.c", "threshold = _hitFunctions[action->hitParams[hitNumber].hitFunction](")) + ";fork name kept" + GAP)
G("G00029", "catalog", "Battle::attackPrerequisiteFunctions", B + "AttackPrerequisites.c",
  "Dispatches the prerequisite check selected by an action hit before it can be performed.", H,
  c(("146C.c", "return _attackPrerequisiteFunctions[action->hitParams[hitNumber]")) + ";fork name kept" + GAP)
G("G00030", "catalog", "Battle::bladePpDamageBonuses", B + "Damage.c",
  "Gives the damage bonus a blade category gains at full PP, scaled by current PP over maximum PP.", H,
  c(("146C.c", "* *((sourceActor->weapon.blade.category) + D_800E82F4))")) + ";indexed by blade category" + GAP)
G("G00031", "catalog", "Battle::statCalculators", B + "Damage.c",
  "Dispatches the stat-change calculation selected by an action hit's statCalculator.", H,
  c(("146C.c", "amount = _statCalculators[action->hitParams[hit].statCalculator](")) + ";fork name kept" + GAP)
G("G00032", "catalog", "Battle::actionEffectMediators", B + "ActionResolution.c",
  "Dispatches the effect handler selected by an action hit's effect.", H,
  c(("146C.c", "_actionEffectMediators[action->hitParams[hit].effect](")) + ";fork name kept" + GAP)
G("G00033", "state", "Battle::vitalsTickCounter", B + "StatusEffects.c",
  "Counts game ticks modulo 720; HP, MP, PP and Risk recovery happen on multiples of 30 to 240.", H,
  c(("146C.c", "D_800E8490 = (D_800E8490 + vs_gametime_tickspeed) % 720;"), ("146C.c", "if ((D_800E8490 % 30 == 0) && (temp_s0->currentHP != 0)) {")))
G("G00034", "state", "Battle::regeneratingLimb", B + "StatusEffects.c",
  "Selects the limb whose HP recovers; advances through the six limbs every 240 ticks.", H,
  c(("146C.c", "D_800E8494 = (D_800E8494 + 1) % 6;"), ("146C.c", "++temp_s0->limbs[D_800E8494].hp;")))
G("G00035", "state", "Battle::targetingRiskTimer", B + "StatusEffects.c",
  "Counts ticks modulo 30 while targeting; the player's Risk rises when it wraps to zero.", H,
  c(("146C.c", "D_800E8498 = (D_800E8498 + vs_gametime_tickspeed) % 30;"), ("146C.c", "void func_80088CA0(void) { D_800E8498 = 0; }")))

# ---------------------------------------------------------------- globals: feedback labels and animation
FB = B + "Feedback.c"
G("G00046", "catalog", "Battle::valueCaptionRects", FB,
  "Gives the caption texture rectangle index for each floating value label kind from 3 up.", M,
  c(("2842C.c", "RECT const* rect = &D_80068FAC[D_800E861C[p->unkE - 3]];")) + ";indexes D_80068FAC" + GAP)
G("G00047", "catalog", "Battle::lockedLabelHalfHeights", FB,
  "Gives the locked label's half height for each of its five open and close steps.", H,
  c(("2842C.c", "y0 = arg1 - D_800E8634[step];"), ("2842C.c", "y1 = arg1 + D_800E8634[step];")) + GAP)
G("G00048", "catalog", "Battle::lockedLabelPieces", FB,
  "Gives the texture rectangles of the four pieces drawn side by side as the locked label.", H,
  c(("2842C.c", "if ((arg0 < 320) && ((arg0 + D_800E8640[i].w) >= 0)) {"), ("2842C.c", "arg0 += D_800E8640[i].w;")) + GAP)
G("G00058", "catalog", "Battle::chainAbilityAnimationGroups", B + "ActionAnimation.c",
  "Gives each Chain Ability's animation group; the animation is 0xCA plus six per group, varied by pitch and side.", H,
  c(("30DB0.c", "anim = D_800E8E80[arg2] * 6 + 0xCA;")) + GAP)
ANIM = "Native\\Game\\src\\Actors\\Animation.c"
G("G00107", "catalog", "Actors::animationEventLengths", ANIM,
  "Gives the byte length of each animation event opcode, used to step to the next event.", H,
  c(("44F14.c", "script += D_800E9298[op];")) + ";read by func_800AD714 (44F14.c), which the function catalog lacks" + GAP)
G("G00108", "catalog", "Actors::drawnWeaponSoundIds", ANIM,
  "Gives the sound event 4 plays for argument 0x58, chosen by the actor's byte 0x5B1 when its weapon is drawn; entry 0 "
  "otherwise.", L,
  c(("44F14.c", "arg = D_800E92DC[0];"), ("44F14.c", "arg = D_800E92DC[actor->unk5B1];")) + ";passed to Audio::PlayActorSoundPair" + GAP)
G("G00114", "catalog", "Actors::animationEventPoints", ANIM,
  "Gives the x, z and heading an animation event 60 moves the actor's motion toward.", M,
  c(("44F14.c", "point = D_800E9A28[0];"), ("44F14.c", "delta = point[2];")) + GAP)

# ---------------------------------------------------------------- globals: sphere
SPH = B + "Sphere.c"
G("G00134", "catalog", "Battle::sphereArcBases", SPH,
  "Gives the three basis components and radius of each arc a wireframe command 0 appends.", M,
  c(("58578.c", "src = &D_800EA234[(command << 2) & 0x7C];")) + ";four shorts per entry" + GAP)
G("G00135", "state", "Battle::sphereMirroredVertices", SPH,
  "Gives vertex runs a wireframe command 2 emits twice, the second time mirrored in x and z.", M,
  c(("58578.c", "src = &D_800EA2C4[command & 0x3FF];"), ("58578.c", "src[0] = -vertices[index].xyz[0];"))
  + ";negated in place and restored by the second pass, so it is writable" + GAP)
G("G00136", "catalog", "Battle::sphereShapeCommandStarts", SPH,
  "Gives each wireframe shape's first command index; the next entry ends it.", H,
  c(("58578.c", "command = D_800EA438[shape];"), ("58578.c", "for (count = D_800EA438[shape + 1] - command; count > 0; --count) {")) + GAP)
G("G00137", "catalog", "Battle::sphereWireframeCommands", SPH,
  "Gives the packed wireframe commands (arc, ring or mirrored vertex run) of every shape.", H,
  c(("58578.c", "commands = &D_800EA46C[command];")) + ";kind in bits 14-15" + GAP)
G("G00139", "catalog", "Battle::positionCursorSpriteOffsets", SPH,
  "Gives the screen offset of each position cursor sprite.", H,
  c(("58578.c", "xy + D_800EA670[index], D_800EA684[index], ot);")) + GAP)
G("G00140", "catalog", "Battle::positionCursorSpriteSizes", SPH,
  "Gives the packed width and height of each position cursor sprite.", H,
  c(("58578.c", "xy + D_800EA670[index], D_800EA684[index], ot);")) + ";wh argument of vs_battle_setSpriteDefaultTexPage" + GAP)
G("G00141", "catalog", "Battle::positionCursorSpriteTexCoords", SPH,
  "Gives the packed texture coordinates and CLUT of each position cursor sprite.", H,
  c(("58578.c", "prim[4] = D_800EA698[index];")) + GAP)
G("G00142", "state", "Battle::positionCursorBlinkFrame", SPH,
  "Steps through the 16-frame brightness animation of the position cursor's third sprite.", H,
  c(("58578.c", "D_800EA6AC = (D_800EA6AC + 1) & 0xF;"), ("58578.c", "index == 2 ? vs_battle_cursorBrightnessAnimation[D_800EA6AC] : 0x80,")))
G("G00160", "state", "Battle::targetSphere", SPH,
  "Points to the targeting Sphere: its volumes, wireframe, target list and label rows; allocated on first use.", H,
  c(("573B8.c", "D_800EB9B8 = vs_main_allocHeap(sizeof *D_800EB9B8);"), ("573B8.c", "if (D_800EB9B8 != NULL) {"))
  + ";points to Battle::TargetSphere")

# ---------------------------------------------------------------- globals: target selection menu
G("G00143", "asset", "Battle::statusStrings", TGT,
  "Holds the battle status and target-description strings: a u16 offset table followed by the encoded strings.", H,
  "asset:text/battle/status-strings;" + c(("5BF94.c", "[vs_battle_statusStrings[VS_statusStrings_INDEX_strDown + i]];"))
  + ";entries listed in assets/BATTLE/BATTLE.PRG/statusStrings.yaml" + GAP)
G("G00144", "drop", "Battle::targetInfoTemplate", TGT,
  "Is the statusStrings entry targetTemplate, the format string printed into the target description.", H,
  "replaced-by:Battle::statusStrings entry targetTemplate;" + c(("5BF94.c", "vs_battle_printf(D_800EB9B8->textBuffer, D_800EA984);"))
  + ";address is u16 offset 142 of the table, the offset of entry 20 (targetTemplate) in BATTLE.PRG")
G("G00145", "drop", "Battle::targetStatusLineTemplate", TGT,
  "Is the statusStrings entry line23, shown with the first status of a target that has no special line.", M,
  "replaced-by:Battle::statusStrings entry line23;" + c(("5BF94.c", "vs_battle_stringContext.strings[1] = D_800EA9B6;"))
  + ";address is u16 offset 167 of the table, the offset of entry 23 (line23) in BATTLE.PRG")
G("G00146", "drop", "Battle::emptyStatusString", TGT,
  "Is the statusStrings entry empty, used to blank unused target description arguments.", H,
  "replaced-by:Battle::statusStrings entry empty;" + c(("5BF94.c", "vs_battle_stringContext.strings[value] = D_800EAA54;"))
  + ";address is u16 offset 246 of the table, the offset of entry 40 (empty); its first byte is the terminator 0xE7")
G("G00147", "state", "Battle::targetSelectionStep", TGT,
  "Steps the target selection menu: rows 0-15 are opened one per frame, then 16-21 handle input and closing.", H,
  c(("5BF94.c", "D_800EABAA = 0;"), ("5BF94.c", "switch (D_800EABAA) {"))
  + ";written each frame although the inventory places it in rodata, right after the status string table")
G("G00154", "state", "Battle::reuseSphereVolumes", SPH,
  "Set when target selection closes on a type-2 primary volume; the next SetSphereVolumes keeps the current volume "
  "transition and only replaces the secondary volume.", M,
  c(("573B8.c", "if (D_800EB9AC != 0) {"), ("5BF94.c", "D_800EB9AC = D_800EB9B8->unk0[0].unk1 == 2;")))

# ---------------------------------------------------------------- globals: HUD
HUD = B + "Hud.c"
G("G00157", "state", "Battle::hudHidden", HUD,
  "Slides the battle HUD offscreen while set.", H,
  c(("5BF94.c", "void func_800CB654(int arg0) { D_800EB9AF = arg0; }"), ("5BF94.c", "if (D_800EB9AF != 0) {"))
  + ";saved by the loot menu (MENUB 260.c)")
G("G00165", "state", "Battle::hudBattleMode", HUD,
  "Selects the battle-mode limb display and slides in the weapon-state indicator when 1.", H,
  c(("5BF94.c", "void func_800CB660(int arg0) { D_800EB9CC = arg0; }"), ("5BF94.c", "j = i + (D_800EB9CC & 1) * 5;")))
G("G00166", "state", "Battle::battleModeIndicatorStep", HUD,
  "Slides the weapon-state indicator in over four steps while in battle mode and back out otherwise.", H,
  c(("5BF94.c", "if ((u_char)D_800EB9CD < 4) {"), ("5BF94.c", "--D_800EB9CD;")))
G("G00168", "state", "Battle::statusIconCarousel", HUD,
  "Holds the previous, current and next status icon shown by the HUD and the scroll phase of their carousel.", M,
  c(("5BF94.c", "if (j == D_800EB9D0.u8[1]) {"), ("5BF94.c", "j = D_800EB9D0.u8[3];")) + ";0xFF marks an empty slot")
G("G00176", "catalog", "Battle::hudGaugePositions", HUD,
  "Gives the packed screen position of each HUD gauge (HP, MP, Risk and a fourth).", H,
  c(("5BF94.c", "int xy = D_800EBBDC[colorIndex] - D_800EB9B0;")) + GAP)
G("G00177", "catalog", "Battle::hudGaugeLabelPositions", HUD,
  "Gives the packed screen position of each HUD gauge's label sprite.", H,
  c(("5BF94.c", "vs_battle_setSpriteDefaultTexPage(0x180, D_800EBBEC[colorIndex] - D_800EB9B0,")) + ";also read by the status menu" + GAP)
G("G00178", "catalog", "Battle::hudGaugeLabelWidths", HUD,
  "Gives the width of each HUD gauge's label sprite; the height is 9.", H,
  c(("5BF94.c", "D_800EBBFC[colorIndex] | 0x90000, prim)[4] =")) + GAP)
G("G00179", "catalog", "Battle::hudGaugeLabelTexU", HUD,
  "Gives the texture u coordinate of each HUD gauge's label sprite.", M,
  c(("5BF94.c", "D_800EBC00[colorIndex] | 0x37F60000;")) + GAP)
G("G00181", "catalog", "Battle::statusIconTexOffsets", HUD,
  "Gives the texture offset of each status icon.", H,
  c(("5BF94.c", "var_s2 = vs_battle_statusIconTexOffsets[arg0];")) + ";also read by the status menu" + GAP)
G("G00182", "catalog", "Battle::limbStatusColors", HUD,
  "Gives the color for each limb condition; the status menu shifts it to subdue it.", H,
  c(("5BF94.c", "prim[2] = D_800EBC54[vs_battle_getLimbStatus(")) + GAP)
G("G00183", "catalog", "Battle::upperStatusIconFlags", HUD,
  "Gives the icon flags (shifted to bits 16 up) for each of the eight upper status bits.", M,
  c(("5BF94.c", "var_a2 |= D_800EBC68[i] << 16;")) + GAP)
G("G00184", "state", "Battle::shortcutHudRequested", HUD,
  "Requests the shortcut HUD's opening animation for the current frame; cleared after drawing.", H,
  c(("5BF94.c", "void func_800C9F88(void) { D_800EBC78 = 1; }"), ("5BF94.c", "D_800EBC78 = 0;")))
G("G00186", "catalog", "Battle::firstShortcutDigitOffsets", HUD,
  "Gives the packed offsets of the number sprites of shortcuts 0-3.", M,
  c(("5BF94.c", "vs_battle_setSpriteDefaultTexPage(0x80, temp_s3 + D_800EBC88[3 - i], 0x100010,")) + GAP)
G("G00187", "catalog", "Battle::secondShortcutMarkerPositions", HUD,
  "Gives the x then y positions of the button markers of shortcuts 4-7.", M,
  c(("5BF94.c", "((temp_s3 + D_800EBC98[i]) & 0xFFFF) | (D_800EBC98[i + 4] << 0x10),")) + GAP)
G("G00188", "catalog", "Battle::secondShortcutIconPositions", HUD,
  "Gives the x then y positions of the icons of shortcuts 4-7.", M,
  c(("5BF94.c", "((temp_s3 + D_800EBCA0[i]) & 0xFFFF) | (D_800EBCA0[i + 4] << 0x10),")) + GAP)


# ================================================================ types
STATE_RECV = r"\b(?:state|s|arg0|self|context)"
VIEW = ";view of the AI actor state T00532 (upstream func_800E78F4_t); fold into it when copying"
FOLD = ";fork-only view; fold into its canonical type when copying"

# ---------------------------------------------------------------- AI actor state views (offset table)
for _tid, _name, _purpose in (
        ("T00608", "Battle::Ai::BlockedRecoveryView", "Views the AI actor state while blocked movement is recovered by a detour."),
        ("T00740", "Battle::Ai::WanderView", "Views the AI actor state while it wanders."),
        ("T00593", "Battle::Ai::AreaRouteView", "Views the AI actor state while a route between navigation areas is resolved."),
        ("T00615", "Battle::Ai::TerrainMoveView", "Views the AI actor state while it moves through height-filtered directions."),
        ("T00613", "Battle::Ai::TargetApproachView", "Views the AI actor state while it approaches a target."),
        ("T00590", "Battle::Ai::RadialRetreatView", "Views the AI actor state while a boss retreats along the arena radius."),
        ("T00623", "Battle::Ai::OrbitTrackingView", "Views the AI actor state while a boss orbits the arena centre tracking the player.")):
    T(_tid, "State", _name, AI + "AiState.h", _purpose, M,
      "declaration-only;" + g.decl(_tid, g.members[_tid][0]["name"]) + VIEW, ai_state.view_specs(g, _tid), recv=STATE_RECV)

# ---------------------------------------------------------------- arena orbit views (Battle::Ai::ArenaOrbitState)
for _tid, _name, _purpose in (
        ("T00587", "Battle::Ai::RadialRetreatOrbitView", "Views the arena orbit state while a boss retreats along the arena radius."),
        ("T00621", "Battle::Ai::OrbitTrackingOrbitView", "Views the arena orbit state while a boss orbits the arena centre.")):
    T(_tid, "State", _name, AI + "AiViews.h", _purpose, M,
      "declaration-only;" + g.decl(_tid, "facing") + ";view of D_800F5920 (Battle::Ai::ArenaOrbitState, T00273); names by offset" + FOLD,
      g.generic_specs(_tid, g.names_from_views(_tid, ["T00273"])), recv=r"\bmotion")

# ---------------------------------------------------------------- other AI views
T("T00588", "State", "Battle::Ai::RetreatPlayerRecordView", AI + "AiViews.h",
  "Views the player's actor record flags while a boss retreat ends.", L,
  c(("7EE98.c", "if (((radialMotionActor*)D_800F4538[0])->flags & 0x70000) {")) + ";view of Actors::ActorRecord" + FOLD,
  g.generic_specs("T00588"), recv=r"\(\(radialMotionActor\*\)D_800F4538\[0\]\)")
T("T00589", "State", "Battle::Ai::ModelActiveView", AI + "AiViews.h",
  "Views the actor record's active bit (bit 4 of the word at 0x5B0) set while a boss moves along its path.", L,
  c(("7EE98.c", "context->active = motion->frame < 14;")) + ";view of Actors::ActorRecord" + FOLD,
  g.generic_specs("T00589"), recv=r"\b(?:context|state->context)")
T("T00622", "State", "Battle::Ai::OrbitPlayerRecordView", AI + "AiViews.h",
  "Views the player's actor record position while a boss tracks the player.", L,
  c(("7EE98.c", "playerAngle = ratan2(0x7C0 - ((trackingMotionActor*)D_800F4538[0])->x,")) + ";view of Actors::ActorRecord" + FOLD,
  g.generic_specs("T00622"), recv=r"\(\(trackingMotionActor\*\)D_800F4538\[0\]\)")
T("T00624", "State", "Battle::Ai::OrbitStatsView", AI + "AiViews.h",
  "Views the boss's HP in its actor statistics while orbit tracking picks a phase.", M,
  c(("7EE98.c", "ratio = (state->stats->current << 16) / state->stats->maximum;")) + ";view of Battle::ActorStats" + FOLD,
  [("pad", "reserved0", "Not read by decompiled code", L),
   ("current", "currentHp", "Current HP (same offset as Battle::ActorStats.currentHp)", H),
   ("maximum", "maxHp", "Maximum HP (same offset as Battle::ActorStats.maxHp)", H)], recv=r"state->stats")
T("T00591", "State", "Battle::Ai::RouteActorView", AI + "AiViews.h",
  "Views an actor's position pointer while a route between navigation areas is resolved.", L,
  c(("771A8.c", "((routeEdgeActor**)0x1F80037C)[state->target]->position[0];")) + FOLD,
  g.generic_specs("T00591"), recv=r"\[state->target\]")
T("T00592", "State", "Battle::Ai::RouteGateView", AI + "AiViews.h",
  "Views the room AI state's closed-gate bits while a route between navigation areas is resolved.", M,
  c(("771A8.c", "if (!((((routeEdgeGlobals*)D_800F58BC)->closedGates >> candidate.p.y)"))
  + ";view of D_800F58BC (Battle::Ai::RoomAiState, T00271) at offset 14, which its declaration leaves undeclared" + FOLD,
  g.generic_specs("T00592"), recv=r"\(\(routeEdgeGlobals\*\)D_800F58BC\)")
g.TYPES.append(["T00745", "Value", "Battle::Ai::RouteEdgePair", AI + "Navigation.h",
                "Holds the two route halves of a navigation edge, read as one word or by half.", M,
                c(("771A8.c", "typedef u_short routeEdgePair[2];"), ("771A8.c", "static inline u_short routeHalf(routeEdgePair* pair, int index) { return (*pair)[index]; }"))])
T("T00610", "State", "Battle::Ai::ApproachActorView", AI + "AiViews.h",
  "Views a battle actor's id while a target approach is updated.", L,
  "declaration-only;" + g.decl("T00610", "id") + ";view of Battle::Actor" + FOLD, g.generic_specs("T00610"), recv=r"\bactor")
T("T00611", "State", "Battle::Ai::ApproachRelationBits", AI + "AiViews.h",
  "Views the relation word bits tested while a target approach is updated.", L,
  "declaration-only;" + g.decl("T00611", "f0") + ";view of Battle::Ai::RelationWord" + FOLD,
  g.generic_specs("T00611", g.names_from_views("T00611", ["T00515"])), recv=r"targets\[[^\]]*\]")
T("T00612", "State", "Battle::Ai::ApproachTargetRecordView", AI + "AiViews.h",
  "Views a target's actor record flags and position while a target approach is updated.", L,
  c(("771A8.c", "model = (targetMovementModel*)D_800F4538[target];")) + ";view of Actors::ActorRecord" + FOLD,
  g.generic_specs("T00612"), recv=r"\b(?:model|s->model)")
T("T00614", "State", "Battle::Ai::TerrainMoveActorView", AI + "AiViews.h",
  "Views the battle actor's queued command while a height-filtered move is issued.", M,
  c(("771A8.c", "terrainMovementActor* actor = state->actor;")) + ";view of Battle::Actor" + FOLD,
  [("pad0", "reserved0", "Not read by decompiled code", L),
   ("mode", "commandOpcode", "Queued command opcode; 9 walks (same offset as Battle::Actor.commandOpcode)", M),
   ("padA", "reservedA", "Not read by decompiled code", L),
   ("angle", "commandHeading", "Low half of the first command argument: the walk heading", L)], recv=r"\bactor")
T("T00739", "State", "Battle::Ai::WanderRecordView", AI + "AiViews.h",
  "Views an actor record's flags while wandering.", L,
  c(("771A8.c", "if (state->object->flags & 0x200000) {")) + ";view of Actors::ActorRecord" + FOLD,
  g.generic_specs("T00739"), recv=r"state->object")

# ---------------------------------------------------------------- tile words
T("T00815", "State", "Battle::Ai::TilePosition", AI + "Navigation.h",
  "Holds a tile position as one word or as tile x, height, z and status bytes.", M,
  "declaration-only;" + g.decl("T00815", "raw") + ";upstream vs_battle_movementPosition; tile bytes are Battle::Ai::TileWord",
  [("raw", "word", "Whole word", M), ("p", "tile", "Tile bytes", M)], recv=r"[\w\]\)]+")
g.D("T00815", ["T00816", "T00817", "T00818"])
for _tid, _name, _member in (("T00811", "Battle::Ai::BlockedTileWord", "p"), ("T00813", "Battle::Ai::TerrainTileWord", "p"),
                             ("T00812", "Battle::Ai::ApproachTileWord", "b")):
    T(_tid, "State", _name, AI + "AiViews.h", "Holds a tile word as a word or tile bytes.", M,
      "declaration-only;" + g.decl(_tid, "raw") + ";view of Battle::Ai::TilePosition" + FOLD,
      [("raw", "word", "Whole word", M), (_member, "tile" if _member == "p" else "bytes",
                                         "Tile bytes" if _member == "p" else "Bytes of the word", M)], recv=r"[\w\]\)]+")

# ---------------------------------------------------------------- unreferenced fork declarations
for _tid, _name, _loc, _purpose in (
        ("T00604", "Battle::SoundEventTargets", B + "ActionEffects.h", "Declares a sound event header followed by its targets; not read."),
        ("T00606", "Battle::StatusEffectTickTable", B + "StatusEffects.h", "Declares a table of status effect ticks; not read."),
        ("T00607", "Battle::StatusEffectTick", B + "StatusEffects.h", "Declares one status effect tick: mask and interval; not read."),
        ("T00632", "Battle::ActorStatsFlags", B + "Actor.h", "Declares a flag word of the actor statistics; not read."),
        ("T00643", "Battle::DoorRecord", B + "Room.h", "Declares a door record: id, exit, flags and lock; not read."),
        ("T00661", "Battle::PrimitiveBytes", B + "Room.h", "Declares four primitive bytes; not read."),
        ("T00681", "Items::WeaponReservedBlock", "Native\\Game\\include\\Items\\Loadout.h", "Declares 24 reserved halfwords of a weapon's statistics; not read.")):
    T(_tid, "State", _name, _loc, _purpose, L,
      "declaration-only;" + g.decl(_tid, g.members[_tid][0]["name"]) + ";no decompiled function reads it",
      g.generic_specs(_tid), recv=None)

T("T00619", "Value", "Battle::TextAnchorOffset", B + "TextBoxes.h",
  "Gives a dialog box anchor's packed offset, alignment and frame flags.", M,
  c(("4A0A8.c", "result->x = x = origin->vx + result->offset.x;")),
  g.generic_specs("T00619"), recv=r"\b(?:result->offset|best->offset)")

# ---------------------------------------------------------------- core battle types
ACT = B + "Actor.h"
T("T00630", "State", "Battle::Actor", ACT,
  "Links one battle actor: id, kind, queued command, equipment models, spawn data and its statistics.", H,
  c(("146C.c", "temp_s1->id = id;"), ("146C.c", "if (actor->unk8 == 0x40) {")), [
      ("next", "next", "Next actor in the room's actor list", H, c(("146C.c", "actor->next = vs_battle_actors[0]->next;"))),
      ("id", "id", "Actor slot index", H, c(("146C.c", "temp_s1->id = id;"))),
      ("unk8", "commandState", "Queued command state: 0x80 queued, 0x40 running, 0 idle", H,
       c(("146C.c", "if (actor->unk8 == 0x40) {"), ("146C.c", "if (actor->unk8 != 0x80) {"))),
      ("unk9", "commandOpcode", "Queued command opcode", H, c(("146C.c", "switch (actor->unk9) {"))),
      ("unkA", "commandTarget", "Target actor of the queued command; 16 or more for an immediate position", M,
       c(("146C.c", "if (actor->unkA >= 16) {"))),
      ("unkB", "commandFlags", "Bits 24-27 of the command word: model sound and follow-up, read through the word", L,
       "declaration-only;" + g.decl("T00630", "unkB") + ";" + c(("146C.c", "switch ((*(u_int*)&actor->unk8 >> 26) & 3) {"))),
      ("unkC", "commandArg0", "First command argument; the remaining x offset (fixed point) of a step command", M,
       c(("146C.c", "if (arg0->unkC < 0) {"))),
      ("unk10", "commandArg1", "Second command argument; the remaining z offset of a step command", M,
       c(("146C.c", "if (arg0->unk10.s32 < 0) {"))),
      ("unk14", "commandArg2", "Third command argument; the remaining y offset of a step command", M,
       c(("146C.c", "if (arg0->unk14 < 0) {"))),
      ("unk18", "modelId", "Model id the actor was created from", H,
       c(("146C.c", "if ((vs_battle_actors[i] != 0) && (vs_battle_actors[i]->unk18 == arg0)) {"))),
      ("subType", "subType", "Template sub-type; scored when an enemy is killed", M, c(("146C.c", "u_int subType = enemy->subType;"))),
      ("unk1C", "kind", "Actor kind bits: 1 Ashley, 2 non-hostile, 4 hostile, 8 object, 0x10 auxiliary; 0x8000 excludes targeting", H,
       c(("146C.c", "temp_s1->unk1C = 1;"), ("146C.c", "&& !(vs_battle_actors[i]->unk1C & 0x8000)) {"))),
      ("unk1E", "facing", "Facing from the room spawn", M, c(("146C.c", "actor->unk1E = spawn->facing;"))),
      ("unk1F", "spawnByte26", "Byte 0x26 of the room spawn record; not read afterwards", L, c(("146C.c", "actor->unk1F = spawn->unk26;"))),
      ("weaponDrawn", "weaponDrawn", "Bit 0 set while the actor is in battle mode", H,
       c(("146C.c", "int func_8007C088(int arg0) { return vs_battle_actors[arg0]->weaponDrawn & 1; }"))),
      ("unk24", "interactionFlags", "Spawn flag bits 2-4; 4 lets the player trigger the actor's event with the action button", L,
       c(("146C.c", "temp_s1->unk24 = arg4 & 0x1C;"), ("146C.c", "if ((i >= 0) && (vs_battle_actors[i]->unk24 == 4)"))),
      ("equippedWeaponCategory", "equippedWeaponCategory", "Category of the equipped blade; 10 when unarmed", H,
       c(("146C.c", "actor->equippedWeaponCategory = category;"))),
      ("unk26", "retainAfterDefeat", "Keeps the actor after its defeat (spawn flag 0x80)", H,
       c(("146C.c", "return vs_battle_actors[arg0]->unk26;"))),
      ("unk27", "roomSpawnIndex", "Index of the actor's room spawn; 0x80 for none", H, c(("146C.c", "actor->unk27 = spawn->index;"))),
      ("defeated", "defeated", "Set once the actor's defeat has been handled", M, c(("146C.c", "if ((enemy->unk27 == 0x80) || (enemy->defeated != 0)) {"))),
      ("unk29", "killCounted", "Set when a killing blow on the actor counted toward Break Art progress", H,
       c(("146C.c", "vs_battle_actors[arg2->unk0.targetActor]->unk29 = 1;"))),
      ("unk2A", "bladeId", "Blade model id of the equipped weapon", M, c(("146C.c", "temp_s1->unk2A = arg2;"))),
      ("unk2B", "shieldId", "Shield model id", M, c(("146C.c", "temp_s1->unk2B = arg3;"))),
      ("unk2C", "lastTriggerTile", "Tile word last checked for floor triggers", M,
       c(("146C.c", "if (((s2->unk0.value & 0xFFFFFF) != (temp_s0->unk2C.s32 & 0xFFFFFF))"))),
      ("unk30_0", "aiGroup", "AI group number other actors link to", M, c(("7CD68.c", "if (other->unk30_0 == actor->unk30_9) {"))),
      ("unk30_4", "reserved30Bit4", "Not read by decompiled code", L),
      ("unk30_9", "aiLinkedGroup", "AI group this actor follows or guards", M, c(("7CD68.c", "if (other->unk30_0 == actor->unk30_9) {"))),
      ("unk30_14", "reserved31Bit6", "Not read by decompiled code", L),
      ("unk34", "reserved34", "Not read by decompiled code", L),
      ("unk38", "templateWord2C", "Word 0x2C of the actor template; not read afterwards", L, c(("146C.c", "actor->unk38 = init->unk2C;"))),
      ("unk3C", "stats", "Combat statistics", H, c(("146C.c", "actor->unk3C = (vs_battle_actor2*)(actor + 1);"))),
      ("unk40", "allocationKind", "Allocation layout: 1 with two equipment model slots, 2 with one", M,
       c(("146C.c", "actor->unk40 = 2;"), ("146C.c", "if (actor->unk40 == 2) {"))),
      ("unk44", "model", "Actor model and animation state", H, c(("146C.c", "actor->unk44 = (void*)((char*)actor + 0x9B4);"))),
      ("unk48", "equipmentModels", "Weapon and shield model slots", H, c(("146C.c", "actor->unk48[0] = (void*)((char*)actor + 0x22B4);"))),
  ])
T("T00631", "State", "Battle::ActorStats", ACT,
  "Holds one battle actor's statistics: name, HP, MP, Risk, attributes, speeds, ranges, equipment, limbs, actions, "
  "statuses, recovery timer and drops.", H,
  c(("146C.c", "temp_s0->maxHP = 100;"), ("146C.c", "actor->unk944 = init->unk28;")), [
      ("name", "name", "Display name", H, c(("5BF94.c", "vs_battle_stringContext.strings[0] = vs_battle_actors[i]->unk3C->name;"))),
      ("currentHP", "currentHp", "Current HP", H, c(("146C.c", "temp_s0->currentHP = 100;"))),
      ("maxHP", "maxHp", "Maximum HP", H, c(("146C.c", "temp_s0->maxHP = 100;"))),
      ("currentMP", "currentMp", "Current MP", H, c(("146C.c", "temp_s0->currentMP = 100;"))),
      ("maxMP", "maxMp", "Maximum MP", H, c(("146C.c", "temp_s0->maxMP = 100;"))),
      ("risk", "risk", "Risk", H, c(("146C.c", "temp_s0->risk = 0;"))),
      ("strength", "strength", "Base STR", M, None),
      ("totalStrength", "totalStrength", "STR with equipment", M, None),
      ("intelligence", "intelligence", "Base INT", M, None),
      ("totalIntelligence", "totalIntelligence", "INT with equipment", M, None),
      ("agility", "agility", "Base AGL", M, None),
      ("totalAgility", "totalAgility", "AGL with equipment", M, None),
      ("unk2E", "baseWalkSpeed", "Walk speed without statuses (nine tenths of the template's)", H,
       c(("146C.c", "arg1->unk31 = arg1->unk2E;"))),
      ("unk2F", "baseRunSpeed", "Run speed without statuses (nine tenths of the template's)", H, c(("146C.c", "arg1->unk33 = arg1->unk2F;"))),
      ("unk30", "movementClass", "Template movement class; 0x10 and 0x20 make saved placement use the current tile while flying", L,
       c(("146C.c", "actor->unk30 = init->unk24;"), ("146C.c", "func_800A36E0(arg0, temp_s1->unk30, &sp10);"))),
      ("unk31", "walkSpeed", "Current walk speed; halved by some statuses", H, c(("146C.c", "movementSpeed = actor->unk3C->unk31 << 12;"))),
      ("unk32", "gaitClass", "Template gait class; 2 and 0x40 make queued walks use the run speed", L,
       c(("146C.c", "if (actor->unk3C->unk32 == 2) {"))),
      ("unk33", "runSpeed", "Current run speed; halved by some statuses", H, c(("146C.c", "movementSpeed = actor->unk3C->unk33 << 12;"))),
      ("unk34", "mobilityLimb0", "First limb whose injury, with the second, inflicts Agility Down; 0 for none", M,
       c(("146C.c", "var_a2 = temp_s3->limbs[temp_s3->unk34].hp + arg0->limbs[temp_s3->unk34].hp;"))),
      ("unk35", "mobilityLimb1", "Second limb whose injury, with the first, inflicts Agility Down; 0 for none", M,
       c(("146C.c", "if ((actor->unk35 != 0) && (actor->limbs[actor->unk35].hp >= 2)) {"))),
      ("unk36", "defaultTargetLimb", "Limb targeted when an action does not pick one", H,
       c(("146C.c", "sp10.unk4C[0].unk0.targetLimb = vs_battle_actors[targetActor]->unk3C->unk36;"))),
      ("enemyClass", "enemyClass", "Enemy class", M, None),
      ("reach", "reach", "Reach added to the weapon's ranges", M, c(("146C.c", "actor->unk39 = actor->weapon.range.unk1 + actor->reach;"))),
      ("currentRange", "currentRange", "Current attack range", M, None),
      ("unk39", "currentRangeY", "Current attack range second component: weapon range plus reach", M,
       c(("146C.c", "actor->unk39 = actor->weapon.range.unk1 + actor->reach;"))),
      ("unk3A", "currentRangeZ", "Current attack range third component: weapon range plus reach", M,
       c(("146C.c", "actor->unk3A = actor->weapon.range.unk2 + actor->reach;"))),
      ("unk3B_0", "currentAttackShape", "Current attack range shape; 1 when a weapon is equipped", M, c(("146C.c", "actor->unk3B_0 = 1;"))),
      ("currentAttackShapeAngle", "currentAttackShapeAngle", "Current attack range shape angle", M, None),
      ("weapon", "weapon", "Equipped weapon statistics", H, None),
      ("shield", "shield", "Equipped shield statistics", H, None),
      ("accessory", "accessory", "Equipped accessory statistics", H, None),
      ("limbs", "limbs", "Statistics of the six limbs", H, None),
      ("armor", "limbActions", "Up to four action slots per limb", H,
       c(("146C.c", "actor->armor[i][j].id = init->limbs[i].actions[j].id;"))),
      ("dpSplash", "dpSplash", "Fork name kept; see cited use", L, None),
      ("unk944", "statusImmunities", "Status flags the actor cannot receive", H,
       c(("146C.c", "if ((!(params->unk0 & temp_a1->unk944)) && (!(params->unkC & temp_a1->statuses))) {"))),
      ("statuses", "statuses", "Active status flags (Battle::StatusFlag)", H, None),
      ("statusTimers", "statusTimers", "Remaining time of timed statuses", H, None),
      ("unk954", "actionRecoveryTimer", "Recovery time after an action; actions are restricted while non-zero", H,
       c(("146C.c", "arg1->unk954 += (arg1->weapon.unk10B + arg1->unk956_2) << 8;"), ("146C.c", "int ret = actor->unk954 != 0;"))),
      ("unk956_0", "cancelsHitResults", "Hits on the actor are reset to outcome 4 after their effects are applied; set for placeholders", L,
       c(("146C.c", "if (arg3 && stats->unk956_0) {"))),
      ("isAnalyzed", "isAnalyzed", "Set once the actor has been analyzed", M, None),
      ("unk956_2", "recoveryDelayBonus", "Template value added to every action's recovery delay", M,
       c(("146C.c", "actor->unk956_2 = init->unk23;"))),
      ("unk957", "actorId", "Actor slot index; 0x80 for placeholder statistics", H,
       c(("146C.c", "if (vs_battle_actors[actor->unk957]->weaponDrawn & 1) {"), ("146C.c", "temp_s0->unk957 = 0x80;"))),
      ("miscItem", "dropItem", "First item the actor drops", M, c(("146C.c", "actor->unk3C->miscItem.id = spawn->itemId;"))),
      ("unk95C", "secondDropItem", "Second item the actor drops", M, c(("146C.c", "actor->unk3C->unk95C.id = spawn->item2Id;"))),
      ("unk960", "randomDropItem", "Third item the actor drops, with a drop rate", M,
       c(("146C.c", "lootList->actorId |= _dropMiscRand(&lootList->loot, &temp_s2->unk960);"))),
  ], recv=r"\b(?:temp_s0|temp_a1|temp_a2|temp_s3|actor|arg1|stats|vs_battle_characterState->unk3C|->unk3C)")

ACTION = "Native\\Game\\include\\Battle\\Action.h"
T("T00629", "Def", "Battle::ActionDefinition", ACTION,
  "Defines one action: presentation effect, type, category, cost, range and area, recovery delay, AI flags, two hit "
  "definitions and its name.", H,
  c(("146C.c", "category = vs_main_actions[action].unk2_4;"), ("146C.c", "if (new_var->flags_0 == 0xFF) {")), [
      ("id", "id", "Action id", M, c(("146C.c", "u_int v = action->id;"))),
      ("unk1", "effectId", "Presentation effect id", H, c(("146C.c", "request.effectId = vs_main_actions[action->actionIndex].unk1;"))),
      ("unk2_0", "returnsToMenuOnCancel", "Cancelling targeting returns to the battle action menu; otherwise menus close and recovery begins", M,
       c(("146C.c", "if (!vs_main_actions[D_800F19CC->unk8.actionIndex].unk2_0) {"))),
      ("type", "type", "Action type (Battle::ActionType)", H, None),
      ("unk2_4", "category", "Targeting category; indexes the category targeting tables", H,
       c(("146C.c", "category = vs_main_actions[action].unk2_4;"))),
      ("cost", "cost", "MP or HP cost", M, None),
      ("rangeX", "rangeX", "Range first component", M, None),
      ("rangeY", "rangeY", "Range second component", M, None),
      ("rangeZ", "rangeZ", "Range third component", M, None),
      ("shape", "shape", "Range shape", M, None),
      ("angle", "angle", "Range shape angle", M, None),
      ("aoe_0", "areaRangeX", "Area first component", M, c(("146C.c", "D_800F19CC->unk29B4 = spec->aoe_0;"))),
      ("aoe_8", "areaRangeY", "Area second component; times 32 is the radial reach", M, c(("7CD68.c", "r = action->aoe_8 << 5;"))),
      ("aoe_16", "areaRangeZ", "Area third component", M, c(("146C.c", "D_800F19CC->unk29B6 = spec->aoe_16;"))),
      ("aoe_24", "areaShape", "Area shape in the low three bits", M, c(("146C.c", "D_800F19CC->unk29B0 = spec->aoe_24 & 7;"))),
      ("flags_0", "recoveryDelay", "Recovery delay added after the action; 0xFF uses the weapon's", H,
       c(("146C.c", "if (new_var->flags_0 == 0xFF) {"))),
      ("flagsD_0", "reservedDBit0", "Not read by decompiled code", L),
      ("flagsD_4", "opaqueDBit4", "Copied to the action state; 1 is cleared when the first target is 0x1C1 or farther", L,
       c(("146C.c", "&& (((vs_main_actions[D_800F19CC->unk8.actionIndex].flagsD_4)) == 1)) {"))),
      ("flagsD_6", "resetsHitResults", "Hits are reset to outcome 4 after their effects (bit 0x4000 of the word at 0xC)", L,
       "declaration-only;" + g.decl("T00629", "flagsD_6") + ";" + c(("146C.c", "if (arg3 && (*(int*)&spec->flags_0 & 0x4000)) {"))),
      ("unlocked", "unlocked", "Set once the player has learned the action", M, None),
      ("flagsE", "aiFlags", "Flags read by enemy AI action selection", M, c(("74088.c", "flags = arg3->flagsE;"))),
      ("unk10", "reserved10", "Not read by decompiled code", L),
      ("hitParams", "hits", "Two hit definitions: effect, prerequisite, hit, stat calculator, factor, type and affinity", H,
       c(("146C.c", "threshold = _hitFunctions[action->hitParams[hitNumber].hitFunction]("))),
      ("name", "name", "Display name", H, None),
  ], recv=r"\b(?:action|spec|new_var|info|arg3|vs_main_actions\[[^\]]*\])")
T("T00628", "Value", "Battle::ActionAiFlags", ACTION,
  "Holds the action flags read by enemy AI action selection.", M,
  c(("74088.c", "if (flags.unk5 && (rand() & 0x7F) < 115) {")), [
      ("unk0", "reserved0Bit0", "Not read by decompiled code", L),
      ("unk4", "opaque0Bit4", "With the actor's settingsModeLow set, scores the action as special with weight 40", L,
       c(("74088.c", "if (arg0->unk2B && flags.unk4) {"))),
      ("unk5", "rarelyChosen", "The AI rejects the action 115 times in 128", M, c(("74088.c", "if (flags.unk5 && (rand() & 0x7F) < 115) {"))),
      ("unk6", "reserved0Bit6", "Not read by decompiled code", L),
      ("unk7", "shapedArea", "Area reach comes from the area range shape", M, c(("7CD68.c", "} else if (action->flagsE.unk7 << 0x17) {"))),
      ("unk8", "reserved1Bit0", "Not read by decompiled code", L),
      ("unk12", "opaque1Bit4", "Copied into the candidate and fallback flags", L,
       c(("74088.c", "list[1].unk10.unk2 = info->flagsE.unk12;"))),
      ("unk13", "radialArea", "Area reach is areaRangeY times 32", M, c(("7CD68.c", "if (action->flagsE.unk13 << 0x1D) {"))),
      ("unk14", "reserved1Bit6", "Not read by decompiled code", L),
  ])
T("T00709", "Def", "Battle::StatusEffectParams", B + "StatusEffects.h",
  "Defines one status effect: its status flags, tick interval, the statuses it cancels and the statuses that block it.", M,
  c(("146C.c", "if ((!(params->unk0 & temp_a1->unk944)) && (!(params->unkC & temp_a1->statuses))) {")), [
      ("unk0", "statusFlags", "Status flags the effect sets", M, c(("146C.c", "remove = params->unk0 & ~temp_a2->unk944 & (~temp_t0 | params->unk8);"))),
      ("unk4", "reserved4", "Not read by decompiled code", L),
      ("unk6", "tickInterval", "Ticks per HP change of a damage-over-time status", M,
       c(("74088.c", "stats->currentHP -= ticks / vs_main_statusEffectParams[14].unk6 * scale / 100;"))),
      ("unk8", "cancelledStatuses", "Active statuses the effect cancels instead of stacking", M,
       c(("146C.c", "inflict = params->unk8 & temp_a2->statuses;"))),
      ("unkC", "blockingStatuses", "Active statuses that keep the effect from applying", M, c(("146C.c", "if (!(temp_t0 & params->unkC)) {"))),
  ])
T("T00726", "State", "Render::FrameScratch", "Native\\Graphics\\src\\Render.h",
  "Holds the per-frame drawing context kept in the scratchpad: primitive cursor, ordering tables, view facing, view "
  "matrix and camera.", H,
  c(("146C.c", "vs_scratch.unk0 = D_8005E0C0[vs_main_frameBuf];"), ("146C.c", "vs_scratch.unk4 = ot + 18;")), [
      ("unk0", "primitiveCursor", "Next free primitive in the frame's primitive buffer", H,
       c(("146C.c", "vs_scratch.unk0 = D_8005E0C0[vs_main_frameBuf];"))),
      ("unk4", "orderingTable", "Main ordering table", H, c(("146C.c", "vs_scratch.unk4 = ot + 18;"))),
      ("unk8", "overlayOrderingTable", "Ordering table entry for overlay sprites, in front of the main table", H,
       c(("146C.c", "vs_scratch.unk8 = ot + 2;"))),
      ("unkC", "reservedC", "Not read by decompiled code", L),
      ("firstPersonViewFacing", "firstPersonViewFacing", "Facing of the first-person view", M, None),
      ("viewMatrix", "viewMatrix", "View matrix", M, None),
      ("camera", "camera", "Camera", M, None),
  ], recv=r"\b(?:vs_scratch|\(\(vs_scratch_t\*\)0x1F800000\))")
T("T00636", "State", "Battle::ActionSlot", ACT,
  "Holds one limb action slot: action id, template index, effect model and a set marker.", M,
  c(("146C.c", "actor->armor[i][j].id = init->limbs[i].actions[j].id;")), [
      ("id", "actionId", "Action in the slot", H, c(("146C.c", "actor->armor[i][j].id = init->limbs[i].actions[j].id;"))),
      ("index", "index", "Template index", L, c(("146C.c", "actor->armor[i][j].index = init->limbs[i].actions[j].index;"))),
      ("unk2_4", "effectModelId", "Effect model shown when the action is presented", M,
       c(("146C.c", "actor->armor[i][j].unk2_4 = init->limbs[i].actions[j].unk2;"))),
      ("unk3", "opaque3", "Set to 1 when the slot is filled; not read", L, c(("146C.c", "actor->armor[i][j].unk3 = 1;"))),
  ])
T("T00676", "State", "Battle::LimbStats", ACT,
  "Holds one limb's HP, bone, name, evasion, types, affinities and armor.", H,
  c(("146C.c", "actor->limbs[i].unk5 = j;")), [
      ("hp", "hp", "Current HP", H, None), ("maxHp", "maxHp", "Maximum HP", H, None),
      ("agilityDefenseBonus", "agilityDefenseBonus", "Fork name kept; see cited use", M, None),
      ("unk5", "anchorBone", "Model bone the limb's labels and markers are projected from", H,
       c(("146C.c", "actor->limbs[i].unk5 = j;"), ("MENU/MENU4.PRG/120.c", "func_800A13EC(1, limb->unk5, lineXy, 0);"))),
      ("nameIndex", "nameIndex", "Limb name index", M, None),
      ("chainEvasion", "chainEvasion", "Chain evasion", M, None),
      ("types", "types", "Type values", M, None),
      ("affinities", "affinities", "Affinity values", M, None),
      ("armor", "armor", "Equipped armor statistics", H, None),
  ], recv=r"\b(?:limbs\[[^\]]*\]|limb)")
T("T00672", "State", "Battle::TextBoxFlags", B + "TextBoxes.h",
  "Packs a text box's kind, open style, alignment, frame shape and part, arrow frame, speaker texture and state bits.", M,
  c(("5BF94.c", "switch (box->unk0.unk0_0) {")), [
      ("unk0_0", "kind", "Box kind; 4 uses the alternate font table", M, c(("5BF94.c", "switch (box->unk0.unk0_0) {"))),
      ("unk0_4", "openStyle", "Opening style; selects the opening step count", L, c(("5BF94.c", "switch (box->unk0.unk0_4) {"))),
      ("unk0_6", "alignment", "Anchor alignment; flips the frame", M, c(("4A0A8.c", "box->unk0.unk0_6 = best->offset.alignment;"))),
      ("unk0_8", "frameShape", "Frame shape index", M, c(("5BF94.c", "u_short* verts = (u_short*)D_800EB588[box->unk0.unk0_8];"))),
      ("unk0_12", "framePart", "Frame part selected from the shape", L, c(("5BF94.c", "part = ((int*)shape)[box->unk0.unk0_12];"))),
      ("arrowAnimFrame", "arrowAnimFrame", "Continue-arrow animation frame", M, None),
      ("unk0_24", "speakerTexture", "Speaker portrait texture; bit 0x20 for none", M, c(("4A0A8.c", "box->unk0.unk0_24 = texture;"))),
      ("done", "done", "Set when the text has finished", M, None),
      ("unk0_31", "opaque3Bit7", "Tested first each update; meaning not established", L, c(("5BF94.c", "if (box->unk0.unk0_31) {"))),
  ], recv=r"\b(?:box->unk0|arg0->unk0|ctx->unk0|vs_battle_textBoxes\[[^\]]*\]\.unk0|temp_s1->unk0)")
LOOT = "Native\\Game\\src\\Items\\Loot.h"
LOOT_CITE = {"blade": ("if (_droppableUnderCurrentDifficulty(arg0->blade.difficultyFlags) != 0) {",
                         "_addLootToList(itemCategoryBlade, _copyBladeToInventory(&arg0->blade.blade, 0));"),
             "gem": ("if (_droppableUnderCurrentDifficulty(arg0->gem.difficultyFlags) != 0) {",
                     "_addLootToList(itemCategoryGem, _copyGemToInventory(&arg0->gem.gem, 0));"),
             "grip": ("if (_droppableUnderCurrentDifficulty(arg0->grip.difficultyFlags) != 0) {",
                      "_addLootToList(itemCategoryGrip, _copyGripToInventory(&arg0->grip.grip, 0));")}
for _tid, _name, _item in (("T00649", "Items::LootedBlade", "blade"), ("T00650", "Items::LootedGem", "gem"), ("T00651", "Items::LootedGrip", "grip")):
    _flags, _copy = LOOT_CITE[_item]
    T(_tid, "State", _name, LOOT, f"Holds a looted {_item} with the difficulty flags that allow its drop.", M,
      c(("MENU/MENUB.PRG/260.c", _flags)), [
          ("difficultyFlags", "difficultyFlags", "Difficulties under which the item drops", M, c(("MENU/MENUB.PRG/260.c", _flags))),
          ("unk1", "reserved1", "Not read by decompiled code", L), ("unk2", "reserved2", "Not read by decompiled code", L),
          ("unk3", "reserved3", "Not read by decompiled code", L),
          (_item, _item, f"The looted {_item}", M, c(("MENU/MENUB.PRG/260.c", _copy)))])
AFF = "Native\\Game\\include\\Items\\Loadout.h"
T("T00641", "State", "Items::ClassAffinityBaseline", AFF, "Holds an item's baseline class and affinity values.", M,
  c(("146C.c", "target->classAffinityBaseline.class[i] =")), [
      ("class", "classes", "Class values (renamed: class is a C++ keyword)", M, c(("146C.c", "target->classAffinityBaseline.class[i] ="))),
      ("affinity", "affinities", "Affinity values", M, c(("146C.c", "target->classAffinityBaseline.affinity[i] ="))),
  ])
T("T00642", "State", "Items::ClassAffinityCurrent", AFF, "Holds an item's two sets of current class and affinity values.", M,
  c(("146C.c", "target->classAffinityCurrent.class[0][i] =")), [
      ("class", "classes", "Class values per set (renamed: class is a C++ keyword)", M, c(("146C.c", "target->classAffinityCurrent.class[0][i] ="))),
      ("affinity", "affinities", "Affinity values per set", M, c(("146C.c", "target->classAffinityCurrent.affinity[0][i] ="))),
  ])


def E(tid, name, loc, purpose, prefix):
    specs = []
    for m in g.members[tid]:
        rest = m["name"][len(prefix):] if m["name"].startswith(prefix) else m["name"]
        final = rest[0].upper() + rest[1:]
        specs.append((m["name"], final, f"{name.rsplit('::', 1)[1]} {final}", M))
    T(tid, "Enum", name, loc, purpose, M, "declaration-only;" + g.decl(tid, g.members[tid][0]["name"]), specs)


E("T00093", "Battle::LimbConditionLevel", ACT, "Names the five limb conditions from critical to excellent.", "limbStatus")
E("T00094", "Menu::Framework::MenuItemScroll", "Native\\Game\\src\\Menu\\Framework\\MenuItems.h",
  "Names a menu row's scroll arrow.", "menuItem_scroll")
E("T00095", "Menu::Framework::MenuItemState", "Native\\Game\\src\\Menu\\Framework\\MenuItems.h",
  "Names a menu row's animation state.", "menuItemState")
E("T00096", "Menu::Framework::MenuState", "Native\\Game\\src\\Menu\\Framework\\MenuItems.h",
  "Names the battle menu states.", "menuState")
E("T00097", "Battle::StatusFlag", B + "StatusEffects.h", "Names the battle status flag bits.", "vsBattleStatus")
E("T00098", "Battle::WarlockSpell", ACTION, "Names the Warlock spell action ids.", "vs_battle_spell_")
E("T00101", "Items::GemTarget", "Native\\Game\\include\\Items\\Equipment.h", "Names the gem target for shields.", "gemTarget")
E("T00102", "Items::ItemCategory", "Native\\Game\\include\\Items\\Inventory.h", "Names the item categories.", "itemCategory")

if __name__ == "__main__":
    g.write()
