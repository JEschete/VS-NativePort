#include "Battle/LearningLive.hpp"

#include "Content/DataStore.hpp"
#include "Core/Log.hpp"

namespace VS::Battle {
namespace {
const Content::LearningCatalog* Catalog() {
  const auto* store = Content::DataStore::Get();
  if (store == nullptr || store->Learning() == nullptr) {
    Core::LogWarn("battle.learning",
                  "learning catalog unavailable; run VS.Extract tables <dir>");
    return nullptr;
  }
  return store->Learning();
}
}  // namespace
std::optional<int> LiveBreakArtRequirement(int category, int learned) {
  const auto* catalog = Catalog();
  auto result = catalog == nullptr
                    ? std::nullopt
                    : catalog->BreakArtRequirement(category, learned);
  if (!result) {
    Core::LogWarn("battle.learning",
                  "Break Art requirement refused: category={} learned={}",
                  category, learned);
  }
  return result;
}
std::optional<int> LiveBattleAbilityRequirement(int unlocked) {
  const auto* catalog = Catalog();
  auto result =
      catalog == nullptr ? std::nullopt : catalog->AbilityRequirement(unlocked);
  if (!result) {
    Core::LogWarn("battle.learning",
                  "battle ability requirement refused: unlocked={}", unlocked);
  }
  return result;
}
}  // namespace VS::Battle
