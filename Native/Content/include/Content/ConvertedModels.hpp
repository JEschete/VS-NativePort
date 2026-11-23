#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "Content/DataStore.hpp"

// Converted character and weapon models the runtime loads in place of
// OBJ\<NN>.SHP and OBJ\<NN>.WEP (ASSET_PIPELINE.md §6.2-6.3, #203, #204), the
// OBJ\*.SEQ animation banks their glTF files hold (§6.2.1, #205), and the
// OBJ\E*.ESQ shared animation sets (§6.2.2, #206).
// Loading never converts; a missing or mismatched output is an error naming
// the file.
namespace VS::Content {

// What a read of a model file names: its first sector and its size rounded
// up to whole sectors, as the disc read requested it.
struct ConvertedModelRead {
  std::uint32_t lba = 0;
  std::uint64_t rounded_size = 0;
  // The game's event stride table (SeqFile.hpp kSeqEventLengthsAt), which
  // reads character models' banks; weapons have none.
  std::span<const std::uint8_t> event_lengths;
};

// The SHP bytes rebuilt from the shp-gltf output of the file `read` names,
// checked against the manifest: the model's id, source path and hash.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
LoadConvertedModel(const DataStore& store, const ConvertedModelRead& read);

// The WEP bytes rebuilt from the wep-gltf output of the file `read` names,
// checked against the manifest as LoadConvertedModel checks a model.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
LoadConvertedWeapon(const DataStore& store, const ConvertedModelRead& read);

// The SEQ bytes rebuilt from the glTF of the model that owns the bank `read`
// names: its seq-gltf output names the owner's shp-gltf output, which must be
// current with the hash the bank output records.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
LoadConvertedSequence(const DataStore& store, const ConvertedModelRead& read);

// The ESQ bytes rebuilt from the esq-json output of the set `read` names (its
// one JSON file), checked against the manifest: the set's source path and
// hash.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
LoadConvertedAnimationSet(const DataStore& store,
                          const ConvertedModelRead& read);

}  // namespace VS::Content
