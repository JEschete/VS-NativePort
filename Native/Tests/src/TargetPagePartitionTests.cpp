// Hand-maintained tests for the typed target page binding (#247).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/TargetPages.hpp"
#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/TargetingState.h"
}

namespace {
constexpr std::size_t kPageSize = 24;

template <std::size_t N>
std::array<std::uint8_t, N> Groups(std::size_t group_size) {
  std::array<std::uint8_t, N> actor_ids{};
  for (std::size_t index = 0; index < N; ++index) {
    actor_ids[index] = static_cast<std::uint8_t>(index / group_size);
  }
  return actor_ids;
}

class RecordingSphere final : public VS::Battle::TargetSphere {
 public:
  explicit RecordingSphere(bool active) : active_(active) {}
  [[nodiscard]] bool Active() const override { return active_; }
  [[nodiscard]] std::size_t VisibleSlots() const override { return kPageSize; }
  void ShowPage(VS::Battle::TargetPage page, int mode) override {
    shown.push_back(page);
    modes.push_back(mode);
  }

  std::vector<VS::Battle::TargetPage> shown;
  std::vector<int> modes;

 private:
  bool active_;
};
}  // namespace

TEST_CASE("Target pages hold whole actor groups up to the page size",
          "[combat][targeting][pages]") {
  const auto actor_ids = Groups<40>(5);
  const auto pages = VS::Battle::PartitionTargetPages(actor_ids, kPageSize);
  REQUIRE(pages.size() == 2);
  CHECK(pages[0].first == 0);
  CHECK(pages[0].count == 20);
  CHECK(pages[1].first == 20);
  CHECK(pages[1].count == 20);
}

TEST_CASE("Target pages fill exactly and leave no empty page",
          "[combat][targeting][pages]") {
  const auto actor_ids = Groups<48>(6);
  const auto pages = VS::Battle::PartitionTargetPages(actor_ids, kPageSize);
  REQUIRE(pages.size() == 2);
  CHECK(pages[0].count == 24);
  CHECK(pages[1].first == 24);
  CHECK(pages[1].count == 24);
  CHECK(VS::Battle::PartitionTargetPages({}, kPageSize).empty());
}

TEST_CASE("Sphere targets show the first page in the requested mode",
          "[combat][targeting][pages]") {
  const auto actor_ids = Groups<40>(5);
  VS::Battle::TargetPageBook book;
  book.current = 1;
  RecordingSphere sphere(true);
  VS::Battle::SetSphereTargets(book, actor_ids, 1, sphere);
  CHECK(book.pages.size() == 2);
  CHECK(book.current == 0);
  CHECK(book.total == 40);
  REQUIRE(sphere.shown.size() == 1);
  CHECK(sphere.shown[0].first == 0);
  CHECK(sphere.shown[0].count == 20);
  CHECK(sphere.modes[0] == 1);
}

TEST_CASE("Sphere targets bind nothing without an active sphere",
          "[combat][targeting][pages]") {
  const auto actor_ids = Groups<8>(2);
  VS::Battle::TargetPageBook book;
  book.pages.push_back({0, 4});
  book.total = 4;
  RecordingSphere inactive(false);
  VS::Battle::SetSphereTargets(book, actor_ids, 0, inactive);
  CHECK(book.pages.empty());
  CHECK(book.total == 0);
  CHECK(inactive.shown.empty());
  RecordingSphere active(true);
  VS::Battle::SetSphereTargets(book, {}, 0, active);
  CHECK(book.pages.empty());
  REQUIRE(active.shown.size() == 1);
  CHECK(active.shown[0].count == 0);
}

TEST_CASE("Checked target previews keep the bound storage",
          "[combat][targeting][pages]") {
  std::array<Battle_TargetPreview, 8> previews{};
  const auto bound = VS::Battle::CheckedTargetPreviews(previews, 3);
  CHECK(bound.data() == previews.data());
  CHECK(bound.size() == 3);
  const auto none = VS::Battle::CheckedTargetPreviews(previews, 0);
  CHECK(none.data() == previews.data());
  CHECK(none.empty());
  CHECK(VS::Battle::CheckedTargetPreviews(previews, 8).size() == 8);
}

TEST_CASE("The promoted target binding has no original module address",
          "[combat][targeting][pages]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800C1664u) == nullptr);
}
