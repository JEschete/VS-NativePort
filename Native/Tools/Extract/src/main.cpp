#include "vs/content/Disc.hpp"
#include "vs/core/Log.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace VS;

namespace {

int Usage()
{
    std::puts("usage:\n"
              "  VS.Extract list    <disc.cue|folder>\n"
              "  VS.Extract extract <disc.cue|folder> <outdir>   (Form 1 files; interleaved STR/XA are skipped)");
    return 2;
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
        const fs::path out = argv[3];
        int written = 0;
        int skipped = 0;
        for (const auto& f : (*disc)->Files()) {
            if (f.directory) {
                continue;
            }
            if (f.interleaved) {
                ++skipped;
                continue;
            }
            auto data = (*disc)->ReadFile(f);
            if (!data) {
                VS_LOG_ERROR("extract", "{}", data.error());
                return 1;
            }
            const fs::path dst = out / fs::path(f.path);
            fs::create_directories(dst.parent_path());
            std::ofstream(dst, std::ios::binary)
                .write(reinterpret_cast<const char*>(data->data()), static_cast<std::streamsize>(data->size()));
            ++written;
        }
        VS_LOG_INFO("extract", "wrote {} files to {} ({} interleaved skipped)", written, out.string(), skipped);
        return 0;
    }
    return Usage();
}
