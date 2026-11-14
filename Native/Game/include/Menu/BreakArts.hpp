#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace VS::Menu::break_arts {
inline constexpr std::size_t kArtCount = 4;
inline constexpr std::size_t kCategoryCount = 10;
inline constexpr std::size_t kMaxArtRows = kArtCount + 1;
inline constexpr std::size_t kMenuStringHalfwords = 2151;

enum class TextKind : std::uint8_t {
  kMenuOffset,
  kActionName,
  kBattleMenuIndex,
};

struct TextRef {
  TextKind kind = TextKind::kMenuOffset;
  std::uint16_t value = 0;
  friend bool operator==(const TextRef&, const TextRef&) = default;
};

struct Row {
  TextRef name;
  TextRef description;
  std::int32_t type = 0;
};

struct RowPlacement {
  int row = 0;
  int x = 0;
  int y = 0;
  int width = 0;
  int background_width = 0;
  int icon = -1;
};

enum class ArtListStep : std::uint8_t {
  kInit,
  kWaitForSelection,
  kReturnSelection,
};

enum class CategoryStep : std::uint8_t {
  kInit,
  kWaitForSelection,
  kReturnSelection,
};

enum class ScreenStep : std::uint8_t {
  kNone = 0,
  kInit = 3,
  kArtsInit,
  kArtsInitWithTitle,
  kArtsMenu,
  kWeaponCategoryMenu,
  kExitToMainMenu,
  kExecuteArt,
  kExit,
};

struct ArtListState {
  ArtListStep step = ArtListStep::kInit;
  std::int32_t selected_row = 0;
  std::int32_t force_cursor_memory = 0;
  std::int32_t weapon_type = 0;
  bool is_last_row = false;
  std::uint8_t animation_index = 0;
  std::uint8_t row_count = 0;
  std::uint8_t remaining_rows = 0;
  std::array<std::uint16_t, kMaxArtRows> action_ids{};
};

struct CategoryState {
  CategoryStep step = CategoryStep::kInit;
  std::int32_t selected_row = 0;
};

struct State {
  ArtListState art_list;
  CategoryState categories;
  std::int32_t update_weapon_type = 0;
};

class Host {
 public:
  virtual ~Host() = default;
  virtual void Refuse(std::string_view reason) = 0;

  [[nodiscard]] virtual std::span<const std::uint16_t> MenuStrings() = 0;
  [[nodiscard]] virtual std::span<const std::uint16_t> BattleMenuStrings() = 0;
  [[nodiscard]] virtual std::uint16_t DaggerArtDescriptionIndex() const = 0;
  [[nodiscard]] virtual std::uint16_t ViewArtsNameOffset() const = 0;
  [[nodiscard]] virtual std::uint16_t ViewArtsDescriptionOffset() const = 0;
  [[nodiscard]] virtual std::uint16_t BreakArtsHeaderIndex() const = 0;
  [[nodiscard]] virtual std::int32_t DaggerArtOneId() const = 0;

  [[nodiscard]] virtual std::optional<std::uint8_t> ActionCost(
      int action_id) = 0;
  [[nodiscard]] virtual std::optional<std::uint32_t> ActionRestrictionFlags(
      int action_id) = 0;
  [[nodiscard]] virtual std::optional<bool> ActionUnlocked(int action_id) = 0;
  [[nodiscard]] virtual bool ActionNameAvailable(int action_id) = 0;
  [[nodiscard]] virtual std::optional<int> EquippedWeaponCategory() = 0;
  [[nodiscard]] virtual std::optional<int> ArtsLearned(int category) = 0;
  [[nodiscard]] virtual std::optional<int> CategoryKills(int category) = 0;
  [[nodiscard]] virtual std::optional<int> PointRequirement(int category,
                                                            int art_index) = 0;

  [[nodiscard]] virtual std::uint8_t CursorMemory() = 0;
  virtual void SetCursorMemory(std::uint8_t value) = 0;
  [[nodiscard]] virtual std::uint32_t ActionRestrictions() = 0;
  [[nodiscard]] virtual bool ShortcutInvoked() = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  [[nodiscard]] virtual int SelectedRow() = 0;
  [[nodiscard]] virtual int HighlightedRow() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> AnimationIndices() = 0;
  [[nodiscard]] virtual std::span<const std::int8_t> RowAnimationSteps() = 0;

  virtual bool SetMenuItem(const RowPlacement& placement, TextRef text) = 0;
  virtual bool CreateRows(int row_info, std::span<const Row> rows) = 0;
  virtual void SetActionCostText(int index, std::string_view text, int x,
                                 int disabled) = 0;
  virtual void DrawCompactText(std::string_view text,
                               std::uint32_t position) = 0;
  virtual void SetDisplayActionCost(int enabled) = 0;
  virtual void ClearRowsExcept(int row) = 0;
  virtual void DismissInformationBox() = 0;
  virtual void InitializeInformationBox() = 0;
  virtual void SetCommandLabel(int command) = 0;
  virtual void SetActionSelection(std::uint16_t action_id) = 0;
  virtual void SetExecuteAbilityType(std::uint8_t type) = 0;
  [[nodiscard]] virtual std::uint8_t CurrentMenu() = 0;
  virtual void SetCurrentMenu(std::uint8_t menu) = 0;
  virtual void SetReturnState(std::uint8_t state) = 0;
  virtual void SetSelectionKind(std::int16_t kind) = 0;
};

struct Context {
  State& state;
  Host& host;
};

// MENU1.PRG _setHPCost (src/MENU/MENU1.PRG/30.c:13).
void SetHpCost(int action_id, Host& host);
// MENU1.PRG _setNextArtUnlockPointsRemaining (src/MENU/MENU1.PRG/30.c:45).
void DrawNextUnlockPoints(int x_offset, int weapon_category, int row,
                          Host& host);
// MENU1.PRG _weaponArtsMenu (src/MENU/MENU1.PRG/30.c:104).
int UpdateArtList(int type_cursor_memory, Context& context);
// MENU1.PRG _weaponCategoriesMenu (src/MENU/MENU1.PRG/30.c:266).
int UpdateWeaponCategories(int initialize, Context& context);
// MENU1.PRG _setMenuHeader (src/MENU/MENU1.PRG/30.c:344).
void CreateHeader(Host& host);
// MENU1.PRG vs_menu1_exec (src/MENU/MENU1.PRG/30.c:358).
int Update(Context& context, std::uint8_t& step);
}  // namespace VS::Menu::break_arts
