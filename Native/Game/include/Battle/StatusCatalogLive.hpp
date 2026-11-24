#pragma once

#include "Content/StatusCatalog.hpp"

namespace VS::Battle {
[[nodiscard]] const Content::StatusCatalog* LiveStatusCatalog();
[[nodiscard]] const Content::StatusCatalog& RequireLiveStatusCatalog();
[[nodiscard]] const Content::StatusRule& RequireLiveStatusRule(int rule);
}  // namespace VS::Battle
