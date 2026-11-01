// Hand-maintained tests for the typed actor movement commands (#247).
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/MovementCommand.hpp"
#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
}

namespace {
struct StoredCommand {
  int actor_id;
  VS::Actors::MovementCommand command;
};

class RecordingRequests final : public VS::Actors::ActorMovementRequests {
 public:
  bool StoreMovementRequest(
      int actor_id, const VS::Actors::MovementCommand& command) override {
    stored.push_back({actor_id, command});
    return true;
  }
  std::vector<StoredCommand> stored;
};
}  // namespace

TEST_CASE("Tile-target commands pack direction and facing in one word",
          "[actors][movement][movement-command]") {
  using VS::Actors::MakeTileTargetCommand;
  using VS::Actors::MovementCommand;
  CHECK(MakeTileTargetCommand(0x300, 1024, 0x800) ==
        MovementCommand{VS::Actors::kMovementRequestTileTarget, 0x300, 0x800,
                        1024u});
  // Direction bits above 16 share the packed word with the facing.
  CHECK(MakeTileTargetCommand(0x12345, 7, 0x800) ==
        MovementCommand{VS::Actors::kMovementRequestTileTarget, 0x2345, 0x0801,
                        7u});
  CHECK(MakeTileTargetCommand(-1, -2, 0x10) ==
        MovementCommand{VS::Actors::kMovementRequestTileTarget, -1, -1,
                        0xFFFFFFFEu});
}

TEST_CASE("Movement commands write only through the request table",
          "[actors][movement][movement-command]") {
  RecordingRequests requests;
  VS::Actors::SetDirectMovement(requests, 3, 0xC00, 1536);
  VS::Actors::QueueNavigationTileTarget(requests, 5, 0x300, 1024, 0x800);
  REQUIRE(requests.stored.size() == 2);
  CHECK(requests.stored[0].actor_id == 3);
  CHECK(requests.stored[0].command ==
        VS::Actors::MovementCommand{VS::Actors::kMovementRequestIdle, 0xC00,
                                    0xC00, 1536u});
  CHECK(requests.stored[1].actor_id == 5);
  CHECK(requests.stored[1].command ==
        VS::Actors::MakeTileTargetCommand(0x300, 1024, 0x800));
}

TEST_CASE("Promoted movement commands have no original module address",
          "[actors][movement][movement-command]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800B64A8u) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800B64CCu) == nullptr);
}

TEST_CASE("The actor-table adapter rejects IDs outside the table",
          "[actors][movement][movement-command]") {
  VS::Actors::ActorMovementRequests& requests =
      VS::Actors::ActorRecordMovementRequests();
  CHECK_FALSE(requests.StoreMovementRequest(-1, {}));
  CHECK_FALSE(requests.StoreMovementRequest(
      static_cast<int>(std::size(Actors_actorRecords)), {}));
  Actors_ActorRecord* saved = Actors_actorRecords[3];
  Actors_actorRecords[3] = nullptr;
  CHECK_FALSE(requests.StoreMovementRequest(3, {}));
  Actors_actorRecords[3] = saved;
}

TEST_CASE("The actor-table adapter writes only the request fields",
          "[actors][movement][movement-command]") {
  VS::Actors::ActorMovementRequests& requests =
      VS::Actors::ActorRecordMovementRequests();
  Actors_ActorRecord* saved = Actors_actorRecords[3];
  Actors_ActorRecord actor;
  std::memset(&actor, 0xA5, sizeof actor);
  Actors_actorRecords[3] = &actor;
  CHECK(requests.StoreMovementRequest(
      3, VS::Actors::MakeTileTargetCommand(0x300, 1024, 0x800)));
  CHECK(actor.turnState.mode == VS::Actors::kMovementRequestTileTarget);
  CHECK(actor.turnState.reserved1 == 0);
  CHECK(actor.turnState.direction == 0x300);
  CHECK(actor.turnState.facing == 0x800);
  CHECK(actor.turnState.distance == 1024u);
  CHECK(static_cast<unsigned>(actor.turnState.reservedC) == 0xA5A5A5A5u);
  Actors_actorRecords[3] = saved;
}
