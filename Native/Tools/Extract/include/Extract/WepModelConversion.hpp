#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ImageFiles.hpp"
#include "Content/WepModelFiles.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ShpModelConversion.hpp"

// The wep-gltf converter (ASSET_PIPELINE.md §6.2.4, #204): each OBJ\<NN>.WEP
// weapon model becomes a glTF model under models\weapons\
// (WepModelFiles.hpp), recorded in manifest.json.
namespace VS::Extract {

// model.<directories>.<stem>.wep, lowercase (OBJ/01.WEP -> model.obj.01.wep).
[[nodiscard]] std::expected<std::string, std::string> WepModelIdForDiscPath(
    std::string_view disc_path);
// image.<directories>.<stem>.wep (OBJ/01.WEP -> image.obj.01.wep).
[[nodiscard]] std::expected<std::string, std::string> WepImageIdForDiscPath(
    std::string_view disc_path);

// The files `document` writes as `stem`: <stem>.gltf and <stem>.bin, then its
// texture's image files.
[[nodiscard]] std::expected<std::vector<Content::ImageFile>, std::string>
EncodeWepModelFiles(const Content::WepModelDocument& document,
                    std::string_view stem);

// One weapon's files and manifest entry. The WEP must decode strictly, and
// the files must read back to the same document and rebuild the exact source
// bytes.
[[nodiscard]] std::expected<ConvertedModel, std::string> ConvertWepModel(
    const SourceFile& source);

// Converts every WEP files.tsv lists under `root`, then records them in
// root\manifest.json. An output whose source and converter version are
// unchanged and whose files exist is kept as it is (so edits to it survive)
// unless `force`. Every weapon converts before anything is written.
[[nodiscard]] std::expected<ModelConversionReport, std::string>
ConvertDiscWeapons(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
