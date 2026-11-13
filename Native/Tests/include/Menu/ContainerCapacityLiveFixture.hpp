#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Items/ContainerGemCountsLive.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerCapacityLive.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/ContainerDetailHelpersSamples.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"

namespace container_capacity_live_tests {
namespace items = VS::Items;
namespace capacity = VS::Menu::container;
namespace framework = VS::Menu::Framework;
namespace equipment = equipment_detail_tests;
namespace samples = container_detail_helpers_tests::samples;
inline constexpr int kLastWeapon = 31;
inline constexpr int kLastPart = 63;
inline constexpr int kLastGem = 191;
inline constexpr std::size_t kRealCategories = 7;

struct LiveFixture : equipment::LiveFixture {
  container_transfers_tests::LiveFixture containers;
  equipment::ValueRestore<u_char> saved_direction{
      Menu_Container_browsingGadgetBag};

  LiveFixture() {
    Menu_Container_containerData = &containers.storage->container;
    Menu_Container_otherStorage = &containers.storage->gadgetBag;
    Menu_Container_browsingGadgetBag = 0;
  }
  static void Fill(Items_ContainerData& owner, items::ItemCategory category,
                   std::size_t count) {
    items::LiveContainerWorld records(&owner);
    for (std::size_t slot = 0; slot < count; ++slot) {
      auto record = records.Read(category, slot);
      std::visit(
          [](auto& value) {
            if constexpr (requires { value.blade; }) {
              value.blade = 1;
            } else if constexpr (requires { value.base; }) {
              value.base.id = 1;
            } else {
              value.id = 1;
            }
          },
          record);
      records.Write(category, slot, record);
    }
  }
  static void AddWeapon(Items_ContainerData& owner) {
    auto grip = samples::kGrip;
    grip.gem_slots = items::kWeaponGemCount;
    items::Live::Commit(std::span(owner.blades).back(), samples::kBlade);
    items::Live::Commit(std::span(owner.grips).back(), grip);
    items::Live::Commit(std::span(owner.gems).back(), samples::kWeaponGem);
    items::Live::Commit(std::span(owner.weapons).back(), samples::kWeapon);
  }
  std::string Line(int y) const {
    std::vector<std::pair<std::int16_t, char>> glyphs;
    for (const auto& sprite : Sprites()) {
      if (sprite.compact &&
          static_cast<std::int16_t>(sprite.position >>
                                    framework::kHalfwordShift) == y) {
        glyphs.emplace_back(static_cast<std::int16_t>(sprite.position),
                            CompactCharacter(sprite));
      }
    }
    std::ranges::sort(glyphs);
    std::string text;
    for (const auto& [x, letter] : glyphs) {
      static_cast<void>(x);
      text += letter;
    }
    return text;
  }
};
}  // namespace container_capacity_live_tests
