// The model authoring upgrade (ModelAuthoring.hpp).
#include "Extract/ModelAuthoring.hpp"

#include <algorithm>
#include <array>
#include <charconv>
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

#include "Content/ActorArchetype.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Content/WepModelFiles.hpp"
#include "Extract/AnimationAuthoring.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "Extract/ZudConversion.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Json = Content::JsonDocument;
template <typename T>
using Result = std::expected<T, std::string>;

using VersionStep = AuthoringVersionStep;

// The converters whose outputs hold numbered polygon entries. A ZUD's own
// body is a character model; the archetype moves on from the version that
// named its header (UpgradeArchetypeAuthoring).
constexpr std::array kConverters{
    VersionStep{.name = Content::kShpModelConverterName,
                .from = kNumberedPolygonShpVersion,
                .to = Content::kShpModelConverterVersion},
    VersionStep{.name = Content::kShpModelConverterName,
                .from = Content::kShpModelConverterVersion - 1,
                .to = Content::kShpModelConverterVersion},
    VersionStep{.name = Content::kWepModelConverterName,
                .from = kNumberedPolygonWepVersion,
                .to = Content::kWepModelConverterVersion},
    VersionStep{.name = Content::kZudArchetypeConverterName,
                .from = kNamedHeaderArchetypeVersion,
                .to = Content::kZudArchetypeConverterVersion},
    VersionStep{.name = Content::kZudArchetypeConverterName,
                .from = Content::kZudArchetypeConverterVersion - 1,
                .to = Content::kZudArchetypeConverterVersion},
};

struct SchemaStep {
  VersionStep version;
  bool weapon = false;  // a WEP's polygons are never coloured (WepFile.cpp)
};

constexpr std::array kSchemas{
    SchemaStep{.version = {.name = Content::kShpModelSchema,
                           .from = kNumberedPolygonShpVersion,
                           .to = Content::kShpModelSchemaVersion},
               .weapon = false},
    SchemaStep{.version = {.name = Content::kWepModelSchema,
                           .from = kNumberedPolygonWepVersion,
                           .to = Content::kWepModelSchemaVersion},
               .weapon = true},
};

constexpr std::int64_t kByteMax = std::numeric_limits<std::uint8_t>::max();
constexpr std::int64_t kHalfwordMax = std::numeric_limits<std::uint16_t>::max();
constexpr std::int64_t kOrderMax = std::numeric_limits<std::uint32_t>::max();
constexpr std::string_view kLegacyMaterialPrefix = "flags_";
constexpr std::size_t kLegacyMaterialDigits = 2;
constexpr int kHexBase = 16;

// The values a model's polygon entries are read against.
struct ModelHeader {
  Content::ShpPolygonCounts counts{};
  bool colored = false;
};

struct PayloadUpgrade {
  std::optional<ModelHeader> numbered_header;
  bool changed = false;
};

template <std::size_t N>
Result<std::array<std::uint16_t, N>> LegacyHalfwords(const Json& payload,
                                                     std::string_view key,
                                                     const std::string& where) {
  const std::string at = std::format("{}.{}", where, key);
  const auto found = payload.find(std::string(key));
  if (found == payload.end() || !found->is_array() || found->size() != N) {
    return std::unexpected(
        std::format("{} must be an array of {} numbers", at, N));
  }
  std::array<std::uint16_t, N> values{};
  for (std::size_t index = 0; index < N; ++index) {
    auto number = AuthoringNumber((*found)[index], kHalfwordMax,
                                  std::format("{}[{}]", at, index));
    if (!number) return std::unexpected(number.error());
    values.at(index) = static_cast<std::uint16_t>(*number);
  }
  return values;
}

const SchemaStep* FindSchema(const Json& payload) {
  const auto schema = payload.find("schema");
  if (schema == payload.end() || !schema->is_string()) return nullptr;
  for (const SchemaStep& step : kSchemas) {
    if (step.version.name == schema->get_ref<const std::string&>()) {
      return &step;
    }
  }
  return nullptr;
}

// `payload` with its numbered counts and limb ranges in their named members,
// in the same places.
Json NamedPayload(Json& payload, const SchemaStep& step,
                  const Content::ShpPolygonCounts& counts,
                  const Content::ShpLimbRanges& ranges) {
  Json named = Json::object();
  for (auto member = payload.begin(); member != payload.end(); ++member) {
    const std::string& key = member.key();
    if (key == "schema_version") {
      named[key] = step.version.to;
    } else if (key == "polygon_counts") {
      named[key] = Content::ShpPolygonCountsJson(counts);
    } else if (key == "limb_ranges") {
      named["body_part_polygons"] = Content::ShpBodyPartPolygonsJson(ranges);
    } else {
      named[key] = std::move(member.value());
    }
  }
  return named;
}

// Moves the model payload `payload` to its current version; nothing when it
// is already current.
Result<PayloadUpgrade> UpgradePayload(Json& payload, const std::string& where) {
  const SchemaStep* step = FindSchema(payload);
  if (step == nullptr) {
    return std::unexpected(std::format("{} is no {} or {} payload", where,
                                       Content::kShpModelSchema,
                                       Content::kWepModelSchema));
  }
  const auto version = payload.find("schema_version");
  const std::optional<std::int64_t> number =
      version == payload.end() ? std::nullopt
                               : Content::JsonWholeNumber(*version);
  const bool numbered = number == step->version.from;
  const bool previous_named = step->version.name == Content::kShpModelSchema &&
                              number == step->version.to - 1;
  if (number == step->version.to) return PayloadUpgrade{};
  if (!numbered && !previous_named) {
    if (step->version.name == Content::kShpModelSchema) {
      return std::unexpected(
          std::format("{}.schema_version must be {}, {}, or {} for {}", where,
                      step->version.from, step->version.to - 1,
                      step->version.to, step->version.name));
    }
    return std::unexpected(
        std::format("{}.schema_version must be {} or {} for {}", where,
                    step->version.from, step->version.to, step->version.name));
  }
  if (previous_named) {
    payload["schema_version"] = step->version.to;
    return PayloadUpgrade{.numbered_header = std::nullopt, .changed = true};
  }
  ModelHeader header;
  if (!step->weapon) {
    const auto colored = payload.find("colored");
    if (colored == payload.end() || !colored->is_boolean()) {
      return std::unexpected(
          std::format("{}.colored must be true or false", where));
    }
    header.colored = colored->get<bool>();
  }
  auto counts =
      LegacyHalfwords<Content::kShpPartCount>(payload, "polygon_counts", where);
  if (!counts) return std::unexpected(counts.error());
  auto ranges = LegacyHalfwords<Content::kShpLimbRangeCount>(
      payload, "limb_ranges", where);
  if (!ranges) return std::unexpected(ranges.error());
  header.counts = *counts;
  Json named = NamedPayload(payload, *step, *counts, *ranges);
  Content::ShpModel model;
  auto read =
      Content::ReadShpModelHeader(Content::JsonMembers(named, where), model);
  if (!read) return std::unexpected(read.error());
  if (*read != *counts || model.limb_ranges != *ranges) {
    return std::unexpected(
        std::format("{} does not read back from its named form", where));
  }
  payload = std::move(named);
  return PayloadUpgrade{.numbered_header = header, .changed = true};
}

template <std::size_t N>
Result<void> AddSequenceBankReferences(
    const Json& layout, std::string_view lba_key, std::string_view size_key,
    const std::string& where, std::span<const Content::DataStore::Entry> files,
    std::array<std::optional<std::string>, N>& references) {
  const auto lbas = layout.find(std::string(lba_key));
  const auto sizes = layout.find(std::string(size_key));
  const std::string lba_where = std::format("{}.{}", where, lba_key);
  const std::string size_where = std::format("{}.{}", where, size_key);
  if (lbas == layout.end() || !lbas->is_array() || lbas->size() != N) {
    return std::unexpected(
        std::format("{} must be an array of {} LBAs", lba_where, N));
  }
  if (sizes == layout.end() || !sizes->is_array() || sizes->size() != N) {
    return std::unexpected(
        std::format("{} must be an array of {} byte sizes", size_where, N));
  }
  for (std::size_t index = 0; index < N; ++index) {
    const std::string at = std::format("{}[{}]", lba_where, index);
    auto lba = AuthoringNumber((*lbas)[index], kOrderMax, at);
    if (!lba) return std::unexpected(lba.error());
    auto size = AuthoringNumber((*sizes)[index], kOrderMax,
                                std::format("{}[{}]", size_where, index));
    if (!size) return std::unexpected(size.error());
    const std::uint32_t sectors = static_cast<std::uint32_t>(*size) >> 11;
    if (sectors == 0) continue;
    const auto file =
        std::ranges::lower_bound(files, static_cast<std::uint32_t>(*lba), {},
                                 &Content::DataStore::Entry::lba);
    if (file == files.end() || file->lba != *lba ||
        !file->path.ends_with(Content::kSeqExtension) ||
        file->SectorCount() != sectors) {
      return std::unexpected(std::format(
          "{} names {} sectors at LBA {}, which is no SEQ files.tsv lists", at,
          sectors, *lba));
    }
    auto id = SeqAnimationIdForDiscPath(file->path);
    if (!id) return std::unexpected(id.error());
    references.at(index) = std::move(*id);
  }
  return {};
}

Result<void> UpgradeSequenceBanks(
    Json& payload, std::span<const Content::DataStore::Entry> files,
    const std::string& where, bool map_layout_references) {
  const auto layout = payload.find("layout");
  if (layout == payload.end() || !layout->is_object()) {
    return std::unexpected(std::format("{}.layout must be an object", where));
  }
  auto sequences = payload.find("sequences");
  if (sequences == payload.end() || !sequences->is_array()) {
    return std::unexpected(std::format("{}.sequences must be an array", where));
  }
  Json named = Json::array();
  std::set<std::string, std::less<>> known;
  for (std::size_t index = 0; index < sequences->size(); ++index) {
    Json& animation = (*sequences)[index];
    const std::string item = std::format("{}.sequences[{}]", where, index);
    if (!animation.is_object()) {
      return std::unexpected(
          std::format("{} must be an animation object", item));
    }
    const auto source = animation.find("source");
    if (source == animation.end() || !source->is_string()) {
      return std::unexpected(std::format("{}.source must be a string", item));
    }
    auto id = SeqAnimationIdForDiscPath(source->get_ref<const std::string&>());
    if (!id) return std::unexpected(id.error());
    if (!known.insert(*id).second) {
      return std::unexpected(
          std::format("{}.source duplicates a sequence bank ID", item));
    }
    Json bank = Json::object();
    bank["id"] = *id;
    bank["animation"] = std::move(animation);
    named.push_back(std::move(bank));
  }

  Content::ShpSequenceBankReferences references;
  if (map_layout_references) {
    if (auto added = AddSequenceBankReferences(*layout, "battle_sequence_lbas",
                                               "battle_sequence_sizes", where,
                                               files, references.battle);
        !added) {
      return added;
    }
    if (auto added = AddSequenceBankReferences(*layout, "special_sequence_lbas",
                                               "special_sequence_sizes", where,
                                               files, references.special);
        !added) {
      return added;
    }
  }
  const auto check = [&](const auto& slots,
                         std::string_view kind) -> Result<void> {
    for (std::size_t index = 0; index < slots.size(); ++index) {
      if (slots[index] && !known.contains(*slots[index])) {
        return std::unexpected(std::format(
            "{}.sequence_bank_ids.{}[{}] has no owned sequence definition "
            "\"{}\"",
            where, kind, index, *slots[index]));
      }
    }
    return {};
  };
  if (auto valid = check(references.battle, "battle"); !valid) {
    return valid;
  }
  if (auto valid = check(references.special, "special"); !valid) {
    return valid;
  }
  const auto json_ids = [](const auto& slots) {
    Json list = Json::array();
    for (const auto& id : slots) {
      list.push_back(id ? Json(*id) : Json(nullptr));
    }
    return list;
  };
  payload["sequences"] = std::move(named);
  payload["sequence_bank_ids"] = {{"battle", json_ids(references.battle)},
                                  {"special", json_ids(references.special)}};
  return {};
}

Result<Content::ShpPolygonEntry> LegacyEntryOf(const Json& json,
                                               const std::string& where) {
  // [ordinal, corners, flags, extra]
  constexpr std::array<std::int64_t, 4> kMaxima{
      kOrderMax, Content::kShpMaxCorners, kByteMax, kByteMax};
  if (!json.is_array() || json.size() != kMaxima.size()) {
    return std::unexpected(
        std::format("{} must be [ordinal, corners, flags, extra]", where));
  }
  std::array<std::int64_t, kMaxima.size()> values{};
  for (std::size_t index = 0; index < values.size(); ++index) {
    auto number = AuthoringNumber(json[index], kMaxima.at(index),
                                  std::format("{}[{}]", where, index));
    if (!number) return std::unexpected(number.error());
    values.at(index) = *number;
  }
  const auto [order, corners, flags, extra] = values;
  return Content::ShpPolygonEntry{.order = static_cast<std::size_t>(order),
                                  .corners = static_cast<std::uint8_t>(corners),
                                  .flags = static_cast<std::uint8_t>(flags),
                                  .extra = static_cast<std::uint8_t>(extra)};
}

// The member `key` of `object`, or null when `object` is no object or lacks
// it.
Json* Member(Json& object, std::string_view key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(std::string(key));
  return found == object.end() ? nullptr : &*found;
}

class PolygonNamer {
 public:
  PolygonNamer(const ModelHeader& header, ModelAuthoringReport& report)
      : header_(header), report_(report) {}

  // Names the polygon entries of every primitive of every mesh of `gltf`.
  Result<void> NameMeshes(Json& gltf, const std::string& where) {
    const auto meshes = gltf.find("meshes");
    if (meshes == gltf.end() || !meshes->is_array()) {
      return std::unexpected(std::format("{}.meshes must be an array", where));
    }
    for (std::size_t mesh = 0; mesh < meshes->size(); ++mesh) {
      const std::string at =
          std::format("{}.meshes[{}].primitives", where, mesh);
      Json& object = (*meshes)[mesh];
      const auto primitives =
          object.is_object() ? object.find("primitives") : object.end();
      if (primitives == object.end() || !primitives->is_array()) {
        return std::unexpected(std::format("{} must be an array", at));
      }
      for (std::size_t index = 0; index < primitives->size(); ++index) {
        if (auto named = NamePrimitive((*primitives)[index],
                                       std::format("{}[{}]", at, index));
            !named) {
          return named;
        }
      }
    }
    return {};
  }

 private:
  Result<void> NamePrimitive(Json& primitive, const std::string& where) {
    const std::string at =
        std::format("{}.extras.{}.{}", where, Content::kShpExtrasKey,
                    Content::kShpPolygonsKey);
    Json* polygons = Member(primitive, "extras");
    polygons = polygons == nullptr ? nullptr
                                   : Member(*polygons, Content::kShpExtrasKey);
    polygons = polygons == nullptr
                   ? nullptr
                   : Member(*polygons, Content::kShpPolygonsKey);
    if (polygons == nullptr || !polygons->is_array()) {
      return std::unexpected(std::format("{} must be an array", at));
    }
    for (std::size_t index = 0; index < polygons->size(); ++index) {
      if (auto named =
              NameEntry((*polygons)[index], std::format("{}[{}]", at, index));
          !named) {
        return named;
      }
    }
    return {};
  }

  Result<void> NameEntry(Json& json, const std::string& where) {
    auto entry = LegacyEntryOf(json, where);
    if (!entry) return std::unexpected(entry.error());
    auto named =
        Content::ShpPolygonEntryJson(*entry, header_.counts, header_.colored);
    if (!named) {
      return std::unexpected(std::format("{}: {}", where, named.error()));
    }
    auto back = Content::ShpPolygonEntryOf(*named, where, header_.counts,
                                           header_.colored);
    if (!back) return std::unexpected(back.error());
    if (*back != *entry) {
      return std::unexpected(
          std::format("{} does not read back from its named form", where));
    }
    json = std::move(*named);
    ++report_.polygons;
    return {};
  }

  const ModelHeader& header_;
  ModelAuthoringReport& report_;
};

// flags_<two hex digits>[_<variant>] as the style name the digits' primitive
// flags have (Content::ShpMaterialName), keeping the variant; nothing for
// other names.
std::optional<std::string> NamedMaterial(std::string_view legacy) {
  if (!legacy.starts_with(kLegacyMaterialPrefix)) return std::nullopt;
  const std::string_view rest = legacy.substr(kLegacyMaterialPrefix.size());
  if (rest.size() < kLegacyMaterialDigits) return std::nullopt;
  const std::string_view digits = rest.substr(0, kLegacyMaterialDigits);
  const std::string_view variant = rest.substr(kLegacyMaterialDigits);
  if (!variant.empty() && !variant.starts_with('_')) return std::nullopt;
  unsigned flags = 0;
  const auto [end, error] = std::from_chars(
      digits.data(), digits.data() + digits.size(), flags, kHexBase);
  if (error != std::errc() || end != digits.data() + digits.size()) {
    return std::nullopt;
  }
  return Content::ShpMaterialName(
             static_cast<std::uint8_t>(flags & Content::kShpPrimitiveFlags)) +
         std::string(variant);
}

void NameMaterials(Json& gltf) {
  const auto materials = gltf.find("materials");
  if (materials == gltf.end() || !materials->is_array()) return;
  for (Json& material : *materials) {
    if (!material.is_object()) continue;
    const auto name = material.find("name");
    if (name == material.end() || !name->is_string()) continue;
    if (auto named = NamedMaterial(name->get_ref<const std::string&>())) {
      *name = std::move(*named);
    }
  }
}

Json* PayloadOf(Json& gltf) {
  Json* asset = Member(gltf, "asset");
  Json* extras = asset == nullptr ? nullptr : Member(*asset, "extras");
  return extras == nullptr ? nullptr : Member(*extras, Content::kShpExtrasKey);
}

// Moves the model `gltf` holds to its current version; whether it did.
Result<bool> UpgradeModel(Json& gltf, const std::string& file,
                          ModelAuthoringReport& report,
                          std::span<const Content::DataStore::Entry> files) {
  Json* payload = PayloadOf(gltf);
  const std::string where =
      std::format("{}.asset.extras.{}", file, Content::kShpExtrasKey);
  if (payload == nullptr || !payload->is_object()) {
    return std::unexpected(std::format("{} must be an object", where));
  }
  auto header = UpgradePayload(*payload, where);
  if (!header) return std::unexpected(header.error());
  if (!header->changed) return false;
  if (header->numbered_header) {
    PolygonNamer namer(*header->numbered_header, report);
    if (auto named = namer.NameMeshes(gltf, file); !named) {
      return std::unexpected(named.error());
    }
    NameMaterials(gltf);
  }
  const auto schema = payload->find("schema");
  if (schema != payload->end() && schema->is_string() &&
      schema->get_ref<const std::string&>() == Content::kShpModelSchema) {
    // ZUD bodies preserve the layout but do not own OBJ sequence banks.
    const auto source = payload->find("source");
    const bool shp_source =
        source != payload->end() && source->is_string() &&
        source->get_ref<const std::string&>().ends_with(Content::kShpExtension);
    if (auto banks = UpgradeSequenceBanks(*payload, files, where, shp_source);
        !banks) {
      return std::unexpected(banks.error());
    }
  }
  ++report.models;
  return true;
}

bool IsGltfFile(std::string_view file) {
  return file.ends_with(Content::kGltfSuffix);
}

}  // namespace

std::expected<ModelAuthoringReport, std::string> UpgradeModelAuthoring(
    const fs::path& root) {
  ModelAuthoringReport report;
  auto files = Content::DataStore::LoadManifest(root);
  if (!files) return std::unexpected(files.error());
  auto written = UpgradeConverterOutputs(
      root, kConverters, IsGltfFile, [&](const std::string& file, Json& json) {
        return UpgradeModel(json, file, report, *files);
      });
  if (!written) return std::unexpected(written.error());
  report.written = *written;
  return report;
}

}  // namespace VS::Extract
