#include <array>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/AssetCatalogImport.hpp"
#include "Content/AudioBank.hpp"
#include "Content/AudioStream.hpp"
#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Content/MovieEncoder.hpp"
#include "Core/Log.hpp"
#include "Extract/ImageConversion.hpp"
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
      "  VS.Extract audio   <disc.cue|disc.chd|folder> <outdir> "
      "[wave-file-id ...]\n"
      "  VS.Extract streams <disc.cue|disc.chd|folder> <outdir> [--ffmpeg "
      "<ffmpeg.exe>]\n"
      "  VS.Extract extract <disc.cue|disc.chd|folder> <outdir> [--ffmpeg "
      "<ffmpeg.exe>]\n"
      "  VS.Extract catalogs <datadir>\n"
      "  VS.Extract images   <datadir> [--force]\n"
      "      Form 1 files are copied as they are; movies (*.STR) are converted "
      "to Matroska (AV1 + FLAC)\n"
      "      with FFmpeg (found on PATH, or --ffmpeg). Effect audio and XA "
      "are imported under audio/streams as lossless PCM assets.\n"
      "      extract also writes catalogs/ (asset ids and the audio tables); "
      "catalogs rewrites\n"
      "      them from an extracted folder.\n"
      "      extract also converts the standalone TIM images to PNG under "
      "textures/ and records them\n"
      "      in manifest.json; images converts again only what changed "
      "(--force: everything).");
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

bool WriteRawStream(const Content::Disc& disc, const Content::DiscFile& file,
                    const fs::path& tmp, bool xa_audio = false) {
  const u64 sectors =
      (static_cast<u64>(file.size) + Content::Disc::SectorData - 1) /
      Content::Disc::SectorData;
  std::vector<u8> raw(static_cast<usize>(sectors) * Content::Disc::SectorRaw);
  if (auto result =
          disc.ReadRawSectors(file.lba, static_cast<u32>(sectors), raw);
      !result) {
    VS::Core::LogError("extract", "{}: {}", file.path, result.error());
    return false;
  }
  if (xa_audio) {
    auto selected = Content::SelectXaAudioSectors(raw);
    if (!selected) {
      VS::Core::LogError("extract", "{}: {}", file.path, selected.error());
      return false;
    }
    raw = std::move(*selected);
  }
  fs::create_directories(tmp.parent_path());
  std::ofstream stream(tmp, std::ios::binary);
  stream.write(reinterpret_cast<const char*>(raw.data()),
               static_cast<std::streamsize>(raw.size()));
  stream.close();
  if (!stream) {
    VS::Core::LogError("extract", "cannot write {}", Platform::PathText(tmp));
    return false;
  }
  return true;
}

// Preserve the movie's decoded audio losslessly; video uses AV1 CRF 18.
bool ConvertMovie(const Content::Disc& disc, const fs::path& ffmpeg,
                  const Content::Av1Encoder& encoder,
                  const Content::DiscFile& file, const fs::path& mkv) {
  fs::path tmp = mkv;
  tmp += L".raw";
  if (!WriteRawStream(disc, file, tmp)) return false;
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

bool ConvertXa(const Content::Disc& disc, const fs::path& ffmpeg,
               const Content::DiscFile& file, const fs::path& flac) {
  fs::path tmp = flac;
  tmp += L".raw";
  if (!WriteRawStream(disc, file, tmp, true)) return false;
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

int ConvertAudio(const Content::Disc& disc, int argc, char** argv) {
  constexpr u32 kMaximumWaveFile = 9999;
  std::set<std::string> selected;
  for (int index = kExtractArguments; index < argc; ++index) {
    u32 id = 0;
    const std::string_view argument = argv[index];
    const auto result =
        std::from_chars(argument.data(), argument.data() + argument.size(), id);
    if (result.ec != std::errc() ||
        result.ptr != argument.data() + argument.size() ||
        id > kMaximumWaveFile) {
      VS::Core::LogError("extract", "invalid wave-file ID: {}", argument);
      return 1;
    }
    selected.insert(std::format("SOUND/WAVE{:04}.DAT", id));
  }
  const bool filtered = !selected.empty();
  int count = 0;
  for (const auto& file : disc.Files()) {
    if (file.directory || !file.path.starts_with("SOUND/WAVE") ||
        !file.path.ends_with(".DAT") ||
        (filtered && !selected.contains(file.path))) {
      continue;
    }
    auto bytes = disc.ReadFile(file);
    if (!bytes) {
      VS::Core::LogError("extract", "{}", bytes.error());
      return 1;
    }
    if (bytes->size() == 1 && (*bytes)[0] == 0) {
      VS::Core::LogInfo("extract", "{}: empty wave-bank placeholder",
                        file.path);
      selected.erase(file.path);
      continue;
    }
    auto bank = Content::DecodeAudioBank(*bytes);
    if (!bank) {
      VS::Core::LogError("extract", "{}: {}", file.path, bank.error());
      return 1;
    }
    if (auto result = Content::ExportAudioBank(
            *bank, Platform::Utf8Path(argv[kOutputArgument]));
        !result) {
      VS::Core::LogError("extract", "{}: {}", file.path, result.error());
      return 1;
    }

    VS::Core::LogInfo("extract", "{}: bank {}, {} instruments", file.path,
                      bank->id, bank->instruments.size());
    for (usize index = 0; index < bank->instruments.size(); ++index) {
      if (!bank->instruments[index].available) {
        VS::Core::LogWarn("extract",
                          "{}: instrument {} has no playable sample/pitch; "
                          "recorded as unavailable, no sample fabricated",
                          file.path, index);
      }
    }
    ++count;
    selected.erase(file.path);
  }
  if (!selected.empty()) {
    VS::Core::LogError("extract", "requested wave file was not found: {}",
                       *selected.begin());
    return 1;
  }
  if (count == 0) {
    VS::Core::LogError("extract", "no SOUND/WAVE banks in the selected source");
    return 1;
  }
  VS::Core::LogInfo("extract", "converted {} audio banks", count);
  return 0;
}

int ConvertStreams(const Content::Disc& disc, const fs::path& out,
                   const std::optional<fs::path>& ffmpeg) {
  nlohmann::json entries = nlohmann::json::array();
  nlohmann::json xa = nlohmann::json::array();
  for (const auto& file : disc.Files()) {
    if (!file.directory && file.path.ends_with(".XA")) {
      if (!ffmpeg) {
        VS::Core::LogError(
            "extract", "{}: FFmpeg is required for XA conversion", file.path);
        return 1;
      }
      const auto asset = Content::XaAudioPath(out, file.path);
      if (!ConvertXa(disc, *ffmpeg, file, asset)) return 1;
      xa.push_back({{"source", file.path},
                    {"lba", file.lba},
                    {"source_bytes", file.size},
                    {"file", 1},
                    {"channel", 0},
                    {"sectors_per_second", Content::kXaSectorsPerSecond},
                    {"asset", fs::relative(asset, out).generic_string()}});
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
  VS::Core::LogInfo(
      "extract", "images: {} converted, {} unchanged, {} stale files removed",
      report->converted, report->unchanged, report->removed);
  return 0;
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
  if (!ffmpeg) {
    VS::Core::LogWarn(
        "extract",
        "FFmpeg not found (PATH or --ffmpeg): movies are listed, not "
        "converted");
  }
  std::optional<Content::Av1Encoder> encoder;
  if (ffmpeg && disc.IsImage()) {
    auto found = Content::FindAv1Encoder(*ffmpeg, out / "ffmpeg-encoders.txt");
    if (!found) {
      VS::Core::LogError("extract", "{}", found.error());
      return 1;
    }
    encoder = std::move(*found);
    VS::Core::LogInfo("extract", "movies use the FFmpeg encoder {}",
                      encoder->name);
  }
  int written = 0;
  int skipped = 0;
  for (const auto& f : disc.Files()) {
    if (f.directory) {
      continue;
    }
    if (f.interleaved) {
      const bool movie = f.path.ends_with(".STR");
      fs::path mkv = out / "disc" / fs::path(f.path);
      mkv += L".mkv";
      if (movie && encoder) {
        if (!ConvertMovie(disc, *ffmpeg, *encoder, f, mkv)) return 1;
        manifest << f.lba << '\t' << f.size << "\tmkv\t" << f.path << '\n';
        ++written;
      } else {
        manifest << f.lba << '\t' << f.size << "\tstream\t" << f.path << '\n';
        ++skipped;
      }
      continue;
    }
    auto data = disc.ReadFile(f);
    if (!data) {
      VS::Core::LogError("extract", "{}", data.error());
      return 1;
    }
    const fs::path dst = out / "disc" / fs::path(f.path);
    fs::create_directories(dst.parent_path());
    std::ofstream output(dst, std::ios::binary);
    output.write(reinterpret_cast<const char*>(data->data()),
                 static_cast<std::streamsize>(data->size()));
    output.close();
    if (!output) {
      VS::Core::LogError("extract", "cannot write {}", Platform::PathText(dst));
      return 1;
    }
    manifest << f.lba << '\t' << f.size << "\traw\t" << f.path << '\n';
    ++written;
  }
  manifest.close();
  if (!manifest) {
    VS::Core::LogError("extract", "cannot finish the data manifest in {}",
                       Platform::PathText(out));
    return 1;
  }
  VS::Core::LogInfo(
      "extract", "wrote {} files to {} ({} interleaved listed, not converted)",
      written, Platform::PathText(out), skipped);
  if (WriteCatalogs(out) != 0) return 1;
  if (ConvertImages(out, false) != 0) return 1;
  return ConvertStreams(disc, out, ffmpeg);
}

bool ValidStreamArguments(int argc, char** argv) {
  return argc == kExtractArguments ||
         (argc == kExtractArguments + 2 &&
          std::string_view(argv[kExtractArguments]) == "--ffmpeg");
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
  if (cmd == "images") {
    const bool force = argc == kForcedImageArguments &&
                       std::string_view(argv[kOutputArgument]) == "--force";
    return argc == kImageArguments || force
               ? ConvertImages(Platform::Utf8Path(argv[2]), force)
               : Usage();
  }
  auto disc = Content::Disc::Open(Platform::Utf8Path(argv[2]));
  if (!disc) {
    VS::Core::LogError("extract", "{}", disc.error());
    return 1;
  }
  if (cmd == "list") {
    return ListDisc(**disc);
  }
  if (cmd == "audio" && argc >= kExtractArguments) {
    return ConvertAudio(**disc, argc, argv);
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
