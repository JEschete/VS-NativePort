// Native image files (ImageFiles.hpp).
#include "Content/ImageFiles.hpp"

#include <lodepng.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
namespace fs = std::filesystem;
using Json = JsonDocument;
using Bytes = std::vector<std::byte>;
using PngBytes = std::vector<unsigned char>;

constexpr unsigned kByteBits = 8;
constexpr std::size_t kRgbaChannels = 4;
constexpr unsigned kRgbaBitDepth = 8;
constexpr std::size_t kGreenChannel = 1;
constexpr std::size_t kBlueChannel = 2;
constexpr std::size_t kAlphaChannel = 3;

constexpr std::array kZeroWordNames{
    std::string_view{"transparent"},
    std::string_view{"black"},
};
constexpr std::array kSemiTransparencyNames{
    std::string_view{"off"},         std::string_view{"average"},
    std::string_view{"add"},         std::string_view{"subtract"},
    std::string_view{"add_quarter"},
};

constexpr std::string_view kStpNone = "none";
constexpr std::string_view kStpAll = "all";
constexpr std::string_view kStpMask = "mask";

std::string NameOf(std::string_view stem, std::string_view suffix) {
  return std::string(stem) + std::string(suffix);
}

std::expected<void, std::string> CheckId(std::string_view id) {
  if (!IsContentId(id)) {
    return std::unexpected(std::format(
        "image id '{}' must be lowercase letters, digits, '.', '_' or '-'",
        id));
  }
  return {};
}

std::expected<void, std::string> CheckTexturePage(
    const TexturePagePlacement& placement) {
  if (placement.page >= kTexturePageCount ||
      placement.vram != TexturePageRect(placement.page)) {
    return std::unexpected(std::format(
        "texture page {} must be below {} and fill its own VRAM rectangle",
        placement.page, kTexturePageCount));
  }
  return {};
}

PngBytes ToPngBytes(std::span<const std::byte> data) {
  PngBytes bytes(data.size());
  std::ranges::transform(data, bytes.begin(), [](std::byte value) {
    return std::to_integer<unsigned char>(value);
  });
  return bytes;
}

// --- Names and identifiers.

std::expected<void, std::string> CheckStem(std::string_view stem) {
  if (stem.empty() || stem == "." || stem == ".." ||
      stem.find_first_of("/\\:") != std::string_view::npos) {
    return std::unexpected(
        std::format("'{}' is not an image file name stem", stem));
  }
  return {};
}

void SetMode(LodePNGColorMode& mode, LodePNGColorType type, unsigned depth) {
  mode.colortype = type;
  mode.bitdepth = depth;
}

std::vector<std::uint8_t> UnpackBits(std::span<const unsigned char> raw,
                                     std::size_t count, unsigned bits) {
  const unsigned mask = (1U << bits) - 1;
  std::vector<std::uint8_t> values(count);
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t bit = index * bits;
    const auto shift =
        static_cast<unsigned>(kByteBits - bits - (bit % kByteBits));
    values[index] =
        static_cast<std::uint8_t>((raw[bit / kByteBits] >> shift) & mask);
  }
  return values;
}

// --- PNG reading.

std::string_view ColorTypeName(LodePNGColorType type) {
  switch (type) {
    case LCT_GREY:
      return "greyscale";
    case LCT_RGB:
      return "RGB";
    case LCT_PALETTE:
      return "indexed";
    case LCT_GREY_ALPHA:
      return "greyscale with alpha";
    case LCT_RGBA:
      return "RGBA";
    default:
      break;
  }
  return "an unknown colour type";
}

// Reads the PNG header and checks the size before decoding any pixels.
std::expected<void, std::string> CheckPngSize(const PngBytes& png,
                                              lodepng::State& state,
                                              std::uint32_t width,
                                              std::uint32_t height,
                                              std::string_view name) {
  unsigned found_width = 0;
  unsigned found_height = 0;
  if (const unsigned error = lodepng_inspect(&found_width, &found_height,
                                             &state, png.data(), png.size());
      error != 0) {
    return std::unexpected(std::format("{}: not a readable PNG: {}", name,
                                       lodepng_error_text(error)));
  }
  if (found_width != width || found_height != height) {
    return std::unexpected(std::format("{} is {}x{}; the sidecar says {}x{}",
                                       name, found_width, found_height, width,
                                       height));
  }
  return {};
}

struct IndexedPng {
  std::vector<std::uint8_t> indices;
  std::vector<ImageColor> palette;
};

std::expected<IndexedPng, std::string> DecodeIndexedPng(const Bytes& bytes,
                                                        std::uint32_t width,
                                                        std::uint32_t height,
                                                        std::string_view name) {
  const PngBytes png = ToPngBytes(bytes);
  lodepng::State state;
  state.decoder.color_convert = 0;
  if (auto size = CheckPngSize(png, state, width, height, name); !size) {
    return std::unexpected(size.error());
  }
  if (state.info_png.color.colortype != LCT_PALETTE) {
    return std::unexpected(std::format(
        "{}: an indexed image must stay an indexed (palette) PNG; this one is "
        "{}",
        name, ColorTypeName(state.info_png.color.colortype)));
  }
  PngBytes raw;
  unsigned decoded_width = 0;
  unsigned decoded_height = 0;
  if (const unsigned error =
          lodepng::decode(raw, decoded_width, decoded_height, state, png);
      error != 0) {
    return std::unexpected(std::format("{}: cannot decode the PNG: {}", name,
                                       lodepng_error_text(error)));
  }
  const LodePNGColorMode& color = state.info_png.color;
  IndexedPng result{
      .indices = UnpackBits(raw, std::size_t{width} * height, color.bitdepth),
      .palette = {}};
  const std::span entries(color.palette, color.palettesize * kRgbaChannels);
  for (std::size_t first = 0; first < entries.size(); first += kRgbaChannels) {
    result.palette.push_back({.red = entries[first],
                              .green = entries[first + kGreenChannel],
                              .blue = entries[first + kBlueChannel],
                              .alpha = entries[first + kAlphaChannel]});
  }
  return result;
}

std::expected<std::vector<ImageColor>, std::string> DecodeRgbaPng(
    const Bytes& bytes, std::uint32_t width, std::uint32_t height,
    std::string_view name) {
  const PngBytes png = ToPngBytes(bytes);
  lodepng::State state;
  if (auto size = CheckPngSize(png, state, width, height, name); !size) {
    return std::unexpected(size.error());
  }
  SetMode(state.info_raw, LCT_RGBA, kRgbaBitDepth);
  PngBytes raw;
  unsigned decoded_width = 0;
  unsigned decoded_height = 0;
  if (const unsigned error =
          lodepng::decode(raw, decoded_width, decoded_height, state, png);
      error != 0) {
    return std::unexpected(std::format("{}: cannot decode the PNG: {}", name,
                                       lodepng_error_text(error)));
  }
  std::vector<ImageColor> colors;
  colors.reserve(raw.size() / kRgbaChannels);
  for (std::size_t first = 0; first < raw.size(); first += kRgbaChannels) {
    colors.push_back({.red = raw[first],
                      .green = raw[first + kGreenChannel],
                      .blue = raw[first + kBlueChannel],
                      .alpha = raw[first + kAlphaChannel]});
  }
  return colors;
}

// --- Sidecar reading. Every member is checked; unknown members are errors so
// a misspelt field is not silently ignored.

struct Sidecar {
  ImageDocument document;  // the image holds the depth and size only
  std::size_t entries = 0;
  std::vector<std::vector<bool>> palette_stp;
  std::string direct_stp;
};

template <typename Enum, std::size_t Count>
std::expected<Enum, std::string> EnumOf(
    const JsonMembers& members, std::string_view key,
    const std::array<std::string_view, Count>& names) {
  auto text = members.String(key);
  if (!text) return std::unexpected(text.error());
  const auto found = std::ranges::find(names, *text);
  if (found == names.end()) {
    return std::unexpected(
        std::format("{} cannot be '{}'", members.Where(key), *text));
  }
  return static_cast<Enum>(found - names.begin());
}

std::expected<TimRect, std::string> RectOf(const Json& value,
                                           const std::string& where) {
  enum Field : std::uint8_t { kX, kY, kWidth, kHeight, kFieldCount };
  constexpr std::int64_t kMinCoordinate =
      std::numeric_limits<std::int16_t>::min();
  constexpr std::int64_t kMaxCoordinate =
      std::numeric_limits<std::int16_t>::max();
  constexpr std::int64_t kMaxExtent = std::numeric_limits<std::uint16_t>::max();
  if (!value.is_array() || value.size() != kFieldCount ||
      !std::ranges::all_of(value, [](const Json& element) {
        return element.is_number_integer();
      })) {
    return std::unexpected(
        std::format("{} must be [x, y, width, height]", where));
  }
  const auto field = [&value](Field index) {
    return value[static_cast<std::size_t>(index)].get<std::int64_t>();
  };
  const auto coordinate = [](std::int64_t number) {
    return number >= kMinCoordinate && number <= kMaxCoordinate;
  };
  const auto extent = [](std::int64_t number) {
    return number >= 0 && number <= kMaxExtent;
  };
  if (!coordinate(field(kX)) || !coordinate(field(kY)) ||
      !extent(field(kWidth)) || !extent(field(kHeight))) {
    return std::unexpected(std::format(
        "{} is outside the TIM rectangle ranges (x, y 16-bit signed; width, "
        "height 16-bit unsigned)",
        where));
  }
  return TimRect{.x = static_cast<std::int16_t>(field(kX)),
                 .y = static_cast<std::int16_t>(field(kY)),
                 .width = static_cast<std::uint16_t>(field(kWidth)),
                 .height = static_cast<std::uint16_t>(field(kHeight))};
}

std::expected<TimOrigin, std::string> TimOriginOf(const Json& value,
                                                  const std::string& where) {
  const JsonMembers tim(value, where);
  if (auto only = tim.Only({"id", "flags", "image", "clut"}); !only) {
    return std::unexpected(only.error());
  }
  auto id = tim.Unsigned("id", UINT32_MAX);
  if (!id) return std::unexpected(id.error());
  auto flags = tim.Unsigned("flags", UINT32_MAX);
  if (!flags) return std::unexpected(flags.error());
  auto image_value = tim.Get("image");
  if (!image_value) return std::unexpected(image_value.error());
  auto image = RectOf(**image_value, tim.Where("image"));
  if (!image) return std::unexpected(image.error());
  TimOrigin origin{.id = static_cast<std::uint32_t>(*id),
                   .flags = static_cast<std::uint32_t>(*flags),
                   .image = *image,
                   .clut = std::nullopt};
  if (const Json* clut_value = tim.Find("clut")) {
    auto clut = RectOf(*clut_value, tim.Where("clut"));
    if (!clut) return std::unexpected(clut.error());
    origin.clut = *clut;
  }
  return origin;
}

std::expected<ImageDependency, std::string> DependencyOf(
    const Json& value, const std::string& where) {
  const JsonMembers entry(value, where);
  if (auto only = entry.Only({"source", "source_sha256", "role"}); !only) {
    return std::unexpected(only.error());
  }
  auto source = entry.String("source");
  if (!source) return std::unexpected(source.error());
  auto hash = entry.String("source_sha256");
  if (!hash) return std::unexpected(hash.error());
  auto role = entry.String("role");
  if (!role) return std::unexpected(role.error());
  return ImageDependency{.source = std::move(*source),
                         .source_sha256 = std::move(*hash),
                         .role = std::move(*role)};
}

std::expected<TexturePagePlacement, std::string> TexturePageOf(
    const Json& value, const std::string& where) {
  const JsonMembers json(value, where);
  if (auto only = json.Only({"effect", "slot", "page", "vram"}); !only) {
    return std::unexpected(only.error());
  }
  auto effect = json.Unsigned("effect", UINT32_MAX);
  if (!effect) return std::unexpected(effect.error());
  auto slot = json.Unsigned("slot", UINT32_MAX);
  if (!slot) return std::unexpected(slot.error());
  auto page = json.Unsigned("page", UINT32_MAX);
  if (!page) return std::unexpected(page.error());
  auto vram_value = json.Get("vram");
  if (!vram_value) return std::unexpected(vram_value.error());
  auto vram = RectOf(**vram_value, json.Where("vram"));
  if (!vram) return std::unexpected(vram.error());
  return TexturePagePlacement{.effect = static_cast<std::uint32_t>(*effect),
                              .slot = static_cast<std::uint32_t>(*slot),
                              .page = static_cast<std::uint32_t>(*page),
                              .vram = *vram};
}

std::expected<PreviewPalette, std::string> PreviewPaletteOf(
    const Json& value, const std::string& where) {
  const JsonMembers json(value, where);
  if (auto only = json.Only({"image", "row", "entry"}); !only) {
    return std::unexpected(only.error());
  }
  auto image = json.String("image");
  if (!image) return std::unexpected(image.error());
  auto row = json.Unsigned("row", UINT32_MAX);
  if (!row) return std::unexpected(row.error());
  auto entry = json.Unsigned("entry", UINT32_MAX);
  if (!entry) return std::unexpected(entry.error());
  return PreviewPalette{.image = std::move(*image),
                        .row = static_cast<std::uint32_t>(*row),
                        .entry = static_cast<std::uint32_t>(*entry)};
}

std::expected<ImageProvenance, std::string> ProvenanceOf(
    const Json& value, const std::string& where) {
  const JsonMembers json(value, where);
  if (auto only =
          json.Only({"source", "source_sha256", "offset", "tim", "texture_page",
                     "preview_palette", "dependencies", "note"});
      !only) {
    return std::unexpected(only.error());
  }
  ImageProvenance provenance;
  auto source = json.String("source");
  if (!source) return std::unexpected(source.error());
  provenance.source = std::move(*source);
  auto hash = json.String("source_sha256");
  if (!hash) return std::unexpected(hash.error());
  provenance.source_sha256 = std::move(*hash);
  auto offset = json.Unsigned("offset", UINT64_MAX);
  if (!offset) return std::unexpected(offset.error());
  provenance.offset = *offset;
  if (const Json* tim = json.Find("tim")) {
    auto origin = TimOriginOf(*tim, json.Where("tim"));
    if (!origin) return std::unexpected(origin.error());
    provenance.tim = *origin;
  }
  if (const Json* page = json.Find("texture_page")) {
    auto placement = TexturePageOf(*page, json.Where("texture_page"));
    if (!placement) return std::unexpected(placement.error());
    provenance.texture_page = *placement;
  }
  if (const Json* preview = json.Find("preview_palette")) {
    auto palette = PreviewPaletteOf(*preview, json.Where("preview_palette"));
    if (!palette) return std::unexpected(palette.error());
    provenance.preview_palette = std::move(*palette);
  }
  if (const Json* dependencies = json.Find("dependencies")) {
    if (!dependencies->is_array()) {
      return std::unexpected(
          std::format("{} must be a list", json.Where("dependencies")));
    }
    for (std::size_t index = 0; index < dependencies->size(); ++index) {
      auto dependency = DependencyOf(
          (*dependencies)[index],
          std::format("{}[{}]", json.Where("dependencies"), index));
      if (!dependency) return std::unexpected(dependency.error());
      provenance.dependencies.push_back(std::move(*dependency));
    }
  }
  if (json.Find("note") != nullptr) {
    auto note = json.String("note");
    if (!note) return std::unexpected(note.error());
    provenance.note = std::move(*note);
  }
  if (auto valid = ValidateImageProvenance(provenance); !valid) {
    return std::unexpected(std::format("{}: {}", where, valid.error()));
  }
  return provenance;
}

bool IsText(const Json& value, std::string_view text) {
  return value.is_string() && value.get_ref<const std::string&>() == text;
}

std::expected<std::vector<bool>, std::string> RowStpOf(
    const Json& value, std::size_t entries, const std::string& where) {
  if (IsText(value, kStpNone)) return std::vector<bool>(entries, false);
  if (IsText(value, kStpAll)) return std::vector<bool>(entries, true);
  if (!value.is_array()) {
    return std::unexpected(std::format(
        "{} must be \"none\", \"all\" or a list of palette indices", where));
  }
  std::vector<bool> stp(entries, false);
  for (const Json& index : value) {
    if (!index.is_number_unsigned() || index.get<std::uint64_t>() >= entries) {
      return std::unexpected(
          std::format("{} lists {}; palette indices run from 0 to {}", where,
                      index.dump(), entries - 1));
    }
    const auto entry = index.get<std::size_t>();
    if (stp[entry]) {
      return std::unexpected(
          std::format("{} lists palette index {} twice", where, entry));
    }
    stp[entry] = true;
  }
  return stp;
}

std::expected<void, std::string> PaletteOf(const JsonMembers& json,
                                           Sidecar& sidecar) {
  auto value = json.Get("palette");
  if (!value) return std::unexpected(value.error());
  const JsonMembers palette(**value, json.Where("palette"));
  if (auto only = palette.Only({"entries", "count", "stp"}); !only) {
    return std::unexpected(only.error());
  }
  auto entries = palette.Unsigned(
      "entries", PaletteCapacity(sidecar.document.image.depth));
  if (!entries) return std::unexpected(entries.error());
  auto count = palette.Unsigned("count", kTimVramHeight);
  if (!count) return std::unexpected(count.error());
  if (*entries == 0 || *count == 0) {
    return std::unexpected(std::format(
        "{} needs at least one palette of one entry", json.Where("palette")));
  }
  sidecar.entries = *entries;
  auto stp = palette.Get("stp");
  if (!stp) return std::unexpected(stp.error());
  if (!(*stp)->is_array() || (*stp)->size() != *count) {
    return std::unexpected(std::format("{} must list one entry per palette",
                                       palette.Where("stp")));
  }
  for (std::size_t row = 0; row < *count; ++row) {
    auto flags = RowStpOf((**stp)[row], sidecar.entries,
                          std::format("{}[{}]", palette.Where("stp"), row));
    if (!flags) return std::unexpected(flags.error());
    sidecar.palette_stp.push_back(std::move(*flags));
  }
  return {};
}

std::expected<void, std::string> ShapeOf(const JsonMembers& json,
                                         Sidecar& sidecar) {
  constexpr std::array kDepths{ImageDepth::kIndexed4, ImageDepth::kIndexed8,
                               ImageDepth::kDirect16};
  auto depth = json.Unsigned("depth", UINT8_MAX);
  if (!depth) return std::unexpected(depth.error());
  const auto found =
      std::ranges::find(kDepths, static_cast<ImageDepth>(*depth));
  if (found == kDepths.end()) {
    return std::unexpected(
        std::format("{} must be 4, 8 or 16", json.Where("depth")));
  }
  auto width = json.Unsigned("width", UINT32_MAX);
  if (!width) return std::unexpected(width.error());
  auto height = json.Unsigned("height", UINT32_MAX);
  if (!height) return std::unexpected(height.error());
  NativeImage& image = sidecar.document.image;
  image.depth = *found;
  image.width = static_cast<std::uint32_t>(*width);
  image.height = static_cast<std::uint32_t>(*height);
  if (auto size = ValidateImageSize(image.depth, image.width, image.height);
      !size) {
    return std::unexpected(
        std::format("{}: {}", json.Where("width"), size.error()));
  }
  if (IsIndexed(image.depth)) {
    if (json.Find("stp") != nullptr) {
      return std::unexpected(std::format(
          "{}: an indexed image keeps STP in palette.stp", json.Where("stp")));
    }
    return PaletteOf(json, sidecar);
  }
  if (json.Find("palette") != nullptr) {
    return std::unexpected(std::format("{}: a 16-bit image has no palette",
                                       json.Where("palette")));
  }
  auto stp = json.String("stp");
  if (!stp) return std::unexpected(stp.error());
  if (*stp != kStpNone && *stp != kStpAll && *stp != kStpMask) {
    return std::unexpected(std::format(
        "{} must be \"none\", \"all\" or \"mask\"", json.Where("stp")));
  }
  sidecar.direct_stp = std::move(*stp);
  return {};
}

std::expected<void, std::string> DetailsOf(const JsonMembers& json,
                                           Sidecar& sidecar) {
  ImageDocument& document = sidecar.document;
  if (const Json* value = json.Find("material")) {
    const JsonMembers material(*value, json.Where("material"));
    if (auto only = material.Only({"zero_word", "semi_transparency"}); !only) {
      return std::unexpected(only.error());
    }
    auto zero = EnumOf<ZeroWord>(material, "zero_word", kZeroWordNames);
    if (!zero) return std::unexpected(zero.error());
    auto semi = EnumOf<SemiTransparency>(material, "semi_transparency",
                                         kSemiTransparencyNames);
    if (!semi) return std::unexpected(semi.error());
    document.material =
        ImageMaterial{.zero_word = *zero, .semi_transparency = *semi};
  }
  if (const Json* value = json.Find("unreferenced")) {
    if (!value->is_boolean()) {
      return std::unexpected(
          std::format("{} must be true or false", json.Where("unreferenced")));
    }
    document.unreferenced = value->get<bool>();
  }
  auto provenance = json.Get("provenance");
  if (!provenance) return std::unexpected(provenance.error());
  auto origin = ProvenanceOf(**provenance, json.Where("provenance"));
  if (!origin) return std::unexpected(origin.error());
  document.provenance = std::move(*origin);
  return {};
}

std::expected<Sidecar, std::string> ParseSidecar(const Bytes& bytes,
                                                 const std::string& name) {
  auto parsed = ParseJsonText(bytes, name);
  if (!parsed) return std::unexpected(parsed.error());
  const Json& json = *parsed;
  const JsonMembers members(json, name);
  if (auto only = members.Only({"schema", "schema_version", "id", "depth",
                                "width", "height", "palette", "stp", "material",
                                "unreferenced", "provenance"});
      !only) {
    return std::unexpected(only.error());
  }
  auto schema = members.String("schema");
  if (!schema) return std::unexpected(schema.error());
  auto version = members.Unsigned("schema_version", UINT32_MAX);
  if (!version) return std::unexpected(version.error());
  if (*schema != kImageSchema || *version != kImageSchemaVersion) {
    return std::unexpected(std::format(
        "{} is schema {} version {}; this build reads {} version {}", name,
        *schema, *version, kImageSchema, kImageSchemaVersion));
  }
  Sidecar sidecar;
  auto id = members.String("id");
  if (!id) return std::unexpected(id.error());
  if (auto valid = CheckId(*id); !valid) {
    return std::unexpected(std::format("{}: {}", name, valid.error()));
  }
  sidecar.document.id = std::move(*id);
  if (auto shape = ShapeOf(members, sidecar); !shape) {
    return std::unexpected(shape.error());
  }
  if (auto details = DetailsOf(members, sidecar); !details) {
    return std::unexpected(details.error());
  }
  return sidecar;
}

// --- Pixels from the files.

std::expected<Bytes, std::string> ReadFile(const ImageFileReader& read,
                                           const std::string& name) {
  std::optional<Bytes> bytes = read(name);
  if (!bytes) return std::unexpected(std::format("{} is missing", name));
  return std::move(*bytes);
}

std::expected<std::uint16_t, std::string> WordOf(ImageColor color, bool stp,
                                                 std::string_view what) {
  const std::optional<std::uint16_t> word = WordOfColor(color, stp);
  if (!word) {
    return std::unexpected(std::format(
        "{} has alpha {}; only 0 (transparent) and 255 (opaque) can be drawn",
        what, color.alpha));
  }
  return *word;
}

std::expected<std::vector<std::uint16_t>, std::string> RowWords(
    std::span<const ImageColor> colors, const std::vector<bool>& stp,
    std::string_view where) {
  std::vector<std::uint16_t> words;
  words.reserve(colors.size());
  for (std::size_t entry = 0; entry < colors.size(); ++entry) {
    auto word = WordOf(colors[entry], stp[entry],
                       std::format("{} entry {}", where, entry));
    if (!word) return std::unexpected(word.error());
    words.push_back(*word);
  }
  return words;
}

std::expected<std::vector<std::vector<std::uint16_t>>, std::string>
PaletteTable(std::string_view stem, const ImageFileReader& read,
             const Sidecar& sidecar,
             const std::vector<std::uint16_t>& default_row) {
  const std::string name = NameOf(stem, kImagePalettesSuffix);
  auto bytes = ReadFile(read, name);
  if (!bytes) return std::unexpected(bytes.error());
  const std::size_t count = sidecar.palette_stp.size();
  auto colors =
      DecodeRgbaPng(*bytes, static_cast<std::uint32_t>(sidecar.entries),
                    static_cast<std::uint32_t>(count), name);
  if (!colors) return std::unexpected(colors.error());
  std::vector<std::vector<std::uint16_t>> rows;
  for (std::size_t row = 0; row < count; ++row) {
    auto words = RowWords(
        std::span(*colors).subspan(row * sidecar.entries, sidecar.entries),
        sidecar.palette_stp[row], std::format("{} row {}", name, row));
    if (!words) return std::unexpected(words.error());
    rows.push_back(std::move(*words));
  }
  const auto [differs, unused] =
      std::ranges::mismatch(rows.front(), default_row);
  if (differs != rows.front().end()) {
    const auto entry = static_cast<std::size_t>(differs - rows.front().begin());
    return std::unexpected(std::format(
        "{} row 0 entry {} is 0x{:04X} but the PNG palette has 0x{:04X}; row "
        "0 must equal the PNG palette",
        name, entry, *differs, default_row[entry]));
  }
  return rows;
}

std::expected<NativeImage, std::string> IndexedPixels(
    std::string_view stem, const ImageFileReader& read,
    const Sidecar& sidecar) {
  const NativeImage& shape = sidecar.document.image;
  const std::string name = NameOf(stem, kImagePngSuffix);
  auto bytes = ReadFile(read, name);
  if (!bytes) return std::unexpected(bytes.error());
  auto png = DecodeIndexedPng(*bytes, shape.width, shape.height, name);
  if (!png) return std::unexpected(png.error());
  if (png->palette.size() != sidecar.entries) {
    return std::unexpected(std::format(
        "{}: the PNG palette has {} entries; the sidecar says {} (keep every "
        "palette entry, used or not)",
        name, png->palette.size(), sidecar.entries));
  }
  auto default_row = RowWords(png->palette, sidecar.palette_stp.front(),
                              std::format("{} palette", name));
  if (!default_row) return std::unexpected(default_row.error());
  NativeImage image{.depth = shape.depth,
                    .width = shape.width,
                    .height = shape.height,
                    .indices = std::move(png->indices),
                    .texels = {},
                    .palettes = {}};
  if (sidecar.palette_stp.size() == 1) {
    image.palettes.push_back(std::move(*default_row));
    return image;
  }
  auto rows = PaletteTable(stem, read, sidecar, *default_row);
  if (!rows) return std::unexpected(rows.error());
  image.palettes = std::move(*rows);
  return image;
}

std::expected<std::vector<bool>, std::string> StpMask(
    std::string_view stem, const ImageFileReader& read,
    const NativeImage& shape) {
  const std::string name = NameOf(stem, kImageStpSuffix);
  auto bytes = ReadFile(read, name);
  if (!bytes) return std::unexpected(bytes.error());
  auto colors = DecodeRgbaPng(*bytes, shape.width, shape.height, name);
  if (!colors) return std::unexpected(colors.error());
  constexpr ImageColor kClear{
      .red = 0, .green = 0, .blue = 0, .alpha = kOpaqueAlpha};
  constexpr ImageColor kSet{.red = kOpaqueAlpha,
                            .green = kOpaqueAlpha,
                            .blue = kOpaqueAlpha,
                            .alpha = kOpaqueAlpha};
  std::vector<bool> mask(colors->size());
  for (std::size_t index = 0; index < colors->size(); ++index) {
    const ImageColor color = (*colors)[index];
    if (color != kClear && color != kSet) {
      return std::unexpected(std::format(
          "{}: pixel ({}, {}) must be black (no STP) or white (STP)", name,
          index % shape.width, index / shape.width));
    }
    mask[index] = color == kSet;
  }
  return mask;
}

std::expected<NativeImage, std::string> DirectPixels(
    std::string_view stem, const ImageFileReader& read,
    const Sidecar& sidecar) {
  const NativeImage& shape = sidecar.document.image;
  const std::string name = NameOf(stem, kImagePngSuffix);
  auto bytes = ReadFile(read, name);
  if (!bytes) return std::unexpected(bytes.error());
  auto colors = DecodeRgbaPng(*bytes, shape.width, shape.height, name);
  if (!colors) return std::unexpected(colors.error());
  std::vector<bool> stp(colors->size(), sidecar.direct_stp == kStpAll);
  if (sidecar.direct_stp == kStpMask) {
    auto mask = StpMask(stem, read, shape);
    if (!mask) return std::unexpected(mask.error());
    stp = std::move(*mask);
  }
  NativeImage image{.depth = shape.depth,
                    .width = shape.width,
                    .height = shape.height,
                    .indices = {},
                    .texels = {},
                    .palettes = {}};
  image.texels.reserve(colors->size());
  for (std::size_t index = 0; index < colors->size(); ++index) {
    auto word = WordOf((*colors)[index], stp[index],
                       std::format("{} pixel ({}, {})", name,
                                   index % shape.width, index / shape.width));
    if (!word) return std::unexpected(word.error());
    image.texels.push_back(*word);
  }
  return image;
}

Bytes TextBytes(std::string_view text) {
  Bytes bytes(text.size());
  std::ranges::transform(text, bytes.begin(), [](char value) {
    return static_cast<std::byte>(value);
  });
  return bytes;
}
}  // namespace

std::string_view ZeroWordName(ZeroWord value) noexcept {
  const auto index = static_cast<std::size_t>(value);
  return index < kZeroWordNames.size() ? kZeroWordNames[index] : "unknown";
}

std::string_view SemiTransparencyName(SemiTransparency value) noexcept {
  const auto index = static_cast<std::size_t>(value);
  return index < kSemiTransparencyNames.size() ? kSemiTransparencyNames[index]
                                               : "unknown";
}

std::expected<void, std::string> ValidateImageProvenance(
    const ImageProvenance& provenance) {
  if (provenance.source.empty() ||
      !IsLowercaseSha256(provenance.source_sha256)) {
    return std::unexpected(
        "provenance needs a source path and its lowercase SHA-256");
  }
  for (const ImageDependency& dependency : provenance.dependencies) {
    if (dependency.source.empty() || dependency.role.empty() ||
        !IsLowercaseSha256(dependency.source_sha256)) {
      return std::unexpected(std::format(
          "dependency '{}' needs a source path, a role and its lowercase "
          "SHA-256",
          dependency.source));
    }
  }
  if (provenance.texture_page) {
    if (auto valid = CheckTexturePage(*provenance.texture_page); !valid) {
      return valid;
    }
  }
  if (provenance.preview_palette &&
      !IsContentId(provenance.preview_palette->image)) {
    return std::unexpected(
        std::format("preview palette image '{}' is not an image id",
                    provenance.preview_palette->image));
  }
  return {};
}

std::expected<ImageDocument, std::string> DecodeImageFiles(
    std::string_view stem, const ImageFileReader& read) {
  if (auto valid = CheckStem(stem); !valid) {
    return std::unexpected(valid.error());
  }
  const std::string sidecar_name = NameOf(stem, kImageSidecarSuffix);
  auto text = ReadFile(read, sidecar_name);
  if (!text) return std::unexpected(text.error());
  auto sidecar = ParseSidecar(*text, sidecar_name);
  if (!sidecar) return std::unexpected(sidecar.error());
  auto pixels = IsIndexed(sidecar->document.image.depth)
                    ? IndexedPixels(stem, read, *sidecar)
                    : DirectPixels(stem, read, *sidecar);
  if (!pixels) return std::unexpected(pixels.error());
  if (auto valid = ValidateImage(*pixels); !valid) {
    return std::unexpected(
        std::format("{}: {}", NameOf(stem, kImagePngSuffix), valid.error()));
  }
  sidecar->document.image = std::move(*pixels);
  return std::move(sidecar->document);
}

std::expected<ImageDocument, std::string> LoadImageFiles(
    const fs::path& directory, std::string_view stem) {
  return DecodeImageFiles(stem, DirectoryFileReader(directory));
}

ImageFileReader DirectoryFileReader(fs::path directory) {
  return [directory = std::move(directory)](
             const std::string& name) -> std::optional<Bytes> {
    std::ifstream input(directory / Platform::Utf8Path(name), std::ios::binary);
    if (!input) return std::nullopt;
    const std::vector<char> data{std::istreambuf_iterator<char>(input),
                                 std::istreambuf_iterator<char>()};
    if (input.bad()) return std::nullopt;
    return TextBytes(std::string_view(data.data(), data.size()));
  };
}

ImageFileReader FileListReader(const std::vector<ImageFile>& files) {
  return [&files](const std::string& name) -> ImageFileBytes {
    const auto found = std::ranges::find(files, name, &ImageFile::name);
    if (found == files.end()) return std::nullopt;
    return found->bytes;
  };
}

}  // namespace VS::Content
