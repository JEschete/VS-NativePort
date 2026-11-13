#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"

extern "C" {
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
}

namespace VS::Menu::Framework {
using LiveDetailController = int (*)(int);

inline std::optional<int> RunLiveDetailController(
    std::uint8_t category, int command,
    std::span<const LiveDetailController> controllers) {
  if (category >= controllers.size()) return std::nullopt;
  return controllers[category](command);
}

inline void SetLiveDetailCommand(bool menu) {
  SetLiveCommandLabel(menu ? Menu_Framework_MenuCommand_Menu
                           : Menu_Framework_MenuCommand_None);
}

template <typename World, void (*CheckRows)(), void (*CheckFrame)(bool)>
class LiveDetailDispatcherWorld : public World {
 public:
  explicit LiveDetailDispatcherWorld(std::string_view cursor_error)
      : cursor_error_(cursor_error) {}

  void ResetDetailCursor() override { ResetLiveItemDetailCursor(); }
  std::int32_t RowsReady() override {
    CheckRows();
    return AreLiveMenuRowsIdle();
  }
  std::uint8_t SelectedDetailElement() override {
    return Menu_Framework_selectedStatusViewElement;
  }
  void SetCursorBright(bool bright) override {
    Menu_Framework_cursorColor = bright ? 1 : 0;
  }
  void DrawDetailTabNavigation() override {
    CheckFrame(true);
    DrawLiveTabNavigation(1);
  }
  void DrawItemDetails() override {
    CheckRows();
    if (Menu_Framework_selectedStatusViewElement >= kStatusViewElementCount) {
      this->Refuse(cursor_error_);
    }
    UpdateLiveItemDetails();
  }

 private:
  const std::string_view cursor_error_;
};
}  // namespace VS::Menu::Framework
