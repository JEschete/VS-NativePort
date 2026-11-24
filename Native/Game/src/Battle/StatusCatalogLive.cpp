#include "Battle/StatusCatalogLive.hpp"

#include <cstdlib>

#include "Content/DataStore.hpp"
#include "Core/Diagnostics.hpp"

namespace VS::Battle {
const Content::StatusCatalog* LiveStatusCatalog() {
  const auto* store = Content::DataStore::Get();
  return store != nullptr ? store->StatusRules() : nullptr;
}
const Content::StatusCatalog& RequireLiveStatusCatalog() {
  const auto* catalog = LiveStatusCatalog();
  if (catalog == nullptr) {
    Core::Errorf("battle", "Status catalog not loaded; run VS.Extract tables");
    std::abort();
  }
  return *catalog;
}
const Content::StatusRule& RequireLiveStatusRule(int rule) {
  const auto* definition = RequireLiveStatusCatalog().Rule(rule);
  if (definition == nullptr) {
    Core::Errorf("battle", "Status rule index %d is not supported", rule);
    std::abort();
  }
  return *definition;
}
}  // namespace VS::Battle
