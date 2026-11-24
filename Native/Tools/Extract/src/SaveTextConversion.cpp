#include "Extract/SaveTextConversion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/GameTextCodec.hpp"
#include "Content/JsonText.hpp"
#include "Content/NativeText.hpp"
#include "Content/StringTables.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ImageFilesWriter.hpp"

namespace VS::Extract {
namespace {
namespace text = Content;
constexpr std::size_t kFontOffset = 0x46B68;
constexpr std::size_t kPaletteOffset = kFontOffset + 0x7000 + 222ull * 128;
constexpr unsigned kAtlasWidth = 256, kAtlasHeight = 108;
constexpr std::size_t kAtlasBytes = 128ull * kAtlasHeight;
constexpr unsigned kPaletteBytes = 32, kByteBits = 8;
constexpr unsigned kNibbleBits = 4, kNibbleMask = 15;
constexpr unsigned kBaseGlyphs = 189, kColumns = 21, kCell = 12;
constexpr int kNormalAdvance = 6;
constexpr std::string_view kFontSource = "TITLE/TITLE.PRG";
constexpr std::string_view kTextSource = "MENU/MCMAN.BIN";
constexpr std::string_view kImageStem = "save-font";
constexpr std::string_view kImageDirectory = "textures/ui";

using Pair = std::pair<std::string_view, std::string_view>;
constexpr std::array kNativeMessages{
    Pair{"text.save.reading", "Reading save files"},
    Pair{"text.save.load-failed", "Load failed - game unchanged"},
    Pair{"text.save.save-failed", "Save failed - see log"},
    Pair{"text.save.read-error", "Cannot read native saves"},
    Pair{"text.save.summary-error", "Save summary unavailable"},
    Pair{"text.save.verify-error", "Cannot verify selected save"},
    Pair{"text.save.changed", "File changed - select again"},
    Pair{"text.save.open-error", "Cannot open native saves"},
    Pair{"text.save.backup-available", "Backup available - select to use"},
    Pair{"text.save.invalid", "Invalid data - not overwritten"},
    Pair{"text.save.select", "Select a file"},
    Pair{"text.save.resume-failed", "Previous resume failed"},
    Pair{"text.save.confirm.recovered-container", "Use recovered container"},
    Pair{"text.save.confirm.empty-container", "Use empty container"},
    Pair{"text.save.confirm.container", "Open selected container"},
    Pair{"text.save.confirm.replace-invalid", "Replace invalid original"},
    Pair{"text.save.confirm.recover", "Recover from valid backup"},
    Pair{"text.save.confirm.save", "Save to selected file"},
    Pair{"text.save.confirm.load", "Load selected game"},
    Pair{"text.save.opening-container", "Opening container"},
    Pair{"text.save.saving", "Saving game"},
    Pair{"text.save.loading", "Loading game"},
    Pair{"text.save.unusable", "No usable native save"},
    Pair{"text.save.complete", "Save complete"},
    Pair{"text.save.no-clear-save", "No ending clear save available"},
};

Content::JsonDocument FontJson() {
  Content::JsonDocument glyphs = Content::JsonDocument::object();
  // decomp: MENU7.PRG printCharacter (src/MENU/memcard.h:1426)
  // Base bank: six-pixel advances except OE/oe and named symbols.
  for (unsigned glyph = 0; glyph < kBaseGlyphs; ++glyph) {
    auto identity = Content::GameGlyphIdentity(glyph);
    if (!identity) continue;
    const int advance =
        glyph == 62 || glyph == 85 || (glyph >= 173 && glyph <= 188)
            ? static_cast<int>(kCell)
            : kNormalAdvance;
    glyphs[*identity] = {{"x", (glyph % kColumns) * kCell},
                         {"y", (glyph / kColumns) * kCell},
                         {"width", kCell},
                         {"height", kCell},
                         {"advance", advance},
                         {"visible", *identity != " "}};
  }
  return {{"schema", "vs.font"},     {"schema_version", 1},
          {"id", text::kSaveFontId}, {"image", text::kSaveFontImageId},
          {"line_height", kCell},    {"glyphs", std::move(glyphs)}};
}

std::expected<Content::JsonDocument, std::string> StringsJson(
    const SourceFile& source) {
  Content::JsonDocument strings = Content::JsonDocument::object();
  for (const auto& [id, value] : kNativeMessages) strings[id] = value;
  constexpr std::array kChoices{
      std::pair{"text.save.choice.yes",
                Content::text::mcman_bin::offset::kYesIndent},
      std::pair{"text.save.choice.no",
                Content::text::mcman_bin::offset::kNoIndent}};
  for (const auto& [id, offset] : kChoices) {
    const std::size_t at = static_cast<std::size_t>(offset) * 2;
    if (at >= source.bytes.size()) {
      return std::unexpected("MCMAN: truncated choice");
    }
    auto decoded =
        Content::DecodeGameText(std::span(source.bytes).subspan(at), id);
    if (!decoded) return std::unexpected(decoded.error());
    auto exact = Content::EncodeGameText(*decoded, id);
    const auto original =
        std::span(source.bytes).subspan(at, decoded->consumed);
    if (!exact ||
        *exact != std::vector<std::byte>(original.begin(), original.end())) {
      return std::unexpected("MCMAN choice failed exact import verification");
    }
    // Original leading FA indentation is layout, not part of the localized
    // choice. Only that proven prefix is normalized; other controls fail.
    std::string value = decoded->markup;
    if (value.starts_with("{spacing:")) {
      const auto end = value.find('}');
      if (end != std::string::npos) value.erase(0, end + 1);
    }
    auto native = Content::ParseNativeText(value, id);
    if (!native) return std::unexpected(native.error());
    strings[id] = value;
  }
  return Content::JsonDocument{{"schema", "vs.strings"},
                               {"schema_version", 1},
                               {"font", text::kSaveFontId},
                               {"scale", 1},
                               {"strings", std::move(strings)}};
}

std::expected<Content::ImageDocument, std::string> FontImage(
    const SourceFile& source) {
  if (source.bytes.size() < kPaletteOffset + kPaletteBytes) {
    return std::unexpected("TITLE: truncated base save font");
  }
  Content::ImageDocument image;
  image.id = text::kSaveFontImageId;
  image.image.depth = Content::ImageDepth::kIndexed4;
  image.image.width = kAtlasWidth;
  image.image.height = kAtlasHeight;
  for (const auto value :
       std::span(source.bytes).subspan(kFontOffset, kAtlasBytes)) {
    const auto byte = std::to_integer<std::uint8_t>(value);
    image.image.indices.push_back(byte & kNibbleMask);
    image.image.indices.push_back(byte >> kNibbleBits);
  }
  image.image.palettes.emplace_back();
  for (std::size_t at = kPaletteOffset; at < kPaletteOffset + kPaletteBytes;
       at += 2) {
    image.image.palettes.front().push_back(static_cast<std::uint16_t>(
        std::to_integer<unsigned>(source.bytes[at]) |
        (std::to_integer<unsigned>(source.bytes[at + 1]) << kByteBits)));
  }
  image.material = Content::ImageMaterial{Content::ZeroWord::kTransparent,
                                          Content::SemiTransparency::kAdd};
  image.provenance.source = source.path;
  image.provenance.source_sha256 = HashOf(source);
  image.provenance.offset = kFontOffset;
  image.provenance.note =
      "G02022 base bank: 128*108 bytes at 0x46B68; 16 palette words at "
      "0x54A68. Remaining TITLE font banks/images and SYSTEM.DAT are not "
      "converted.";
  return image;
}

std::vector<std::byte> Bytes(std::string_view value) {
  const auto bytes = std::as_bytes(std::span(value));
  return {bytes.begin(), bytes.end()};
}

std::expected<Content::ImageFile, std::string> ImportProvenance(
    const SourceFile& source) {
  Content::JsonDocument provenance{{"schema", "vs.save-text-import"},
                                   {"schema_version", 1},
                                   {"source", source.path},
                                   {"source_sha256", HashOf(source)},
                                   {"choices", Content::JsonDocument::array()}};
  for (const auto offset : {Content::text::mcman_bin::offset::kYesIndent,
                            Content::text::mcman_bin::offset::kNoIndent}) {
    const auto at = static_cast<std::size_t>(offset) * 2;
    if (at >= source.bytes.size()) {
      return std::unexpected("MCMAN: truncated choice");
    }
    const auto decoded = Content::DecodeGameText(
        std::span(source.bytes).subspan(at), "save choice");
    if (!decoded) return std::unexpected(decoded.error());
    const bool yes = offset == Content::text::mcman_bin::offset::kYesIndent;
    provenance["choices"].push_back(
        {{"id", yes ? "text.save.choice.yes" : "text.save.choice.no"},
         {"table_entry", yes ? Content::text::mcman_bin::index::kYesIndent
                             : Content::text::mcman_bin::index::kNoIndent},
         {"byte_offset", at},
         {"markup", decoded->markup},
         {"consumed_bytes", decoded->consumed}});
  }
  return Content::ImageFile{"text/save-text.import.json",
                            Bytes(Content::FormatJsonText(provenance) + "\n")};
}
}  // namespace

std::expected<void, std::string> ConvertSaveText(
    const std::filesystem::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto mcman = ReadSource(root, kTextSource);
  auto title = ReadSource(root, kFontSource);
  if (!mcman) return std::unexpected(mcman.error());
  if (!title) return std::unexpected(title.error());
  auto strings = StringsJson(*mcman);
  auto image = FontImage(*title);
  if (!strings) return std::unexpected(strings.error());
  if (!image) return std::unexpected(image.error());
  auto images = EncodeImageFiles(*image, kImageStem);
  if (!images) return std::unexpected(images.error());
  auto decoded_image =
      Content::DecodeImageFiles(kImageStem, Content::FileListReader(*images));
  if (!decoded_image || *decoded_image != *image) {
    return std::unexpected("save font: image round trip failed");
  }
  auto batch = StartBatch(start->previous, text::kSaveTextConverter, 1, force);
  std::vector<Content::ImageFile> files;
  const auto add = [&](std::string_view id, const SourceFile& source,
                       std::vector<std::string> paths, std::string note) {
    Content::ConversionOutput output{
        .id = std::string(id),
        .source = source.path,
        .source_sha256 = HashOf(source),
        .converter = std::string(text::kSaveTextConverter),
        .files = std::move(paths),
        .dependencies = {},
        .unreferenced = false,
        .note = std::move(note),
        .alias_of = {}};
    auto expected = output;
    expected.files.clear();
    const auto* old = MatchingOutput(start->previous, root, expected);
    batch.outputs.push_back(batch.reuse && old ? *old : output);
    return !(batch.reuse && old);
  };
  if (add(text::kSaveTextId, *mcman,
          {std::string(text::kSaveTextPath), "text/save-text.import.json"},
          "Only MCMAN entries 36/37 (Yes/No); 25 native-owned status messages. "
          "All other 135 MCMAN entries, including 47 unsupported Japanese "
          "labels, remain outside this slice.")) {
    files.push_back({std::string(text::kSaveTextPath),
                     Bytes(Content::FormatJsonText(*strings) + "\n")});
    auto provenance = ImportProvenance(*mcman);
    if (!provenance) return std::unexpected(provenance.error());
    files.push_back(std::move(*provenance));
  }
  if (add(text::kSaveFontId, *title, {std::string(text::kSaveFontPath)},
          "G02022 English base glyph metrics; upstream memcard.h:1426 and "
          "shared GameTextCodec glyph identities.")) {
    files.push_back({std::string(text::kSaveFontPath),
                     Bytes(Content::FormatJsonText(FontJson()) + "\n")});
  }
  std::vector<std::string> paths;
  for (const auto& file : *images) {
    paths.push_back(std::string(kImageDirectory) + "/" + file.name);
  }
  if (add(text::kSaveFontImageId, *title, paths, image->provenance.note)) {
    for (auto& file : *images) {
      file.name = std::string(kImageDirectory) + "/" + file.name;
      files.push_back(std::move(file));
    }
  }
  if (files.empty()) return {};
  auto published = PublishBatch(root, start->previous, batch, [&]() {
    std::vector<OutputBytes> output;
    for (const auto& file : files) output.push_back({file.name, file.bytes});
    return WriteOutputFiles(root, output);
  });
  if (!published) return std::unexpected(published.error());
  return {};
}
}  // namespace VS::Extract
