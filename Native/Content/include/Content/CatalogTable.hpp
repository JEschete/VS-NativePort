#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// CSV gameplay tables (ASSET_PIPELINE.md §1.1, §4): one table per file, each
// described by a <table>.schema.json that lists its columns' types, ranges and
// references. Reading is strict: the header names every column exactly once
// (in any order), with an optional informational display_name column. Each
// gameplay cell holds its column's type within its range, and the
// key columns are unique together. Errors give the file, line and column, so
// a typo is reported instead of being read as zero.
namespace VS::Content {

inline constexpr std::string_view kCsvSuffix = ".csv";
inline constexpr std::string_view kTableSchemaSuffix = ".schema.json";
inline constexpr std::uint32_t kTableSchemaVersion = 1;

enum class ColumnType : std::uint8_t {
  kId,         // a content id (IsContentId)
  kInteger,    // a decimal integer within [minimum, maximum]
  kReference,  // the id of a row in the table `references` names
  kBytes,      // `maximum` bytes as lowercase hexadecimal, two digits each
};

struct ColumnSpec {
  std::string_view name;
  ColumnType type = ColumnType::kInteger;
  std::int64_t minimum = 0;
  std::int64_t maximum = 0;     // kBytes: the byte count
  std::string_view references;  // kReference only
  std::string_view description;
};

struct TableSpec {
  std::string_view name;  // the file stem, e.g. combination-blade-results
  std::string_view description;
  std::span<const ColumnSpec> columns;
  std::span<const std::string_view> key;  // unique together
};

// One CSV record: its first line in the file and its fields.
struct CsvRecord {
  std::size_t line = 0;
  std::vector<std::string> fields;
};

// RFC 4180 records: comma-separated, '"'-quoted fields with "" for a quote,
// CRLF or LF line ends. A leading UTF-8 byte order mark and blank lines are
// ignored. Errors name `file` and the line.
[[nodiscard]] std::expected<std::vector<CsvRecord>, std::string> ParseCsv(
    std::string_view text, std::string_view file);
// The records as CSV with LF line ends, quoting only fields that need it.
[[nodiscard]] std::string FormatCsv(
    std::span<const std::vector<std::string>> records);

// One data row in the spec's column order. Integer columns' values are in
// `values`; other columns hold 0 there. Optional display_name metadata is not
// part of these cells, so it cannot change a consumer's column positions.
struct TableRow {
  std::size_t line = 0;
  std::vector<std::string> cells;
  std::vector<std::int64_t> values;
};

// "<file>:<line>: column '<column>': <problem>".
[[nodiscard]] std::string CellError(std::string_view file, std::size_t line,
                                    std::string_view column,
                                    std::string_view problem);

// The data rows of `text` checked against `spec`. References are checked
// for their id syntax only; the caller resolves them.
[[nodiscard]] std::expected<std::vector<TableRow>, std::string> ReadTable(
    const TableSpec& spec, std::string_view text, std::string_view file);
// `rows` (cells in the spec's column order) under the spec's header.
[[nodiscard]] std::string FormatTable(
    const TableSpec& spec, std::span<const std::vector<std::string>> rows);

// The <table>.schema.json text, ending in a newline.
[[nodiscard]] std::string FormatTableSchema(const TableSpec& spec);

}  // namespace VS::Content
