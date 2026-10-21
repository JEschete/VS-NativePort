#pragma once

#include <expected>
#include <string>

#include "Battle/Encounter.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Field/Mpd.h"
}

namespace VS::Game {

std::expected<Field_MpdEnemySpawn, std::string> ReadEncounterSpawnProfile(
    const EncounterEnemy& enemy);

std::expected<Field_MpdEnemySpawn, std::string> PrepareEncounterSpawn(
    const Field_MpdEnemySpawn& source, const EncounterSpawn& instance,
    std::size_t index);

}  // namespace VS::Game
