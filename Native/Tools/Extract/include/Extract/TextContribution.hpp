#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "Content/JsonText.hpp"
#include "Content/TextCatalogRegistration.hpp"
#include "Extract/ConverterBatch.hpp"

namespace VS::Extract {
inline constexpr std::string_view kTextCatalogConverter{
    Content::kNativeTextCatalogConverter,
};
inline constexpr auto kTextCatalogId = Content::kNativeTextCatalogId;
inline constexpr std::string_view kTextOwnershipPath{
    "text/strings.import.json",
};

struct TextContribution {
  std::string owner;
  std::string prefix;
  Content::SourceDependency source;
  Content::JsonDocument strings;
};
struct PreparedTextContribution {
  ConverterBatch batch;
  std::vector<Content::ImageFile> files;
};
// The sole writer of strings.en.json. Ownership is per ID; unchanged source
// contributions retain authored strings, and force replaces only that owner.
[[nodiscard]] std::expected<PreparedTextContribution, std::string>
PrepareTextContribution(const std::filesystem::path& root,
                        const Content::ConversionManifest& previous,
                        const TextContribution& contribution, bool force);
}  // namespace VS::Extract
