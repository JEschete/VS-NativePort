#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ImageFiles.hpp"

namespace VS::Extract {

// Encodes one document in a fixed order; identical input gives identical
// bytes. Runtime code reads these files through VS.Content.
[[nodiscard]] std::expected<std::vector<Content::ImageFile>, std::string>
EncodeImageFiles(const Content::ImageDocument& document, std::string_view stem);

// Writes every file through a temporary path before replacing destinations.
[[nodiscard]] std::expected<void, std::string> WriteImageFiles(
    const std::filesystem::path& directory,
    const std::vector<Content::ImageFile>& files);

}  // namespace VS::Extract
