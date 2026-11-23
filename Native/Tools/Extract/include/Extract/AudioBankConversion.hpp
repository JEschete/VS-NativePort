#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>

#include "Content/ConversionManifest.hpp"
#include "Extract/ConverterBatch.hpp"

// The wave-bank converter (ASSET_PIPELINE.md §6.5, #504): each SOUND\WAVEnnnn
// .DAT instrument bank becomes audio\banks\<bank id>\, holding bank.json (the
// instrument records) and, for each playable instrument, its decoded sample
// as a 16-bit WAV with an SFZ region carrying its loop. The unused one-byte
// zero placeholders have no output.
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
// unchanged and whose files exist is kept unless `force`. Two banks with the
// same id are refused, as they would write the same directory.
[[nodiscard]] std::expected<BankConversionReport, std::string> ConvertDiscBanks(
    const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
