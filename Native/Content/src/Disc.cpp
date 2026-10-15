#include "vs/content/Disc.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <fstream>
#include <mutex>

namespace VS::Content {

namespace fs = std::filesystem;

struct Disc::Impl {
    mutable std::ifstream bin;
    mutable std::mutex mutex;
    u32 sectorSize = SectorRaw;  // bytes per sector in the image file
    u32 dataOffset = 24;         // offset of Form 1 user data inside a sector (MODE2: 24, MODE1: 16, ISO: 0)
    u64 sectorCount = 0;
    fs::path folder;             // extracted-folder mode
};

namespace {

u32 ReadLe32(const u8* p) { return u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24); }

std::string Trim(std::string s)
{
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) {
        s.pop_back();
    }
    return s;
}

std::string Upper(std::string_view s)
{
    std::string out(s);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

} // namespace

std::string NormalizeDiscPath(std::string_view path)
{
    std::string out = Upper(path);
    std::ranges::replace(out, '\\', '/');
    while (!out.empty() && out.front() == '/') {
        out.erase(out.begin());
    }
    if (auto semi = out.find(';'); semi != std::string::npos) {
        out.resize(semi);
    }
    return out;
}

Disc::~Disc() = default;

std::expected<std::unique_ptr<Disc>, std::string> Disc::Open(const fs::path& path)
{
    std::unique_ptr<Disc> disc(new Disc());
    disc->m_impl = std::make_unique<Impl>();
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        if (auto r = disc->ScanFolder(path); !r) {
            return std::unexpected(r.error());
        }
        return disc;
    }
    if (!fs::exists(path, ec)) {
        return std::unexpected(std::format("disc path not found: {}", path.string()));
    }

    fs::path bin = path;
    u32 sectorSize = SectorRaw;
    u32 dataOffset = 24;
    std::string ext = Upper(path.extension().string());
    if (ext == ".CUE") {
        std::ifstream cue(path);
        std::string line;
        std::string file;
        std::string mode;
        while (std::getline(cue, line)) {
            std::string u = Upper(line);
            if (auto f = u.find("FILE"); f != std::string::npos && file.empty()) {
                auto q1 = line.find('"');
                auto q2 = line.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    file = line.substr(q1 + 1, q2 - q1 - 1);
                }
            } else if (auto t = u.find("TRACK"); t != std::string::npos && mode.empty()) {
                mode = Trim(u.substr(u.rfind(' ') + 1));
            }
        }
        if (file.empty()) {
            return std::unexpected("cue sheet has no FILE entry");
        }
        bin = path.parent_path() / file;
        if (mode == "MODE1/2352") {
            dataOffset = 16;
        } else if (mode == "MODE1/2048" || mode == "MODE2/2048") {
            sectorSize = SectorData;
            dataOffset = 0;
        } else if (mode != "MODE2/2352") {
            return std::unexpected(std::format("unsupported cue track mode '{}'", mode));
        }
    } else if (ext == ".ISO") {
        sectorSize = SectorData;
        dataOffset = 0;
    }
    if (auto r = disc->OpenImage(bin, sectorSize, dataOffset); !r) {
        return std::unexpected(r.error());
    }
    return disc;
}

std::expected<void, std::string> Disc::OpenImage(const fs::path& bin, u32 sectorSize, u32 dataOffset)
{
    m_impl->bin.open(bin, std::ios::binary);
    if (!m_impl->bin) {
        return std::unexpected(std::format("cannot open disc image {}", bin.string()));
    }
    m_impl->sectorSize = sectorSize;
    m_impl->dataOffset = dataOffset;
    m_impl->sectorCount = fs::file_size(bin) / sectorSize;
    m_isImage = true;
    return ParseIso9660();
}

std::expected<void, std::string> Disc::ReadRawSectors(u32 lba, u32 count, std::span<u8> out) const
{
    if (!m_isImage) {
        return std::unexpected("raw sector access requires a disc image");
    }
    const u32 size = m_impl->sectorSize;
    if (out.size() < u64(count) * size) {
        return std::unexpected("output buffer too small");
    }
    if (u64(lba) + count > m_impl->sectorCount) {
        return std::unexpected(std::format("sector {}+{} beyond end of disc", lba, count));
    }
    std::lock_guard lock(m_impl->mutex);
    m_impl->bin.clear();
    m_impl->bin.seekg(static_cast<std::streamoff>(u64(lba) * size));
    m_impl->bin.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(u64(count) * size));
    if (!m_impl->bin) {
        return std::unexpected(std::format("read failed at sector {}", lba));
    }
    return {};
}

std::expected<void, std::string> Disc::ReadDataSectors(u32 lba, u32 count, std::span<u8> out) const
{
    if (out.size() < u64(count) * SectorData) {
        return std::unexpected("output buffer too small");
    }
    std::vector<u8> raw(m_impl->sectorSize);
    for (u32 i = 0; i < count; ++i) {
        if (auto r = ReadRawSectors(lba + i, 1, raw); !r) {
            return r;
        }
        std::memcpy(out.data() + u64(i) * SectorData, raw.data() + m_impl->dataOffset, SectorData);
    }
    return {};
}

std::expected<void, std::string> Disc::ParseIso9660()
{
    std::vector<u8> pvd(SectorData);
    if (auto r = ReadDataSectors(16, 1, pvd); !r) {
        return r;
    }
    if (pvd[0] != 1 || std::memcmp(&pvd[1], "CD001", 5) != 0) {
        return std::unexpected("no ISO9660 primary volume descriptor at sector 16");
    }
    m_label = Trim(std::string(reinterpret_cast<const char*>(&pvd[40]), 32));
    const u8* root = &pvd[156];
    WalkDirectory(ReadLe32(root + 2), ReadLe32(root + 10), "", 0);
    std::ranges::sort(m_files, {}, &DiscFile::path);
    return {};
}

void Disc::WalkDirectory(u32 lba, u32 size, const std::string& prefix, int depth)
{
    if (depth > 16) {
        return;
    }
    const u32 sectors = (size + SectorData - 1) / SectorData;
    std::vector<u8> data(u64(sectors) * SectorData);
    if (!ReadDataSectors(lba, sectors, data)) {
        return;
    }
    u32 pos = 0;
    while (pos < size) {
        const u8 len = data[pos];
        if (len == 0) {
            pos = (pos / SectorData + 1) * SectorData;  // records never span sectors
            continue;
        }
        const u8* rec = &data[pos];
        const u8 nameLen = rec[32];
        std::string name(reinterpret_cast<const char*>(rec + 33), nameLen);
        const bool isDir = (rec[25] & 0x02) != 0;
        if (!(nameLen == 1 && (rec[33] == 0 || rec[33] == 1))) {
            DiscFile f;
            f.path = prefix + NormalizeDiscPath(name);
            f.lba = ReadLe32(rec + 2);
            f.size = ReadLe32(rec + 10);
            f.directory = isDir;
            // CD-XA system use area: "XA" signature, attribute bit 0x2000 (Form 2) / 0x4000 (interleaved).
            u32 su = 33 + nameLen + ((nameLen & 1) == 0 ? 1 : 0);
            if (su + 14 <= len && rec[su + 6] == 'X' && rec[su + 7] == 'A') {
                const u16 attr = static_cast<u16>((rec[su + 4] << 8) | rec[su + 5]);
                f.interleaved = (attr & 0x2000) || (attr & 0x4000);
            }
            m_files.push_back(f);
            if (isDir) {
                WalkDirectory(f.lba, f.size, f.path + "/", depth + 1);
            }
        }
        pos += len;
    }
}

std::expected<void, std::string> Disc::ScanFolder(const fs::path& root)
{
    m_impl->folder = root;
    m_label = root.filename().string();
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); ++it) {
        DiscFile f;
        f.path = NormalizeDiscPath(fs::relative(it->path(), root, ec).generic_string());
        f.directory = it->is_directory();
        f.size = f.directory ? 0 : static_cast<u32>(it->file_size());
        const std::string ext = Upper(it->path().extension().string());
        f.interleaved = ext == ".STR" || ext == ".XA";
        m_files.push_back(std::move(f));
    }
    if (ec) {
        return std::unexpected(std::format("cannot scan {}: {}", root.string(), ec.message()));
    }
    std::ranges::sort(m_files, {}, &DiscFile::path);
    return {};
}

std::optional<DiscFile> Disc::Find(std::string_view path) const
{
    const std::string key = NormalizeDiscPath(path);
    auto it = std::ranges::lower_bound(m_files, key, {}, &DiscFile::path);
    if (it != m_files.end() && it->path == key) {
        return *it;
    }
    return std::nullopt;
}

std::expected<std::vector<u8>, std::string> Disc::ReadFile(const DiscFile& file) const
{
    if (file.directory) {
        return std::unexpected(std::format("{} is a directory", file.path));
    }
    if (!m_isImage) {
        std::ifstream in(m_impl->folder / fs::path(file.path), std::ios::binary);
        if (!in) {
            return std::unexpected(std::format("cannot open {}", file.path));
        }
        std::vector<u8> out(file.size);
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
        return out;
    }
    const u32 sectors = (file.size + SectorData - 1) / SectorData;
    std::vector<u8> out(u64(sectors) * SectorData);
    if (auto r = ReadDataSectors(file.lba, sectors, out); !r) {
        return std::unexpected(r.error());
    }
    out.resize(file.size);
    return out;
}

std::expected<std::vector<u8>, std::string> Disc::ReadFile(std::string_view path) const
{
    auto f = Find(path);
    if (!f) {
        return std::unexpected(std::format("file not on disc: {}", path));
    }
    return ReadFile(*f);
}

} // namespace VS::Content
