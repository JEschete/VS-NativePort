#include "Content/BackgroundLayout.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"
#include "Core/Sha256.hpp"

namespace VS::Content {
namespace {

constexpr std::size_t kHeaderBytes = 12;
constexpr std::size_t kCellBytes = 24;
constexpr std::uint32_t kByteBits = 8;
constexpr std::uint32_t kByteMax = 255;
constexpr std::uint32_t kImageSideLimit = kByteMax + 1;
constexpr std::uint32_t kLegacyGridSideLimit = 31;
constexpr std::size_t kPaletteEntries = 256;
constexpr std::size_t kWordBytes = sizeof(std::uint32_t);
constexpr std::size_t kHeaderRowsAt = 4;
constexpr std::size_t kHeaderUnusedAt = 8;
constexpr std::size_t kRightDeltaAt = 4;
constexpr std::size_t kBottomDeltaAt = 6;
constexpr std::size_t kColorsAt = 8;
constexpr std::size_t kColorBytes = 4;
constexpr std::size_t kDepthCodeAt = 15;
constexpr auto kUnusedCodeOffsets = std::to_array<std::size_t>({11, 19, 23});
constexpr auto kCorners = std::to_array<std::string_view>(
    {"top_left", "top_right", "bottom_left", "bottom_right"});
constexpr std::string_view kPresentation = "camera-wrapped-grid";
constexpr std::string_view kDrawOrder = "row-major-farthest";

std::uint32_t Word(std::span<const std::byte> bytes, std::size_t offset,
                   std::size_t count) {
  std::uint32_t result = 0;
  for (std::size_t i = 0; i < count; ++i) {
    result |= std::to_integer<std::uint32_t>(bytes[offset + i])
              << (i * kByteBits);
  }
  return result;
}

std::int32_t Half(std::span<const std::byte> bytes, std::size_t offset) {
  return std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(Word(bytes, offset, 2)));
}

void Put(std::vector<std::byte>& bytes, std::uint32_t value,
         std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    bytes.push_back(
        static_cast<std::byte>((value >> (i * kByteBits)) & kByteMax));
  }
}

JsonDocument CellJson(const BackgroundCell& cell) {
  JsonDocument colors = JsonDocument::object();
  for (std::size_t i = 0; i < kCorners.size(); ++i) {
    const auto& color = cell.colors.at(i);
    colors[kCorners.at(i)] = {
        {"red", color.red}, {"green", color.green}, {"blue", color.blue}};
  }
  return {{"source_texels",
           {{"left", cell.left},
            {"top", cell.top},
            {"right", cell.right},
            {"bottom", cell.bottom}}},
          {"corner_colors", colors},
          {"import_provenance",
           {{"unused_corner_bytes", cell.unused_corner_bytes}}}};
}

std::expected<BackgroundColor, std::string> ReadColor(const JsonDocument& json,
                                                      std::string where) {
  const JsonMembers members(json, std::move(where));
  if (auto valid = members.Only({"red", "green", "blue"}); !valid) {
    return std::unexpected(valid.error());
  }
  auto red = members.Unsigned("red", kByteMax);
  auto green = members.Unsigned("green", kByteMax);
  auto blue = members.Unsigned("blue", kByteMax);
  if (!red) return std::unexpected(red.error());
  if (!green) return std::unexpected(green.error());
  if (!blue) return std::unexpected(blue.error());
  return BackgroundColor{static_cast<std::uint32_t>(*red),
                         static_cast<std::uint32_t>(*green),
                         static_cast<std::uint32_t>(*blue)};
}

std::expected<BackgroundCell, std::string> ReadCell(const JsonDocument& json,
                                                    std::string where) {
  const JsonMembers members(json, where);
  if (auto valid =
          members.Only({"source_texels", "corner_colors", "import_provenance"});
      !valid) {
    return std::unexpected(valid.error());
  }
  auto rect = members.Get("source_texels");
  auto colors = members.Get("corner_colors");
  auto provenance = members.Get("import_provenance");
  if (!rect) return std::unexpected(rect.error());
  if (!colors) return std::unexpected(colors.error());
  if (!provenance) return std::unexpected(provenance.error());
  const JsonMembers bounds(**rect, where + ".source_texels");
  if (auto valid = bounds.Only({"left", "top", "right", "bottom"}); !valid) {
    return std::unexpected(valid.error());
  }
  auto left = bounds.Integer("left", 0, kByteMax);
  auto top = bounds.Integer("top", 0, kByteMax);
  auto right = bounds.Integer("right", 0, kByteMax);
  auto bottom = bounds.Integer("bottom", 0, kByteMax);
  if (!left) return std::unexpected(left.error());
  if (!top) return std::unexpected(top.error());
  if (!right) return std::unexpected(right.error());
  if (!bottom) return std::unexpected(bottom.error());
  BackgroundCell cell{.left = static_cast<std::int32_t>(*left),
                      .top = static_cast<std::int32_t>(*top),
                      .right = static_cast<std::int32_t>(*right),
                      .bottom = static_cast<std::int32_t>(*bottom)};
  const JsonMembers shades(**colors, where + ".corner_colors");
  if (auto valid =
          shades.Only({"top_left", "top_right", "bottom_left", "bottom_right"});
      !valid) {
    return std::unexpected(valid.error());
  }
  for (std::size_t i = 0; i < kCorners.size(); ++i) {
    auto color = shades.Get(kCorners.at(i));
    if (!color) return std::unexpected(color.error());
    auto parsed = ReadColor(**color, shades.Where(kCorners.at(i)));
    if (!parsed) return std::unexpected(parsed.error());
    cell.colors.at(i) = *parsed;
  }
  const JsonMembers origin(**provenance, where + ".import_provenance");
  if (auto valid = origin.Only({"unused_corner_bytes"}); !valid) {
    return std::unexpected(valid.error());
  }
  auto unused = origin.Get("unused_corner_bytes");
  if (!unused) return std::unexpected(unused.error());
  if (!(**unused).is_array() ||
      (**unused).size() != cell.unused_corner_bytes.size()) {
    return std::unexpected(origin.Where("unused_corner_bytes") +
                           ": expected three unconsumed bytes");
  }
  for (std::size_t i = 0; i < cell.unused_corner_bytes.size(); ++i) {
    auto value = JsonWholeNumber((**unused).at(i));
    if (!value || *value < 0 || *value > kByteMax) {
      return std::unexpected(origin.Where("unused_corner_bytes") +
                             ": expected byte values");
    }
    cell.unused_corner_bytes.at(i) = static_cast<std::uint8_t>(*value);
  }
  return cell;
}

std::expected<void, std::string> ReadIdentity(
    const JsonMembers& members, BackgroundLayoutDocument& layout) {
  auto id = members.String("id");
  auto image = members.String("image_id");
  auto unreferenced = members.Boolean("unreferenced");
  auto depth = members.String("image_depth");
  auto presentation = members.String("presentation");
  auto order = members.String("draw_order");
  if (!id) return std::unexpected(id.error());
  if (!image) return std::unexpected(image.error());
  if (!unreferenced) return std::unexpected(unreferenced.error());
  if (!depth) return std::unexpected(depth.error());
  if (!presentation) return std::unexpected(presentation.error());
  if (!order) return std::unexpected(order.error());
  if (*depth != "indexed8" || *presentation != kPresentation ||
      *order != kDrawOrder) {
    return std::unexpected(
        "unsupported background image depth/presentation/draw order");
  }
  layout.id = *id;
  layout.image_id = *image;
  layout.unreferenced = *unreferenced;
  return {};
}

std::expected<void, std::string> ReadOrigin(const JsonDocument& json,
                                            BackgroundLayoutDocument& layout) {
  const JsonMembers members(json, "background.import_provenance");
  if (auto valid = members.Only({"source", "source_sha256", "image_source",
                                 "image_source_sha256", "unused_header_word"});
      !valid) {
    return std::unexpected(valid.error());
  }
  auto source = members.String("source");
  auto hash = members.String("source_sha256");
  auto image = members.String("image_source");
  auto image_hash = members.String("image_source_sha256");
  auto unused = members.Unsigned("unused_header_word", UINT32_MAX);
  if (!source) return std::unexpected(source.error());
  if (!hash) return std::unexpected(hash.error());
  if (!image) return std::unexpected(image.error());
  if (!image_hash) return std::unexpected(image_hash.error());
  if (!unused) return std::unexpected(unused.error());
  layout.source = *source;
  layout.source_sha256 = *hash;
  layout.image_source = *image;
  layout.image_source_sha256 = *image_hash;
  layout.unused_header_word = static_cast<std::uint32_t>(*unused);
  return {};
}
}  // namespace

std::expected<void, std::string> ValidateBackgroundLayout(
    const BackgroundLayoutDocument& layout) {
  if (!IsContentId(layout.id) || !IsContentId(layout.image_id) ||
      !IsDataRelativePath(layout.source) ||
      !IsDataRelativePath(layout.image_source) ||
      !IsLowercaseSha256(layout.source_sha256) ||
      !IsLowercaseSha256(layout.image_source_sha256)) {
    return std::unexpected("background: invalid identity or import provenance");
  }
  if (layout.columns < 1 || layout.columns > kBackgroundGridSideBudget ||
      layout.rows < 1 || layout.rows > kBackgroundGridSideBudget ||
      !IsBackgroundCellSize(layout.cell_size_units) ||
      layout.cells.size() > kBackgroundCellBudget ||
      layout.cells.size() != std::size_t{layout.columns} * layout.rows) {
    return std::unexpected(layout.id +
                           ": expected a complete 1..64 grid of at most 1024 "
                           "cells, sized 16, 32 or 64 units");
  }
  for (std::size_t i = 0; i < layout.cells.size(); ++i) {
    const auto& cell = layout.cells[i];
    for (const auto endpoint : {cell.left, cell.top, cell.right, cell.bottom}) {
      if (endpoint < 0 || static_cast<std::uint32_t>(endpoint) > kByteMax) {
        return std::unexpected(std::format(
            "{} cell {}: texel endpoint outside 0..255", layout.id, i));
      }
    }
    for (const auto& color : cell.colors) {
      if (color.red > kByteMax || color.green > kByteMax ||
          color.blue > kByteMax) {
        return std::unexpected(
            std::format("{} cell {}: color outside 0..255", layout.id, i));
      }
    }
  }
  return {};
}

std::expected<void, std::string> ValidateBackgroundImage(
    const BackgroundLayoutDocument& layout, const ImageDocument& image) {
  if (auto valid = ValidateBackgroundLayout(layout); !valid) {
    return valid;
  }
  if (image.image.depth != ImageDepth::kIndexed8 || image.image.width == 0 ||
      image.image.width > kImageSideLimit || image.image.width % 2 != 0 ||
      image.image.height == 0 || image.image.height > kImageSideLimit ||
      image.image.palettes.size() != 1 ||
      image.image.PaletteEntries() != kPaletteEntries ||
      image.material !=
          ImageMaterial{.zero_word = ZeroWord::kTransparent,
                        .semi_transparency = SemiTransparency::kOff}) {
    return std::unexpected(std::format(
        "{}: image {} has incompatible depth, dimensions, palette or material",
        layout.id, layout.image_id));
  }
  if (auto valid = ValidateImage(image.image); !valid) {
    return valid;
  }
  for (std::size_t i = 0; i < layout.cells.size(); ++i) {
    const auto& cell = layout.cells[i];
    if (static_cast<std::uint32_t>(std::max(cell.left, cell.right)) >=
            image.image.width ||
        static_cast<std::uint32_t>(std::max(cell.top, cell.bottom)) >=
            image.image.height) {
      return std::unexpected(
          std::format("{} cell {}: source rectangle exceeds image {}",
                      layout.id, i, image.id));
    }
  }
  return {};
}

std::expected<ImageDocument, std::string> LoadBackgroundLayoutImage(
    const std::filesystem::path& root, const ConversionManifest& manifest,
    const BackgroundLayoutDocument& layout) {
  auto image = LoadConvertedImageById(root, manifest, layout.image_id);
  if (!image) return std::unexpected(layout.id + ": " + image.error());
  if (auto valid = ValidateBackgroundImage(layout, *image); !valid) {
    return std::unexpected(valid.error());
  }
  const auto* output = manifest.Find(layout.image_id);
  if (output == nullptr) {
    return std::unexpected("background image output disappeared");
  }
  auto dependencies = output->dependencies;
  dependencies.push_back({output->source, output->source_sha256});
  const auto* owner = manifest.FilesOf(*output);
  if (owner == nullptr) {
    return std::unexpected("background image alias target disappeared");
  }
  if (owner != output) {
    dependencies.insert(dependencies.end(), owner->dependencies.begin(),
                        owner->dependencies.end());
    dependencies.push_back({owner->source, owner->source_sha256});
  }
  const auto reader = DirectoryFileReader(root);
  for (const auto& dependency : dependencies) {
    auto original = reader("disc/" + dependency.source);
    if (!original || Core::Sha256Hex(*original) != dependency.source_sha256) {
      return std::unexpected(layout.image_id + ": missing/stale source " +
                             dependency.source + "; rerun its image converter");
    }
  }
  return image;
}

// decomp: BATTLE.PRG func_8009147C (src/BATTLE/BATTLE.PRG/2842C.c:464)
std::expected<BackgroundLayoutDocument, std::string> DecodeFar(
    std::span<const std::byte> bytes, BackgroundLayoutDocument identity) {
  if (bytes.size() < kHeaderBytes) {
    return std::unexpected("FAR: truncated header");
  }
  identity.columns = Word(bytes, 0, kWordBytes);
  identity.rows = Word(bytes, kHeaderRowsAt, kWordBytes);
  identity.unused_header_word = Word(bytes, kHeaderUnusedAt, kWordBytes);
  identity.cell_size_units = kBackgroundDefaultCellUnits;
  if (identity.columns < 1 || identity.columns > kLegacyGridSideLimit ||
      identity.rows < 1 || identity.rows > kLegacyGridSideLimit ||
      bytes.size() != kHeaderBytes + std::size_t{identity.columns} *
                                         identity.rows * kCellBytes) {
    return std::unexpected(
        "FAR: inconsistent dimensions or truncated/trailing tile table");
  }
  identity.cells.clear();
  for (std::size_t offset = kHeaderBytes; offset < bytes.size();
       offset += kCellBytes) {
    BackgroundCell cell{.left = Half(bytes, offset),
                        .top = Half(bytes, offset + 2)};
    cell.right = cell.left + Half(bytes, offset + kRightDeltaAt);
    cell.bottom = cell.top + Half(bytes, offset + kBottomDeltaAt);
    if (Word(bytes, offset + kDepthCodeAt, 1) != 1) {
      return std::unexpected(
          std::format("FAR tile {}: unsupported texture-page depth code",
                      identity.cells.size()));
    }
    for (std::size_t i = 0; i < cell.colors.size(); ++i) {
      const auto at = offset + kColorsAt + i * kColorBytes;
      cell.colors.at(i) = {Word(bytes, at, 1), Word(bytes, at + 1, 1),
                           Word(bytes, at + 2, 1)};
    }
    for (std::size_t i = 0; i < cell.unused_corner_bytes.size(); ++i) {
      cell.unused_corner_bytes.at(i) = std::to_integer<std::uint8_t>(
          bytes[offset + kUnusedCodeOffsets.at(i)]);
    }
    identity.cells.push_back(cell);
  }
  if (auto valid = ValidateBackgroundLayout(identity); !valid) {
    return std::unexpected(valid.error());
  }
  return identity;
}

std::expected<std::vector<std::byte>, std::string> EncodeFar(
    const BackgroundLayoutDocument& layout) {
  if (auto valid = ValidateBackgroundLayout(layout); !valid) {
    return std::unexpected(valid.error());
  }
  if (layout.cell_size_units != kBackgroundDefaultCellUnits) {
    return std::unexpected(
        layout.id + ": native cell size has no original FAR representation");
  }
  if (layout.columns > kLegacyGridSideLimit ||
      layout.rows > kLegacyGridSideLimit) {
    return std::unexpected(
        layout.id + ": native grid exceeds the admitted FAR import budget");
  }
  std::vector<std::byte> bytes;
  Put(bytes, layout.columns, kWordBytes);
  Put(bytes, layout.rows, kWordBytes);
  Put(bytes, layout.unused_header_word, kWordBytes);
  for (const auto& cell : layout.cells) {
    for (const auto value : {cell.left, cell.top, cell.right - cell.left,
                             cell.bottom - cell.top}) {
      Put(bytes, static_cast<std::uint32_t>(value), 2);
    }
    const std::array<std::uint8_t, kBackgroundCornerCount> codes{
        cell.unused_corner_bytes[0], 1, cell.unused_corner_bytes[1],
        cell.unused_corner_bytes[2]};
    for (std::size_t i = 0; i < cell.colors.size(); ++i) {
      const auto& color = cell.colors.at(i);
      Put(bytes, color.red, 1);
      Put(bytes, color.green, 1);
      Put(bytes, color.blue, 1);
      Put(bytes, codes.at(i), 1);
    }
  }
  return bytes;
}

std::expected<std::string, std::string> WriteBackgroundLayout(
    const BackgroundLayoutDocument& layout) {
  if (auto valid = ValidateBackgroundLayout(layout); !valid) {
    return std::unexpected(valid.error());
  }
  JsonDocument cells = JsonDocument::array();
  for (const auto& cell : layout.cells) cells.push_back(CellJson(cell));
  const JsonDocument json{
      {"schema", kBackgroundLayoutSchema},
      {"schema_version", kBackgroundLayoutVersion},
      {"id", layout.id},
      {"image_id", layout.image_id},
      {"unreferenced", layout.unreferenced},
      {"image_depth", "indexed8"},
      {"presentation", kPresentation},
      {"draw_order", kDrawOrder},
      {"columns", layout.columns},
      {"rows", layout.rows},
      {"cell_size_units", layout.cell_size_units},
      {"cells", cells},
      {"import_provenance",
       {{"source", layout.source},
        {"source_sha256", layout.source_sha256},
        {"image_source", layout.image_source},
        {"image_source_sha256", layout.image_source_sha256},
        {"unused_header_word", layout.unused_header_word}}}};
  return FormatJsonText(json) + "\n";
}

std::expected<BackgroundLayoutDocument, std::string> ReadBackgroundLayout(
    std::span<const std::byte> bytes, std::string_view name) {
  constexpr std::size_t kMaxLayoutTextBytes = std::size_t{1024} * 1024;
  if (bytes.size() > kMaxLayoutTextBytes) {
    return std::unexpected(std::string(name) +
                           ": background JSON exceeds 1 MiB");
  }
  auto json = ParseJsonText(bytes, name);
  if (!json) return std::unexpected(json.error());
  const JsonMembers members(*json, std::string(name));
  if (auto valid = members.Only(
          {"schema", "schema_version", "id", "image_id", "unreferenced",
           "image_depth", "presentation", "draw_order", "columns", "rows",
           "cell_size_units", "cells", "import_provenance"});
      !valid) {
    return std::unexpected(valid.error());
  }
  auto schema = members.String("schema");
  auto version = members.Unsigned("schema_version", kBackgroundLayoutVersion);
  if (!schema) return std::unexpected(schema.error());
  if (!version) return std::unexpected(version.error());
  if (*schema != kBackgroundLayoutSchema || *version < 1 ||
      *version > kBackgroundLayoutVersion) {
    return std::unexpected(std::string(name) +
                           ": unsupported background schema/version");
  }
  BackgroundLayoutDocument layout;
  if (auto valid = ReadIdentity(members, layout); !valid) {
    return std::unexpected(valid.error());
  }
  const auto side_limit =
      *version == 1 ? kLegacyGridSideLimit : kBackgroundGridSideBudget;
  auto columns = members.Unsigned("columns", side_limit);
  auto rows = members.Unsigned("rows", side_limit);
  auto cells = members.Get("cells");
  auto origin = members.Get("import_provenance");
  if (!columns) return std::unexpected(columns.error());
  if (!rows) return std::unexpected(rows.error());
  if (!cells) return std::unexpected(cells.error());
  if (!origin) return std::unexpected(origin.error());
  layout.columns = static_cast<std::uint32_t>(*columns);
  layout.rows = static_cast<std::uint32_t>(*rows);
  if (*version >= kBackgroundLayoutVersion) {
    auto units = members.Unsigned("cell_size_units", kBackgroundLargeCellUnits);
    if (!units) return std::unexpected(units.error());
    layout.cell_size_units = static_cast<std::uint32_t>(*units);
  } else if (members.Find("cell_size_units") != nullptr) {
    return std::unexpected(
        std::string(name) +
        ": cell_size_units requires background schema version 2");
  }
  if (!(**cells).is_array() || (**cells).size() != *columns * *rows) {
    return std::unexpected(std::string(name) +
                           ": cells must match grid dimensions");
  }
  if (auto valid = ReadOrigin(**origin, layout); !valid) {
    return std::unexpected(valid.error());
  }
  for (const auto& cell : **cells) {
    auto parsed =
        ReadCell(cell, std::format("{}.cells[{}]", name, layout.cells.size()));
    if (!parsed) return std::unexpected(parsed.error());
    layout.cells.push_back(*parsed);
  }
  if (auto valid = ValidateBackgroundLayout(layout); !valid) {
    return std::unexpected(valid.error());
  }
  return layout;
}

std::expected<BackgroundLayoutDocument, std::string> LoadConvertedBackground(
    const DataStore& store, const ConvertedSource& source) {
  const auto* manifest = store.Conversions();
  if (manifest == nullptr) {
    return std::unexpected("no background conversion manifest");
  }
  if (auto valid =
          CheckRequiredConverters(*manifest, kBackgroundLayoutConverters);
      !valid) {
    return std::unexpected(valid.error() +
                           " (run VS.Extract backgrounds <datadir>)");
  }
  if (source.conversion->converter != kFarConverterName ||
      source.files->files.size() != 1) {
    return std::unexpected(source.entry->path +
                           ": no unique native background layout");
  }
  const auto& file = source.files->files.front();
  auto bytes = DirectoryFileReader(store.Root())(file);
  if (!bytes) return std::unexpected("cannot read native background " + file);
  auto layout = ReadBackgroundLayout(*bytes, file);
  if (!layout) return std::unexpected(layout.error());
  if (layout->id != source.files->id || layout->source != source.entry->path ||
      layout->source_sha256 != source.conversion->source_sha256 ||
      layout->unreferenced != source.conversion->unreferenced ||
      source.conversion->dependencies !=
          std::vector<SourceDependency>{
              {.source = layout->image_source,
               .source_sha256 = layout->image_source_sha256}}) {
    return std::unexpected(
        file +
        ": stale layout identity/provenance; rerun VS.Extract backgrounds");
  }
  const auto reader = DirectoryFileReader(store.Root());
  auto original = reader("disc/" + layout->source);
  if (!original || Core::Sha256Hex(*original) != layout->source_sha256) {
    return std::unexpected(file + ": missing/stale source " + layout->source +
                           "; rerun VS.Extract images and backgrounds");
  }
  return layout;
}
}  // namespace VS::Content
