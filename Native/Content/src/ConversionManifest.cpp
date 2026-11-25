// Native\data\manifest.json (ConversionManifest.hpp).
#include "Content/ConversionManifest.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <fstream>
#include <limits>
#include <ranges>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/JsonText.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
namespace fs = std::filesystem;
using Json = JsonDocument;

bool IsConverterName(std::string_view name) {
  return !name.empty() && std::ranges::all_of(name, [](char value) {
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
           value == '-';
  });
}

std::expected<void, std::string> CheckDependencies(
    const ConversionOutput& output) {
  for (std::size_t index = 0; index < output.dependencies.size(); ++index) {
    const SourceDependency& dependency = output.dependencies[index];
    if (dependency.source.empty() ||
        !IsLowercaseSha256(dependency.source_sha256)) {
      return std::unexpected(std::format(
          "{}: dependency {} needs a source path and its lowercase SHA-256",
          output.id, index));
    }
    if (index != 0 &&
        output.dependencies[index - 1].source >= dependency.source) {
      return std::unexpected(std::format(
          "{}: dependencies must be sorted by source without repeats ('{}')",
          output.id, dependency.source));
    }
  }
  return {};
}

std::expected<void, std::string> CheckOutput(const ConversionManifest& manifest,
                                             const ConversionOutput& output) {
  if (!IsContentId(output.id)) {
    return std::unexpected(std::format(
        "output id '{}' must be lowercase letters, digits, '.', '_' or '-'",
        output.id));
  }
  if (output.source.empty() || !IsLowercaseSha256(output.source_sha256)) {
    return std::unexpected(std::format(
        "{}: needs a source path and its lowercase SHA-256", output.id));
  }
  if (!manifest.converters.contains(output.converter)) {
    return std::unexpected(
        std::format("{}: converter '{}' is not listed in converters", output.id,
                    output.converter));
  }
  if (!output.alias_of.empty()) {
    if (!output.files.empty()) {
      return std::unexpected(std::format("{}: an alias of {} lists no files",
                                         output.id, output.alias_of));
    }
  } else if (output.files.empty()) {
    return std::unexpected(std::format("{}: lists no files", output.id));
  }
  for (const std::string& file : output.files) {
    if (!IsDataRelativePath(file)) {
      return std::unexpected(std::format(
          "{}: file '{}' is not a '/'-separated path under the data root",
          output.id, file));
    }
  }
  return CheckDependencies(output);
}

// An alias names another output of its converter that has files and the same
// source bytes.
std::expected<void, std::string> CheckAlias(const ConversionManifest& manifest,
                                            const ConversionOutput& output) {
  if (output.alias_of.empty()) return {};
  const ConversionOutput* const target = manifest.Find(output.alias_of);
  if (target == nullptr || !target->alias_of.empty() ||
      target->converter != output.converter) {
    return std::unexpected(
        std::format("{}: alias_of '{}' must name a {} output with files",
                    output.id, output.alias_of, output.converter));
  }
  if (target->source_sha256 != output.source_sha256) {
    return std::unexpected(
        std::format("{}: its source hash differs from its alias target {}",
                    output.id, output.alias_of));
  }
  return {};
}

std::expected<std::vector<std::string>, std::string> FileListOf(
    const JsonMembers& members) {
  auto value = members.Get("files");
  if (!value) return std::unexpected(value.error());
  if (!(*value)->is_array() ||
      !std::ranges::all_of(**value,
                           [](const Json& file) { return file.is_string(); })) {
    return std::unexpected(
        std::format("{} must be a list of paths", members.Where("files")));
  }
  std::vector<std::string> files;
  for (const Json& file : **value) files.push_back(file.get<std::string>());
  return files;
}

std::expected<std::vector<SourceDependency>, std::string> DependenciesOf(
    const JsonMembers& members) {
  std::vector<SourceDependency> dependencies;
  const Json* value = members.Find("dependencies");
  if (value == nullptr) return dependencies;
  if (!value->is_array()) {
    return std::unexpected(
        std::format("{} must be a list", members.Where("dependencies")));
  }
  for (std::size_t index = 0; index < value->size(); ++index) {
    const JsonMembers entry(
        (*value)[index],
        std::format("{}[{}]", members.Where("dependencies"), index));
    if (auto only = entry.Only({"source", "source_sha256"}); !only) {
      return std::unexpected(only.error());
    }
    auto source = entry.String("source");
    if (!source) return std::unexpected(source.error());
    auto hash = entry.String("source_sha256");
    if (!hash) return std::unexpected(hash.error());
    dependencies.push_back(
        {.source = std::move(*source), .source_sha256 = std::move(*hash)});
  }
  return dependencies;
}

std::expected<ConversionOutput, std::string> OutputOf(const Json& value,
                                                      std::string where) {
  const JsonMembers members(value, std::move(where));
  if (auto only =
          members.Only({"id", "source", "source_sha256", "converter", "files",
                        "dependencies", "unreferenced", "note", "alias_of"});
      !only) {
    return std::unexpected(only.error());
  }
  ConversionOutput output;
  for (auto [key, field] :
       {std::pair{"id", &output.id}, std::pair{"source", &output.source},
        std::pair{"source_sha256", &output.source_sha256},
        std::pair{"converter", &output.converter}}) {
    auto text = members.String(key);
    if (!text) return std::unexpected(text.error());
    *field = std::move(*text);
  }
  auto files = FileListOf(members);
  if (!files) return std::unexpected(files.error());
  output.files = std::move(*files);
  auto dependencies = DependenciesOf(members);
  if (!dependencies) return std::unexpected(dependencies.error());
  output.dependencies = std::move(*dependencies);
  if (members.Find("unreferenced") != nullptr) {
    auto unreferenced = members.Boolean("unreferenced");
    if (!unreferenced) return std::unexpected(unreferenced.error());
    output.unreferenced = *unreferenced;
  }
  if (members.Find("note") != nullptr) {
    auto note = members.String("note");
    if (!note) return std::unexpected(note.error());
    output.note = std::move(*note);
  }
  if (members.Find("alias_of") != nullptr) {
    auto alias = members.String("alias_of");
    if (!alias) return std::unexpected(alias.error());
    output.alias_of = std::move(*alias);
  }
  return output;
}

std::expected<void, std::string> ConvertersOf(const JsonMembers& members,
                                              ConversionManifest& manifest) {
  auto value = members.Get("converters");
  if (!value) return std::unexpected(value.error());
  if (!(*value)->is_object()) {
    return std::unexpected(
        std::format("{} must be an object", members.Where("converters")));
  }
  const JsonMembers converters(**value, members.Where("converters"));
  for (const auto& item : (*value)->items()) {
    auto number = converters.Unsigned(
        item.key(), std::numeric_limits<std::uint32_t>::max());
    if (!number) return std::unexpected(number.error());
    manifest.converters[item.key()] = static_cast<std::uint32_t>(*number);
  }
  return {};
}

std::expected<void, std::string> OutputsOf(const JsonMembers& members,
                                           ConversionManifest& manifest) {
  auto value = members.Get("outputs");
  if (!value) return std::unexpected(value.error());
  if (!(*value)->is_array()) {
    return std::unexpected(
        std::format("{} must be a list", members.Where("outputs")));
  }
  for (std::size_t index = 0; index < (*value)->size(); ++index) {
    auto output =
        OutputOf((**value)[index],
                 std::format("{}[{}]", members.Where("outputs"), index));
    if (!output) return std::unexpected(output.error());
    manifest.outputs.push_back(std::move(*output));
  }
  return {};
}

}  // namespace

bool IsContentId(std::string_view id) noexcept {
  return !id.empty() && std::ranges::all_of(id, [](char value) {
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
           value == '.' || value == '_' || value == '-';
  });
}

bool IsDataRelativePath(std::string_view path) noexcept {
  if (path.empty() || path.front() == '/' ||
      path.find_first_of("\\:") != std::string_view::npos) {
    return false;
  }
  for (const auto part : std::views::split(path, '/')) {
    const std::string_view name(part.begin(), part.end());
    if (name.empty() || name == "." || name == "..") return false;
  }
  return true;
}

const ConversionOutput* ConversionManifest::Find(std::string_view id) const {
  const auto found = std::ranges::find(outputs, id, &ConversionOutput::id);
  return found == outputs.end() ? nullptr : &*found;
}

const ConversionOutput* ConversionManifest::FilesOf(
    const ConversionOutput& output) const {
  return output.alias_of.empty() ? &output : Find(output.alias_of);
}

std::expected<void, std::string> ValidateConversionManifest(
    const ConversionManifest& manifest) {
  if (manifest.extractor_version.empty()) {
    return std::unexpected("the manifest names no extractor version");
  }
  for (const auto& [name, version] : manifest.converters) {
    if (!IsConverterName(name) || version == 0) {
      return std::unexpected(std::format(
          "converter '{}' needs a lowercase name and a version from 1", name));
    }
  }
  std::set<std::string_view> ids;
  for (const ConversionOutput& output : manifest.outputs) {
    if (auto valid = CheckOutput(manifest, output); !valid) {
      return valid;
    }
    if (!ids.insert(output.id).second) {
      return std::unexpected(
          std::format("output id '{}' is listed twice", output.id));
    }
  }
  for (const ConversionOutput& output : manifest.outputs) {
    if (auto alias = CheckAlias(manifest, output); !alias) {
      return alias;
    }
  }
  return {};
}

std::expected<ConversionManifest, std::string> ParseConversionManifest(
    std::span<const std::byte> text, std::string_view name) {
  auto json = ParseJsonText(text, name);
  if (!json) return std::unexpected(json.error());
  const JsonMembers members(*json, std::string(name));
  if (auto only = members.Only(
          {"schema_version", "extractor_version", "converters", "outputs"});
      !only) {
    return std::unexpected(only.error());
  }
  auto version = members.Unsigned("schema_version",
                                  std::numeric_limits<std::uint32_t>::max());
  if (!version) return std::unexpected(version.error());
  if (*version < kOldestConversionManifestSchemaVersion ||
      *version > kConversionManifestSchemaVersion) {
    return std::unexpected(std::format(
        "{} is schema version {}; this build reads versions {} to {}", name,
        *version, kOldestConversionManifestSchemaVersion,
        kConversionManifestSchemaVersion));
  }
  ConversionManifest manifest;
  auto extractor = members.String("extractor_version");
  if (!extractor) return std::unexpected(extractor.error());
  manifest.extractor_version = std::move(*extractor);
  if (auto converters = ConvertersOf(members, manifest); !converters) {
    return std::unexpected(converters.error());
  }
  if (auto outputs = OutputsOf(members, manifest); !outputs) {
    return std::unexpected(outputs.error());
  }
  if (auto valid = ValidateConversionManifest(manifest); !valid) {
    return std::unexpected(std::format("{}: {}", name, valid.error()));
  }
  std::ranges::sort(manifest.outputs, {}, &ConversionOutput::id);
  return manifest;
}

std::expected<std::optional<ConversionManifest>, std::string>
LoadConversionManifest(const fs::path& root) {
  const fs::path path = root / Platform::Utf8Path(kConversionManifestName);
  std::error_code error;
  if (!fs::exists(path, error)) {
    if (error) {
      return std::unexpected(std::format(
          "cannot inspect {}: {}", Platform::PathText(path), error.message()));
    }
    return std::optional<ConversionManifest>{};
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::unexpected(
        std::format("cannot open {}", Platform::PathText(path)));
  }
  input.seekg(0, std::ios::end);
  const std::streamoff size = input.tellg();
  input.seekg(0, std::ios::beg);
  std::vector<char> data(size > 0 ? static_cast<std::size_t>(size) : 0);
  if (size < 0 || !input.read(data.data(), size)) {
    return std::unexpected(
        std::format("cannot read {}", Platform::PathText(path)));
  }
  auto manifest = ParseConversionManifest(std::as_bytes(std::span(data)),
                                          Platform::PathText(path));
  if (!manifest) return std::unexpected(manifest.error());
  return std::optional<ConversionManifest>(std::move(*manifest));
}

std::expected<void, std::string> CheckRequiredConverters(
    const ConversionManifest& manifest,
    std::span<const ConverterVersion> required) {
  for (const ConverterVersion& needed : required) {
    const auto found = manifest.converters.find(needed.name);
    if (found == manifest.converters.end()) {
      return std::unexpected(
          std::format("the converted data has no {} output", needed.name));
    }
    if (found->second < needed.version) {
      return std::unexpected(std::format(
          "the converted data is {} version {}; this build needs version {}",
          needed.name, found->second, needed.version));
    }
  }
  return {};
}

}  // namespace VS::Content
