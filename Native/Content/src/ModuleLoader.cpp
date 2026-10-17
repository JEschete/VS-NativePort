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
    SymbolMap()
    {
        for (unsigned i = 0; i < Content_moduleSymbolCount; ++i) {
            const Content_ModuleSymbol& s = Content_moduleSymbols[i];
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
        if (s->size != 0 && address < s->address + s->size) {
            return static_cast<u8*>(s->target) + (address - s->address);
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
        if (static_cast<u64>(m.offset) + m.size > image.size()) {
            if (!image.empty()) {
                VS_LOG_ERROR("content", "module data: {} +0x{:X} ({} bytes) is past the end", m.module, m.offset, m.size);
            }
            ++failed;
            continue;
        }
        std::memcpy(m.target, image.data() + m.offset, m.size);
    }
    return failed;
}

int RelocatePointers(Modules& modules)
{
    const SymbolMap symbols;
    int failed = 0;
    for (unsigned i = 0; i < Content_modulePointerCount; ++i) {
        const Content_ModulePointerTable& t = Content_modulePointers[i];
        const std::vector<u8>& image = modules.Get(t.module);
        if (static_cast<u64>(t.offset) + t.count * 4ull > image.size()) {
            ++failed;
            continue;
        }
        int unresolved = 0;
        for (unsigned k = 0; k < t.count; ++k) {
            u32 address = 0;
            std::memcpy(&address, image.data() + t.offset + k * 4, 4);
            void* p = address != 0 ? symbols.Resolve(t.module, address) : nullptr;
            if (address != 0 && p == nullptr) {
                if (unresolved++ == 0) {
                    VS_LOG_WARN("content", "pointer table {} +0x{:X}: [{}] 0x{:08X} has no native symbol", t.module,
                                t.offset, k, address);
                }
            }
            t.target[k] = p;
        }
        if (unresolved != 0) {
            VS_LOG_WARN("content", "pointer table {} +0x{:X}: {} of {} unresolved", t.module, t.offset, unresolved,
                        t.count);
            ++failed;
        }
    }
    return failed;
}

} // namespace

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
