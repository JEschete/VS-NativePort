#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/DataAccess.h"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
#include "Core/Log.hpp"

namespace {

using VS::u32;
using VS::u64;
using VS::u8;
using VS::usize;
using VS::Content::DataStore;
namespace Content = VS::Content;

constexpr std::string_view kResidentModule = "SLUS_010.40";

// Module files read once per start-up.
class Modules {
 public:
  explicit Modules(const DataStore& store) : store_(store) {}

  const std::vector<u8>& Get(const char* module) {
    auto it = images_.find(module);
    if (it == images_.end()) {
      std::vector<u8> bytes;
      if (const DataStore::Entry* entry = store_.Find(module)) {
        if (auto all = store_.ReadAll(*entry)) {
          bytes = std::move(*all);
        }
      }
      if (bytes.empty()) {
        VS_LOG_ERROR("content", "module data: {} not extracted", module);
      }
      it = images_.emplace(module, std::move(bytes)).first;
    }
    return it->second;
  }

  usize Count() const { return images_.size(); }

 private:
  const DataStore& store_;
  std::map<std::string, std::vector<u8>, std::less<>> images_;
};

// PS1 address -> native pointer, searching the table's own module, then the
// resident one.
class SymbolMap {
 public:
  explicit SymbolMap(std::span<const Content_ModuleSymbol> symbols) {
    for (const Content_ModuleSymbol& symbol : symbols) {
      if (symbol.module != nullptr) {
        by_module_[symbol.module].push_back(&symbol);
      }
    }
    for (auto& [module, list] : by_module_) {
      std::ranges::sort(list, {}, &Content_ModuleSymbol::address);
    }
  }

  void* Resolve(std::string_view module, u32 address) const {
    if (void* target = ResolveIn(module, address)) {
      return target;
    }
    return module == kResidentModule ? nullptr
                                     : ResolveIn(kResidentModule, address);
  }

 private:
  void* ResolveIn(std::string_view module, u32 address) const {
    auto it = by_module_.find(module);
    if (it == by_module_.end()) {
      return nullptr;
    }
    const auto& list = it->second;
    auto at = std::ranges::upper_bound(list, address, {},
                                       &Content_ModuleSymbol::address);
    if (at == list.begin()) {
      return nullptr;
    }
    const Content_ModuleSymbol* symbol = *(at - 1);
    if (symbol->target == nullptr) {
      return nullptr;
    }
    if (symbol->address == address) {
      return symbol->target;
    }
    const u64 offset = static_cast<u64>(address) - symbol->address;
    if (offset < symbol->size && symbol->sourceElementSize != 0 &&
        symbol->nativeElementSize != 0) {
      const u64 element = offset / symbol->sourceElementSize;
      const u64 within = offset % symbol->sourceElementSize;
      if (within != 0 &&
          symbol->sourceElementSize != symbol->nativeElementSize) {
        return nullptr;
      }
      const u64 native_offset = element * symbol->nativeElementSize + within;
      if (native_offset < symbol->nativeSize) {
        return static_cast<u8*>(symbol->target) + native_offset;
      }
    }
    return nullptr;
  }

  std::map<std::string, std::vector<const Content_ModuleSymbol*>, std::less<>>
      by_module_;
};

int LoadData(Modules& modules) {
  int failed = 0;
  for (unsigned index = 0; index < Content_moduleDataCount; ++index) {
    const Content_ModuleDataEntry& entry = Content_moduleData[index];
    const std::vector<u8>& image = modules.Get(entry.module);
    if (!Content::CopyModuleDataEntry(entry, image)) {
      if (!image.empty()) {
        VS_LOG_ERROR(
            "content",
            "module data: {} +0x{:X} ({} bytes, capacity {}) is invalid",
            entry.module, entry.offset, entry.size, entry.capacity);
      }
      ++failed;
      continue;
    }
  }
  return failed;
}

bool RelocateTable(const Content_ModulePointerTable& table,
                   std::span<const u8> image, const SymbolMap& symbols) {
  if (table.module == nullptr ||
      (table.count != 0 && table.target == nullptr) ||
      table.count > table.capacity ||
      static_cast<u64>(table.offset) +
              static_cast<u64>(table.count) * sizeof(u32) >
          image.size()) {
    VS_LOG_ERROR(
        "content",
        "pointer table {} +0x{:X}: {} entries exceed source or capacity {}",
        table.module ? table.module : "<missing>", table.offset, table.count,
        table.capacity);
    return false;
  }
  std::vector<void*> resolved(table.count);
  unsigned unresolved = 0;
  for (unsigned index = 0; index < table.count; ++index) {
    std::array<u8, sizeof(u32)> address_bytes{};
    std::ranges::copy(
        image.subspan(table.offset + static_cast<usize>(index) * sizeof(u32),
                      sizeof(u32)),
        address_bytes.begin());
    const u32 address = std::bit_cast<u32>(address_bytes);
    resolved[index] =
        address != 0 ? symbols.Resolve(table.module, address) : nullptr;
    if (address != 0 && resolved[index] == nullptr) {
      VS_LOG_ERROR(
          "content",
          "pointer table {} +0x{:X}: [{}] 0x{:08X} has no safe native mapping",
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

int RelocatePointers(Modules& modules) {
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

}  // namespace

namespace VS::Content {

bool CopyModuleDataEntry(const Content_ModuleDataEntry& entry,
                         std::span<const unsigned char> image) {
  if (entry.target == nullptr || entry.size > entry.capacity ||
      static_cast<u64>(entry.offset) + entry.size > image.size()) {
    return false;
  }
  if (entry.size != 0) {
    std::ranges::copy(image.subspan(entry.offset, entry.size),
                      static_cast<u8*>(entry.target));
  }
  return true;
}

void* ResolveModuleAddress(std::span<const Content_ModuleSymbol> symbols,
                           std::string_view module, unsigned address) {
  return SymbolMap(symbols).Resolve(module, address);
}

bool RelocateModulePointerTable(const Content_ModulePointerTable& table,
                                std::span<const unsigned char> image,
                                std::span<const Content_ModuleSymbol> symbols) {
  return RelocateTable(table, image, SymbolMap(symbols));
}

}  // namespace VS::Content

extern "C" int Content_LoadModuleData(void) {
  const DataStore* store = DataStore::Get();
  if (store == nullptr) {
    VS_LOG_ERROR("content", "module data: no extracted data");
    return static_cast<int>(Content_moduleDataCount +
                            Content_modulePointerCount);
  }
  Modules modules(*store);
  const int data_failed = LoadData(modules);
  const int pointers_failed = RelocatePointers(modules);
  VS_LOG_INFO("content",
              "module data: {} globals, {} pointer tables ({} symbols) from {} "
              "modules; {} failed",
              Content_moduleDataCount, Content_modulePointerCount,
              Content_moduleSymbolCount, modules.Count(),
              data_failed + pointers_failed);
  return data_failed + pointers_failed;
}
