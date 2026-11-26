// The vs.audio-bank document (AudioBankJson.hpp).
#include "Content/AudioBankJson.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace VS::Content {
namespace {

template <class T>
using Result = std::expected<T, std::string>;

constexpr std::array<std::string_view, 2> kCurves{"linear", "exponential"};
constexpr std::array<std::string_view, 2> kDirections{"increase", "decrease"};
// libspu.h:155-164 SPU_VOICE_DIRECT to SPU_VOICE_EXPDec; N is normal phase and
// R reverse phase.
constexpr std::array<std::string_view, 8> kModes{
    "direct",
    "linear-increase",
    "linear-increase-reverse-phase",
    "linear-decrease",
    "linear-decrease-reverse-phase",
    "exponential-increase",
    "exponential-increase-reverse-phase",
    "exponential-decrease",
};

// One member of the envelope: a field of the lower or upper ADSR word, a
// number from 0 to `max`, or a name in `names` for a one-bit field. The SPU
// layout is VoiceEngine.cpp's DecodeAkaoEnvelope; the two words' members
// cover every bit but the upper word's bit 13.
struct EnvelopeMember {
  std::string_view name;
  bool upper = false;
  int shift = 0;
  std::uint16_t max = 0;
  std::span<const std::string_view> names;
};

constexpr std::uint16_t kUnusedUpperBits = 0x2000;
constexpr std::array<EnvelopeMember, 9> kEnvelope{{
    {"attack_curve", false, 15, 1, kCurves},
    {"attack_rate", false, 8, 127, {}},
    {"decay_rate", false, 4, 15, {}},
    {"sustain_level", false, 0, 15, {}},
    {"sustain_curve", true, 15, 1, kCurves},
    {"sustain_direction", true, 14, 1, kDirections},
    {"sustain_rate", true, 6, 127, {}},
    {"release_curve", true, 5, 1, kCurves},
    {"release_rate", true, 0, 31, {}},
}};

using ModeMember = std::uint8_t AudioLegacyArticulation::*;
constexpr std::array<std::pair<std::string_view, ModeMember>, 3> kModeMembers{{
    {"attack_mode", &AudioLegacyArticulation::attack_mode},
    {"sustain_mode", &AudioLegacyArticulation::sustain_mode},
    {"release_mode", &AudioLegacyArticulation::release_mode},
}};

constexpr std::array<std::string_view, 6> kBankMembers{
    "schema",           "schema_version",     "bank_id",
    "first_instrument", "source_destination", "instruments",
};
constexpr std::array<std::string_view, 7> kInstrumentMembers{
    "available",     "root_note",   "fine_tune",          "envelope",
    "sample_offset", "loop_offset", "akao3_articulation",
};
constexpr std::array<std::string_view, 4> kArticulationMembers{
    "pitch_table",
    "attack_mode",
    "sustain_mode",
    "release_mode",
};

constexpr std::string_view kVersionMember = "version";
constexpr std::uint64_t kWordMax = std::numeric_limits<std::uint32_t>::max();

struct EnvelopeWords {
  std::uint16_t lower = 0;
  std::uint16_t upper = 0;
};

// "a, b or c"
std::string Choices(std::span<const std::string_view> names) {
  std::string text;
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (index != 0) text += index + 1 == names.size() ? " or " : ", ";
    text += names[index];
  }
  return text;
}

// The position of the name `key` holds in `names`.
Result<std::size_t> NameIndex(const JsonMembers& members, std::string_view key,
                              std::span<const std::string_view> names) {
  auto text = members.String(key);
  if (!text) return std::unexpected(text.error());
  const auto found = std::ranges::find(names, *text);
  if (found == names.end()) {
    return std::unexpected(
        std::format("{} must be {}", members.Where(key), Choices(names)));
  }
  return static_cast<std::size_t>(found - names.begin());
}

Result<JsonDocument> EnvelopeJson(EnvelopeWords words, std::string_view where) {
  if ((words.upper & kUnusedUpperBits) != 0) {
    return std::unexpected(std::format(
        "{}: the upper ADSR word {:#06x} sets bit 13, which has no member",
        where, words.upper));
  }
  JsonDocument json = JsonDocument::object();
  for (const EnvelopeMember& member : kEnvelope) {
    const auto value = static_cast<std::uint16_t>(
        ((member.upper ? words.upper : words.lower) >> member.shift) &
        member.max);
    if (member.names.empty()) {
      json[std::string(member.name)] = value;
    } else {
      json[std::string(member.name)] = std::string(member.names[value]);
    }
  }
  return json;
}

Result<EnvelopeWords> EnvelopeOf(const JsonDocument& json,
                                 const std::string& where) {
  const JsonMembers members(json, where);
  std::array<std::string_view, kEnvelope.size()> keys{};
  std::ranges::transform(kEnvelope, keys.begin(), &EnvelopeMember::name);
  if (auto only = members.Only(keys); !only) {
    return std::unexpected(only.error());
  }
  EnvelopeWords words;
  for (const EnvelopeMember& member : kEnvelope) {
    std::int64_t value = 0;
    if (member.names.empty()) {
      auto number = members.Integer(member.name, 0, member.max);
      if (!number) return std::unexpected(number.error());
      value = *number;
    } else {
      auto index = NameIndex(members, member.name, member.names);
      if (!index) return std::unexpected(index.error());
      value = static_cast<std::int64_t>(*index);
    }
    std::uint16_t& word = member.upper ? words.upper : words.lower;
    word = static_cast<std::uint16_t>(word | (value << member.shift));
  }
  return words;
}

Result<JsonDocument> ArticulationJson(
    const AudioLegacyArticulation& articulation, std::string_view where) {
  JsonDocument json = JsonDocument::object();
  json["pitch_table"] = articulation.pitch_table;
  for (const auto& [name, mode] : kModeMembers) {
    const std::uint8_t value = articulation.*mode;
    if (value >= kModes.size()) {
      return std::unexpected(
          std::format("{}: {} {} is not a libspu mode", where, name, value));
    }
    json[std::string(name)] = std::string(kModes.at(value));
  }
  return json;
}

Result<AudioLegacyArticulation> ArticulationOf(const JsonDocument& json,
                                               const std::string& where) {
  const JsonMembers members(json, where);
  if (auto only = members.Only(kArticulationMembers); !only) {
    return std::unexpected(only.error());
  }
  AudioLegacyArticulation articulation;
  auto table = members.Get("pitch_table");
  if (!table) return std::unexpected(table.error());
  const JsonDocument& pitches = **table;
  if (!pitches.is_array() ||
      pitches.size() != articulation.pitch_table.size() ||
      !std::ranges::all_of(pitches, [](const JsonDocument& pitch) {
        return pitch.is_number_unsigned() &&
               pitch.get<std::uint64_t>() <= kWordMax;
      })) {
    return std::unexpected(
        std::format("{} must hold {} whole numbers from 0 to {}",
                    members.Where("pitch_table"),
                    articulation.pitch_table.size(), kWordMax));
  }
  std::ranges::transform(
      pitches, articulation.pitch_table.begin(),
      [](const JsonDocument& pitch) { return pitch.get<std::uint32_t>(); });
  for (const auto& [name, mode] : kModeMembers) {
    auto index = NameIndex(members, name, kModes);
    if (!index) return std::unexpected(index.error());
    articulation.*mode = static_cast<std::uint8_t>(*index);
  }
  return articulation;
}

Result<JsonDocument> InstrumentJson(const AudioInstrument& instrument,
                                    const std::string& where) {
  auto envelope = EnvelopeJson(
      {.lower = instrument.adsr_lower, .upper = instrument.adsr_upper}, where);
  if (!envelope) return std::unexpected(envelope.error());
  JsonDocument json = JsonDocument::object();
  json["available"] = instrument.available;
  json["root_note"] = instrument.root_note;
  json["fine_tune"] = instrument.fine_tune;
  json["envelope"] = std::move(*envelope);
  json["sample_offset"] = instrument.sample_offset;
  json["loop_offset"] = instrument.loop_offset;
  if (instrument.legacy_articulation) {
    auto articulation =
        ArticulationJson(*instrument.legacy_articulation, where);
    if (!articulation) return std::unexpected(articulation.error());
    json["akao3_articulation"] = std::move(*articulation);
  }
  return json;
}

Result<AudioInstrument> InstrumentOf(const JsonDocument& json,
                                     const std::string& where) {
  const JsonMembers members(json, where);
  if (auto only = members.Only(kInstrumentMembers); !only) {
    return std::unexpected(only.error());
  }
  AudioInstrument instrument;
  auto available = members.Boolean("available");
  if (!available) return std::unexpected(available.error());
  instrument.available = *available;
  constexpr std::int64_t kNoteMin = std::numeric_limits<std::int16_t>::min();
  constexpr std::int64_t kNoteMax = std::numeric_limits<std::int16_t>::max();
  auto root = members.Integer("root_note", kNoteMin, kNoteMax);
  if (!root) return std::unexpected(root.error());
  instrument.root_note = static_cast<std::int16_t>(*root);
  auto tune = members.Integer("fine_tune", kNoteMin, kNoteMax);
  if (!tune) return std::unexpected(tune.error());
  instrument.fine_tune = static_cast<std::int16_t>(*tune);
  auto envelope = members.Get("envelope");
  if (!envelope) return std::unexpected(envelope.error());
  auto words = EnvelopeOf(**envelope, members.Where("envelope"));
  if (!words) return std::unexpected(words.error());
  instrument.adsr_lower = words->lower;
  instrument.adsr_upper = words->upper;
  auto sample = members.Unsigned("sample_offset", kWordMax);
  if (!sample) return std::unexpected(sample.error());
  instrument.sample_offset = static_cast<std::uint32_t>(*sample);
  auto loop = members.Unsigned("loop_offset", kWordMax);
  if (!loop) return std::unexpected(loop.error());
  instrument.loop_offset = static_cast<std::uint32_t>(*loop);
  if (const JsonDocument* articulation = members.Find("akao3_articulation")) {
    auto read =
        ArticulationOf(*articulation, members.Where("akao3_articulation"));
    if (!read) return std::unexpected(read.error());
    instrument.legacy_articulation = *read;
  }
  return instrument;
}

}  // namespace

std::expected<JsonDocument, std::string> AudioBankJson(const AudioBank& bank) {
  JsonDocument json = JsonDocument::object();
  json["schema"] = std::string(kAudioBankSchema);
  json["schema_version"] = kAudioBankSchemaVersion;
  json["bank_id"] = bank.id;
  json["first_instrument"] = bank.first_instrument;
  json["source_destination"] = bank.source_destination;
  JsonDocument instruments = JsonDocument::array();
  for (std::size_t index = 0; index < bank.instruments.size(); ++index) {
    auto instrument =
        InstrumentJson(bank.instruments[index],
                       std::format("bank {} instrument {}", bank.id, index));
    if (!instrument) return std::unexpected(instrument.error());
    instruments.push_back(std::move(*instrument));
  }
  json["instruments"] = std::move(instruments);
  return json;
}

std::expected<AudioBank, std::string> AudioBankOf(const JsonDocument& json,
                                                  std::string_view where) {
  const JsonMembers members(json, std::string(where));
  if (members.Find(kVersionMember) != nullptr &&
      members.Find("schema") == nullptr) {
    return std::unexpected(
        std::format("{} is a version 1 sound bank; run VS.Extract authoring "
                    "<datadir> to name its envelopes",
                    where));
  }
  if (auto only = members.Only(kBankMembers); !only) {
    return std::unexpected(only.error());
  }
  if (auto checked = CheckJsonSchema(members, where, kAudioBankSchema,
                                     kAudioBankSchemaVersion);
      !checked) {
    return std::unexpected(checked.error());
  }
  AudioBank bank;
  auto id = members.Unsigned("bank_id", kWordMax);
  if (!id) return std::unexpected(id.error());
  bank.id = static_cast<std::uint32_t>(*id);
  auto first = members.Unsigned("first_instrument", kAudioInstrumentSlots - 1);
  if (!first) return std::unexpected(first.error());
  bank.first_instrument = static_cast<std::uint32_t>(*first);
  auto destination = members.Unsigned("source_destination", kWordMax);
  if (!destination) return std::unexpected(destination.error());
  bank.source_destination = static_cast<std::uint32_t>(*destination);
  auto instruments = members.Get("instruments");
  if (!instruments) return std::unexpected(instruments.error());
  const JsonDocument& entries = **instruments;
  const std::size_t slots = kAudioInstrumentSlots - bank.first_instrument;
  if (!entries.is_array() || entries.empty() || entries.size() > slots) {
    return std::unexpected(
        std::format("{} must hold 1 to {} instruments from first_instrument",
                    members.Where("instruments"), slots));
  }
  for (std::size_t index = 0; index < entries.size(); ++index) {
    auto instrument = InstrumentOf(
        entries[index],
        std::format("{}[{}]", members.Where("instruments"), index));
    if (!instrument) return std::unexpected(instrument.error());
    bank.instruments.push_back(std::move(*instrument));
  }
  return bank;
}

}  // namespace VS::Content
