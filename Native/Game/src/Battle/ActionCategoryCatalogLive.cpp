#include "Battle/ActionCategoryCatalogLive.hpp"

#include <cstdlib>

#include "Content/DataStore.hpp"
#include "Core/Diagnostics.hpp"

namespace VS::Battle {
const Content::ActionCategoryCatalog* LiveActionCategoryCatalog() {
  const auto* store = Content::DataStore::Get();
  return store != nullptr ? store->ActionCategories() : nullptr;
}
const Content::ActionCategoryCatalog& RequireLiveActionCategoryCatalog() {
  const auto* catalog = LiveActionCategoryCatalog();
  if (catalog == nullptr) {
    Core::Errorf("battle",
                 "Action category catalog not loaded; run VS.Extract tables");
    std::abort();
  }
  return *catalog;
}
const Content::ActionCategoryRule& RequireLiveActionCategory(int category) {
  const auto* rule = RequireLiveActionCategoryCatalog().Rule(category);
  if (rule == nullptr) {
    Core::Errorf("battle", "Action category %d is not supported", category);
    std::abort();
  }
  return *rule;
}
}  // namespace VS::Battle
