#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ShpModelConversion.hpp"

// The zud-json converter (ASSET_PIPELINE.md §6.2.5, #200): each MAP\*.ZUD
// enemy bundle becomes a vs.actor-archetype JSON file under models\zud\
// (Content/ActorArchetype.hpp), recorded in manifest.json.
//
// A ZUD copies the OBJ files a room enemy loads: its body SHP, weapon and
// shield WEPs, and common and battle SEQ banks. Each section that is byte for
// byte an OBJ file the earlier converters wrote is named by that output's id,
// so an edit to the model reaches every archetype that names it; a section
// that is no OBJ file is kept in the archetype's own form (a bank in the
// JSON, a body as a glTF model beside it). The output records each OBJ file
// it names, BATTLE.PRG (the event stride table) when it has a bank, and a
// body's texture lender as dependencies.
namespace VS::Extract {

// archetype.<directories>.<stem>, lowercase
// (MAP/Z001U00.ZUD -> archetype.map.z001u00).
[[nodiscard]] std::expected<std::string, std::string> ArchetypeIdForDiscPath(
    std::string_view disc_path);

// An OBJ file a section names: its disc path, hash and conversion output.
struct ZudPart {
  std::string path;
  std::string source_sha256;
  std::string id;
};

struct ZudConversion {
  SourceFile zud;
  // Each section's OBJ file, in ZudSection order; nothing for an empty
  // section or one that is no OBJ file.
  std::vector<std::optional<ZudPart>> parts;
  // The lender of a body that is no OBJ file and borrows its texture.
  std::optional<SourceFile> lender;
  SeqEventTable events;
};

// One archetype's files and manifest entry. The ZUD and every section of its
// own must decode strictly, and the files must read back to the same
// document and rebuild the exact source bytes.
[[nodiscard]] std::expected<ConvertedModel, std::string> ConvertZudArchetype(
    const ZudConversion& conversion);

// Converts every ZUD files.tsv lists under `root`, then records them in
// root\manifest.json. The shp-gltf, wep-gltf and seq-gltf outputs of the OBJ
// files the sections match must already be recorded. An output whose source,
// dependencies and converter version are unchanged and whose files exist is
// kept as it is (so edits to it survive) unless `force`. Every archetype
// converts before anything is written.
[[nodiscard]] std::expected<ModelConversionReport, std::string>
ConvertDiscArchetypes(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
