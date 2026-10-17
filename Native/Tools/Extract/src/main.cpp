#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Core/Log.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using namespace VS;

namespace {

int Usage()
{
    std::puts("usage:\n"
              "  VS.Extract list    <disc.cue|folder>\n"
              "  VS.Extract extract <disc.cue|folder> <outdir> [--ffmpeg <ffmpeg.exe>]\n"
              "      Form 1 files are copied as they are; movies (*.STR) are converted to Matroska (AV1 + FLAC)\n"
              "      with FFmpeg (found on PATH, or --ffmpeg). Other interleaved files (XA) are listed only.");
    return 2;
}

// --ffmpeg <path>, else ffmpeg.exe on PATH.
std::optional<fs::path> FindFfmpeg(int argc, char** argv)
{
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
        const fs::path candidate = fs::path(std::string(dirs.substr(0, sep))) / "ffmpeg.exe";
        if (!candidate.parent_path().empty() && fs::exists(candidate, ec)) {
            return candidate;
        }
        dirs = sep == std::string_view::npos ? std::string_view{} : dirs.substr(sep + 1);
    }
    return std::nullopt;
}

// A PS1 movie (MDEC video + XA audio in raw 2352-byte sectors) to <disc path>.mkv: AV1 at high quality (CRF 18), full
// range kept as the MDEC decodes it, and FLAC (lossless) for the decoded XA audio.
bool ConvertMovie(const Content::Disc& disc, const Content::DiscFile& f, const fs::path& mkv, const fs::path& ffmpeg)
{
    const u32 sectors = (f.size + Content::Disc::SectorData - 1) / Content::Disc::SectorData;
    std::vector<u8> raw(static_cast<usize>(sectors) * Content::Disc::SectorRaw);
    if (auto r = disc.ReadRawSectors(f.lba, sectors, raw); !r) {
        VS_LOG_ERROR("extract", "{}: {}", f.path, r.error());
        return false;
    }
    const fs::path tmp = fs::path(mkv.string() + ".raw");
    fs::create_directories(mkv.parent_path());
    std::ofstream(tmp, std::ios::binary).write(reinterpret_cast<const char*>(raw.data()),
                                               static_cast<std::streamsize>(raw.size()));
    const std::string cmd = "\"\"" + ffmpeg.string() + "\" -hide_banner -loglevel error -y -f psxstr -i \"" +
                            tmp.string() + "\" -map 0:v -map 0:a? -c:v libsvtav1 -preset 6 -crf 18 -pix_fmt yuv420p "
                            "-color_range pc -colorspace bt470bg -c:a flac \"" + mkv.string() + "\"\"";
    const int rc = std::system(cmd.c_str());
    std::error_code ec;
    fs::remove(tmp, ec);
    if (rc != 0 || !fs::exists(mkv, ec)) {
        VS_LOG_ERROR("extract", "{}: FFmpeg failed ({})", f.path, rc);
        return false;
    }
    VS_LOG_INFO("extract", "{} -> {} ({} bytes)", f.path, mkv.string(), fs::file_size(mkv, ec));
    return true;
}

} // namespace

int main(int argc, char** argv)
{
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
            std::printf("%8u %10u %c%c %s\n", f.lba, f.size, f.directory ? 'd' : '-', f.interleaved ? 'x' : '-',
                        f.path.c_str());
        }
        std::printf("%zu entries, label '%s'\n", (*disc)->Files().size(), std::string((*disc)->Label()).c_str());
        return 0;
    }
    if (cmd == "extract" && argc >= 4) {
        // <outdir>\disc\<path> for each file, and the manifest Content::DataStore reads (sector, size, form, path).
        // Interleaved files (movies, XA audio) are listed with form "stream" until their converters write them.
        const fs::path out = argv[3];
        fs::create_directories(out);
        std::ofstream manifest(out / Content::DataStore::ManifestName, std::ios::binary);
        manifest << "lba\tsize\tform\tpath\n";
        const auto ffmpeg = FindFfmpeg(argc, argv);
        if (!ffmpeg) {
            VS_LOG_WARN("extract", "FFmpeg not found (PATH or --ffmpeg): movies are listed, not converted");
        }
        int written = 0;
        int skipped = 0;
        for (const auto& f : (*disc)->Files()) {
            if (f.directory) {
                continue;
            }
            if (f.interleaved) {
                const bool movie = f.path.ends_with(".STR");
                const fs::path mkv = fs::path(out / "disc" / fs::path(f.path)).string() + ".mkv";
                if (movie && ffmpeg && ConvertMovie(**disc, f, mkv, *ffmpeg)) {
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
            std::ofstream(dst, std::ios::binary)
                .write(reinterpret_cast<const char*>(data->data()), static_cast<std::streamsize>(data->size()));
            manifest << f.lba << '\t' << f.size << "\traw\t" << f.path << '\n';
            ++written;
        }
        VS_LOG_INFO("extract", "wrote {} files to {} ({} interleaved listed, not converted)", written,
                    out.string(), skipped);
        return 0;
    }
    return Usage();
}
