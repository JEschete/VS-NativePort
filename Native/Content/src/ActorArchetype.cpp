// The vs.actor-archetype JSON form of a ZUD enemy bundle (ActorArchetype.hpp).
#include "Content/ActorArchetype.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "Content/ActorAnimation.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"

namespace VS::Content {
namespace {
using Json = JsonDocument;
template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::string_view kModelExtension = ".gltf";
constexpr std::int64_t kByteMax = 0xFF;

template <typename... Visitors>
struct Overloaded : Visitors... {
  using Visitors::operator()...;
};

Json HeaderJson(const ArchetypeHeader& header) {
  Json json = Json::object();
  json["character_id"] = header.character_id;
  json["weapon"] = header.weapon;
  json["battle_sequence_model"] = header.battle_sequence_model;
  json["weapon_material"] = header.weapon_material;
  json["shield"] = header.shield;
  json["shield_material"] = header.shield_material;
  json["default_material"] = header.default_material;
  return json;
}

Json ReferenceJson(std::string_view key, const ArchetypeReference& reference) {
  Json json = Json::object();
  json[std::string(key)] = reference.id;
  return json;
}

Json BodyJson(const ArchetypeBody& body) {
  return std::visit(Overloaded{[](const ArchetypeReference& reference) {
                                 return ReferenceJson("model", reference);
                               },
                               [](const ArchetypeOwnModel& own) {
                                 Json json = Json::object();
                                 json["file"] = own.file;
                                 return json;
                               }},
                    body);
}

Json PartJson(const std::optional<ArchetypeReference>& part) {
  return part ? ReferenceJson("model", *part) : Json(nullptr);
}

Json AnimationJson(const ArchetypeAnimation& animation) {
  return std::visit(Overloaded{[](std::monostate) { return Json(nullptr); },
                               [](const ArchetypeReference& reference) {
                                 return ReferenceJson("animation", reference);
                               },
                               [](const SeqFile& file) {
                                 Json json = Json::object();
                                 json["bones"] = file.bones;
                                 json["bank"] = SeqBankJson(file.bank);
                                 return json;
                               }},
                    animation);
}

Result<std::uint8_t> ByteOf(const JsonMembers& members, std::string_view key) {
  return members.Integer(key, 0, kByteMax).transform([](std::int64_t value) {
    return static_cast<std::uint8_t>(value);
  });
}

Result<ArchetypeHeader> HeaderOf(const JsonMembers& members) {
  auto value = members.Get("header");
  if (!value) return std::unexpected(value.error());
  const JsonMembers header(**value, members.Where("header"));
  if (auto only = header.Only(
          {"character_id", "weapon", "battle_sequence_model", "weapon_material",
           "shield", "shield_material", "default_material"});
      !only) {
    return std::unexpected(only.error());
  }
  ArchetypeHeader result;
  for (auto [key, field] :
       {std::pair{"character_id", &ArchetypeHeader::character_id},
        std::pair{"weapon", &ArchetypeHeader::weapon},
        std::pair{"battle_sequence_model",
                  &ArchetypeHeader::battle_sequence_model},
        std::pair{"weapon_material", &ArchetypeHeader::weapon_material},
        std::pair{"shield", &ArchetypeHeader::shield},
        std::pair{"shield_material", &ArchetypeHeader::shield_material},
        std::pair{"default_material", &ArchetypeHeader::default_material}}) {
    auto byte = ByteOf(header, key);
    if (!byte) return std::unexpected(byte.error());
    result.*field = *byte;
  }
  return result;
}

Result<ArchetypeReference> ReferenceOf(const JsonMembers& members,
                                       std::string_view key) {
  if (auto only = members.Only({key}); !only) {
    return std::unexpected(only.error());
  }
  auto id = members.String(key);
  if (!id) return std::unexpected(id.error());
  if (!IsContentId(*id)) {
    return std::unexpected(std::format(
        "{} \"{}\" is not a content id (lowercase letters, digits, '.', '_' "
        "and '-')",
        members.Where(key), *id));
  }
  return ArchetypeReference{.id = std::move(*id)};
}

Result<ArchetypeBody> BodyOf(const JsonMembers& members) {
  auto value = members.Get("body");
  if (!value) return std::unexpected(value.error());
  const JsonMembers body(**value, members.Where("body"));
  if (body.Find("file") == nullptr) {
    return ReferenceOf(body, "model")
        .transform([](ArchetypeReference reference) {
          return ArchetypeBody(std::move(reference));
        });
  }
  if (auto only = body.Only({"file"}); !only) {
    return std::unexpected(only.error());
  }
  auto file = body.String("file");
  if (!file) return std::unexpected(file.error());
  if (!ArchetypeModelStem(*file)) {
    return std::unexpected(
        std::format("{} \"{}\" must be a .gltf file name of letters, digits, "
                    "'_' and '-' in the archetype's directory",
                    body.Where("file"), *file));
  }
  return ArchetypeBody(ArchetypeOwnModel{.file = std::move(*file)});
}

Result<std::optional<ArchetypeReference>> PartOf(const JsonMembers& members,
                                                 std::string_view key) {
  auto value = members.Get(key);
  if (!value) return std::unexpected(value.error());
  if ((*value)->is_null()) return std::nullopt;
  return ReferenceOf(JsonMembers(**value, members.Where(key)), "model");
}

Result<ArchetypeAnimation> AnimationOf(const JsonMembers& members,
                                       std::string_view key) {
  auto value = members.Get(key);
  if (!value) return std::unexpected(value.error());
  if ((*value)->is_null()) return ArchetypeAnimation();
  const JsonMembers animation(**value, members.Where(key));
  if (animation.Find("animation") != nullptr) {
    return ReferenceOf(animation, "animation")
        .transform([](ArchetypeReference reference) {
          return ArchetypeAnimation(std::move(reference));
        });
  }
  if (auto only = animation.Only({"bones", "bank"}); !only) {
    return std::unexpected(only.error());
  }
  SeqFile file;
  auto bones =
      animation.Integer("bones", 1, static_cast<std::int64_t>(kSeqMaxBones));
  if (!bones) return std::unexpected(bones.error());
  file.bones = static_cast<std::uint16_t>(*bones);
  auto bank = animation.Get("bank").and_then([&](const Json* bank_json) {
    return SeqBankOf(*bank_json, animation.Where("bank"));
  });
  if (!bank) return std::unexpected(bank.error());
  file.bank = std::move(*bank);
  return ArchetypeAnimation(std::move(file));
}

}  // namespace

std::optional<std::string_view> ArchetypeModelStem(
    std::string_view file) noexcept {
  if (!file.ends_with(kModelExtension)) return std::nullopt;
  const std::string_view stem =
      file.substr(0, file.size() - kModelExtension.size());
  const bool plain = !stem.empty() && std::ranges::all_of(stem, [](char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '_' || value == '-';
  });
  return plain ? std::optional(stem) : std::nullopt;
}

JsonDocument ActorArchetypeJson(const ActorArchetypeDocument& document) {
  Json json = Json::object();
  json["schema"] = kActorArchetypeSchema;
  json["schema_version"] = kActorArchetypeSchemaVersion;
  json["source"] = document.source;
  json["source_sha256"] = document.source_sha256;
  json["header"] = HeaderJson(document.header);
  json["body"] = BodyJson(document.body);
  json["weapon"] = PartJson(document.weapon);
  json["shield"] = PartJson(document.shield);
  json["common_animation"] = AnimationJson(document.common_animation);
  json["battle_animation"] = AnimationJson(document.battle_animation);
  return json;
}

std::expected<ActorArchetypeDocument, std::string> ActorArchetypeOf(
    const JsonDocument& json, std::string_view where) {
  const JsonMembers members(json, std::string(where));
  if (auto only = members.Only(
          {"schema", "schema_version", "source", "source_sha256", "header",
           "body", "weapon", "shield", "common_animation", "battle_animation"});
      !only) {
    return std::unexpected(only.error());
  }
  auto source = JsonSourceOf(members, where, kActorArchetypeSchema,
                             kActorArchetypeSchemaVersion);
  if (!source) return std::unexpected(source.error());
  ActorArchetypeDocument document;
  document.source = std::move(source->source);
  document.source_sha256 = std::move(source->source_sha256);
  auto header = HeaderOf(members);
  if (!header) return std::unexpected(header.error());
  document.header = *header;
  auto body = BodyOf(members);
  if (!body) return std::unexpected(body.error());
  document.body = std::move(*body);
  auto weapon = PartOf(members, "weapon");
  if (!weapon) return std::unexpected(weapon.error());
  document.weapon = std::move(*weapon);
  auto shield = PartOf(members, "shield");
  if (!shield) return std::unexpected(shield.error());
  document.shield = std::move(*shield);
  auto common = AnimationOf(members, "common_animation");
  if (!common) return std::unexpected(common.error());
  document.common_animation = std::move(*common);
  auto battle = AnimationOf(members, "battle_animation");
  if (!battle) return std::unexpected(battle.error());
  document.battle_animation = std::move(*battle);
  return document;
}

}  // namespace VS::Content
