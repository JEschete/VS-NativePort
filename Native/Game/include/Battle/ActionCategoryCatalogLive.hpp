#pragma once

#include "Content/ActionCategoryCatalog.hpp"

namespace VS::Battle {
[[nodiscard]] const Content::ActionCategoryCatalog* LiveActionCategoryCatalog();
[[nodiscard]] const Content::ActionCategoryCatalog&
RequireLiveActionCategoryCatalog();
[[nodiscard]] const Content::ActionCategoryRule& RequireLiveActionCategory(
    int category);
}  // namespace VS::Battle
