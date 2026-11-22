#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Extract/ConverterBatch.hpp"

// The shp-gltf converter (ASSET_PIPELINE.md §6.2, #203): each OBJ\<NN>.SHP
// character model becomes a glTF model under models\characters\
// (ShpModelFiles.hpp), recorded in manifest.json. A model that borrows its
// texture records the lender's SHP as a dependency, because the lender's
// texture flag picks the polygon layout and its size scales the UVs.
namespace VS::Extract {

using Content::kShpExtension;

// --- Shared by every model whose glTF holds an ShpModel (SHP, WEP).

// The disc file's stem when its name ends in `extension`
// (OBJ/4F.SHP, .SHP -> 4F).
[[nodiscard]] std::optional<std::string> DiscStemOf(std::string_view disc_path,
                                                    std::string_view extension);

using ModelBytes = std::span<const std::byte>;
// Adds the bytes to the glTF buffer as a buffer view of its own and returns
// its index; nothing for no bytes.
using ModelViewAdder = std::function<std::optional<std::size_t>(ModelBytes)>;

// A material variant (KHR_materials_variants): its name and the image its
// materials show.
struct ModelMaterialVariant {
  std::string_view name;
  std::string_view image_uri;
};

// What a model's glTF is written with.
struct ModelGltfSpec {
  std::string_view stem;       // <stem>.gltf, <stem>.bin and the mesh name
  std::string_view image_uri;  // the texture the materials show
  std::string_view generator;  // asset.generator
  Content::ShpTextureSize uv_scale;  // what the texture coordinates divide by
  // The skeleton members (Content::ShpSkeletonGltf), naming the inverse bind
  // matrices accessor 0.
  Content::JsonDocument skeleton;
  // asset.extras.vs, made after the mesh; it may add buffer views.
  std::function<Content::JsonDocument(const ModelViewAdder&)> payload;
  // The model's looks, if it has more than one: the first shows image_uri
  // with the primitives' own materials, and each other adds a copy of them
  // showing its image.
  std::span<const ModelMaterialVariant> variants;
};

// <stem>.gltf and <stem>.bin: `model` skinned to the spec's skeleton, its
// polygons in one primitive per kShpPrimitiveFlags value, which the
// ShpModelFiles.hpp reader takes back (Content::ReadModelMesh).
[[nodiscard]] std::expected<std::vector<Content::ImageFile>, std::string>
EncodeModelGltf(const Content::ShpModel& model, const ModelGltfSpec& spec);

struct ConvertedModel {
  Content::ConversionOutput output;
  std::vector<Content::ImageFile> files;  // under the converter's directory
};

// `output` with `files`, listing each as <directory>/<name>.
[[nodiscard]] ConvertedModel ConvertedModelOf(
    Content::ConversionOutput output, std::vector<Content::ImageFile> files,
    std::string_view directory);

struct ModelConversionReport {
  std::size_t converted = 0;
  std::size_t unchanged = 0;  // every recorded input and the version match
  std::size_t removed = 0;    // files no current output lists
};

// One model a converter run may convert: its manifest entry without files,
// and how to convert it when the previous output cannot be kept.
struct ModelJob {
  Content::ConversionOutput expected;
  std::function<std::expected<ConvertedModel, std::string>()> convert;
};

struct ModelConverterSpec {
  std::string_view name;
  std::uint32_t version = 0;
  std::string_view directory;  // under the root, e.g. models/characters
  bool force = false;          // convert even unchanged models
};

// Runs one model converter over `jobs`: keeps each previous output that
// matches its job and whose files exist (MatchingOutput) unless `force`,
// converts the rest, writes their files under the root's directory, and
// records the run in manifest.json. Every model converts before anything is
// written.
[[nodiscard]] std::expected<ModelConversionReport, std::string>
RunModelConverter(const std::filesystem::path& root, const RunStart& start,
                  const ModelConverterSpec& spec,
                  std::span<const ModelJob> jobs);

// --- The shp-gltf converter.
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

// One model's files and manifest entry. The SHP must decode strictly, and
// the files must read back to the same document and rebuild the exact
// source bytes.
[[nodiscard]] std::expected<ConvertedModel, std::string> ConvertShpModel(
    const ShpConversion& conversion);

// Converts every SHP files.tsv lists under `root`, then records them in
// root\manifest.json. An output whose source, lender and converter version
// are unchanged and whose files exist is kept as it is (so edits to it
// survive) unless `force`. Every model converts before anything is written.
[[nodiscard]] std::expected<ModelConversionReport, std::string>
ConvertDiscModels(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
