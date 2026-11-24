#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/LearningCatalog.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"

namespace VS::Extract {
inline constexpr std::size_t kLearningSourceOffset = 0x405E4;
inline constexpr std::size_t kArtRequirementBytes = 80;
inline constexpr std::size_t kUnusedArtActionBytes = 40;
inline constexpr std::size_t kAbilityRequirementBytes = 44;
inline constexpr std::size_t kActorExtraShapeBytes = 256;
inline constexpr std::size_t kLearningImportBytes = 420;
inline constexpr std::string_view kLearningImportPath{
    "catalogs/battle/import/learning.json",
};

struct ImportedLearning {
  Content::LearningCatalog definitions;
  std::array<std::byte, kUnusedArtActionBytes> unused_art_actions{};
  std::array<std::byte, kActorExtraShapeBytes> actor_extra_shapes{};
};

// Offline only. The last two fields preserve neighboring source data, not
// learning properties. Runtime never reads this record or calls the encoder.
[[nodiscard]] std::expected<ImportedLearning, std::string> DecodeLearning(
    std::span<const std::byte> source_section);
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeLearning(
    const ImportedLearning& imported);
[[nodiscard]] std::string FormatLearningProvenance(
    const ImportedLearning& imported, std::string_view source_sha256);
[[nodiscard]] std::expected<ImportedLearning, std::string>
ParseLearningProvenance(std::span<const std::byte> text);
[[nodiscard]] std::expected<TableConversionReport, std::string>
ConvertDiscLearning(const std::filesystem::path& root, bool force);
}  // namespace VS::Extract
