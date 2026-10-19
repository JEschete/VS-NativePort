#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Core/Log.hpp"
#include "Platform/App.hpp"

namespace fs = std::filesystem;
using VS::u32;
using VS::u64;
using VS::u8;
using VS::usize;
namespace Content = VS::Content;
namespace Platform = VS::Platform;

namespace {

int Usage() {
  std::puts(
      "usage:\n"
      "  VS.Extract list    <disc.cue|folder>\n"
      "  VS.Extract extract <disc.cue|folder> <outdir> [--ffmpeg "
      "<ffmpeg.exe>]\n"
      "      Form 1 files are copied as they are; movies (*.STR) are converted "
      "to Matroska (AV1 + FLAC)\n"
      "      with FFmpeg (found on PATH, or --ffmpeg). Other interleaved files "
      "(XA) are listed only.");
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

// A PS1 movie (MDEC video + XA audio in raw 2352-byte sectors) to <disc
// path>.mkv: AV1 at high quality (CRF 18), full range kept as the MDEC decodes
// it, and FLAC (lossless) for the decoded XA audio.
bool ConvertMovie(const Content::Disc& disc, const fs::path& ffmpeg,
                  const Content::DiscFile& file, const fs::path& mkv) {
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
  const fs::path tmp = fs::path(mkv.string() + ".raw");
  fs::create_directories(mkv.parent_path());
  std::ofstream stream(tmp, std::ios::binary);
  stream.write(reinterpret_cast<const char*>(raw.data()),
               static_cast<std::streamsize>(raw.size()));
  stream.close();
  if (!stream) {
    VS_LOG_ERROR("extract", "cannot write {}", tmp.string());
    return false;
  }
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

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 3) {
    return Usage();
  }
  const std::string cmd = argv[1];
  auto disc = Content::Disc::Open(argv[2]);
  if (!disc) {
    VS_LOG_ERROR("extract", "{}", disc.error());
    return 1;
  }
  if (cmd == "list") {
    for (const auto& f : (*disc)->Files()) {
      std::printf("%8u %10u %c%c %s\n", f.lba, f.size, f.directory ? 'd' : '-',
                  f.interleaved ? 'x' : '-', f.path.c_str());
    }
    std::printf("%zu entries, label '%s'\n", (*disc)->Files().size(),
                std::string((*disc)->Label()).c_str());
    return 0;
  }
  if (cmd == "extract" && argc >= 4) {
    // <outdir>\disc\<path> for each file, and the manifest Content::DataStore
    // reads (sector, size, form, path). Interleaved files (movies, XA audio)
    // are listed with form "stream" until their converters write them.
    const fs::path out = argv[3];
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
    return 0;
  }
  return Usage();
} catch (const std::exception& error) {
  std::fprintf(stderr, "extract: %s\n", error.what());
  return 1;
} catch (...) {
  std::fputs("extract: unknown failure\n", stderr);
  return 1;
}
