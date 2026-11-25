#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ActionCatalog.hpp"
#include "Content/ActionCategoryCatalog.hpp"
#include "Content/AssetCatalog.hpp"
#include "Content/AssetHandle.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/LearningCatalog.hpp"
#include "Content/MaterialCatalog.hpp"
#include "Content/NativeText.hpp"
#include "Content/StatusCatalog.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Core/Types.hpp"

namespace VS::Content {

enum class SourceVerification { kCached, kFull };

struct SourceVerificationStats {
  std::size_t cache_hits = 0;
  std::size_t files_hashed = 0;
  u64 bytes_hashed = 0;
};

// The extracted game data: what VS.Extract wrote from the user's disc
// (Native\data by default, never committed). Files keep their disc paths under
// disc\; files.tsv lists each one's first sector, size and form ("raw" = the
// disc file as is; otherwise the converted file's extension, e.g. "mkv" for
// MOV/TITLE.STR.mkv; "stream" = an interleaved file not converted), so code
// that still addresses the disc by sector finds it. catalogs\ holds the asset
// catalog (AssetCatalog.hpp), the runtime contract migrated consumers read
// assets through instead of sectors.
class DataStore {
 public:
  // Exclusive, stack-nested override of the process-wide service. Keeps the
  // exact previous instance alive (including borrowed Entry pointers), then
  // restores ownership without reopening its files. Not concurrent with Open,
  // other overrides, or readers whose current-store lifetime is not guarded.
  class ScopedOverride {
   public:
    ScopedOverride();
    ~ScopedOverride();
    ScopedOverride(const ScopedOverride&) = delete;
    ScopedOverride& operator=(const ScopedOverride&) = delete;
    ScopedOverride(ScopedOverride&&) = delete;
    ScopedOverride& operator=(ScopedOverride&&) = delete;

    void Restore() noexcept;

   private:
    std::shared_ptr<DataStore> m_previous_;
    bool m_active_ = true;
  };

  static constexpr u32 kDataSectorBytes = 2048;
  struct Entry {
    std::string path;  // disc path, e.g. "TITLE/TITLE.PRG"
    u32 lba = 0;
    u32 size = 0;
    std::string form;  // "raw", or the converted form

    u32 SectorCount() const;
    bool ContainsSector(u32 sector) const;
  };

  static constexpr const char* ManifestName = "files.tsv";

  // Reads root's files.tsv alone (the asset importer's input), sorted by lba.
  static std::expected<std::vector<Entry>, std::string> LoadManifest(
      const std::filesystem::path& root);
  // Opens the process-wide store (used by the C API in Content/DataAccess.h):
  // the manifest and, when root\catalogs\ holds assets.json or audio.json,
  // the asset catalog, which must then load. Each open catalog gets a fresh
  // handle generation. Catalog tables listed in manifest.json must load too.
  static std::expected<void, std::string> Open(
      const std::filesystem::path& root);
  // Open for read-only callers that repeatedly need the same extracted data
  // (the Debug test suite's real-data fixtures): the first call opens `root`;
  // later calls publish that same store again, with the same asset handle
  // generation, until files.tsv or manifest.json changes or another root is
  // asked for. The store is immutable once open. Use Open when a caller needs
  // a fresh generation or reads a store it will edit. Not concurrent with
  // Open or other overrides.
  static std::expected<void, std::string> OpenShared(
      const std::filesystem::path& root);
  static const DataStore* Get();

  const std::filesystem::path& Root() const { return m_root_; }
  const std::vector<Entry>& Entries() const { return m_entries_; }
  const Entry* Find(std::string_view path) const;
  const Entry* FindBySector(
      u32 lba) const;  // the file whose sectors include `lba`
  std::filesystem::path FileOf(const Entry& e) const;

  bool Read(const Entry& e, u64 offset, std::span<u8> out) const;
  std::expected<std::vector<u8>, std::string> ReadAll(const Entry& e) const;

  // Null when the store has no asset catalog files (synthetic test stores).
  [[nodiscard]] const AssetCatalog* Catalog() const {
    return m_catalog_ ? &*m_catalog_ : nullptr;
  }
  // The game and tools that play from this store need its catalog; the error
  // says how to make one.
  [[nodiscard]] std::expected<void, std::string> RequireCatalog() const;
  // Requires the converters used by native runtime consumers and verifies
  // that each standalone TIM, effect FBC and FBT, workshop SYD, actor SHP,
  // SEQ and WEP, and sound bank has a current output (source and dependency
  // hashes) whose files are present, and that each XA track and movie has an
  // output whose files are present. Empty placeholder files need none.
  // Original sources may reuse validated size/time/hash stamps. kFull ignores
  // stamp hits; native authoring files never use timestamp-only validation.
  [[nodiscard]] std::expected<void, std::string> RequireConversions(
      SourceVerification verification = SourceVerification::kCached,
      SourceVerificationStats* statistics = nullptr) const;
  [[nodiscard]] const ConversionManifest* Conversions() const {
    return m_conversion_manifest_ ? &*m_conversion_manifest_ : nullptr;
  }
  // The workshop recipe tables (catalogs\menu\), or null when manifest.json
  // lists none (synthetic test stores, data from before the tables).
  [[nodiscard]] const WorkshopRecipeCatalog* WorkshopRecipes() const {
    return m_workshop_recipes_ ? &*m_workshop_recipes_ : nullptr;
  }
  [[nodiscard]] const MaterialCatalog* Materials() const {
    return m_materials_ ? &*m_materials_ : nullptr;
  }
  [[nodiscard]] const LearningCatalog* Learning() const {
    return m_learning_ ? &*m_learning_ : nullptr;
  }
  [[nodiscard]] const StatusCatalog* StatusRules() const {
    return m_status_ ? &*m_status_ : nullptr;
  }
  [[nodiscard]] const ActionCategoryCatalog* ActionCategories() const {
    return m_action_categories_ ? &*m_action_categories_ : nullptr;
  }
  [[nodiscard]] const ActionCatalog* Actions() const {
    return m_actions_ ? &*m_actions_ : nullptr;
  }
  [[nodiscard]] const NativeTextAssets* SaveText() const {
    return m_save_text_ ? &*m_save_text_ : nullptr;
  }
  [[nodiscard]] const CatalogDatabase* CatalogTables() const {
    return m_catalog_tables_ ? &*m_catalog_tables_ : nullptr;
  }
  // Reads `asset` into `out` as the original's whole-sector load did: its
  // bytes, then zeros. Refuses an asset this store's catalog did not issue
  // and a destination past the asset's load size.
  [[nodiscard]] std::expected<void, std::string> ReadAsset(
      AssetHandle asset, std::span<u8> out) const;

 private:
  std::filesystem::path m_root_;
  std::vector<Entry> m_entries_;  // sorted by lba
  std::optional<AssetCatalog> m_catalog_;
  std::optional<ConversionManifest> m_conversion_manifest_;
  std::optional<WorkshopRecipeCatalog> m_workshop_recipes_;
  std::optional<MaterialCatalog> m_materials_;
  std::optional<LearningCatalog> m_learning_;
  std::optional<StatusCatalog> m_status_;
  std::optional<ActionCategoryCatalog> m_action_categories_;
  std::optional<ActionCatalog> m_actions_;
  std::optional<NativeTextAssets> m_save_text_;
  std::optional<CatalogDatabase> m_catalog_tables_;
};

}  // namespace VS::Content
