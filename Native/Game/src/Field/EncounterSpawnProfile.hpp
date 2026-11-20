#pragma once

#include <expected>
#include <span>
#include <string>

#include "Battle/Encounter.hpp"
#include "Field/Mpd.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"

namespace VS::Game {

std::expected<std::vector<EncounterFloor>, std::string> ReadEncounterFloor(
    std::span<const std::uint8_t> collision,
    std::span<const std::uint8_t> tile_cells);

bool IsEncounterHostileSpawn(const VS::Field::MpdEnemySpawn& source);

bool UsesConfiguredEncounterPlacements(const EncounterSetup& setup, int zone,
                                       int room,
                                       bool external_event_started = false);
bool UsesEncounterRoomEvents(const EncounterSetup& setup,
                             bool external_event_started);

std::expected<VS::Field::MpdEnemySpawn, std::string> ReadEncounterSpawnProfile(
    const EncounterEnemy& enemy);

std::expected<VS::Items::WeaponSpawnLoadout, std::string> ReadEncounterWeapon(
    EncounterWeapon weapon);

std::expected<VS::Field::MpdEnemySpawn, std::string> PrepareEncounterSpawn(
    const VS::Field::MpdEnemySpawn& source, const EncounterSpawn& instance,
    std::size_t index);

}  // namespace VS::Game
