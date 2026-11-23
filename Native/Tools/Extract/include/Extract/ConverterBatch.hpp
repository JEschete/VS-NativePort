#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
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

// A file to write: its name and its bytes.
struct OutputBytes {
  std::string_view name;
  std::span<const std::byte> bytes;
};

// Writes every file under `directory` (created if needed) through a temporary
// file, replacing each only after all are written.
[[nodiscard]] std::expected<void, std::string> WriteOutputFiles(
    const std::filesystem::path& directory, std::span<const OutputBytes> files);

}  // namespace VS::Extract
