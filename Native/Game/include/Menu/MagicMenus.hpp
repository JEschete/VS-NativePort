#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include "Menu/Framework/Prompts.hpp"
#include "Menu/MagicSpellMenu.hpp"
#include "Render/GameFont.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {
enum class School : std::uint8_t { kWarlock, kShaman, kSorcerer, kEnchanter };
inline constexpr std::size_t kSchoolCount = 4;
inline constexpr auto kSpellCounts = std::to_array<std::size_t>({18, 6, 18, 8});

enum class SpellMenuStep : std::uint32_t {
  kInit,
  kInput,
  kReturn,
  kInitRanks,
  kRanks
};

// The original MENU0 image restores these statics on reload. An ordinary
// init call resets only step/shortcut; the owner resets everything on reload.
struct SpellMenuState {
  SpellMenuStep step = SpellMenuStep::kInit;
  std::int32_t selection = 0;
  bool remember_cursor = false;
  AvailableSpellList available;
  std::uint8_t blink = 0;
};

enum class TextTable : std::uint8_t { kMagic, kTeleport, kActionName };
struct TextRef {
  TextTable table = TextTable::kMagic;
  std::uint16_t index = 0;  // Halfword offset, or action ID for kActionName.
  friend bool operator==(const TextRef&, const TextRef&) = default;
};
struct SpellRow {
  TextRef name;
  TextRef description;
  std::int32_t type = 0;
  friend bool operator==(const SpellRow&, const SpellRow&) = default;
};

struct RankCursor {
  std::uint8_t selected = 0;
  std::uint8_t count = 0;
  std::uint8_t scroll = 0;
};
struct RankInput {
  bool quit = false;
  bool back = false;
  bool confirm = false;
  bool right = false;
  bool left = false;
};
enum class MenuSound : std::uint8_t { kLeave, kSelect, kInvalid, kChange };

struct RankTarget {
  std::reference_wrapper<Render::PacketQueue> text;
  std::reference_wrapper<Render::PacketQueue> icons;
  Render::CompactFontTables font;
  std::span<const std::uint8_t> brightness;
};

// Implementations publish each write immediately, rather than committing a
// cached framework snapshot after a nested menu call.
class MenuHost {
 public:
  virtual ~MenuHost() = default;
  virtual void Refuse(std::string_view reason) = 0;
  virtual std::span<const std::uint16_t> MagicStrings() = 0;
  virtual std::span<const std::uint8_t> SpellIds(School school) = 0;
  virtual std::optional<bool> SpellUnlocked(int action) = 0;
  virtual std::optional<std::uint32_t> SpellRestrictions(int action) = 0;
  virtual std::uint32_t ActionRestrictions() = 0;
  virtual std::uint8_t ShortcutInvoked() = 0;
  virtual bool RowsIdle() = 0;
  virtual int SelectedRow() = 0;
  virtual int HighlightedRow() = 0;
  virtual std::uint8_t CursorMemory() = 0;
  virtual void SetCursorMemory(std::uint8_t value) = 0;
  virtual bool CreateRows(int style, std::span<const SpellRow> rows) = 0;
  virtual void FlyOutRows(int row, int offset) = 0;
  virtual void FlyOutRow(int row) = 0;
  virtual void DisplayCost(bool enabled) = 0;
  virtual bool SpellCost(int action) = 0;
  virtual void ClearRows(int except) = 0;
  virtual void DismissInformation() = 0;
  virtual void InitializeInformation() = 0;
  virtual void SetCommand(Framework::MenuCommand command) = 0;
  virtual RankCursor ReadRankCursor() = 0;
  virtual void SetRankCount(std::uint8_t count) = 0;
  virtual void SetRankSelection(std::uint8_t selected) = 0;
  virtual RankInput ReadRankInput() = 0;
  virtual void PlaySound(MenuSound sound) = 0;
  virtual std::optional<RankTarget> RankDrawing() = 0;

  virtual bool SchoolUnlocked(School school) = 0;
  virtual std::uint8_t TeleportAvailability() = 0;
  virtual std::span<const std::uint8_t> SavePointStates() = 0;
  virtual bool AtSavePoint() = 0;
  virtual bool SetSchoolTitle(School school, TextRef text, int width) = 0;
  virtual bool CreateHeader() = 0;
  virtual std::optional<int> RunSchool(School school, unsigned int init) = 0;
  virtual int RunTeleport(int init) = 0;
  virtual void SetActionSelection(std::int32_t action) = 0;
  virtual void SetAbilityType(std::uint8_t type) = 0;
  virtual void SetSelectionKind(std::int16_t kind) = 0;
  virtual std::uint8_t CurrentMenu() = 0;
  virtual void SetCurrentMenu(std::uint8_t menu) = 0;
  virtual void SetReturnMenu(std::uint8_t menu) = 0;
};

// A missing result is an explicit refusal, never a selected spell or exit.
// decomp: warlockMagicMenu (src/MENU/MENU0.PRG/84.c:177; was
// _warlockMagicMenu).
std::optional<int> UpdateWarlockMenu(unsigned int init, SpellMenuState& state,
                                     MenuHost& host);
// decomp: shamanMagicMenu (src/MENU/MENU0.PRG/84.c:525; was _shamanMagicMenu).
std::optional<int> UpdateShamanMenu(unsigned int init, SpellMenuState& state,
                                    MenuHost& host);
// decomp: sorcererMagicMenu (src/MENU/MENU0.PRG/84.c:628; was
// _sorcererMagicMenu).
std::optional<int> UpdateSorcererMenu(unsigned int init, SpellMenuState& state,
                                      MenuHost& host);
// decomp: enchanterMagicMenu (src/MENU/MENU0.PRG/84.c:729; was
// _enchanterMagicMenu).
std::optional<int> UpdateEnchanterMenu(unsigned int init, SpellMenuState& state,
                                       MenuHost& host);
}  // namespace VS::Menu::Magic
