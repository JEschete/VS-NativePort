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

if __name__ == "__main__":
    g.write(partial=True)
