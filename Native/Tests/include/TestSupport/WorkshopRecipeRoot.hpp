#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "Content/DataStore.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"

namespace VS::test_support {

// The id the converter gives `kind`'s record `legacy`.
[[nodiscard]] inline std::string WorkshopResultId(Content::WorkshopKind kind,
                                                  std::uint32_t legacy) {
  return std::format("{}-{:03}", Content::LayoutOf(kind).name, legacy);
}

// Every record `kind`'s SYD file holds (record i has legacy id i and the
// converter's id), all fields zero, with no pairs and no material rules.
[[nodiscard]] inline Content::WorkshopRecipeTable FullWorkshopTable(
    Content::WorkshopKind kind) {
  const Content::WorkshopLayout& layout = Content::LayoutOf(kind);
  Content::WorkshopRecipeTable table{};
  table.kind = kind;
  const auto records =
      static_cast<std::uint32_t>(layout.first_pair_id + layout.pair_side);
  for (std::uint32_t legacy = 0; legacy < records; ++legacy) {
    if (kind == Content::WorkshopKind::kBlade) {
      Content::BladeResult blade{};
      blade.id = WorkshopResultId(kind, legacy);
      blade.legacy_id = legacy;
      table.blades.push_back(std::move(blade));
    } else {
      Content::PieceResult piece{};
      piece.id = WorkshopResultId(kind, legacy);
      piece.legacy_id = legacy;
      table.pieces.push_back(std::move(piece));
    }
  }
  return table;
}

// A temporary data root whose files.tsv lists MENU\BLADE.SYD, SHIELD.SYD and
// ARMOR.SYD as raw files. Tables() starts as each kind's FullWorkshopTable;
// WriteSources encodes them as the SYD files.
class WorkshopRecipeRoot {
 public:
  WorkshopRecipeRoot()
      : root_(std::filesystem::temp_directory_path() /
              std::format(
                  "vs-workshop-recipes-{}-{}",
                  std::chrono::steady_clock::now().time_since_epoch().count(),
                  sequence++)) {
    for (const Content::WorkshopKind kind : Content::kWorkshopKinds) {
      tables_.at(static_cast<std::size_t>(kind)) = FullWorkshopTable(kind);
    }
  }
  // NOLINTNEXTLINE(bugprone-exception-escape): errors use the error_code API.
  ~WorkshopRecipeRoot() {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
    if (error) {
      std::fprintf(stderr, "workshop recipe fixture left files: %s\n",
                   error.message().c_str());
    }
  }
  WorkshopRecipeRoot(const WorkshopRecipeRoot&) = delete;
  WorkshopRecipeRoot& operator=(const WorkshopRecipeRoot&) = delete;
  WorkshopRecipeRoot(WorkshopRecipeRoot&&) = delete;
  WorkshopRecipeRoot& operator=(WorkshopRecipeRoot&&) = delete;

  [[nodiscard]] const std::filesystem::path& Root() const { return root_; }
  [[nodiscard]] Content::WorkshopRecipeTable& Table(
      Content::WorkshopKind kind) {
    return tables_.at(static_cast<std::size_t>(kind));
  }

  void WriteSources() const {
    std::filesystem::create_directories(root_ / "disc" / "MENU");
    std::ofstream manifest(root_ / Content::DataStore::ManifestName);
    manifest << "lba\tsize\tform\tpath\n";
    std::uint32_t lba = kFirstSector;
    for (const Content::WorkshopRecipeTable& table : tables_) {
      const auto bytes = Extract::EncodeSydRecipes(table);
      REQUIRE(bytes.has_value());
      const std::string_view source = Content::LayoutOf(table.kind).source;
      std::ofstream file(root_ / "disc" / std::filesystem::path(source),
                         std::ios::binary | std::ios::trunc);
      file.write(reinterpret_cast<const char*>(bytes->data()),
                 static_cast<std::streamsize>(bytes->size()));
      REQUIRE(file.good());
      manifest << lba << '\t' << bytes->size() << "\traw\t" << source << '\n';
      lba += kSectorsPerFile;
    }
    REQUIRE(manifest.good());
  }

  // Runs the tables converter; fails the test when it fails.
  [[nodiscard]] Extract::TableConversionReport Convert(bool force) const {
    auto report = Extract::ConvertDiscTables(root_, force);
    if (!report) FAIL(report.error());
    return report.value_or(Extract::TableConversionReport{});
  }

  [[nodiscard]] std::filesystem::path TableFile(
      Content::WorkshopKind kind, Content::WorkshopTablePart part) const {
    return root_ /
           std::filesystem::path(Content::WorkshopTablePath(kind, part));
  }

 private:
  static constexpr std::uint32_t kFirstSector = 100;
  static constexpr std::uint32_t kSectorsPerFile = 8;
  static inline unsigned sequence = 0;
  std::filesystem::path root_;
  std::array<Content::WorkshopRecipeTable, Content::kWorkshopKindCount> tables_;
};

}  // namespace VS::test_support
