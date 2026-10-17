"""Game_Menu_02 globals, types and fields from code read directly. Citations are resolved by exact text search.

Function-local state enums are named after their declaring function (as in Game_Menu_01); other enums drop the
prefix their enumerators share; structs go through common.Gen.generic_specs with overrides for members whose uses
were read. Globals mostly keep meaningful fork names; address-named ones are named from their read uses.
"""
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen, LEDGERS  # noqa: E402
import Game_Menu_01 as m1  # noqa: E402  (helpers only; its module-level rows go to its own Gen)

g = Gen("Game_Menu_02")
c, G = g.c, g.G
L, M, H = "low", "med", "high"
FN = m1.FN
SAVE = m1.SAVE
FW = "Native\\Game\\src\\Menu\\Framework\\"
LOAD = "Native\\Game\\include\\Items\\Loadout.h"
INV = "Native\\Game\\include\\Items\\Inventory.h"
GAP = ";inventory size is an address gap, not a capacity"


def header_of(function: str) -> str:
    return FN[function]["final_location"].removesuffix(".c") + ".h"


def D(tid, member=None):
    return "declaration-only;" + g.decl(tid, member or g.members[tid][0]["name"])


def enum_specs(tid, name, conf):
    names = [m["name"] for m in g.members[tid]]
    split = [m1.words(n) for n in names]
    common = 0
    if len(names) > 1:
        while all(len(w) > common + 1 and w[common].lower() == split[0][common].lower() for w in split):
            common += 1
    out = []
    for n, w in zip(names, split):
        rest = [x for x in w[common:] if x != "_"]
        final = "".join(x[0].upper() + x[1:] for x in rest)
        out.append((n, final, f"{name.rsplit('::', 1)[1]} {final}", conf))
    return out


def T(tid, role, name, loc, purpose, conf, overrides=None, recv=r"[\w\]\)]+"):
    names = {m["name"] for m in g.members[tid]}
    assert set(overrides or {}) <= names, (tid, set(overrides) - names)
    g.T(tid, role, name, loc, purpose, conf, D(tid), g.generic_specs(tid, overrides), recv=recv)


# ---------------------------------------------------------------- function-local state machines
for _tid, _name, _loc, _purpose in (
        ("T00079", "Menu::Options::OptionsMenuState", header_of("Menu::Options::Update"), "Steps the Options menu (declared in Menu::Options::Update)"),
        ("T00080", "Menu::Score::ScoreOverlayState", header_of("Menu::Score::Update"), "Steps the Score overlay entry (declared in vs_menu9_exec)"),
        ("T00081", "Menu::Score::ScoreMenuState", header_of("Menu::Score::Update"), "Steps the Score menu (declared in Menu::Score::Update)"),
        ("T00082", "Menu::Loot::DisassemblyState", header_of("Menu::Loot::UpdateDisassembly"), "Steps item disassembly (declared in Menu::Loot::UpdateDisassembly)"),
        ("T00083", "Menu::Loot::LootSelectionState", header_of("Menu::Loot::UpdateLootSelection"), "Steps the loot screen (declared in Menu::Loot::UpdateLootSelection)"),
        ("T00084", "Menu::Container::SortMenuState", header_of("Menu::Container::UpdateSortMenu"), "Steps the container sort menu (declared in Menu::Container::UpdateSortMenu)"),
        ("T00085", "Menu::Manual::ManualOverlayState", header_of("Menu::Manual::UpdateTopicMenu"), "Steps the Manual overlay entry (declared in vs_menuE_exec)"),
        ("T00086", "Menu::Manual::ManualMenuState", header_of("Menu::Manual::UpdateTopicMenu"), "Steps the Manual menu (declared in _showMenu)"),
        ("T00087", "Menu::Manual::TopicMenuState", header_of("Menu::Manual::UpdateTopicMenu"), "Steps the Manual topic list (declared in Menu::Manual::UpdateTopicMenu)")):
    g.T(_tid, "Enum", _name, _loc, _purpose, M, D(_tid), enum_specs(_tid, _name, M))

# ---------------------------------------------------------------- other enums
MM = header_of("Menu::Framework::CreateMenuRows")
TB = FW + "TextBoxes.h"
for _tid, _name, _loc, _purpose, _single in (
        ("T00092", "Menu::InformationBoxHeader", TB, "Names the information box headers.", None),
        ("T00100", "Menu::Files::FileMenuUiId", header_of("Menu::Files::DrawElement"), "Names the file menu interface sprites.", None),
        ("T00103", "Menu::Framework::ActionsMenuAction", MM, "Names the actions menu entries.", None),
        ("T00104", "Menu::Framework::ButtonId", MM, "Names the controller button icons.", None),
        ("T00105", "Menu::Framework::CopyItemFlag", MM, "Names the copy-item flag.", "Write"),
        ("T00106", "Menu::Framework::MainMenuIcon", MM, "Names the main menu icon.", "Gem"),
        ("T00107", "Menu::Framework::MenuCommand", MM, "Names what a menu row does when chosen.", None),
        ("T00108", "Menu::Framework::MenuItemId", MM, "Names the menu item id sentinel.", "None"),
        ("T00109", "Menu::Framework::MenuSelection", MM, "Names how a menu was left.", None),
        ("T00110", "Menu::Framework::DpPpBarsState", MM, "Names the DP/PP bar animation states.", None),
        ("T00111", "Menu::Framework::DpPpBarsTarget", MM, "Names which DP/PP bars are drawn.", None),
        ("T00112", "Menu::Framework::RowTypeFlag", MM, "Names the disabled row flag.", "Disabled"),
        ("T00113", "Menu::Framework::StatPage", MM, "Names the equipment stat pages.", None),
        ("T00114", "Menu::Framework::StatPageEnabled", MM, "Names which stat pages are enabled.", None),
        ("T00115", "Menu::Framework::StatusUiId", MM, "Names the equipment status interface elements.", None)):
    if _single:
        n = g.members[_tid][0]["name"]
        g.T(_tid, "Enum", _name, _loc, _purpose, M, D(_tid), [(n, _single, f"{_name.rsplit('::', 1)[1]} {_single}", M)])
    else:
        g.T(_tid, "Enum", _name, _loc, _purpose, M, D(_tid), enum_specs(_tid, _name, M))

# ---------------------------------------------------------------- structs
T("T00605", "OnDisc", "Save::SaveFileStats", SAVE, "Gives a save file's summary: play time, save counts, HP, MP, location, clears and map completion.", M)
T("T00618", "Value", "Menu::Framework::TextHeader", MM, "Gives a menu header's text, disabled flag and x.", M)
T("T00814", "Value", "State::PlayTime", "Native\\Game\\src\\State\\GameTime.h", "Holds the play time as fields or as one word.", M)
T("T00644", "Value", "Render::ScreenOffset", "Native\\Graphics\\src\\Render.h", "Gives a screen geometry offset.", M)
T("T00645", "State", "Items::BattleLoot", "Native\\Game\\src\\Items\\Loot.h", "Holds one battle's loot: weapon, blade, grip, shield, armor, accessory, gem and items.", M)
T("T00656", "State", "Menu::Score::CursorMemory", header_of("Menu::Score::Update"), "Remembers the Score menu's pages and rows.", M)
T("T00657", "State", "Menu::Framework::MenuRow", MM, "Holds one menu row: state, size, animation, selection, icons, colors and text.", M)
T("T00658", "State", "Menu::Framework::MenuControlState", MM, "Holds the menu state: current menu, return state, ability request, loading and read.", M)
T("T00663", "State", "Field::MapRoom", "Native\\Game\\src\\Field\\Map.h", "Gives one map room: visited flag, data and zone and map ids.", M)
T("T00664", "OnDisc", "Field::MapRoomName", "Native\\Game\\src\\Field\\Map.h", "Gives a map room's name and the previous and next rooms.", M)
T("T00665", "OnDisc", "Field::MapRoomVertices", "Native\\Game\\src\\Field\\Map.h", "Gives a map room outline's vertex count and vertices.", M)
T("T00666", "OnDisc", "Field::MapScene", "Native\\Game\\src\\Field\\Map.h", "Gives a map's room count and rooms.", M)
T("T00669", "State", "Menu::StringContext", TB, "Holds the integers and strings substituted into formatted text.", M)
T("T00671", "State", "Menu::TextBoxState", TB, "Holds one text box: flags, text, speed, layout, scaled frame, state and speaker.", M, {
    "unk0": ("flags", "Kind, open style, alignment, frame and speaker bits (Battle::TextBoxFlags)", H, c(("5BF94.c", "switch (box->unk0.unk0_0) {"))),
    "unk22": ("openDuration", "Frames the box takes to open or close", M, c(("4A0A8.c", "vs_battle_getTextBox(arg0[1] >> 5)->unk22 = arg0[1] & 0x1F;"))),
    "unk24": ("frameX", "Scaled frame x while opening", M, c(("5BF94.c", "box->unk24 = (box->x * scale + box->centerX * (0x10000 - scale)) >> 16;"))),
    "unk26": ("frameY", "Scaled frame y while opening", M, c(("5BF94.c", "box->unk26 = (box->y * scale + box->centerY * (0x10000 - scale)) >> 16;"))),
    "unk28": ("frameWidth", "Scaled frame width", M, c(("5BF94.c", "box->unk28 = ((box->charsPerLine * 12 + 10) * scale) >> 16;"))),
    "unk2A": ("frameHeight", "Scaled frame height", M, c(("5BF94.c", "box->unk2A = ((box->lineCount * 13 + 4) * scale) >> 16;"))),
}, recv=r"\b(?:box|ctx|arg0|temp_s1)")
AFF = ("affinityModifiers", "Temporary class/affinity modifiers added when the affinities are rebuilt; cleared by status removal", M)
T("T00674", "State", "Items::ArmorLoadout", LOAD, "Holds an equipped armor's statistics.", M,
  {"unk88": (*AFF, c(("146C.c", "v + arg0->limbs[arg1].armor.unk88[i];")))}, recv=r"\b(?:target|armor|arg1->limbs\[[^\]]*\]\.armor)")
T("T00679", "State", "Items::ShieldLoadout", LOAD, "Holds an equipped shield's statistics.", M,
  {"unk134": (*AFF, c(("146C.c", "temp_v0_2 + arg0->shield.unk134[i];")))}, recv=r"\b(?:target|shield|source|arg1->shield)")
T("T00680", "State", "Items::WeaponLoadout", LOAD, "Holds an equipped weapon's statistics.", M, {
    "unk10B": ("recoveryDelay", "Recovery delay after the weapon's attacks (20 for placeholders)", M,
               c(("146C.c", "arg1->unk954 += (arg1->weapon.unk10B + arg1->unk956_2) << 8;"))),
    "unk168": (*AFF, c(("146C.c", "temp_v0_2 + arg0->weapon.unk168[i];")))}, recv=r"\b(?:target|weapon|arg1->weapon)")
T("T00684", "State", "Render::Camera", "Native\\Graphics\\src\\Render.h", "Holds the camera position, look-at, angles and far clip.", M)
T("T00690", "Value", "Content::DiscFile", "Native\\Content\\src\\DiscQueue.h", "Gives a disc file's sector and size.", H)
T("T00691", "State", "Content::DiscQueueSlot", "Native\\Content\\src\\DiscQueue.h", "Holds one queued disc read: state, priority, file and destination.", M)
T("T00693", "State", "State::ArtsStatus", "Native\\Game\\src\\State\\Progress.h", "Holds Break Arts learned and kills per weapon category.", M)
T("T00695", "State", "State::StateFlags", "Native\\Game\\src\\State\\StateFlags.h",
  "Holds the game state flags: clears, difficulty, puzzle timer, boss records, keys, sigils, save points and chests.", M)
T("T00696", "State", "Items::InventoryArmor", INV, "Holds one inventory armor or shield base record.", M)
T("T00697", "State", "Items::InventoryBlade", INV, "Holds one inventory blade.", M)
T("T00698", "State", "Items::InventoryGem", INV, "Holds one inventory gem.", M)
T("T00699", "State", "Items::InventoryGrip", INV, "Holds one inventory grip.", M)
T("T00700", "State", "Items::InventoryIndices", INV, "Holds the display order of each inventory category.", M)
T("T00701", "State", "Items::InventoryMisc", INV, "Holds one inventory item stack.", M)
T("T00702", "State", "Items::InventoryShield", INV, "Holds one inventory shield: base record and gems.", M)
T("T00703", "State", "Items::InventoryWeapon", INV, "Holds one inventory weapon: blade, grip, gems and name.", M)
T("T00704", "State", "Items::Inventory", INV, "Holds the inventory by category.", H)
T("T00706", "State", "State::ScoreData", "Native\\Game\\src\\State\\Progress.h", "Holds titles, kills, attacks, time trial scores, chains, clears and completion.", M)
T("T00707", "State", "State::Settings", "Native\\Game\\src\\State\\Settings.h", "Holds the option settings.", M)
T("T00710", "Value", "Input::StickPosition", "Native\\Input\\src\\Buttons.h", "Gives the analog stick positions.", M)
T("T00711", "State", "Items::Container", INV, "Holds the container's items and display order.", M,
  {"data": ("items", "Items by category", M, D("T00711", "data"))})
T("T00712", "State", "Items::ContainerData", INV, "Holds the container's items by category.", M)
T("T00713", "State", "Items::ContainerIndices", INV, "Holds the container's display order by category.", M)
T("T00714", "State", "Menu::Container::InventoryStorage", header_of("Menu::Container::UpdateSortMenu"),
  "Holds the container menu's working copies: saved inventory, transfer previews, the container and the gadget bag.", M, {
      "unk0": ("savedInventory", "Copy of the inventory", M, c(("MENU/MENU7.PRG/260.c", "vs_battle_memcpy(vs_menu_inventoryStorage->unk0, &vs_main_inventory,"))),
      "unkF00": ("savedInventoryIndices", "Copy of the inventory order", M, c(("MENU/MENU7.PRG/260.c", "vs_battle_memcpy(&vs_menu_inventoryStorage->unkF00, &vs_main_inventoryIndices,"))),
      "unkC430": ("container", "The container", M, c(("MENU/MENUD.PRG/234.c", "D_80109A88 = &vs_menu_inventoryStorage->unkC430;"))),
      "unk105B0": ("gadgetBag", "The bag's working copy", L, c(("MENU/MENUD.PRG/234.c", "vs_menu_container* container = &vs_menu_inventoryStorage->unk105B0;"))),
  }, recv=r"vs_menu_inventoryStorage")
T("T00738", "OnDisc", "State::TrialScore", "Native\\Game\\src\\State\\Progress.h", "Gives one time trial record: time, round and difficulty.", M)

# ---------------------------------------------------------------- globals
MI = "Native\\Game\\src\\Menu\\Inventory\\Inventory.c"
G("G00000", "drop", "Menu::legacyUnusedVertices", "Native\\Game\\src\\Menu\\Legacy.c", "Is an unused vertex table marked unused in the fork.", H,
  "replaced-by:nothing;explicit unused attribute;" + c(("MENU/unused_vertices.h", "static u_int const __unused_vertices__[] __attribute__((unused)) = { 0xFFEAFFFA,")))
for _gid, _name, _purpose, _cite in (
        ("G00001", "ashleyRenderState", "Steps the Ashley preview: 1 initialise, 0 render", ("MENU/items1.h", "_ashleyRenderState = 1;")),
        ("G00002", "ashleyYOffset", "Bobs the Ashley preview vertically", ("MENU/_renderAshley.h", "_ashleyYOffset = ++yOffset;")),
        ("G00003", "exitToBattle", "Set when the menu is left with the button that returns to battle", ("MENU/items1.h", "_exitToBattle = vs_main_buttonsPressed.all & PADRup;")),
        ("G00004", "itemCategoryIconsAnimationStep", "Steps the item category icons' slide", ("MENU/MENU3.PRG/16C.c", "_itemCategoryIconsAnimationStep = 0;")),
        ("G00005", "itemCategoryIconsEnabled", "Shows the item category icons", ("MENU/items1.h", "_itemCategoryIconsEnabled = 1;")),
        ("G00006", "secondaryTitleAnimationStep", "Steps the secondary title's slide", ("MENU/items2.h", "_secondaryTitleAnimationStep = 0;")),
        ("G00007", "secondaryTitleEnabled", "Shows the secondary title", ("MENU/items2.h", "_secondaryTitleEnabled = 1;")),
        ("G00008", "selectedItemCategoryIconOnTop", "Draws the selected category icon above the others", ("MENU/items1.h", "_selectedItemCategoryIconOnTop = 2;")),
        ("G00009", "statusCommandAnimationStep", "Steps the status command's slide", ("MENU/MENU3.PRG/16C.c", "_statusCommandAnimationStep = 0;")),
        ("G00010", "statusCommandState", "Shows the status command", ("MENU/items1.h", "_statusCommandState = 1;"))):
    G(_gid, "state", "Menu::Inventory::" + _name, MI, _purpose + ".", H, c(_cite) + ";fork name kept")
G("G00011", "state", "Render::frameScratch", "Native\\Graphics\\src\\Render.c", "Holds the per-frame drawing context kept in the scratchpad.", H,
  c(("146C.c", "bound = inPos->vy - vs_scratch.camera.lookAt.vy;")) + ";type Render::FrameScratch")
G("G00063", "state", "Menu::Inventory::equipmentModelLoadStep", MI, "Steps reloading the equipment models after an equipment change.", M,
  c(("MENU/MENU3.PRG/68C4.c", "switch (vs_battle_loadObjectDataState) {")) + ";fork name loadObjectDataState")
G("G00138", "catalog", "Battle::limbNames", "Native\\Game\\src\\Battle\\Hud.c", "Gives the name of each limb.", H,
  c(("5BF94.c", "item->limbName = vs_battle_limbNames[src->unk8];")) + ";fork name kept" + GAP)
MF = FW + "Menus.c"
G("G00155", "state", "Menu::Framework::loadedSubMenu", MF, "Gives the menu overlay currently loaded.", H,
  c(("5BF94.c", "_loadedSubMenu = subMenu;")) + ";fork name kept")
G("G00156", "state", "Battle::hudStatusIconsHidden", "Native\\Game\\src\\Battle\\Hud.c", "Hides the HUD status icons while the status menu is open.", M,
  c(("MENU/MENU4.PRG/120.c", "D_800EB9AE = 1;"), ("5BF94.c", "if (D_800EB9AE != 0) {")))
G("G00158", "state", "Battle::hudSlideOffset", "Native\\Game\\src\\Battle\\Hud.c", "Gives how far the HUD has slid offscreen (0 shown).", H,
  c(("5BF94.c", "int xy = D_800EBBDC[colorIndex] - D_800EB9B0;"), ("5BF94.c", "D_800EB9B0 += 0x80000;")))
G("G00162", "state", "Menu::Framework::menuItems", MF, "Points to the menu rows.", H,
  c(("5BF94.c", "vs_battle_menuItem_t* vs_battle_getMenuItem(int id) { return vs_battle_menuItems + id; }")) + ";fork name kept")
G("G00163", "state", "Menu::Framework::lootLists", MF, "Points to the loot lists the loot menu applies.", H,
  c(("5BF94.c", "D_800EB9C4 = arg0;"), ("MENU/MENUB.PRG/260.c", "_applyAllLootLists(D_800EB9C4);")))
G("G00164", "state", "Menu::Framework::lootEntry", MF, "Points to the single loot record the loot menu applies.", H,
  c(("5BF94.c", "D_800EB9C8 = arg0;"), ("MENU/MENUB.PRG/260.c", "_applyLootList(D_800EB9C8);")))
G("G00167", "state", "Menu::textBoxBufferIndex", TB.removesuffix(".h") + ".c", "Selects which of three text box ordering tables is filled this frame.", H,
  c(("5BF94.c", "D_800EB9CE = D_800EB9CE == 2 ? 0 : D_800EB9CE + 1;")))
G("G00169", "state", "Menu::Framework::menuBackgroundBuffer", MF, "Points to the unpacked menu background.", H,
  c(("5BF94.c", "vs_main_freeHeapR(_menuBgUnpackedBuf);")) + ";fork name menuBgUnpackedBuf")
G("G00170", "state", "Menu::sceneNotificationBuffer", TB.removesuffix(".h") + ".c", "Points to the 112-byte text of the scene notification.", H,
  c(("5BF94.c", "D_800EB9D8 = vs_main_allocHeapR(112);")))
G("G00171", "asset", "Menu::menuStrings", MF, "Holds the battle menu strings: offsets followed by encoded text.", H,
  "asset:text/battle/menu-strings;" + c(("5BF94.c", "(char*)&vs_battle_menuStrings[vs_battle_menuStrings[mode + 12]];"))
  + ";assets/BATTLE/BATTLE.PRG/menuStrings.yaml")
G("G00172", "constexpr", "Menu::Framework::kMainMenuHelpText", MF, "Is the text shown when the main menu's help is requested.", L,
  c(("5BF94.c", "vs_battle_setTextBox(7, D_800EBA78);")) + GAP)
G("G00173", "catalog", "Menu::Framework::gradientTopColors", MF, "Gives the top gradient color of menu row colors 16 and up.", M,
  c(("5BF94.c", "color0 = D_800EBBB8[((colorIndex >> 3) - 2)];")) + GAP)
G("G00174", "catalog", "Menu::Framework::gradientBottomColors", MF, "Gives the bottom gradient color of menu row colors 16 and up.", M,
  c(("5BF94.c", "color1 = D_800EBBC0[((colorIndex >> 3) - 2)];")) + GAP)
G("G00175", "catalog", "Menu::Framework::rowAnimationSteps", MF, "Gives the x offset of each menu row slide step.", H,
  c(("58578.c", "if ((vs_battle_rowAnimationSteps[var_v1] + 0xF8) >= temp_a0) {")) + ";fork name kept" + GAP)
G("G00185", "catalog", "Menu::Framework::animationIndices", MF, "Gives the next step of each menu slide animation.", H,
  c(("5BF94.c", "D_800EBCE8 = vs_battle_animationIndices[D_800EBCE8];")) + ";fork name kept" + GAP)
G("G00200", "catalog", "Menu::Framework::abilityGroupActionRanges", MF,
  "Gives the action id range of three ability groups; a group's menu flag is set once any of its actions is learned.", M,
  c(("5BF94.c", "for (var_a0 = D_800EBD68[i * 2]; var_a0 < D_800EBD68[i * 2 + 1];")) + GAP)
MG = header_of("Menu::Magic::Update").removesuffix(".h") + ".c"
for _gid, _cls, _cite in (("G00201", "warlock", "int actionId = vs_battle_warlockSpellIds[i];"), ("G00202", "shaman", "actionId = vs_battle_shamanSpellIds[i];"),
                          ("G00203", "sorcerer", "actionId = vs_battle_sorcererSpellIds[i];"), ("G00204", "enchanter", "actionId = vs_battle_enchanterSpellIds[i];")):
    G(_gid, "catalog", f"Menu::Magic::{_cls}SpellIds", MG, f"Lists the {_cls.capitalize()} spell action ids.", H,
      c(("MENU/MENU0.PRG/84.c", _cite)) + ";fork name kept" + GAP)
G("G00210", "constexpr", "Menu::kNotificationItemNames", TB.removesuffix(".h") + ".c",
  "Gives the 12-byte names shown for items from 0x1CA in scene notifications (every second entry).", L,
  c(("5BF94.c", "vs_battle_stringContext.strings[0] = D_800EBF58[(arg1 - 0x1CA) * 2];")) + GAP)
G("G00214", "catalog", "Menu::Framework::cursorBrightnessAnimation", MF, "Gives the brightness of each cursor blink step.", H,
  c(("58578.c", "index == 2 ? vs_battle_cursorBrightnessAnimation[D_800EA6AC] : 0x80,")) + ";fork name kept" + GAP)
G("G00216", "catalog", "Menu::textBoxAnchors", TB.removesuffix(".h") + ".c", "Gives the two anchor offsets of each text box kind.", M,
  c(("5BF94.c", "&D_800EC284[arg0][((arg1 >> 2) & 3) == 1 || ((arg1 >> 2) & 3) == 2];")) + ";element type Menu::TextAnchorWord" + GAP)
G("G00217", "catalog", "Menu::informationHeaderPositions", TB.removesuffix(".h") + ".c", "Gives the sprite position of each information box header.", M,
  c(("5BF94.c", "vs_battle_setSpriteDefault(D_800EC2CC[arg1], (arg0 << 0x10) | 0xA)[4] =")) + GAP)

# Auto-citations whose receiver is another type (reviewed): cite the declaration instead.
WRONG_RECEIVER = {"T00605.002", "T00644.001", "T00645.004", "T00700.006", "T00701.002", "T00702.001", "T00702.002",
                  "T00704.005", "T00707.007", "T00707.014", "T00707.015", "T00712.002", "T00712.003", "T00712.005",
                  "T00713.005", "T00738.002"}
_orig = {m["id"]: (m["type_id"], m["name"]) for ms in g.members.values() for m in ms}
for _f in g.FIELDS:
    if _f[0] in WRONG_RECEIVER:
        _tid, _name = _orig[_f[0]]
        _f[3] = M if _f[3] == H else _f[3]
        _f[4] = "declaration-only;" + g.decl(_tid, _name)

if __name__ == "__main__":
    g.write()
