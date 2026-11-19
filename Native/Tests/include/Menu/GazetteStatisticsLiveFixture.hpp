#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "Menu/GazetteTimesLiveFixture.hpp"
#include "Menu/Score/GazetteRows.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"
#include "Menu/Score/GazetteStatisticsLive.hpp"

namespace gazette_statistics_live_tests {
namespace shared = gazette_times_live_tests;

inline auto AdditionalOwners() {
  return std::array{
      shared::BytesOf(VS::Menu::Score::titleText),
      shared::BytesOf(VS::Menu::Score::titleDescriptions),
      shared::BytesOf(VS::Menu::Score::statHeaders),
      shared::BytesOf(VS::Menu::Score::statDescriptions),
      shared::BytesOf(VS::Menu::Score::mapCompletion),
      shared::BytesOf(VS::Menu::Score::openedChestCount),
      shared::BytesOf(VS::Menu::Score::LiveScrollableRowAnimation())};
}

inline std::vector<std::byte> SnapshotBorrowedInputs() {
  auto result = shared::SnapshotBorrowedInputs();
  for (const auto owner : AdditionalOwners()) {
    result.insert(result.end(), owner.begin(), owner.end());
  }
  return result;
}

class SavedAdditionalInputs {
 public:
  SavedAdditionalInputs() {
    for (const auto owner : AdditionalOwners()) {
      backups_.emplace_back(owner.begin(), owner.end());
    }
  }
  ~SavedAdditionalInputs() {
    const auto owners = AdditionalOwners();
    for (std::size_t index = 0; index < owners.size(); ++index) {
      std::ranges::copy(backups_[index], owners[index].begin());
    }
  }
  SavedAdditionalInputs(const SavedAdditionalInputs&) = delete;
  SavedAdditionalInputs& operator=(const SavedAdditionalInputs&) = delete;

 private:
  std::vector<std::vector<std::byte>> backups_;
};

inline void ReloadAdditionalTables() {
  constexpr std::size_t kAdditionalTables = 4;
  const auto image = shared::ReadImage(R"(Native\data\disc\MENU\MENU9.PRG)");
  const auto entries = VS::Content::ModuleDataEntries();
  const auto owners = AdditionalOwners();
  for (const auto owner : std::span(owners).first(kAdditionalTables)) {
    const auto found =
        std::ranges::find(entries, static_cast<void*>(owner.data()),
                          &VS::Content::ModuleDataEntry::target);
    if (found == entries.end() || found->module == nullptr ||
        std::string_view(found->module) != shared::kGazetteModule ||
        found->capacity != owner.size() || found->size > owner.size() ||
        !VS::Content::CopyModuleDataEntry(*found, image)) {
      throw std::runtime_error(
          "Gazette statistics original table owner/import mismatch");
    }
  }
}

// The shared fixture owns the score, clear count, rows, fonts and frame.
// Only statistics-specific tables/progress and the reused scroller are added.
class StatisticsFixture : public shared::Fixture {
 private:
  SavedAdditionalInputs saved_;

 public:
  StatisticsFixture() {
    ReloadAdditionalTables();
    VS::Menu::Score::ResetLiveGazetteStatisticsStatics();
    VS::Menu::Score::LiveScrollableRowAnimation() = {};
    std::ranges::fill(VS::Render::variableFontGlyphWidths, char{6});
  }
  ~StatisticsFixture() { VS::Menu::Score::ResetLiveGazetteStatisticsStatics(); }
  StatisticsFixture(const StatisticsFixture&) = delete;
  StatisticsFixture& operator=(const StatisticsFixture&) = delete;
};
}  // namespace gazette_statistics_live_tests
