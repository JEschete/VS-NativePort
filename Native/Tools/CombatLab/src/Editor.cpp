#include "CombatLab/Editor.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace VS::combat_lab {
namespace {
enum Row {
  kZone,
  kRoom,
  kEnemy,
  kQuantity,
  kPlayerX,
  kPlayerZ,
  kElevation,
  kOffsetX,
  kOffsetZ,
  kSpacing,
  kSeed,
  kAshleyProfile,
  kAshleyStats,
  kAddGroup,
  kClearGroups,
  kStart,
  kSave,
  kRowCount
};
constexpr int kFirstRowY = 65;
constexpr int kRowHeight = 18;
constexpr int kPreferredZone = 9;
constexpr int kMaxTile = Game::EncounterLimits::kTiles - 1;
constexpr int kMaxSpacing = Game::EncounterLimits::kTiles / 2;
constexpr int kLabelX = 18;
constexpr int kTitleY = 22;
constexpr int kSubtitleY = 40;
constexpr int kSelectionX = 12;
constexpr int kSelectionOffset = 3;
constexpr int kSelectionWidth = 360;
constexpr int kSelectionHeight = 17;
constexpr int kPreviewX = 390;
constexpr int kLegendY = 83;
constexpr int kLayoutLabelY = 101;
constexpr int kValidationY = 376;
constexpr int kWarningY = 394;
constexpr int kMessageY = 412;
constexpr int kEditorHelpY = 436;
constexpr int kCombatHelpY = 453;
constexpr std::uint32_t kHighlight = 0xFF514635;
constexpr std::uint32_t kWarning = 0xFF7CA4EF;
constexpr std::uint32_t kPlayerMarker = 0xFFF0B060;
constexpr std::uint32_t kEnemyMarker = 0xFF6666EE;
constexpr std::uint32_t kSupportedFloor = 0xFF365437;

struct StatField {
  const char* label;
  std::optional<int> Game::EncounterStatOverrides::* member;
};
constexpr auto kStats = std::to_array<StatField>({
    {"MAX / STARTING HP", &Game::EncounterStatOverrides::max_hp},
    {"MAX / STARTING MP", &Game::EncounterStatOverrides::max_mp},
    {"BASE STRENGTH", &Game::EncounterStatOverrides::strength},
    {"BASE INTELLIGENCE", &Game::EncounterStatOverrides::intelligence},
    {"BASE AGILITY", &Game::EncounterStatOverrides::agility},
});
constexpr int kResetStats = static_cast<int>(kStats.size());
constexpr int kBackFromStats = kResetStats + 1;
constexpr int kStatRows = kBackFromStats + 1;

std::size_t Cycle(std::size_t index, std::size_t count, int direction) {
  if (count == 0) return 0;
  return (index + count + direction) % count;
}
}  // namespace

Editor::Editor(std::vector<Game::EncounterZone> catalog)
    : catalog_(std::move(catalog)) {
  std::erase_if(catalog_, [](const auto& zone) { return zone.rooms.empty(); });
  if (catalog_.empty()) throw std::runtime_error("no selectable rooms");
  const auto preferred =
      std::ranges::find(catalog_, kPreferredZone, &Game::EncounterZone::id);
  if (preferred != catalog_.end()) {
    zone_index_ = static_cast<std::size_t>(preferred - catalog_.begin());
  }
  ResetRoom();
}

const Game::EncounterZone& Editor::Zone() const {
  return catalog_[zone_index_];
}

void Editor::ResetRoom() {
  const auto& room = Zone().rooms[room_index_];
  setup_.zone = Zone().id;
  setup_.room = room.index;
  setup_.player = room.entrance;
  setup_.enemies.clear();
  message_.clear();
}

Game::EncounterSetup Editor::Setup() const {
  auto setup = setup_;
  if (setup.enemies.empty() && !Zone().enemies.empty()) {
    for (int index = 0; index < quantity_; ++index) {
      auto position = setup.player;
      position.x += offset_x_ + index * spacing_;
      position.z += offset_z_;
      setup.enemies.push_back({Zone().enemies[enemy_index_].index, position});
    }
  }
  return setup;
}

void Editor::Load(const Game::EncounterSetup& setup) {
  const auto zone =
      std::ranges::find(catalog_, setup.zone, &Game::EncounterZone::id);
  if (zone == catalog_.end()) {
    throw std::runtime_error("preset zone is missing");
  }
  const auto room =
      std::ranges::find(zone->rooms, setup.room, &Game::EncounterRoom::index);
  if (room == zone->rooms.end()) {
    throw std::runtime_error("preset room is missing");
  }
  zone_index_ = static_cast<std::size_t>(zone - catalog_.begin());
  room_index_ = static_cast<std::size_t>(room - zone->rooms.begin());
  enemy_index_ = 0;
  quantity_ = setup.enemies.empty() ? 0 : 1;
  setup_ = setup;
  stats_open_ = false;
}

void Editor::AddGroup() {
  if (quantity_ == 0) {
    message_ = "QUANTITY 0 - NO GROUP ADDED";
    return;
  }
  if (Zone().enemies.empty()) return;
  if (setup_.enemies.size() + static_cast<std::size_t>(quantity_) >
      Game::EncounterLimits::kEnemies) {
    message_ = "PRIMARY ACTOR BUDGET EXCEEDED";
    return;
  }
  for (int index = 0; index < quantity_; ++index) {
    auto position = setup_.player;
    position.x += offset_x_ + index * spacing_;
    position.z += offset_z_;
    setup_.enemies.push_back({Zone().enemies[enemy_index_].index, position});
  }
  message_ = "GROUP ADDED - CHANGE OFFSETS BEFORE ADDING ANOTHER";
}

void Editor::Change(int direction) {
  switch (row_) {
    case kZone:
      zone_index_ = Cycle(zone_index_, catalog_.size(), direction);
      room_index_ = enemy_index_ = 0;
      ResetRoom();
      break;
    case kRoom:
      room_index_ = Cycle(room_index_, Zone().rooms.size(), direction);
      ResetRoom();
      break;
    case kEnemy:
      enemy_index_ = Cycle(enemy_index_, Zone().enemies.size(), direction);
      break;
    case kQuantity:
      quantity_ =
          std::clamp(quantity_ + direction, 0, Game::EncounterLimits::kEnemies);
      break;
    case kPlayerX:
      setup_.player.x = std::clamp(setup_.player.x + direction, 0, kMaxTile);
      break;
    case kPlayerZ:
      setup_.player.z = std::clamp(setup_.player.z + direction, 0, kMaxTile);
      break;
    case kElevation:
      setup_.player.elevation =
          std::clamp(setup_.player.elevation + direction, 0,
                     Game::EncounterLimits::kElevations - 1);
      break;
    case kOffsetX:
      offset_x_ = std::clamp(offset_x_ + direction, -kMaxTile, kMaxTile);
      break;
    case kOffsetZ:
      offset_z_ = std::clamp(offset_z_ + direction, -kMaxTile, kMaxTile);
      break;
    case kSpacing:
      spacing_ = std::clamp(spacing_ + direction, 1, kMaxSpacing);
      break;
    case kSeed:
      setup_.seed += static_cast<std::uint32_t>(direction);
      break;
    case kAshleyProfile:
      setup_.profile = static_cast<Game::EncounterProfile>(Cycle(
          static_cast<std::size_t>(setup_.profile),
          static_cast<std::size_t>(Game::EncounterProfile::kCount), direction));
      break;
    default:
      break;
  }
}

Editor::Action Editor::Update(const Input::PadState& pad, bool confirm) {
  if (stats_open_) {
    UpdateStats(pad, confirm);
    return Action::kNone;
  }
  const auto buttons = pad.repeat | pad.pressed;
  if ((buttons & Input::Up) != 0) row_ = (row_ + kRowCount - 1) % kRowCount;
  if ((buttons & Input::Down) != 0) row_ = (row_ + 1) % kRowCount;
  if ((buttons & Input::Left) != 0) Change(-1);
  if ((buttons & Input::Right) != 0) Change(1);
  if (!confirm && (pad.pressed & Input::Circle) == 0) return Action::kNone;
  switch (row_) {
    case kAshleyStats:
      stat_row_ = 0;
      stats_open_ = true;
      break;
    case kAddGroup:
      AddGroup();
      break;
    case kClearGroups:
      setup_.enemies.clear();
      message_ = quantity_ == 0
                     ? "GROUPS CLEARED - SOLO ASHLEY"
                     : "GROUPS CLEARED - CURRENT SELECTION WILL BE USED";
      break;
    case kStart:
      return Action::kStart;
    case kSave:
      return Action::kSave;
    default:
      Change(1);
      break;
  }
  return Action::kNone;
}

void Editor::UpdateStats(const Input::PadState& pad, bool confirm) {
  const auto buttons = pad.repeat | pad.pressed;
  if ((pad.pressed & Input::Cross) != 0) {
    stats_open_ = false;
    return;
  }
  if ((buttons & Input::Up) != 0) {
    stat_row_ = (stat_row_ + kStatRows - 1) % kStatRows;
  }
  if ((buttons & Input::Down) != 0) stat_row_ = (stat_row_ + 1) % kStatRows;
  confirm = confirm || (pad.pressed & Input::Circle) != 0;
  if (stat_row_ >= kResetStats) {
    if (confirm && stat_row_ == kResetStats) setup_.stat_overrides = {};
    if (confirm && stat_row_ == kBackFromStats) stats_open_ = false;
    return;
  }
  auto& value = setup_.stat_overrides.*kStats[stat_row_].member;
  if (confirm) {
    value.reset();
    return;
  }
  int step = 1;
  constexpr int kMediumStep = 10, kLargeStep = 100;
  if ((pad.held & Input::L1) != 0) step = kMediumStep;
  if ((pad.held & Input::R1) != 0) step = kLargeStep;
  const int direction =
      ((buttons & Input::Right) != 0) - ((buttons & Input::Left) != 0);
  if (direction == 0) return;
  constexpr int kMin = Game::EncounterLimits::kMinPlayerStat;
  constexpr int kMax = Game::EncounterLimits::kMaxPlayerStat;
  value = value ? std::clamp(*value + direction * step, kMin, kMax)
                : (direction > 0 ? kMin : kMax);
}

void Editor::DrawStats(Canvas& canvas) const {
  canvas.Text(kLabelX, kTitleY, "ASHLEY STARTING STAT OVERRIDES");
  canvas.Text(kLabelX, kSubtitleY,
              "LAB ONLY - APPLIED ONCE AT ENCOUNTER START");
  for (int index = 0; index < kStatRows; ++index) {
    const int y = kFirstRowY + index * kRowHeight;
    if (index == stat_row_) {
      canvas.Rectangle(kSelectionX, y - kSelectionOffset, kSelectionWidth,
                       kSelectionHeight, kHighlight);
    }
    std::string label;
    if (index < kResetStats) {
      const auto& field = kStats[index];
      const auto value = setup_.stat_overrides.*field.member;
      label = std::format("{}: {}", field.label,
                          value ? std::to_string(*value) : "DEFAULT");
    } else {
      label = index == kResetStats ? "RESET ALL TO DEFAULT" : "BACK TO SETUP";
    }
    canvas.Text(kLabelX, y, label);
  }
  canvas.Text(kLabelX, kValidationY,
              "VALUES 1..999. DEFAULT PRESERVES PROFILE.");
  canvas.Text(kLabelX, kWarningY,
              "HP / MP START FULL. INVENTORY AND SPELLS ARE UNCHANGED.");
  canvas.Text(kLabelX, kMessageY,
              "LEFT FROM DEFAULT: 999. RIGHT FROM DEFAULT: 1.");
  canvas.Text(kLabelX, kEditorHelpY,
              "LEFT/RIGHT: +/-1   HOLD L1/Q: 10   R1/E: 100");
  canvas.Text(kLabelX, kCombatHelpY,
              "ENTER/CIRCLE: RESET SELECTED STAT   CROSS/X: BACK");
}

void Editor::Draw(Canvas& canvas) const {
  canvas.Clear();
  if (stats_open_) {
    DrawStats(canvas);
    return;
  }
  canvas.Text(kLabelX, kTitleY, "VAGRANT STORY - COMBAT LAB");
  canvas.Text(kLabelX, kSubtitleY,
              "STANDALONE ENCOUNTER BUILDER - PRODUCTION GAMEPLAY");
  const auto& zone = Zone();
  const auto& room = zone.rooms[room_index_];
  const auto enemy_name =
      zone.enemies.empty()
          ? "NONE"
          : std::format("{} {}", zone.enemies[enemy_index_].index,
                        zone.enemies[enemy_index_].name);
  const bool default_stats =
      setup_.stat_overrides == Game::EncounterStatOverrides{};
  const std::array<std::string, kRowCount> rows = {
      std::format("ZONE: {}", zone.id),
      std::format("ROOM: {} {}", room.index, room.asset),
      std::format("ENEMY: {}", enemy_name),
      std::format("QUANTITY: {}{}", quantity_, quantity_ == 0 ? " (SOLO)" : ""),
      std::format("ASHLEY TILE X: {}", setup_.player.x),
      std::format("ASHLEY TILE Z: {}", setup_.player.z),
      std::format("ELEVATION: {}", setup_.player.elevation),
      std::format("GROUP OFFSET X: {}", offset_x_),
      std::format("GROUP OFFSET Z: {}", offset_z_),
      std::format("LINE SPACING: {}", spacing_),
      std::format("RANDOM SEED: {}", setup_.seed),
      std::format("ASHLEY: {}", Game::EncounterProfileId(setup_.profile)),
      default_stats ? "ASHLEY STATS: DEFAULT (ENTER TO EDIT)"
                    : "ASHLEY STATS: CUSTOM (ENTER TO EDIT)",
      "ADD GROUP",
      "CLEAR GROUPS",
      "START ENCOUNTER",
      "SAVE PRESET"};
  for (int index = 0; index < kRowCount; ++index) {
    const int y = kFirstRowY + index * kRowHeight;
    if (index == row_) {
      canvas.Rectangle(kSelectionX, y - kSelectionOffset, kSelectionWidth,
                       kSelectionHeight, kHighlight);
    }
    canvas.Text(kLabelX, y, rows[index]);
  }
  const auto setup = Setup();
  canvas.Text(kPreviewX, kFirstRowY,
              std::format("{} ENEMIES", setup.enemies.size()));
  canvas.Text(kPreviewX, kLegendY, "BLUE: ASHLEY / RED: ENEMY");
  canvas.Text(kPreviewX, kLayoutLabelY, "GREEN: FLOOR / DARK: UNSUPPORTED");
  if (room.width > 0 && room.depth > 0) {
    const int cell = std::max(1, std::min(220 / room.width, 210 / room.depth));
    const int left = 390;
    const int top = 125;
    for (int z = 0; z < room.depth; ++z) {
      for (int x = 0; x < room.width; ++x) {
        const auto index = static_cast<std::size_t>(z) * room.width + x;
        if (index < room.floor.size() &&
            room.floor[index] == Game::EncounterFloor::kSupported) {
          canvas.Rectangle(left + x * cell, top + z * cell, cell, cell,
                           kSupportedFloor);
        }
      }
    }
    for (int x = 0; x <= room.width; ++x) {
      canvas.Rectangle(left + x * cell, top, 1, room.depth * cell, kHighlight);
    }
    for (int z = 0; z <= room.depth; ++z) {
      canvas.Rectangle(left, top + z * cell, room.width * cell, 1, kHighlight);
    }
    const auto marker = [&](const Game::EncounterPosition& position,
                            std::uint32_t color) {
      if (position.x < 0 || position.x >= room.width || position.z < 0 ||
          position.z >= room.depth) {
        return;
      }
      canvas.Rectangle(left + position.x * cell + 1,
                       top + position.z * cell + 1, std::max(1, cell - 2),
                       std::max(1, cell - 2), color);
    };
    marker(setup.player, kPlayerMarker);
    for (const auto& enemy : setup.enemies) {
      marker(enemy.position, kEnemyMarker);
    }
  }
  const auto errors = Game::ValidateEncounter(setup, zone);
  canvas.Text(
      kLabelX, kValidationY,
      errors.empty() ? "STATIC FLOOR AND SLOT BUDGET VALID" : errors.front(),
      kWarning);
  canvas.Text(kLabelX, kWarningY,
              "LARGE FOOTPRINTS AND ENEMY BEHAVIOR NOT VERIFIED", kWarning);
  canvas.Text(kLabelX, kMessageY, message_, kWarning);
  canvas.Text(kLabelX, kEditorHelpY,
              "ARROWS: SELECT/ADJUST   ENTER OR CIRCLE: CONFIRM");
  canvas.Text(kLabelX, kCombatHelpY,
              "F6 PAUSE  F7 STEP  F10 EDITOR  F11 ITEM TEST HELP");
}

}  // namespace VS::combat_lab
