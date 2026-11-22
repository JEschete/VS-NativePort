// What every converter VS.Extract runs shares (ConverterBatch.hpp).
#include "Extract/ConverterBatch.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Core/Sha256.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
using Content::ConversionManifest;
using Content::ConversionOutput;

namespace {
namespace fs = std::filesystem;

constexpr std::string_view kDiscDirectory = "disc";
constexpr std::string_view kTemporarySuffix = ".tmp";

// Removes the files the previous `converter` outputs listed that no current
// one does.
std::expected<std::size_t, std::string> RemoveStaleFiles(
    const fs::path& root, const ConversionManifest& previous,
    const std::vector<ConversionOutput>& outputs, std::string_view converter) {
  std::set<std::string_view> current;
  for (const ConversionOutput& output : outputs) {
    current.insert(output.files.begin(), output.files.end());
  }
  std::size_t removed = 0;
  for (const ConversionOutput& output : previous.outputs) {
    if (output.converter != converter) continue;
    for (const std::string& file : output.files) {
      if (current.contains(file)) continue;
      std::error_code error;
      if (fs::remove(root / Platform::Utf8Path(file), error)) ++removed;
      if (error) {
        return std::unexpected(
            std::format("cannot remove {}: {}", file, error.message()));
      }
    }
  }
  return removed;
}

}  // namespace

std::expected<std::vector<std::byte>, std::string> ReadFileBytes(
    const fs::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return std::unexpected(
        std::format("cannot open {}", Platform::PathText(path)));
  }

  const std::streamoff size = input.tellg();
  std::vector<std::byte> bytes;
  if (size < 0 || static_cast<std::uintmax_t>(size) > bytes.max_size() ||
      size > std::numeric_limits<std::streamsize>::max()) {
    return std::unexpected(
        std::format("cannot size {}", Platform::PathText(path)));
  }
  bytes.resize(static_cast<std::size_t>(size));
  input.seekg(0, std::ios::beg);
  if (!input) {
    return std::unexpected(
        std::format("cannot read {}", Platform::PathText(path)));
  }
  if (size != 0) {
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(size));
  }
  if (input.gcount() != static_cast<std::streamsize>(size)) {
    return std::unexpected(
        std::format("cannot read {}", Platform::PathText(path)));
  }
  return bytes;
}

std::expected<SourceFile, std::string> ReadSource(const fs::path& root,
                                                  std::string_view path) {
  auto bytes = ReadFileBytes(root / Platform::Utf8Path(kDiscDirectory) /
                             Platform::Utf8Path(path));
  if (!bytes) return std::unexpected(bytes.error());
  return SourceFile{.path = std::string(path), .bytes = std::move(*bytes)};
}

std::string HashOf(const SourceFile& source) {
  return Core::Sha256Hex(source.bytes);
}

const ConversionOutput* PresentOutput(const fs::path& root,
                                      const ConversionOutput* output) {
  std::error_code error;
  const bool present =
      std::ranges::all_of(output->files, [&](const std::string& file) {
        return fs::is_regular_file(root / Platform::Utf8Path(file), error);
      });
  return present ? output : nullptr;
}

const ConversionOutput* MatchingOutput(const ConversionManifest& manifest,
                                       const fs::path& root,
                                       const ConversionOutput& expected) {
  const ConversionOutput* output = manifest.Find(expected.id);
  if (output == nullptr || output->files.empty()) return nullptr;
  ConversionOutput recorded = *output;
  recorded.files.clear();
  return recorded == expected ? PresentOutput(root, output) : nullptr;
}

ConverterBatch StartBatch(const ConversionManifest& previous,
                          std::string_view name, std::uint32_t version,
                          bool force) {
  const auto found = previous.converters.find(name);
  return {.name = name,
          .version = version,
          .reuse = !force && found != previous.converters.end() &&
                   found->second == version,
          .outputs = {}};
}

std::expected<RunStart, std::string> StartRun(const fs::path& root) {
  auto entries = Content::DataStore::LoadManifest(root);
  if (!entries) return std::unexpected(entries.error());
  auto loaded = Content::LoadConversionManifest(root);
  if (!loaded) return std::unexpected(loaded.error());
  return RunStart{.entries = std::move(*entries),
                  .previous = loaded->value_or(ConversionManifest{})};
}

std::expected<std::size_t, std::string> PublishBatches(
    const fs::path& root, const ConversionManifest& previous,
    std::span<const ConverterBatch> batches) {
  ConversionManifest manifest = previous;
  manifest.extractor_version = std::string(Content::kExtractorVersion);
  for (const ConverterBatch& batch : batches) {
    ReplaceConverterOutputs(manifest, batch.name, batch.version, batch.outputs);
  }
  if (auto written = WriteConversionManifest(root, manifest); !written) {
    return std::unexpected(written.error());
  }
  std::size_t removed = 0;
  for (const ConverterBatch& batch : batches) {
    auto stale = RemoveStaleFiles(root, previous, batch.outputs, batch.name);
    if (!stale) return std::unexpected(stale.error());
    removed += *stale;
  }
  return removed;
}

std::expected<std::size_t, std::string> PublishBatch(
    const fs::path& root, const ConversionManifest& previous,
    const ConverterBatch& batch,
    const std::function<std::expected<void, std::string>()>& write) {
  if (auto written = write(); !written) {
    return std::unexpected(written.error());
  }
  return PublishBatches(root, previous, std::span(&batch, 1));
}

std::expected<void, std::string> WriteOutputFiles(
    const fs::path& directory, std::span<const OutputBytes> files) {
  std::error_code error;
  fs::create_directories(directory, error);
  if (error) {
    return std::unexpected(std::format("cannot create {}: {}",
                                       Platform::PathText(directory),
                                       error.message()));
  }
  std::vector<fs::path> staged;
  const auto discard = [&staged] {
    std::error_code ignored;
    for (const fs::path& path : staged) fs::remove(path, ignored);
  };
  for (const OutputBytes& file : files) {
    fs::path temporary = directory / Platform::Utf8Path(std::format(
                                         "{}{}", file.name, kTemporarySuffix));
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    staged.push_back(temporary);
    output.write(reinterpret_cast<const char*>(file.bytes.data()),
                 static_cast<std::streamsize>(file.bytes.size()));
    output.close();
    if (!output) {
      discard();
      return std::unexpected(
          std::format("cannot write {}", Platform::PathText(temporary)));
    }
  }
  for (std::size_t index = 0; index < files.size(); ++index) {
    const fs::path target = directory / Platform::Utf8Path(files[index].name);
    fs::rename(staged[index], target, error);
    if (error) {
      return std::unexpected(std::format("cannot replace {}: {}",
                                         Platform::PathText(target),
                                         error.message()));
    }
  }
  return {};
}

}  // namespace VS::Extract
