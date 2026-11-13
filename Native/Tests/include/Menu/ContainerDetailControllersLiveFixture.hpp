#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "Items/ContainerTransfersLiveFixture.hpp"
#include "Menu/Container/ContainerDetailControllersLive.hpp"
#include "Menu/ItemDetailDispatchLiveFixture.hpp"

namespace container_detail_controllers_tests {
namespace detail = VS::Menu::container;
namespace items = VS::Items;
namespace fw = VS::Menu::Framework;
using equipment_detail_tests::kBladeStrength;
using equipment_detail_tests::kGripStrength;
using equipment_detail_tests::ValueRestore;
inline constexpr std::size_t kContainerWeapon = 31;
inline constexpr std::size_t kContainerPart = 63;
inline constexpr std::size_t kContainerGem = 191;
inline constexpr int kContainerFrames = 11;
inline constexpr int kContainerRow = 20;
inline constexpr int kWeaponStrength = kBladeStrength + kGripStrength;
inline constexpr int kChangedStrength = 19;
inline constexpr std::uint8_t kFirstGripHelpId = 6;
inline constexpr std::uint8_t kGripCategory = 1;

struct ContainerLiveFixture : item_detail_tests::DispatchLiveFixture {
  container_transfers_tests::LiveFixture containers;
  ValueRestore<char> saved_container_title{Menu_Container_categoryTitleVisible};
  ValueRestore<char> saved_container_icons{Menu_Container_categoryIconsVisible};
  ValueRestore<char> saved_container_controls{Menu_Container_controlsPanelMode};
  ValueRestore<char> saved_container_phase{Menu_Container_detailsViewPhase};
  ValueRestore<int> saved_triangle{Menu_Container_detailsClosedWithTriangle};

  ContainerLiveFixture() {
    detail::ResetLiveContainerDetailControllers();
    Bind(containers.storage->container);
  }
  ~ContainerLiveFixture() { detail::ResetLiveContainerDetailControllers(); }

  static void Bind(Items_Container& owner) {
    Menu_Container_containerData = &owner;
    Menu_Framework_weapons = owner.items.weapons;
    Menu_Framework_blades = owner.items.blades;
    Menu_Framework_grips = owner.items.grips;
    Menu_Framework_shields = owner.items.shields;
    Menu_Framework_armor = owner.items.armor;
    Menu_Framework_gems = owner.items.gems;
  }
  void AddContainerWeapon() {
    AddWeapon();
    auto& owner = containers.storage->container;
    owner.items.blades[kContainerPart] = Audio_Seq_unusedInventory.blades[0];
    owner.items.grips[kContainerPart] = Audio_Seq_unusedInventory.grips[0];
    owner.items.grips[kContainerPart].id = kFirstGripHelpId;
    owner.items.grips[kContainerPart].category = kGripCategory;
    auto weapon = items::Live::Typed(Audio_Seq_unusedInventory.weapons[0]);
    weapon.blade = kContainerPart + 1;
    weapon.grip = kContainerPart + 1;
    items::Live::Commit(owner.items.weapons[kContainerWeapon], weapon);
    items::Live::Commit(owner.items.weapons[0], weapon);
    owner.indices.weapons[0] = kContainerWeapon + 1;
    owner.indices.weapons[1] = 1;
  }
  void AddContainerBlade() {
    AddParts();
    auto& owner = containers.storage->container;
    owner.items.blades[kContainerPart] =
        std::span(Audio_Seq_unusedInventory.blades).back();
    owner.items.blades[0] = Audio_Seq_unusedInventory.blades[0];
    owner.indices.blades[0] = kContainerPart + 1;
    owner.indices.blades[1] = 1;
  }
  void AddContainerGrip() {
    AddParts();
    auto& owner = containers.storage->container;
    owner.items.grips[kContainerPart] =
        std::span(Audio_Seq_unusedInventory.grips).back();
    owner.items.grips[0] = Audio_Seq_unusedInventory.grips[0];
    owner.items.grips[kContainerPart].id = kFirstGripHelpId + 1;
    owner.items.grips[kContainerPart].category = kGripCategory;
    owner.items.grips[0].id = kFirstGripHelpId;
    owner.items.grips[0].category = kGripCategory;
    owner.indices.grips[0] = kContainerPart + 1;
    owner.indices.grips[1] = 1;
  }
  void AddContainerShield() {
    AddShield();
    auto& owner = containers.storage->container;
    owner.items.shields[kContainerWeapon] =
        Audio_Seq_unusedInventory.shields[0];
    owner.items.shields[0] = Audio_Seq_unusedInventory.shields[0];
    owner.indices.shields[0] = kContainerWeapon + 1;
    owner.indices.shields[1] = 1;
  }
  void AddContainerArmor() {
    AddArmor();
    auto& owner = containers.storage->container;
    owner.items.armor[0] = Audio_Seq_unusedInventory.armor[0];
    owner.items.armor[kContainerPart] =
        std::span(Audio_Seq_unusedInventory.armor).back();
    owner.indices.armor[0] = 1;
    owner.indices.armor[1] = kContainerPart + 1;
  }
  void AddContainerGems() {
    AddGems();
    auto& owner = containers.storage->container;
    owner.items.gems[0] = Audio_Seq_unusedInventory.gems[0];
    owner.items.gems[kContainerGem] =
        std::span(Audio_Seq_unusedInventory.gems).back();
    owner.indices.gems[0] = kContainerGem + 1;
    owner.indices.gems[1] = 1;
  }
  void Advance(int (*update)(int)) {
    for (int tick = 0; tick < kContainerFrames; ++tick) {
      IdleRows();
      (void)update(0);
    }
  }
};
}  // namespace container_detail_controllers_tests
