// The esq-json converter (EsqConversion.hpp).
#include "Extract/EsqConversion.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/AnimationSet.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/EsqFile.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ShpFile.hpp"
#include "Extract/ImageConversion.hpp"

namespace VS::Extract {
using Content::AnimationSetDocument;
using Content::ConversionOutput;
using Content::DataStore;
using Content::EsqEntry;
using Content::EsqFile;
using Content::ImageFile;
using Content::kSeqEventLengthCount;
using Content::kSeqEventLengthsAt;
using Content::SourceDependency;

namespace {
namespace fs = std::filesystem;
using Check = std::expected<void, std::string>;
template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::string_view kRawForm = "raw";
constexpr std::string_view kAnimationSetIdPrefix = "animation-set";
constexpr std::string_view kJsonSuffix = ".json";
constexpr std::uint32_t kByteBits = 8;
constexpr int kHexBase = 16;

// BATTLE.PRG's shared set tables (ActorAssets.cpp SequenceFile and
// BindSharedActorSequence): the kind each model number binds by, then each
// set's first sector (halfwords, from kSetFirstSector) and sector count
// (bytes), by set number. The game reads a model's kind from the kind table
// without a bound, so a model number past 255 reads the set tables' bytes.
// decomp: D_800E8960 (src/BATTLE/BATTLE.PRG/30DB0.c:61)
constexpr std::size_t kKindTableAt = 0x80160;
// decomp: D_800E8A60 (D_800E8A60@BATTLE/BATTLE.PRG)
constexpr std::size_t kSetOffsetsAt = 0x80260;
// decomp: D_800E8B40 (D_800E8B40@BATTLE/BATTLE.PRG)
constexpr std::size_t kSetSectorsAt = 0x80340;
constexpr std::size_t kSetCount = 112;
constexpr auto kKindTableBytes = kSetSectorsAt + kSetCount - kKindTableAt;
constexpr std::uint32_t kSetFirstSector = 0x19797;
constexpr auto kProgramBytes = kSeqEventLengthsAt + kSeqEventLengthCount;
static_assert(kKindTableAt + kKindTableBytes <= kSeqEventLengthsAt);

// A model a set may bind.
struct SetModel {
  std::string path;
  std::string source_sha256;
  std::size_t bones = 0;
};

// What the sets convert from: every ESQ files.tsv lists, with BATTLE.PRG and
// the models (by number) when it lists any.
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct SetSources {
  std::vector<SourceFile> sets;
  std::map<std::uint32_t, const DataStore::Entry*> set_sectors;  // by LBA
  std::optional<SourceFile> program;
  std::map<std::int32_t, SetModel> models;
};

// The model number `path` names when it is a model the game loads by number
// (OBJ/4F.SHP -> 79).
std::optional<std::int32_t> ModelNumberOf(std::string_view path) {
  const auto stem = DiscStemOf(path, Content::kShpExtension);
  if (!stem) return std::nullopt;
  std::int32_t number = 0;
  const char* const end = stem->data() + stem->size();
  const auto [last, error] =
      std::from_chars(stem->data(), end, number, kHexBase);
  if (error != std::errc() || last != end || ShpLenderPath(number) != path) {
    return std::nullopt;
  }
  return number;
}

Result<SetModel> ModelOf(const SourceFile& source) {
  auto counts = Content::DecodeShpCounts(source.bytes);
  if (!counts) {
    return std::unexpected(std::format("{}: {}", source.path, counts.error()));
  }
  return SetModel{.path = source.path,
                  .source_sha256 = HashOf(source),
                  .bones = counts->bones};
}

// Reads the models only when there are sets to bind them.
Result<SetSources> ReadSetSources(
    const fs::path& root, const std::vector<DataStore::Entry>& entries) {
  SetSources sources;
  for (const DataStore::Entry& entry : entries) {
    if (entry.form != kRawForm) continue;
    const bool set = entry.path.ends_with(Content::kEsqExtension);
    if (!set && entry.path != kBattleProgramPath) continue;
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    if (set) {
      sources.set_sectors.emplace(entry.lba, &entry);
      sources.sets.push_back(std::move(*source));
    } else {
      sources.program = std::move(*source);
    }
  }
  if (sources.sets.empty()) return sources;
  for (const DataStore::Entry& entry : entries) {
    const auto number = ModelNumberOf(entry.path);
    if (entry.form != kRawForm || !number) continue;
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    auto model = ModelOf(*source);
    if (!model) return std::unexpected(model.error());
    sources.models.emplace(*number, std::move(*model));
  }
  return sources;
}

std::uint32_t ProgramByte(const SourceFile& program, std::size_t at) {
  return std::to_integer<std::uint32_t>(program.bytes[at]);
}

std::uint32_t ProgramHalfword(const SourceFile& program, std::size_t at) {
  return ProgramByte(program, at) | (ProgramByte(program, at + 1) << kByteBits);
}

// Every set table entry with sectors names a set files.tsv lists, and every
// set has one.
Check CheckSetTables(const SourceFile& program, const SetSources& sources) {
  std::set<std::string, std::less<>> named;
  for (std::size_t set = 0; set < kSetCount; ++set) {
    const std::uint32_t sectors = ProgramByte(program, kSetSectorsAt + set);
    if (sectors == 0) continue;
    const std::uint32_t lba =
        kSetFirstSector +
        ProgramHalfword(program, kSetOffsetsAt + (set * sizeof(std::uint16_t)));
    const auto found = sources.set_sectors.find(lba);
    if (found == sources.set_sectors.end() ||
        found->second->SectorCount() != sectors) {
      return std::unexpected(
          std::format("{}: set {} names {} sectors at LBA {}, which is no ESQ "
                      "files.tsv lists",
                      program.path, set, sectors, lba));
    }
    named.insert(found->second->path);
  }
  for (const SourceFile& set : sources.sets) {
    if (!named.contains(set.path)) {
      return std::unexpected(
          std::format("{} is named by no entry of {}'s set tables", set.path,
                      program.path));
    }
  }
  return {};
}

Result<SeqEventTable> EventsOf(const SourceFile& program) {
  if (program.bytes.size() < kProgramBytes) {
    return std::unexpected(std::format(
        "{} is too short for the set tables and the event stride table",
        program.path));
  }
  SeqEventTable events{
      .source = program.path, .source_sha256 = HashOf(program), .lengths = {}};
  std::ranges::transform(
      std::span(program.bytes)
          .subspan(kSeqEventLengthsAt, kSeqEventLengthCount),
      std::back_inserter(events.lengths),
      [](std::byte value) { return std::to_integer<std::uint8_t>(value); });
  return events;
}

// The game reads selectors as signed halfwords.
std::int32_t SelectorOf(const EsqEntry& entry) {
  return static_cast<std::int16_t>(entry.selector);
}

// The kind the game binds model `number` by, if it reads one.
std::optional<std::int32_t> KindOf(const SourceFile& program,
                                   std::int32_t number) {
  if (number < 0 || static_cast<std::size_t>(number) >= kKindTableBytes) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(
      ProgramByte(program, kKindTableAt + static_cast<std::size_t>(number)));
}

Check CheckSelectors(const SourceFile& set, const EsqFile& file) {
  for (std::size_t later = 1; later < file.entries.size(); ++later) {
    for (std::size_t earlier = 0; earlier < later; ++earlier) {
      if (file.entries[earlier].selector == file.entries[later].selector) {
        return std::unexpected(std::format(
            "{}: entry {} repeats entry {}'s selector {}, so the "
            "game would never use it",
            set.path, later, earlier, file.entries[later].selector));
      }
    }
  }
  return {};
}

// The models each entry binds, as the game binds them: a model takes the first
// entry whose selector is its kind or its number.
std::vector<std::vector<const SetModel*>> BoundModels(
    const EsqFile& file, const SourceFile& program, const SetSources& sources) {
  std::vector<std::vector<const SetModel*>> bound(file.entries.size());
  for (const auto& [number, model] : sources.models) {
    const std::optional<std::int32_t> kind = KindOf(program, number);
    const auto entry =
        std::ranges::find_if(file.entries, [&](const EsqEntry& candidate) {
          const std::int32_t selector = SelectorOf(candidate);
          return selector == number || selector == kind;
        });
    if (entry != file.entries.end()) {
      bound[static_cast<std::size_t>(entry - file.entries.begin())].push_back(
          &model);
    }
  }
  return bound;
}

// BATTLE.PRG and the models the set binds, sorted by source. Every entry binds
// a model with its bones, and a texture row is for a model number.
Result<std::vector<SourceDependency>> SetDependencies(
    const SourceFile& set, const EsqFile& file, const SourceFile& program,
    const SetSources& sources) {
  std::vector<SourceDependency> dependencies{
      {.source = program.path, .source_sha256 = HashOf(program)}};
  const auto bound = BoundModels(file, program, sources);
  for (std::size_t index = 0; index < file.entries.size(); ++index) {
    const EsqEntry& entry = file.entries[index];
    if (bound[index].empty()) {
      return std::unexpected(
          std::format("{}: entry {} (selector {}) binds no model files.tsv "
                      "lists",
                      set.path, index, entry.selector));
    }
    for (const SetModel* model : bound[index]) {
      if (model->bones != entry.animation.bones) {
        return std::unexpected(std::format(
            "{}: entry {} (selector {}) animates {} bones, but {} has {}",
            set.path, index, entry.selector, entry.animation.bones, model->path,
            model->bones));
      }
      dependencies.push_back(
          {.source = model->path, .source_sha256 = model->source_sha256});
    }
    if (entry.texture_row && !sources.models.contains(SelectorOf(entry))) {
      return std::unexpected(
          std::format("{}: entry {} gives texture row {} to selector {}, "
                      "which is no model number files.tsv lists",
                      set.path, index, *entry.texture_row, entry.selector));
    }
  }
  std::ranges::sort(dependencies, {}, &SourceDependency::source);
  return dependencies;
}

Result<AnimationSetDocument> DocumentOf(
    const SourceFile& set, std::span<const std::uint8_t> event_lengths) {
  auto file = Content::DecodeEsqFile(set.bytes, event_lengths);
  if (!file) {
    return std::unexpected(std::format("{}: {}", set.path, file.error()));
  }
  return AnimationSetDocument{.source = set.path,
                              .source_sha256 = HashOf(set),
                              .file = std::move(*file)};
}

Result<EsqConversion> PlanConversion(const SourceFile& set,
                                     const SourceFile& program,
                                     const SetSources& sources,
                                     const SeqEventTable& events) {
  auto document = DocumentOf(set, events.lengths);
  if (!document) return std::unexpected(document.error());
  if (auto unique = CheckSelectors(set, document->file); !unique) {
    return std::unexpected(unique.error());
  }
  auto dependencies = SetDependencies(set, document->file, program, sources);
  if (!dependencies) return std::unexpected(dependencies.error());
  return EsqConversion{
      .set = set, .events = events, .dependencies = std::move(*dependencies)};
}

// The manifest entry `conversion` makes, without its file.
Result<ConversionOutput> SetOutputOf(const EsqConversion& conversion) {
  auto id = AnimationSetIdForDiscPath(conversion.set.path);
  if (!id) return std::unexpected(id.error());
  return ConversionOutput{
      .id = std::move(*id),
      .source = conversion.set.path,
      .source_sha256 = HashOf(conversion.set),
      .converter = std::string(Content::kEsqAnimationSetConverterName),
      .files = {},
      .dependencies = conversion.dependencies,
      .unreferenced = false,
      .note = {},
      .alias_of = {}};
}

// The file must read back to `document` and rebuild the source bytes.
Check VerifySet(const AnimationSetDocument& document, const ImageFile& file,
                const EsqConversion& conversion) {
  const std::string& path = conversion.set.path;
  auto json = Content::ParseJsonText(file.bytes, file.name);
  if (!json) {
    return std::unexpected(std::format("{}: the converted set is not JSON: {}",
                                       path, json.error()));
  }
  auto read = Content::AnimationSetOf(*json, file.name);
  if (!read) {
    return std::unexpected(std::format(
        "{}: the converted set does not read back: {}", path, read.error()));
  }
  if (*read != document) {
    return std::unexpected(std::format(
        "{}: the converted set reads back to a different set", path));
  }
  auto encoded = Content::EncodeEsqFile(read->file, conversion.events.lengths);
  if (!encoded || *encoded != conversion.set.bytes) {
    return std::unexpected(std::format(
        "{}: the converted set does not rebuild the source bytes", path));
  }
  return {};
}

}  // namespace

std::expected<std::string, std::string> AnimationSetIdForDiscPath(
    std::string_view disc_path) {
  if (!DiscStemOf(disc_path, Content::kEsqExtension)) {
    return std::unexpected(
        std::format("'{}' is not an ESQ animation set", disc_path));
  }
  auto image = ImageIdForDiscPath(disc_path);
  if (!image) return std::unexpected(image.error());
  // image.<directories>.<stem> -> animation-set.<directories>.<stem>
  std::string id =
      std::format("{}{}", kAnimationSetIdPrefix,
                  std::string_view(*image).substr(image->find('.')));
  if (!Content::IsContentId(id)) {
    return std::unexpected(
        std::format("'{}' gives the animation set id '{}', which is not valid",
                    disc_path, id));
  }
  return id;
}

std::expected<ConvertedModel, std::string> ConvertEsqAnimationSet(
    const EsqConversion& conversion) {
  auto document = DocumentOf(conversion.set, conversion.events.lengths);
  if (!document) return std::unexpected(document.error());
  const auto stem = DiscStemOf(conversion.set.path, Content::kEsqExtension);
  if (!stem) {
    return std::unexpected(
        std::format("'{}' is not an ESQ animation set", conversion.set.path));
  }
  const std::string text =
      Content::FormatJsonText(Content::AnimationSetJson(*document)) + "\n";
  const auto bytes = std::as_bytes(std::span(text));
  ImageFile file{.name = std::format("{}{}", *stem, kJsonSuffix),
                 .bytes = {bytes.begin(), bytes.end()}};
  if (auto verified = VerifySet(*document, file, conversion); !verified) {
    return std::unexpected(verified.error());
  }
  auto output = SetOutputOf(conversion);
  if (!output) return std::unexpected(output.error());
  std::vector<ImageFile> files;
  files.push_back(std::move(file));
  return ConvertedModelOf(std::move(*output), std::move(files),
                          Content::kAnimationSetDirectory);
}

std::expected<ModelConversionReport, std::string> ConvertDiscAnimationSets(
    const fs::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto sources = ReadSetSources(root, start->entries);
  if (!sources) return std::unexpected(sources.error());
  std::vector<ModelJob> jobs;
  if (!sources->sets.empty()) {
    if (!sources->program) {
      return std::unexpected(
          std::format("{} needs {}, which files.tsv does not list",
                      sources->sets.front().path, kBattleProgramPath));
    }
    const SourceFile& program = *sources->program;
    auto events = EventsOf(program);
    if (!events) return std::unexpected(events.error());
    if (auto named = CheckSetTables(program, *sources); !named) {
      return std::unexpected(named.error());
    }
    for (const SourceFile& set : sources->sets) {
      auto conversion = PlanConversion(set, program, *sources, *events);
      if (!conversion) return std::unexpected(conversion.error());
      auto expected = SetOutputOf(*conversion);
      if (!expected) return std::unexpected(expected.error());
      jobs.push_back({.expected = std::move(*expected),
                      .companions = {},
                      .convert = [conversion = std::move(*conversion)] {
                        return ConvertEsqAnimationSet(conversion);
                      }});
    }
  }
  return RunModelConverter(
      root, *start,
      {.name = Content::kEsqAnimationSetConverterName,
       .version = Content::kEsqAnimationSetConverterVersion,
       .directory = Content::kAnimationSetDirectory,
       .force = force,
       .companion = std::nullopt},
      jobs);
}

}  // namespace VS::Extract
