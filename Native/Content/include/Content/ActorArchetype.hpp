#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"

// The actor-archetype form of a MAP\*.ZUD enemy bundle (ASSET_PIPELINE.md
// §6.2.5, #200), in schema vs.actor-archetype
// (Native/Content/schemas/actor-archetype.schema.json). It names the parts
// ZudFile.hpp holds instead of copying them:
//   header       the identity bytes, as named numbers
//   body         {"model": id}, a converted character model, or
//                {"file": "<NAME>.gltf"}, a model of its own beside the JSON
//   weapon,      null or {"model": id}, a converted weapon
//   shield
//   common_animation, battle_animation
//                null, {"animation": id}, a converted bank, or
//                {"bones": n, "bank": ...}, a bank of its own as
//                vs.actor-animation writes it (ActorAnimation.hpp)
// An id is a conversion output id (e.g. model.obj.0a, model.obj.01.wep,
// animation.obj.00_bt1), so an edit to that output reaches every archetype
// that names it. The reader checks each value's type and range and names the
// member at fault; the loader checks that each id names an output of the
// right kind.
namespace VS::Content {

inline constexpr std::string_view kActorArchetypeSchema = "vs.actor-archetype";
inline constexpr std::uint32_t kActorArchetypeSchemaVersion = 2;
// The zud-json converter writes each MAP\*.ZUD as <directory>/<ZUD>.json, and
// a body no converted model matches as <directory>/<ZUD>.gltf.
inline constexpr std::string_view kZudArchetypeConverterName = "zud-json";
inline constexpr std::uint32_t kZudArchetypeConverterVersion = 2;
inline constexpr auto kArchetypeDirectory = "models/zud";

// The identity bytes (ZudData, MENU9.PRG); the file's eighth byte is zero.
struct ArchetypeHeader {
  std::uint8_t character_id = 0;
  std::uint8_t weapon = 0;
  std::uint8_t battle_sequence_model = 0;
  std::uint8_t weapon_material = 0;
  std::uint8_t shield = 0;
  std::uint8_t shield_material = 0;
  std::uint8_t default_material = 0;

  bool operator==(const ArchetypeHeader&) const = default;
};

// A part that is another conversion output, named by its id.
struct ArchetypeReference {
  std::string id;

  bool operator==(const ArchetypeReference&) const = default;
};

// A body model of the archetype's own: a glTF file name in its directory.
struct ArchetypeOwnModel {
  std::string file;  // e.g. Z049U06.gltf

  bool operator==(const ArchetypeOwnModel&) const = default;
};

using ArchetypeBody = std::variant<ArchetypeReference, ArchetypeOwnModel>;
// No bank, a converted bank, or a bank of the archetype's own.
using ArchetypeAnimation =  // NOLINTNEXTLINE(whitespace/indent_namespace)
    std::variant<std::monostate, ArchetypeReference, SeqFile>;

// One archetype and where it came from.
struct ActorArchetypeDocument {
  std::string source;  // the disc path, e.g. MAP/Z001U00.ZUD
  std::string source_sha256;
  ArchetypeHeader header;
  ArchetypeBody body;
  std::optional<ArchetypeReference> weapon;
  std::optional<ArchetypeReference> shield;
  ArchetypeAnimation common_animation;
  ArchetypeAnimation battle_animation;

  bool operator==(const ActorArchetypeDocument&) const = default;
};

[[nodiscard]] JsonDocument ActorArchetypeJson(
    const ActorArchetypeDocument& document);
// The archetype `json` holds; errors name the member at fault under `where`.
[[nodiscard]] std::expected<ActorArchetypeDocument, std::string>
ActorArchetypeOf(const JsonDocument& json, std::string_view where);

// The stem of an own model's file name ("Z049U06" for "Z049U06.gltf"), or
// nothing when it is not a plain .gltf name in the archetype's directory.
[[nodiscard]] std::optional<std::string_view> ArchetypeModelStem(
    std::string_view file) noexcept;

}  // namespace VS::Content
