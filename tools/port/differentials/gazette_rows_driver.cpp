// Native side of the gazette row differentials (gazette_rows_common.py,
// #276). One vector per line, starting with the mode.
//
// RENDER: frame buffer, previous frame, step, row state, width, background,
// fade, scroll flag, x, y, the 32 text bytes, four glyph widths and the
// gradient salt. Prints the previous frame, step, fade, the line count and
// each line's six words after its tag, then the calls.
//
// The menu modes: row count, absolute row, cursor memory setting, the two
// cursor memory bytes, the text pointer, the selected row reply, the
// initialize flag, the top menu's step and selected row, the gazette row,
// the four _menuText offsets and the ready reply. Prints the return value,
// the top menu's statics, the cursor memory bytes and the setting, then the
// calls.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/Framework/Framework.hpp"
#include "Menu/Score/GazetteRows.hpp"

namespace {
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;

enum Mode : int {
  kRender,
  kSetRows,
  kSelected,
  kTopMenu,
  kClearRow8,
  kReady,
};
enum CallKind : std::int64_t {
  kCallArrow = 1,
  kCallGlyph,
  kCallGradient,
  kCallTile,
  kCallSprite,
  kCallShow,
  kCallShowList,
  kCallSelect,
  kCallClear,
  kCallFlyOut,
  kCallReady,
  // A call the original never makes.
  kCallRefuse = 90,
};

// The fixture addresses gazette_rows_common.py uses.
constexpr std::int64_t kRow = 0x80300000;
constexpr std::int64_t kOrdering = 0x80304000;
constexpr std::int64_t kArrowEntry = kOrdering + 4;
constexpr std::int64_t kRowEntry = kOrdering + 8;
constexpr std::int64_t kTileCode = 0x60000000;
constexpr std::uint32_t kMenuText = 0x80108520U;

using Call = std::vector<std::int64_t>;

template <typename Value>
bool Read(Value& value) {
  long long read = 0;
  if (!(std::cin >> read)) return false;
  value = static_cast<Value>(read);
  return true;
}

std::int64_t Unsigned(std::uint32_t value) {
  return static_cast<std::int64_t>(value);
}

std::uint32_t GradientColor(std::uint32_t salt, std::uint32_t gradient,
                            std::uint32_t color_index,
                            std::uint32_t brightness) {
  return ((gradient * 0x9E37U) + (color_index * 0x85EBU) +
          (brightness * 0xC2B3U)) ^
         salt;
}

class RowHost final : public score::ScrollableRowHost {
 public:
  int frame = 0;
  std::array<int, 4> widths{};
  std::uint32_t salt = 0;
  std::vector<Call> calls;
  std::vector<score::GradientLine> lines;

  int FrameBufferIndex() override { return frame; }
  void DrawScrollArrow(const score::ScrollArrowSprite& arrow) override {
    calls.push_back({kCallArrow, Unsigned(static_cast<std::uint32_t>(
                                     arrow.brightness)),
                     Unsigned(arrow.position), Unsigned(arrow.size),
                     kArrowEntry, Unsigned(arrow.texture)});
  }
  int DrawGlyph(std::uint32_t character, int x) override {
    calls.push_back({kCallGlyph, Unsigned(character),
                     Unsigned(static_cast<std::uint32_t>(x)), kRow,
                     kRowEntry});
    const int pen = x + widths[glyphs_ % widths.size()];
    ++glyphs_;
    return pen;
  }
  std::uint32_t GradientStopColor(std::uint32_t gradient,
                                  std::uint32_t color_index,
                                  int brightness) override {
    const auto level = static_cast<std::uint32_t>(brightness);
    calls.push_back({kCallGradient, Unsigned(gradient), Unsigned(color_index),
                     Unsigned(level)});
    return GradientColor(salt, gradient, color_index, level);
  }
  void AddGradientLine(const score::GradientLine& line) override {
    lines.push_back(line);
  }
  void AddShadowTile(std::uint32_t position, std::uint32_t size) override {
    calls.push_back(
        {kCallTile, kRowEntry, kTileCode, Unsigned(position), Unsigned(size)});
  }
  void AddReflection(const score::ReflectionSprite& reflection) override {
    calls.push_back(
        {kCallSprite,
         Unsigned(static_cast<std::uint32_t>(reflection.brightness)),
         Unsigned(reflection.position), Unsigned(reflection.size), kRowEntry,
         Unsigned(reflection.draw_mode), Unsigned(reflection.texture)});
  }
  void Refuse(const char*) override { calls.push_back({kCallRefuse}); }

 private:
  std::size_t glyphs_ = 0;
};

struct MenuVector {
  int count = 0;
  int row = 0;
  std::uint8_t setting = 0;
  score::TopMenuCursor cursor;
  std::int64_t text = 0;
  int selected = 0;
  int initialize = 0;
  score::TopMenuState top;
  std::uint8_t gazette_row = 0;
  std::array<std::uint16_t, 4> menu_text{};
  int ready = 0;
};

class MenuHost final : public score::GazetteMenuHost {
 public:
  explicit MenuHost(MenuVector& vector, bool top_menu)
      : vector_(vector), top_menu_(top_menu) {}

  std::vector<Call> calls;

  void ListTopMenuChoices() override {
    listed_ = true;
    for (std::size_t i = 0; i < choices_.size(); ++i) {
      choices_[i] = kMenuText + (2U * vector_.menu_text[i]);
    }
  }
  std::uint8_t GazetteRow() override { return vector_.gazette_row; }
  void SetTopMenuCursor(score::TopMenuCursor cursor) override {
    vector_.cursor = cursor;
  }
  std::uint8_t TopMenuCursorRow() override { return vector_.cursor.row; }
  std::uint8_t CursorMemorySetting() override { return vector_.setting; }
  void SetCursorMemorySetting(std::uint8_t setting) override {
    vector_.setting = setting;
  }
  void ShowRows(std::int32_t row_info,
                std::span<const std::int32_t> row_types) override {
    std::int64_t types = 0;
    for (const std::int32_t type : row_types) types += type != 0 ? 1 : 0;
    const auto count = static_cast<std::int64_t>(row_types.size());
    if (top_menu_) {
      Call call{kCallShowList, count, row_info};
      for (const std::uint32_t choice : choices_) {
        call.push_back(listed_ ? Unsigned(choice) : -1);
        call.push_back(0);
      }
      call.push_back(vector_.setting);
      call.push_back(types);
      calls.push_back(call);
    } else {
      calls.push_back(
          {kCallShow, count, row_info, vector_.text, vector_.setting, types});
    }
  }
  std::int32_t SelectedRow() override {
    calls.push_back({kCallSelect});
    return vector_.selected;
  }
  void ClearMenuExcept(std::int32_t row) override {
    calls.push_back({kCallClear, row});
  }
  void FlyOutAndHoist(std::int32_t row, std::int32_t offset) override {
    calls.push_back({kCallFlyOut, row, offset});
  }
  std::int32_t MenuRowsIdle() override {
    calls.push_back({kCallReady});
    return vector_.ready;
  }
  void Refuse(const char*) override { calls.push_back({kCallRefuse}); }

 private:
  MenuVector& vector_;
  bool top_menu_ = false;
  bool listed_ = false;
  std::array<std::uint32_t, 4> choices_{};
};

void PrintCalls(const std::vector<Call>& calls) {
  std::cout << ' ' << calls.size();
  for (const Call& call : calls) {
    for (const std::int64_t word : call) std::cout << ' ' << word;
  }
  std::cout << '\n';
}

bool Render() {
  RowHost host;
  score::ScrollableRowAnimation animation;
  fw::MenuRow row;
  int state = 0;
  bool read = Read(host.frame) && Read(animation.previous_frame_buffer) &&
              Read(animation.step) && Read(state) && Read(row.w) &&
              Read(row.background_width) && Read(row.gradient_state) &&
              Read(row.is_scrollable) && Read(row.x) && Read(row.y);
  for (std::uint8_t& value : row.text) read = read && Read(value);
  for (int& width : host.widths) read = read && Read(width);
  read = read && Read(host.salt);
  if (!read) return false;
  row.state = static_cast<fw::RowState>(state);
  score::RenderScrollableRow(host, animation, row);
  std::cout << static_cast<int>(animation.previous_frame_buffer) << ' '
            << static_cast<int>(animation.step) << ' '
            << static_cast<int>(row.gradient_state) << ' '
            << host.lines.size();
  for (const score::GradientLine& line : host.lines) {
    for (const std::uint32_t word : line) std::cout << ' ' << word;
  }
  PrintCalls(host.calls);
  return true;
}

bool Menu(int mode) {
  MenuVector vector;
  bool read = Read(vector.count) && Read(vector.row) && Read(vector.setting) &&
              Read(vector.cursor.row) && Read(vector.cursor.first_row) &&
              Read(vector.text) && Read(vector.selected) &&
              Read(vector.initialize) && Read(vector.top.step) &&
              Read(vector.top.selected) && Read(vector.gazette_row);
  for (std::uint16_t& offset : vector.menu_text) read = read && Read(offset);
  read = read && Read(vector.ready);
  if (!read) return false;
  MenuHost host(vector, mode == kTopMenu);
  int result = 0;
  switch (mode) {
    case kSetRows:
      score::RestoreTopMenuCursor(host, vector.count, vector.row);
      break;
    case kSelected:
      result = score::GetSelectedTopMenuRow(host);
      break;
    case kTopMenu:
      result = score::UpdateTopMenu(host, vector.top, vector.initialize);
      break;
    case kClearRow8:
      score::ClearMenuExceptRow8(host);
      break;
    case kReady:
      score::PollMenuReady(host);
      break;
    default:
      return false;
  }
  std::cout << result << ' ' << vector.top.step << ' ' << vector.top.selected
            << ' ' << static_cast<int>(vector.cursor.row) << ' '
            << static_cast<int>(vector.cursor.first_row) << ' '
            << static_cast<int>(vector.setting);
  PrintCalls(host.calls);
  return true;
}
}  // namespace

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  int mode = 0;
  while (Read(mode)) {
    if (!(mode == kRender ? Render() : Menu(mode))) return 1;
  }
  return 0;
}
