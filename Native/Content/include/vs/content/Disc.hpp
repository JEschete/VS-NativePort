#pragma once

#include "vs/core/Types.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VS::Content {

// One file on the disc (from the ISO9660 directory) or in an extracted folder.
struct DiscFile {
    std::string path;          // upper-case, '/'-separated, no ";1" (e.g. "MAP/ZONE009.ZND")
    u32 lba = 0;               // first sector (disc image only)
    u32 size = 0;              // bytes as recorded in the directory
    bool interleaved = false;  // XA mixed-mode (Form 2 / streaming), e.g. *.STR, *.XA
    bool directory = false;
};

// Read-only access to the user's disc: a .cue/.bin image (MODE2/2352 or MODE1/2352 or 2048-byte ISO)
// or an extracted folder (e.g. SLUS-01040\). Replaces libcd/libds sector reads (plan §6.6).
class Disc {
public:
    static constexpr u32 SectorRaw = 2352;
    static constexpr u32 SectorData = 2048;
    static constexpr u32 SectorForm2 = 2324;

    ~Disc();
    Disc(const Disc&) = delete;
    Disc& operator=(const Disc&) = delete;

    // Accepts a .cue, .bin/.iso, or a directory.
    static std::expected<std::unique_ptr<Disc>, std::string> Open(const std::filesystem::path& path);

    bool IsImage() const { return m_isImage; }
    std::string_view Label() const { return m_label; }
    const std::vector<DiscFile>& Files() const { return m_files; }

    // Case-insensitive; accepts '\' or '/', with or without leading slash and ";1".
    std::optional<DiscFile> Find(std::string_view path) const;

    // Whole file as Form 1 user data (2048 bytes/sector).
    std::expected<std::vector<u8>, std::string> ReadFile(const DiscFile& file) const;
    std::expected<std::vector<u8>, std::string> ReadFile(std::string_view path) const;

    // Raw 2352-byte sectors (image only); used by the movie/XA streamer for interleaved files.
    std::expected<void, std::string> ReadRawSectors(u32 lba, u32 count, std::span<u8> out) const;

    // Form 1 user data for a sector range (image only).
    std::expected<void, std::string> ReadDataSectors(u32 lba, u32 count, std::span<u8> out) const;

private:
    Disc() = default;
    std::expected<void, std::string> OpenImage(const std::filesystem::path& bin, u32 sectorSize, u32 dataOffset);
    std::expected<void, std::string> ParseIso9660();
    std::expected<void, std::string> ScanFolder(const std::filesystem::path& root);
    void WalkDirectory(u32 lba, u32 size, const std::string& prefix, int depth);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_isImage = false;
    std::string m_label;
    std::vector<DiscFile> m_files;
};

std::string NormalizeDiscPath(std::string_view path);

} // namespace VS::Content
