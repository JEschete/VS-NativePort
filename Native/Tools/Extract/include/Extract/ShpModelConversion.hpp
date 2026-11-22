#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Extract/ConverterBatch.hpp"

// The shp-gltf converter (ASSET_PIPELINE.md §6.2, #203): each OBJ\<NN>.SHP
// character model becomes a glTF model under models\characters\
// (ShpModelFiles.hpp), recorded in manifest.json. A model that borrows its
// texture records the lender's SHP as a dependency, because the lender's
// texture flag picks the polygon layout and its size scales the UVs.
namespace VS::Extract {

using Content::kShpExtension;

// model.<directories>.<stem>, lowercase (OBJ/4F.SHP -> model.obj.4f).
[[nodiscard]] std::expected<std::string, std::string> ShpModelIdForDiscPath(
    std::string_view disc_path);
// The disc path of the model whose texture model `lender` is
// (79 -> OBJ/4F.SHP).
[[nodiscard]] std::string ShpLenderPath(std::int32_t lender);

// The files `document` writes as `stem`: <stem>.gltf and <stem>.bin, then its
// own texture's image files. The materials show the texture `image_uri`
// names: <stem>.png, or the lender's PNG for a borrowed texture.
[[nodiscard]] std::expected<std::vector<Content::ImageFile>, std::string>
EncodeShpModelFiles(const Content::ShpModelDocument& document,
                    std::string_view stem, std::string_view image_uri);

struct ShpConversion {
  SourceFile model;
  std::optional<SourceFile> lender;  // needed when the model borrows
};

struct ConvertedModel {
  Content::ConversionOutput output;
  std::vector<Content::ImageFile> files;  // under models/characters
};

// One model's files and manifest entry. The SHP must decode strictly, and
// the files must read back to the same document and rebuild the exact
// source bytes.
[[nodiscard]] std::expected<ConvertedModel, std::string> ConvertShpModel(
    const ShpConversion& conversion);

struct ModelConversionReport {
  std::size_t converted = 0;
  std::size_t unchanged = 0;  // source, lender and version match
  std::size_t removed = 0;    // files no current output lists
};

// Converts every SHP files.tsv lists under `root`, then records them in
// root\manifest.json. An output whose source, lender and converter version
// are unchanged and whose files exist is kept as it is (so edits to it
// survive) unless `force`. Every model converts before anything is written.
[[nodiscard]] std::expected<ModelConversionReport, std::string>
ConvertDiscModels(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
