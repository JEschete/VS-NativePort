#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

#include "Content/ConversionManifest.hpp"
#include "Extract/ConverterBatch.hpp"

// The wave-bank converter (ASSET_PIPELINE.md §6.5, #504): each SOUND\WAVEnnnn
// .DAT instrument bank becomes audio\banks\<bank id>\, holding bank.json (the
// instrument records, Content/AudioBankJson.hpp) and, for each playable
// instrument, its decoded sample as a 16-bit WAV with an SFZ region carrying
// its loop. The unused one-byte zero placeholders have no output.
namespace VS::Extract {

// The manifest entry for `source`'s bank, without its files.
[[nodiscard]] Content::ConversionOutput WaveBankOutput(
    const SourceFile& source);

struct BankConversionReport {
  std::size_t converted = 0;
  std::size_t unchanged = 0;  // source and converter version match
  std::size_t skipped = 0;    // empty placeholders
  std::size_t removed = 0;    // files no current output lists
};

// Converts every sound bank files.tsv lists under `root`, then records them
// in root\manifest.json. An output whose source and converter version are
// unchanged and whose files exist is kept unless `force`; without `force`,
// UpgradeBankAuthoring runs first. Two banks with the same id are refused, as
// they would write the same directory.
[[nodiscard]] std::expected<BankConversionReport, std::string> ConvertDiscBanks(
    const std::filesystem::path& root, bool force);

// The wave-bank converter version whose bank.json numbered the ADSR words and
// kept each instrument's disc record; UpgradeBankAuthoring names them.
inline constexpr std::uint32_t kNumberedAdsrBankVersion = 1;

struct BankAuthoringReport {
  std::size_t banks = 0;    // bank.json files moved to schema vs.audio-bank
  std::size_t written = 0;  // files written, with the manifest
};

// Rewrites each existing bank.json the numbered converter wrote in schema
// vs.audio-bank, without reconverting the banks or dropping their
// display_name labels. A 16-byte disc record is dropped, as its members
// already hold every byte; a 64-byte AKAO 3.0 record keeps its pitch table and
// modes. Each bank is checked to read back with the same instruments; the
// converter version is published last, and a file already current is left as
// it is, so an interrupted run resumes.
[[nodiscard]] std::expected<BankAuthoringReport, std::string>
UpgradeBankAuthoring(const std::filesystem::path& root);

}  // namespace VS::Extract
