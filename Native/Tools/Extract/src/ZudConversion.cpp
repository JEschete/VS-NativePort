// The zud-json converter (ZudConversion.hpp).
#include "Extract/ZudConversion.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Content/ActorArchetype.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Content/WepFile.hpp"
#include "Content/ZudFile.hpp"
#include "Core/Sha256.hpp"
#include "Extract/AnimationAuthoring.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/EsqConversion.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ModelAuthoring.hpp"
#include "Extract/WepModelConversion.hpp"

namespace VS::Extract {
using Content::ActorArchetypeDocument;
using Content::ArchetypeAnimation;
using Content::ArchetypeOwnModel;
using Content::ArchetypeReference;
using Content::ConversionManifest;
using Content::ConversionOutput;
using Content::DataStore;
using Content::ImageFile;
using Content::ShpModelDocument;
using Content::ShpTextureSize;
using Content::SourceDependency;
using Content::ZudFile;
using Content::ZudSection;

namespace {
namespace fs = std::filesystem;
using Check = std::expected<void, std::string>;
template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::string_view kRawForm = "raw";
constexpr std::string_view kArchetypeIdPrefix = "archetype";
constexpr std::string_view kModelIdPrefix = "model";
constexpr std::string_view kJsonSuffix = ".json";
// Where a borrowed texture's PNG is from models\zud: the lender's own files.
constexpr std::string_view kLenderImageDirectory = "../characters/";
// The identity byte after the seven the header names; zero in every ZUD.
constexpr std::size_t kPadByte = 7;

// The OBJ files a section may be.
struct PartKind {
  std::string_view extension;
  std::string_view converter;
  Result<std::string> (*id_of)(std::string_view);
  // The identity byte that numbers the file the game loads (<NN>.SHP and
  // <NN>.WEP), which breaks a tie between identical files.
  std::optional<std::size_t> number_byte;
};

constexpr std::size_t kCharacterByte = 0;
constexpr std::size_t kWeaponByte = 1;
constexpr std::size_t kShieldByte = 4;

PartKind KindOf(ZudSection section) {
  switch (section) {
    case ZudSection::kBody:
      return {.extension = Content::kShpExtension,
              .converter = Content::kShpModelConverterName,
              .id_of = ShpModelIdForDiscPath,
              .number_byte = kCharacterByte};
    case ZudSection::kWeapon:
    case ZudSection::kShield:
      return {.extension = Content::kWepExtension,
              .converter = Content::kWepModelConverterName,
              .id_of = WepModelIdForDiscPath,
              .number_byte =
                  section == ZudSection::kWeapon ? kWeaponByte : kShieldByte};
    case ZudSection::kCommonAnimation:
    case ZudSection::kBattleAnimation:
      break;
  }
  return {.extension = Content::kSeqExtension,
          .converter = Content::kSeqAnimationConverterName,
          .id_of = SeqAnimationIdForDiscPath,
          .number_byte = std::nullopt};
}

ZudSection SectionAt(std::size_t index) {
  return static_cast<ZudSection>(index);
}

// What the archetypes convert from: every ZUD files.tsv lists, BATTLE.PRG,
// and the disc paths of every SHP, WEP and SEQ by their bytes' hash (sorted).
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct ZudSources {
  std::vector<SourceFile> zuds;
  std::optional<SourceFile> program;
  std::map<std::string, std::vector<std::string>, std::less<>> parts;
};

bool IsPartPath(std::string_view path) {
  return path.ends_with(Content::kShpExtension) ||
         path.ends_with(Content::kWepExtension) ||
         path.ends_with(Content::kSeqExtension);
}

// Reads the OBJ files only when there are ZUDs to match them.
Result<ZudSources> ReadZudSources(
    const fs::path& root, const std::vector<DataStore::Entry>& entries) {
  ZudSources sources;
  for (const DataStore::Entry& entry : entries) {
    if (entry.form != kRawForm) continue;
    const bool zud = entry.path.ends_with(Content::kZudExtension);
    if (!zud && entry.path != kBattleProgramPath) continue;
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    if (zud) {
      sources.zuds.push_back(std::move(*source));
    } else {
      sources.program = std::move(*source);
    }
  }
  if (sources.zuds.empty()) return sources;
  for (const DataStore::Entry& entry : entries) {
    if (entry.form != kRawForm || !IsPartPath(entry.path)) continue;
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    sources.parts[HashOf(*source)].push_back(entry.path);
  }
  for (auto& [hash, paths] : sources.parts) std::ranges::sort(paths);
  return sources;
}

// The file of `kind` whose bytes hash to `hash`: the one the identity byte
// numbers, else the first by path.
std::optional<std::string> MatchOf(const ZudSources& sources,
                                   std::string_view hash, const PartKind& kind,
                                   const ZudFile& zud) {
  const auto found = sources.parts.find(hash);
  if (found == sources.parts.end()) return std::nullopt;
  std::optional<std::string> match;
  for (const std::string& path : found->second) {
    const std::optional<std::string> stem = DiscStemOf(path, kind.extension);
    if (!stem) continue;
    if (kind.number_byte &&
        *stem == std::format("{:02X}", zud.identity.at(*kind.number_byte))) {
      return path;
    }
    if (!match) match = path;
  }
  return match;
}

// The section's OBJ file `path`, whose output the earlier converters
// recorded.
Result<ZudPart> PartOf(const ConversionManifest& manifest, std::string path,
                       std::string hash, const PartKind& kind,
                       std::string_view zud, ZudSection section) {
  auto id = kind.id_of(path);
  if (!id) return std::unexpected(id.error());
  const ConversionOutput* const output = manifest.Find(*id);
  if (output == nullptr || output->converter != kind.converter ||
      output->source != path || output->source_sha256 != hash) {
    return std::unexpected(std::format(
        "{}: its {} is {}, which has no current {} output {}; convert the "
        "models first",
        zud, Content::ZudSectionName(section), path, kind.converter, *id));
  }
  return ZudPart{
      .path = std::move(path), .source_sha256 = std::move(hash), .id = *id};
}

Result<ZudFile> DecodeZud(const SourceFile& source) {
  auto zud = Content::DecodeZudFile(source.bytes);
  if (!zud) {
    return std::unexpected(std::format("{}: {}", source.path, zud.error()));
  }
  if (zud->identity.at(kPadByte) != 0) {
    return std::unexpected(std::format("{}: identity byte {} is {}, not 0",
                                       source.path, kPadByte,
                                       zud->identity.at(kPadByte)));
  }
  return zud;
}

Result<ZudConversion> PlanConversion(SourceFile source,
                                     const ZudSources& sources,
                                     const ConversionManifest& manifest,
                                     const SeqEventTable& events,
                                     const fs::path& root) {
  auto zud = DecodeZud(source);
  if (!zud) return std::unexpected(zud.error());
  ZudConversion conversion{
      .zud = {}, .parts = {}, .lender = std::nullopt, .events = events};
  for (std::size_t index = 0; index < Content::kZudSectionCount; ++index) {
    const ZudSection section = SectionAt(index);
    const std::vector<std::byte>& bytes = zud->Section(section);
    std::optional<ZudPart> part;
    if (!bytes.empty()) {
      const PartKind kind = KindOf(section);
      std::string hash = Core::Sha256Hex(bytes);
      if (auto match = MatchOf(sources, hash, kind, *zud)) {
        auto found = PartOf(manifest, std::move(*match), std::move(hash), kind,
                            source.path, section);
        if (!found) return std::unexpected(found.error());
        part = std::move(*found);
      }
    }
    conversion.parts.push_back(std::move(part));
  }
  const std::vector<std::byte>& body = zud->Section(ZudSection::kBody);
  if (!conversion.parts.front() && !body.empty()) {
    auto peeked = Content::PeekShpTexture(body);
    if (!peeked) {
      return std::unexpected(
          std::format("{}: the body: {}", source.path, peeked.error()));
    }
    if (peeked->lender) {
      auto lender = ReadSource(root, ShpLenderPath(*peeked->lender));
      if (!lender) return std::unexpected(lender.error());
      conversion.lender = std::move(*lender);
    }
  }
  conversion.zud = std::move(source);
  return conversion;
}

// Whether a non-empty section is no OBJ file, so the archetype keeps it.
bool KeepsOwn(const ZudConversion& conversion, const ZudFile& zud,
              ZudSection section) {
  const auto index = static_cast<std::size_t>(section);
  return !zud.Section(section).empty() && !conversion.parts.at(index);
}

// The OBJ files the archetype names, the event stride table when it keeps a
// body or bank of its own, and the body's lender, sorted by source.
Result<std::vector<SourceDependency>> DependenciesOf(
    const ZudConversion& conversion) {
  auto zud = DecodeZud(conversion.zud);
  if (!zud) return std::unexpected(zud.error());
  std::vector<SourceDependency> dependencies;
  bool keeps_own = false;
  for (std::size_t index = 0; index < Content::kZudSectionCount; ++index) {
    if (const std::optional<ZudPart>& part = conversion.parts.at(index)) {
      dependencies.push_back(
          {.source = part->path, .source_sha256 = part->source_sha256});
    }
    keeps_own = keeps_own || KeepsOwn(conversion, *zud, SectionAt(index));
  }
  if (keeps_own) {
    if (conversion.events.source.empty()) {
      return std::unexpected(
          std::format("{} needs {}, which files.tsv does not list",
                      conversion.zud.path, kBattleProgramPath));
    }
    dependencies.push_back({.source = conversion.events.source,
                            .source_sha256 = conversion.events.source_sha256});
  }
  if (conversion.lender) {
    dependencies.push_back({.source = conversion.lender->path,
                            .source_sha256 = HashOf(*conversion.lender)});
  }
  std::ranges::sort(dependencies, {}, &SourceDependency::source);
  const auto repeated =
      std::ranges::unique(dependencies, {}, &SourceDependency::source);
  dependencies.erase(repeated.begin(), repeated.end());
  return dependencies;
}

// The manifest entry `conversion` makes, without its files.
Result<ConversionOutput> ArchetypeOutputOf(const ZudConversion& conversion) {
  auto id = ArchetypeIdForDiscPath(conversion.zud.path);
  if (!id) return std::unexpected(id.error());
  auto dependencies = DependenciesOf(conversion);
  if (!dependencies) return std::unexpected(dependencies.error());
  return ConversionOutput{
      .id = std::move(*id),
      .source = conversion.zud.path,
      .source_sha256 = HashOf(conversion.zud),
      .converter = std::string(Content::kZudArchetypeConverterName),
      .files = {},
      .dependencies = std::move(*dependencies),
      .unreferenced = false,
      .note = {},
      .alias_of = {}};
}

// The id <prefix>.<directories>.<stem> of the ZUD at `disc_path`.
Result<std::string> ZudIdOf(std::string_view disc_path,
                            std::string_view prefix) {
  if (!DiscStemOf(disc_path, Content::kZudExtension)) {
    return std::unexpected(
        std::format("'{}' is not a ZUD enemy bundle", disc_path));
  }
  auto image = ImageIdForDiscPath(disc_path);
  if (!image) return std::unexpected(image.error());
  // image.<directories>.<stem> -> <prefix>.<directories>.<stem>
  std::string id = std::format(
      "{}{}", prefix, std::string_view(*image).substr(image->find('.')));
  if (!Content::IsContentId(id)) {
    return std::unexpected(std::format(
        "'{}' gives the id '{}', which is not valid", disc_path, id));
  }
  return id;
}

ArchetypeAnimation AnimationOf(const std::optional<ZudPart>& part) {
  if (part) return ArchetypeReference{.id = part->id};
  return std::monostate{};
}

Result<ActorArchetypeDocument> DocumentOf(const ZudConversion& conversion,
                                          const ZudFile& zud,
                                          std::string_view stem) {
  const std::string& path = conversion.zud.path;
  ActorArchetypeDocument document{
      .source = path,
      .source_sha256 = HashOf(conversion.zud),
      .header = Content::ArchetypeHeaderOf(zud.identity),
      .body = ArchetypeReference{},
      .weapon = std::nullopt,
      .shield = std::nullopt,
      .common_animation = AnimationOf(conversion.parts.at(
          static_cast<std::size_t>(ZudSection::kCommonAnimation))),
      .battle_animation = AnimationOf(conversion.parts.at(
          static_cast<std::size_t>(ZudSection::kBattleAnimation)))};
  if (zud.Section(ZudSection::kBody).empty()) {
    return std::unexpected(std::format("{} has no body", path));
  }
  if (const std::optional<ZudPart>& body = conversion.parts.front()) {
    document.body = ArchetypeReference{.id = body->id};
  } else {
    document.body = ArchetypeOwnModel{
        .file = std::format("{}{}", stem, Content::kGltfSuffix)};
  }
  for (const ZudSection section : {ZudSection::kWeapon, ZudSection::kShield}) {
    const std::optional<ZudPart>& part =
        conversion.parts.at(static_cast<std::size_t>(section));
    if (KeepsOwn(conversion, zud, section)) {
      return std::unexpected(std::format("{}: its {} is no WEP files.tsv lists",
                                         path,
                                         Content::ZudSectionName(section)));
    }
    if (part) {
      (section == ZudSection::kWeapon ? document.weapon : document.shield) =
          ArchetypeReference{.id = part->id};
    }
  }
  for (const ZudSection section :
       {ZudSection::kCommonAnimation, ZudSection::kBattleAnimation}) {
    if (!KeepsOwn(conversion, zud, section)) continue;
    auto file =
        Content::DecodeSeqFile(zud.Section(section), conversion.events.lengths);
    if (!file) {
      return std::unexpected(std::format("{}: the {}: {}", path,
                                         Content::ZudSectionName(section),
                                         file.error()));
    }
    (section == ZudSection::kCommonAnimation ? document.common_animation
                                             : document.battle_animation) =
        std::move(*file);
  }
  return document;
}

// A body of the archetype's own and the texture its materials show.
struct OwnBody {
  ShpModelDocument document;
  std::string image_uri;
};

Result<OwnBody> OwnBodyOf(const ZudConversion& conversion, const ZudFile& zud,
                          std::string_view stem) {
  const std::string& path = conversion.zud.path;
  const std::vector<std::byte>& body = zud.Section(ZudSection::kBody);
  auto peeked = Content::PeekShpTexture(body);
  if (!peeked) {
    return std::unexpected(
        std::format("{}: the body: {}", path, peeked.error()));
  }
  std::optional<bool> colored;
  std::optional<ShpTextureSize> lender_texture;
  std::string image_uri = std::format("{}{}", stem, Content::kImagePngSuffix);
  if (peeked->lender) {
    const std::string lender_path = ShpLenderPath(*peeked->lender);
    if (!conversion.lender || conversion.lender->path != lender_path) {
      return std::unexpected(
          std::format("{}'s body borrows the texture of {}, which was not "
                      "given",
                      path, lender_path));
    }
    auto lender = Content::DecodeShp(conversion.lender->bytes);
    if (!lender) {
      return std::unexpected(
          std::format("{}: {}", lender_path, lender.error()));
    }
    if (!lender->texture) {
      return std::unexpected(
          std::format("{}'s body borrows the texture of {}, which borrows its "
                      "own",
                      path, lender_path));
    }
    const std::optional<std::string> lender_stem =
        DiscStemOf(lender_path, Content::kShpExtension);
    if (!lender_stem) {
      return std::unexpected(
          std::format("'{}' is not an SHP model", lender_path));
    }
    colored = lender->colored;
    lender_texture = ShpTextureSize{.width = lender->texture->Width(),
                                    .height = lender->texture->Height()};
    image_uri = std::format("{}{}{}", kLenderImageDirectory, *lender_stem,
                            Content::kImagePngSuffix);
  } else if (conversion.lender) {
    return std::unexpected(std::format(
        "{}'s body has its own texture, so it has no lender", path));
  }
  auto shp = Content::DecodeShp(body, colored);
  if (!shp) {
    return std::unexpected(std::format("{}: the body: {}", path, shp.error()));
  }
  auto id = ZudIdOf(path, kModelIdPrefix);
  if (!id) return std::unexpected(id.error());
  OwnBody own{.document = {.id = std::move(*id),
                           .source = path,
                           .source_sha256 = HashOf(conversion.zud),
                           .image_id = {},
                           .lender_texture = lender_texture,
                           .shp = std::move(*shp),
                           .sequences = {}},
              .image_uri = std::move(image_uri)};
  if (own.document.shp.texture) {
    auto image = ImageIdForDiscPath(path);
    if (!image) return std::unexpected(image.error());
    own.document.image_id = std::move(*image);
  }
  return own;
}

// The own body's files must read back to `document` and rebuild the body.
Result<std::vector<std::byte>> VerifyOwnBody(
    const ShpModelDocument& document, std::string_view stem,
    const std::vector<ImageFile>& files, const ZudConversion& conversion,
    const ZudFile& zud) {
  const std::string& path = conversion.zud.path;
  auto decoded = Content::DecodeShpModelFiles(
      stem, Content::FileListReader(files), conversion.events.lengths);
  if (!decoded) {
    return std::unexpected(
        std::format("{}: the converted body does not read back: {}", path,
                    decoded.error()));
  }
  if (*decoded != document) {
    return std::unexpected(std::format(
        "{}: the converted body reads back to a different model", path));
  }
  auto encoded = Content::EncodeShp(decoded->shp);
  if (!encoded || *encoded != zud.Section(ZudSection::kBody)) {
    return std::unexpected(std::format(
        "{}: the converted body does not rebuild the source bytes", path));
  }
  return encoded;
}

// The section an animation member rebuilds: the OBJ bank's bytes (`named`)
// or the archetype's own bank.
Result<std::vector<std::byte>> AnimationBytes(
    const ArchetypeAnimation& animation, const std::vector<std::byte>& named,
    std::span<const std::uint8_t> event_lengths) {
  if (const auto* file = std::get_if<Content::SeqFile>(&animation)) {
    return Content::EncodeSeqFile(*file, event_lengths);
  }
  if (std::holds_alternative<ArchetypeReference>(animation)) return named;
  return std::vector<std::byte>{};
}

// The JSON must read back to `document`, and it with the OBJ files it names
// and the own body (`own_body`, when it has one) must rebuild the source
// bytes.
Check VerifyArchetype(const ActorArchetypeDocument& document,
                      const ImageFile& file, const ZudConversion& conversion,
                      const ZudFile& zud,
                      const std::optional<std::vector<std::byte>>& own_body) {
  const std::string& path = conversion.zud.path;
  auto json = Content::ParseJsonText(file.bytes, file.name);
  if (!json) {
    return std::unexpected(std::format(
        "{}: the converted archetype is not JSON: {}", path, json.error()));
  }
  auto read = Content::ActorArchetypeOf(*json, file.name);
  if (!read) {
    return std::unexpected(
        std::format("{}: the converted archetype does not read back: {}", path,
                    read.error()));
  }
  if (*read != document) {
    return std::unexpected(std::format(
        "{}: the converted archetype reads back to a different archetype",
        path));
  }
  ZudFile rebuilt{.identity = Content::ZudIdentityOf(read->header),
                  .sections = {}};
  // A named part's bytes are the OBJ file's, whose hash the section has.
  auto& sections = rebuilt.sections;
  using enum ZudSection;
  sections.at(static_cast<std::size_t>(kBody)) =
      own_body ? *own_body : zud.Section(kBody);
  for (const ZudSection part : {kWeapon, kShield}) {
    if (conversion.parts.at(static_cast<std::size_t>(part))) {
      sections.at(static_cast<std::size_t>(part)) = zud.Section(part);
    }
  }
  for (const auto& [section, animation] :
       {std::pair{kCommonAnimation, &read->common_animation},
        std::pair{kBattleAnimation, &read->battle_animation}}) {
    auto bytes = AnimationBytes(*animation, zud.Section(section),
                                conversion.events.lengths);
    if (!bytes) {
      return std::unexpected(std::format("{}: the {}: {}", path,
                                         Content::ZudSectionName(section),
                                         bytes.error()));
    }
    sections.at(static_cast<std::size_t>(section)) = std::move(*bytes);
  }
  auto encoded = Content::EncodeZudFile(rebuilt);
  if (!encoded || *encoded != conversion.zud.bytes) {
    return std::unexpected(std::format(
        "{}: the converted archetype does not rebuild the source bytes", path));
  }
  return {};
}

using Identity = std::array<std::uint8_t, Content::kZudIdentityBytes>;
constexpr std::size_t kHeaderMembers = Content::kZudIdentityBytes - 1;

// The numbered header's members in identity byte order.
constexpr std::array<std::string_view, kHeaderMembers> kNumberedHeader{
    "character_id",     "weapon", "battle_sequence_model",
    "weapon_material",  "shield", "shield_material",
    "default_material",
};

Result<Identity> NumberedIdentityOf(const Content::JsonDocument& document,
                                    const std::string& where) {
  const auto header = document.find("header");
  if (header == document.end() || !header->is_object()) {
    return std::unexpected(std::format("{}.header must be an object", where));
  }
  const Content::JsonMembers members(*header, where + ".header");
  if (auto only = members.Only(kNumberedHeader); !only) {
    return std::unexpected(only.error());
  }
  Identity identity{};
  for (std::size_t index = 0; index < kNumberedHeader.size(); ++index) {
    auto byte = members.Integer(kNumberedHeader.at(index), 0,
                                std::numeric_limits<std::uint8_t>::max());
    if (!byte) return std::unexpected(byte.error());
    identity.at(index) = static_cast<std::uint8_t>(*byte);
  }
  return identity;
}

// Names the header of the archetype `json` holds when it still numbers it;
// whether it did. Other documents and current archetypes are left as they are.
Result<bool> NameHeader(Content::JsonDocument& json, const std::string& where) {
  if (!json.is_object()) return false;
  const auto schema = json.find("schema");
  if (schema == json.end() || !schema->is_string() ||
      schema->get_ref<const std::string&>() != Content::kActorArchetypeSchema) {
    return false;
  }
  const auto version = json.find("schema_version");
  const std::optional<std::int64_t> number =
      version == json.end() ? std::nullopt : Content::JsonWholeNumber(*version);
  if (number == Content::kActorArchetypeSchemaVersion) return false;
  if (number != kNumberedHeaderArchetypeVersion) {
    return std::unexpected(std::format(
        "{}.schema_version must be {} or {} for {}", where,
        kNumberedHeaderArchetypeVersion, Content::kActorArchetypeSchemaVersion,
        Content::kActorArchetypeSchema));
  }
  auto identity = NumberedIdentityOf(json, where);
  if (!identity) return std::unexpected(identity.error());
  *version = Content::kActorArchetypeSchemaVersion;
  json["header"] =
      Content::ArchetypeHeaderJson(Content::ArchetypeHeaderOf(*identity));
  auto read = Content::ActorArchetypeOf(json, where);
  if (!read) return std::unexpected(read.error());
  if (Content::ZudIdentityOf(read->header) != *identity) {
    return std::unexpected(
        std::format("{}.header does not read back from its named form", where));
  }
  return true;
}

// The archetype files of the zud-json outputs when its converter still writes
// numbered headers, moving it to its current version; whether it moved.
bool PlanHeaderFiles(ConversionManifest& manifest,
                     std::set<std::string>& files) {
  const auto found =
      manifest.converters.find(Content::kZudArchetypeConverterName);
  if (found == manifest.converters.end() ||
      found->second != kNumberedHeaderArchetypeVersion) {
    return false;
  }
  found->second = kNamedHeaderArchetypeVersion;
  for (const ConversionOutput& output : manifest.outputs) {
    if (output.converter != Content::kZudArchetypeConverterName) continue;
    for (const std::string& file : output.files) {
      if (file.ends_with(kJsonSuffix)) files.insert(file);
    }
  }
  return true;
}

Check UpgradeHeaderFile(const fs::path& root, const std::string& file,
                        ArchetypeAuthoringReport& report) {
  auto written = EditJsonOutput(
      root, file,
      [&file](Content::JsonDocument& json) { return NameHeader(json, file); });
  if (!written) return std::unexpected(written.error());
  if (*written) {
    ++report.headers;
    ++report.written;
  }
  return {};
}

}  // namespace

std::expected<std::string, std::string> ArchetypeIdForDiscPath(
    std::string_view disc_path) {
  return ZudIdOf(disc_path, kArchetypeIdPrefix);
}

std::expected<ConvertedModel, std::string> ConvertZudArchetype(
    const ZudConversion& conversion) {
  const std::string& path = conversion.zud.path;
  auto zud = DecodeZud(conversion.zud);
  if (!zud) return std::unexpected(zud.error());
  if (conversion.parts.size() != Content::kZudSectionCount) {
    return std::unexpected(
        std::format("{}: the conversion names {} sections, not {}", path,
                    conversion.parts.size(), Content::kZudSectionCount));
  }
  const auto stem = DiscStemOf(path, Content::kZudExtension);
  if (!stem) {
    return std::unexpected(std::format("'{}' is not a ZUD enemy bundle", path));
  }
  auto document = DocumentOf(conversion, *zud, *stem);
  if (!document) return std::unexpected(document.error());
  std::vector<ImageFile> model_files;
  std::optional<std::vector<std::byte>> own_body;
  if (std::holds_alternative<ArchetypeOwnModel>(document->body)) {
    auto own = OwnBodyOf(conversion, *zud, *stem);
    if (!own) return std::unexpected(own.error());
    auto files = EncodeShpModelFiles(own->document, *stem, own->image_uri,
                                     conversion.events.lengths,
                                     Content::kZudHeaderBytes);
    if (!files) return std::unexpected(files.error());
    auto body = VerifyOwnBody(own->document, *stem, *files, conversion, *zud);
    if (!body) return std::unexpected(body.error());
    own_body = std::move(*body);
    model_files = std::move(*files);
  } else if (conversion.lender) {
    return std::unexpected(
        std::format("{}: its body is an OBJ model, so it has no lender", path));
  }
  const std::string text =
      Content::FormatJsonText(Content::ActorArchetypeJson(*document)) + "\n";
  const auto bytes = std::as_bytes(std::span(text));
  ImageFile file{.name = std::format("{}{}", *stem, kJsonSuffix),
                 .bytes = {bytes.begin(), bytes.end()}};
  if (auto verified =
          VerifyArchetype(*document, file, conversion, *zud, own_body);
      !verified) {
    return std::unexpected(verified.error());
  }
  auto output = ArchetypeOutputOf(conversion);
  if (!output) return std::unexpected(output.error());
  std::vector<ImageFile> files;
  files.push_back(std::move(file));
  std::ranges::move(model_files, std::back_inserter(files));
  return ConvertedModelOf(std::move(*output), std::move(files),
                          Content::kArchetypeDirectory);
}

std::expected<ModelConversionReport, std::string> ConvertDiscArchetypes(
    const fs::path& root, bool force) {
  // Existing outputs move to the named event, header and polygon forms
  // instead of reconverting.
  if (!force) {
    if (auto upgraded = UpgradeAnimationAuthoring(root); !upgraded) {
      return std::unexpected(upgraded.error());
    }
    if (auto upgraded = UpgradeArchetypeAuthoring(root); !upgraded) {
      return std::unexpected(upgraded.error());
    }
    if (auto upgraded = UpgradeModelAuthoring(root); !upgraded) {
      return std::unexpected(upgraded.error());
    }
  }
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto sources = ReadZudSources(root, start->entries);
  if (!sources) return std::unexpected(sources.error());
  std::vector<ModelJob> jobs;
  SeqEventTable events;
  if (sources->program) {
    auto table = ProgramEventTable(*sources->program);
    if (!table) return std::unexpected(table.error());
    events = std::move(*table);
  }
  for (SourceFile& zud : sources->zuds) {
    auto conversion =
        PlanConversion(std::move(zud), *sources, start->previous, events, root);
    if (!conversion) return std::unexpected(conversion.error());
    auto expected = ArchetypeOutputOf(*conversion);
    if (!expected) return std::unexpected(expected.error());
    jobs.push_back({.expected = std::move(*expected),
                    .companions = {},
                    .convert = [conversion = std::move(*conversion)] {
                      return ConvertZudArchetype(conversion);
                    }});
  }
  return RunModelConverter(root, *start,
                           {.name = Content::kZudArchetypeConverterName,
                            .version = Content::kZudArchetypeConverterVersion,
                            .directory = Content::kArchetypeDirectory,
                            .force = force,
                            .companion = std::nullopt},
                           jobs);
}

std::expected<ArchetypeAuthoringReport, std::string> UpgradeArchetypeAuthoring(
    const fs::path& root) {
  auto manifest = Content::LoadConversionManifest(root);
  if (!manifest) return std::unexpected(manifest.error());
  ArchetypeAuthoringReport report;
  if (!*manifest) return report;
  std::set<std::string> files;
  if (!PlanHeaderFiles(**manifest, files)) return report;
  for (const std::string& file : files) {
    if (auto upgraded = UpgradeHeaderFile(root, file, report); !upgraded) {
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
