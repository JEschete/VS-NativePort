// Globals the module table cannot fill because a published catalog owns their
// data. Content::LoadModuleData calls this after the module copies.
#include "Battle/ActionTable.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Log.hpp"

int VS::Content::LoadCatalogBackedGlobals(const DataStore& store) {
  const auto* actions = store.Actions();
  if (actions == nullptr) {
    Core::LogError("content",
                   "action catalog not published (run VS.Extract actions "
                   "<dir>)");
    return 1;
  }
  if (const auto built =
          Battle::BuildActionTable(*actions, Battle::actionDefinitions);
      !built) {
    Core::LogError("content", "action table: {}", built.error());
    return 1;
  }
  return 0;
}
