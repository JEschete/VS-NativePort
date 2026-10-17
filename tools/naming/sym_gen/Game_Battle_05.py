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

if __name__ == "__main__":
    g.write(partial=True)
