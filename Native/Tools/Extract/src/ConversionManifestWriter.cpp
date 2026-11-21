#include "Extract/ConversionManifestWriter.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/JsonText.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Content::ConversionOutput;
using Content::JsonDocument;
using Json = JsonDocument;

Json OutputJson(const ConversionOutput& output) {
  Json json;
  json["id"] = output.id;
  json["source"] = output.source;
  json["source_sha256"] = output.source_sha256;
  json["converter"] = output.converter;
  json["files"] = output.files;
  if (!output.dependencies.empty()) {
    Json dependencies = Json::array();
    for (const Content::SourceDependency& dependency : output.dependencies) {
      Json entry;
      entry["source"] = dependency.source;
      entry["source_sha256"] = dependency.source_sha256;
      dependencies.push_back(std::move(entry));
    }
    json["dependencies"] = std::move(dependencies);
  }
  if (output.unreferenced) json["unreferenced"] = true;
  if (!output.note.empty()) json["note"] = output.note;
  return json;
}
}  // namespace

std::expected<std::string, std::string> FormatConversionManifest(
    const Content::ConversionManifest& manifest) {
  if (auto valid = Content::ValidateConversionManifest(manifest); !valid) {
    return std::unexpected(valid.error());
  }
  Json json;
  json["schema_version"] = Content::kConversionManifestSchemaVersion;
  json["extractor_version"] = manifest.extractor_version;
  Json converters = Json::object();
  for (const auto& [name, version] : manifest.converters) {
    converters[name] = version;
  }
  json["converters"] = std::move(converters);
  std::vector<const ConversionOutput*> sorted;
  for (const ConversionOutput& output : manifest.outputs) {
    sorted.push_back(&output);
  }
  std::ranges::sort(sorted, {}, &ConversionOutput::id);
  Json outputs = Json::array();
  for (const ConversionOutput* output : sorted) {
    outputs.push_back(OutputJson(*output));
  }
  json["outputs"] = std::move(outputs);
  return Content::FormatJsonText(json) + "\n";
}

std::expected<void, std::string> WriteConversionManifest(
    const fs::path& root, const Content::ConversionManifest& manifest) {
  auto text = FormatConversionManifest(manifest);
  if (!text) return std::unexpected(text.error());
  const fs::path path =
      root / Platform::Utf8Path(Content::kConversionManifestName);
  fs::path temporary = path;
  temporary += ".tmp";
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  output.write(text->data(), static_cast<std::streamsize>(text->size()));
  output.close();
  std::error_code error;
  if (!output) {
    fs::remove(temporary, error);
    return std::unexpected(
        std::format("cannot write {}", Platform::PathText(temporary)));
  }
  fs::rename(temporary, path, error);
  if (error) {
    return std::unexpected(std::format(
        "cannot replace {}: {}", Platform::PathText(path), error.message()));
  }
  return {};
}

void ReplaceConverterOutputs(Content::ConversionManifest& manifest,
                             std::string_view converter, std::uint32_t version,
                             std::vector<Content::ConversionOutput> outputs) {
  std::erase_if(manifest.outputs, [converter](const ConversionOutput& output) {
    return output.converter == converter;
  });
  manifest.converters[std::string(converter)] = version;
  std::ranges::move(outputs, std::back_inserter(manifest.outputs));
  std::ranges::sort(manifest.outputs, {}, &ConversionOutput::id);
}

}  // namespace VS::Extract
