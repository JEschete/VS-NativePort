#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

// MENU0.PRG MP cost text, teleport cost policy, teleport destination menu and
// Magic header (src/MENU/MENU0.PRG/84.c, 15AC.c). The typed cores run over a
// Host; MagicCostsLive.cpp binds them to the game state.
// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {
inline constexpr std::size_t kSavePointCount = 48;
inline constexpr std::size_t kActionCount = 256;
inline constexpr int kTeleportBaseMpCost = 15;
// The largest teleport cost the cost text formats; the menu stores costs as
// halfwords.
inline constexpr int kMaxFormattedMpCost = 0xFFFF;

// One _savePointData word (MENU0.PRG 84.c:95) decoded. The room id is the low
// halfword the cost lookup matches against the current room; the destination
// fields come from the word plus one, which the teleport result carries.
struct SavePoint {
  std::uint16_t room_id = 0;
  // Bits 0..8 of word + 1 (the room id plus one).
  std::uint16_t location = 0;
  // Bits 16..20 of word + 1.
  std::uint8_t tile_x = 0;
  // Bits 24..28 of word + 1.
  std::uint8_t tile_z = 0;
  friend bool operator==(const SavePoint&, const SavePoint&) = default;
};

using SavePointTable = std::array<SavePoint, kSavePointCount>;

[[nodiscard]] SavePoint DecodeSavePoint(std::uint32_t word) noexcept;
// The table the original module carries (_savePointData).
[[nodiscard]] const SavePointTable& OriginalSavePoints() noexcept;

// A confirmed teleport destination.
struct TeleportDestination {
  std::uint8_t save_point = 0;
  std::uint16_t location = 0;
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::uint16_t mp_cost = 0;
  friend bool operator==(const TeleportDestination&,
                         const TeleportDestination&) = default;
};

// The legacy result word vs_menu0_exec expects: location in bits 0..8, tile x
// in 9..13, tile z in 14..18 and the MP cost from bit 19.
[[nodiscard]] std::int32_t PackTeleportDestination(
    const TeleportDestination& destination) noexcept;
// The word read back as the battle flow reads it (D_800F18EC_t's bitfields);
// the save point is not packed.
[[nodiscard]] TeleportDestination UnpackTeleportDestination(
    std::int32_t packed) noexcept;

// A teleport row: halfword offsets into the teleportation strings and the
// framework row type (bit 0 disabled, bit 2 the current save point).
struct TeleportRow {
  std::uint16_t name_offset = 0;
  std::uint16_t description_offset = 0;
  std::int32_t type = 0;
  friend bool operator==(const TeleportRow&, const TeleportRow&) = default;
};

// vs_battle_setMenuItem's arguments plus the fields the caller sets on the
// returned row.
struct MenuItem {
  int row = 0;
  int x = 0;
  int y = 0;
  int width = 0;
  int background_width = 0;
  // Halfword offset of the label in the battle menu strings.
  std::uint16_t text_offset = 0;
  std::uint8_t state = 0;
  std::int16_t target_position = 0;
  std::uint8_t selected = 0;
  friend bool operator==(const MenuItem&, const MenuItem&) = default;
};

enum class TeleportStep : std::uint8_t {
  kInit,
  kHandleInput,
  kReturnIfReady,
};

// _teleportMenu's statics. The row arrays keep earlier entries past the
// current row count, as the module statics did.
struct TeleportMenuState {
  TeleportStep step = TeleportStep::kInit;
  std::int32_t selected_row = 0;
  std::int32_t current_mp = 0;
  std::array<std::uint8_t, kSavePointCount> save_points{};
  std::array<std::uint16_t, kSavePointCount> mp_costs{};
};

// selection is 0 while pending, the 1-based confirmed row, or the framework's
// negative back (-1) and quit (-2) sentinels. A confirmed row carries its
// destination.
struct TeleportMenuResult {
  std::int32_t selection = 0;
  std::optional<TeleportDestination> destination;
};

class Host {
 public:
  virtual ~Host() = default;
  virtual void Refuse(std::string_view reason) = 0;

  // Cost text.
  [[nodiscard]] virtual std::optional<std::uint8_t> ActionCost(
      int action_id) = 0;
  [[nodiscard]] virtual std::optional<std::uint32_t> ActionRestrictionFlags(
      int action_id) = 0;
  virtual void SetActionCostText(int index, std::string_view text, int x,
                                 int disabled) = 0;

  // Teleport destinations.
  [[nodiscard]] virtual int CurrentRoomId() = 0;
  [[nodiscard]] virtual const SavePointTable& SavePoints() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> SavePointStates() = 0;
  [[nodiscard]] virtual std::optional<std::int16_t> CurrentMp() = 0;
  [[nodiscard]] virtual std::span<const std::uint16_t>
  TeleportationStrings() = 0;
  [[nodiscard]] virtual std::uint16_t LocationsIndex() const = 0;
  [[nodiscard]] virtual std::uint16_t DisabledDescriptionOffset() const = 0;

  // Menu framework.
  virtual void FlyOutRowsAndHoistSelection(int row, int offset) = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  [[nodiscard]] virtual std::uint32_t ActionRestrictions() = 0;
  virtual void SetTeleportCursorMemory(std::uint8_t row,
                                       std::uint8_t scroll) = 0;
  [[nodiscard]] virtual std::uint8_t CursorMemorySetting() = 0;
  virtual void SetCursorMemorySetting(std::uint8_t value) = 0;
  virtual void CreateRows(int row_info, std::span<const TeleportRow> rows) = 0;
  [[nodiscard]] virtual int SelectedRow() = 0;
  [[nodiscard]] virtual int HighlightedRow() = 0;
  virtual void SetDisplayActionCost(int enabled) = 0;
  virtual void ClearRowsExcept(int row) = 0;
  virtual void DismissInformationBox() = 0;
  virtual void SetCommandLabel(int command) = 0;

  // Header.
  [[nodiscard]] virtual std::span<const std::uint16_t> BattleMenuStrings() = 0;
  [[nodiscard]] virtual std::uint16_t MagicHeaderIndex() const = 0;
  virtual void SetMenuItem(const MenuItem& item) = 0;
  virtual void InitializeInformationBox() = 0;
};

// MENU0.PRG _setMPCost (src/MENU/MENU0.PRG/84.c:18). A negative id marks a
// variable-cost spell.
void SetSpellMpCost(int signed_action_id, Host& host);
// MENU0.PRG _setMPCostDirect (src/MENU/MENU0.PRG/84.c:64).
void SetTeleportMpCost(int mp_cost, int disabled, Host& host);
// MENU0.PRG _getTeleportCost (src/MENU/MENU0.PRG/84.c:117). Empty for a
// target outside 0..kSavePointCount.
[[nodiscard]] std::optional<int> TeleportMpCost(
    int target_save_point, int current_room_id,
    const SavePointTable& save_points);
// MENU0.PRG _teleportMenu (src/MENU/MENU0.PRG/15AC.c:22).
TeleportMenuResult UpdateTeleportMenu(int init, TeleportMenuState& state,
                                      Host& host);
// MENU0.PRG _setMenuHeader (src/MENU/MENU0.PRG/15AC.c:148).
void CreateHeader(Host& host);
}  // namespace VS::Menu::Magic
