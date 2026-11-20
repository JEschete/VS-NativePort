#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/AbilityState.h"
#include "Battle/ActionMenu.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Menu/Magic.hpp"

namespace hud_rendering_fixture {
class LiveShortcut {
 public:
  Actors_ActorRecord record{};
  Battle_ActorStats stats{};
  Battle_Actor actor{};

  LiveShortcut()
      : saved_character_(Battle_characterState),
        saved_record_(Actors_actorRecords[0]) {
    Capture();
    Battle_characterState = &actor;
    Actors_actorRecords[0] = &record;
    actor.stats = &stats;
    actor.model = &record.instance;
    actor.equippedWeaponCategory = 1;
    record.instance.weaponDrawn = 1;
    stats.currentHp = 1;
    stats.currentMp = 1;
    std::ranges::fill(VS::Battle::spellClassCounts, u_char{0});
    std::ranges::fill(VS::Battle::spellClassSpellIds, nullptr);
    std::ranges::fill(VS::Battle::chainAbilityActionIds, char{0});
    std::ranges::fill(VS::Battle::defenseAbilityActionIds, char{0});
    for (auto& action : Battle_actionDefinitions) action.unlocked = 0;
  }
  ~LiveShortcut() {
    Restore();
    Actors_actorRecords[0] = saved_record_;
    Battle_characterState = saved_character_;
  }
  LiveShortcut(const LiveShortcut&) = delete;
  LiveShortcut& operator=(const LiveShortcut&) = delete;
  LiveShortcut(LiveShortcut&&) = delete;
  LiveShortcut& operator=(LiveShortcut&&) = delete;

  static void SpellClass(std::size_t spell_class, std::uint8_t* first,
                         std::uint8_t count) {
    auto counts = std::span(VS::Battle::spellClassCounts);
    auto ids = std::span(VS::Battle::spellClassSpellIds);
    counts[spell_class] = count;
    ids[spell_class] = first;
  }

 private:
  Battle_Actor* saved_character_;
  Actors_ActorRecord* saved_record_;
  std::array<u_char, 4> counts_{};
  std::array<u_char*, 4> ids_{};
  std::array<char, 16> chain_{}, defense_{};
  std::array<std::uint8_t, 20> warlock_{}, sorcerer_{};
  std::array<std::uint8_t, 8> shaman_{}, enchanter_{};
  std::array<std::uint8_t, 256> unlocked_{};

  void Capture() {
    std::ranges::copy(VS::Battle::spellClassCounts, counts_.begin());
    std::ranges::copy(VS::Battle::spellClassSpellIds, ids_.begin());
    std::ranges::copy(VS::Battle::chainAbilityActionIds, chain_.begin());
    std::ranges::copy(VS::Battle::defenseAbilityActionIds, defense_.begin());
    std::ranges::copy(VS::Menu::Magic::warlockSpellIds, warlock_.begin());
    std::ranges::copy(VS::Menu::Magic::sorcererSpellIds, sorcerer_.begin());
    std::ranges::copy(VS::Menu::Magic::shamanSpellIds, shaman_.begin());
    std::ranges::copy(VS::Menu::Magic::enchanterSpellIds, enchanter_.begin());
    for (std::size_t index = 0; index < unlocked_.size(); ++index) {
      unlocked_.at(index) = static_cast<std::uint8_t>(
          std::span(Battle_actionDefinitions)[index].unlocked);
    }
  }
  void Restore() {
    std::ranges::copy(counts_, std::begin(VS::Battle::spellClassCounts));
    std::ranges::copy(ids_, std::begin(VS::Battle::spellClassSpellIds));
    std::ranges::copy(chain_, std::begin(VS::Battle::chainAbilityActionIds));
    std::ranges::copy(defense_,
                      std::begin(VS::Battle::defenseAbilityActionIds));
    std::ranges::copy(warlock_, std::begin(VS::Menu::Magic::warlockSpellIds));
    std::ranges::copy(sorcerer_, std::begin(VS::Menu::Magic::sorcererSpellIds));
    std::ranges::copy(shaman_, std::begin(VS::Menu::Magic::shamanSpellIds));
    std::ranges::copy(enchanter_,
                      std::begin(VS::Menu::Magic::enchanterSpellIds));
    for (std::size_t index = 0; index < unlocked_.size(); ++index) {
      std::span(Battle_actionDefinitions)[index].unlocked = unlocked_.at(index);
    }
  }
};
}  // namespace hud_rendering_fixture
