#include "Extract/AnimationAuthoring.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "Content/ActorAnimation.hpp"
#include "Content/ActorArchetype.hpp"
#include "Content/AnimationSet.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqEventForms.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ZudConversion.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Json = Content::JsonDocument;
template <typename T>
using Result = std::expected<T, std::string>;

// A converter or schema version the named event form replaces.
struct VersionStep {
  std::string_view name;
  std::uint32_t from = 0;
  std::uint32_t to = 0;
};

// The versions that wrote events as [frame, op, arguments...] arrays. An
// archetype moves to the version that still numbers its header;
// UpgradeArchetypeAuthoring names that next.
constexpr std::array kConverters{
    VersionStep{.name = Content::kShpModelConverterName,
                .from = 2,
                .to = Content::kShpModelConverterVersion},
    VersionStep{.name = Content::kSeqAnimationConverterName,
                .from = 1,
                .to = Content::kSeqAnimationConverterVersion},
    VersionStep{.name = Content::kEsqAnimationSetConverterName,
                .from = 1,
                .to = Content::kEsqAnimationSetConverterVersion},
    VersionStep{.name = Content::kZudArchetypeConverterName,
                .from = 1,
                .to = kNumberedHeaderArchetypeVersion},
};
constexpr std::array kSchemas{
    VersionStep{.name = Content::kShpModelSchema,
                .from = 2,
                .to = Content::kShpModelSchemaVersion},
    VersionStep{.name = Content::kActorAnimationSchema,
                .from = 1,
                .to = Content::kActorAnimationSchemaVersion},
    VersionStep{.name = Content::kAnimationSetSchema,
                .from = 1,
                .to = Content::kAnimationSetSchemaVersion},
    VersionStep{.name = Content::kActorArchetypeSchema,
                .from = 1,
                .to = kNumberedHeaderArchetypeVersion},
};

constexpr std::int64_t kByteMax = std::numeric_limits<std::uint8_t>::max();
constexpr std::int64_t kWordMax = std::numeric_limits<std::uint16_t>::max();
// A legacy event is its frame and op, then its arguments.
constexpr std::size_t kLegacyHeader = 2;

const VersionStep* FindStep(std::span<const VersionStep> steps,
                            std::string_view name) {
  for (const VersionStep& step : steps) {
    if (step.name == name) return &step;
  }
  return nullptr;
}

Result<std::int64_t> NumberIn(const Json& value, std::int64_t max,
                              const std::string& where) {
  const std::optional<std::int64_t> number = Content::JsonWholeNumber(value);
  if (!number || *number < 0 || *number > max) {
    return std::unexpected(
        std::format("{} must be a whole number from 0 to {}", where, max));
  }
  return *number;
}

Result<Content::SeqEvent> LegacyEventOf(const Json& json,
                                        const std::string& where) {
  if (!json.is_array() || json.size() < kLegacyHeader) {
    return std::unexpected(std::format(
        "{} must be an array of a frame, an op and its arguments", where));
  }
  Content::SeqEvent event;
  auto frame = NumberIn(json[0], kWordMax, where + "[0]");
  if (!frame) return std::unexpected(frame.error());
  event.frame = static_cast<std::uint16_t>(*frame);
  auto op = NumberIn(json[1], kByteMax, where + "[1]");
  if (!op) return std::unexpected(op.error());
  event.op = static_cast<std::uint8_t>(*op);
  for (std::size_t index = kLegacyHeader; index < json.size(); ++index) {
    auto argument =
        NumberIn(json[index], kByteMax, std::format("{}[{}]", where, index));
    if (!argument) return std::unexpected(argument.error());
    event.arguments.push_back(static_cast<std::uint8_t>(*argument));
  }
  return event;
}

class Upgrader {
 public:
  explicit Upgrader(AnimationAuthoringReport& report) : report_(report) {}

  // Moves every document under `node` still at its previous version to the
  // current one, naming the events of those documents' scripts.
  Result<void> Walk(Json& node, bool converting, const std::string& where) {
    if (node.is_array()) {
      for (std::size_t index = 0; index < node.size(); ++index) {
        auto walked =
            Walk(node[index], converting, std::format("{}[{}]", where, index));
        if (!walked) return walked;
      }
      return {};
    }
    if (!node.is_object()) return {};
    auto current = DocumentVersion(node, where);
    if (!current) return std::unexpected(current.error());
    if (*current) converting = !**current;
    for (auto member = node.begin(); member != node.end(); ++member) {
      const std::string at = std::format("{}.{}", where, member.key());
      if (converting && member.key() == "events" && member->is_array()) {
        if (auto named = NameEvents(*member, at); !named) {
          return named;
        }
      } else if (auto walked = Walk(*member, converting, at); !walked) {
        return walked;
      }
    }
    return {};
  }

 private:
  // Whether `node` is a document already at its current version, or nothing
  // when it is no document this migration knows; a previous version moves to
  // the current one.
  Result<std::optional<bool>> DocumentVersion(Json& node,
                                              const std::string& where) {
    const auto schema = node.find("schema");
    if (schema == node.end() || !schema->is_string()) return std::nullopt;
    const VersionStep* step =
        FindStep(kSchemas, schema->get_ref<const std::string&>());
    if (step == nullptr) return std::nullopt;
    const auto version = node.find("schema_version");
    const std::optional<std::int64_t> number =
        version == node.end() ? std::nullopt
                              : Content::JsonWholeNumber(*version);
    if (number == step->to) return true;
    if (number != step->from) {
      return std::unexpected(
          std::format("{}.schema_version must be {} or {} for {}", where,
                      step->from, step->to, step->name));
    }
    *version = step->to;
    ++report_.documents;
    return false;
  }

  Result<void> NameEvents(Json& events, const std::string& where) {
    for (std::size_t index = 0; index < events.size(); ++index) {
      const std::string at = std::format("{}[{}]", where, index);
      auto event = LegacyEventOf(events[index], at);
      if (!event) return std::unexpected(event.error());
      Json named = Content::SeqEventJson(*event);
      auto back = Content::SeqEventOf(named, at);
      if (!back) return std::unexpected(back.error());
      if (*back != *event) {
        return std::unexpected(
            std::format("{} does not read back from its named form", at));
      }
      events[index] = std::move(named);
      ++report_.events;
    }
    return {};
  }

  AnimationAuthoringReport& report_;
};

bool IsJsonFile(std::string_view file) {
  return file.ends_with(".json") || file.ends_with(Content::kGltfSuffix);
}

// The files of every output whose converter still writes legacy events,
// moving those converters to their current versions; whether any moved. Other
// versions are left for the converters to reconvert.
bool PlanFiles(Content::ConversionManifest& manifest,
               std::set<std::string>& files) {
  bool moved = false;
  for (const VersionStep& step : kConverters) {
    const auto found = manifest.converters.find(step.name);
    if (found == manifest.converters.end() || found->second != step.from) {
      continue;
    }
    found->second = step.to;
    moved = true;
    for (const Content::ConversionOutput& output : manifest.outputs) {
      if (output.converter != step.name) continue;
      for (const std::string& file : output.files) {
        if (IsJsonFile(file)) files.insert(file);
      }
    }
  }
  return moved;
}

Result<void> UpgradeFile(const fs::path& root, const std::string& file,
                         AnimationAuthoringReport& report) {
  auto written = EditJsonOutput(root, file, [&](Json& json) -> Result<bool> {
    const std::size_t before = report.documents;
    Upgrader upgrader(report);
    if (auto walked = upgrader.Walk(json, false, file); !walked) {
      return std::unexpected(walked.error());
    }
    return report.documents != before;
  });
  if (!written) return std::unexpected(written.error());
  if (*written) ++report.written;
  return {};
}

}  // namespace

std::expected<bool, std::string> EditJsonOutput(
    const fs::path& root, const std::string& file,
    const std::function<std::expected<bool, std::string>(Json&)>& edit) {
  const fs::path path = root / Platform::Utf8Path(file);
  std::error_code error;
  if (!fs::exists(path, error)) {
    if (error) {
      return std::unexpected(Platform::PathText(path) + ": " + error.message());
    }
    return false;
  }
  auto bytes = ReadFileBytes(path);
  if (!bytes) return std::unexpected(bytes.error());
  auto json = Content::ParseJsonText(*bytes, file);
  if (!json) return std::unexpected(json.error());
  auto edited = edit(*json);
  if (!edited || !*edited) return edited;
  const std::string text = Content::FormatJsonText(*json) + "\n";
  const std::array outputs{
      OutputBytes{.name = file, .bytes = std::as_bytes(std::span(text))}};
  if (auto written = WriteOutputFiles(root, outputs); !written) {
    return std::unexpected(written.error());
  }
  return true;
}

std::expected<AnimationAuthoringReport, std::string> UpgradeAnimationAuthoring(
    const fs::path& root) {
  auto manifest = Content::LoadConversionManifest(root);
  if (!manifest) return std::unexpected(manifest.error());
  AnimationAuthoringReport report;
  if (!*manifest) return report;
  std::set<std::string> files;
  if (!PlanFiles(**manifest, files)) return report;
  // One file at a time: the character models alone are over 100 MB of JSON.
  for (const std::string& file : files) {
    if (auto upgraded = UpgradeFile(root, file, report); !upgraded) {
      return std::unexpected(upgraded.error());
    }
  }
  if (auto written = WriteConversionManifest(root, **manifest); !written) {
    return std::unexpected(written.error());
  }
  ++report.written;
  return report;
}

}  // namespace VS::Extract
