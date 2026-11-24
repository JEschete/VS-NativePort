#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "Content/CatalogTable.hpp"

namespace VS::Content {

struct CatalogTableSource {
  TableSpec spec;
  std::filesystem::path file;  // Relative to each data/layer root.
};

struct CatalogConflict {
  std::string table;
  std::string key;
  std::string previous_source;
  std::string winning_source;
};

// CSV is authoritative. Unchanged content can open a compiled SQLite cache
// read-only; otherwise a private immutable database is built.
// catalog-layers.json optionally lists patch/mod roots in increasing
// precedence.
class CatalogDatabase {
 public:
  ~CatalogDatabase();
  CatalogDatabase(CatalogDatabase&&) noexcept;
  CatalogDatabase& operator=(CatalogDatabase&&) noexcept;
  CatalogDatabase(const CatalogDatabase&) = delete;
  CatalogDatabase& operator=(const CatalogDatabase&) = delete;

  [[nodiscard]] static std::expected<CatalogDatabase, std::string> Load(
      const std::filesystem::path& root,
      std::span<const CatalogTableSource> sources, bool use_cache = true);
  [[nodiscard]] std::expected<std::vector<TableRow>, std::string> Read(
      const TableSpec& spec) const;
  [[nodiscard]] std::span<const CatalogConflict> Conflicts() const;
  [[nodiscard]] bool CacheHit() const;
  // Only publish after typed consumers have validated the compiled records.
  // Base/patch files are never written; publication is an atomic replacement.
  [[nodiscard]] std::expected<void, std::string> PublishCache() const;

 private:
  struct State;
  explicit CatalogDatabase(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
};

}  // namespace VS::Content
