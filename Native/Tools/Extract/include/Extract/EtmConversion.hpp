#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "Extract/ShpModelConversion.hpp"

// The etm-png converter (ASSET_PIPELINE.md §6.2.3, #207): each OBJ\E*.ETM
// effect texture sheet becomes one indexed PNG with an .image.json sidecar
// per model that draws from it, under textures\actors\<sheet>\<model>.png
// (Content/EtmFile.hpp), recorded in manifest.json.
//
// The game loads a sheet by the set number its model uses for the paired
// OBJ\E*.ESQ set (BATTLE.PRG's sheet and set file tables), and each set entry
// with a texture row gives that model number its region of the sheet, from
// that line to the next model's. A region converts at its model's texture
// depth: its own texture's, or 8 bits for a model that borrows a texture.
// Its PNG shows the palette of the model that owns the texture only for
// previewing; the game draws with the model's palette. The converter checks
// that every sheet is one the tables name with a set, and that its regions
// cover the sheet. The output records BATTLE.PRG, the set and the SHP files
// the depth and preview palettes come from as dependencies.
namespace VS::Extract {

// actor-texture.<directories>.<stem>, lowercase
// (OBJ/E001.ETM -> actor-texture.obj.e001).
[[nodiscard]] std::expected<std::string, std::string> ActorTextureIdForDiscPath(
    std::string_view disc_path);

// Converts every ETM files.tsv lists under `root`, then records them in
// root\manifest.json. Every region's image files must read back to the same
// image, and the regions must rebuild the exact sheet. An output whose
// source, dependencies and converter version are unchanged and whose files
// exist is kept as it is (so edits to it survive) unless `force`. Every
// sheet converts before anything is written.
[[nodiscard]] std::expected<ModelConversionReport, std::string>
ConvertDiscActorTextures(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
