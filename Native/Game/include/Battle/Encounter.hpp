#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace VS::Game {

struct EncounterLimits {
  static constexpr int kEnemies = 8;
  static constexpr int kActorSlots = 16;
  static constexpr int kTiles = 32;
  static constexpr int kElevations = 4;
  static constexpr int kEnemyIdEnd = 127;
  static constexpr int kLastZone = 255;
};

struct EncounterPosition {
  int x = 0;
  int z = 0;
  int elevation = 0;
  int direction = 0;
};

struct EncounterEnemy {
  int index = 0;
  std::string name;
  int hp = 0;
  int mp = 0;
  bool secondary_model = false;
  std::string unavailable_reason;
  std::string spawn_asset;
  int spawn_index = -1;
};

enum class EncounterFloor { kSupported, kBlocked, kPit, kNoFloor };

struct EncounterRoom {
  int index = 0;
  std::string asset;
  int width = 0;
  int depth = 0;
  EncounterPosition entrance;
  std::string unavailable_reason;
  std::vector<EncounterFloor> floor;
};

struct EncounterZone {
  int id = 0;
  std::string asset;
  std::vector<EncounterRoom> rooms;
  std::vector<EncounterEnemy> enemies;
};

struct EncounterSpawn {
  int enemy = 0;
  EncounterPosition position;
};

struct EncounterSetup {
  int zone = 0;
  int room = 0;
  std::uint32_t seed = 1;
  EncounterPosition player;
  std::vector<EncounterSpawn> enemies;
};

// Reads the same extracted ZND/MPD definitions consumed by the room loader.
// Does not initialize gameplay or modify content files.
std::expected<std::vector<EncounterZone>, std::string> ReadEncounterCatalog();
std::vector<std::string> ValidateEncounter(const EncounterSetup& setup,
                                           const EncounterZone& zone);

struct EncounterActorState {
  int slot = 0;
  int hp = 0;
  int mp = 0;
  int risk = 0;
  std::uint32_t statuses = 0;
  int command = 0;
  int command_state = 0;
  int weapon_drawn = 0;
  std::uint32_t actor_flags = 0;
  int recovery = 0;
  int x = 0;
  int y = 0;
  int z = 0;
};

struct EncounterItemState {
  int slot = 0;
  int id = 0;
  int count = 0;
};

struct EncounterState {
  bool ready = false;
  int zone = 0;
  int room = 0;
  std::uint32_t control_mode = 0;
  std::uint32_t transition = 0;
  std::uint32_t native_buttons = 0;
  std::uint32_t native_pressed = 0;
  unsigned action_phase = 0;
  int action = 0;
  int source_actor = 0;
  int target_count = 0;
  std::vector<EncounterActorState> actors;
  std::vector<EncounterItemState> items;
};

// A single production session can be active: existing gameplay owns globals.
// RunEncounter must execute on a Platform::GameFiber, like Boot_RunGame.
std::expected<void, std::string> ConfigureEncounter(const EncounterSetup& setup,
                                                    const EncounterZone& zone);
void RunEncounter();
void RequestEncounterStop();
EncounterState InspectEncounter();

}  // namespace VS::Game
