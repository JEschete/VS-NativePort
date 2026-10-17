#include "Content/DataAccess.h"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"

#include "Core/Log.hpp"

#include <cstring>
#include <map>

extern "C" int Content_LoadModuleData(void)
{
    using namespace VS;
    using VS::Content::DataStore;
    const DataStore* store = DataStore::Get();
    if (store == nullptr) {
        VS_LOG_ERROR("content", "module data: no extracted data");
        return static_cast<int>(Content_moduleDataCount);
    }
    std::map<std::string, std::vector<u8>> modules;
    int failed = 0;
    for (unsigned i = 0; i < Content_moduleDataCount; ++i) {
        const Content_ModuleDataEntry& m = Content_moduleData[i];
        auto it = modules.find(m.module);
        if (it == modules.end()) {
            std::vector<u8> bytes;
            if (const DataStore::Entry* e = store->Find(m.module)) {
                if (auto all = store->ReadAll(*e)) {
                    bytes = std::move(*all);
                }
            }
            if (bytes.empty()) {
                VS_LOG_ERROR("content", "module data: {} not extracted", m.module);
            }
            it = modules.emplace(m.module, std::move(bytes)).first;
        }
        const std::vector<u8>& image = it->second;
        if (static_cast<u64>(m.offset) + m.size > image.size()) {
            if (!image.empty()) {
                VS_LOG_ERROR("content", "module data: {} +0x{:X} ({} bytes) is past the end", m.module, m.offset, m.size);
            }
            ++failed;
            continue;
        }
        std::memcpy(m.target, image.data() + m.offset, m.size);
    }
    VS_LOG_INFO("content", "module data: {} globals from {} modules, {} failed", Content_moduleDataCount,
                modules.size(), failed);
    return failed;
}
