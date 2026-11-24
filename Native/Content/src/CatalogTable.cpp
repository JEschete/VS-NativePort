// CSV gameplay tables (CatalogTable.hpp).
#include "Content/CatalogTable.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"

namespace VS::Content {
namespace {

constexpr std::string_view kByteOrderMark = "\xEF\xBB\xBF";
constexpr std::size_t kNotFound = std::string_view::npos;
// Separates key cells; ids and integers never hold it.
constexpr char kKeySeparator = '\x1f';

// Where ParseCsv is in the text.
struct CsvCursor {
  std::string_view text;
  std::string_view file;
  std::size_t at = 0;
  std::size_t line = 1;

  [[nodiscard]] bool AtEnd() const { return at >= text.size(); }
  [[nodiscard]] bool At(char value) const {
    return !AtEnd() && text[at] == value;
  }
  [[nodiscard]] bool AtCrLf() const {
    return At('\r') && at + 1 < text.size() && text[at + 1] == '\n';
  }
  // Consumes a line end; true also at the end of the text.
  bool EndLine() {
    if (AtEnd()) return true;
    if (At('\n')) {
      ++at;
    } else if (AtCrLf()) {
      at += 2;
    } else {
      return false;
    }
    ++line;
    return true;
  }
  [[nodiscard]] std::string Error(std::size_t where,
                                  std::string_view problem) const {
    return std::format("{}:{}: {}", file, where, problem);
  }
};

std::expected<std::string, std::string> ParseQuoted(CsvCursor& cursor) {
  const std::size_t start = cursor.line;
  ++cursor.at;
  std::string field;
  while (!cursor.AtEnd()) {
    const char value = cursor.text[cursor.at++];
    if (value == '"') {
      if (!cursor.At('"')) return field;
      ++cursor.at;
    } else if (value == '\n') {
      ++cursor.line;
    }
    field += value;
  }
  return std::unexpected(cursor.Error(start, "a quoted field is not closed"));
}

std::expected<std::string, std::string> ParseUnquoted(CsvCursor& cursor) {
  std::size_t end = cursor.text.find_first_of(",\r\n\"", cursor.at);
  if (end == kNotFound) end = cursor.text.size();
  std::string field(cursor.text.substr(cursor.at, end - cursor.at));
  cursor.at = end;
  if (cursor.At('"')) {
    return std::unexpected(
        cursor.Error(cursor.line, "a quote inside an unquoted field"));
  }
  return field;
}

std::expected<CsvRecord, std::string> ParseRecord(CsvCursor& cursor) {
  CsvRecord record{.line = cursor.line, .fields = {}};
  while (true) {
    auto field = cursor.At('"') ? ParseQuoted(cursor) : ParseUnquoted(cursor);
    if (!field) return std::unexpected(field.error());
    record.fields.push_back(std::move(*field));
    if (cursor.At(',')) {
      ++cursor.at;
      continue;
    }
    if (cursor.EndLine()) return record;
    return std::unexpected(cursor.Error(
        cursor.line, "a field must end at a comma or at the line's end"));
  }
}

bool NeedsQuotes(std::string_view field) {
  return field.find_first_of(",\"\r\n") != kNotFound;
}

std::string_view TypeName(ColumnType type) {
  switch (type) {
    case ColumnType::kId:
      return "id";
    case ColumnType::kInteger:
      return "integer";
    case ColumnType::kReference:
      return "reference";
    case ColumnType::kBytes:
      return "bytes";
  }
  return "integer";
}

bool IsLowerHexDigit(char value) {
  return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
}

// Each spec column's field position in the header.
std::expected<std::vector<std::size_t>, std::string> HeaderOrder(
    const TableSpec& spec, const CsvRecord& header, std::string_view file) {
  std::vector<std::size_t> position(spec.columns.size(), kNotFound);
  bool has_label = false;
  for (std::size_t field = 0; field < header.fields.size(); ++field) {
    const std::string_view name = header.fields[field];
    if (name == kDisplayName) {
      if (has_label) {
        return std::unexpected(std::format("{}:{}: column '{}' appears twice",
                                           file, header.line, name));
      }
      has_label = true;
      continue;
    }
    const auto found = std::ranges::find(spec.columns, name, &ColumnSpec::name);
    if (found == spec.columns.end()) {
      return std::unexpected(
          std::format("{}:{}: unknown column '{}'", file, header.line, name));
    }
    const auto column = static_cast<std::size_t>(found - spec.columns.begin());
    if (position[column] != kNotFound) {
      return std::unexpected(std::format("{}:{}: column '{}' appears twice",
                                         file, header.line, name));
    }
    position[column] = field;
  }
  for (std::size_t column = 0; column < spec.columns.size(); ++column) {
    if (position[column] == kNotFound) {
      return std::unexpected(std::format("{}:{}: missing column '{}'", file,
                                         header.line,
                                         spec.columns[column].name));
    }
  }
  return position;
}

std::expected<std::int64_t, std::string> ReadCell(const ColumnSpec& column,
                                                  std::string_view cell,
                                                  std::string_view file,
                                                  std::size_t line) {
  if (column.type == ColumnType::kBytes) {
    const auto digits = static_cast<std::size_t>(2 * column.maximum);
    if (cell.size() == digits && std::ranges::all_of(cell, IsLowerHexDigit)) {
      return 0;
    }
    return std::unexpected(
        CellError(file, line, column.name,
                  std::format("'{}' is not {} bytes as lowercase hexadecimal",
                              cell, column.maximum)));
  }
  if (column.type != ColumnType::kInteger) {
    if (IsContentId(cell)) return 0;
    return std::unexpected(CellError(
        file, line, column.name,
        std::format("'{}' is not an id (lowercase letters, digits, '.', '_' "
                    "or '-')",
                    cell)));
  }
  std::int64_t value = 0;
  const auto parsed =
      std::from_chars(cell.data(), cell.data() + cell.size(), value);
  if (cell.empty() || parsed.ec != std::errc() ||
      parsed.ptr != cell.data() + cell.size()) {
    return std::unexpected(
        CellError(file, line, column.name,
                  std::format("'{}' is not a decimal integer", cell)));
  }
  if (value < column.minimum || value > column.maximum) {
    return std::unexpected(
        CellError(file, line, column.name,
                  std::format("{} is outside {}..{}", value, column.minimum,
                              column.maximum)));
  }
  return value;
}

std::expected<TableRow, std::string> ReadRow(
    const TableSpec& spec, const std::vector<std::size_t>& position,
    const CsvRecord& record, std::size_t header_fields, std::string_view file) {
  if (record.fields.size() != header_fields) {
    return std::unexpected(
        std::format("{}:{}: has {} fields; the header has {}", file,
                    record.line, record.fields.size(), header_fields));
  }
  TableRow row{.line = record.line, .cells = {}, .values = {}};
  for (std::size_t column = 0; column < spec.columns.size(); ++column) {
    const std::string& cell = record.fields[position[column]];
    auto value = ReadCell(spec.columns[column], cell, file, record.line);
    if (!value) return std::unexpected(value.error());
    row.cells.push_back(cell);
    row.values.push_back(*value);
  }
  return row;
}

std::expected<std::vector<std::size_t>, std::string> KeyColumns(
    const TableSpec& spec) {
  std::vector<std::size_t> columns;
  for (const std::string_view name : spec.key) {
    const auto found = std::ranges::find(spec.columns, name, &ColumnSpec::name);
    if (found == spec.columns.end()) {
      return std::unexpected(
          std::format("table {}: key column '{}' is not "
                      "one of its columns",
                      spec.name, name));
    }
    columns.push_back(static_cast<std::size_t>(found - spec.columns.begin()));
  }
  return columns;
}

// The key's cells, and how an error names them.
struct RowKey {
  std::string cells;
  std::string text;
};

RowKey KeyOf(const TableSpec& spec, const std::vector<std::size_t>& columns,
             const TableRow& row) {
  RowKey key;
  for (const std::size_t column : columns) {
    if (!key.text.empty()) {
      key.cells += kKeySeparator;
      key.text += ", ";
    }
    key.cells += row.cells[column];
    key.text +=
        std::format("{}={}", spec.columns[column].name, row.cells[column]);
  }
  return key;
}

}  // namespace

std::expected<std::vector<CsvRecord>, std::string> ParseCsv(
    std::string_view text, std::string_view file) {
  if (text.starts_with(kByteOrderMark)) {
    text.remove_prefix(kByteOrderMark.size());
  }
  CsvCursor cursor{.text = text, .file = file};
  std::vector<CsvRecord> records;
  while (!cursor.AtEnd()) {
    if (cursor.At('\n') || cursor.AtCrLf()) {
      cursor.EndLine();
      continue;
    }
    auto record = ParseRecord(cursor);
    if (!record) return std::unexpected(record.error());
    records.push_back(std::move(*record));
  }
  return records;
}

std::string FormatCsv(std::span<const std::vector<std::string>> records) {
  std::string text;
  for (const std::vector<std::string>& record : records) {
    for (std::size_t index = 0; index < record.size(); ++index) {
      if (index != 0) text += ',';
      const std::string& field = record[index];
      if (!NeedsQuotes(field)) {
        text += field;
        continue;
      }
      text += '"';
      for (const char value : field) {
        if (value == '"') text += '"';
        text += value;
      }
      text += '"';
    }
    text += '\n';
  }
  return text;
}

std::string CellError(std::string_view file, std::size_t line,
                      std::string_view column, std::string_view problem) {
  return std::format("{}:{}: column '{}': {}", file, line, column, problem);
}

std::expected<std::vector<TableRow>, std::string> ReadTable(
    const TableSpec& spec, std::string_view text, std::string_view file) {
  auto records = ParseCsv(text, file);
  if (!records) return std::unexpected(records.error());
  if (records->empty()) {
    return std::unexpected(std::format("{}: has no header line", file));
  }
  auto position = HeaderOrder(spec, records->front(), file);
  if (!position) return std::unexpected(position.error());
  auto key_columns = KeyColumns(spec);
  if (!key_columns) return std::unexpected(key_columns.error());
  std::vector<TableRow> rows;
  std::unordered_map<std::string, std::size_t> keys;
  for (std::size_t index = 1; index < records->size(); ++index) {
    auto row = ReadRow(spec, *position, (*records)[index],
                       records->front().fields.size(), file);
    if (!row) return std::unexpected(row.error());
    RowKey key = KeyOf(spec, *key_columns, *row);
    const auto [found, added] = keys.emplace(std::move(key.cells), row->line);
    if (!added) {
      return std::unexpected(
          std::format("{}:{}: repeats the key ({}) of line {}", file, row->line,
                      key.text, found->second));
    }
    rows.push_back(std::move(*row));
  }
  return rows;
}

std::string FormatTable(const TableSpec& spec,
                        std::span<const std::vector<std::string>> rows) {
  std::vector<std::vector<std::string>> records;
  records.reserve(rows.size() + 1);
  std::vector<std::string> header;
  for (const ColumnSpec& column : spec.columns) {
    header.emplace_back(column.name);
  }
  records.push_back(std::move(header));
  records.insert(records.end(), rows.begin(), rows.end());
  return FormatCsv(records);
}

std::string FormatTableSchema(const TableSpec& spec) {
  JsonDocument columns = JsonDocument::array();
  for (const ColumnSpec& column : spec.columns) {
    JsonDocument entry = JsonDocument::object();
    entry["name"] = std::string(column.name);
    entry["type"] = std::string(TypeName(column.type));
    if (column.type == ColumnType::kInteger) {
      entry["minimum"] = column.minimum;
      entry["maximum"] = column.maximum;
    }
    if (column.type == ColumnType::kReference) {
      entry["references"] = std::string(column.references);
    }
    if (column.type == ColumnType::kBytes) {
      entry["bytes"] = column.maximum;
    }
    entry["description"] = std::string(column.description);
    columns.push_back(std::move(entry));
    if (columns.size() == 1) {
      columns.push_back(
          {{"name", kDisplayName},
           {"type", "text"},
           {"optional", true},
           {"description",
            "Informational plain-text label; not a key or gameplay value."}});
    }
  }
  JsonDocument key = JsonDocument::array();
  for (const std::string_view name : spec.key) key.push_back(std::string(name));
  JsonDocument schema = JsonDocument::object();
  schema["schema_version"] = kTableSchemaVersion;
  schema["table"] = std::string(spec.name);
  schema["description"] = std::string(spec.description);
  schema["key"] = std::move(key);
  if (!spec.legacy_alias_prefix.empty()) {
    schema["legacy_alias"] = {
        {"column", "legacy_id"},
        {"pattern", std::string(spec.legacy_alias_prefix) + "-{legacy_id:03}"},
        {"role", "compatibility only; write current semantic ids"}};
  }
  schema["columns"] = std::move(columns);
  return FormatJsonText(schema) + "\n";
}

}  // namespace VS::Content
