#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/ItemRecords.hpp"
#include "Render/EncodedText.hpp"

// MAINMENU.PRG's item rows (2D10.c, C48.c): the title ranks an item's class
// and affinity totals earn, the name, description and row type an item list
// row shows (the description formatted from the ITEMHELP.BIN templates into a
// caller buffer), the weapon and shield component rows of the detail screens
// and the ITEMNAME/ITEMHELP/MENU12 text load. The functions reach the text,
// the inventory tables, the battle string context and the menu rows through
// an ItemRowsHost (ItemRowsLive.hpp binds it).
namespace VS::Menu::Framework {
struct MenuRow;
struct RowPlacement;

// ITEMNAME.BIN, ITEMHELP.BIN and MENU12.BIN (DiscLayout.h), loaded as one
// block in that order.
inline constexpr std::size_t kItemNamesBytes = 12288;
inline constexpr std::size_t kItemHelpBytes = 28672;
inline constexpr std::size_t kMenu12Bytes = 4096;
inline constexpr std::size_t kItemTextBytes = 45056;
static_assert(kItemTextBytes ==
              kItemNamesBytes + kItemHelpBytes + kMenu12Bytes);
inline constexpr std::size_t kItemNameRowBytes = 24;
// The bytes a row description is formatted into: vs_battle_stringBuf and the
// callers' description buffers.
inline constexpr std::size_t kRowDescriptionBufferBytes = 96;

// ITEMHELP.BIN's halfword offset table entries (Text_Menu_ITEMHELP_BIN_Index_*)
// and the halfword offsets of its fixed texts
// (Text_Menu_ITEMHELP_BIN_Offset_*); ITEMNAME.BIN's rows
// (Text_Menu_ITEMNAME_BIN_Index_*). ItemRowsLive.cpp pins them.
namespace item_text {
inline constexpr int kBattleKnifeDesc = 0;
inline constexpr int kGemFeldspar = 121;
inline constexpr int kCureRootDesc = 183;
inline constexpr int kWarriorEquipment = 373;
inline constexpr int kWeaponBasePrestige = 383;
inline constexpr int kArmorBasePrestige = 392;
inline constexpr int kDagger = 399;
inline constexpr int kBlunt = 409;
inline constexpr int kOneHanded = 412;
inline constexpr int kPhantomPointsDesc = 415;
inline constexpr int kHumanClassDesc = 416;
inline constexpr int kPhysicalAffinityDesc = 422;
inline constexpr int kBluntTypeDesc = 429;
inline constexpr int kRangeDesc = 432;
inline constexpr int kWeaponPrestige = 558;
inline constexpr int kArmorPrestige = 584;
inline constexpr int kRoodNecklace = 610;
inline constexpr int kShortGrip = 641;
inline constexpr int kArmorCategory = 645;
inline constexpr int kAttackStr = 658;
inline constexpr int kDefStr = 661;
inline constexpr std::size_t kArmorBasePrestigeOffset = 9849;
inline constexpr std::size_t kArmorCategoryOffset = 13261;
inline constexpr std::size_t kClassTemplateOffset = 13301;
inline constexpr std::size_t kWeaponDescTemplateOffset = 13307;
inline constexpr std::size_t kGripTemplateOffset = 13317;
inline constexpr std::size_t kArmorTemplateOffset = 13319;
inline constexpr std::size_t kNoneOffset = 13323;
inline constexpr std::size_t kNoGemsOffset = 13326;
inline constexpr std::size_t kEndsSetupOffset = 13335;
inline constexpr int kItem0Name = 0;
inline constexpr int kWoodName = 254;
}  // namespace item_text

// Where a row text points: nowhere (null), an empty text, a byte offset into
// ITEMHELP.BIN or ITEMNAME.BIN, the caller's description buffer or the
// inventory weapon's own name.
enum class TextSource : std::uint8_t {
  kNone,
  kEmpty,
  kHelp,
  kNames,
  kBuffer,
  kWeaponName,
};

struct TextRef {
  TextSource source = TextSource::kNone;
  std::size_t offset = 0;
  friend bool operator==(const TextRef&, const TextRef&) = default;
};

// A row's menuText[0] (name) and menuText[1] (description) and its row type
// (icon in bits 26-31, material in bits 16-18, count in bits 9-15). An
// absent text is one the original left as it was.
struct ItemRowText {
  std::optional<TextRef> name;
  std::optional<TextRef> description;
  std::uint32_t row_type = 0;
};

// The text tables, the battle string context and the stats an inventory
// record assembles into.
class ItemTextHost {
 public:
  virtual ~ItemTextHost() = default;
  virtual void Refuse(std::string_view what) = 0;
  // vs_mainMenu_itemHelp and vs_mainMenu_itemNames; empty before the load.
  [[nodiscard]] virtual std::span<const std::uint8_t> HelpText() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> NameText() = 0;
  // vs_mainMenu_weaponHands: per blade category, the ITEMHELP hand text.
  [[nodiscard]] virtual std::span<const std::uint8_t> WeaponHands() = 0;
  // vs_battle_stringContext: the arguments a template formats with.
  [[nodiscard]] virtual Render::EncodedTextArguments TextArguments() = 0;
  // Points vs_battle_stringContext.strings[slot] at `text`.
  virtual void SetStringArgument(std::size_t slot,
                                 std::span<const std::uint8_t> text) = 0;
  // vs_battle_applyWeapon over Ashley's inventory or the container's
  // _initUiWeapon, by the table vs_mainMenu_weapons points at; nothing when
  // neither applies.
  [[nodiscard]] virtual std::optional<Items::WeaponLoadout>
  InventoryWeaponLoadout(const Items::InventoryWeapon& weapon) = 0;
  [[nodiscard]] virtual std::optional<Items::ShieldLoadout>
  InventoryShieldLoadout(const Items::InventoryShield& shield) = 0;
};

// ItemTextHost plus the item tables the menus show (vs_mainMenu_weapons ..
// vs_mainMenu_armor), vs_battle_stringBuf and the menu rows.
class ItemRowsHost : public ItemTextHost {
 public:
  // The 1-based record, or nothing for 0 or a reference past the table.
  [[nodiscard]] virtual std::optional<Items::InventoryWeapon> Weapon(
      int reference) = 0;
  [[nodiscard]] virtual std::optional<Items::InventoryBlade> Blade(
      int reference) = 0;
  [[nodiscard]] virtual std::optional<Items::InventoryGrip> Grip(
      int reference) = 0;
  [[nodiscard]] virtual std::optional<Items::InventoryShield> Shield(
      int reference) = 0;
  [[nodiscard]] virtual std::optional<Items::InventoryArmor> Armor(
      int reference) = 0;
  [[nodiscard]] virtual std::optional<Items::InventoryGem> Gem(
      int reference) = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> StringBuffer() = 0;
  virtual void DeactivateRow(int row) = 0;
  // SetMenuItem with the text `text` resolves to; null when refused.
  virtual MenuRow* SetMenuItem(const RowPlacement& placement, TextRef text) = 0;
};

// The ITEMNAME/ITEMHELP/MENU12 read: the request a load queued, its
// completion and the block it fills.
class ItemTextLoadHost {
 public:
  virtual ~ItemTextLoadHost() = default;
  virtual void Refuse(std::string_view what) = 0;
  // Allocates the block from the heap's high end, points the three tables
  // into it and queues the read.
  virtual void StartItemTextLoad() = 0;
  // Whether the queued read finished; nothing when no read is queued.
  [[nodiscard]] virtual std::optional<bool> ItemTextLoaded() = 0;
  virtual void ReleaseItemTextRequest() = 0;
  [[nodiscard]] virtual bool ItemTextAllocated() = 0;
  // Frees the block and clears vs_mainMenu_itemNames.
  virtual void FreeItemText() = 0;
};

struct ItemTextLoadState {
  bool loading = false;
};

// The ITEMHELP text at a halfword offset, and the text the offset table's
// entry `index` names; nothing outside the text.
[[nodiscard]] std::optional<TextRef> ItemHelpText(
    std::span<const std::uint8_t> help, std::size_t halfword);
[[nodiscard]] std::optional<TextRef> ItemHelpEntry(
    std::span<const std::uint8_t> help, int index);

// MAINMENU.PRG vs_mainMenu_getEquipmentMaxStats (C48.c:87): over the second
// class (six) and affinity (seven) totals, the greatest (bits 8-15 rank it
// against 50/95), the runner-up (bits 0-7 rank it in steps of ten) and the
// key holding the greatest (bits 16+: the class, or 6 + the affinity).
[[nodiscard]] int EquipmentTitleRanks(
    const Items::ClassAffinityCurrent& totals);

ItemRowText FormatWeaponRow(ItemTextHost& host,
                            const Items::WeaponLoadout& weapon,
                            std::span<std::uint8_t> buffer);
ItemRowText FormatInventoryWeaponRow(ItemTextHost& host,
                                     const Items::InventoryWeapon& weapon,
                                     std::span<std::uint8_t> buffer);
ItemRowText FormatBladeRow(ItemTextHost& host,
                           const Items::InventoryBlade& blade,
                           std::span<std::uint8_t> buffer);
ItemRowText FormatGripRow(ItemTextHost& host, const Items::InventoryGrip& grip,
                          std::span<std::uint8_t> buffer);
ItemRowText FormatShieldRow(ItemTextHost& host,
                            const Items::ShieldLoadout& shield,
                            std::span<std::uint8_t> buffer);
ItemRowText FormatInventoryShieldRow(ItemTextHost& host,
                                     const Items::InventoryShield& shield,
                                     std::span<std::uint8_t> buffer);
ItemRowText FormatArmorRow(ItemTextHost& host, const Items::ArmorLoadout& armor,
                           std::span<std::uint8_t> buffer);
ItemRowText FormatInventoryArmorRow(ItemTextHost& host,
                                    const Items::InventoryArmor& armor,
                                    std::span<std::uint8_t> buffer);
ItemRowText FormatGemRow(ItemTextHost& host, const Items::InventoryGem& gem,
                         std::span<std::uint8_t> buffer);
ItemRowText FormatMiscItemRow(ItemTextHost& host,
                              const Items::InventoryMisc& misc,
                              std::span<std::uint8_t> buffer);

// The weapon detail screen's blade (row 1), grip (row 2) and gem (rows 3-5)
// rows of the 1-based weapon; null when the grip has no such gem slot. A
// slide-in row enters from the right edge.
MenuRow* CreateWeaponComponentRow(ItemRowsHost& host, int row, int weapon_index,
                                  bool slide_in);
// The shield detail screen's gem rows 1-3.
MenuRow* CreateShieldGemRow(ItemRowsHost& host, int row, int shield_index,
                            bool slide_in);

// Starts the text load (`load`) or polls it: 1 once the text is resident.
int LoadItemText(ItemTextLoadState& state, ItemTextLoadHost& host, bool load);
// 1 once the text is freed (or was never loaded); waits for a pending read.
int UnloadItemTextWhenReady(ItemTextLoadState& state, ItemTextLoadHost& host);
}  // namespace VS::Menu::Framework
