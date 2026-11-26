#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ZudFile.hpp"

// The actor-archetype form of a MAP\*.ZUD enemy bundle (ASSET_PIPELINE.md
// §6.2.5, #200), in schema vs.actor-archetype
// (Native/Content/schemas/actor-archetype.schema.json). It names the parts
// ZudFile.hpp holds instead of copying them:
//   header       the identity bytes under their meanings: model, blade and
//                shield item ids, weapon category, materials by name and
//                movement class
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
inline constexpr std::uint32_t kActorArchetypeSchemaVersion = 3;
// The zud-json converter writes each MAP\*.ZUD as <directory>/<ZUD>.json, and
// a body no converted model matches as <directory>/<ZUD>.gltf.
inline constexpr std::string_view kZudArchetypeConverterName = "zud-json";
inline constexpr std::uint32_t kZudArchetypeConverterVersion = 4;
inline constexpr auto kArchetypeDirectory = "models/zud";

// The equipped weapon category of an archetype without a blade
// (func_80077240's i = 0xA); 1-9 are the blade's kBladeCategories.
inline constexpr std::uint8_t kBareHandsWeaponCategory = 10;
// A shield's header byte is its item id minus this; 0 is no shield.
inline constexpr int kArchetypeShieldItemOffset = 31;

// The identity bytes (ZudData, MENU9.PRG); the file's eighth byte is zero.
// The encyclopedia preview passes each to vs_battle_populateDataSlot.
struct ArchetypeHeader {
  // The SHP's model id: its sharing key and, for a converted body, the
  // model.obj.<hex> number.
  std::uint8_t model_id = 0;
  std::uint8_t blade_item_id = 0;  // 0 for none
  // The battle SEQ sharing key: 1-9 the blade's category, else
  // kBareHandsWeaponCategory (the <shape>_BT<n> suffix, A for 10).
  std::uint8_t weapon_category = 0;
  std::uint8_t weapon_material = 0;  // kMaterials
  // The shield's item id minus kArchetypeShieldItemOffset; 0 for none.
  std::uint8_t shield = 0;
  std::uint8_t shield_material = 0;  // kMaterials
  // The shape's PlaceActorInRoom movement class (ActorPlacement.hpp); the
  // decomp's defaultMaterial.
  std::uint8_t movement_class = 0;

  bool operator==(const ArchetypeHeader&) const = default;
};

// The header a ZUD's identity bytes give, ignoring the pad byte, and the
// identity bytes a header gives back, with a zero pad.
[[nodiscard]] ArchetypeHeader ArchetypeHeaderOf(
    const std::array<std::uint8_t, kZudIdentityBytes>& identity) noexcept;
[[nodiscard]] std::array<std::uint8_t, kZudIdentityBytes> ZudIdentityOf(
    const ArchetypeHeader& header) noexcept;
// The header member of an archetype document, as ActorArchetypeJson writes
// it.
[[nodiscard]] JsonDocument ArchetypeHeaderJson(const ArchetypeHeader& header);

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
