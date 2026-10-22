#pragma once

#include <expected>
#include <span>
#include <string>

#include "Battle/Encounter.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Field/Mpd.h"
#include "Items/Loadout.h"
}

namespace VS::Game {

std::expected<std::vector<EncounterFloor>, std::string> ReadEncounterFloor(
    std::span<const std::uint8_t> collision,
    std::span<const std::uint8_t> tile_cells);

bool IsEncounterHostileSpawn(const Field_MpdEnemySpawn& source);

std::expected<Field_MpdEnemySpawn, std::string> ReadEncounterSpawnProfile(
    const EncounterEnemy& enemy);

std::expected<Items_WeaponSpawnLoadout, std::string> ReadEncounterWeapon(
    EncounterWeapon weapon);

std::expected<Field_MpdEnemySpawn, std::string> PrepareEncounterSpawn(
    const Field_MpdEnemySpawn& source, const EncounterSpawn& instance,
    std::size_t index);

}  // namespace VS::Game
