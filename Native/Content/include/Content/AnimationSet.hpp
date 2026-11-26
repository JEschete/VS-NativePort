#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "Content/EsqFile.hpp"
#include "Content/JsonText.hpp"

// The animation-set form of an ESQ shared animation set (ASSET_PIPELINE.md
// §6.2.2, #206), in schema vs.animation-set
// (Native/Content/schemas/animation-set.schema.json). It is EsqFile.hpp's
// model written as JSON: one object per entry, in the order the game searches
// them, with
//   selector     the model number or model kind the entry is for
//   texture_row  the effect texture row a model selected by its number uses,
//                or null
//   bones, bank  the entry's animation bank, as vs.actor-animation writes it
//                (ActorAnimation.hpp)
// The reader checks each value's type and range and names the member at
// fault, and refuses a selector an earlier entry already has (the game would
// never reach the later one); EncodeEsqFile checks the banks.
namespace VS::Content {

inline constexpr std::string_view kAnimationSetSchema = "vs.animation-set";
inline constexpr std::uint32_t kAnimationSetSchemaVersion = 2;
// The esq-json converter writes each OBJ\E*.ESQ as <directory>/<STEM>.json.
inline constexpr std::string_view kEsqAnimationSetConverterName = "esq-json";
inline constexpr std::uint32_t kEsqAnimationSetConverterVersion = 2;
inline constexpr auto kAnimationSetDirectory = "models/animation-sets";

// One animation set and where it came from.
struct AnimationSetDocument {
  std::string source;  // the disc path, e.g. OBJ/E001.ESQ
  std::string source_sha256;
  EsqFile file;

  bool operator==(const AnimationSetDocument&) const = default;
};

[[nodiscard]] JsonDocument AnimationSetJson(
    const AnimationSetDocument& document);
// The set `json` holds; errors name the member at fault under `where`.
[[nodiscard]] std::expected<AnimationSetDocument, std::string> AnimationSetOf(
    const JsonDocument& json, std::string_view where);

}  // namespace VS::Content
