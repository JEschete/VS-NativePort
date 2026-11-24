#include "Extract/AssetLabels.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/ActorArchetype.hpp"
#include "Content/CatalogTable.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Content/ZoneTables.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Platform/Paths.hpp"
#include "Render/EncodedText.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Json = Content::JsonDocument;
using Check = std::expected<void, std::string>;
using Names = std::set<std::string, std::less<>>;
using NameMap = std::map<std::string, Names, std::less<>>;
constexpr std::size_t kNameBytes = 24;
constexpr std::string_view kItemNames = "MENU/ITEMNAME.BIN";
constexpr std::string_view kEnemies = "zone-enemies";
constexpr std::string_view kParts = "zone-enemy-parts";
constexpr unsigned kHexAlphabetOffset = 10;
constexpr unsigned kHexBits = 4;

void AppendUtf8(std::string& text, std::uint32_t code) {
  constexpr std::uint32_t kAsciiLimit = 0x80;
  constexpr std::uint32_t kTwoByteLimit = 0x800;
  constexpr std::uint32_t kTwoBytePrefix = 0xC0;
  constexpr std::uint32_t kThreeBytePrefix = 0xE0;
  constexpr std::uint32_t kContinuationPrefix = 0x80;
  constexpr std::uint32_t kPayloadMask = 0x3F;
  constexpr unsigned kPayloadBits = 6;
  if (code < kAsciiLimit) {
    text += static_cast<char>(code);
  } else if (code < kTwoByteLimit) {
    text += static_cast<char>(kTwoBytePrefix | (code >> kPayloadBits));
    text += static_cast<char>(kContinuationPrefix | (code & kPayloadMask));
  } else {
    text += static_cast<char>(kThreeBytePrefix | (code >> (2 * kPayloadBits)));
    text += static_cast<char>(kContinuationPrefix |
                              ((code >> kPayloadBits) & kPayloadMask));
    text += static_cast<char>(kContinuationPrefix | (code & kPayloadMask));
  }
}

// English glyph indices: upstream tools/etc/vsString.py:6-40.
void AppendGlyph(std::string& text, std::uint8_t glyph) {
  constexpr std::uint8_t kFirstSign = 0x86;
  constexpr std::uint8_t kFirstPunctuation = 0x8F;
  constexpr std::uint8_t kFirstSymbol = 0xAD;
  constexpr std::string_view kAlphabet =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  constexpr std::array<std::uint32_t, 45> kAccents{
      0x0152, 0x00C0, 0x00C1, 0x00C2, 0x00C4, 0x00C7, 0x00C8, 0x00C9, 0x00CA,
      0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF, 0x00D2, 0x00D3, 0x00D4, 0x00D6,
      0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DF, 0x0153, 0x00E0, 0x00E1, 0x00E2,
      0x00E4, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE,
      0x00EF, 0x00F2, 0x00F3, 0x00F4, 0x00F6, 0x00F9, 0x00FA, 0x00FB, 0x00FC};
  constexpr std::array<std::uint32_t, 9> kSigns{
      0x201E, 0x203C, 0x2260, 0x2266, 0x2267, 0x00F7, 0x00B7, 0x2014, 0x22EF};
  constexpr std::string_view kPunctuation = " !\"#$%&'()=@[];:,./\\<>?_-+*`{}";
  constexpr std::array<std::string_view, 13> kSymbols{
      "[music]", "[triangle]", "[square]", "[circle]", "[cross]",
      "[left]",  "[right]",    "[up]",     "[down]",   "Lv.",
      "[star]",  "[block]",    "~"};
  if (glyph < kAlphabet.size()) {
    text += kAlphabet[glyph];
  } else if (glyph < kAlphabet.size() + kAccents.size()) {
    AppendUtf8(text, kAccents[glyph - kAlphabet.size()]);
  } else if (glyph >= kFirstSign && glyph < kFirstSign + kSigns.size()) {
    AppendUtf8(text, kSigns[glyph - kFirstSign]);
  } else if (glyph >= kFirstPunctuation &&
             glyph < kFirstPunctuation + kPunctuation.size()) {
    text += kPunctuation[glyph - kFirstPunctuation];
  } else if (glyph >= kFirstSymbol && glyph < kFirstSymbol + kSymbols.size()) {
    text += kSymbols[glyph - kFirstSymbol];
  } else {
    text += std::format("[glyph {:02X}]", glyph);
  }
}

std::string Join(const Names& names) {
  std::string joined;
  for (const std::string& name : names) {
    if (!joined.empty()) joined += " / ";
    joined += name;
  }
  return joined;
}

std::string TextOf(std::span<const std::byte> bytes) {
  if (bytes.empty()) return {};
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::expected<bool, std::string> Present(const fs::path& path) {
  std::error_code error;
  const bool present = fs::is_regular_file(path, error);
  if (error && error != std::errc::no_such_file_or_directory) {
    return std::unexpected(
        std::format("{}: {}", Platform::PathText(path), error.message()));
  }
  return present;
}

std::expected<Json, std::string> ReadJson(const fs::path& root,
                                          std::string_view file) {
  auto bytes = ReadFileBytes(root / Platform::Utf8Path(file));
  if (!bytes) return std::unexpected(bytes.error());
  return Content::ParseJsonText(*bytes, file);
}

struct Table {
  std::string path;
  const Content::TableSpec* spec = nullptr;
  std::vector<Content::CsvRecord> records;
  std::vector<Content::TableRow> rows;

  std::size_t Column(std::string_view name) const {
    const auto column =
        std::ranges::find(spec->columns, name, &Content::ColumnSpec::name);
    if (column == spec->columns.end()) {
      throw std::logic_error(std::format("{}: no column {}", path, name));
    }
    return static_cast<std::size_t>(column - spec->columns.begin());
  }
  const std::string& Cell(std::size_t row, std::string_view name) const {
    return rows.at(row).cells.at(Column(name));
  }
  std::int64_t Number(std::size_t row, std::string_view name) const {
    return rows.at(row).values.at(Column(name));
  }
};

std::expected<std::vector<Table>, std::string> LoadTables(
    const fs::path& root) {
  std::map<std::string, const Content::TableSpec*> specs;
  for (const auto table : Content::kZoneTables) {
    specs.emplace(Content::ZoneTablePath(table),
                  &Content::ZoneTableSpec(table));
  }
  for (const auto kind : Content::kWorkshopKinds) {
    for (const auto part : Content::kWorkshopTableParts) {
      specs.emplace(Content::WorkshopTablePath(kind, part),
                    &Content::WorkshopTableSpec(kind, part));
    }
  }
  std::vector<Table> tables;
  for (const auto& [path, spec] : specs) {
    auto present = Present(root / Platform::Utf8Path(path));
    if (!present) return std::unexpected(present.error());
    if (!*present) continue;
    auto bytes = ReadFileBytes(root / Platform::Utf8Path(path));
    if (!bytes) return std::unexpected(bytes.error());
    const std::string text = TextOf(*bytes);
    auto records = Content::ParseCsv(text, path);
    if (!records) return std::unexpected(records.error());
    auto rows = Content::ReadTable(*spec, text, path);
    if (!rows) return std::unexpected(rows.error());
    tables.push_back({.path = path,
                      .spec = spec,
                      .records = std::move(*records),
                      .rows = std::move(*rows)});
  }
  return tables;
}

unsigned HexDigit(char value) {
  return value <= '9' ? static_cast<unsigned>(value - '0')
                      : static_cast<unsigned>(value - 'a') + kHexAlphabetOffset;
}

std::expected<std::string, std::string> NameCell(const Table& table,
                                                 std::size_t row,
                                                 std::string_view column) {
  // ReadTable already checked the byte column's length and hex digits.
  const std::string& cell = table.Cell(row, column);
  std::vector<std::byte> bytes;
  for (std::size_t at = 0; at < cell.size(); at += 2) {
    bytes.push_back(static_cast<std::byte>((HexDigit(cell.at(at)) << kHexBits) |
                                           HexDigit(cell.at(at + 1))));
  }
  auto name = DecodeAssetName(bytes);
  if (!name) {
    return std::unexpected(Content::CellError(
        table.path, table.rows.at(row).line, column, name.error()));
  }
  return name;
}

struct LabelIndex {
  std::vector<std::string> items;
  std::map<std::string, std::string, std::less<>> enemies;
  std::map<std::string, std::string, std::less<>> enemy_archetypes;
  std::map<std::string, std::string, std::less<>> results;
  NameMap ids;
  NameMap weapons;
  NameMap shields;
  NameMap files;
  NameMap sources;

  std::string Item(std::int64_t id) const {
    if (id >= 0 && static_cast<std::uint64_t>(id) < items.size()) {
      const std::string& name = items.at(static_cast<std::size_t>(id));
      if (!name.empty()) return name;
    }
    return std::format("Item {}", id);
  }
  std::string Enemy(std::string_view id) const {
    const auto found = enemies.find(id);
    return found == enemies.end() ? std::string(id) : found->second;
  }
  std::string Result(std::string_view id) const {
    const auto found = results.find(id);
    return found == results.end() ? std::string(id) : found->second;
  }
};

Check LoadItemNames(const fs::path& root, const RunStart& run,
                    LabelIndex& names) {
  if (std::ranges::find(run.entries, kItemNames,
                        &Content::DataStore::Entry::path) ==
      run.entries.end()) {
    return {};
  }
  auto loaded = ReadItemDisplayNames(root);
  if (!loaded) return std::unexpected(loaded.error());
  names.items = std::move(*loaded);
  return {};
}

Check IndexEnemies(std::span<const Table> tables, LabelIndex& names) {
  for (const Table& table : tables) {
    if (table.spec->name != kEnemies) continue;
    for (std::size_t row = 0; row < table.rows.size(); ++row) {
      auto name = NameCell(table, row, "name");
      if (!name) return std::unexpected(name.error());
      auto weapon = NameCell(table, row, "weapon_name");
      if (!weapon) return std::unexpected(weapon.error());
      const std::string& id = table.Cell(row, "id");
      const std::string& archetype = table.Cell(row, "archetype");
      names.enemies[id] = name->empty() ? id : *name;
      names.enemy_archetypes[id] = archetype;
      if (!name->empty()) names.ids[archetype].insert(*name);
      if (!weapon->empty()) names.weapons[archetype].insert(*weapon);
    }
  }
  return {};
}

void IndexItems(std::span<const Table> tables, LabelIndex& names) {
  constexpr std::int64_t kEquipmentItemBase = 126;
  for (const Table& table : tables) {
    if (table.spec->name.ends_with("-results")) {
      const bool blade = table.spec->name == "combination-blade-results";
      for (std::size_t row = 0; row < table.rows.size(); ++row) {
        const auto legacy = table.Number(row, "legacy_id");
        names.results[table.Cell(row, "id")] =
            legacy == 0 ? "Empty result"
                        : names.Item(legacy + (blade ? 0 : kEquipmentItemBase));
      }
    } else if (table.spec->name == kParts) {
      for (std::size_t row = 0; row < table.rows.size(); ++row) {
        const auto owner =
            names.enemy_archetypes.find(table.Cell(row, "enemy"));
        if (owner == names.enemy_archetypes.end()) continue;
        const std::string& part = table.Cell(row, "part");
        if (part == "shield.base") {
          names.shields[owner->second].insert(
              names.Item(table.Number(row, "item_id")));
        } else if (part == "weapon.blade") {
          names.weapons[owner->second].insert(
              names.Item(table.Number(row, "item_id")));
        }
      }
    }
  }
}

std::string StringMember(const Json& object, std::string_view key) {
  if (!object.is_object()) return {};
  const auto found = object.find(std::string(key));
  return found != object.end() && found->is_string() ? found->get<std::string>()
                                                     : std::string{};
}

void BindNames(const Json& document, std::string_view member,
               const Names& labels, std::string_view file, LabelIndex& names) {
  const auto part = document.find(std::string(member));
  if (part == document.end() || !part->is_object()) return;
  const std::string model = StringMember(*part, "model");
  if (!model.empty()) {
    names.ids[model].insert(labels.begin(), labels.end());
  }
  const std::string own = StringMember(*part, "file");
  if (!own.empty()) {
    const std::string relative =
        std::string(file.substr(0, file.rfind('/') + 1)) + own;
    names.files[relative].insert(labels.begin(), labels.end());
  }
}

Check IndexArchetypes(const fs::path& root,
                      const Content::ConversionManifest& manifest,
                      LabelIndex& names) {
  for (const auto& output : manifest.outputs) {
    if (output.converter != Content::kZudArchetypeConverterName) continue;
    for (const std::string& file : output.files) {
      if (!file.ends_with(".json") || file.ends_with(".image.json")) continue;
      auto json = ReadJson(root, file);
      if (!json) return std::unexpected(json.error());
      if (StringMember(*json, "schema") != Content::kActorArchetypeSchema) {
        continue;
      }
      BindNames(*json, "body", names.ids[output.id], file, names);
      BindNames(*json, "weapon", names.weapons[output.id], file, names);
      BindNames(*json, "shield", names.shields[output.id], file, names);
    }
  }
  for (const auto& output : manifest.outputs) {
    const Names& labels = names.ids[output.id];
    names.sources[output.source].insert(labels.begin(), labels.end());
    for (const std::string& file : output.files) {
      names.files[file].insert(labels.begin(), labels.end());
    }
  }
  return {};
}

std::string Material(std::int64_t id) {
  constexpr std::array<std::string_view, 8> kMaterials{
      "None", "Wood",   "Leather", "Bronze",
      "Iron", "Hagane", "Silver",  "Damascus"};
  if (id >= 0 && static_cast<std::uint64_t>(id) < kMaterials.size()) {
    return std::string(kMaterials.at(static_cast<std::size_t>(id)));
  }
  return std::format("Material {}", id);
}

std::string RowLabel(const Table& table, std::size_t row,
                     const LabelIndex& names) {
  const std::string_view kind = table.spec->name;
  if (kind == kEnemies) return names.Enemy(table.Cell(row, "id"));
  if (kind == "zone-enemy-limbs") {
    return std::format("{} - limb {}", names.Enemy(table.Cell(row, "enemy")),
                       table.Number(row, "limb"));
  }
  if (kind == kParts) {
    return std::format("{} - {}: {}", names.Enemy(table.Cell(row, "enemy")),
                       table.Cell(row, "part"),
                       names.Item(table.Number(row, "item_id")));
  }
  if (kind == "zones") return "Zone " + table.Cell(row, "id");
  if (kind == "zone-rooms") return "Room " + table.Cell(row, "room");
  if (kind == "zone-textures") return "Texture " + table.Cell(row, "image");
  if (kind.ends_with("-results")) {
    return names.Result(table.Cell(row, "id"));
  }
  if (kind.ends_with("-pairs")) {
    return std::format("{} + {} -> {}", names.Result(table.Cell(row, "first")),
                       names.Result(table.Cell(row, "second")),
                       names.Result(table.Cell(row, "result")));
  }
  std::string label = std::format(
      "{} + {} -> {}", Material(table.Number(row, "first_material")),
      Material(table.Number(row, "second_material")),
      Material(table.Number(row, "material")));
  if (kind != "combination-shield-materials") {
    label += std::format(" (categories {} + {})",
                         table.Number(row, "first_category"),
                         table.Number(row, "second_category"));
  }
  return label;
}

Check WriteChanged(const fs::path& root, std::string_view file,
                   const std::string& text, AssetLabelReport& report) {
  auto before = ReadFileBytes(root / Platform::Utf8Path(file));
  if (!before) return std::unexpected(before.error());
  if (TextOf(*before) == text) {
    ++report.unchanged;
    return {};
  }
  const OutputBytes output{
      .name = file,
      .bytes = std::as_bytes(std::span(text.data(), text.size()))};
  auto written = WriteOutputFiles(root, std::span(&output, 1));
  if (!written) return written;
  ++report.written;
  return {};
}

Check LabelTable(const fs::path& root, const Table& table,
                 const LabelIndex& names, bool refresh,
                 AssetLabelReport& report) {
  auto header = table.records.front().fields;
  const auto previous = std::ranges::find(header, Content::kDisplayName);
  const std::size_t label_at =
      static_cast<std::size_t>(previous - header.begin());
  const bool labeled = previous != header.end();
  if (labeled) header.erase(previous);
  header.insert(header.begin() + 1, std::string(Content::kDisplayName));
  std::vector<std::vector<std::string>> records{std::move(header)};
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    auto cells = table.records.at(row + 1).fields;
    std::string label =
        labeled && !refresh ? cells.at(label_at) : RowLabel(table, row, names);
    if (labeled) {
      cells.erase(cells.begin() + static_cast<std::ptrdiff_t>(label_at));
    }
    cells.insert(cells.begin() + 1, std::move(label));
    records.push_back(std::move(cells));
  }
  if (auto written =
          WriteChanged(root, table.path, Content::FormatCsv(records), report);
      !written) {
    return written;
  }
  const std::string schema =
      table.path.substr(0, table.path.size() - Content::kCsvSuffix.size()) +
      std::string(Content::kTableSchemaSuffix);
  report.rows += table.rows.size();
  return WriteChanged(root, schema, Content::FormatTableSchema(*table.spec),
                      report);
}

Check PutLabel(Json& object, std::string label, bool refresh,
               std::string_view where) {
  if (!object.is_object()) {
    return std::unexpected(std::format("{} must be an object", where));
  }
  const auto existing = object.find(std::string(Content::kDisplayName));
  if (existing != object.end()) {
    if (!existing->is_string()) {
      return std::unexpected(
          std::format("{}.display_name must be a string", where));
    }
    if (!refresh) label = existing->get<std::string>();
  }
  Json named = {{std::string(Content::kDisplayName), std::move(label)}};
  for (auto& [key, value] : object.items()) {
    if (key != Content::kDisplayName) named[key] = std::move(value);
  }
  object = std::move(named);
  return {};
}

std::string SourceRole(std::string_view source) {
  if (source.starts_with("SOUND/WAVE")) return "Sound bank";
  if (source.starts_with("MUSIC/")) return "Music";
  constexpr std::array<std::pair<std::string_view, std::string_view>, 17>
      kRoles{{{".SHP", "Actor model"},
              {".WEP", "Equipment model"},
              {".ZUD", "Actor archetype"},
              {".ZND", "Zone"},
              {".MPD", "Room"},
              {".ARM", "Area map"},
              {".EVT", "Event script"},
              {".SEQ", "Animation"},
              {".ESQ", "Shared animations"},
              {".ETM", "Texture animation"},
              {".TIM", "Image"},
              {".FBT", "Effect texture"},
              {".FBC", "Effect palette"},
              {".SYD", "Workshop recipes"},
              {".STR", "Movie"},
              {".XA", "Streamed audio"},
              {".P", "Effect package"}}};
  for (const auto& [extension, role] : kRoles) {
    if (source.ends_with(extension)) return std::string(role);
  }
  return "Asset";
}

std::string SourceLabel(const LabelIndex& names, std::string_view source) {
  const auto found = names.sources.find(source);
  if (found != names.sources.end() && !found->second.empty()) {
    return std::format("{} ({})", Join(found->second), source);
  }
  return std::format("{} {}", SourceRole(source), source);
}

std::string FileLabel(const LabelIndex& names, std::string_view file,
                      const Json& document, std::string_view source) {
  if (file == "catalogs/assets.json") return "Asset catalog";
  if (file == "catalogs/audio.json") return "Audio catalog";
  if (file == "audio/streams/index.json") return "Audio stream index";
  std::string declared_source = StringMember(document, "source");
  const auto provenance = document.find("provenance");
  if (declared_source.empty() && provenance != document.end()) {
    declared_source = StringMember(*provenance, "source");
  }
  if (!declared_source.empty()) source = declared_source;
  const auto found = names.files.find(file);
  std::string label = found != names.files.end() && !found->second.empty()
                          ? Join(found->second)
                          : SourceLabel(names, source.empty() ? file : source);
  if (file.ends_with(".image.json")) {
    label += " - texture " + StringMember(document, "id");
  } else if (file.ends_with("/stream.json")) {
    label += " - audio stream";
  }
  return label;
}

Check LabelCatalogEntries(Json& json, const LabelIndex& names, bool refresh,
                          std::string_view file) {
  const auto assets = json.find("assets");
  if (assets != json.end() && assets->is_array()) {
    for (Json& asset : *assets) {
      const auto provenance = asset.find("provenance");
      const std::string source = provenance == asset.end()
                                     ? StringMember(asset, "file")
                                     : StringMember(*provenance, "disc_path");
      if (auto label =
              PutLabel(asset, SourceLabel(names, source), refresh, file);
          !label) {
        return label;
      }
    }
  }
  for (const std::string_view key : {"music", "sfx_banks", "wave_banks"}) {
    const auto entries = json.find(std::string(key));
    if (entries == json.end() || !entries->is_array()) continue;
    for (std::size_t index = 0; index < entries->size(); ++index) {
      Json& entry = (*entries)[index];
      if (auto label = PutLabel(entry,
                                std::format("{} {}: {}", key, index,
                                            StringMember(entry, "asset")),
                                refresh, file);
          !label) {
        return label;
      }
    }
  }
  return {};
}

Check LabelInstruments(Json& json, std::string_view name, bool refresh,
                       std::string_view file) {
  const auto instruments = json.find("instruments");
  if (instruments == json.end() || !instruments->is_array()) return {};
  for (std::size_t index = 0; index < instruments->size(); ++index) {
    if (auto label =
            PutLabel((*instruments)[index],
                     std::format("{} - sample {}", name, index), refresh, file);
        !label) {
      return label;
    }
  }
  return {};
}

Check LabelStreams(Json& json, const LabelIndex& names, bool refresh,
                   std::string_view file) {
  for (const std::string_view key : {"streams", "xa"}) {
    const auto entries = json.find(std::string(key));
    if (entries == json.end() || !entries->is_array()) continue;
    for (Json& entry : *entries) {
      const std::string source = StringMember(entry, "source");
      const std::string label =
          std::format("{} from {}", key == "xa" ? "XA audio" : "Audio stream",
                      SourceLabel(names, source));
      if (auto named = PutLabel(entry, label, refresh, file); !named) {
        return named;
      }
    }
  }
  return {};
}

Check LabelJsonFile(const fs::path& root, std::string_view file,
                    std::string_view source, const LabelIndex& names,
                    bool refresh, AssetLabelReport& report) {
  auto document = ReadJson(root, file);
  if (!document) return std::unexpected(document.error());
  if (file.ends_with(".gltf")) {
    // Custom glTF metadata belongs in extras, never in a nonstandard root key.
    auto asset = document->find("asset");
    if (asset == document->end() || !asset->is_object() ||
        !asset->contains("extras") || !(*asset)["extras"].is_object() ||
        !(*asset)["extras"].contains("vs")) {
      return std::unexpected(std::format("{}: no native glTF payload", file));
    }
    Json& payload = (*asset)["extras"]["vs"];
    const std::string label = FileLabel(names, file, payload, source);
    if (auto named = PutLabel(payload, label, refresh, file); !named) {
      return named;
    }
  } else {
    const std::string label = FileLabel(names, file, *document, source);
    if (auto named = PutLabel(*document, label, refresh, file); !named) {
      return named;
    }
    if (auto entries = LabelCatalogEntries(*document, names, refresh, file);
        !entries) {
      return entries;
    }
    if (auto samples = LabelInstruments(*document, label, refresh, file);
        !samples) {
      return samples;
    }
    if (auto streams = LabelStreams(*document, names, refresh, file);
        !streams) {
      return streams;
    }
  }
  return WriteChanged(root, file, Content::FormatJsonText(*document) + "\n",
                      report);
}

std::map<std::string, std::string> StructuredFiles(
    const Content::ConversionManifest& manifest) {
  std::map<std::string, std::string> files;
  for (const auto& output : manifest.outputs) {
    for (const std::string& file : output.files) {
      if (file.starts_with("catalogs/menu/import/")) continue;
      if ((file.ends_with(".json") && !file.ends_with(".schema.json")) ||
          file.ends_with(".gltf")) {
        files.try_emplace(file, output.source);
      }
    }
  }
  return files;
}

}  // namespace

std::expected<std::string, std::string> DecodeAssetName(
    std::span<const std::byte> bytes) {
  if (std::ranges::all_of(bytes,
                          [](std::byte byte) { return byte == std::byte{}; })) {
    return std::string{};
  }
  std::string text;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const auto code = std::to_integer<std::uint8_t>(bytes[index]);
    if (code == Render::kTextTerminator) break;
    if (code < Render::kTextFirstControl) {
      AppendGlyph(text, code);
    } else if (code == Render::kTextNewline) {
      text += ' ';
    } else if (code >= Render::kTextFirstParameterized) {
      if (++index == bytes.size()) {
        return std::unexpected(
            std::format("control {:02X} has no parameter", code));
      }
      const auto parameter = std::to_integer<std::uint8_t>(bytes[index]);
      if (code == Render::kTextSpacing) {
        if (parameter != 0 && !text.empty() && text.back() != ' ') text += ' ';
      } else if (code != Render::kTextFontControl) {
        text += std::format("[control {:02X}:{:02X}]", code, parameter);
      }
    }
  }
  const auto first = text.find_first_not_of(' ');
  if (first == std::string::npos) return std::string{};
  return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

std::expected<std::vector<std::string>, std::string> ReadItemDisplayNames(
    const fs::path& root) {
  auto source = ReadSource(root, kItemNames);
  if (!source) return std::unexpected(source.error());
  if (source->bytes.size() % kNameBytes != 0) {
    return std::unexpected("MENU/ITEMNAME.BIN: incomplete 24-byte name");
  }
  std::vector<std::string> names;
  for (std::size_t at = 0; at < source->bytes.size(); at += kNameBytes) {
    auto name =
        DecodeAssetName(std::span(source->bytes).subspan(at, kNameBytes));
    if (!name) {
      return std::unexpected(std::format("{} item {}: {}", kItemNames,
                                         at / kNameBytes, name.error()));
    }
    names.push_back(std::move(*name));
  }
  return names;
}

std::expected<AssetLabelReport, std::string> LabelNativeAssets(
    const fs::path& root, bool refresh) {
  std::error_code error;
  const bool directory = fs::is_directory(root, error);
  if (error) {
    return std::unexpected(
        std::format("{}: {}", Platform::PathText(root), error.message()));
  }
  if (!directory) {
    return std::unexpected(
        std::format("{} is not an asset directory", Platform::PathText(root)));
  }
  auto manifest = Content::LoadConversionManifest(root);
  if (!manifest) return std::unexpected(manifest.error());
  RunStart run{.entries = {},
               .previous = manifest->value_or(Content::ConversionManifest{})};
  auto has_inventory = Present(root / Content::DataStore::ManifestName);
  if (!has_inventory) return std::unexpected(has_inventory.error());
  if (*has_inventory) {
    auto entries = Content::DataStore::LoadManifest(root);
    if (!entries) return std::unexpected(entries.error());
    run.entries = std::move(*entries);
  }
  auto tables = LoadTables(root);
  if (!tables) return std::unexpected(tables.error());
  LabelIndex names;
  if (auto items = LoadItemNames(root, run, names); !items) {
    return std::unexpected(items.error());
  }
  if (auto enemies = IndexEnemies(*tables, names); !enemies) {
    return std::unexpected(enemies.error());
  }
  IndexItems(*tables, names);
  if (auto archetypes = IndexArchetypes(root, run.previous, names);
      !archetypes) {
    return std::unexpected(archetypes.error());
  }
  AssetLabelReport report;
  for (const Table& table : *tables) {
    if (auto labeled = LabelTable(root, table, names, refresh, report);
        !labeled) {
      return std::unexpected(labeled.error());
    }
  }
  auto files = StructuredFiles(run.previous);
  for (const std::string_view file :
       {"catalogs/assets.json", "catalogs/audio.json",
        "audio/streams/index.json"}) {
    auto present = Present(root / Platform::Utf8Path(file));
    if (!present) return std::unexpected(present.error());
    if (*present) files.try_emplace(std::string(file), std::string(file));
  }
  for (const auto& [file, source] : files) {
    if (auto labeled =
            LabelJsonFile(root, file, source, names, refresh, report);
        !labeled) {
      return std::unexpected(labeled.error());
    }
  }
  return report;
}

}  // namespace VS::Extract
