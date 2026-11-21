#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"

namespace VS::Extract {

// The manifest's text: outputs by id, then a newline. Identical manifests
// give identical bytes.
[[nodiscard]] std::expected<std::string, std::string> FormatConversionManifest(
    const Content::ConversionManifest& manifest);

// Writes root\manifest.json through a temporary file.
[[nodiscard]] std::expected<void, std::string> WriteConversionManifest(
    const std::filesystem::path& root,
    const Content::ConversionManifest& manifest);

// Replaces outputs made by one converter and records its version.
void ReplaceConverterOutputs(Content::ConversionManifest& manifest,
                             std::string_view converter, std::uint32_t version,
                             std::vector<Content::ConversionOutput> outputs);

}  // namespace VS::Extract
