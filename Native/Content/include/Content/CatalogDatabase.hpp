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

// CSV is authoritative. Each load builds a private, immutable SQLite database;
// no cache or patch operation writes to the base files. catalog-layers.json
// optionally lists patch/mod roots in increasing precedence.
class CatalogDatabase {
 public:
  ~CatalogDatabase();
  CatalogDatabase(CatalogDatabase&&) noexcept;
  CatalogDatabase& operator=(CatalogDatabase&&) noexcept;
  CatalogDatabase(const CatalogDatabase&) = delete;
  CatalogDatabase& operator=(const CatalogDatabase&) = delete;

  [[nodiscard]] static std::expected<CatalogDatabase, std::string> Load(
      const std::filesystem::path& root,
      std::span<const CatalogTableSource> sources);
  [[nodiscard]] std::expected<std::vector<TableRow>, std::string> Read(
      const TableSpec& spec) const;
  [[nodiscard]] std::span<const CatalogConflict> Conflicts() const;

 private:
  struct State;
  explicit CatalogDatabase(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
};

}  // namespace VS::Content
