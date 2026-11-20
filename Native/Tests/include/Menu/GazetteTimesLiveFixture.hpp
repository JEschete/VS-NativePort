#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "Battle/StatusEffects.h"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Score/GazetteTimes.hpp"
#include "Menu/Score/GazetteTimesLive.hpp"
#include "Menu/Score/Records.hpp"
#include "Menu/TextBox.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/Sprites.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/Text/VariableFont.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Progress.hpp"

namespace gazette_times_live_tests {
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
inline constexpr std::size_t kPrimitiveWords = 0x20000 / sizeof(u_long);
inline constexpr std::size_t kOrderingWords = 0x2088 / sizeof(u_long);
inline constexpr std::size_t kSceneHead = 18;
inline constexpr std::size_t kOverlayHead = 2;
inline constexpr std::size_t kInformationBox = 7;
inline constexpr std::uint32_t kCanary = 0xDEADBEEF;
inline constexpr std::uint32_t kLinkMask = 0xFFFFFF;
inline constexpr std::uint32_t kMissingTime = 0x800000;
inline constexpr std::string_view kBattleModule = "BATTLE/BATTLE.PRG";
inline constexpr std::string_view kGazetteModule = "MENU/MENU9.PRG";
using Packet = std::vector<std::uint32_t>;
using EncodedText = std::vector<std::uint8_t>;

template <typename T>
std::span<std::byte> BytesOf(T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  return std::as_writable_bytes(std::span(&value, 1));
}

inline auto TableOwners() {
  return std::array{BytesOf(score::timeAttacks),
                    BytesOf(score::timeAttackDescriptions),
                    BytesOf(score::miscInfo),
                    BytesOf(VS::Render::compactGlyphLeftTrim),
                    BytesOf(VS::Render::compactGlyphWidths),
                    BytesOf(VS::Render::compactGlyphIndices),
                    BytesOf(VS::Render::variableFontGlyphWidths),
                    BytesOf(VS::Menu::Framework::gradientTopColors),
                    BytesOf(VS::Menu::Framework::gradientBottomColors),
                    BytesOf(VS::Menu::Framework::rowAnimationSteps),
                    BytesOf(VS::Menu::Framework::cursorBrightnessAnimation)};
}

inline auto MutableOwners() {
  return std::array{BytesOf(score::gazetteRows),
                    BytesOf(score::clearCount),
                    BytesOf(VS::State::scoreData),
                    BytesOf(Battle_unusedMenu9CursorMemory),
                    BytesOf(VS::Menu::Framework::menuItems),
                    BytesOf(VS::Menu::Framework::menuMemory),
                    BytesOf(VS::Menu::Framework::cursorColor),
                    BytesOf(VS::Menu::Framework::hideMenu),
                    BytesOf(VS::Menu::textBoxes),
                    BytesOf(VS::Render::FrameScratchpad()),
                    BytesOf(VS::Render::legacyPrimitiveBuffers),
                    BytesOf(VS::Render::legacyOrderingTables),
                    BytesOf(VS::Render::frameBuf),
                    BytesOf(VS::Render::LiveFontStyle())};
}

inline std::vector<std::span<std::byte>> BorrowedOwners() {
  const auto tables = TableOwners();
  const auto mutable_owners = MutableOwners();
  std::vector<std::span<std::byte>> owners(tables.begin(), tables.end());
  owners.insert(owners.end(), mutable_owners.begin(), mutable_owners.end());
  return owners;
}

inline std::vector<std::byte> SnapshotBorrowedInputs() {
  std::vector<std::byte> result;
  for (const auto owner : BorrowedOwners()) {
    result.insert(result.end(), owner.begin(), owner.end());
  }
  return result;
}

class SavedInputs {
 public:
  SavedInputs() {
    for (const auto owner : BorrowedOwners()) {
      backups_.push_back({owner, {owner.begin(), owner.end()}});
    }
  }
  ~SavedInputs() { Restore(); }
  SavedInputs(const SavedInputs&) = delete;
  SavedInputs& operator=(const SavedInputs&) = delete;

  void Restore() {
    for (const auto& backup : backups_) {
      std::ranges::copy(backup.bytes, backup.owner.begin());
    }
    backups_.clear();
  }

 private:
  struct Backup {
    std::span<std::byte> owner;
    std::vector<std::byte> bytes;
  };
  std::vector<Backup> backups_;
};

inline std::vector<unsigned char> ReadImage(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error(std::string("Gazette Live fixture needs ") + path);
  }
  return {std::istreambuf_iterator<char>(stream), {}};
}

inline void ReloadBorrowedTables() {
  const auto battle = ReadImage(R"(Native\data\disc\BATTLE\BATTLE.PRG)");
  const auto gazette = ReadImage(R"(Native\data\disc\MENU\MENU9.PRG)");
  const auto entries = VS::Content::ModuleDataEntries();
  for (const auto owner : TableOwners()) {
    const auto found =
        std::ranges::find(entries, static_cast<void*>(owner.data()),
                          &VS::Content::ModuleDataEntry::target);
    if (found == entries.end() || found->module == nullptr ||
        found->capacity != owner.size() || found->size > owner.size()) {
      throw std::runtime_error("Gazette Live table owner registry mismatch");
    }
    const std::string_view module(found->module);
    if ((module != kBattleModule && module != kGazetteModule) ||
        !VS::Content::CopyModuleDataEntry(
            *found, module == kBattleModule ? battle : gazette)) {
      throw std::runtime_error("Gazette Live original table import failed");
    }
  }
}

inline EncodedText TerminatedText(std::span<const std::uint8_t> bytes) {
  const auto end = std::ranges::find(bytes, std::uint8_t{0xE7});
  if (end == bytes.end()) {
    throw std::runtime_error("Gazette Live text left its bounded owner");
  }
  return {bytes.begin(), end + 1};
}

template <std::size_t Count>
EncodedText Label(const u_short (&table)[Count], std::size_t index) {
  if (index >= Count || table[index] >= Count) {
    throw std::runtime_error("Gazette Live label offset left its table");
  }
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(table);
  return TerminatedText(
      std::span(bytes, sizeof table).subspan(table[index] * sizeof(u_short)));
}

inline EncodedText BorrowedText(const std::uint8_t* text) {
  const auto address = reinterpret_cast<std::uintptr_t>(text);
  const auto tables = TableOwners();
  constexpr std::size_t kGazetteTables = 3;
  for (const auto table : std::span(tables).first(kGazetteTables)) {
    const auto start = reinterpret_cast<std::uintptr_t>(table.data());
    if (address >= start && address - start < table.size()) {
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(table.data());
      return TerminatedText(
          std::span(bytes, table.size()).subspan(address - start));
    }
  }
  throw std::runtime_error("Gazette Live row does not borrow a MENU9 table");
}

struct Sprite {
  std::uint32_t color = 0;
  std::uint32_t position = 0;
  std::uint32_t texture = 0;
  std::uint32_t size = 0;
  friend bool operator==(const Sprite&, const Sprite&) = default;
};

struct TextRun {
  std::string text;
  std::vector<Sprite> glyphs;
  [[nodiscard]] int X() const {
    return static_cast<std::int16_t>(glyphs.front().position);
  }
  [[nodiscard]] int Y() const {
    return static_cast<std::int16_t>(glyphs.front().position >> 16);
  }
  [[nodiscard]] int Right() const {
    const auto& last = glyphs.back();
    return static_cast<std::int16_t>(last.position) +
           static_cast<int>(last.size & 0xFFFF);
  }
};

inline char CompactCharacter(const Sprite& sprite) {
  constexpr std::size_t kPrintedGlyphs = '[' - ' ';
  for (std::size_t glyph = 0; glyph < kPrintedGlyphs; ++glyph) {
    const std::uint32_t cell = VS::Render::compactGlyphIndices[glyph];
    const std::uint32_t width =
        (VS::Render::compactGlyphWidths[glyph / 2] >> ((glyph % 2) * 4)) & 15;
    const std::uint32_t texture =
        (cell & 0xFE) | ((76 + (cell & 1) * 10) << 8) | (0x37F6U << 16);
    if (width != 0 && sprite.texture == texture &&
        sprite.size == (width | (10U << 16))) {
      return static_cast<char>(glyph + ' ');
    }
  }
  throw std::runtime_error("Gazette Live output has an unknown atlas glyph");
}

struct RowObservation {
  fw::RowState state{};
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t target = 0;
  std::uint8_t selected = 0;
  std::uint8_t disabled = 0;
  std::uint8_t delay = 0;
  std::array<std::uint8_t, fw::kRowTextBytes> text{};
  friend bool operator==(const RowObservation&,
                         const RowObservation&) = default;
};

struct ObservableState {
  std::array<RowObservation, score::kTimeTrialCount> rows{};
  EncodedText information;
  std::vector<Packet> packets;
  friend bool operator==(const ObservableState&,
                         const ObservableState&) = default;
};

class Fixture {
 private:
  SavedInputs saved_;
  alignas(std::max_align_t)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 256> arena_{};
  VS::Core::Memory::ScopedLiveGameHeap heap_{arena_};

 public:
  std::array<fw::MenuRow, fw::kRowCount> rows{};
  std::vector<u_long> primitives =
      std::vector<u_long>(kPrimitiveWords + 2, kCanary);
  std::array<u_long, kOrderingWords> ordering{};

  Fixture() {
    ReloadBorrowedTables();
    VS::Menu::Framework::menuItems = rows.data();
    VS::Menu::Framework::cursorColor = 0;
    VS::Menu::Framework::hideMenu = 0;
    VS::Menu::textBoxes[kInformationBox] = {};
    Battle_unusedMenu9CursorMemory.gazette_page = 0;
    std::ranges::fill(score::gazetteRows, score::ListRow{});
    for (auto& trial : VS::State::scoreData.boss_time_trial_scores) {
      for (auto& record : trial) {
        record = {};
        record.time = kMissingTime;
      }
    }
    VS::Render::SetLiveMenuFontStyle(0);
    score::ResetLiveTimeTrialStatics();
    ResetFrame();
  }
  ~Fixture() {
    score::ResetLiveTimeTrialStatics();
    saved_.Restore();
  }
  Fixture(const Fixture&) = delete;
  Fixture& operator=(const Fixture&) = delete;

  // Only packet storage is reset: a caller can observe cursor phase across
  // an actual module/cold reset without a private production-state getter.
  void ResetFrame() {
    std::ranges::fill(primitives, kCanary);
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = primitives.data() + 1;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().primitive_cursor = primitives.data() + 1;
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + kSceneHead;
    VS::Render::FrameScratchpad().overlay_ordering_table =
        ordering.data() + kOverlayHead;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }

  static void SetScore(std::size_t trial, std::size_t place, std::uint32_t time,
                       std::uint8_t round = 0, bool hard = false) {
    if (trial >= score::kTimeTrialCount || place >= score::kTimeTrialPlaces) {
      throw std::runtime_error("Gazette Live score index outside its owner");
    }
    auto& record = VS::State::scoreData.boss_time_trial_scores[trial][place];
    record.time = time & kLinkMask;
    record.round = static_cast<std::uint32_t>(round) & 127U;
    record.difficulty = hard ? 1U : 0U;
  }

  static fw::MenuRow& Row(std::size_t index) {
    if (index >= fw::kRowCount) {
      throw std::runtime_error("Gazette Live row index outside its owner");
    }
    return fw::LiveMenuRow(static_cast<int>(index));
  }

  void SettleEntrance() {
    constexpr int kFrameLimit = 64;
    for (int frame = 0; frame < kFrameLimit; ++frame) {
      ResetFrame();
      score::RenderLiveTimeTrialRecords(8);
      fw::UpdateAndDrawLiveRows();
      bool idle = true;
      for (std::size_t i = 0; i < score::kTimeTrialCount; ++i) {
        idle = idle && score::gazetteRows[i].animation_state == 0 &&
               Row(i).state == fw::RowState::kIdle && Row(i).x == 194;
      }
      if (idle) return;
    }
    throw std::runtime_error("Actual Gazette entrance did not settle");
  }

  [[nodiscard]] bool GuardsIntact() const {
    return primitives.front() == kCanary && primitives.back() == kCanary;
  }

  [[nodiscard]] std::size_t UsedWords() const {
    const auto start = reinterpret_cast<std::uintptr_t>(primitives.data() + 1);
    const auto end = reinterpret_cast<std::uintptr_t>(
        VS::Render::FrameScratchpad().primitive_cursor);
    if (end < start || end - start > kPrimitiveWords * sizeof(u_long) ||
        (end - start) % sizeof(u_long) != 0) {
      throw std::runtime_error("Gazette Live primitive cursor left its owner");
    }
    return (end - start) / sizeof(u_long);
  }

  [[nodiscard]] std::vector<Packet> Packets() const {
    std::vector<Packet> packets;
    const auto primitive_start =
        reinterpret_cast<std::uintptr_t>(primitives.data() + 1);
    const auto table_start = reinterpret_cast<std::uintptr_t>(ordering.data());
    const std::size_t used_bytes = UsedWords() * sizeof(u_long);
    const u_long* current = &ordering.back();
    for (std::size_t visits = 0; visits < kPrimitiveWords + kOrderingWords;
         ++visits) {
      const auto address = reinterpret_cast<std::uintptr_t>(current);
      const bool in_table =
          address >= table_start && address - table_start < sizeof ordering;
      const bool in_packets =
          address >= primitive_start && address - primitive_start < used_bytes;
      if ((!in_table && !in_packets) || address % alignof(u_long) != 0) {
        throw std::runtime_error("Gazette Live chain left its frame owners");
      }
      const std::size_t length = *current >> 24;
      if ((in_table && length != 0) ||
          (in_packets && (length + 1) * sizeof(u_long) >
                             used_bytes - (address - primitive_start))) {
        throw std::runtime_error("Gazette Live packet length exceeds owner");
      }
      if (length != 0) {
        Packet words(current, current + length + 1);
        words.front() &= ~kLinkMask;
        packets.push_back(std::move(words));
      }
      const auto next = *current & kLinkMask;
      if (next == kLinkMask) return packets;
      current = static_cast<const u_long*>(LegacyGpu_Pointer(next));
    }
    throw std::runtime_error("Gazette Live ordering chain contains a cycle");
  }

  [[nodiscard]] std::vector<TextRun> TextRuns() const {
    std::vector<TextRun> runs;
    bool continued = false;
    for (const auto& packet : Packets()) {
      constexpr std::size_t kCompactWords = 5;
      if (packet.size() != kCompactWords ||
          (packet[1] >> 24) != VS::Render::kSpriteCommand) {
        continued = false;
        continue;
      }
      if (!continued) runs.emplace_back();
      const Sprite sprite{packet[1], packet[2], packet[3], packet[4]};
      runs.back().text += CompactCharacter(sprite);
      runs.back().glyphs.push_back(sprite);
      continued = true;
    }
    return runs;
  }

  [[nodiscard]] TextRun Text(std::string_view expected, int y) const {
    for (const auto& run : TextRuns()) {
      if (run.text == expected && run.Y() == y) return run;
    }
    throw std::runtime_error("Missing actual Gazette glyph run: " +
                             std::string(expected));
  }

  [[nodiscard]] Sprite Cursor() const {
    constexpr std::uint32_t kCursorTexture = 0x37F83020;
    for (const auto& packet : Packets()) {
      if (packet.size() == VS::Render::kSpritePacketWords &&
          (packet[VS::Render::kSpriteColorWord] >> 24) ==
              VS::Render::kSpriteCommand &&
          packet[VS::Render::kSpriteTextureWord] == kCursorTexture) {
        return {packet[VS::Render::kSpriteColorWord],
                packet[VS::Render::kSpritePositionWord],
                packet[VS::Render::kSpriteTextureWord],
                packet[VS::Render::kSpriteSizeWord]};
      }
    }
    throw std::runtime_error("Actual Gazette frame contains no menu cursor");
  }

  [[nodiscard]] static EncodedText InformationText() {
    const auto* text = reinterpret_cast<const std::uint8_t*>(
        VS::Menu::textBoxes[kInformationBox].text);
    if (text == nullptr) return {};
    return TerminatedText(
        std::span(text, fw::kInformationMessageBytes).subspan(2));
  }

  [[nodiscard]] ObservableState Observe() const {
    ObservableState state{.information = InformationText(),
                          .packets = Packets()};
    for (std::size_t i = 0; i < state.rows.size(); ++i) {
      const auto& row = Row(i);
      state.rows[i] = {
          row.state,
          row.x,
          row.y,
          row.target0,
          row.selected,
          row.unselectable,
          static_cast<std::uint8_t>(score::gazetteRows[i].animation_state),
          row.text};
    }
    return state;
  }
};
}  // namespace gazette_times_live_tests
