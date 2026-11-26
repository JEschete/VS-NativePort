#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"

// What every converter VS.Extract runs shares: reading an extracted disc file,
// keeping outputs whose inputs are unchanged, writing output files in place
// of the old ones, and recording a run in manifest.json.
namespace VS::Extract {
namespace Content = VS::Content;

struct SourceFile {
  std::string path;  // disc path, '/'-separated
  std::vector<std::byte> bytes;
};

[[nodiscard]] std::expected<std::vector<std::byte>, std::string> ReadFileBytes(
    const std::filesystem::path& path);
// The extracted disc file root\disc\<path>.
[[nodiscard]] std::expected<SourceFile, std::string> ReadSource(
    const std::filesystem::path& root, std::string_view path);
// Its bytes' SHA-256, as manifest.json records it.
[[nodiscard]] std::string HashOf(const SourceFile& source);

// `output` when every file it lists exists under `root`, else nullptr.
[[nodiscard]] const Content::ConversionOutput* PresentOutput(
    const std::filesystem::path& root, const Content::ConversionOutput* output);

// The manifest's output with `expected`'s id when it records everything
// `expected` does but the files (`expected` lists none), and its files exist
// under `root`; else nullptr.
[[nodiscard]] const Content::ConversionOutput* MatchingOutput(
    const Content::ConversionManifest& manifest,
    const std::filesystem::path& root,
    const Content::ConversionOutput& expected);

// One converter's outputs in a run, kept unchanged or newly converted.
struct ConverterBatch {
  std::string_view name;
  std::uint32_t version = 0;
  bool reuse = false;  // the previous outputs are from this version
  std::vector<Content::ConversionOutput> outputs;
};

[[nodiscard]] ConverterBatch StartBatch(
    const Content::ConversionManifest& previous, std::string_view name,
    std::uint32_t version, bool force);

// Refuses to convert `output` over files that another converter lists, files
// that exist but this converter did not write, and (unless `force`) authored
// files whose recorded source hash differs from `output`'s.
[[nodiscard]] std::expected<void, std::string> CheckAuthoringOwnership(
    const Content::ConversionManifest& previous,
    const std::filesystem::path& root, const Content::ConversionOutput& output,
    bool force);

// What a conversion run starts from: the extracted files files.tsv lists and
// the previous manifest.json (empty before the first run).
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct RunStart {
  std::vector<Content::DataStore::Entry> entries;
  Content::ConversionManifest previous;
};

[[nodiscard]] std::expected<RunStart, std::string> StartRun(
    const std::filesystem::path& root);

// Records `batches` in place of their converters' previous outputs, writes
// manifest.json, then removes the files those converters' previous outputs
// listed that no output in the new manifest does, and returns how many it
// removed. A file that several outputs share stays while any of them lists it.
// Removal comes last, so a failed removal leaves only unlisted files.
[[nodiscard]] std::expected<std::size_t, std::string> PublishBatches(
    const std::filesystem::path& root,
    const Content::ConversionManifest& previous,
    std::span<const ConverterBatch> batches);

// A single converter's run: writes its converted files with `write`, then
// records `batch` (PublishBatches), and returns how many stale files it
// removed. Nothing is recorded when a write fails.
[[nodiscard]] std::expected<std::size_t, std::string> PublishBatch(
    const std::filesystem::path& root,
    const Content::ConversionManifest& previous, const ConverterBatch& batch,
    const std::function<std::expected<void, std::string>()>& write);

// An authored-table converter's last step: refuses to convert `output` over
// files it does not own (CheckAuthoringOwnership), records `output` in
// `batch`, then writes and publishes like PublishBatch.
[[nodiscard]] std::expected<std::size_t, std::string> PublishAuthoredBatch(
    const Content::ConversionManifest& previous,
    const std::filesystem::path& root, ConverterBatch& batch,
    const Content::ConversionOutput& output, bool force,
    const std::function<std::expected<void, std::string>()>& write);

// A file to write: its name and its bytes.
struct OutputBytes {
  std::string_view name;
  std::span<const std::byte> bytes;
};

// Reading and writing the byte fields of an original record, and the
// hexadecimal text offline provenance files keep for the rest.
namespace source_bytes {
// The little-endian unsigned value of the `width` bytes at `bytes[offset]`.
[[nodiscard]] std::uint32_t Read(std::span<const std::byte> bytes,
                                 std::size_t offset, std::size_t width);
// Appends the low `width` bytes of `value`, least significant first.
void Append(std::vector<std::byte>& bytes, std::uint32_t value,
            std::size_t width);
// Lowercase hexadecimal, two digits per byte.
[[nodiscard]] std::string Hex(std::span<const std::byte> bytes);
// The bytes `hex` spells; nothing when its length is odd or it holds a
// character other than 0-9 and a-f.
[[nodiscard]] std::optional<std::vector<std::byte>> Unhex(std::string_view hex);
}  // namespace source_bytes

// Writes every file under `directory` (created if needed) through a temporary
// file, replacing each only after all are written.
[[nodiscard]] std::expected<void, std::string> WriteOutputFiles(
    const std::filesystem::path& directory, std::span<const OutputBytes> files);

// Writes each name and text of `texts` as WriteOutputFiles does; the number of
// files written.
template <typename Texts>
[[nodiscard]] std::expected<std::size_t, std::string> WriteTextFiles(
    const std::filesystem::path& directory, const Texts& texts) {
  std::vector<OutputBytes> files;
  for (const auto& [name, text] : texts) {
    files.push_back({.name = name, .bytes = std::as_bytes(std::span(text))});
  }
  if (files.empty()) return 0;
  if (auto written = WriteOutputFiles(directory, files); !written) {
    return std::unexpected(written.error());
  }
  return files.size();
}

}  // namespace VS::Extract
