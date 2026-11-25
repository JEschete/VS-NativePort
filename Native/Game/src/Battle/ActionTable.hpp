#pragma once
// The action table: the 256 action definitions the battle code reads.
// ActionTable.cpp defines it; Content::LoadModuleData fills it at start-up from
// the published Content::ActionCatalog.

#include <array>
#include <cstddef>
#include <expected>
#include <string>

#include "Battle/Action.hpp"

namespace VS::Content {
struct ActionCatalog;
}

namespace VS::Battle {

inline constexpr std::size_t kActionTableCount = 256;
using ActionDefinitionTable = std::array<ActionDefinition, kActionTableCount>;

// decomp: vs_main_actions (src/SLUS_010.40/actions.h:64)
extern ActionDefinitionTable actionDefinitions;

// Fills `table` with the record layout the battle code still reads, from the
// typed catalog. Temporary bridge (EC-04): the callers move to
// Content::ActionCatalog rules and this retires with them (#499, #58). Bits no
// code reads (the reserved flag bits and word) come out zero.
[[nodiscard]] std::expected<void, std::string> BuildActionTable(
    const Content::ActionCatalog& catalog, ActionDefinitionTable& table);

}  // namespace VS::Battle
