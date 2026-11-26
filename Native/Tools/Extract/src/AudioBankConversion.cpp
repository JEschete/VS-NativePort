// The wave-bank converter (AudioBankConversion.hpp).
#include "Extract/AudioBankConversion.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/AudioBank.hpp"
#include "Content/AudioBankJson.hpp"
#include "Content/JsonText.hpp"
#include "Core/Log.hpp"
#include "Extract/AnimationAuthoring.hpp"
#include "Extract/ConversionManifestWriter.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Content::ConversionManifest;
using Content::ConversionOutput;
using Json = Content::JsonDocument;
template <class T>
using Result = std::expected<T, std::string>;

constexpr std::string_view kRawForm = "raw";
constexpr std::string_view kJsonSuffix = ".json";

// No bytes, or a single zero byte: an unused bank slot.
bool IsPlaceholder(std::span<const std::byte> bytes) {
  return bytes.empty() || (bytes.size() == 1 && bytes[0] == std::byte{0});
}

// SOUND/WAVE0005.DAT -> audio.bank.wave0005
std::string OutputIdOf(std::string_view path) {
  std::string_view stem = path.substr(path.rfind('/') + 1);
  stem = stem.substr(0, stem.rfind('.'));
  std::string id = "audio.bank.";
  for (const char letter : stem) {
    id.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
  }
  return id;
}

// Records that `output` writes its files, refusing a file another output of
// this run already writes.
std::expected<void, std::string> ClaimFiles(
    std::map<std::string, std::string, std::less<>>& owners,
    const std::vector<std::string>& files, const std::string& source) {
  for (const std::string& file : files) {
    const auto [owner, added] = owners.emplace(file, source);
    if (!added) {
      return std::unexpected(
          std::format("{} and {} both write {}", owner->second, source, file));
    }
  }
  return {};
}

// The members of a bank.json the numbered converter wrote.
constexpr std::string_view kNumberedVersion = "version";
constexpr std::array<std::string_view, 5> kNumberedBank{
    kNumberedVersion,     "bank_id",     "first_instrument",
    "source_destination", "instruments",
};
constexpr std::array<std::string_view, 8> kNumberedInstrument{
    "sample_offset", "loop_offset", "fine_tune", "root_note",
    "adsr_lower",    "adsr_upper",  "available", "source_record",
};
constexpr std::size_t kRecordBytes = 16;
constexpr std::int64_t kHalfMin = std::numeric_limits<std::int16_t>::min();
constexpr std::int64_t kHalfMax = std::numeric_limits<std::int16_t>::max();
constexpr std::uint64_t kByteMax = std::numeric_limits<std::uint8_t>::max();
constexpr std::uint64_t kAdsrMax = std::numeric_limits<std::uint16_t>::max();
constexpr std::uint64_t kWordMax = std::numeric_limits<std::uint32_t>::max();

// The AKAO 3.0 articulation a numbered instrument's disc record holds; none
// for a 16-byte record, whose bytes the other members hold.
Result<std::optional<Content::AudioLegacyArticulation>> RecordArticulation(
    const Content::JsonMembers& members) {
  auto found = members.Get("source_record");
  if (!found) return std::unexpected(found.error());
  const Json& record = **found;
  const bool sized = record.is_array() &&
                     (record.size() == kRecordBytes ||
                      record.size() == Content::kAudioLegacyInstrumentBytes);
  if (!sized || !std::ranges::all_of(record, [](const Json& byte) {
        return byte.is_number_unsigned() &&
               byte.get<std::uint64_t>() <= kByteMax;
      })) {
    return std::unexpected(std::format(
        "{} must hold {} or {} bytes", members.Where("source_record"),
        kRecordBytes, Content::kAudioLegacyInstrumentBytes));
  }
  if (record.size() == kRecordBytes) return std::nullopt;
  std::array<u8, Content::kAudioLegacyInstrumentBytes> bytes{};
  std::ranges::transform(record, bytes.begin(), [](const Json& byte) {
    return byte.get<std::uint8_t>();
  });
  return Content::AudioLegacyArticulationOf(bytes);
}

Result<Content::AudioInstrument> NumberedInstrumentOf(
    const Json& json, const std::string& where) {
  const Content::JsonMembers members(json, where);
  if (auto only = members.Only(kNumberedInstrument); !only) {
    return std::unexpected(only.error());
  }
  Content::AudioInstrument instrument;
  auto sample = members.Unsigned("sample_offset", kWordMax);
  if (!sample) return std::unexpected(sample.error());
  instrument.sample_offset = static_cast<u32>(*sample);
  auto loop = members.Unsigned("loop_offset", kWordMax);
  if (!loop) return std::unexpected(loop.error());
  instrument.loop_offset = static_cast<u32>(*loop);
  auto tune = members.Integer("fine_tune", kHalfMin, kHalfMax);
  if (!tune) return std::unexpected(tune.error());
  instrument.fine_tune = static_cast<i16>(*tune);
  auto root = members.Integer("root_note", kHalfMin, kHalfMax);
  if (!root) return std::unexpected(root.error());
  instrument.root_note = static_cast<i16>(*root);
  auto lower = members.Unsigned("adsr_lower", kAdsrMax);
  if (!lower) return std::unexpected(lower.error());
  instrument.adsr_lower = static_cast<u16>(*lower);
  auto upper = members.Unsigned("adsr_upper", kAdsrMax);
  if (!upper) return std::unexpected(upper.error());
  instrument.adsr_upper = static_cast<u16>(*upper);
  auto available = members.Boolean("available");
  if (!available) return std::unexpected(available.error());
  instrument.available = *available;
  auto articulation = RecordArticulation(members);
  if (!articulation) return std::unexpected(articulation.error());
  instrument.legacy_articulation = *articulation;
  return instrument;
}

Result<Content::AudioBank> NumberedBankOf(const Json& json,
                                          const std::string& where) {
  const Content::JsonMembers members(json, where);
  if (auto only = members.Only(kNumberedBank); !only) {
    return std::unexpected(only.error());
  }
  Content::AudioBank bank;
  auto id = members.Unsigned("bank_id", kWordMax);
  if (!id) return std::unexpected(id.error());
  bank.id = static_cast<u32>(*id);
  auto first =
      members.Unsigned("first_instrument", Content::kAudioInstrumentSlots - 1);
  if (!first) return std::unexpected(first.error());
  bank.first_instrument = static_cast<u32>(*first);
  auto destination = members.Unsigned("source_destination", kWordMax);
  if (!destination) return std::unexpected(destination.error());
  bank.source_destination = static_cast<u32>(*destination);
  auto found = members.Get("instruments");
  if (!found) return std::unexpected(found.error());
  const Json& instruments = **found;
  if (!instruments.is_array()) {
    return std::unexpected(
        std::format("{} must be an array", members.Where("instruments")));
  }
  for (std::size_t index = 0; index < instruments.size(); ++index) {
    auto instrument = NumberedInstrumentOf(
        instruments[index],
        std::format("{}[{}]", members.Where("instruments"), index));
    if (!instrument) return std::unexpected(instrument.error());
    bank.instruments.push_back(std::move(*instrument));
  }
  return bank;
}

// `named`, led by the display_name label `numbered` carries.
Json Labeled(const Json& numbered, Json named) {
  const auto label = numbered.find(std::string(Content::kDisplayName));
  if (label == numbered.end()) return named;
  Json labeled = Json::object();
  labeled[std::string(Content::kDisplayName)] = *label;
  for (auto member = named.begin(); member != named.end(); ++member) {
    labeled[member.key()] = std::move(member.value());
  }
  return labeled;
}

// Moves the bank `json` holds to schema vs.audio-bank when the numbered
// converter wrote it; whether it did. A current bank is left as it is.
Result<bool> NameBank(Json& json, const std::string& where) {
  if (!json.is_object()) {
    return std::unexpected(std::format("{} must be an object", where));
  }
  if (json.contains("schema")) {
    if (auto current = Content::AudioBankOf(json, where); !current) {
      return std::unexpected(current.error());
    }
    return false;
  }
  const auto version = json.find(std::string(kNumberedVersion));
  if (version == json.end() ||
      Content::JsonWholeNumber(*version) != kNumberedAdsrBankVersion) {
    return std::unexpected(
        std::format("{}.version must be {} for a numbered sound bank", where,
                    kNumberedAdsrBankVersion));
  }
  auto bank = NumberedBankOf(json, where);
  if (!bank) return std::unexpected(bank.error());
  auto named = Content::AudioBankJson(*bank);
  if (!named) {
    return std::unexpected(std::format("{}: {}", where, named.error()));
  }
  Json labeled = Labeled(json, std::move(*named));
  const Json& numbered = json.at("instruments");
  Json& instruments = labeled.at("instruments");
  for (std::size_t index = 0; index < instruments.size(); ++index) {
    instruments[index] =
        Labeled(numbered.at(index), std::move(instruments[index]));
  }
  auto read = Content::AudioBankOf(labeled, where);
  if (!read) return std::unexpected(read.error());
  if (read->id != bank->id ||
      read->first_instrument != bank->first_instrument ||
      read->source_destination != bank->source_destination ||
      read->instruments != bank->instruments) {
    return std::unexpected(
        std::format("{} does not read back from its named form", where));
  }
  json = std::move(labeled);
  return true;
}

// The bank files of the wave-bank outputs when the numbered converter wrote
// them, moving it to its current version; whether it moved.
bool PlanBankFiles(ConversionManifest& manifest, std::set<std::string>& files) {
  const auto found = manifest.converters.find(Content::kWaveBankConverterName);
  if (found == manifest.converters.end() ||
      found->second != kNumberedAdsrBankVersion) {
    return false;
  }
  found->second = Content::kWaveBankConverterVersion;
  for (const ConversionOutput& output : manifest.outputs) {
    if (output.converter != Content::kWaveBankConverterName) continue;
    for (const std::string& file : output.files) {
      if (file.ends_with(kJsonSuffix)) files.insert(file);
    }
  }
  return true;
}

}  // namespace

ConversionOutput WaveBankOutput(const SourceFile& source) {
  return {.id = OutputIdOf(source.path),
          .source = source.path,
          .source_sha256 = HashOf(source),
          .converter = std::string(Content::kWaveBankConverterName),
          .files = {},
          .dependencies = {},
          .unreferenced = false,
          .note = {},
          .alias_of = {}};
}

std::expected<BankConversionReport, std::string> ConvertDiscBanks(
    const fs::path& root, bool force) {
  // Existing outputs move to the named envelope form instead of reconverting.
  if (!force) {
    if (auto upgraded = UpgradeBankAuthoring(root); !upgraded) {
      return std::unexpected(upgraded.error());
    }
  }
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  const ConversionManifest& previous = start->previous;
  ConverterBatch batch = StartBatch(previous, Content::kWaveBankConverterName,
                                    Content::kWaveBankConverterVersion, force);
  BankConversionReport report;
  std::map<std::string, std::string, std::less<>> owners;
  for (const Content::DataStore::Entry& entry : start->entries) {
    if (entry.form != kRawForm ||
        !entry.path.starts_with(Content::kWaveBankPrefix) ||
        !entry.path.ends_with(Content::kWaveBankExtension)) {
      continue;
    }
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    if (IsPlaceholder(source->bytes)) {
      ++report.skipped;
      continue;
    }
    ConversionOutput output = WaveBankOutput(*source);
    const ConversionOutput* kept =
        batch.reuse ? MatchingOutput(previous, root, output) : nullptr;
    if (kept != nullptr) {
      if (auto claimed = ClaimFiles(owners, kept->files, entry.path);
          !claimed) {
        return std::unexpected(claimed.error());
      }
      batch.outputs.push_back(*kept);
      ++report.unchanged;
      continue;
    }
    std::vector<u8> bytes(source->bytes.size());
    std::ranges::transform(source->bytes, bytes.begin(), [](std::byte value) {
      return std::to_integer<std::uint8_t>(value);
    });
    auto bank = Content::DecodeAudioBank(bytes);
    if (!bank) {
      return std::unexpected(std::format("{}: {}", entry.path, bank.error()));
    }
    output.files = Content::AudioBankFiles(*bank);
    if (auto claimed = ClaimFiles(owners, output.files, entry.path); !claimed) {
      return std::unexpected(claimed.error());
    }
    // Written as each bank decodes, so only one bank's samples are held.
    if (auto exported = Content::ExportAudioBank(*bank, root); !exported) {
      return std::unexpected(
          std::format("{}: {}", entry.path, exported.error()));
    }
    for (usize index = 0; index < bank->instruments.size(); ++index) {
      if (!bank->instruments[index].available) {
        Core::LogInfo("extract",
                      "{}: instrument {} has no playable sample or pitch; "
                      "recorded as unavailable",
                      entry.path, index);
      }
    }
    batch.outputs.push_back(std::move(output));
    ++report.converted;
  }
  auto removed =
      PublishBatch(root, previous, batch,
                   []() -> std::expected<void, std::string> { return {}; });
  if (!removed) return std::unexpected(removed.error());
  report.removed = *removed;
  return report;
}

std::expected<BankAuthoringReport, std::string> UpgradeBankAuthoring(
    const fs::path& root) {
  auto manifest = Content::LoadConversionManifest(root);
  if (!manifest) return std::unexpected(manifest.error());
  BankAuthoringReport report;
  if (!*manifest) return report;
  std::set<std::string> files;
  if (!PlanBankFiles(**manifest, files)) return report;
  for (const std::string& file : files) {
    auto written = EditJsonOutput(
        root, file, [&file](Json& json) { return NameBank(json, file); });
    if (!written) return std::unexpected(written.error());
    if (*written) {
      ++report.banks;
      ++report.written;
    }
  }
  if (auto written = WriteConversionManifest(root, **manifest); !written) {
    return std::unexpected(written.error());
  }
  ++report.written;
  return report;
}

}  // namespace VS::Extract
