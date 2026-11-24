#include "Content/CatalogDatabase.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/JsonText.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
namespace fs = std::filesystem;

using SqlConnection = std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
using Aliases = std::map<std::string, std::map<std::string, std::string>>;

// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct SourceTable {
  CatalogTableSource source;
  std::vector<std::optional<TableRow>> rows;
  std::map<std::string, std::string> writers;
};

std::expected<std::string, std::string> ReadText(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::unexpected("cannot open " + Platform::PathText(path));
  std::string text{std::istreambuf_iterator<char>(input),
                   std::istreambuf_iterator<char>()};
  if (input.bad()) {
    return std::unexpected("cannot read " + Platform::PathText(path));
  }
  return text;
}

std::expected<bool, std::string> Exists(const fs::path& path) {
  std::error_code error;
  const bool exists = fs::exists(path, error);
  if (error) {
    return std::unexpected(Platform::PathText(path) + ": " + error.message());
  }
  return exists;
}

std::expected<std::vector<fs::path>, std::string> LayerRoots(
    const fs::path& root) {
  const auto file = root / "catalog-layers.json";
  auto exists = Exists(file);
  if (!exists) return std::unexpected(exists.error());
  if (!*exists) return std::vector<fs::path>{};
  auto text = ReadText(file);
  if (!text) return std::unexpected(text.error());
  const std::string name = Platform::PathText(file);
  auto json = ParseJsonText(std::as_bytes(std::span(*text)), name);
  if (!json) return std::unexpected(json.error());
  const JsonMembers members(*json, name);
  if (auto only = members.Only({"schema", "schema_version", "layers"}); !only) {
    return std::unexpected(only.error());
  }
  auto schema = members.String("schema");
  auto version = members.Integer("schema_version", 1, 1);
  auto layers = members.Get("layers");
  if (!schema) return std::unexpected(schema.error());
  if (!version) return std::unexpected(version.error());
  if (*schema != "vs.catalog-layers") {
    return std::unexpected(name + ": expected schema vs.catalog-layers");
  }
  if (!layers) return std::unexpected(layers.error());
  if (!(**layers).is_array()) {
    return std::unexpected(name + ": layers must be an ordered array of paths");
  }
  std::vector<fs::path> roots;
  for (const auto& layer : **layers) {
    if (!layer.is_string() || layer.get_ref<const std::string&>().empty()) {
      return std::unexpected(name + ": each layer must be a nonempty path");
    }
    const fs::path path = root / Platform::Utf8Path(layer.get<std::string>());
    std::error_code error;
    if (!fs::is_directory(path, error)) {
      return std::unexpected(name + ": missing/unreadable layer " +
                             Platform::PathText(path) +
                             (error ? ": " + error.message() : ""));
    }
    std::vector<fs::path> previous = roots;
    previous.push_back(root);
    for (const auto& earlier : previous) {
      const bool same = fs::equivalent(path, earlier, error);
      if (error) return std::unexpected(name + ": " + error.message());
      if (same) {
        return std::unexpected(name + ": repeated/base layer " +
                               Platform::PathText(path));
      }
    }
    roots.push_back(path);
  }
  return roots;
}

std::size_t ColumnIndex(const TableSpec& spec, std::string_view name) {
  const auto found =
      std::ranges::find_if(spec.columns, [name](const auto& column) {
        return column.name == name ||
               (!column.legacy_name.empty() && column.legacy_name == name);
      });
  return static_cast<std::size_t>(found - spec.columns.begin());
}

std::expected<Aliases, std::string> BuildAliases(
    const std::vector<SourceTable>& tables) {
  Aliases aliases;
  for (const auto& table : tables) {
    const auto& spec = table.source.spec;
    const auto id = ColumnIndex(spec, "id");
    if (id == spec.columns.size()) continue;
    const auto legacy = ColumnIndex(spec, "legacy_id");
    auto& names = aliases[std::string(spec.name)];
    for (const auto& row : table.rows) {
      if (!row) continue;
      const auto add = [&](const std::string& alias) {
        const auto [found, added] = names.emplace(alias, row->cells[id]);
        return added || found->second == row->cells[id];
      };
      if (!add(row->cells[id]) ||
          (!spec.legacy_alias_prefix.empty() && legacy != spec.columns.size() &&
           !add(std::format("{}-{:03}", spec.legacy_alias_prefix,
                            row->values[legacy])))) {
        return std::unexpected(CellError(row->source, row->line, "id",
                                         "ambiguous legacy identity"));
      }
    }
  }
  return aliases;
}

std::string CanonicalCell(const TableSpec& spec, const ColumnSpec& column,
                          const std::string& cell, const Aliases& aliases) {
  const std::string_view target =
      column.type == ColumnType::kReference ? column.references : spec.name;
  if (column.type != ColumnType::kReference && column.type != ColumnType::kId) {
    return cell;
  }
  const auto table = aliases.find(std::string(target));
  if (table == aliases.end()) return cell;
  const auto found = table->second.find(cell);
  return found == table->second.end() ? cell : found->second;
}

std::string KeyOf(const TableSpec& spec, const TableRow& row,
                  const Aliases& aliases) {
  std::string key;
  for (const auto name : spec.key) {
    const auto column = ColumnIndex(spec, name);
    if (!key.empty()) key += '|';  // Content IDs and numeric domains omit '|'.
    key += spec.columns[column].type == ColumnType::kInteger
               ? std::to_string(row.values[column])
               : CanonicalCell(spec, spec.columns[column], row.cells[column],
                               aliases);
  }
  return key;
}

std::expected<std::map<std::string, std::size_t>, std::string> RowIndex(
    const SourceTable& table, const Aliases& aliases) {
  std::map<std::string, std::size_t> index;
  for (std::size_t i = 0; i < table.rows.size(); ++i) {
    const auto& row = table.rows[i];
    if (!row) continue;
    const auto key = KeyOf(table.source.spec, *row, aliases);
    if (!index.emplace(key, i).second) {
      return std::unexpected(std::format("{}:{}: duplicate resolved key {}",
                                         row->source, row->line, key));
    }
  }
  return index;
}

struct PatchHeader {
  std::vector<ColumnSpec> columns;
  std::vector<std::size_t> fields;
  std::optional<std::size_t> operation;
  std::size_t count = 0;
};

std::expected<PatchHeader, std::string> ReadPatchHeader(const TableSpec& spec,
                                                        const CsvRecord& header,
                                                        std::string_view file) {
  PatchHeader patch;
  patch.count = header.fields.size();
  std::set<std::string> seen;
  for (std::size_t i = 0; i < header.fields.size(); ++i) {
    const auto& name = header.fields[i];
    if (!seen.insert(name).second) {
      return std::unexpected(
          CellError(file, header.line, name, "duplicate column"));
    }
    if (name == "_op") {
      patch.operation = i;
    } else if (name != kDisplayName) {
      const auto column = ColumnIndex(spec, name);
      if (column == spec.columns.size()) {
        return std::unexpected(
            CellError(file, header.line, name, "unknown column"));
      }
      if (std::ranges::find(patch.columns, spec.columns[column].name,
                            &ColumnSpec::name) != patch.columns.end()) {
        return std::unexpected(
            CellError(file, header.line, name, "duplicate aliased column"));
      }
      patch.columns.push_back(spec.columns[column]);
      patch.fields.push_back(i);
    }
  }
  for (const auto key : spec.key) {
    if (std::ranges::find(patch.columns, key, &ColumnSpec::name) ==
        patch.columns.end()) {
      return std::unexpected(
          CellError(file, header.line, key, "missing key column"));
    }
  }
  return patch;
}

struct PatchRow {
  std::string operation;
  std::vector<ColumnSpec> columns;
  TableRow row;
};

std::expected<PatchRow, std::string> ReadPatchRow(const TableSpec& spec,
                                                  const PatchHeader& header,
                                                  const CsvRecord& record,
                                                  std::string_view file) {
  if (record.fields.size() != header.count) {
    return std::unexpected(std::format("{}:{}: row/header field count differs",
                                       file, record.line));
  }
  PatchRow patch;
  patch.operation =
      header.operation ? record.fields[*header.operation] : "merge";
  if (patch.operation.empty()) patch.operation = "merge";
  if (patch.operation != "merge" && patch.operation != "add" &&
      patch.operation != "replace" && patch.operation != "remove") {
    return std::unexpected(CellError(file, record.line, "_op",
                                     "expected add, merge, replace or remove"));
  }
  std::vector<std::string> cells;
  for (std::size_t i = 0; i < header.columns.size(); ++i) {
    const auto& column = header.columns[i];
    const auto& cell = record.fields[header.fields[i]];
    const bool key = std::ranges::find(spec.key, column.name) != spec.key.end();
    if (patch.operation == "remove" && !key) {
      if (!cell.empty()) {
        return std::unexpected(CellError(file, record.line, column.name,
                                         "remove accepts only key values"));
      }
      continue;
    }
    patch.columns.push_back(column);
    cells.push_back(cell);
  }
  TableSpec partial = spec;
  partial.columns = patch.columns;
  auto row = ReadTableRow(partial, cells, file, record.line);
  if (!row) return std::unexpected(row.error());
  patch.row = std::move(*row);
  return patch;
}

std::expected<TableRow, std::string> MergeRow(const TableSpec& spec,
                                              const PatchRow& patch,
                                              const TableRow* previous) {
  std::vector<std::string> cells(spec.columns.size());
  if (previous != nullptr && patch.operation == "merge") {
    cells = previous->cells;
  }
  for (std::size_t i = 0; i < patch.columns.size(); ++i) {
    cells[ColumnIndex(spec, patch.columns[i].name)] = patch.row.cells[i];
  }
  return ReadTableRow(spec, cells, patch.row.source, patch.row.line);
}

std::expected<void, std::string> ApplyPatchRow(
    SourceTable& table, const PatchRow& patch, const std::string& key,
    std::map<std::string, std::size_t>& index,
    std::vector<CatalogConflict>& conflicts) {
  const auto found = index.find(key);
  const bool exists = found != index.end();
  if ((patch.operation == "add" && exists) ||
      ((patch.operation == "replace" || patch.operation == "remove") &&
       !exists)) {
    return std::unexpected(
        CellError(patch.row.source, patch.row.line, "_op",
                  std::format("{} requires an {} key ({})", patch.operation,
                              exists ? "absent" : "existing", key)));
  }
  if (patch.operation == "remove") {
    table.rows[found->second].reset();
    index.erase(found);
  } else {
    const TableRow* previous = exists ? &*table.rows[found->second] : nullptr;
    auto merged = MergeRow(table.source.spec, patch, previous);
    if (!merged) return std::unexpected(merged.error());
    if (exists) {
      table.rows[found->second] = std::move(*merged);
    } else {
      index.emplace(key, table.rows.size());
      table.rows.emplace_back(std::move(*merged));
    }
  }
  const auto prior = table.writers.find(key);
  if (prior != table.writers.end()) {
    conflicts.push_back({std::string(table.source.spec.name), key,
                         prior->second, patch.row.source});
  }
  table.writers[key] = patch.row.source;
  return {};
}

std::expected<void, std::string> ApplyPatch(
    SourceTable& table, const fs::path& file, const Aliases& aliases,
    std::vector<CatalogConflict>& conflicts) {
  auto exists = Exists(file);
  if (!exists) return std::unexpected(exists.error());
  if (!*exists) return {};
  auto text = ReadText(file);
  if (!text) return std::unexpected(text.error());
  const auto name = Platform::PathText(file);
  auto records = ParseCsv(*text, name);
  if (!records) return std::unexpected(records.error());
  if (records->empty()) return std::unexpected(name + ": missing patch header");
  auto header = ReadPatchHeader(table.source.spec, records->front(), name);
  if (!header) return std::unexpected(header.error());
  auto index = RowIndex(table, aliases);
  if (!index) return std::unexpected(index.error());
  std::set<std::string> touched;
  for (std::size_t i = 1; i < records->size(); ++i) {
    auto patch = ReadPatchRow(table.source.spec, *header, (*records)[i], name);
    if (!patch) return std::unexpected(patch.error());
    TableSpec partial = table.source.spec;
    partial.columns = patch->columns;
    const auto key = KeyOf(partial, patch->row, aliases);
    if (!touched.insert(key).second) {
      return std::unexpected(std::format("{}:{}: duplicate patch key {}", name,
                                         patch->row.line, key));
    }
    // Resolve aliases in keys too, so a legacy spelling replaces, not renames,
    // a semantic definition or directed relationship.
    for (std::size_t c = 0; c < patch->columns.size(); ++c) {
      patch->row.cells[c] = CanonicalCell(partial, patch->columns[c],
                                          patch->row.cells[c], aliases);
    }
    if (auto applied = ApplyPatchRow(table, *patch, key, *index, conflicts);
        !applied) {
      return applied;
    }
  }
  return {};
}

std::expected<void, std::string> NormalizeReferences(
    std::vector<SourceTable>& tables, const Aliases& aliases) {
  for (auto& table : tables) {
    const auto& spec = table.source.spec;
    for (auto& entry : table.rows) {
      if (!entry) continue;
      TableRow& row = *entry;
      for (std::size_t c = 0; c < spec.columns.size(); ++c) {
        const auto& column = spec.columns[c];
        if (column.type != ColumnType::kReference) continue;
        const auto target = aliases.find(std::string(column.references));
        if (target == aliases.end() || !target->second.contains(row.cells[c])) {
          return std::unexpected(
              CellError(row.source, row.line, column.name,
                        std::format("'{}' names no row of {}", row.cells[c],
                                    column.references)));
        }
        row.cells[c] = target->second.at(row.cells[c]);
      }
    }
    if (auto index = RowIndex(table, aliases); !index) {
      return std::unexpected(index.error());
    }
  }
  return {};
}

std::expected<void, std::string> CheckLayerFiles(
    const fs::path& layer, std::span<const CatalogTableSource> sources) {
  const auto directory = layer / "catalogs";
  auto exists = Exists(directory);
  if (!exists) return std::unexpected(exists.error());
  if (!*exists) return {};
  std::error_code error;
  fs::recursive_directory_iterator entry(directory, error);
  const fs::recursive_directory_iterator end;
  while (!error && entry != end) {
    const auto& path = entry->path();
    if (entry->is_regular_file(error) && path.extension() == ".csv") {
      const auto relative = path.lexically_relative(layer);
      if (std::ranges::none_of(sources, [&](const auto& source) {
            return source.file == relative;
          })) {
        return std::unexpected("unsupported layered catalog table " +
                               Platform::PathText(path));
      }
    }
    if (!error) entry.increment(error);
  }
  if (error) {
    return std::unexpected(Platform::PathText(directory) + ": " +
                           error.message());
  }
  return {};
}

std::expected<std::vector<SourceTable>, std::string> ReadLayers(
    const fs::path& root, std::span<const CatalogTableSource> sources,
    std::vector<CatalogConflict>& conflicts) {
  auto layers = LayerRoots(root);
  if (!layers) return std::unexpected(layers.error());
  std::vector<SourceTable> tables;
  for (const auto& source : sources) {
    auto text = ReadText(root / source.file);
    if (!text) return std::unexpected(text.error());
    auto rows =
        ReadTable(source.spec, *text, Platform::PathText(root / source.file));
    if (!rows) return std::unexpected(rows.error());
    SourceTable table{.source = source, .rows = {}, .writers = {}};
    for (auto& row : *rows) table.rows.emplace_back(std::move(row));
    tables.push_back(std::move(table));
  }
  for (const auto& layer : *layers) {
    if (auto supported = CheckLayerFiles(layer, sources); !supported) {
      return std::unexpected(supported.error());
    }
    for (auto& table : tables) {
      auto aliases = BuildAliases(tables);
      if (!aliases) return std::unexpected(aliases.error());
      if (auto applied =
              ApplyPatch(table, layer / table.source.file, *aliases, conflicts);
          !applied) {
        return std::unexpected(applied.error());
      }
    }
  }
  auto aliases = BuildAliases(tables);
  if (!aliases) return std::unexpected(aliases.error());
  if (auto normalized = NormalizeReferences(tables, *aliases); !normalized) {
    return std::unexpected(normalized.error());
  }
  return tables;
}

std::string Quoted(std::string_view name) {
  std::string result = "\"";
  for (const char ch : name) {
    result += ch;
    if (ch == '"') result += ch;
  }
  return result + '"';
}

std::string Columns(const TableSpec& spec) {
  std::string columns;
  for (const auto& column : spec.columns) {
    if (!columns.empty()) columns += ',';
    columns += Quoted(column.name);
  }
  return columns;
}

std::expected<void, std::string> Execute(sqlite3* db, const std::string& sql) {
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    return std::unexpected(std::string("catalog SQLite: ") +
                           sqlite3_errmsg(db));
  }
  return {};
}

std::expected<Statement, std::string> Prepare(sqlite3* db,
                                              const std::string& sql) {
  sqlite3_stmt* raw = nullptr;
  const int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr);
  Statement statement(raw, sqlite3_finalize);
  if (result != SQLITE_OK) {
    return std::unexpected(std::string("catalog SQLite: ") +
                           sqlite3_errmsg(db));
  }
  return statement;
}

std::string CreateTableSql(const TableSpec& spec) {
  std::string sql = "CREATE TABLE " + Quoted(spec.name) + '(';
  for (const auto& column : spec.columns) {
    sql += Quoted(column.name);
    sql += column.type == ColumnType::kInteger ? " INTEGER NOT NULL,"
                                               : " TEXT NOT NULL,";
  }
  sql += "\"__source\" TEXT NOT NULL,\"__line\" INTEGER NOT NULL,PRIMARY KEY(";
  for (std::size_t i = 0; i < spec.key.size(); ++i) {
    if (i != 0) sql += ',';
    sql += Quoted(spec.key[i]);
  }
  sql += ')';
  for (const auto& column : spec.columns) {
    if (column.type == ColumnType::kReference) {
      sql += ",FOREIGN KEY(" + Quoted(column.name) + ") REFERENCES " +
             Quoted(column.references) +
             "(\"id\") DEFERRABLE INITIALLY DEFERRED";
    }
  }
  return sql + ')';
}

bool BindText(sqlite3_stmt* statement, int index, const std::string& value) {
  return sqlite3_bind_text64(statement, index, value.data(), value.size(),
                             SQLITE_TRANSIENT, SQLITE_UTF8) == SQLITE_OK;
}

std::expected<void, std::string> InsertTable(sqlite3* db,
                                             const SourceTable& table) {
  const auto& spec = table.source.spec;
  std::string sql = "INSERT INTO " + Quoted(spec.name) + " VALUES(";
  for (std::size_t i = 0; i < spec.columns.size() + 2; ++i) {
    if (i != 0) sql += ',';
    sql += '?';
  }
  auto statement = Prepare(db, sql + ')');
  if (!statement) return std::unexpected(statement.error());
  for (const auto& entry : table.rows) {
    if (!entry) continue;
    const TableRow& row = *entry;
    int parameter = 1;
    bool bound = true;
    for (std::size_t c = 0; c < spec.columns.size(); ++c, ++parameter) {
      bound =
          bound && (spec.columns[c].type == ColumnType::kInteger
                        ? sqlite3_bind_int64(statement->get(), parameter,
                                             row.values[c]) == SQLITE_OK
                        : BindText(statement->get(), parameter, row.cells[c]));
    }
    bound = bound && BindText(statement->get(), parameter++, row.source);
    bound = bound && sqlite3_bind_int64(statement->get(), parameter,
                                        static_cast<sqlite3_int64>(row.line)) ==
                         SQLITE_OK;
    if (!bound || sqlite3_step(statement->get()) != SQLITE_DONE ||
        sqlite3_reset(statement->get()) != SQLITE_OK) {
      return std::unexpected(std::format("{}:{}: catalog SQLite: {}",
                                         row.source, row.line,
                                         sqlite3_errmsg(db)));
    }
  }
  return {};
}

std::expected<std::string, std::string> TextAt(sqlite3_stmt* statement,
                                               int column) {
  const auto* text = sqlite3_column_text(statement, column);
  const auto bytes = sqlite3_column_bytes(statement, column);
  if (text == nullptr) {
    return std::unexpected(std::string("cannot read catalog cell: ") +
                           sqlite3_errmsg(sqlite3_db_handle(statement)));
  }
  return std::string(reinterpret_cast<const char*>(text),
                     static_cast<std::size_t>(bytes));
}
}  // namespace

struct CatalogDatabase::State {
  SqlConnection db{nullptr, sqlite3_close_v2};
  std::vector<CatalogConflict> conflicts;
};

CatalogDatabase::CatalogDatabase(std::unique_ptr<State> state)
    : state_(std::move(state)) {}
CatalogDatabase::~CatalogDatabase() = default;
CatalogDatabase::CatalogDatabase(CatalogDatabase&&) noexcept = default;
CatalogDatabase& CatalogDatabase::operator=(CatalogDatabase&&) noexcept =
    // NOLINTNEXTLINE(whitespace/indent_namespace): wrapped defaulted operator.
    default;

std::expected<CatalogDatabase, std::string> CatalogDatabase::Load(
    const fs::path& root, std::span<const CatalogTableSource> sources) {
  auto state = std::make_unique<State>();
  auto tables = ReadLayers(root, sources, state->conflicts);
  if (!tables) return std::unexpected(tables.error());
  sqlite3* raw = nullptr;
  const int opened = sqlite3_open(":memory:", &raw);
  state->db.reset(raw);
  if (opened != SQLITE_OK) {
    return std::unexpected(std::string("cannot open catalog SQLite: ") +
                           sqlite3_errstr(opened));
  }
  if (auto begin = Execute(raw, "PRAGMA foreign_keys=ON; BEGIN"); !begin) {
    return std::unexpected(begin.error());
  }
  for (const auto& table : *tables) {
    if (auto created = Execute(raw, CreateTableSql(table.source.spec));
        !created) {
      return std::unexpected(created.error());
    }
  }
  for (const auto& table : *tables) {
    if (auto inserted = InsertTable(raw, table); !inserted) {
      return std::unexpected(inserted.error());
    }
  }
  if (auto frozen = Execute(raw, "COMMIT; PRAGMA query_only=ON"); !frozen) {
    return std::unexpected(frozen.error());
  }
  return CatalogDatabase(std::move(state));
}

std::expected<std::vector<TableRow>, std::string> CatalogDatabase::Read(
    const TableSpec& spec) const {
  if (!state_) return std::unexpected("catalog database has been moved");
  auto statement =
      Prepare(state_->db.get(), "SELECT " + Columns(spec) +
                                    ",\"__source\",\"__line\" FROM " +
                                    Quoted(spec.name) + " ORDER BY rowid");
  if (!statement) return std::unexpected(statement.error());
  std::vector<TableRow> rows;
  int result = SQLITE_OK;
  while ((result = sqlite3_step(statement->get())) == SQLITE_ROW) {
    std::vector<std::string> cells;
    for (std::size_t c = 0; c < spec.columns.size(); ++c) {
      auto cell = TextAt(statement->get(), static_cast<int>(c));
      if (!cell) return std::unexpected(cell.error());
      cells.push_back(std::move(*cell));
    }
    const auto metadata = static_cast<int>(spec.columns.size());
    const auto source = TextAt(statement->get(), metadata);
    if (!source) return std::unexpected(source.error());
    const auto line = sqlite3_column_int64(statement->get(), metadata + 1);
    auto row =
        ReadTableRow(spec, cells, *source, static_cast<std::size_t>(line));
    if (!row) return std::unexpected(row.error());
    rows.push_back(std::move(*row));
  }
  if (result != SQLITE_DONE) {
    return std::unexpected(std::string("cannot read catalog SQLite: ") +
                           sqlite3_errmsg(state_->db.get()));
  }
  return rows;
}

std::span<const CatalogConflict> CatalogDatabase::Conflicts() const {
  return state_ ? std::span<const CatalogConflict>(state_->conflicts)
                : std::span<const CatalogConflict>{};
}
}  // namespace VS::Content
