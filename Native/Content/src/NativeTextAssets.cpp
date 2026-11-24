#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <utility>

#include "Content/CatalogDatabase.hpp"
#include "Content/ConvertedImages.hpp"
#include "Content/DataStore.hpp"
#include "Content/NativeText.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
std::expected<std::vector<std::byte>, std::string> Read(
    const std::filesystem::path& root, std::string_view path) {
  auto bytes = DirectoryFileReader(root)(std::string(path));
  if (!bytes) {
    return std::unexpected("cannot read native text " + std::string(path));
  }
  return std::move(*bytes);
}

std::expected<void, std::string> Registered(const DataStore& store,
                                            std::string_view id,
                                            std::string_view path,
                                            std::string_view source) {
  const auto* manifest = store.Conversions();
  const auto* output = manifest == nullptr ? nullptr : manifest->Find(id);
  const auto source_id = AssetIdForDiscPath(source);
  if (output == nullptr || output->converter != kSaveTextConverter ||
      output->source != source || !source_id || store.Catalog() == nullptr ||
      !store.Catalog()->Find(*source_id) ||
      std::ranges::find(output->files, path) == output->files.end()) {
    return std::unexpected(std::string(id) +
                           ": missing registered native text output; run "
                           "VS.Extract text <datadir>");
  }
  return {};
}
}  // namespace

std::expected<NativeTextAssets, std::string> LoadSaveTextAssets(
    const DataStore& store) {
  if (auto registered =
          Registered(store, kSaveTextId, kSaveTextPath, "MENU/MCMAN.BIN");
      !registered) {
    return std::unexpected(registered.error());
  }
  if (auto registered =
          Registered(store, kSaveFontId, kSaveFontPath, "TITLE/TITLE.PRG");
      !registered) {
    return std::unexpected(registered.error());
  }
  auto text = Read(store.Root(), kSaveTextPath);
  if (!text) return std::unexpected(text.error());
  auto catalog = ParseNativeStrings(*text);
  if (!catalog) return std::unexpected(catalog.error());
  auto layers = LoadCatalogLayerRoots(store.Root());
  if (!layers) return std::unexpected(layers.error());
  for (const auto& root : *layers) {
    auto layer_bytes = DirectoryFileReader(root)(std::string(kSaveTextPath));
    if (!layer_bytes) {
      std::error_code error;
      if (std::filesystem::exists(root / Platform::Utf8Path(kSaveTextPath),
                                  error) ||
          error) {
        return std::unexpected("unreadable text layer");
      }
      continue;
    }
    auto layer = ParseNativeStrings(*layer_bytes);
    if (!layer) return std::unexpected(layer.error());
    if (layer->font != catalog->font) {
      return std::unexpected("text layer: mismatched font reference");
    }
    catalog->scale = layer->scale;
    for (auto& [id, value] : layer->strings) {
      catalog->strings.insert_or_assign(id, std::move(value));
    }
  }
  auto font_bytes = Read(store.Root(), kSaveFontPath);
  if (!font_bytes) return std::unexpected(font_bytes.error());
  auto image = LoadConvertedImageById(store.Root(), *store.Conversions(),
                                      kSaveFontImageId);
  if (!image) return std::unexpected(image.error());
  auto font = ParseNativeFont(*font_bytes, image->image);
  if (!font) return std::unexpected(font.error());
  if (font->image != image->id || catalog->font != font->id) {
    return std::unexpected("native text: unresolved font/image reference");
  }
  for (const auto& [id, tokens] : catalog->strings) {
    for (const auto& token : tokens) {
      if (token.kind == TextTokenKind::kGlyph &&
          !font->glyphs.contains(token.identity)) {
        return std::unexpected(id + ": missing font glyph " + token.identity);
      }
    }
  }
  return NativeTextAssets{std::move(*catalog), std::move(*font),
                          std::move(*image)};
}
}  // namespace VS::Content
