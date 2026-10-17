#include "Content/DataStore.hpp"
#include "Content/DataAccess.h"

#include "Core/Log.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <fstream>
#include <memory>
#include <unordered_set>

namespace fs = std::filesystem;

namespace VS::Content {

namespace {

std::unique_ptr<DataStore>& Instance()
{
    static std::unique_ptr<DataStore> store;
    return store;
}

bool ParseU32(std::string_view s, u32& out)
{
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size();
}

} // namespace

std::expected<void, std::string> DataStore::Open(const fs::path& root)
{
    std::ifstream in(root / ManifestName);
    if (!in) {
        return std::unexpected("no extracted data at " + (root / ManifestName).string() +
                               " (run VS.Extract extract <disc> <dir>)");
    }
    auto store = std::make_unique<DataStore>();
    store->m_root = root;
    std::string line;
    std::getline(in, line);  // header
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::string_view v(line);
        Entry e;
        std::string_view cols[4];
        for (int i = 0; i < 4; ++i) {
            const usize tab = i < 3 ? v.find('\t') : v.size();
            if (tab == std::string_view::npos) {
                return std::unexpected("bad line in " + (root / ManifestName).string() + ": " + line);
            }
            cols[i] = v.substr(0, tab);
            v = i < 3 ? v.substr(tab + 1) : std::string_view{};
        }
        if (!ParseU32(cols[0], e.lba) || !ParseU32(cols[1], e.size)) {
            return std::unexpected("bad line in " + (root / ManifestName).string() + ": " + line);
        }
        e.form = cols[2];
        e.path = cols[3];
        store->m_entries.push_back(std::move(e));
    }
    std::ranges::sort(store->m_entries, {}, &Entry::lba);
    VS_LOG_INFO("content", "extracted data: {} files from {}", store->m_entries.size(), root.string());
    Instance() = std::move(store);
    return {};
}

const DataStore* DataStore::Get()
{
    return Instance().get();
}

const DataStore::Entry* DataStore::Find(std::string_view path) const
{
    for (const Entry& e : m_entries) {
        if (e.path == path) {
            return &e;
        }
    }
    return nullptr;
}

const DataStore::Entry* DataStore::FindBySector(u32 lba) const
{
    auto it = std::ranges::upper_bound(m_entries, lba, {}, &Entry::lba);
    if (it == m_entries.begin()) {
        return nullptr;
    }
    --it;
    const u32 sectors = (it->size + 2047) / 2048;
    return lba < it->lba + std::max<u32>(sectors, 1) ? &*it : nullptr;
}

fs::path DataStore::FileOf(const Entry& e) const
{
    return m_root / "disc" / fs::path(e.path);
}

bool DataStore::Read(const Entry& e, u64 offset, std::span<u8> out) const
{
    std::ifstream f(FileOf(e), std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(static_cast<std::streamoff>(offset));
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return static_cast<usize>(f.gcount()) == out.size();
}

std::expected<std::vector<u8>, std::string> DataStore::ReadAll(const Entry& e) const
{
    std::vector<u8> data(e.size);
    if (!Read(e, 0, data)) {
        return std::unexpected("cannot read " + FileOf(e).string());
    }
    return data;
}

} // namespace VS::Content

extern "C" int Content_ReadDiscBytes(unsigned lba, unsigned bytes, void* dest)
{
    using namespace VS;
    using VS::Content::DataStore;
    const DataStore* store = DataStore::Get();
    const DataStore::Entry* e = store ? store->FindBySector(lba) : nullptr;
    if (e == nullptr) {
        VS_LOG_ERROR("content", "no extracted file holds sector {} ({} bytes)", lba, bytes);
        return 0;
    }
    const u64 offset = static_cast<u64>(lba - e->lba) * 2048;
    // the game reads whole sectors: the last one may run past the end of the file
    const u64 available = offset < e->size ? e->size - offset : 0;
    const usize n = static_cast<usize>(std::min<u64>(bytes, available));
    if (n < bytes) {
        std::memset(static_cast<u8*>(dest) + n, 0, bytes - n);
    }
    if (!store->Read(*e, offset, std::span(static_cast<u8*>(dest), n))) {
        VS_LOG_ERROR("content", "read failed: {} at {} ({} bytes)", e->path, offset, n);
        return 0;
    }
    static std::unordered_set<std::string> logged;
    if (logged.insert(e->path).second) {
        VS_LOG_INFO("content", "disc read: {} (sector {}, {} bytes, {})", e->path, lba, bytes, e->form);
    }
    return 1;
}
