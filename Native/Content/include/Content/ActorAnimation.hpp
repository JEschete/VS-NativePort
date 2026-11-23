#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"

// The actor-animation form of a SEQ animation bank (ASSET_PIPELINE.md §6.2.1,
// #205): the exact data each character glTF carries for its banks, in schema
// vs.actor-animation (Native/Content/schemas/actor-animation.schema.json).
// It is SeqFile.hpp's model written as JSON, so it edits as numbers and lists:
//   bank.clips    one object per clip: length, base (null or an own clip),
//                 flags, events (null or a motion element), root, travel (null
//                 or a vector track), rotations (one per bone) and scales (null
//                 or one per bone)
//   bank.map      animation index to clip number, or null
//   bank.motion   the element pool in layout order: a track is an object with
//                 an optional initial key ("vector", "raw_scale" or
//                 "packed_scale", three numbers), its "commands" as one flat
//                 list of frames, x, y, z groups (null keeps that axis's
//                 delta) and "terminated": false when no zero byte ends it;
//                 an event script is {"events": [[frame, op, arguments...]]};
//                 bytes nothing reads are {"unread": "<lowercase hex>"}
// A clip's track reference is the element number (read from its start),
// {"track": n, "command": c} (read from command c) or {"empty": offset}.
// The reader checks each value's type and range and names the member at
// fault; EncodeSeqBank checks the references, flags and event lengths.
namespace VS::Content {

inline constexpr std::string_view kActorAnimationSchema = "vs.actor-animation";
inline constexpr std::uint32_t kActorAnimationSchemaVersion = 1;

// One bank and where it came from.
struct ActorAnimationDocument {
  std::string source;  // the disc path, e.g. OBJ/6F_BT2.SEQ
  std::string source_sha256;
  SeqFile file;

  bool operator==(const ActorAnimationDocument&) const = default;
};

[[nodiscard]] JsonDocument SeqBankJson(const SeqBank& bank);
// The bank `json` holds; errors name the member at fault under `where`.
[[nodiscard]] std::expected<SeqBank, std::string> SeqBankOf(
    const JsonDocument& json, std::string_view where);

// The schema, its version, the source, its hash, the bones and the bank.
[[nodiscard]] JsonDocument ActorAnimationJson(
    const ActorAnimationDocument& document);
[[nodiscard]] std::expected<ActorAnimationDocument, std::string>
ActorAnimationOf(const JsonDocument& json, std::string_view where);

}  // namespace VS::Content
