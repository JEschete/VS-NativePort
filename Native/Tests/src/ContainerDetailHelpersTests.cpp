#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "Input/Pad.hpp"
#include "Menu/ContainerDetailHelpersWorld.hpp"

namespace {
namespace detail = VS::Menu::container;
namespace items = VS::Items;
namespace fw = VS::Menu::Framework;
namespace samples = container_detail_helpers_tests::samples;
using container_detail_helpers_tests::Call;
using container_detail_helpers_tests::Event;
using container_detail_helpers_tests::Records;
using container_detail_helpers_tests::World;
constexpr detail::DetailRow kSelected{20};
constexpr std::size_t kHeader = 31;

items::InventoryWeapon LastPartsWeapon(Records& records) {
  auto& contents = records.storage.items;
  contents.blades.back() = samples::kBlade;
  contents.grips.back() = samples::kGrip;
  contents.gems.back() = samples::kWeaponGem;
  return samples::kWeapon;
}
}  // namespace

TEST_CASE("F02925 assembles signed stats from full container extents",
          "[container-detail-helpers][F02925]") {
  Records records;
  const auto weapon = LastPartsWeapon(records);
  items::WeaponLoadout target;
  target.baseline.classes.back() = samples::kRetainedClassTotal;
  CHECK(detail::BuildWeaponStats(target, &weapon, records) ==
        items::ItemsStatus::kOk);
  CHECK(target.index == 0xFE);
  CHECK(target.blade.id == 41);
  CHECK(target.blade.material == 0x34);
  CHECK(target.grip.id == 300);
}

TEST_CASE("F02925 retains full name and aggregates repeated signed gem parts",
          "[container-detail-helpers][F02925]") {
  Records records;
  const auto weapon = LastPartsWeapon(records);
  items::WeaponLoadout target;
  target.baseline.classes.back() = samples::kRetainedClassTotal;
  REQUIRE(detail::BuildWeaponStats(target, &weapon, records) ==
          items::ItemsStatus::kOk);
  CHECK(target.gems[0].id == 511);
  CHECK(target.gems[1].id == 0);
  CHECK(target.gems[2].id == 511);
  CHECK(target.attributes.current_strength == 19);
  CHECK(target.damage_type_value == 249);
  CHECK(target.name == weapon.name);
  CHECK(target.baseline.classes.back() == 1901);
  CHECK(records.reads.size() == 4);
}

TEST_CASE("F02925 optional zeros are lazy but the name is mandatory",
          "[container-detail-helpers][F02925]") {
  Records records;
  items::InventoryWeapon source;
  source.name.fill('n');
  items::WeaponLoadout target;
  CHECK(detail::BuildWeaponStats(target, &source, records) ==
        items::ItemsStatus::kOk);
  CHECK(records.reads.empty());
  CHECK(target.name == source.name);
  const auto before = target;
  CHECK_THROWS_WITH(detail::BuildWeaponStats(target, nullptr, records),
                    "missing mandatory weapon name source");
  CHECK(target == before);
}

TEST_CASE("F02925 invalid later parts retain target and earlier read order",
          "[container-detail-helpers][F02925]") {
  Records records;
  auto source = LastPartsWeapon(records);
  source.grip = samples::kInvalidGrip;
  items::WeaponLoadout target;
  target.index = samples::kOldWeaponIndex;
  CHECK_THROWS_AS(detail::BuildWeaponStats(target, &source, records),
                  std::out_of_range);
  CHECK(target.index == 91);
  REQUIRE(records.reads.size() == 2);
  CHECK(records.reads[0].first == items::ItemCategory::kBlade);
  CHECK(records.reads[0].second == 63);
  CHECK(records.reads[1].second == 64);
  records.wrong_type = true;
  CHECK_THROWS_WITH(detail::BuildWeaponStats(target, &source, records),
                    "detail part type disagrees with container category");
}

TEST_CASE("F02926 masks flagged shield gems and ignores the fourth slot",
          "[container-detail-helpers][F02926]") {
  Records records;
  records.storage.items.gems[samples::kFlaggedGemIndex] = samples::kShieldGem;
  const auto source = samples::kShield;
  items::ShieldLoadout target;
  target.reserved0.fill(samples::kReservedHeader);
  target.reserved_db = samples::kReservedByte;
  detail::BuildShieldStats(target, &source, records);
  CHECK(target.index == 255);
  CHECK(target.base.id == 31);
  CHECK(target.base.material == 0x34);
  CHECK(target.gems[0].id == 500);
  CHECK(target.gems[1].id == 0);
  CHECK(target.gems[2].id == 500);
}

TEST_CASE("F02926 keeps signed type totals and untouched header bytes",
          "[container-detail-helpers][F02926]") {
  Records records;
  records.storage.items.gems[samples::kFlaggedGemIndex] = samples::kShieldGem;
  const auto source = samples::kShield;
  items::ShieldLoadout target;
  target.reserved0.fill(samples::kReservedHeader);
  target.reserved_db = samples::kReservedByte;
  detail::BuildShieldStats(target, &source, records);
  CHECK(target.types == std::array<std::uint8_t, 4>{255, 2, 253, 4});
  CHECK(target.attributes.current_strength == -6);
  CHECK(target.reserved0.front() == 98);
  CHECK(target.reserved_db == 0xA5);
  CHECK(records.reads.size() == 2);
}

TEST_CASE("F02926 absent shield produces empty stats without reading records",
          "[container-detail-helpers][F02926]") {
  Records records;
  items::ShieldLoadout target;
  target.base.id = samples::kOldPhase;
  target.reserved0.back() = samples::kReservedHeaderTail;
  detail::BuildShieldStats(target, nullptr, records);
  CHECK(target.base.id == 0);
  CHECK(target.index == 0);
  CHECK(target.reserved0.back() == 71);
  CHECK(records.reads.empty());
}

TEST_CASE("F02926 nonzero flag-only gem is not an optional empty gem",
          "[container-detail-helpers][F02926]") {
  Records records;
  items::InventoryShield source{.gems = {1, samples::kFlagOnlyGem, 0, 0}};
  items::ShieldLoadout target;
  target.index = samples::kOldShieldIndex;
  CHECK_THROWS_AS(detail::BuildShieldStats(target, &source, records),
                  std::out_of_range);
  REQUIRE(records.reads.size() == 2);
  CHECK(records.reads[0].second == 0);
  CHECK(records.reads[1].second == std::numeric_limits<std::uint32_t>::max());
  CHECK(target.index == 87);
}

TEST_CASE("F02942 copies the complete visibility byte only",
          "[container-detail-helpers][F02942]") {
  World world;
  world.phase = samples::kOldWeaponIndex;
  for (const std::uint8_t visibility : {0, 1, 2, 128, 255}) {
    detail::SetPanelVisibility(world.panels, visibility);
    CHECK(world.panels ==
          detail::PanelVisibility{visibility, visibility, visibility});
  }
  CHECK(world.phase == 91);
  CHECK(world.calls.empty());
}

TEST_CASE("F02943 simultaneous held shoulders bypass count and input rereads",
          "[container-detail-helpers][F02943]") {
  World world;
  world.held = VS::Input::L1 | VS::Input::R1;
  world.repeat = VS::Input::L1;
  CHECK(detail::PollDetailsPaging(world, {-1}).index == -1);
  CHECK(world.calls.empty());
}

TEST_CASE("F02943 paging wraps once and samples count callback input changes",
          "[container-detail-helpers][F02943]") {
  World world;
  world.count = samples::kWeapon.gems[0];
  world.on_call = [&](Event event, std::int32_t) {
    if (event == Event::kCount) world.repeat = VS::Input::L1;
  };
  CHECK(detail::PollDetailsPaging(world, {0}).index == 191);
  world.on_call = {};
  world.repeat = VS::Input::R1;
  CHECK(detail::PollDetailsPaging(world, {191}).index == 0);
  world.repeat = VS::Input::L1 | VS::Input::R1;
  CHECK(detail::PollDetailsPaging(world, {17}).index == 17);
  world.repeat = 0;
  CHECK(detail::PollDetailsPaging(world, {400}).index == 208);
}

TEST_CASE("F02943 signed overflow and invalid feedback retain source rules",
          "[container-detail-helpers][F02943]") {
  World world;
  world.count = 1;
  world.repeat = VS::Input::R1;
  world.pressed = VS::Input::R1;
  CHECK(detail::PollDetailsPaging(world, {0}).index == 0);
  CHECK(world.calls.back().first == Event::kInvalid);
  CHECK(detail::PollDetailsPaging(world, {std::numeric_limits<int>::max()})
            .index == std::numeric_limits<int>::min());
  world.count = 0;
  world.repeat = VS::Input::L1;
  CHECK(detail::PollDetailsPaging(world, {0}).index == -1);
}

TEST_CASE("F02945 clears rows before setting header and selectable item",
          "[container-detail-helpers][F02945]") {
  World world;
  world.rows[0][kSelected.index].unselectable = samples::kControls;
  world.rows[0][kSelected.index].sub_text = "borrowed";
  detail::BeginDetailsTransition(world, kSelected);
  CHECK(world.calls == std::vector<Call>{{Event::kClear, 40},
                                         {Event::kRow, 31},
                                         {Event::kRow, 20}});
  CHECK(world.rows[0][kHeader].state == fw::RowState::kSlideY);
  CHECK(world.rows[0][kHeader].target0 == 18);
  CHECK(world.rows[0][20].state == fw::RowState::kSlideX);
  CHECK(world.rows[0][20].target0 == 155);
  CHECK(world.rows[0][20].selected == 1);
  CHECK(world.rows[0][20].unselectable == 0);
  CHECK(world.rows[0][20].sub_text == nullptr);
}

TEST_CASE("F02945 callback owner changes preserve the already borrowed header",
          "[container-detail-helpers][F02945]") {
  World world;
  world.on_call = [&](Event event, std::int32_t row) {
    if (event == Event::kRow && row == static_cast<int>(kHeader)) {
      world.owner = 1;
    }
  };
  detail::BeginDetailsTransition(world, kSelected);
  CHECK(world.rows[0][kHeader].target0 == 18);
  CHECK(world.rows[1][kHeader].target0 == 0);
  CHECK(world.rows[0][20].selected == 0);
  CHECK(world.rows[1][20].selected == 1);
}

TEST_CASE("F02945 selected header alias keeps the final selected-row writes",
          "[container-detail-helpers][F02945]") {
  World world;
  detail::BeginDetailsTransition(world, {static_cast<int>(kHeader)});
  CHECK(world.rows[0][kHeader].state == fw::RowState::kSlideX);
  CHECK(world.rows[0][kHeader].target0 == 155);
  CHECK(world.rows[0][kHeader].selected == 1);
}

TEST_CASE("F02947 hides only container icons and controls before callbacks",
          "[container-detail-helpers][F02947]") {
  World world;
  world.on_call = [&](Event event, std::int32_t) {
    if (event == Event::kContinue) {
      CHECK(world.panels.title == 7);
      CHECK(world.panels.icons == 0);
      CHECK(world.panels.controls == 0);
      world.panels.icons = samples::kUpdatedIcons;
    }
  };
  detail::InitializeDetailsLayout(world, kSelected, -1);
  CHECK(world.calls == std::vector<Call>{{Event::kContinue, 20},
                                         {Event::kPages, -1},
                                         {Event::kPrimary, 1}});
  CHECK(world.panels.icons == 77);
}

TEST_CASE("F02949 returns unsigned halfwords not inventory byte slots",
          "[container-detail-helpers][F02949]") {
  World world;
  world.order.back() = samples::kHalfwordToken;
  CHECK(detail::SelectDetailsItem(world, {255}) == 65535);
  CHECK(world.cursor.row == 0);
  CHECK(world.cursor.page == 255);
  world.order.back() = 0;
  CHECK(detail::SelectDetailsItem(world, {255}) == 0);
}

TEST_CASE("F02949 remembers cursor before current-order resolution or refusal",
          "[container-detail-helpers][F02949]") {
  World world;
  world.on_remember = [&](detail::DetailCursor cursor) {
    world.order[cursor.page] = samples::kHighToken;
  };
  CHECK(detail::SelectDetailsItem(world, {17}) == 0x8001);
  CHECK_THROWS_AS(detail::SelectDetailsItem(world, {-1}), std::out_of_range);
  CHECK(world.cursor.page == 255);
  world.order_size = 0;
  CHECK_THROWS_AS(detail::SelectDetailsItem(world, {2}), std::out_of_range);
  CHECK(world.cursor.page == 2);
}

TEST_CASE(
    "F02950 exit callbacks precede return-phase and current triangle read",
    "[container-detail-helpers][F02950]") {
  World world;
  world.phase = samples::kOldPhase;
  world.on_call = [&](Event event, std::int32_t) {
    if (event == Event::kPrimary) {
      CHECK(world.phase == 99);
      world.pressed = VS::Input::Triangle;
    }
    if (event == Event::kBars) {
      CHECK(world.phase == 2);
      CHECK(world.triangle == static_cast<int>(VS::Input::Triangle));
    }
  };
  detail::ExitDetailsView(world, true);
  CHECK(world.calls == std::vector<Call>{{Event::kLeave, 0},
                                         {Event::kClear, 40},
                                         {Event::kDismiss, -1},
                                         {Event::kPages, -1},
                                         {Event::kPrimary, 2},
                                         {Event::kBars, 4}});
}

TEST_CASE("F02950 repeat entry does not invent a reset or optional bars call",
          "[container-detail-helpers][F02950]") {
  World world;
  world.pressed = VS::Input::Triangle;
  detail::ExitDetailsView(world, false);
  CHECK(world.triangle == static_cast<int>(VS::Input::Triangle));
  world.pressed = VS::Input::Circle;
  world.calls.clear();
  detail::ExitDetailsView(world, false);
  CHECK(world.triangle == 0);
  CHECK(world.phase == 2);
  CHECK(world.calls.size() == 5);
  CHECK(world.calls.back() == Call{Event::kPrimary, 2});
}
