#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/EquipmentDetailControllers.hpp"

extern "C" {
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
}

namespace VS::Menu::Inventory {
template <typename World>
class LiveDetailWorld : public World {
 public:
  std::int32_t RowsReady() override {
    require_rows_();
    return Framework::AreLiveMenuRowsIdle();
  }
  std::uint32_t PressedButtons() override {
    return Input_buttonsPressed.allButtons;
  }
  void BeginTransition(DetailRowId row) override {
    BeginLiveDetailsTransition(row.index);
  }
  void InitializeLayout(DetailRowId row, DetailStatPages pages) override {
    InitializeLiveDetailsLayout(row.index, static_cast<int>(pages));
  }

 protected:
  explicit LiveDetailWorld(void (*require_rows)())
      : require_rows_(require_rows) {}

 private:
  void (*require_rows_)();
};

inline std::span<char> FindLiveItemDetailText(char* text,
                                              std::span<char> borrowed_buffer) {
  const auto address = reinterpret_cast<std::uintptr_t>(text);
  const std::array tables{
      borrowed_buffer,
      std::span<char>{Menu_Framework_stringBuf,
                      Menu_Framework_stringBuf == nullptr
                          ? 0
                          : Framework::kStringBufferBytes},
      std::span<char>{
          reinterpret_cast<char*>(Menu_Framework_itemHelp),
          Menu_Framework_itemHelp == nullptr ? 0 : Framework::kItemHelpBytes},
      std::span<char>{reinterpret_cast<char*>(Menu_Framework_itemNames),
                      Menu_Framework_itemNames == nullptr
                          ? 0
                          : Framework::kItemNamesBytes}};
  for (const auto table : tables) {
    const auto first = reinterpret_cast<std::uintptr_t>(table.data());
    if (!table.empty() && address >= first && address - first < table.size()) {
      return {text, table.size() - (address - first)};
    }
  }
  return {};
}
}  // namespace VS::Menu::Inventory
