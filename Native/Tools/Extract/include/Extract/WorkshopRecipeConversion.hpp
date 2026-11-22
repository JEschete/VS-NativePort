#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Extract/ConverterBatch.hpp"

// The syd-recipe-csv converter (ASSET_PIPELINE.md §6.1, #499): the workshop
// recipe files MENU\BLADE.SYD, SHIELD.SYD and ARMOR.SYD become CSV tables with
// their schemas under catalogs\menu\ (WorkshopRecipes.hpp). A SYD file holds
// three little-endian offsets (12 bytes), the result matrix (pair_side
// squared bytes, padded with zeros to four), the material matrix and the
// result records, each record starting with its own index.
namespace VS::Extract {

// The kind MENU/<KIND>.SYD holds, if it is one of the three.
[[nodiscard]] std::optional<Content::WorkshopKind> WorkshopKindForDiscPath(
    std::string_view disc_path);

// The tables a SYD file holds. The file must have exactly its layout's size,
// offsets and zero padding, every record must start with its index, and each
// matrix entry must name a record or a material (1-7).
[[nodiscard]] std::expected<Content::WorkshopRecipeTable, std::string>
DecodeSydRecipes(Content::WorkshopKind kind, std::span<const std::byte> file);
// The SYD file `table` makes; DecodeSydRecipes reads it back to `table`.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
EncodeSydRecipes(const Content::WorkshopRecipeTable& table);

// A converted table or schema file: its name under catalogs/menu.
struct TableFile {
  std::string name;
  std::string text;
};

struct ConvertedTables {
  Content::ConversionOutput output;
  std::vector<TableFile> files;
};

// The tables and schemas of one SYD file and its manifest entry. The tables
// must read back to the decoded ones, and those must rebuild the exact source
// bytes.
[[nodiscard]] std::expected<ConvertedTables, std::string> ConvertSydRecipes(
    const SourceFile& source);

struct TableConversionReport {
  std::size_t converted = 0;
  std::size_t unchanged = 0;  // source and converter version match
  std::size_t removed = 0;    // files no current output lists
};

// Converts every SYD file files.tsv lists under `root`, then records them in
// root\manifest.json. An output whose source and converter version are
// unchanged and whose files exist is kept as it is (so edits to the tables
// survive) unless `force`.
[[nodiscard]] std::expected<TableConversionReport, std::string>
ConvertDiscTables(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
