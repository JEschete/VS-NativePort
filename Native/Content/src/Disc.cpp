#include "Content/Disc.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace VS::Content {

namespace fs = std::filesystem;

namespace {
constexpr u32 kMode1DataOffset = 16;
constexpr u32 kMode2DataOffset = 24;
constexpr u32 kPrimaryDescriptorSector = 16;
constexpr usize kVolumeLabelOffset = 40;
constexpr usize kVolumeLabelBytes = 32;
constexpr usize kRootRecordOffset = 156;
constexpr usize kRecordLbaOffset = 2;
constexpr usize kRecordSizeOffset = 10;
constexpr usize kRecordFlagsOffset = 25;
constexpr usize kRecordNameLengthOffset = 32;
constexpr usize kRecordNameOffset = 33;
constexpr usize kXaExtensionBytes = 14;
constexpr usize kXaAttributeOffset = 4;
constexpr usize kXaSignatureOffset = 6;
constexpr u16 kXaInterleavedFlags = 0x6000;
constexpr int kDirectoryDepthLimit = 16;
}  // namespace

struct Disc::Impl {
  mutable std::ifstream bin;
  mutable std::mutex mutex;
  u32 sector_size = SectorRaw;
  u32 data_offset = kMode2DataOffset;
  u64 sector_count = 0;
  fs::path folder;  // extracted-folder mode
};

namespace {

u32 ReadLe32(const u8* source) {
  std::array<u8, sizeof(u32)> bytes{};
  std::copy_n(source, bytes.size(), bytes.begin());
  const u32 value = std::bit_cast<u32>(bytes);
  if constexpr (std::endian::native == std::endian::big) {
    return std::byteswap(value);
  }
  return value;
}

std::string Trim(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) {
    s.pop_back();
  }
  return s;
}

std::string Upper(std::string_view s) {
  std::string out(s);
  std::ranges::transform(out, out.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return out;
}

}  // namespace

std::string NormalizeDiscPath(std::string_view path) {
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

std::expected<std::unique_ptr<Disc>, std::string> Disc::Open(
    const fs::path& path) {
  std::unique_ptr<Disc> disc(new Disc());
  disc->m_impl_ = std::make_unique<Impl>();
  std::error_code ec;
  if (fs::is_directory(path, ec)) {
    if (auto r = disc->ScanFolder(path); !r) {
      return std::unexpected(r.error());
    }
    return disc;
  }
  if (!fs::exists(path, ec)) {
    return std::unexpected(
        std::format("disc path not found: {}", path.string()));
  }

  fs::path bin = path;
  u32 sector_size = SectorRaw;
  u32 data_offset = kMode2DataOffset;
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
      } else if (auto t = u.find("TRACK");
                 t != std::string::npos && mode.empty()) {
        mode = Trim(u.substr(u.rfind(' ') + 1));
      }
    }
    if (file.empty()) {
      return std::unexpected("cue sheet has no FILE entry");
    }
    bin = path.parent_path() / file;
    if (mode == "MODE1/2352") {
      data_offset = kMode1DataOffset;
    } else if (mode == "MODE1/2048" || mode == "MODE2/2048") {
      sector_size = SectorData;
      data_offset = 0;
    } else if (mode != "MODE2/2352") {
      return std::unexpected(
          std::format("unsupported cue track mode '{}'", mode));
    }
  } else if (ext == ".ISO") {
    sector_size = SectorData;
    data_offset = 0;
  }
  if (auto r = disc->OpenImage(bin, sector_size, data_offset); !r) {
    return std::unexpected(r.error());
  }
  return disc;
}

std::expected<void, std::string> Disc::OpenImage(const fs::path& bin,
                                                 u32 sector_size,
                                                 u32 data_offset) {
  m_impl_->bin.open(bin, std::ios::binary);
  if (!m_impl_->bin) {
    return std::unexpected(
        std::format("cannot open disc image {}", bin.string()));
  }
  m_impl_->sector_size = sector_size;
  m_impl_->data_offset = data_offset;
  m_impl_->sector_count = fs::file_size(bin) / sector_size;
  m_is_image_ = true;
  return ParseIso9660();
}

std::expected<void, std::string> Disc::ReadRawSectors(u32 lba, u32 count,
                                                      std::span<u8> out) const {
  if (!m_is_image_) {
    return std::unexpected("raw sector access requires a disc image");
  }
  const u32 size = m_impl_->sector_size;
  if (out.size() < static_cast<u64>(count) * size) {
    return std::unexpected("output buffer too small");
  }
  if (static_cast<u64>(lba) + count > m_impl_->sector_count) {
    return std::unexpected(
        std::format("sector {}+{} beyond end of disc", lba, count));
  }
  std::lock_guard lock(m_impl_->mutex);
  m_impl_->bin.clear();
  m_impl_->bin.seekg(static_cast<std::streamoff>(static_cast<u64>(lba) * size));
  m_impl_->bin.read(
      reinterpret_cast<char*>(out.data()),
      static_cast<std::streamsize>(static_cast<u64>(count) * size));
  if (!m_impl_->bin) {
    return std::unexpected(std::format("read failed at sector {}", lba));
  }
  return {};
}

std::expected<void, std::string> Disc::ReadDataSectors(
    u32 lba, u32 count, std::span<u8> out) const {
  if (out.size() < static_cast<u64>(count) * SectorData) {
    return std::unexpected("output buffer too small");
  }
  std::vector<u8> raw(m_impl_->sector_size);
  for (u32 index = 0; index < count; ++index) {
    if (auto r = ReadRawSectors(lba + index, 1, raw); !r) {
      return r;
    }
    std::ranges::copy(
        std::span(raw).subspan(m_impl_->data_offset, SectorData),
        out.subspan(static_cast<usize>(index) * SectorData).begin());
  }
  return {};
}

std::expected<void, std::string> Disc::ParseIso9660() {
  std::vector<u8> pvd(SectorData);
  if (auto r = ReadDataSectors(kPrimaryDescriptorSector, 1, pvd); !r) {
    return r;
  }
  constexpr std::string_view kSignature = "CD001";
  if (pvd[0] != 1 ||
      !std::ranges::equal(std::span(pvd).subspan(1, kSignature.size()),
                          kSignature)) {
    return std::unexpected("no ISO9660 primary volume descriptor at sector 16");
  }
  m_label_ =
      Trim(std::string(reinterpret_cast<const char*>(&pvd[kVolumeLabelOffset]),
                       kVolumeLabelBytes));
  const u8* root = &pvd[kRootRecordOffset];
  if (root[0] < kRecordNameOffset + 1) {
    return std::unexpected("truncated ISO9660 root directory record");
  }
  if (auto result = WalkDirectory(ReadLe32(root + kRecordLbaOffset),
                                  ReadLe32(root + kRecordSizeOffset), "", 0);
      !result) {
    return result;
  }
  std::ranges::sort(m_files_, {}, &DiscFile::path);
  return {};
}

std::expected<void, std::string> Disc::WalkDirectory(u32 lba, u32 size,
                                                     const std::string& prefix,
                                                     int depth) {
  if (depth > kDirectoryDepthLimit) {
    return std::unexpected("ISO9660 directory nesting exceeds the limit");
  }
  const u64 sectors = (static_cast<u64>(size) + SectorData - 1) / SectorData;
  if (static_cast<u64>(lba) + sectors > m_impl_->sector_count) {
    return std::unexpected("ISO9660 directory extends beyond the disc image");
  }
  std::vector<u8> data(sectors * SectorData);
  if (auto result = ReadDataSectors(lba, static_cast<u32>(sectors), data);
      !result) {
    return result;
  }
  u32 pos = 0;
  while (pos < size) {
    const u8 len = data[pos];
    if (len == 0) {
      pos = (pos / SectorData + 1) * SectorData;  // records never span sectors
      continue;
    }
    if (len < kRecordNameOffset + 1 || len > size - pos ||
        len > SectorData - pos % SectorData) {
      return std::unexpected(
          "truncated or cross-sector ISO9660 directory record");
    }
    const u8* rec = &data[pos];
    const u8 name_len = rec[kRecordNameLengthOffset];
    if (name_len == 0 || kRecordNameOffset + name_len > len) {
      return std::unexpected(
          "ISO9660 file name extends beyond its directory record");
    }
    std::string name(reinterpret_cast<const char*>(rec + kRecordNameOffset),
                     name_len);
    const bool is_dir = (rec[kRecordFlagsOffset] & 0x02) != 0;
    if (!(name_len == 1 &&
          (rec[kRecordNameOffset] == 0 || rec[kRecordNameOffset] == 1))) {
      DiscFile f;
      f.path = prefix + NormalizeDiscPath(name);
      f.lba = ReadLe32(rec + kRecordLbaOffset);
      f.size = ReadLe32(rec + kRecordSizeOffset);
      f.directory = is_dir;
      // CD-XA system use area: "XA" signature, attribute bit 0x2000 (Form 2) /
      // 0x4000 (interleaved).
      const usize extension =
          kRecordNameOffset + name_len + ((name_len & 1) == 0 ? 1 : 0);
      if (extension + kXaExtensionBytes <= len &&
          rec[extension + kXaSignatureOffset] == 'X' &&
          rec[extension + kXaSignatureOffset + 1] == 'A') {
        constexpr unsigned kByteBits = 8;
        const u16 attributes = static_cast<u16>(
            (rec[extension + kXaAttributeOffset] << kByteBits) |
            rec[extension + kXaAttributeOffset + 1]);
        f.interleaved = (attributes & kXaInterleavedFlags) != 0;
      }
      m_files_.push_back(f);
      if (is_dir) {
        if (auto result = WalkDirectory(f.lba, f.size, f.path + "/", depth + 1);
            !result) {
          return result;
        }
      }
    }
    pos += len;
  }
  return {};
}

std::expected<void, std::string> Disc::ScanFolder(const fs::path& root) {
  m_impl_->folder = root;
  m_label_ = root.filename().string();
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(root, ec);
       it != fs::recursive_directory_iterator(); ++it) {
    DiscFile f;
    f.path =
        NormalizeDiscPath(fs::relative(it->path(), root, ec).generic_string());
    f.directory = it->is_directory();
    f.size = f.directory ? 0 : static_cast<u32>(it->file_size());
    const std::string ext = Upper(it->path().extension().string());
    f.interleaved = ext == ".STR" || ext == ".XA";
    m_files_.push_back(std::move(f));
  }
  if (ec) {
    return std::unexpected(
        std::format("cannot scan {}: {}", root.string(), ec.message()));
  }
  std::ranges::sort(m_files_, {}, &DiscFile::path);
  return {};
}

std::optional<DiscFile> Disc::Find(std::string_view path) const {
  const std::string key = NormalizeDiscPath(path);
  auto it = std::ranges::lower_bound(m_files_, key, {}, &DiscFile::path);
  if (it != m_files_.end() && it->path == key) {
    return *it;
  }
  return std::nullopt;
}

std::expected<std::vector<u8>, std::string> Disc::ReadFile(
    const DiscFile& file) const {
  if (file.directory) {
    return std::unexpected(std::format("{} is a directory", file.path));
  }
  if (!m_is_image_) {
    std::ifstream in(m_impl_->folder / fs::path(file.path), std::ios::binary);
    if (!in) {
      return std::unexpected(std::format("cannot open {}", file.path));
    }
    std::vector<u8> out(file.size);
    in.read(reinterpret_cast<char*>(out.data()),
            static_cast<std::streamsize>(out.size()));
    if (!in) {
      return std::unexpected(std::format("truncated file: {}", file.path));
    }
    return out;
  }
  const u64 sectors =
      (static_cast<u64>(file.size) + SectorData - 1) / SectorData;
  if (static_cast<u64>(file.lba) + sectors > m_impl_->sector_count) {
    return std::unexpected(
        std::format("file extends beyond disc: {}", file.path));
  }
  std::vector<u8> out(sectors * SectorData);
  if (auto r = ReadDataSectors(file.lba, static_cast<u32>(sectors), out); !r) {
    return std::unexpected(r.error());
  }
  out.resize(file.size);
  return out;
}

std::expected<std::vector<u8>, std::string> Disc::ReadFile(
    std::string_view path) const {
  auto f = Find(path);
  if (!f) {
    return std::unexpected(std::format("file not on disc: {}", path));
  }
  return ReadFile(*f);
}

}  // namespace VS::Content
