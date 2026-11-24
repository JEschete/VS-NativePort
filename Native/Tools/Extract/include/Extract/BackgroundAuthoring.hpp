#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>

#include "Extract/ConverterBatch.hpp"

namespace VS::Extract {

struct BackgroundAuthoringReport {
  std::size_t identities = 0;
  std::size_t files = 0;
  std::size_t removed = 0;
};

// Move existing native files without reimporting their values. The manifest is
// published last; conflicting destinations or identities refuse the migration.
[[nodiscard]] std::expected<BackgroundAuthoringReport, std::string>
UpgradeBackgroundAuthoring(const std::filesystem::path& root);

// Preserve explicit custom background outputs and append the four source-ID
// aliases for whichever canonical outputs this batch contains.
[[nodiscard]] std::expected<void, std::string> KeepBackgroundCompatibility(
    ConverterBatch& batch, const Content::ConversionManifest& previous);

}  // namespace VS::Extract
