"""Game_Battle_05 globals from code read directly. Citations are resolved by exact text search (common.Gen).

Each name comes from the use sites printed by files/gu.py and read in their functions. Inventory sizes are address
gaps, not capacities. String tables were matched to assets/BATTLE/BATTLE.PRG/*.yaml by their splat offsets.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen  # noqa: E402

g = Gen("Game_Battle_05")
c, G = g.c, g.G
L, M, H = "low", "med", "high"
B = "Native\\Game\\src\\Battle\\"
AI = B + "Ai\\"
GAP = ";inventory size is an address gap, not a capacity"
HUD, TGT, FB, SPH = B + "Hud.c", B + "Targeting.c", B + "Feedback.c", B + "Sphere.c"

# ---------------------------------------------------------------- group A: HUD
G("G00189", "catalog", "Battle::secondShortcutIconSizes", HUD, "Gives the packed width and height of the icons of shortcuts 4-7.", M,
  c(("5BF94.c", "D_800EBCA8[i], vs_scratch.unk8);")) + GAP)
G("G00190", "catalog", "Battle::secondShortcutIconTexCoords", HUD, "Gives the texture coordinates of the icons of shortcuts 4-7.", M,
  c(("5BF94.c", "temp_v0_2[4] = (*((i) + D_800EBCB8) | 0x373D0000);")) + GAP)
G("G00191", "catalog", "Battle::firstShortcutMarkerPositions", HUD, "Gives the x then y positions of the button markers of shortcuts 0-3.", M,
  c(("5BF94.c", "((temp_s3 + D_800EBCC0[i]) & 0xFFFF) | (D_800EBCC0[i + 4] << 0x10),")) + GAP)
G("G00192", "catalog", "Battle::firstShortcutIconPositions", HUD, "Gives the x then y positions of the icons of shortcuts 0-3.", M,
  c(("5BF94.c", "(((temp_s3 + D_800EBCC8[i]) - 2) & 0xFFFF)")) + GAP)
G("G00193", "catalog", "Battle::firstShortcutIconSizes", HUD, "Gives the packed width and height of the icons of shortcuts 0-3.", M,
  c(("5BF94.c", "D_800EBCD0[i], vs_scratch.unk8);")) + GAP)
G("G00194", "catalog", "Battle::firstShortcutIconTexCoords", HUD, "Gives the texture coordinates of the icons of shortcuts 0-3.", M,
  c(("5BF94.c", "temp_v0_2[4] = D_800EBCE0[i] | 0x373D0000;")) + GAP)
G("G00195", "state", "Battle::shortcutHudAnimationStep", HUD,
  "Steps the shortcut HUD slide: counts down to 0 while requested, else follows the closing sequence to 10 (hidden).", H,
  c(("5BF94.c", "D_800EBCE8 = vs_battle_animationIndices[D_800EBCE8];"), ("5BF94.c", "if (D_800EBCE8 != 10) {")))
G("G00196", "catalog", "Battle::limbDisplaySpriteOffsets", HUD,
  "Gives the screen offset of each limb sprite of the HUD limb display; five per battle mode.", M,
  c(("5BF94.c", "0, k + D_800EBCEC[j], D_800EBD14[j], ot);")) + ";indexed by limb + mode * 5" + GAP)
G("G00197", "catalog", "Battle::limbDisplaySpriteSizes", HUD,
  "Gives the packed width and height of each limb sprite of the HUD limb display.", M,
  c(("5BF94.c", "0, k + D_800EBCEC[j], D_800EBD14[j], ot);")) + GAP)
G("G00198", "catalog", "Battle::limbDisplaySpriteTexCoords", HUD,
  "Gives the texture coordinates and CLUT of each limb sprite of the HUD limb display.", M,
  c(("5BF94.c", "prim[4] = D_800EBD3C[j];")) + GAP)
G("G00199", "state", "Battle::battleModeIndicatorWobble", HUD,
  "Advances the sine phase that bobs the weapon-state indicator.", H,
  c(("5BF94.c", "k = rsin(D_800EBD64 & 0x7FF);"), ("5BF94.c", "D_800EBD64 += vs_gametime_tickspeed * 64;")))
G("G00205", "catalog", "Battle::spellClassCounts", B + "ActionMenu.c", "Gives the number of spells in each spell class.", H,
  c(("5BF94.c", "for (i = 0; i < _spellClassCounts[spellClass]; ++i) {")) + ";fork name kept" + GAP)
G("G00206", "catalog", "Battle::spellClassSpellIds", B + "ActionMenu.c", "Lists the spell action ids of each spell class.", H,
  c(("5BF94.c", "if (vs_main_actions[_spellIds[spellClass][i]].unlocked) {")) + GAP)
G("G00207", "catalog", "Battle::chainAbilityActionIds", B + "ActionMenu.c", "Lists the action ids of the 14 Chain Abilities.", H,
  c(("5BF94.c", "if (v0[defense == 0 ? vs_battle_chainAbilityOffsets[i]"), ("MENU/MENU2.PRG/64.c", "int action = vs_battle_chainAbilityOffsets[i];")) + GAP)
G("G00208", "catalog", "Battle::defenseAbilityActionIds", B + "ActionMenu.c", "Lists the action ids of the 14 Defense Abilities.", H,
  c(("5BF94.c", ": vs_battle_defenseAbilityOffsets[i]]"), ("MENU/MENU2.PRG/64.c", "int action = vs_battle_defenseAbilityOffsets[i];")) + GAP)
G("G00209", "asset", "Battle::doorStrings", B + "TextBoxes.c",
  "Holds the door and chest messages and the no-target message: a u16 offset table followed by the encoded strings.", H,
  "asset:text/battle/door-strings;" + c(("5BF94.c", "vs_battle_printf((char*)D_800EB9D8, (char*)&D_800EBDDC[D_800EBDDC[arg0]]);"),
                                         ("5BF94.c", "vs_battle_setTextBox(7, (char*)&D_800EBDDC[D_800EBDDC[0]]);"))
  + ";splat offset 0x835DC is assets/BATTLE/BATTLE.PRG/doorStrings.yaml; entry 0 is noTarget")
G("G00211", "drop", "Battle::puzzleTimerTextPointer", HUD,
  "Points to the puzzle timer text that the timer rewrites each frame.", H,
  "replaced-by:Battle::puzzleTimerText;" + c(("5BF94.c", "D_800EC258[6 - i * 3] = (bcd >> 4) + '0';"))
  + ";its initial value in BATTLE.PRG is 0x80069998 (G00614)")
G("G00614", "state", "Battle::puzzleTimerText", HUD,
  "Holds the puzzle timer text, initially \"00:00:00\"; its digits are rewritten each frame through G00211.", H,
  c(("5BF94.c", "char const D_80069998[] = \"00:00:00\";"), ("5BF94.c", "vs_battle_renderTextRaw(D_800EC258, vs_getXY(128, 200), vs_scratch.unk4 - 4);"))
  + ";declared const but written through the pointer G00211")
G("G00507", "state", "Battle::lowerScreenUiState", HUD,
  "Selects the lower-screen UI: 1 while the puzzle timer is shown; bit 1 hides the HUD limb display.", M,
  c(("5BF94.c", "void func_800CB550(void) { vs_battle_lowerScreenUiState = 1; }"), ("5BF94.c", "if (!(vs_battle_lowerScreenUiState & 2)"))
  + ";fork name kept")

# ---------------------------------------------------------------- group A: target selection columns
G("G00483", "state", "Battle::targetColumnEntries", TGT,
  "Lists the Sphere target indices shown in each of the two target selection columns.", H,
  c(("5BF94.c", "D_800F4C70[1][D_800F4CB0++] = i;"), ("5BF94.c", "D_800F4C70[0][D_800F4CB1++] = i;")))
G("G00489", "state", "Battle::firstColumnEntryCount", TGT, "Counts the targets in the first target selection column.", H,
  c(("5BF94.c", "D_800F4C70[0][D_800F4CB1++] = i;")))
G("G00488", "state", "Battle::secondColumnEntryCount", TGT, "Counts the targets in the second target selection column.", H,
  c(("5BF94.c", "D_800F4C70[1][D_800F4CB0++] = i;")))
G("G00485", "state", "Battle::firstColumnGroups", TGT, "Lists the target groups (actors) placed in the first column.", H,
  c(("5BF94.c", "D_800F4C98[D_800F4CB3++] = group;")))
G("G00491", "state", "Battle::firstColumnGroupCount", TGT, "Counts the target groups placed in the first column.", H,
  c(("5BF94.c", "D_800F4C98[D_800F4CB3++] = group;")))
G("G00487", "state", "Battle::firstColumnGroupFirstTargets", TGT, "Gives the first target of each group in the first column.", H,
  c(("5BF94.c", "D_800F4CA8[D_800F4CB3] = first - 1;"), ("5BF94.c", "D_800EB9B8->unk2D = D_800F4CA8[i];")))
G("G00484", "state", "Battle::secondColumnGroups", TGT, "Lists the target groups (actors) placed in the second column.", H,
  c(("5BF94.c", "D_800F4C90[D_800F4CB2++] = group;")))
G("G00490", "state", "Battle::secondColumnGroupCount", TGT, "Counts the target groups placed in the second column.", H,
  c(("5BF94.c", "D_800F4C90[D_800F4CB2++] = group;")))
G("G00486", "state", "Battle::secondColumnGroupFirstTargets", TGT, "Gives the first target of each group in the second column.", H,
  c(("5BF94.c", "D_800F4CA0[D_800F4CB2] = first - 1;"), ("5BF94.c", "D_800EB9B8->unk2D = D_800F4CA0[i];")))
G("G00492", "state", "Battle::targetColumnTopOffset", TGT,
  "Centres the target columns vertically: half of 11 pixels per unused row of 16.", H,
  c(("5BF94.c", "D_800F4CB4 = ((16 - i) * 11) >> 1;"), ("58578.c", "y = arg0->unk6 + D_800F4CB4;")))

# ---------------------------------------------------------------- group A: floating labels and markers
G("G00310", "state", "Battle::floatingLabels", FB, "Points to the 32 floating labels (values, statuses, chain counts).", H,
  c(("2842C.c", "vs_main_bzero(D_800F1BAC, sizeof *D_800F1BAC);"), ("2842C.c", "for (p = *D_800F1BAC; i < D_800F227E; ++i, ++p) {"))
  + ";points to Battle::FloatingLabel[32]")
G("G00372", "state", "Battle::floatingLabelCount", FB, "Counts the floating labels in use.", H,
  c(("2842C.c", "++D_800F227E;"), ("2842C.c", "for (p = *D_800F1BAC; i < D_800F227E; ++i, ++p) {")))
G("G00320", "state", "Battle::placedLabelPositions", FB,
  "Holds the screen positions of labels already placed this frame, to push new labels clear of them.", H,
  c(("2842C.c", "x = pos[0] - D_800F1CBC[i].vx;"), ("2842C.c", "y = pos[1] - D_800F1CBC[i].vy;")))
G("G00371", "state", "Battle::placedLabelCount", FB, "Counts the labels placed this frame.", H,
  c(("2842C.c", "D_800F227C = 0;"), ("2842C.c", "for (i = 0; i < D_800F227C; ++i) {")))
G("G00369", "state", "Battle::statusLabelStepTimer", FB,
  "Accumulates half ticks; past 20 each status label's lifetime steps down and older status labels move up.", M,
  c(("2842C.c", "D_800F2278 += vs_gametime_tickspeed / 2;"), ("2842C.c", "if (D_800F2278 > 20) {")))
G("G00370", "state", "Battle::ashleyStatusLabelsSuppressed", FB, "Suppresses status labels on Ashley while set.", M,
  c(("2842C.c", "void func_8009722C(void) { D_800F227A = 1; }"), ("2842C.c", "if (label->unk8.u8[1] != 0 || D_800F227A == 0) {")))
G("G00366", "state", "Battle::actorMarkersHidden", FB, "Hides every actor marker while set.", H,
  c(("2842C.c", "void func_80095B70(int arg0) { D_800F2270 = arg0; }"), ("2842C.c", "if (D_800F2270 == 0) {")))
G("G00365", "state", "Battle::actorMarkers", FB, "Gives each actor's marker kind in the low seven bits; bit 7 is set and cleared separately.", M,
  c(("2842C.c", "D_800F2260[arg0] |= arg1 & 0x7F;"), ("2842C.c", "func_80095C18(i, D_800F2260[i]);")))
G("G00368", "state", "Battle::actorMarkerSides", FB,
  "Holds one bit per actor choosing the side its marker is drawn on, switched when it passes another actor by 32.", M,
  c(("2842C.c", "D_800F2274 |= 1 << actor;"), ("2842C.c", "side = (D_800F2274 >> actor) & 1;")))
G("G00367", "state", "Battle::abilityTimingCueFrames", FB, "Counts the frames the ability timing cue stays on screen.", H,
  c(("2842C.c", "D_800F2272 += arg0;"), ("2842C.c", "--D_800F2272;")))
G("G00373", "state", "Battle::abilityTimingCuePosition", FB, "Holds the screen position of the ability timing cue above Ashley.", H,
  c(("2842C.c", "if (func_800A1280(0, 0xFB, &D_800F2280, 0) != 0) {"), ("2842C.c", "setXY4(prim, D_800F2280.vx - 11, D_800F2280.vy - 32, D_800F2280.vx + 11,")))
G("G00606", "constexpr", "Battle::kMarkerFrameCorners", FB, "Gives the four corner offsets of the marker frame for each side.", M,
  c(("2842C.c", "corners = D_80068F04[side];")))
G("G00607", "constexpr", "Battle::kMarkerFrameTexCoords", FB, "Gives the four texture coordinates of the marker frame for each side.", M,
  c(("2842C.c", "uv = D_80068F64[side];")))
G("G00608", "constexpr", "Battle::kMarkerIconOffsets", FB, "Gives the offset of the marker icon for each side.", M,
  c(("2842C.c", "vs_battle_setSprite(128, xy + D_80068F94[side], vs_getWH(7, 12), ot)[4] =")))
G("G00609", "constexpr", "Battle::kLabelCaptionRects", FB, "Gives the texture rectangles of the floating label captions.", H,
  c(("2842C.c", "RECT const* rect = &D_80068FAC[D_800E861C[p->unkE - 3]];"), ("2842C.c", "rects[i] = &D_80068FAC[D_800690B4[p->unkC].labels[i]];")))
G("G00610", "constexpr", "Battle::kStatusLabelLayouts", FB,
  "Gives each status label's caption count, caption rectangles and color.", H,
  c(("2842C.c", "for (i = 0; i < D_800690B4[p->unkC].count; ++i) {"), ("2842C.c", "width = D_800690B4[p->unkC].color;")))
G("G00611", "constexpr", "Battle::kStatusLabelColors", FB, "Gives the four status label colors.", H,
  c(("2842C.c", "setRGB0(prim, D_80069134[width].r / 2, D_80069134[width].g / 2,")))
G("G00612", "constexpr", "Battle::kStatChangeFirstWordRects", FB,
  "Gives the texture rectangle of the first word of an equipment stat change label.", M,
  c(("2842C.c", "x += D_80069144[p->unkE].w + 2;")))
G("G00613", "constexpr", "Battle::kStatChangeSecondWordRects", FB,
  "Gives the texture rectangle of the second word of an equipment stat change label.", M,
  c(("2842C.c", "setXY4(prim, x, y, x + D_8006917C[p->unkA].w, y, x, y + D_8006917C[p->unkA].h,")))

# ---------------------------------------------------------------- group B: AI tables and room AI state
AST, NAV, PLAN, PER, MOV = AI + "ActorState.c", AI + "Navigation.c", AI + "ActionPlanning.c", AI + "Perception.c", AI + "Movement.c"
ORB = AI + "OrbitingBoss.c"
G("G00230", "catalog", "Battle::Ai::directionDeltas", NAV,
  "Gives the tile x and z step of each AI direction; the flood fill steps through every second entry.", M,
  c(("7CD68.c", "x = arg0 + D_800F16EC[i * 2].dx;"), ("7CD68.c", "for (i = 0, step = D_800F16EC; i < 4; ++i, step += 2) {"))
  + ";element type Battle::Ai::DirectionDelta" + GAP)
G("G00231", "state", "Battle::Ai::frameTickBudget", PER,
  "Skips an actor's AI work for the frame once the elapsed root-counter ticks exceed it.", M,
  c(("6FC00.c", "if (D_800F16F4 < arg) {"), ("71618.c", "if (D_800F16F4 < value) {")) + ";compared with Core::ElapsedFrameCounterTicks")
G("G00232", "catalog", "Battle::Ai::routeStepDirections", NAV,
  "Maps a route edge's two direction bits (bits 11-12) to an AI direction index.", M,
  c(("771A8.c", "step = &D_800F175C[(route >> 11) & 3];"), ("771A8.c", "direction = &table[(route >> 11) & 3];")) + GAP)
G("G00233", "catalog", "Battle::Ai::decisionCallbacks", AST,
  "Gives the decision function an actor uses, selected by bits 4-8 of its AI flags; stored as decisionCallback.", H,
  c(("7CD68.c", "state->callback = D_800F1790[(actor->flags.raw >> 4) & 31];")) + GAP)
G("G00234", "catalog", "Battle::Ai::viewAngles", AST,
  "Gives the view angle in degrees selected by bits 16-17 of the AI settings word (360, 190, 85, 40 in BATTLE.PRG).", H,
  c(("7CD68.c", "state->angle = D_800F17B8[settings->half[1] & 3][0];"), ("7CD68.c", "state->cosine = rcos((state->angle / 2) * 4096 / 360);")) + GAP)
G("G00235", "catalog", "Battle::Ai::settingsBits8Values", AST,
  "Gives the AI state value at 0xB0 selected by bits 8-9 of the AI settings word (0, 130, 258, 514 in BATTLE.PRG).", L,
  c(("7CD68.c", "state->fB0 = D_800F17C8[(settings->raw >> 8) & 3][0];")) + ";value meaning not established" + GAP)
G("G00236", "catalog", "Battle::Ai::facingDelayScales", AST,
  "Gives the facing delay scale (sixteenths of 128, or 32 for large actors) selected by bits 2-3 of the AI settings word.", M,
  c(("7CD68.c", "state->fBA = (step * D_800F17D8[(settings->raw >> 2) & 3]) >> 4;")) + ";stored at AI state offset 0xBA (facingDelay)" + GAP)
G("G00237", "catalog", "Battle::Ai::radialRetreatPath", ORB,
  "Gives a retreating boss's radial distance and height for each frame; an all-zero entry ends the retreat.", M,
  c(("7EE98.c", "if (!D_800F17DC[motion->frame][0] && !D_800F17DC[motion->frame][1]) {"),
    ("7EE98.c", "state->targetY = (state->y + D_800F17DC[motion->frame][1]) << 12;")) + GAP)
G("G00579", "state", "Battle::Ai::actorAiStates", AST, "Points to each actor's AI state, indexed by actor id.", H,
  c(("7CD68.c", "D_800F5878[id] = (void*)state;"), ("6FC00.c", "height = ((actorRelationState*)D_800F5878[id])->height;"))
  + ";points to Battle::Ai::ActorAiState")
G("G00580", "state", "Battle::Ai::tileFlagMap", NAV,
  "Points to the room's 32 by 32 AI tile flags: 0x20 special terrain, 0x10 a tile an actor starts on, 0x70 no ground.", M,
  c(("7CD68.c", "if (x < 32 && z < 32 && !(D_800F58B8[z][x] & 0x70)) {"), ("7CD68.c", "if ((D_800F58B8[z][x] & 0x20) && !(D_800F58D0[z][x] & 0x2000)) {"))
  + ";filled by code that is not decompiled")
G("G00581", "state", "Battle::Ai::roomAiState", AST, "Points to the room's AI state: bounds, readiness level and engaged actors.", H,
  c(("6FC00.c", "if (D_800F58BC->unk18 < 0xFFFE) {"), ("6FC00.c", "D_800F58BC->unk24 = 0;")) + ";points to Battle::Ai::RoomAiState")
G("G00582", "state", "Battle::Ai::roomHasWaypoints", AST,
  "Enables waypoint behavior: when set and a waypoint is enabled, bit 12 of the AI settings word is applied.", L,
  c(("7CD68.c", "if (D_800F58C0) {")) + ";filled by code that is not decompiled")
G("G00583", "state", "Battle::Ai::savedMovementRandomState", MOV, "Keeps the movement random generator state saved from the scratchpad.", H,
  c(("6FC00.c", "void func_800D9538(void) { D_800F58C4 = *(int*)0x1F8003F8; }")))
G("G00584", "state", "Battle::Ai::roomWaypoints", NAV,
  "Points to the room's waypoint words: bit 15 enabled, bits 0-4 tile x, bits 5-9 tile z.", M,
  c(("74088.c", "entry = *nearestWaypointAddress(D_800F58C8, index);"), ("7CD68.c", "if (D_800F58C8[i] >> 15) {")))
G("G00585", "state", "Battle::Ai::specialTerrainHeights", NAV,
  "Points to the 32 by 32 heights filled into special terrain from its neighbours: bits 0-9 height, bit 13 filled.", M,
  c(("7CD68.c", "D_800F58D0[arg1][arg0] = (D_800F58D0[arg1][arg0] & 0xFC00) | value | 0x2000;"), ("74088.c", "*(int*)0x1F8003BC = (int)D_800F58D0;"))
  + ";the nearest-waypoint search uses it as the height map")
G("G00586", "state", "Battle::Ai::secondPerceptionTarget", PER,
  "Gives the argument of a second perception pass; the pass is skipped while it is zero.", L,
  c(("6FC00.c", "arg = D_800F58D4;")) + ";never written by decompiled code")
G("G00587", "state", "Battle::Ai::patrolRoutes", MOV, "Points to the room's patrol routes, eight waypoints per route.", M,
  c(("771A8.c", "row = D_800F58D8[state->row];")))
G("G00588", "state", "Battle::Ai::candidateEngagedThisUpdate", PER,
  "Set when an actor engages an action candidate; cleared at the start of each perception update.", L,
  c(("6FC00.c", "D_800F58E0 = 0;"), ("71618.c", "D_800F58E0 = 1;")) + ";not read by decompiled code")
G("G00589", "state", "Battle::Ai::planningStatsSnapshot", PLAN,
  "Holds every actor's statistics while plans are simulated, restored afterwards.", H,
  c(("74088.c", "D_800F58E8 = vs_main_allocHeapR(sizeof(vs_battle_actor2) * 16);"), ("74088.c", "func_800DC3E8(&D_800F58E8[actor->id], actor->unk3C);")))
G("G00594", "state", "Battle::Ai::planningWorkspace", PLAN,
  "Points to the 0x1ECC-byte planning workspace: the reaction record, then pass candidates, exclusions and fallbacks.", H,
  c(("74088.c", "D_800F5900 = vs_main_allocHeapR(0x1ECC);"), ("74088.c", "if (D_800F5900->unk4.unk40 == 0) {")))
G("G00590", "state", "Battle::Ai::planningPassCandidates", PLAN, "Points to 16 candidates per planning pass, inside the planning workspace.", M,
  c(("74088.c", "D_800F58EC = (void*)((char*)D_800F5900 + 0x9CC);"), ("74088.c", "func_800DC2E0(&D_800F58EC[pass][i], state->entries[i]);")))
G("G00595", "state", "Battle::Ai::candidateExclusions", PLAN,
  "Points to one flag per target and action; a set flag keeps that action off that target.", M,
  c(("74088.c", "D_800F5904 = (void*)((char*)D_800F5900 + 0xD8C);"), ("74088.c", "if (!func_800E4CF4(arg0, candidate, id) || D_800F5904[id][action]) {")))
G("G00593", "state", "Battle::Ai::fallbackCandidates", PLAN, "Points to the fallback candidates used when a plan has none.", M,
  c(("74088.c", "D_800F58F8 = (void*)((char*)D_800F5900 + 0x1D8C);"), ("74088.c", "func_800DC2E0(fallback, &D_800F58F8[i]);")))
G("G00591", "state", "Battle::Ai::fallbackCandidateCount", PLAN, "Counts the fallback candidates.", M,
  c(("74088.c", "for (i = 0; i < D_800F58F0; ++i) {"), ("74088.c", "state->count = D_800F58F0;")))
G("G00592", "state", "Battle::Ai::unusedPlanCounter", PLAN,
  "Cleared at the start of each plan build beside fallbackCandidateCount; not otherwise used by decompiled code.", L,
  c(("74088.c", "D_800F58F4 = 0;")))
G("G00596", "state", "Battle::Ai::attributeActorMask", AST,
  "Holds one bit per actor whose attributes have any of bits 0, 2 and 4 when its AI state is created.", L,
  c(("7CD68.c", "D_800F590C |= 1 << id;")) + ";not read by decompiled code")
G("G00597", "state", "Battle::Ai::routeGates", NAV, "Points to the room's route gates as pairs of entry and exit ends.", M,
  c(("771A8.c", "if (D_800F5910[i * 2].unk0_0 == arg0->unk0"), ("771A8.c", "arg1->unk0 = D_800F5910[i * 2 + 1].unk0_0;")) + ";element type Battle::Ai::RouteGateEnd")
G("G00598", "state", "Battle::Ai::orbitAngularVelocity", ORB, "Gives the orbiting boss's angular velocity (16.16) added to its heading.", M,
  c(("7EE98.c", "D_800F5918 += delta;"), ("7EE98.c", "motion->facing += D_800F5918 >> 16;")))
G("G00599", "state", "Battle::Ai::orbitHeightOffset", ORB, "Gives the orbiting boss's height above the arena centre, kept between -100 and 500.", M,
  c(("7EE98.c", "D_800F591C += correction >> 16;"), ("7EE98.c", "state->targetY = center[1] - (D_800F591C << 12);")))
G("G00600", "state", "Battle::Ai::arenaOrbit", ORB, "Points to the boss arena orbit state; null in other rooms.", M,
  c(("71618.c", "if (D_800F5920 == NULL) {"), ("74088.c", "if (D_800F5920->unk16 && (u_int)(self->subType - 172) < 2) {"))
  + ";points to Battle::Ai::ArenaOrbitState")
G("G00615", "constexpr", "Battle::Ai::kFlags34Bit14Values", AST,
  "Gives the AI state value at 0x102 selected by bit 14 of the actor's second flag word (9 or 7).", L,
  c(("7CD68.c", "state->f102 = D_80069C08[(actor->flags34.raw >> 14) & 1];")) + ";value meaning not established")
G("G00619", "constexpr", "Battle::Ai::kStatusScores", PLAN, "Gives the planning score of each of the 32 status bits.", H,
  c(("74088.c", "score += statusScores[i];")) + ";fork name kept")

# ---------------------------------------------------------------- group C: battle flow
REW, FLOW, EXE = B + "Rewards.c", B + "ActionFlow.c", B + "ActionExecution.c"
G("G00240", "state", "Battle::lootMenuOpened", REW, "Set once the loot list menu is opened, so closing it continues to the loot results.", M,
  c(("146C.c", "D_800F1860 = 1;"), ("146C.c", "if (D_800F1860 != 0) {")))
G("G00241", "state", "Battle::rewardsFollowAction", REW,
  "Set when rewards follow an action rather than an event; Ashley's pose and ability celebration are restored around them.", M,
  c(("146C.c", "D_800F1864 = 1;"), ("146C.c", "D_800F1864 = 0;")))
G("G00242", "state", "Effects::effectBusyFlags", "Native\\Game\\src\\Effects\\Runtime.c",
  "Holds the effect runtime's busy bits each frame: bit 0 primary, bit 1 secondary effects still running.", M,
  c(("146C.c", "D_800F1868 = func_800CF060();"), ("146C.c", "int func_80074294(void) { return D_800F1868 & 3; }")))
G("G00255", "state", "Battle::actionSelection", FLOW,
  "Points to the menu action-selection state captured when the battle menu returns a selection; kind 8 uses a misc item.", M,
  c(("146C.c", "D_800F18EC = menu;"), ("146C.c", "vs_battle_decreaseMiscCount(D_800F18EC->executeAbility.u16[1]);")))
G("G00256", "state", "Battle::actionFlowMode", FLOW,
  "Selects the stage of the action flow (targeting, execution, ability timing, rewards, loot) that the camera tracking "
  "and game loop switch on.", M,
  c(("146C.c", "switch (_cameraMode) {"), ("146C.c", "_cameraMode = 0xB;")) + ";fork calls it _cameraMode; values 0-14")
G("G00259", "state", "Battle::recoveryRewardMode", FLOW,
  "Recovery kind passed to BeginPostActionRecovery: 0 skips the ability reward check, 2 checks Break Art unlocks first.", M,
  c(("146C.c", "D_800F18FC = arg0;"), ("146C.c", "switch (D_800F18FC) {")))
G("G00265", "state", "Battle::actors", B + "Actors.c", "Points to the battle actor in each of the 16 actor slots.", H,
  c(("146C.c", "vs_battle_actor* actor = vs_battle_actors[i];")) + ";points to Battle::Actor;fork name kept")
G("G00274", "state", "Battle::actionState", FLOW, "Points to the current action state.", H,
  c(("146C.c", "if (D_800F19CC->unk8.actionIndex >= 40) {")) + ";points to Battle::ActionState")
G("G00276", "state", "Battle::settledCameraDistance", EXE,
  "Compared with the action's camera distance; the action sequence waits until they match.", L,
  c(("146C.c", "if ((vs_main_projectionDistance != 512) || (D_800F19D8 != D_800F19CC->unk2C0C)")) + ";never written by decompiled code")
G("G00277", "state", "Battle::cameraEaseMode", EXE,
  "When 1 or 3, the projection and camera distance ease toward their targets while an action finishes.", L,
  c(("146C.c", "if ((D_800F19EC == 1) || (D_800F19EC == 3)) {")) + ";never written by decompiled code")
RES = B + "ActionResolution.c"
G("G00279", "state", "Battle::hitPrerequisiteResult", RES,
  "Holds the current hit's prerequisite result; when zero the hit threshold becomes 255.", M,
  c(("146C.c", "D_800F1A00 = _canPerformAttack(spec, (char*)entry, hit);"), ("146C.c", "if (D_800F1A00 == 0) {")))
G("G00280", "state", "Battle::previousHitResult", RES,
  "Holds the previous hit's result; a follow-up hit function returns 100 when it hit, else 255.", M,
  c(("146C.c", "D_800F1A04 = _doesAttackHit(spec, entry, entry, hit, arg3);"), ("146C.c", "return D_800F1A04 != 0 ? 100 : 255;")))
G("G00281", "state", "Battle::lastActionStatValue", B + "Damage.c", "Keeps the last stat change a hit calculated, for later scaling.", H,
  c(("146C.c", "_lastValue = amount;"), ("146C.c", "return (_lastValue * arg0->hitParams[arg3].statFactor) / 10;")) + ";fork name kept")
G("G00282", "state", "Battle::criticalHitOccurred", B + "Damage.c",
  "Set when a basic attack's damage was doubled by the Risk-based critical chance; the hit is then shown as critical.", H,
  c(("146C.c", "D_800F1A0C = 1;"), ("146C.c", "arg2->hpEffect = 3;")))
G("G00283", "state", "Battle::limbArmorDpLoss", B + "Damage.c",
  "Holds each limb's share of a hit's value; each limb's armor later loses that much DP.", M,
  c(("146C.c", "_armorDpAdjustmentAmounts[i] = baseValue / limbs;"), ("146C.c", "target->limbs[i].armor.currentDp -= _armorDpAdjustmentAmounts[i];")))
ANIMB = B + "ActionAnimation.c"
G("G00423", "state", "Battle::chainAbilityAnimationSide", ANIMB,
  "Alternates the side of successive Chain Ability animations; reset when a chain starts.", H,
  c(("30DB0.c", "D_800F49D8 ^= 1;"), ("30DB0.c", "anim += D_800F49D8;")))
G("G00426", "state", "Battle::specialActionPitch", ANIMB,
  "Gives the target elevation of the current special action: 0 below, 1 level, 2 above; selects its animation.", H,
  c(("30DB0.c", "D_800F49E4 = 2;"), ("40564.c", "idx = idx * 3 + D_800F49E4;")))

# ---------------------------------------------------------------- group C: unreferenced and aliases
G("G00530", "drop", "Battle::legacyUnusedTargetBlock", "Native\\Game\\src\\Battle\\Legacy.c",
  "Reserves an unreferenced effect target block declared in the target selection unit.", H,
  "replaced-by:nothing;no function references in either tree;" + c(("5BF94.c", "extern D_800F53B8_t3 D_800F5234;")))
G("G00531", "drop", "Battle::legacyUnreferencedByte", "Native\\Game\\src\\Battle\\Legacy.c",
  "Reserves an unreferenced byte declared in the target selection unit.", H,
  "replaced-by:nothing;no function references in either tree;" + c(("5BF94.c", "extern u_char D_800F5238;")))
G("G00573", "drop", "Effects::rendererTableAlias", "Native\\Game\\src\\Effects\\Runtime.c",
  "Is the effect renderer table (G00574) declared again under its address name.", H,
  "view-of:G00574;replaced-by:the renderer table;" + c(("6F014.c", "extern effectExec D_800F56A8[];"), ("6F014.c", "vs_battle_effectRenderers[index] = func;"))
  + ";same address 0x800F56A8")

# ---------------------------------------------------------------- group C: main executable data used by battle
G("G01903", "constexpr", "Core::Math::kArcTangentTable", "Native\\Core\\src\\Math\\FixedPoint.c",
  "Gives the arctangent for a ratio scaled to 0-512, used by the fixed-point atan2.", H,
  c(("SLUS_010.40/31B7C.c", "c = D_80040A14[c];"), ("573B8.c", "angle = D_80040A14[angle];")) + GAP)
G("G01904", "catalog", "Battle::actionDefinitions", "Native\\Game\\src\\Battle\\ActionTable.c", "Defines the 256 actions.", H,
  c(("146C.c", "category = vs_main_actions[action].unk2_4;")) + ";element type Battle::ActionDefinition")
G("G01906", "catalog", "Battle::statusEffectParams", B + "StatusEffects.c", "Defines the 32 status effects.", H,
  c(("146C.c", "vs_main_statusEffectParams_t* params = &vs_main_statusEffectParams[arg0];")) + ";element type Battle::StatusEffectParams")
for _gid, _entry, _off, _name, _cite in (
        ("G01907", 5, 4, "statusParams5Duration", "arg1->statusTimers[0] = D_8004EF20;"),
        ("G01908", 11, 4, "statusParams11Duration", "arg1->statusTimers[1] = D_8004EF80;"),
        ("G01909", 13, 4, "statusParams13Duration", "arg1->statusTimers[3] = D_8004EFA0;"),
        ("G01910", 14, 6, "statusParams14TickInterval", "arg1->statusTimers[5] = D_8004EFB2;"),
        ("G01911", 17, 6, "statusParams17TickInterval", "arg1->statusTimers[5] = D_8004EFE2;"),
        ("G01912", 19, 4, "statusParams19Duration", "arg1->statusTimers[2] = D_8004F000;")):
    G(_gid, "drop", "Battle::" + _name, B + "StatusEffects.c",
      f"Is the low byte of statusEffectParams[{_entry}] at offset {_off}, loaded into a status timer.", H,
      f"view-of:G01906;replaced-by:Battle::statusEffectParams[{_entry}];" + c(("146C.c", _cite))
      + f";address = 0x8004EECC + {_entry} * 16 + {_off}")
G("G01916", "catalog", "Battle::breakArtPointRequirements", B + "Abilities.c",
  "Gives the kill points each weapon category needs for each of its four Break Arts.", H,
  c(("146C.c", ">= vs_main_artsPointsRequirements[category][artsLearned];")) + ";fork name kept" + GAP)
G("G01917", "catalog", "Battle::battleAbilityPointRequirements", B + "Abilities.c",
  "Gives the points needed to learn each next Battle Ability.", H,
  c(("146C.c", ">= vs_main_battleAbilitiesPointsRequirements[vs_main_artsStatus.kills")) + ";fork name kept" + GAP)
G("G01972", "state", "State::tickSpeed", "Native\\Game\\src\\State\\GameTime.c",
  "Gives the game ticks per frame (2 or 4); timers advance by it.", H,
  c(("146C.c", "vs_gametime_tickspeed = 2;"), ("146C.c", "D_800F19CC->battleAbilityTimer + vs_gametime_tickspeed * 2;")) + ";fork name kept")
G("G01981", "state", "Battle::spellCommitFlag", FLOW,
  "Cleared when an action from index 55 up is committed; not read by decompiled code.", L,
  c(("146C.c", "D_8005FFAF = 0;")))
G("G01982", "state", "Battle::abilityInputResult", B + "Abilities.c",
  "Cleared when a Battle Ability input is not successful; not read by decompiled code.", L,
  c(("146C.c", "D_8005FFB0 = 0;")))

if __name__ == "__main__":
    g.write()
