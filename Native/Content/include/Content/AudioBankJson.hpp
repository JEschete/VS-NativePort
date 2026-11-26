#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "Content/AudioBank.hpp"
#include "Content/JsonText.hpp"

// The bank.json of a converted sound bank (ASSET_PIPELINE.md §6.5, #520), in
// schema vs.audio-bank (Native/Content/schemas/audio-bank.schema.json). It
// names what the disc packs into each 16-byte instrument record:
//   root_note, fine_tune   the note that plays the sample at its recorded rate,
//                          and the pitch scale on top of every note
//   envelope               the two SPU ADSR words as curves, a direction, rates
//                          and a sustain level
//   sample_offset,         byte offsets into the bank's ADPCM sample data
//   loop_offset
//   akao3_articulation     only for an AKAO 3.0 record: its pitch table and
//                          libspu modes as the disc held them
// The samples are the WAV and SFZ files beside it (AudioInstrumentPath).
namespace VS::Content {

inline constexpr std::string_view kAudioBankSchema = "vs.audio-bank";
inline constexpr std::uint32_t kAudioBankSchemaVersion = 2;

// The document of `bank`'s header and instruments, without samples. An upper
// ADSR word with bit 13 set has no name (the SPU does not use the bit), so it
// is refused.
[[nodiscard]] std::expected<JsonDocument, std::string> AudioBankJson(
    const AudioBank& bank);
// The bank `json` holds, without samples; errors name the member at fault
// under `where`.
[[nodiscard]] std::expected<AudioBank, std::string> AudioBankOf(
    const JsonDocument& json, std::string_view where);

}  // namespace VS::Content
