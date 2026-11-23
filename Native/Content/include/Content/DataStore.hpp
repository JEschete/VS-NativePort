#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/AssetCatalog.hpp"
#include "Content/AssetHandle.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Core/Types.hpp"

namespace VS::Content {

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
    std::unique_ptr<DataStore> m_previous_;
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
  // handle generation. When manifest.json lists workshop recipe tables, they
  // must load too.
  static std::expected<void, std::string> Open(
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
  // that each standalone TIM, effect FBC and FBT, workshop SYD and actor SHP,
  // SEQ and WEP has a current output (source and dependency hashes) whose
  // files are present.
  [[nodiscard]] std::expected<void, std::string> RequireConversions() const;
  [[nodiscard]] const ConversionManifest* Conversions() const {
    return m_conversion_manifest_ ? &*m_conversion_manifest_ : nullptr;
  }
  // The workshop recipe tables (catalogs\menu\), or null when manifest.json
  // lists none (synthetic test stores, data from before the tables).
  [[nodiscard]] const WorkshopRecipeCatalog* WorkshopRecipes() const {
    return m_workshop_recipes_ ? &*m_workshop_recipes_ : nullptr;
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
};

}  // namespace VS::Content
