"""Game_Menu_01 types and fields from code read directly. Citations are resolved by exact text search (common.Gen).

Most types here are function-local `enum state` state machines: each is named after the function that declares it
(several identical declarations in sibling functions share one row) and placed in that function's header.
Enumerators keep their fork names without the prefix all enumerators of the type share. Structs go through
common.Gen.generic_specs (used members keep a meaningful fork name or become opaque<Off>; unread members are
reserved<Off>).
"""
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen, LEDGERS  # noqa: E402

g = Gen("Game_Menu_01")
c = g.c
L, M, H = "low", "med", "high"
FN = {r["final_name"]: r for r in csv.DictReader(open(LEDGERS / "FUNCTION_CATALOG.csv", encoding="utf-8")) if r["final_name"]}
SAVE = "Native\\Engine\\src\\Save\\MemoryCard\\GameFiles.h"


def header_of(function: str) -> str:
    return FN[function]["final_location"].removesuffix(".c") + ".h"


def D(tid: str) -> str:
    return "declaration-only;" + g.decl(tid, g.members[tid][0]["name"])


def words(n: str) -> list[str]:
    return re.findall(r"[A-Z]+(?![a-z])|[A-Z]?[a-z0-9]+|_", n)


def enumerators(tid: str, strip_suffix: str = "") -> list[tuple]:
    names = [m["name"] for m in g.members[tid]]
    split = [words(n) for n in names]
    common = 0
    if len(names) > 1:
        while all(len(w) > common + 1 and w[common].lower() == split[0][common].lower() for w in split):
            common += 1
    out = []
    for n, w in zip(names, split, strict=True):
        rest = [x for x in w[common:] if x != "_"]
        if strip_suffix and rest and rest[-1] == strip_suffix:
            rest = rest[:-1]
        final = "".join(x[0].upper() + x[1:] for x in rest) or n[0].upper() + n[1:]
        out.append((n, final, f"{final}", M))
    return out


RENAME = {"TempFileCreated": "TemporaryFileCreated"}


def E(tid, name, loc, purpose, conf=M, strip_suffix=""):
    specs = [(n, RENAME.get(f, f), f"{name.rsplit('::', 1)[1]} {RENAME.get(f, f)}", cf)
             for n, f, _, cf in enumerators(tid, strip_suffix)]
    g.T(tid, "Enum", name, loc, purpose, conf, D(tid), specs)


def S(tid, name, function, purpose):
    """A function-local state enum, named after (and placed beside) the function that declares it."""
    E(tid, name, header_of(function), purpose + f" (declared in {function})")


def T(tid, role, name, loc, purpose, conf, overrides=None, recv=r"[\w\]\)]+"):
    names = {m["name"] for m in g.members[tid]}
    assert set(overrides or {}) <= names, (tid, set(overrides) - names)
    g.T(tid, role, name, loc, purpose, conf, D(tid), g.generic_specs(tid, overrides), recv=recv)


# ---------------------------------------------------------------- function-local state machines
S("T00033", "Menu::Inventory::EquipmentDetailsState", "Menu::Inventory::UpdateBladeDetails", "Steps the blade, grip and armor detail screens")
S("T00034", "Menu::Inventory::EquipmentSlotMenuState", "Menu::Inventory::UpdateWeaponMenu", "Steps the weapon, shield and accessory menus")
S("T00035", "Menu::Framework::InitOnlyState", "Menu::Status::UpdateEquipmentDetails", "Has only the initial step of a one-step menu update")
S("T00036", "Menu::Magic::SpellListMenuState", "Menu::Magic::UpdateSorcererMenu", "Steps the Sorcerer and Enchanter spell lists")
S("T00037", "Menu::BreakArts::ArtSelectionState", "Menu::BreakArts::UpdateArtList", "Steps the Break Art and weapon category lists")
S("T00038", "Menu::BattleAbilities::AbilityListState", "Menu::BattleAbilities::UpdateChainAbilityMenu", "Steps the Chain and Defense Ability lists")
S("T00039", "Menu::Inventory::DiscardMenuState", "Menu::Inventory::UpdateDiscardMenu", "Steps the discard menu")
E("T00040", "Save::CardReadinessState", SAVE, "Steps the memory card readiness check (declared in Save::MemoryCard::AdvanceCardReadiness)")
E("T00041", "Save::SaveReadState", SAVE, "Steps a save file read (declared in Save::MemoryCard::ReadSaveStep)")
E("T00042", "Save::SaveWriteState", SAVE, "Steps a save file write and verification (declared in Save::MemoryCard::WriteSaveStep)")
E("T00043", "Save::ResourceInitState", SAVE, "Steps the save menu resource setup (declared in Save::MemoryCard::InitializeMenuResourcesStep)")
S("T00044", "Menu::Files::ConfirmationState", "Menu::Files::UpdateConfirmation", "Steps the yes/no confirmation")
S("T00045", "Menu::Files::FormatPromptState", "Menu::Files::UpdateFormatPrompt", "Steps the format prompt")
S("T00046", "Menu::Files::SaveFilesState", "Menu::Files::UpdateSaveFiles", "Steps the save file list")
S("T00047", "Menu::Files::SaveCardState", "Menu::Files::UpdateSaveCard", "Steps the save card selection")
S("T00048", "Menu::Files::SaveMenuState", "Menu::Files::UpdateDataSaveMenu", "Steps the save menu")
S("T00049", "Menu::Files::LoadFilesState", "Menu::Files::UpdateLoadFiles", "Steps the load file list")
S("T00050", "Menu::Files::LoadCardState", "Menu::Files::UpdateLoadCard", "Steps the load card selection")
S("T00051", "Menu::Files::LoadMenuState", "Menu::Files::UpdateDataLoadMenu", "Steps the load menu")
S("T00052", "Menu::Container::EquipmentDetailsState", "Menu::Container::UpdateWeaponDetails", "Steps the container weapon and blade detail screens")
S("T00053", "Menu::Inventory::MiscItemShortcutState", "Menu::Inventory::UpdateMiscItemShortcut", "Steps the item shortcut menu")
S("T00054", "Menu::Magic::TeleportMenuState", "Menu::Magic::UpdateTeleportMenu", "Steps the teleport menu")
S("T00055", "Menu::Magic::MagicMenuState", "Menu::Magic::Update", "Steps the Magic menu")
S("T00056", "Menu::Magic::WarlockMenuState", "Menu::Magic::UpdateWarlockMenu", "Steps the Warlock spell list")
S("T00057", "Menu::Magic::ShamanMenuState", "Menu::Magic::UpdateShamanMenu", "Steps the Shaman spell list")
S("T00058", "Menu::BreakArts::BreakArtsMenuState", "Menu::BreakArts::Update", "Steps the Break Arts menu")
S("T00059", "Menu::BattleAbilities::AbilityUnlockState", "Menu::BattleAbilities::UpdateAbilityUnlock", "Steps the ability unlock screen")
S("T00060", "Menu::BattleAbilities::UnlockNotificationState", "Menu::BattleAbilities::UpdateUnlockNotifications", "Steps the unlock notifications")
S("T00061", "Menu::BattleAbilities::AssignmentState", "Menu::BattleAbilities::UpdateAssignments", "Steps the ability assignment screen")
S("T00062", "Menu::BattleAbilities::BattleAbilitiesMenuState", "Menu::BattleAbilities::Update", "Steps the Battle Abilities menu")
S("T00063", "Menu::Inventory::WeaponDetailsState", "Menu::Inventory::UpdateWeaponDetails", "Steps the weapon detail screen")
S("T00064", "Menu::Inventory::ShieldDetailsState", "Menu::Inventory::UpdateShieldDetails", "Steps the shield detail screen")
S("T00065", "Menu::Inventory::SortMenuState", "Menu::Inventory::UpdateSortMenu", "Steps the sort menu")
S("T00066", "Menu::Inventory::ItemListState", "Menu::Inventory::UpdateItemList", "Steps the item list")
S("T00067", "Menu::Inventory::EquipmentMenuState", "Menu::Inventory::UpdateEquipmentMenu", "Steps the equipment menu")
S("T00068", "Menu::Inventory::InventoryMenuState", "Menu::Inventory::Update", "Steps the Items menu")
S("T00069", "Menu::Status::ActorSwitchState", "Menu::Status::AnimateActorSwitch", "Steps the actor switch animation")
S("T00070", "Menu::Status::EquipmentListState", "Menu::Status::UpdateEquipmentList", "Steps the status equipment list")
S("T00071", "Menu::Status::StatusMenuState", "Menu::Status::Update", "Steps the Status menu")
S("T00073", "Menu::Map::MapMenuState", "Menu::Map::UpdateAreaSelection", "Steps the Map menu")
S("T00074", "Menu::Map::AreaSelectionState", "Menu::Map::UpdateAreaSelection", "Steps the map area selection")
S("T00075", "Menu::Files::GameOverState", "Menu::Files::UpdateGameOver", "Steps the game over screen")
S("T00076", "Menu::Files::ContainerConfirmationState", "Menu::Files::UpdateContainerConfirmation", "Steps the container save confirmation")
S("T00077", "Menu::Files::DataMenuState", "Menu::Files::UpdateDataMenu", "Steps the Data menu")
S("T00078", "Menu::Rename::NameEditorState", "Menu::Rename::UpdateNameEditor", "Steps the name editor")
g.D("T00035", ["T00072"])

# ---------------------------------------------------------------- other enums
E("T00000", "Menu::Results::BuffReelStat", header_of("Menu::Results::DrawBonusReel"), "Names the stats a bonus reel can raise.")
E("T00001", "Menu::Results::DisFile", header_of("Menu::Results::DrawBonusReel"), "Names the results screen DIS image files.", strip_suffix="Dis")
E("T00002", "Save::FindSaveResult", SAVE, "Names the outcome of looking for the current save on the card.")
E("T00003", "Save::EventMask", SAVE, "Names the memory card event masks.")
E("T00005", "Menu::Status::LimbUiMode", header_of("Menu::Status::Update"), "Names the limb display modes of the status screen.")
E("T00006", "Menu::BattleAbilities::RemainingAbilities", header_of("Menu::BattleAbilities::UpdateAbilityUnlock"),
  "Names which ability kinds remain to be learned.")
E("T00007", "Menu::BattleAbilities::AbilityType", header_of("Menu::BattleAbilities::Update"), "Names the Chain and Defense ability kinds.")
E("T00009", "Menu::Inventory::ItemAction", header_of("Menu::Inventory::UpdateItemList"), "Names the actions of the item list.")
E("T00011", "Menu::Manual::ArrowType", header_of("Menu::Manual::DrawUpperScrollFade"), "Names the scroll arrows.")
E("T00012", "Menu::Framework::AshleyRenderMode", "Native\\Game\\src\\Menu\\Framework\\AshleyPreview.h", "Names the Ashley preview render modes.")
E("T00014", "Menu::Container::CopyContainerFlag", header_of("Menu::Container::UpdateDiscardMenu"), "Names the container copy flag.")
E("T00015", "Menu::Results::DisIndex", header_of("Menu::Results::DrawBonusReel"), "Names the images of the results screen DIS files.")
E("T00017", "Menu::BreakArts::ArtListOption", header_of("Menu::BreakArts::UpdateArtList"), "Names the Break Art list options.")
E("T00020", "Menu::Files::ElementState", header_of("Menu::Files::DrawElement"), "Names a file menu element's animation state.")
E("T00022", "Menu::Loot::LootItemState", header_of("Menu::Loot::AppendLootRow"), "Names a loot row's state and indent.")
E("T00023", "Menu::Magic::MagicSubmenu", header_of("Menu::Magic::Update"), "Names the Magic submenus.")
E("T00024", "Save::MaskedHandlerResult", SAVE, "Names the outcome of a masked card operation.")
E("T00030", "Menu::Magic::ShortcutMagicMenu", header_of("Menu::Magic::Update"), "Names the spell classes reachable by shortcut.")
E("T00032", "Menu::Files::StatType", header_of("Menu::Files::DrawElement"), "Names the HP and MP bars of a file entry.")

# ---------------------------------------------------------------- structs
INPUT = "Native\\Engine\\src\\Input\\Buttons.h"
T("T00746", "State", "Input::PressedButtonsView", INPUT, "Holds the pressed buttons as one word or as two ports of two bytes.", M,
  {"all": ("allButtons", "Pressed buttons for both ports", H, c(("146C.c", "if (vs_main_buttonsPressed.all & PADRdown) {"))),
   "pad": ("ports", "Pressed button byte pairs per port", M, g.decl("T00746", "pad"))})


def same_as_applied(canonical: str, duplicates: list[str]) -> None:
    """Duplicate rows of a type applied by an earlier batch (common.Gen.D needs the canonical in this batch)."""
    trow, fields = g.applied(canonical)
    for tid in duplicates:
        g.TYPES.append([tid, trow["role"], trow["final_name"], trow["final_location"], trow["purpose"], trow["confidence"],
                        f"same-as:{canonical};identical layout indexed through a different include path"])
        for m, f in zip(g.members[tid], fields):
            g.FIELDS.append([m["id"], f["final_name"], f["meaning"], f["confidence"], f"same-as:{canonical}"])


same_as_applied("T00192", ["T00124", "T00187"])
MAP = header_of("Menu::Map::DrawCompass")
T("T00170", "Value", "Menu::Map::CompassBound", MAP, "Gives one compass corner's screen position.", M)
T("T00171", "Value", "Menu::Map::MapLabel", MAP, "Gives one queued map label: position, kind and id.", M)
FILES = header_of("Menu::Files::DrawElement")
T("T00784", "Packet", "Menu::Files::FilePrimitive", FILES, "Is one file menu primitive: sprite, tile, gradient or tile with gradient.", M,
  {"tile_tpage": ("tileTpage", "Tile with a texture page command", M, g.decl("T00784", "tile_tpage")),
   "polyG4_tpage": ("polyG4Tpage", "Gouraud quad with a texture page command", M, g.decl("T00784", "polyG4_tpage"))})
T("T00585", "Packet", "Menu::Files::PrimitiveBuffer", FILES, "Holds one tagged file menu primitive.", L)
g.D("T00585", ["T00586"])
T("T00425", "State", "Menu::Files::FileMenuElement", FILES, "Holds one animated file menu element: state, slot, blend, position, size and text.", M)
T("T00594", "OnDisc", "Save::SaveSlotBase", SAVE, "Holds a save slot's state and generation (the newest generation is the current save).", M)
T("T00596", "OnDisc", "Save::SaveFileSummary", SAVE, "Holds a save file's slot base record, summary stats and an extra word.", M)
SETUP = header_of("Menu::Setup::BuildCombinedBlade")
T("T00172", "OnDisc", "Menu::Setup::CombinedBladeRecord", SETUP, "Gives a combined blade's ids, category, damage and cost types, cost, stats and range.", M,
  {"category": ("category", "Blade category", M, "declaration-only;" + g.decl("T00172", "category"))})
T("T00304", "OnDisc", "Menu::Setup::CombinedArmorRecord", SETUP, "Gives a combined armor or shield's ids, category, gem slots and stats.", M)
T("T00340", "OnDisc", "Menu::Setup::SydHeader", SETUP, "Gives the offsets of the combination, material and initial data tables of a SYD file.", M)
T("T00181", "OnDisc", "Menu::Results::PuzzleRecord", header_of("Menu::Results::DrawBonusReel"), "Gives one cube puzzle's zone, map, par time and rank cap.", M)
T("T00306", "Value", "Menu::Results::BuffReel", header_of("Menu::Results::DrawBonusReel"), "Gives one bonus reel's stat and amount.", M)
MAN = header_of("Menu::Manual::DrawUpperScrollFade")
T("T00196", "State", "Menu::Manual::AnimationState", MAN, "Holds one manual illustration animation: enable, frame mask and duration, repeat and timer.", M)
T("T00317", "Value", "Menu::Manual::BackgroundChunkWidths", MAN, "Gives the widths of the manual background's scroll-fade chunks.", L)
T("T00318", "Value", "Menu::Manual::BackgroundTransparencies", MAN, "Gives the transparencies of the manual background's scroll-fade chunks.", L)
FW = "Native\\Game\\src\\Menu\\Framework\\"
T("T00248", "State", "Menu::Framework::PreviewPoseBytes", FW + "ActorPreview.h", "Views four bytes of an actor record while the preview pose is reset.", L)
T("T00254", "State", "Menu::Framework::PreviewPoseState", FW + "ActorPreview.h", "Holds the actor preview pose state reset when models reload.", L)
T("T00261", "State", "Menu::Framework::MenuMemory", FW + "MenuMemory.h", "Holds remembered menu cursors, item categories and selected equipment.", M)
T("T00313", "Value", "Menu::Framework::ItemText", FW + "MenuRows.h", "Gives an item's name, row flags and description.", M)
T("T00341", "State", "Menu::Framework::TextBoxSelector", FW + "TextBoxes.h", "Holds a text box selector: display order, selected index and count.", M)
T("T00488", "Value", "Menu::TextAnchorWord", FW + "TextBoxes.h", "Gives a text box anchor's packed offset, alignment and flags (as Battle::TextAnchorOffset).", M,
  g.names_from_views("T00488", ["T00619"]))
SCORE = header_of("Menu::Score::Update")
T("T00310", "State", "Menu::Score::Row", SCORE, "Holds one Score menu row: selection, animation, position, title and description.", M)
T("T00319", "State", "Menu::Score::EnemyEntry", SCORE, "Holds one enemy list entry: model, class, kill flags, selection and name.", M)
T("T00349", "OnDisc", "Menu::Score::ZudHeader", SCORE, "Gives a ZUD model file's character, equipment and the offsets of its parts.", M)
T("T00316", "State", "Menu::Loot::LootListItem", header_of("Menu::Loot::AppendLootRow"), "Gives one loot row's category, index and state.", M)
T("T00342", "Value", "Render::TextureRegion", "Native\\Engine\\src\\Render\\Images.h", "Gives a texture region: position, size, page and CLUT.", M)

if __name__ == "__main__":
    g.write()
