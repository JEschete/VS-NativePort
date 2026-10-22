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

#include "Content/AudioBank.hpp"
#include "Content/AudioStream.hpp"
#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Core/Log.hpp"
#include "Platform/App.hpp"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;
using VS::u32;
using VS::u64;
using VS::u8;
using VS::usize;
namespace Content = VS::Content;
namespace Platform = VS::Platform;

namespace {

constexpr int kMinimumArguments = 3;
constexpr int kExtractArguments = 4;
constexpr int kOutputArgument = 3;

int Usage() {
  std::puts(
      "usage:\n"
      "  VS.Extract list    <disc.cue|folder>\n"
      "  VS.Extract audio   <disc.cue|folder> <outdir> [wave-file-id ...]\n"
      "  VS.Extract streams <disc.cue|folder> <outdir> [--ffmpeg "
      "<ffmpeg.exe>]\n"
      "  VS.Extract extract <disc.cue|folder> <outdir> [--ffmpeg "
      "<ffmpeg.exe>]\n"
      "      Form 1 files are copied as they are; movies (*.STR) are converted "
      "to Matroska (AV1 + FLAC)\n"
      "      with FFmpeg (found on PATH, or --ffmpeg). Effect audio and XA "
      "are imported under audio/streams as lossless PCM assets.");
  return 2;
}

// --ffmpeg <path>, else ffmpeg.exe on PATH.
std::optional<fs::path> FindFfmpeg(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == "--ffmpeg") {
      return fs::path(argv[i + 1]);
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
    VS_LOG_ERROR("extract", "{}: {}", file.path, result.error());
    return false;
  }
  if (xa_audio) {
    auto selected = Content::SelectXaAudioSectors(raw);
    if (!selected) {
      VS_LOG_ERROR("extract", "{}: {}", file.path, selected.error());
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
    VS_LOG_ERROR("extract", "cannot write {}", tmp.string());
    return false;
  }
  return true;
}

// Preserve the movie's decoded audio losslessly; video uses AV1 CRF 18.
bool ConvertMovie(const Content::Disc& disc, const fs::path& ffmpeg,
                  const Content::DiscFile& file, const fs::path& mkv) {
  const fs::path tmp = fs::path(mkv.string() + ".raw");
  if (!WriteRawStream(disc, file, tmp)) return false;
  const auto arguments = std::to_array<std::string>({ffmpeg.string(),
                                                     "-hide_banner",
                                                     "-loglevel",
                                                     "error",
                                                     "-y",
                                                     "-f",
                                                     "psxstr",
                                                     "-i",
                                                     tmp.string(),
                                                     "-map",
                                                     "0:v",
                                                     "-map",
                                                     "0:a?",
                                                     "-c:v",
                                                     "libsvtav1",
                                                     "-preset",
                                                     "6",
                                                     "-crf",
                                                     "18",
                                                     "-pix_fmt",
                                                     "yuv420p",
                                                     "-color_range",
                                                     "pc",
                                                     "-colorspace",
                                                     "bt470bg",
                                                     "-c:a",
                                                     "flac",
                                                     mkv.string()});
  const auto result = Platform::RunProcess(arguments);
  std::error_code ec;
  fs::remove(tmp, ec);
  if (!result || *result != 0 || !fs::exists(mkv, ec)) {
    VS_LOG_ERROR("extract", "{}: FFmpeg failed ({})", file.path,
                 result ? std::to_string(*result) : result.error());
    return false;
  }
  VS_LOG_INFO("extract", "{} -> {} ({} bytes)", file.path, mkv.string(),
              fs::file_size(mkv, ec));
  return true;
}

bool ConvertXa(const Content::Disc& disc, const fs::path& ffmpeg,
               const Content::DiscFile& file, const fs::path& flac) {
  const fs::path tmp = fs::path(flac.string() + ".raw");
  if (!WriteRawStream(disc, file, tmp, true)) return false;
  const auto arguments = std::to_array<std::string>(
      {ffmpeg.string(), "-hide_banner", "-loglevel", "error", "-y", "-f",
       "psxstr", "-i", tmp.string(), "-map", "0:a:0", "-c:a", "flac",
       flac.string()});
  const auto result = Platform::RunProcess(arguments);
  std::error_code error;
  fs::remove(tmp, error);
  if (!result || *result != 0 || !fs::exists(flac, error)) {
    VS_LOG_ERROR("extract", "{}: XA conversion failed ({})", file.path,
                 result ? std::to_string(*result) : result.error());
    return false;
  }
  VS_LOG_INFO("extract", "{} -> {} (lossless decoded XA FLAC)", file.path,
              flac.string());
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
      VS_LOG_ERROR("extract", "invalid wave-file ID: {}", argument);
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
      VS_LOG_ERROR("extract", "{}", bytes.error());
      return 1;
    }
    if (bytes->size() == 1 && (*bytes)[0] == 0) {
      VS_LOG_INFO("extract", "{}: empty wave-bank placeholder", file.path);
      selected.erase(file.path);
      continue;
    }
    auto bank = Content::DecodeAudioBank(*bytes);
    if (!bank) {
      VS_LOG_ERROR("extract", "{}: {}", file.path, bank.error());
      return 1;
    }
    if (auto result = Content::ExportAudioBank(*bank, argv[kOutputArgument]);
        !result) {
      VS_LOG_ERROR("extract", "{}: {}", file.path, result.error());
      return 1;
    }

    VS_LOG_INFO("extract", "{}: bank {}, {} instruments", file.path, bank->id,
                bank->instruments.size());
    for (usize index = 0; index < bank->instruments.size(); ++index) {
      if (!bank->instruments[index].available) {
        VS_LOG_WARN("extract",
                    "{}: instrument {} has no playable sample/pitch; "
                    "recorded as unavailable, no sample fabricated",
                    file.path, index);
      }
    }
    ++count;
    selected.erase(file.path);
  }
  if (!selected.empty()) {
    VS_LOG_ERROR("extract", "requested wave file was not found: {}",
                 *selected.begin());
    return 1;
  }
  if (count == 0) {
    VS_LOG_ERROR("extract", "no SOUND/WAVE banks in the selected source");
    return 1;
  }
  VS_LOG_INFO("extract", "converted {} audio banks", count);
  return 0;
}

int ConvertStreams(const Content::Disc& disc, const fs::path& out,
                   const std::optional<fs::path>& ffmpeg) {
  nlohmann::json entries = nlohmann::json::array();
  nlohmann::json xa = nlohmann::json::array();
  for (const auto& file : disc.Files()) {
    if (!file.directory && file.path.ends_with(".XA")) {
      if (!ffmpeg) {
        VS_LOG_ERROR("extract", "{}: FFmpeg is required for XA conversion",
                     file.path);
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
      VS_LOG_ERROR("extract", "{}", bytes.error());
      return 1;
    }
    auto streams = Content::FindEffectAudioStreams(*bytes);
    if (!streams) {
      VS_LOG_ERROR("extract", "{}: {}", file.path, streams.error());
      return 1;
    }
    for (const auto& source : *streams) {
      auto stream = Content::DecodeAudioStream(source.bytes);
      if (!stream) {
        VS_LOG_ERROR("extract", "{} at {}: {}", file.path, source.offset,
                     stream.error());
        return 1;
      }
      if (auto result = Content::ExportAudioStream(*stream, out); !result) {
        VS_LOG_ERROR("extract", "{}: {}", file.path, result.error());
        return 1;
      }
      entries.push_back({{"source", file.path},
                         {"offset", source.offset},
                         {"bytes", source.bytes.size()},
                         {"id", stream->info.id}});
      VS_LOG_INFO("extract", "{}: stream {}, {} channels, {} frames", file.path,
                  stream->info.id, stream->info.channels, stream->info.frames);
    }
  }
  if (entries.empty() && xa.empty()) {
    VS_LOG_ERROR("extract", "no effect-package or XA audio streams found");
    return 1;
  }
  std::ofstream index(out / "audio" / "streams" / "index.json");
  index << nlohmann::json{{"version", 1}, {"streams", entries}, {"xa", xa}}
               .dump(2)
        << '\n';
  index.close();
  if (!index) {
    VS_LOG_ERROR("extract", "writing stream provenance index");
    return 1;
  }
  VS_LOG_INFO("extract", "converted {} effect streams and {} XA tracks",
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

bool ValidStreamArguments(int argc, char** argv) {
  return argc == kExtractArguments ||
         (argc == kExtractArguments + 2 &&
          std::string_view(argv[kExtractArguments]) == "--ffmpeg");
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < kMinimumArguments) {
    return Usage();
  }
  const std::string cmd = argv[1];
  auto disc = Content::Disc::Open(argv[2]);
  if (!disc) {
    VS_LOG_ERROR("extract", "{}", disc.error());
    return 1;
  }
  if (cmd == "list") {
    return ListDisc(**disc);
  }
  if (cmd == "audio" && argc >= kExtractArguments) {
    return ConvertAudio(**disc, argc, argv);
  }
  if (cmd == "streams" && ValidStreamArguments(argc, argv)) {
    return ConvertStreams(**disc, argv[kOutputArgument],
                          FindFfmpeg(argc, argv));
  }
  if (cmd == "extract" && argc >= kExtractArguments) {
    // <outdir>\disc\<path> for each file, and the manifest Content::DataStore
    // reads (sector, size, form, path). Interleaved files (movies, XA audio)
    // are listed with form "stream" until their converters write them.
    const fs::path out = argv[kOutputArgument];
    fs::create_directories(out);
    std::ofstream manifest(out / Content::DataStore::ManifestName,
                           std::ios::binary);
    if (!manifest) {
      VS_LOG_ERROR("extract", "cannot create the data manifest in {}",
                   out.string());
      return 1;
    }
    manifest << "lba\tsize\tform\tpath\n";
    const auto ffmpeg = FindFfmpeg(argc, argv);
    if (!ffmpeg) {
      VS_LOG_WARN("extract",
                  "FFmpeg not found (PATH or --ffmpeg): movies are listed, not "
                  "converted");
    }
    int written = 0;
    int skipped = 0;
    for (const auto& f : (*disc)->Files()) {
      if (f.directory) {
        continue;
      }
      if (f.interleaved) {
        const bool movie = f.path.ends_with(".STR");
        const fs::path mkv =
            fs::path(out / "disc" / fs::path(f.path)).string() + ".mkv";
        if (movie && ffmpeg && ConvertMovie(**disc, *ffmpeg, f, mkv)) {
          manifest << f.lba << '\t' << f.size << "\tmkv\t" << f.path << '\n';
          ++written;
        } else {
          manifest << f.lba << '\t' << f.size << "\tstream\t" << f.path << '\n';
          ++skipped;
        }
        continue;
      }
      auto data = (*disc)->ReadFile(f);
      if (!data) {
        VS_LOG_ERROR("extract", "{}", data.error());
        return 1;
      }
      const fs::path dst = out / "disc" / fs::path(f.path);
      fs::create_directories(dst.parent_path());
      std::ofstream output(dst, std::ios::binary);
      output.write(reinterpret_cast<const char*>(data->data()),
                   static_cast<std::streamsize>(data->size()));
      output.close();
      if (!output) {
        VS_LOG_ERROR("extract", "cannot write {}", dst.string());
        return 1;
      }
      manifest << f.lba << '\t' << f.size << "\traw\t" << f.path << '\n';
      ++written;
    }
    manifest.close();
    if (!manifest) {
      VS_LOG_ERROR("extract", "cannot finish the data manifest in {}",
                   out.string());
      return 1;
    }
    VS_LOG_INFO("extract",
                "wrote {} files to {} ({} interleaved listed, not converted)",
                written, out.string(), skipped);
    return ConvertStreams(**disc, out, ffmpeg);
  }
  return Usage();
}  // namespace
catch (const std::exception& error) {
  std::fprintf(stderr, "extract: %s\n", error.what());
  return 1;
} catch (...) {
  std::fputs("extract: unknown failure\n", stderr);
  return 1;
}
