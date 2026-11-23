// The wave-bank converter (AudioBankConversion.hpp).
#include "Extract/AudioBankConversion.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/AudioBank.hpp"
#include "Core/Log.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Content::ConversionManifest;
using Content::ConversionOutput;

constexpr std::string_view kRawForm = "raw";

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

}  // namespace VS::Extract
