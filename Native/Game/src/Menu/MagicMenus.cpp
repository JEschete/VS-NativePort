#include "Menu/MagicMenus.hpp"  // NOLINT(build/include): typed public API.

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Render/Sprites.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Menu::Magic {
namespace {
constexpr std::array<std::uint16_t, kSchoolCount> kDescriptionIndices = {
    12,
    30,
    36,
    54,
};
constexpr std::uint16_t kWarlockNames = 62;
constexpr std::array<std::uint8_t, 7> kRankedSpells = {
    106, 110, 114, 118, 122, 126, 130,
};
constexpr std::array<std::string_view, 5> kRankLabels = {
    "LV1", "LV2", "LV3", "LV4", "--",
};
constexpr int kSchoolStyle = 0x207;
constexpr int kNoRow = 40;
constexpr int kRankColumns = 4;
constexpr int kRankSpacing = 32;
constexpr int kRankTextX = 99;
constexpr int kLockedTextX = 102;
constexpr int kRankTextY = 103;
constexpr int kRankIconX = 96;
constexpr int kRankIconY = 100;
constexpr int kRankHeight = 16;
constexpr int kRankIconU = 192;
constexpr int kRankIconV = 112;
constexpr int kSelectedClutX = 976;
constexpr int kUnselectedClutX = 992;
constexpr int kRankClutY = 223;
constexpr int kArrowX = 80;
constexpr int kArrowU = 80;
constexpr int kArrowV = 48;
constexpr int kArrowSize = 16;
constexpr int kFirstRankRow = 20;
constexpr int kRankRows = 10;
constexpr int kDimBrightness = 64;
constexpr std::uint8_t kBlinkMask = 15;
constexpr std::uint32_t kRankTextClut = 0x37F50000;
constexpr std::uint32_t kTextSemiTransparency = 0x02000000;
constexpr std::uint32_t kRankTextPage = 0xE100002C;
constexpr std::uint32_t kLockedIconMode = 0x64602000;
constexpr std::size_t kCompactGlyphWords = 5;
constexpr std::size_t kCompactModeWords = 2;
constexpr std::size_t kCompactColor = 1;
constexpr std::size_t kCompactTexture = 3;
constexpr std::uint32_t kLowHalf = 0xFFFF;

std::optional<int> Refuse(MenuHost& host, std::string_view reason) {
  host.Refuse(reason);
  return std::nullopt;
}

bool Ranked(int action) {
  return std::ranges::find(kRankedSpells, action) != kRankedSpells.end();
}

void CloseSpellMenu(MenuHost& host) {
  host.ClearRows(kNoRow);
  host.DismissInformation();
  host.SetCommand(Framework::MenuCommand::kNone);
}

bool InitializeRows(School school, SpellMenuState& state, MenuHost& host) {
  const auto school_index = static_cast<std::size_t>(school);
  const auto ids = host.SpellIds(school);
  const auto strings = host.MagicStrings();
  const std::size_t count = kSpellCounts[school_index];
  const auto description = kDescriptionIndices[school_index];
  if (ids.size() < count ||
      strings.size() < static_cast<std::size_t>(description) + count ||
      (school == School::kWarlock && strings.size() < kWarlockNames + count)) {
    host.Refuse("Magic spell or text index table is truncated");
    return false;
  }
  std::array<SpellCandidate, AvailableSpellList::kCapacity> candidates{};
  for (std::size_t i = 0; i < count; ++i) {
    const auto unlocked = host.SpellUnlocked(ids[i]);
    if (!unlocked) {
      host.Refuse("Magic spell action is unavailable");
      return false;
    }
    candidates[i] = {static_cast<std::uint8_t>(i), ids[i], *unlocked};
  }
  const auto available =
      BuildAvailableSpellList(std::span(candidates).first(count));
  if (!available) {
    host.Refuse("Magic spell list exceeds its capacity");
    return false;
  }
  std::array<SpellRow, AvailableSpellList::kCapacity> rows{};
  std::array<int, AvailableSpellList::kCapacity> types{};
  for (std::size_t row = 0; row < available->count; ++row) {
    const auto spell = available->spells[row];
    const auto flags = host.SpellRestrictions(spell.action_id);
    if (!flags) {
      host.Refuse("Magic spell restriction source is unavailable");
      return false;
    }
    rows[row].name = school == School::kWarlock
                         ? TextRef{TextTable::kMagic,
                                   strings[kWarlockNames + spell.source_index]}
                         : TextRef{TextTable::kActionName, spell.action_id};
    rows[row].description = {TextTable::kMagic,
                             strings[description + spell.source_index]};
    types[row] = *flags != 0 ? 1 : 0;
  }
  ApplyGlobalSpellRestrictions(*available, types,
                               std::bit_cast<int>(host.ActionRestrictions()));
  for (std::size_t row = 0; row < available->count; ++row) {
    rows[row].type = types[row];
  }
  const auto previous_cursor = host.CursorMemory();
  if (state.remember_cursor) {
    host.SetCursorMemory(1);
  }
  const bool created = host.CreateRows(kSchoolStyle + static_cast<int>(school),
                                       std::span(rows).first(available->count));
  host.SetCursorMemory(previous_cursor);
  if (!created) {
    return false;
  }
  // Retain unused entries as the original statics do, but never index them.
  std::copy_n(available->spells.begin(), available->count,
              state.available.spells.begin());
  state.available.count = available->count;
  state.step = SpellMenuStep::kInput;
  return true;
}

std::optional<int> AvailableAction(int row, const SpellMenuState& state,
                                   MenuHost& host) {
  if (row < 0 || state.available.count > state.available.spells.size() ||
      static_cast<std::size_t>(row) >= state.available.count) {
    return Refuse(host, "Magic selected or highlighted row is not populated");
  }
  return state.available.spells[static_cast<std::size_t>(row)].action_id;
}

std::optional<int> HandleInput(School school, SpellMenuState& state,
                               MenuHost& host) {
  // ADDIU wraps all 32 bits before the original signed branch.
  const int selection = std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(host.SelectedRow()) + 1U);
  if (selection == 0) {
    state.selection = 0;
    auto action = AvailableAction(host.HighlightedRow(), state, host);
    if (!action) {
      return std::nullopt;
    }
    if (school == School::kWarlock && Ranked(*action)) {
      const auto unlocked = host.SpellUnlocked(*action + 1);
      if (!unlocked) {
        return Refuse(host, "Magic next spell rank is unavailable");
      }
      if (*unlocked) {
        *action = -*action;
      }
    }
    return host.SpellCost(*action) ? std::optional<int>{0} : std::nullopt;
  }
  std::optional<int> action;
  bool ranks = false;
  if (selection > 0) {
    action = AvailableAction(selection - 1, state, host);
    if (!action) {
      return std::nullopt;
    }
    if (school == School::kWarlock && Ranked(*action)) {
      const auto unlocked = host.SpellUnlocked(*action + 1);
      if (!unlocked) {
        return Refuse(host, "Magic next spell rank is unavailable");
      }
      ranks = *unlocked;
    }
  }
  state.selection = selection;
  host.DisplayCost(false);
  if (host.ShortcutInvoked() != 0 && state.selection == -1) {
    state.selection = -2;
  }
  if (state.selection == -1) {
    host.ClearRows(0);
  } else {
    if (action) {
      state.selection = *action;
    }
    if (ranks) {
      host.FlyOutRows(host.ReadRankCursor().scroll, 2);
      host.DisplayCost(true);
      state.step = SpellMenuStep::kInitRanks;
      return 0;
    }
    CloseSpellMenu(host);
  }
  state.step = SpellMenuStep::kReturn;
  return 0;
}

std::optional<int> InitializeRanks(SpellMenuState& state, MenuHost& host) {
  if (!host.RowsIdle()) {
    return 0;
  }
  if (!Ranked(state.selection)) {
    return Refuse(host, "Magic rank selection has no ranked base spell");
  }
  std::uint8_t count = 2;
  const auto third = host.SpellUnlocked(state.selection + 2);
  if (!third) {
    return Refuse(host, "Magic third spell rank is unavailable");
  }
  if (*third) {
    const auto fourth = host.SpellUnlocked(state.selection + kRankColumns - 1);
    if (!fourth) {
      return Refuse(host, "Magic fourth spell rank is unavailable");
    }
    count =
        static_cast<std::uint8_t>(*fourth ? kRankColumns : kRankColumns - 1);
  }
  const auto cursor = host.ReadRankCursor();
  host.SetRankCount(count);
  if (cursor.selected >= count) {
    host.SetRankSelection(static_cast<std::uint8_t>(count - 1U));
  }
  state.blink = 0;
  state.step = SpellMenuStep::kRanks;
  return 0;
}

// Style emitted glyphs, not words behind a later allocation. The native
// compact font deliberately has a different physical packet layout (#413).
class RankTextQueue final : public Render::PacketQueue {
 public:
  explicit RankTextQueue(Render::PacketQueue& target) : target_(target) {}
  std::uint32_t ChainLink() const override { return target_.ChainLink(); }
  bool Emit(std::span<const std::uint32_t> packet) override {
    if (packet.size() == kCompactGlyphWords) {
      std::array<std::uint32_t, kCompactGlyphWords> glyph{};
      std::ranges::copy(packet, glyph.begin());
      glyph[kCompactColor] |= kTextSemiTransparency;
      glyph[kCompactTexture] =
          (glyph[kCompactTexture] & kLowHalf) | kRankTextClut;
      return target_.Emit(glyph);
    }
    if (packet.size() == kCompactModeWords) {
      const std::array mode{packet[0], kRankTextPage};
      return target_.Emit(mode);
    }
    return false;
  }

 private:
  Render::PacketQueue& target_;
};

bool DrawRankColumn(RankTarget& target, int level, int selected, int count,
                    std::uint8_t& blink) {
  const bool locked = level > selected && level >= count;
  RankTextQueue text(target.text);
  const auto label = kRankLabels[locked ? kRankColumns : level];
  if (!Render::DrawCompactText(
          text, target.font, label,
          Render::PackScreenWord(
              (locked ? kLockedTextX : kRankTextX) + level * kRankSpacing,
              kRankTextY),
          Render::kCompactTextGray)) {
    return false;
  }
  const int brightness = level <= selected ? target.brightness[blink]
                                           : (locked ? 0 : kDimBrightness);
  auto icon = Render::BuildSpritePacket(
      {.draw_mode = locked ? kLockedIconMode : Render::kDefaultSpriteDrawMode,
       .brightness_transparency = brightness,
       .packed_position = Render::PackScreenWord(
           kRankIconX + level * kRankSpacing, kRankIconY),
       .packed_size = Render::PackScreenWord(kRankSpacing, kRankHeight)},
      target.icons.get().ChainLink());
  icon[Render::kSpriteTextureWord] =
      static_cast<std::uint32_t>(Render::PackUvClutWord(
          kRankIconU, kRankIconV,
          level <= selected ? kSelectedClutX : kUnselectedClutX, kRankClutY));
  if (!target.icons.get().Emit(icon)) {
    return false;
  }
  if (level <= selected) {
    blink = static_cast<std::uint8_t>((blink + (level == selected ? 1U : 0U)) &
                                      kBlinkMask);
  }
  return true;
}

bool DrawRankArrows(Render::PacketQueue& queue) {
  auto left = Render::BuildSpritePacket(
      {.draw_mode = Render::kDefaultSpriteDrawMode,
       .brightness_transparency = Render::kDefaultSpriteBrightness,
       .packed_position = Render::PackScreenWord(kArrowX, kRankIconY),
       .packed_size = Render::PackScreenWord(kArrowSize, kArrowSize)},
      queue.ChainLink());
  left[Render::kSpriteTextureWord] = static_cast<std::uint32_t>(
      Render::PackUvClutWord(kArrowU, kArrowV, kSelectedClutX, kRankClutY));
  if (!queue.Emit(left)) {
    return false;
  }
  const std::array<std::uint32_t, 10> right{
      Render::PackTagWord(9, queue.ChainLink()),
      0x2C808080,
      0x006400E0,
      0x37FD305F,
      0x006400F0,
      0x000C304F,
      0x007400E0,
      0x405F,
      0x007400F0,
      0x404F};
  return queue.Emit(right);
}

std::optional<int> HandleRanks(SpellMenuState& state, MenuHost& host) {
  auto cursor = host.ReadRankCursor();
  const auto input = host.ReadRankInput();
  if (input.quit) {
    host.PlaySound(MenuSound::kLeave);
    host.DisplayCost(false);
    CloseSpellMenu(host);
    state.selection = -2;
    state.step = SpellMenuStep::kReturn;
    return 0;
  }
  if (input.back) {
    host.PlaySound(MenuSound::kLeave);
    for (int row = kFirstRankRow; row < kFirstRankRow + kRankRows; ++row) {
      host.FlyOutRow(row);
    }
    state.step = SpellMenuStep::kInit;
    return 0;
  }
  if (!Ranked(state.selection) || cursor.count < 2 ||
      cursor.count > kRankColumns || cursor.selected >= cursor.count ||
      state.blink > kBlinkMask) {
    return Refuse(host, "Magic rank cursor or base spell is invalid");
  }
  if (input.confirm) {
    const auto flags =
        host.SpellRestrictions(state.selection + cursor.selected);
    if (!flags) {
      return Refuse(host, "Magic rank restriction source is unavailable");
    }
    if (*flags == 0) {
      host.PlaySound(MenuSound::kSelect);
      host.DisplayCost(false);
      CloseSpellMenu(host);
      state.step = SpellMenuStep::kReturn;
      state.selection += cursor.selected;
      return 0;
    }
    host.PlaySound(MenuSound::kInvalid);
  }
  if (input.right && cursor.selected < cursor.count - 1) {
    ++cursor.selected;
  }
  if (input.left && cursor.selected > 0) {
    --cursor.selected;
  }
  if (cursor.selected != host.ReadRankCursor().selected) {
    host.PlaySound(MenuSound::kChange);
    host.SetRankSelection(cursor.selected);
  }
  if (!host.SpellCost(state.selection + cursor.selected)) {
    return std::nullopt;
  }
  auto target = host.RankDrawing();
  constexpr std::size_t kRequiredWidths = 28;
  constexpr std::size_t kRequiredTrims = 77;
  constexpr std::size_t kRequiredIndices = 55;
  if (!target || target->brightness.size() <= kBlinkMask ||
      target->font.widths.size() < kRequiredWidths ||
      target->font.left_trim.size() < kRequiredTrims ||
      target->font.indices.size() < kRequiredIndices) {
    return Refuse(host, "Magic rank drawing target or font tables are missing");
  }
  for (int level = 0; level < kRankColumns; ++level) {
    if (!DrawRankColumn(*target, level, cursor.selected, cursor.count,
                        state.blink)) {
      return Refuse(host, "Magic rank text or icon packet was refused");
    }
  }
  if (!DrawRankArrows(target->icons)) {
    return Refuse(host, "Magic rank arrow packet was refused");
  }
  return 0;
}

std::optional<int> UpdateSchool(School school, unsigned int init,
                                SpellMenuState& state, MenuHost& host) {
  if (init != 0) {
    state.remember_cursor = init == 2;
    host.FlyOutRows(static_cast<int>(school), 1);
    state.step = SpellMenuStep::kInit;
    return 0;
  }
  switch (state.step) {
    case SpellMenuStep::kInit:
      if (host.ShortcutInvoked() == 0 && !host.RowsIdle()) {
        return 0;
      }
      return InitializeRows(school, state, host) ? std::optional<int>{0}
                                                 : std::nullopt;
    case SpellMenuStep::kInput:
      return HandleInput(school, state, host);
    case SpellMenuStep::kReturn:
      return host.RowsIdle() ? state.selection : 0;
    case SpellMenuStep::kInitRanks:
      if (school == School::kWarlock) {
        return InitializeRanks(state, host);
      }
      break;
    case SpellMenuStep::kRanks:
      if (school == School::kWarlock) {
        return HandleRanks(state, host);
      }
      break;
  }
  return Refuse(host, "Magic school state is invalid");
}
}  // namespace

// decomp: warlockMagicMenu (src/MENU/MENU0.PRG/84.c:177; was
// _warlockMagicMenu).
std::optional<int> UpdateWarlockMenu(unsigned int init, SpellMenuState& state,
                                     MenuHost& host) {
  return UpdateSchool(School::kWarlock, init, state, host);
}
// decomp: shamanMagicMenu (src/MENU/MENU0.PRG/84.c:525; was _shamanMagicMenu).
std::optional<int> UpdateShamanMenu(unsigned int init, SpellMenuState& state,
                                    MenuHost& host) {
  return UpdateSchool(School::kShaman, init, state, host);
}
// decomp: sorcererMagicMenu (src/MENU/MENU0.PRG/84.c:628; was
// _sorcererMagicMenu).
std::optional<int> UpdateSorcererMenu(unsigned int init, SpellMenuState& state,
                                      MenuHost& host) {
  return UpdateSchool(School::kSorcerer, init, state, host);
}
// decomp: enchanterMagicMenu (src/MENU/MENU0.PRG/84.c:729; was
// _enchanterMagicMenu).
std::optional<int> UpdateEnchanterMenu(unsigned int init, SpellMenuState& state,
                                       MenuHost& host) {
  return UpdateSchool(School::kEnchanter, init, state, host);
}
}  // namespace VS::Menu::Magic
