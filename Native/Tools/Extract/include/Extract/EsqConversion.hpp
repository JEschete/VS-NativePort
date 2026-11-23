#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ShpModelConversion.hpp"

// The esq-json converter (ASSET_PIPELINE.md §6.2.2, #206): each OBJ\E*.ESQ
// shared animation set becomes a vs.animation-set JSON file under
// models\animation-sets\ (Content/AnimationSet.hpp), recorded in
// manifest.json.
//
// The game loads a set by its number through BATTLE.PRG's set file tables
// and binds each actor to the first entry whose selector is its model's kind
// (BATTLE.PRG's kind table) or its model number. The converter checks that
// every set is one the tables name, and that every entry binds a model
// files.tsv lists with the bones the entry animates. The output records
// BATTLE.PRG (the tables and the event stride table) and the models each set
// binds as dependencies.
namespace VS::Extract {

// animation-set.<directories>.<stem>, lowercase
// (OBJ/E001.ESQ -> animation-set.obj.e001).
[[nodiscard]] std::expected<std::string, std::string> AnimationSetIdForDiscPath(
    std::string_view disc_path);

struct EsqConversion {
  SourceFile set;
  SeqEventTable events;
  // BATTLE.PRG and the models the set binds, sorted by source.
  std::vector<Content::SourceDependency> dependencies;
};

// One set's JSON file and manifest entry. The ESQ must decode strictly, and
// the file must read back to the same document and rebuild the exact source
// bytes.
[[nodiscard]] std::expected<ConvertedModel, std::string> ConvertEsqAnimationSet(
    const EsqConversion& conversion);

// Converts every ESQ files.tsv lists under `root`, then records them in
// root\manifest.json. An output whose source, dependencies and converter
// version are unchanged and whose file exists is kept as it is (so edits to
// it survive) unless `force`. Every set converts before anything is written.
[[nodiscard]] std::expected<ModelConversionReport, std::string>
ConvertDiscAnimationSets(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
