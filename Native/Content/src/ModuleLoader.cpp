#include "Content/DataAccess.h"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"

#include "Core/Log.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace VS;
using VS::Content::DataStore;

constexpr std::string_view ResidentModule = "SLUS_010.40";

// Module files read once per start-up.
class Modules {
public:
    explicit Modules(const DataStore& store) : m_store(store) {}

    const std::vector<u8>& Get(const char* module)
    {
        auto it = m_images.find(module);
        if (it == m_images.end()) {
            std::vector<u8> bytes;
            if (const DataStore::Entry* e = m_store.Find(module)) {
                if (auto all = m_store.ReadAll(*e)) {
                    bytes = std::move(*all);
                }
            }
            if (bytes.empty()) {
                VS_LOG_ERROR("content", "module data: {} not extracted", module);
            }
            it = m_images.emplace(module, std::move(bytes)).first;
        }
        return it->second;
    }

    usize Count() const { return m_images.size(); }

private:
    const DataStore& m_store;
    std::map<std::string, std::vector<u8>, std::less<>> m_images;
};

// PS1 address -> native pointer, searching the table's own module, then the resident one.
class SymbolMap {
public:
    explicit SymbolMap(std::span<const Content_ModuleSymbol> symbols)
    {
        for (const Content_ModuleSymbol& s : symbols) {
            m_byModule[s.module].push_back(&s);
        }
        for (auto& [module, list] : m_byModule) {
            std::ranges::sort(list, {}, &Content_ModuleSymbol::address);
        }
    }

    void* Resolve(std::string_view module, u32 address) const
    {
        if (void* p = ResolveIn(module, address)) {
            return p;
        }
        return module == ResidentModule ? nullptr : ResolveIn(ResidentModule, address);
    }

private:
    void* ResolveIn(std::string_view module, u32 address) const
    {
        auto it = m_byModule.find(module);
        if (it == m_byModule.end()) {
            return nullptr;
        }
        const auto& list = it->second;
        auto at = std::ranges::upper_bound(list, address, {}, &Content_ModuleSymbol::address);
        if (at == list.begin()) {
            return nullptr;
        }
        const Content_ModuleSymbol* s = *(at - 1);
        if (s->address == address) {
            return s->target;
        }
        const u64 offset = static_cast<u64>(address) - s->address;
        if (offset < s->size && s->sourceElementSize != 0 && s->nativeElementSize != 0) {
            const u64 element = offset / s->sourceElementSize;
            const u64 within = offset % s->sourceElementSize;
            if (within != 0 && s->sourceElementSize != s->nativeElementSize) {
                return nullptr;
            }
            const u64 nativeOffset = element * s->nativeElementSize + within;
            if (nativeOffset < s->nativeSize) {
                return static_cast<u8*>(s->target) + nativeOffset;
            }
        }
        return nullptr;
    }

    std::map<std::string, std::vector<const Content_ModuleSymbol*>, std::less<>> m_byModule;
};

int LoadData(Modules& modules)
{
    int failed = 0;
    for (unsigned i = 0; i < Content_moduleDataCount; ++i) {
        const Content_ModuleDataEntry& m = Content_moduleData[i];
        const std::vector<u8>& image = modules.Get(m.module);
        if (!Content::CopyModuleDataEntry(m, image)) {
            if (!image.empty()) {
                VS_LOG_ERROR("content", "module data: {} +0x{:X} ({} bytes, capacity {}) is invalid",
                             m.module, m.offset, m.size, m.capacity);
            }
            ++failed;
            continue;
        }
    }
    return failed;
}

bool RelocateTable(const Content_ModulePointerTable& table, std::span<const u8> image, const SymbolMap& symbols)
{
    if (table.module == nullptr || (table.count != 0 && table.target == nullptr) || table.count > table.capacity ||
        static_cast<u64>(table.offset) + table.count * 4ull > image.size()) {
        VS_LOG_ERROR("content", "pointer table {} +0x{:X}: {} entries exceed source or capacity {}",
                     table.module ? table.module : "<missing>", table.offset, table.count, table.capacity);
        return false;
    }
    std::vector<void*> resolved(table.count);
    unsigned unresolved = 0;
    for (unsigned index = 0; index < table.count; ++index) {
        u32 address = 0;
        std::memcpy(&address, image.data() + table.offset + index * 4ull, 4);
        resolved[index] = address != 0 ? symbols.Resolve(table.module, address) : nullptr;
        if (address != 0 && resolved[index] == nullptr) {
            VS_LOG_ERROR("content", "pointer table {} +0x{:X}: [{}] 0x{:08X} has no safe native mapping",
                         table.module, table.offset, index, address);
            ++unresolved;
        }
    }
    if (unresolved != 0) {
        return false;
    }
    std::ranges::copy(resolved, table.target);
    return true;
}

int RelocatePointers(Modules& modules)
{
    const SymbolMap symbols({Content_moduleSymbols, Content_moduleSymbolCount});
    int failed = 0;
    for (unsigned index = 0; index < Content_modulePointerCount; ++index) {
        const Content_ModulePointerTable& table = Content_modulePointers[index];
        if (!RelocateTable(table, modules.Get(table.module), symbols)) {
            ++failed;
        }
    }
    return failed;
}

} // namespace

namespace VS::Content {

bool CopyModuleDataEntry(const Content_ModuleDataEntry& entry, std::span<const unsigned char> image)
{
    if (entry.target == nullptr || entry.size > entry.capacity ||
        static_cast<u64>(entry.offset) + entry.size > image.size()) {
        return false;
    }
    if (entry.size != 0) {
        std::memcpy(entry.target, image.data() + entry.offset, entry.size);
    }
    return true;
}

void* ResolveModuleAddress(std::span<const Content_ModuleSymbol> symbols, std::string_view module, unsigned address)
{
    return SymbolMap(symbols).Resolve(module, address);
}

bool RelocateModulePointerTable(const Content_ModulePointerTable& table, std::span<const unsigned char> image,
                               std::span<const Content_ModuleSymbol> symbols)
{
    return RelocateTable(table, image, SymbolMap(symbols));
}

} // namespace VS::Content

extern "C" int Content_LoadModuleData(void)
{
    const DataStore* store = DataStore::Get();
    if (store == nullptr) {
        VS_LOG_ERROR("content", "module data: no extracted data");
        return static_cast<int>(Content_moduleDataCount + Content_modulePointerCount);
    }
    Modules modules(*store);
    const int dataFailed = LoadData(modules);
    const int pointersFailed = RelocatePointers(modules);
    VS_LOG_INFO("content", "module data: {} globals, {} pointer tables ({} symbols) from {} modules; {} failed",
                Content_moduleDataCount, Content_modulePointerCount, Content_moduleSymbolCount, modules.Count(),
                dataFailed + pointersFailed);
    return dataFailed + pointersFailed;
}
