#pragma once

#include <expected>
#include <string>
#include <vector>

#include "Content/TimFile.hpp"

namespace VS::Extract {

// Rebuilds the exact TIM bytes. This writer belongs to the offline importer,
// not the runtime format reader.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeTim(
    const Content::TimFile& tim);

}  // namespace VS::Extract
