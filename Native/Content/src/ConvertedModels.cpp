// Converted character and weapon models the runtime loads
// (ConvertedModels.hpp).
#include "Content/ConvertedModels.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/AnimationSet.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/ConvertedImages.hpp"
#include "Content/DataStore.hpp"
#include "Content/EsqFile.hpp"
#include "Content/EtmFile.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/NativeImage.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Content/WepFile.hpp"
#include "Content/WepModelFiles.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
namespace fs = std::filesystem;

constexpr std::string_view kJsonSuffix = ".json";

struct ModelFileLocation {
  std::filesystem::path directory;
  std::string stem;
};

// The directory and stem of the one glTF file an output lists.
std::expected<ModelFileLocation, std::string> ModelFilesOf(
    const DataStore& store, const ConvertedSource& source) {
  std::string_view gltf;
  for (const std::string& file : source.files->files) {
    if (!file.ends_with(kGltfSuffix)) continue;
    if (!gltf.empty()) {
      return std::unexpected(
          std::format("{} has multiple glTF models", source.entry->path));
    }
    gltf = file;
  }
  if (gltf.empty()) {
    return std::unexpected(
        std::format("{} has no glTF model", source.entry->path));
  }
  const std::string_view stem_path =
      gltf.substr(0, gltf.size() - kGltfSuffix.size());
  const std::size_t separator = stem_path.rfind('/');
  ModelFileLocation location{
      .directory = store.Root(),
      .stem = std::string(stem_path.substr(
          separator == std::string_view::npos ? 0 : separator + 1))};
  if (separator != std::string_view::npos) {
    location.directory /= Platform::Utf8Path(stem_path.substr(0, separator));
  }
  return location;
}

struct ModelKind {
  std::string_view extension;  // the disc file's
  std::string_view converter;
};

// The output a read names and where its glTF model is.
struct FoundModel {
  ConvertedSource source;
  ModelFileLocation location;
};

std::expected<FoundModel, std::string> FindModel(const DataStore& store,
                                                 const ConvertedModelRead& read,
                                                 const ModelKind& kind) {
  auto found = FindConvertedSource(store, {.lba = read.lba,
                                           .rounded_size = read.rounded_size,
                                           .extension = kind.extension,
                                           .converter = kind.converter});
  if (!found) return std::unexpected(found.error());
  auto location = ModelFilesOf(store, *found);
  if (!location) return std::unexpected(location.error());
  return FoundModel{.source = *found, .location = std::move(*location)};
}

// Whether the document `source`'s output read back to is the one the manifest
// records.
template <typename Document>
std::expected<void, std::string> CheckRecorded(const ConvertedSource& source,
                                               const Document& document) {
  if (document.id != source.files->id ||
      document.source != source.files->source ||
      document.source_sha256 != source.conversion->source_sha256) {
    return std::unexpected(std::format(
        "{} does not match its conversion manifest", source.entry->path));
  }
  return {};
}

// The disc file's bytes `encode` rebuilds from the document `source`'s output
// read back to.
template <typename Document, typename Encode>
std::expected<std::vector<std::byte>, std::string> RebuiltBytes(
    const ConvertedSource& source, const Document& document, Encode encode) {
  if (auto recorded = CheckRecorded(source, document); !recorded) {
    return std::unexpected(recorded.error());
  }
  auto bytes = encode(document);
  if (!bytes) {
    return std::unexpected(
        std::format("{}: {}", source.entry->path, bytes.error()));
  }
  return bytes;
}

// A file of a decoded model as it was when the model was read.
struct FileStamp {
  fs::path path;
  fs::file_time_type written;
  std::uintmax_t size = 0;

  bool operator==(const FileStamp&) const = default;
};

// The stamps of the files `output` lists, or nothing when one cannot be read.
std::optional<std::vector<FileStamp>> StampsOf(const DataStore& store,
                                               const ConversionOutput& output) {
  std::vector<FileStamp> stamps;
  for (const std::string& file : output.files) {
    FileStamp stamp{.path = store.Root() / Platform::Utf8Path(file),
                    .written = {},
                    .size = 0};
    std::error_code error;
    stamp.written = fs::last_write_time(stamp.path, error);
    if (!error) stamp.size = fs::file_size(stamp.path, error);
    if (error) return std::nullopt;
    stamps.push_back(std::move(stamp));
  }
  return stamps;
}

// The last few character models decoded. Decoding reads and checks a model's
// whole glTF with every bank it owns, so a model whose banks load one after
// another decodes once while its files are unchanged.
class ShpModelCache {
 public:
  std::expected<std::shared_ptr<const ShpModelDocument>, std::string> Load(
      const DataStore& store, const ConversionOutput& files,
      const ModelFileLocation& location,
      std::span<const std::uint8_t> event_lengths) {
    std::optional<std::vector<FileStamp>> stamps = StampsOf(store, files);
    const std::scoped_lock lock(mutex_);
    if (stamps) {
      const auto found =
          std::ranges::find_if(models_, [&](const CachedModel& model) {
            return model.stamps == *stamps &&
                   std::ranges::equal(model.event_lengths, event_lengths);
          });
      if (found != models_.end()) {
        std::rotate(models_.begin(), found, std::next(found));
        return models_.front().document;
      }
    }
    auto decoded =
        LoadShpModelFiles(location.directory, location.stem, event_lengths);
    if (!decoded) return std::unexpected(decoded.error());
    auto document =
        std::make_shared<const ShpModelDocument>(std::move(*decoded));
    if (stamps) {
      models_.insert(models_.begin(), {.stamps = std::move(*stamps),
                                       .event_lengths = {event_lengths.begin(),
                                                         event_lengths.end()},
                                       .document = document});
      if (models_.size() > kCachedModels) models_.pop_back();
    }
    return document;
  }

 private:
  static constexpr std::size_t kCachedModels = 4;

  struct CachedModel {
    std::vector<FileStamp> stamps;
    std::vector<std::uint8_t> event_lengths;
    std::shared_ptr<const ShpModelDocument> document;
  };

  std::mutex mutex_;
  std::vector<CachedModel> models_;  // the most recently used first
};

std::expected<std::shared_ptr<const ShpModelDocument>, std::string>
LoadShpModel(const DataStore& store, const ConvertedSource& source,
             const ModelFileLocation& location,
             std::span<const std::uint8_t> event_lengths) {
  static ShpModelCache cache;
  auto document = cache.Load(store, *source.files, location, event_lengths);
  if (!document) {
    return std::unexpected(
        std::format("{}: {}", source.entry->path, document.error()));
  }
  return document;
}

// The shp-gltf output of the model whose glTF holds `bank`: the bank output's
// one SHP dependency, which must be at the hash the model output records.
std::expected<ConvertedSource, std::string> OwnerOf(
    const DataStore& store, const ConvertedSource& bank) {
  const std::string& path = bank.entry->path;
  const SourceDependency* owner = nullptr;
  for (const SourceDependency& dependency : bank.conversion->dependencies) {
    if (!dependency.source.ends_with(kShpExtension)) continue;
    if (owner != nullptr) {
      return std::unexpected(
          std::format("{} names more than one owner model", path));
    }
    owner = &dependency;
  }
  if (owner == nullptr) {
    return std::unexpected(std::format("{} names no owner model", path));
  }
  const ConversionManifest* const manifest = store.Conversions();
  if (manifest == nullptr) {
    return std::unexpected("no conversion manifest is loaded");
  }
  const DataStore::Entry* const entry = store.Find(owner->source);
  const auto model = std::ranges::find_if(
      manifest->outputs, [&](const ConversionOutput& output) {
        return output.converter == kShpModelConverterName &&
               output.source == owner->source;
      });
  if (entry == nullptr || model == manifest->outputs.end() ||
      model->source_sha256 != owner->source_sha256) {
    return std::unexpected(
        std::format("{}: its owner {} has no {} output at the recorded hash",
                    path, owner->source, kShpModelConverterName));
  }
  const ConversionOutput* const files = manifest->FilesOf(*model);
  if (files == nullptr) {
    return std::unexpected(std::format("{}: alias target {} is not registered",
                                       owner->source, model->alias_of));
  }
  return ConvertedSource{.entry = entry, .conversion = &*model, .files = files};
}

// The depth the model `region` names draws its texture at: a model that
// borrows a texture draws it at 8 bits (ActorAssets.cpp BorrowActorTexture),
// one with its own at that texture's depth. The model loads from its glTF, so
// an edit to the model counts.
std::expected<ImageDepth, std::string> RegionModelDepth(
    const DataStore& store, const ConvertedModelRead& read,
    const ImageDocument& region) {
  const auto named = [](const ImageDependency& dependency) {
    return dependency.role == kEtmRegionModelRole;
  };
  const auto& dependencies = region.provenance.dependencies;
  const auto model = std::ranges::find_if(dependencies, named);
  if (model == dependencies.end()) {
    return std::unexpected(std::format("{} names no model", region.id));
  }
  if (std::ranges::count_if(dependencies, named) != 1) {
    return std::unexpected(
        std::format("{} names more than one model", region.id));
  }
  const DataStore::Entry* const entry = store.Find(model->source);
  if (entry == nullptr) {
    return std::unexpected(
        std::format("{}: its model {} is not in the extracted data", region.id,
                    model->source));
  }
  auto found = FindModel(
      store,
      {.lba = entry->lba,
       .rounded_size =
           std::uint64_t{entry->SectorCount()} * DataStore::kDataSectorBytes,
       .event_lengths = read.event_lengths},
      {.extension = kShpExtension, .converter = kShpModelConverterName});
  if (!found) return std::unexpected(found.error());
  auto document =
      LoadShpModel(store, found->source, found->location, read.event_lengths);
  if (!document) return std::unexpected(document.error());
  const ShpFile& shp = (*document)->shp;
  return shp.lender.has_value() ||
                 (shp.texture.has_value() && shp.texture->Is8Bit())
             ? ImageDepth::kIndexed8
             : ImageDepth::kIndexed4;
}

}  // namespace

std::expected<std::vector<std::byte>, std::string> LoadConvertedModel(
    const DataStore& store, const ConvertedModelRead& read) {
  auto found = FindModel(
      store, read,
      {.extension = kShpExtension, .converter = kShpModelConverterName});
  if (!found) return std::unexpected(found.error());
  auto document =
      LoadShpModel(store, found->source, found->location, read.event_lengths);
  if (!document) return std::unexpected(document.error());
  return RebuiltBytes(
      found->source, **document,
      [](const ShpModelDocument& model) { return EncodeShp(model.shp); });
}

std::expected<std::vector<std::byte>, std::string> LoadConvertedWeapon(
    const DataStore& store, const ConvertedModelRead& read) {
  auto found = FindModel(
      store, read,
      {.extension = kWepExtension, .converter = kWepModelConverterName});
  if (!found) return std::unexpected(found.error());
  auto document =
      LoadWepModelFiles(found->location.directory, found->location.stem);
  if (!document) {
    return std::unexpected(
        std::format("{}: {}", found->source.entry->path, document.error()));
  }
  return RebuiltBytes(
      found->source, *document,
      [](const WepModelDocument& weapon) { return EncodeWep(weapon.wep); });
}

std::expected<std::vector<std::byte>, std::string> LoadConvertedSequence(
    const DataStore& store, const ConvertedModelRead& read) {
  auto bank =
      FindConvertedSource(store, {.lba = read.lba,
                                  .rounded_size = read.rounded_size,
                                  .extension = kSeqExtension,
                                  .converter = kSeqAnimationConverterName});
  if (!bank) return std::unexpected(bank.error());
  const std::string& path = bank->entry->path;
  auto owner = OwnerOf(store, *bank);
  if (!owner) return std::unexpected(owner.error());
  auto location = ModelFilesOf(store, *owner);
  if (!location) return std::unexpected(location.error());
  auto listed = ModelFilesOf(store, *bank);
  if (!listed) return std::unexpected(listed.error());
  if (listed->directory != location->directory ||
      listed->stem != location->stem) {
    return std::unexpected(
        std::format("{} lists a glTF model that its owner {} does not", path,
                    owner->entry->path));
  }
  auto document = LoadShpModel(store, *owner, *location, read.event_lengths);
  if (!document) return std::unexpected(document.error());
  if (auto recorded = CheckRecorded(*owner, **document); !recorded) {
    return std::unexpected(recorded.error());
  }
  const auto& sequences = (*document)->sequences;
  const auto found =
      std::ranges::find_if(sequences, [&](const ActorAnimationDocument& owned) {
        return owned.source == path &&
               owned.source_sha256 == bank->conversion->source_sha256;
      });
  if (found == sequences.end()) {
    return std::unexpected(
        std::format("{}'s model holds no {} at the hash its output records",
                    owner->entry->path, path));
  }
  auto bytes = EncodeSeqFile(found->file, read.event_lengths);
  if (!bytes) {
    return std::unexpected(std::format("{}: {}", path, bytes.error()));
  }
  return bytes;
}

std::expected<std::vector<std::byte>, std::string> LoadConvertedAnimationSet(
    const DataStore& store, const ConvertedModelRead& read) {
  auto set =
      FindConvertedSource(store, {.lba = read.lba,
                                  .rounded_size = read.rounded_size,
                                  .extension = kEsqExtension,
                                  .converter = kEsqAnimationSetConverterName});
  if (!set) return std::unexpected(set.error());
  const std::string& path = set->entry->path;
  const std::string* json = nullptr;
  for (const std::string& file : set->files->files) {
    if (!file.ends_with(kJsonSuffix)) continue;
    if (json != nullptr) {
      return std::unexpected(
          std::format("{} has more than one animation set file", path));
    }
    json = &file;
  }
  if (json == nullptr) {
    return std::unexpected(std::format("{} has no animation set file", path));
  }
  const ImageFileBytes text = DirectoryFileReader(store.Root())(*json);
  if (!text) {
    return std::unexpected(std::format("{}: {} cannot be read", path, *json));
  }
  auto parsed = ParseJsonText(*text, *json);
  if (!parsed) return std::unexpected(parsed.error());
  auto document = AnimationSetOf(*parsed, *json);
  if (!document) return std::unexpected(document.error());
  if (document->source != set->conversion->source ||
      document->source_sha256 != set->conversion->source_sha256) {
    return std::unexpected(
        std::format("{} does not match its conversion manifest", path));
  }
  auto bytes = EncodeEsqFile(document->file, read.event_lengths);
  if (!bytes) {
    return std::unexpected(std::format("{}: {}", *json, bytes.error()));
  }
  return bytes;
}

std::expected<std::vector<std::byte>, std::string> LoadConvertedActorTexture(
    const DataStore& store, const ConvertedModelRead& read) {
  auto sheet =
      FindConvertedSource(store, {.lba = read.lba,
                                  .rounded_size = read.rounded_size,
                                  .extension = kEtmExtension,
                                  .converter = kEtmTextureConverterName});
  if (!sheet) return std::unexpected(sheet.error());
  const ConversionOutput& files = *sheet->files;
  std::vector<ImageDocument> regions;
  for (const std::string& file : files.files) {
    if (!file.ends_with(kImageSidecarSuffix)) continue;
    // Each region's files are <directory>/<sheet>/<model>.*; the image reader
    // takes the leaf stem in its own directory.
    const std::filesystem::path stem(std::string_view(file).substr(
        0, file.size() - kImageSidecarSuffix.size()));
    auto region = DecodeImageFiles(
        stem.filename().string(),
        DirectoryFileReader(store.Root() / stem.parent_path()));
    if (!region) return std::unexpected(region.error());
    if (region->provenance.source_sha256 != files.source_sha256) {
      return std::unexpected(std::format(
          "{} does not match its conversion manifest", sheet->entry->path));
    }
    auto depth = RegionModelDepth(store, read, *region);
    if (!depth) return std::unexpected(depth.error());
    if (region->image.depth != *depth) {
      return std::unexpected(std::format(
          "{} is {} bits deep, but its model draws its texture at {} bits",
          region->id, BitsPerPixel(region->image.depth), BitsPerPixel(*depth)));
    }
    regions.push_back(std::move(*region));
  }
  auto rebuilt = EtmSheetOfImages(regions, files.source);
  if (!rebuilt) return std::unexpected(rebuilt.error());
  auto bytes = EncodeEtmFile(*rebuilt);
  if (!bytes) {
    return std::unexpected(
        std::format("{}: {}", sheet->entry->path, bytes.error()));
  }
  return bytes;
}

}  // namespace VS::Content
