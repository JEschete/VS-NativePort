#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/AssetCatalogImport.hpp"
#include "Content/AudioBank.hpp"
#include "Content/AudioStream.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Content/MovieEncoder.hpp"
#include "Core/Log.hpp"
#include "Core/Sha256.hpp"
#include "Extract/AudioBankConversion.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/EsqConversion.hpp"
#include "Extract/EtmConversion.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "Extract/WepModelConversion.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"
#include "Platform/App.hpp"
#include "Platform/Paths.hpp"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;
using VS::u32;
using VS::u64;
using VS::u8;
using VS::usize;
namespace Content = VS::Content;
namespace Extract = VS::Extract;
namespace Platform = VS::Platform;

namespace {

constexpr int kMinimumArguments = 3;
constexpr int kExtractArguments = 4;
constexpr int kOutputArgument = 3;
constexpr int kCatalogArguments = 3;
constexpr int kImageArguments = 3;
constexpr int kForcedImageArguments = 4;

int Usage() {
  std::puts(
      "usage:\n"
      "  VS.Extract list    <disc.cue|disc.chd|folder>\n"
      "  VS.Extract streams <disc.cue|disc.chd|folder> <outdir> [--ffmpeg "
      "<ffmpeg.exe>]\n"
      "  VS.Extract extract <disc.cue|disc.chd|folder> <outdir> [--ffmpeg "
      "<ffmpeg.exe>]\n"
      "  VS.Extract catalogs <datadir>\n"
      "  VS.Extract images   <datadir> [--force]\n"
      "  VS.Extract tables   <datadir> [--force]\n"
      "  VS.Extract models   <datadir> [--force]\n"
      "  VS.Extract banks    <datadir> [--force]\n"
      "      Form 1 files are copied as they are; movies (*.STR) are converted "
      "to Matroska (AV1 + FLAC)\n"
      "      with FFmpeg (found on PATH, or --ffmpeg). Effect audio and XA "
      "are imported under audio/streams as lossless PCM assets.\n"
      "      Movies and XA tracks are recorded in manifest.json and kept when "
      "their sectors are unchanged.\n"
      "      extract also writes catalogs/ (asset ids and the audio tables); "
      "catalogs rewrites\n"
      "      them from an extracted folder.\n"
      "      extract also converts standalone TIM images and effect FBC "
      "palettes to PNG under textures/ and records them\n"
      "      in manifest.json; images converts again only what changed "
      "(--force: everything).\n"
      "      extract also converts the workshop recipe files (MENU/*.SYD) to "
      "CSV tables under catalogs/menu/;\n"
      "      tables converts again only what changed (--force: everything, "
      "replacing edited tables).\n"
      "      extract also converts the character models (OBJ/*.SHP) to glTF "
      "under models/characters/ and the\n"
      "      weapon models (OBJ/*.WEP) to glTF under models/weapons/, the "
      "shared animation sets\n"
      "      (OBJ/E*.ESQ) to JSON under models/animation-sets/ and the actor "
      "effect texture sheets\n"
      "      (OBJ/E*.ETM) to PNG under textures/actors/;\n"
      "      models converts again only what changed (--force: everything, "
      "replacing edited models).\n"
      "      extract also converts the sound banks (SOUND/WAVE*.DAT) to WAV "
      "and SFZ under audio/banks/;\n"
      "      banks converts again only what changed (--force: everything).");
  return 2;
}

// --ffmpeg <path>, else ffmpeg.exe on PATH.
std::optional<fs::path> FindFfmpeg(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == "--ffmpeg") {
      return Platform::Utf8Path(argv[i + 1]);
    }
  }
  const char* path = std::getenv("PATH");
  std::string_view dirs = path ? path : "";
  std::error_code ec;
  while (!dirs.empty()) {
    const usize sep = dirs.find(';');
    const fs::path candidate =
        fs::path(std::string(dirs.substr(0, sep))) / "ffmpeg.exe";
    if (!candidate.parent_path().empty() && fs::exists(candidate, ec)) {
      return candidate;
    }
    dirs = sep == std::string_view::npos ? std::string_view{}
                                         : dirs.substr(sep + 1);
  }
  return std::nullopt;
}

// An interleaved file's raw 2352-byte sectors: what its manifest hash covers.
std::optional<std::vector<u8>> ReadRawFile(const Content::Disc& disc,
                                           const Content::DiscEntry& file) {
  const u64 sectors =
      (static_cast<u64>(file.size) + Content::Disc::SectorData - 1) /
      Content::Disc::SectorData;
  std::vector<u8> raw(static_cast<usize>(sectors) * Content::Disc::SectorRaw);
  if (auto result =
          disc.ReadRawSectors(file.lba, static_cast<u32>(sectors), raw);
      !result) {
    VS::Core::LogError("extract", "{}: {}", file.path, result.error());
    return std::nullopt;
  }
  return raw;
}

bool WriteBytes(std::span<const u8> bytes, const fs::path& tmp) {
  fs::create_directories(tmp.parent_path());
  std::ofstream stream(tmp, std::ios::binary);
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  stream.close();
  if (!stream) {
    VS::Core::LogError("extract", "cannot write {}", Platform::PathText(tmp));
    return false;
  }
  return true;
}

// TITLE.STR -> title
std::string LowerStem(std::string_view path) {
  std::string_view stem = path.substr(path.rfind('/') + 1);
  stem = stem.substr(0, stem.find('.'));
  std::string lower;
  for (const char letter : stem) {
    lower.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
  }
  return lower;
}

// The manifest entry for an interleaved file, without its files: the source
// hash covers its raw sectors, as no copy of it is under disc\.
Content::ConversionOutput InterleavedOutput(std::string id,
                                            std::string_view converter,
                                            const Content::DiscEntry& file,
                                            std::span<const u8> raw) {
  return {.id = std::move(id),
          .source = file.path,
          .source_sha256 = VS::Core::Sha256Hex(std::as_bytes(raw)),
          .converter = std::string(converter),
          .files = {},
          .dependencies = {},
          .unreferenced = false,
          .note = {},
          .alias_of = {}};
}

// Preserve the movie's decoded audio losslessly; video uses AV1 CRF 18.
bool ConvertMovie(const fs::path& ffmpeg, const Content::Av1Encoder& encoder,
                  const Content::DiscEntry& file, std::span<const u8> raw,
                  const fs::path& mkv) {
  fs::path tmp = mkv;
  tmp += L".raw";
  if (!WriteBytes(raw, tmp)) return false;
  std::vector<std::string> arguments{Platform::PathText(ffmpeg),
                                     "-hide_banner",
                                     "-loglevel",
                                     "error",
                                     "-y",
                                     "-f",
                                     "psxstr",
                                     "-i",
                                     Platform::PathText(tmp),
                                     "-map",
                                     "0:v",
                                     "-map",
                                     "0:a?",
                                     "-c:v",
                                     encoder.name};
  arguments.insert(arguments.end(), encoder.options.begin(),
                   encoder.options.end());
  for (const auto* setting : {"-pix_fmt", "yuv420p", "-color_range", "pc",
                              "-colorspace", "bt470bg", "-c:a", "flac"}) {
    arguments.emplace_back(setting);
  }
  arguments.push_back(Platform::PathText(mkv));
  const auto result = Platform::RunProcess(arguments);
  std::error_code ec;
  fs::remove(tmp, ec);
  if (!result || *result != 0 || !fs::exists(mkv, ec)) {
    VS::Core::LogError("extract", "{}: FFmpeg failed ({})", file.path,
                       result ? std::to_string(*result) : result.error());
    return false;
  }
  VS::Core::LogInfo("extract", "{} -> {} ({} bytes)", file.path,
                    Platform::PathText(mkv), fs::file_size(mkv, ec));
  return true;
}

// Converts or keeps one movie, adding its output to `batch`. Whether it has
// an output: without an encoder an unchanged earlier conversion is still
// kept, otherwise the movie is only listed. nullopt on a logged failure.
std::optional<bool> ConvertOrKeepMovie(
    const Content::Disc& disc, const fs::path& out,
    const std::optional<fs::path>& ffmpeg,
    const std::optional<Content::Av1Encoder>& encoder,
    const Content::ConversionManifest& previous, const Content::DiscEntry& file,
    Extract::ConverterBatch& batch) {
  const auto raw = ReadRawFile(disc, file);
  if (!raw) return std::nullopt;
  auto output = InterleavedOutput("movie." + LowerStem(file.path),
                                  Content::kStrMkvConverterName, file, *raw);
  const Content::ConversionOutput* kept =
      batch.reuse ? Extract::MatchingOutput(previous, out, output) : nullptr;
  if (kept != nullptr) {
    VS::Core::LogInfo("extract", "{}: unchanged", file.path);
    batch.outputs.push_back(*kept);
    return true;
  }
  if (!ffmpeg || !encoder) return false;
  std::string mkv = std::format("disc/{}.{}", file.path, Content::kMovieForm);
  if (!ConvertMovie(*ffmpeg, *encoder, file, *raw,
                    out / Platform::Utf8Path(mkv))) {
    return std::nullopt;
  }
  output.files = {std::move(mkv)};
  batch.outputs.push_back(std::move(output));
  return true;
}

bool ConvertXa(const fs::path& ffmpeg, const Content::DiscEntry& file,
               std::span<const u8> raw, const fs::path& flac) {
  fs::path tmp = flac;
  tmp += L".raw";
  const auto selected = Content::SelectXaAudioSectors(raw);
  if (!selected) {
    VS::Core::LogError("extract", "{}: {}", file.path, selected.error());
    return false;
  }
  if (!WriteBytes(*selected, tmp)) return false;
  const auto arguments = std::to_array<std::string>(
      {Platform::PathText(ffmpeg), "-hide_banner", "-loglevel", "error", "-y",
       "-f", "psxstr", "-i", Platform::PathText(tmp), "-map", "0:a:0", "-c:a",
       "flac", Platform::PathText(flac)});
  const auto result = Platform::RunProcess(arguments);
  std::error_code error;
  fs::remove(tmp, error);
  if (!result || *result != 0 || !fs::exists(flac, error)) {
    VS::Core::LogError("extract", "{}: XA conversion failed ({})", file.path,
                       result ? std::to_string(*result) : result.error());
    return false;
  }
  VS::Core::LogInfo("extract", "{} -> {} (lossless decoded XA FLAC)", file.path,
                    Platform::PathText(flac));
  return true;
}

// audio/banks/ and their manifest.json outputs from the disc files already
// under root.
int ConvertBanks(const fs::path& root, bool force) {
  const auto report = Extract::ConvertDiscBanks(root, force);
  if (!report) {
    VS::Core::LogError("extract", "{}", report.error());
    return 1;
  }
  VS::Core::LogInfo("extract",
                    "banks: {} converted, {} unchanged, {} placeholders "
                    "skipped, {} stale files removed",
                    report->converted, report->unchanged, report->skipped,
                    report->removed);
  return 0;
}

// Converts or keeps one XA track, adding its output to `batch`.
bool ConvertOrKeepXa(const Content::Disc& disc, const fs::path& out,
                     const std::optional<fs::path>& ffmpeg,
                     const Content::ConversionManifest& previous,
                     const Content::DiscEntry& file,
                     Extract::ConverterBatch& batch) {
  const auto raw = ReadRawFile(disc, file);
  if (!raw) return false;
  auto output = InterleavedOutput("audio.xa." + LowerStem(file.path),
                                  Content::kXaFlacConverterName, file, *raw);
  const Content::ConversionOutput* kept =
      batch.reuse ? Extract::MatchingOutput(previous, out, output) : nullptr;
  if (kept != nullptr) {
    VS::Core::LogInfo("extract", "{}: unchanged", file.path);
    batch.outputs.push_back(*kept);
    return true;
  }
  if (!ffmpeg) {
    VS::Core::LogError("extract", "{}: FFmpeg is required for XA conversion",
                       file.path);
    return false;
  }
  if (!ConvertXa(*ffmpeg, file, *raw, Content::XaAudioPath(out, file.path))) {
    return false;
  }
  output.files = {Content::XaAudioFile(file.path)};
  batch.outputs.push_back(std::move(output));
  return true;
}

int ConvertStreams(const Content::Disc& disc, const fs::path& out,
                   const std::optional<fs::path>& ffmpeg) {
  const auto loaded = Content::LoadConversionManifest(out);
  if (!loaded) {
    VS::Core::LogError("extract", "{}", loaded.error());
    return 1;
  }
  const Content::ConversionManifest previous =
      loaded->value_or(Content::ConversionManifest{});
  Extract::ConverterBatch tracks =
      Extract::StartBatch(previous, Content::kXaFlacConverterName,
                          Content::kXaFlacConverterVersion, false);
  nlohmann::json entries = nlohmann::json::array();
  nlohmann::json xa = nlohmann::json::array();
  for (const auto& file : disc.Files()) {
    if (!file.directory && file.path.ends_with(Content::kXaExtension)) {
      if (!ConvertOrKeepXa(disc, out, ffmpeg, previous, file, tracks)) {
        return 1;
      }
      xa.push_back({{"source", file.path},
                    {"lba", file.lba},
                    {"source_bytes", file.size},
                    {"file", 1},
                    {"channel", 0},
                    {"sectors_per_second", Content::kXaSectorsPerSecond},
                    {"asset", Content::XaAudioFile(file.path)}});
      continue;
    }
    if (file.directory || !file.path.starts_with("EFFECT/") ||
        !file.path.ends_with(".P")) {
      continue;
    }
    auto bytes = disc.ReadFile(file);
    if (!bytes) {
      VS::Core::LogError("extract", "{}", bytes.error());
      return 1;
    }
    auto streams = Content::FindEffectAudioStreams(*bytes);
    if (!streams) {
      VS::Core::LogError("extract", "{}: {}", file.path, streams.error());
      return 1;
    }
    for (const auto& source : *streams) {
      auto stream = Content::DecodeAudioStream(source.bytes);
      if (!stream) {
        VS::Core::LogError("extract", "{} at {}: {}", file.path, source.offset,
                           stream.error());
        return 1;
      }
      if (auto result = Content::ExportAudioStream(*stream, out); !result) {
        VS::Core::LogError("extract", "{}: {}", file.path, result.error());
        return 1;
      }
      entries.push_back({{"source", file.path},
                         {"offset", source.offset},
                         {"bytes", source.bytes.size()},
                         {"id", stream->info.id}});
      VS::Core::LogInfo("extract", "{}: stream {}, {} channels, {} frames",
                        file.path, stream->info.id, stream->info.channels,
                        stream->info.frames);
    }
  }
  if (entries.empty() && xa.empty()) {
    VS::Core::LogError("extract",
                       "no effect-package or XA audio streams found");
    return 1;
  }
  std::ofstream index(out / "audio" / "streams" / "index.json");
  index << nlohmann::json{{"version", 1}, {"streams", entries}, {"xa", xa}}
               .dump(2)
        << '\n';
  index.close();
  if (!index) {
    VS::Core::LogError("extract", "writing stream provenance index");
    return 1;
  }
  const auto removed =
      Extract::PublishBatches(out, previous, std::span(&tracks, 1));
  if (!removed) {
    VS::Core::LogError("extract", "{}", removed.error());
    return 1;
  }
  VS::Core::LogInfo("extract", "converted {} effect streams and {} XA tracks",
                    entries.size(), xa.size());
  return 0;
}

int ListDisc(const Content::Disc& disc) {
  for (const auto& file : disc.Files()) {
    std::printf("%8u %10u %c%c %s\n", file.lba, file.size,
                file.directory ? 'd' : '-', file.interleaved ? 'x' : '-',
                file.path.c_str());
  }
  std::printf("%zu entries, label '%s'\n", disc.Files().size(),
              std::string(disc.Label()).c_str());
  return 0;
}

// catalogs/ from the manifest and executable already under root.
int WriteCatalogs(const fs::path& root) {
  const auto written = Content::WriteAssetCatalogs(root);
  if (!written) {
    VS::Core::LogError("extract", "{}", written.error());
    return 1;
  }
  VS::Core::LogInfo("extract", "wrote {} with {} assets",
                    Platform::PathText(root / "catalogs"), *written);
  return 0;
}

// textures/ and manifest.json from the disc files already under root.
int ConvertImages(const fs::path& root, bool force) {
  const auto report = Extract::ConvertDiscImages(root, force);
  if (!report) {
    VS::Core::LogError("extract", "{}", report.error());
    return 1;
  }
  VS::Core::LogInfo("extract",
                    "images: {} converted, {} unchanged, {} aliased, {} stale "
                    "files removed",
                    report->converted, report->unchanged, report->aliased,
                    report->removed);
  return 0;
}

// catalogs/menu/ tables and their manifest.json outputs from the disc files
// already under root.
int ConvertTables(const fs::path& root, bool force) {
  const auto report = Extract::ConvertDiscTables(root, force);
  if (!report) {
    VS::Core::LogError("extract", "{}", report.error());
    return 1;
  }
  VS::Core::LogInfo("extract",
                    "tables: {} converted, {} unchanged, {} stale files "
                    "removed",
                    report->converted, report->unchanged, report->removed);
  return 0;
}

// models/characters/ and models/weapons/ glTF models, models/animation-sets/
// JSON sets, textures/actors/ effect texture images and their manifest.json
// outputs from the disc files already under root.
int ConvertModels(const fs::path& root, bool force) {
  const auto characters = Extract::ConvertDiscModels(root, force);
  if (!characters) {
    VS::Core::LogError("extract", "{}", characters.error());
    return 1;
  }
  VS::Core::LogInfo("extract",
                    "models: {} converted, {} unchanged, {} stale files "
                    "removed",
                    characters->converted, characters->unchanged,
                    characters->removed);
  const auto weapons = Extract::ConvertDiscWeapons(root, force);
  if (!weapons) {
    VS::Core::LogError("extract", "{}", weapons.error());
    return 1;
  }
  VS::Core::LogInfo("extract",
                    "weapons: {} converted, {} unchanged, {} stale files "
                    "removed",
                    weapons->converted, weapons->unchanged, weapons->removed);
  const auto sets = Extract::ConvertDiscAnimationSets(root, force);
  if (!sets) {
    VS::Core::LogError("extract", "{}", sets.error());
    return 1;
  }
  VS::Core::LogInfo("extract",
                    "animation sets: {} converted, {} unchanged, {} stale "
                    "files removed",
                    sets->converted, sets->unchanged, sets->removed);
  const auto textures = Extract::ConvertDiscActorTextures(root, force);
  if (!textures) {
    VS::Core::LogError("extract", "{}", textures.error());
    return 1;
  }
  VS::Core::LogInfo("extract",
                    "actor textures: {} converted, {} unchanged, {} stale "
                    "files removed",
                    textures->converted, textures->unchanged,
                    textures->removed);
  return 0;
}

// The AV1 encoder movies use; none without FFmpeg or a disc image to read
// the movie's raw sectors from.
std::expected<std::optional<Content::Av1Encoder>, std::string> MovieEncoder(
    const Content::Disc& disc, const fs::path& out,
    const std::optional<fs::path>& ffmpeg) {
  if (!ffmpeg) {
    VS::Core::LogWarn(
        "extract",
        "FFmpeg not found (PATH or --ffmpeg): movies are listed, not "
        "converted");
    return std::nullopt;
  }
  if (!disc.IsImage()) return std::nullopt;
  auto found = Content::FindAv1Encoder(*ffmpeg, out / "ffmpeg-encoders.txt");
  if (!found) return std::unexpected(std::move(found.error()));
  VS::Core::LogInfo("extract", "movies use the FFmpeg encoder {}", found->name);
  return std::optional<Content::Av1Encoder>(std::move(*found));
}

// <out>\disc\<path>: a file's bytes as they are on the disc.
bool CopyDiscFile(const Content::Disc& disc, const fs::path& out,
                  const Content::DiscEntry& file) {
  const auto data = disc.ReadFile(file);
  if (!data) {
    VS::Core::LogError("extract", "{}", data.error());
    return false;
  }
  return WriteBytes(*data, out / "disc" / fs::path(file.path));
}

// The conversions that read the extracted files rather than the disc, then
// the interleaved streams.
int ConvertExtracted(const Content::Disc& disc, const fs::path& out,
                     const std::optional<fs::path>& ffmpeg) {
  if (WriteCatalogs(out) != 0) return 1;
  if (ConvertImages(out, false) != 0) return 1;
  if (ConvertTables(out, false) != 0) return 1;
  if (ConvertModels(out, false) != 0) return 1;
  if (ConvertBanks(out, false) != 0) return 1;
  return ConvertStreams(disc, out, ffmpeg);
}

int ExtractDisc(const Content::Disc& disc, int argc, char** argv) {
  // <outdir>\disc\<path> for each file, and the manifest Content::DataStore
  // reads (sector, size, form, path). Interleaved files (movies, XA audio)
  // are listed with form "stream" until their converters write them.
  const fs::path out = Platform::Utf8Path(argv[kOutputArgument]);
  fs::create_directories(out);
  std::ofstream manifest(out / Content::DataStore::ManifestName,
                         std::ios::binary);
  if (!manifest) {
    VS::Core::LogError("extract", "cannot create the data manifest in {}",
                       Platform::PathText(out));
    return 1;
  }
  manifest << "lba\tsize\tform\tpath\n";
  const auto ffmpeg = FindFfmpeg(argc, argv);
  const auto encoder = MovieEncoder(disc, out, ffmpeg);
  if (!encoder) {
    VS::Core::LogError("extract", "{}", encoder.error());
    return 1;
  }
  const auto loaded = Content::LoadConversionManifest(out);
  if (!loaded) {
    VS::Core::LogError("extract", "{}", loaded.error());
    return 1;
  }
  const Content::ConversionManifest previous =
      loaded->value_or(Content::ConversionManifest{});
  Extract::ConverterBatch movies =
      Extract::StartBatch(previous, Content::kStrMkvConverterName,
                          Content::kStrMkvConverterVersion, false);
  int written = 0;
  int skipped = 0;
  for (const auto& f : disc.Files()) {
    if (f.directory) {
      continue;
    }
    if (f.interleaved) {
      bool converted = false;
      if (f.path.ends_with(Content::kMovieExtension) && disc.IsImage()) {
        const auto movie = ConvertOrKeepMovie(disc, out, ffmpeg, *encoder,
                                              previous, f, movies);
        if (!movie) return 1;
        converted = *movie;
      }
      manifest << f.lba << '\t' << f.size << '\t'
               << (converted ? Content::kMovieForm : "stream") << '\t' << f.path
               << '\n';
      ++(converted ? written : skipped);
      continue;
    }
    if (!CopyDiscFile(disc, out, f)) return 1;
    manifest << f.lba << '\t' << f.size << "\traw\t" << f.path << '\n';
    ++written;
  }
  manifest.close();
  if (!manifest) {
    VS::Core::LogError("extract", "cannot finish the data manifest in {}",
                       Platform::PathText(out));
    return 1;
  }
  if (const auto removed =
          Extract::PublishBatches(out, previous, std::span(&movies, 1));
      !removed) {
    VS::Core::LogError("extract", "{}", removed.error());
    return 1;
  }
  VS::Core::LogInfo(
      "extract", "wrote {} files to {} ({} interleaved listed, not converted)",
      written, Platform::PathText(out), skipped);
  return ConvertExtracted(disc, out, ffmpeg);
}

bool ValidStreamArguments(int argc, char** argv) {
  return argc == kExtractArguments ||
         (argc == kExtractArguments + 2 &&
          std::string_view(argv[kExtractArguments]) == "--ffmpeg");
}

// <command> <datadir> [--force]: whether the arguments are valid, and
// whether --force is given.
std::optional<bool> ForceArgument(int argc, char** argv) {
  if (argc == kImageArguments) return false;
  if (argc == kForcedImageArguments &&
      std::string_view(argv[kOutputArgument]) == "--force") {
    return true;
  }
  return std::nullopt;
}

}  // namespace

int Run(int argc, char** argv) {
  if (argc < kMinimumArguments) {
    return Usage();
  }
  const std::string cmd = argv[1];
  if (cmd == "catalogs") {
    return argc == kCatalogArguments
               ? WriteCatalogs(Platform::Utf8Path(argv[2]))
               : Usage();
  }
  if (cmd == "images" || cmd == "tables" || cmd == "models" || cmd == "banks") {
    const auto force = ForceArgument(argc, argv);
    if (!force) return Usage();
    const fs::path root = Platform::Utf8Path(argv[2]);
    if (cmd == "models") return ConvertModels(root, *force);
    if (cmd == "banks") return ConvertBanks(root, *force);
    return cmd == "images" ? ConvertImages(root, *force)
                           : ConvertTables(root, *force);
  }
  auto disc = Content::Disc::Open(Platform::Utf8Path(argv[2]));
  if (!disc) {
    VS::Core::LogError("extract", "{}", disc.error());
    return 1;
  }
  if (cmd == "list") {
    return ListDisc(**disc);
  }
  if (cmd == "streams" && ValidStreamArguments(argc, argv)) {
    return ConvertStreams(**disc, Platform::Utf8Path(argv[kOutputArgument]),
                          FindFfmpeg(argc, argv));
  }
  if (cmd == "extract" && argc >= kExtractArguments) {
    return ExtractDisc(**disc, argc, argv);
  }
  return Usage();
}

int wmain(int argc, wchar_t** argv) try {
  std::vector<std::string> values;
  values.reserve(argc);
  for (int index = 0; index < argc; ++index) {
    values.push_back(Platform::PathText(fs::path(argv[index])));
  }
  std::vector<char*> arguments;
  for (auto& value : values) arguments.push_back(value.data());
  return Run(argc, arguments.data());
} catch (const std::exception& error) {
  std::fprintf(stderr, "extract: %s\n", error.what());
  return 1;
} catch (...) {
  std::fputs("extract: unknown failure\n", stderr);
  return 1;
}
