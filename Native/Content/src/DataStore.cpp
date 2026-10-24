#include "Content/DataStore.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Content/DataAccess.h"
#include "Core/Log.hpp"
#include "Platform/CrashHandler.hpp"

namespace fs = std::filesystem;

namespace VS::Content {

namespace {

std::unique_ptr<DataStore>& Instance() {
  static std::unique_ptr<DataStore> store;
  return store;
}

bool ParseU32(std::string_view s, u32& out) {
  const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
  return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

}  // namespace

DataStore::ScopedOverride::ScopedOverride() {
  m_previous_ = std::move(Instance());
}

DataStore::ScopedOverride::~ScopedOverride() { Restore(); }

void DataStore::ScopedOverride::Restore() noexcept {
  if (!m_active_) return;
  Instance() = std::move(m_previous_);
  m_active_ = false;
}

u32 DataStore::Entry::SectorCount() const {
  const u64 rounded =
      (static_cast<u64>(size) + kDataSectorBytes - 1) / kDataSectorBytes;
  return static_cast<u32>(std::max<u64>(rounded, 1));
}

bool DataStore::Entry::ContainsSector(u32 sector) const {
  return sector >= lba &&
         static_cast<u64>(sector) < static_cast<u64>(lba) + SectorCount();
}

std::expected<void, std::string> DataStore::Open(const fs::path& root) {
  enum : usize {
    kLbaColumn,
    kSizeColumn,
    kFormColumn,
    kPathColumn,
    kColumnCount
  };
  std::ifstream in(root / ManifestName);
  if (!in) {
    return std::unexpected("no extracted data at " +
                           (root / ManifestName).string() +
                           " (run VS.Extract extract <disc> <dir>)");
  }
  auto store = std::make_unique<DataStore>();
  store->m_root_ = root;
  std::string line;
  std::getline(in, line);  // header
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    std::string_view v(line);
    Entry e;
    std::string_view cols[kColumnCount];
    for (usize column = 0; column < kColumnCount; ++column) {
      const usize tab = column < kPathColumn ? v.find('\t') : v.size();
      if (tab == std::string_view::npos) {
        return std::unexpected("bad line in " + (root / ManifestName).string() +
                               ": " + line);
      }
      cols[column] = v.substr(0, tab);
      v = column < kPathColumn ? v.substr(tab + 1) : std::string_view{};
    }
    if (!ParseU32(cols[kLbaColumn], e.lba) ||
        !ParseU32(cols[kSizeColumn], e.size)) {
      return std::unexpected("bad line in " + (root / ManifestName).string() +
                             ": " + line);
    }
    e.form = cols[kFormColumn];
    e.path = cols[kPathColumn];
    store->m_entries_.push_back(std::move(e));
  }
  std::ranges::sort(store->m_entries_, {}, &Entry::lba);
  VS_LOG_INFO("content", "extracted data: {} files from {}",
              store->m_entries_.size(), root.string());
  Instance() = std::move(store);
  return {};
}

const DataStore* DataStore::Get() { return Instance().get(); }

const DataStore::Entry* DataStore::Find(std::string_view path) const {
  for (const Entry& e : m_entries_) {
    if (e.path == path) {
      return &e;
    }
  }
  return nullptr;
}

const DataStore::Entry* DataStore::FindBySector(u32 lba) const {
  auto it = std::ranges::upper_bound(m_entries_, lba, {}, &Entry::lba);
  if (it == m_entries_.begin()) {
    return nullptr;
  }
  --it;
  return it->ContainsSector(lba) ? &*it : nullptr;
}

fs::path DataStore::FileOf(const Entry& e) const {
  // a converted file keeps the disc name with its form as the extension
  // (MOV/TITLE.STR.mkv)
  const fs::path disc = m_root_ / "disc" / fs::path(e.path);
  return e.form == "raw" ? disc : fs::path(disc.string() + "." + e.form);
}

bool DataStore::Read(const Entry& e, u64 offset, std::span<u8> out) const {
  std::ifstream f(FileOf(e), std::ios::binary);
  if (!f) {
    return false;
  }
  f.seekg(static_cast<std::streamoff>(offset));
  f.read(reinterpret_cast<char*>(out.data()),
         static_cast<std::streamsize>(out.size()));
  return static_cast<usize>(f.gcount()) == out.size();
}

std::expected<std::vector<u8>, std::string> DataStore::ReadAll(
    const Entry& e) const {
  std::vector<u8> data(e.size);
  if (!Read(e, 0, data)) {
    return std::unexpected("cannot read " + FileOf(e).string());
  }
  return data;
}

}  // namespace VS::Content

extern "C" int Content_ReadDiscBytes(unsigned lba, unsigned bytes, void* dest) {
  using VS::u32;
  using VS::u64;
  using VS::u8;
  using VS::usize;
  using VS::Content::DataStore;
  const DataStore* store = DataStore::Get();
  if (store == nullptr) {
    return 0;
  }
  // A read runs on through consecutive files, as the drive read sectors
  // (MCDATA.BIN is read together with the MCMAN.BIN after it). Bytes past a
  // file's end inside its last sector, and sectors no file holds, read as 0.
  static std::unordered_set<std::string> logged;
  auto* out = static_cast<u8*>(dest);
  u32 sector = lba;
  usize remaining = bytes;
  while (remaining != 0) {
    const DataStore::Entry* e = store->FindBySector(sector);
    if (e == nullptr) {
      VS_LOG_ERROR(
          "content",
          "no extracted file holds sector {} (read of {} bytes at sector {})",
          sector, bytes, lba);
      VS::Platform::LogCallStack("bad disc read");
      std::memset(out, 0, remaining);
      return 0;
    }
    const u64 offset =
        static_cast<u64>(sector - e->lba) * DataStore::kDataSectorBytes;
    const u32 sectors = e->SectorCount();
    const usize span = std::min<usize>(
        remaining, (static_cast<u64>(e->lba) + sectors - sector) *
                       DataStore::kDataSectorBytes);
    const usize n = static_cast<usize>(
        std::min<u64>(span, offset < e->size ? e->size - offset : 0));
    if (n != 0 && !store->Read(*e, offset, std::span(out, n))) {
      VS_LOG_ERROR("content", "read failed: {} at {} ({} bytes)", e->path,
                   offset, n);
      return 0;
    }
    std::memset(out + n, 0, span - n);
    if (logged.insert(e->path).second) {
      VS_LOG_INFO("content", "disc read: {} (sector {}, {} bytes, {})", e->path,
                  sector, span, e->form);
    }
    out += span;
    remaining -= span;
    sector += static_cast<u32>(span / DataStore::kDataSectorBytes);
  }
  return 1;
}
