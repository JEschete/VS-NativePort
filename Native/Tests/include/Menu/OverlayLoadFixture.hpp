#pragma once

#include <cstdint>
#include <stdexcept>

#include "Content/CdDriveLive.hpp"
#include "Content/DiscQueue.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"

namespace menu_overlay_tests {

struct Load {
  equipment_detail_tests::ValueRestore<decltype(VS::Menu::Framework::menuState)>
      saved_control{VS::Menu::Framework::menuState};
  equipment_detail_tests::ValueRestore<
      decltype(VS::Menu::Framework::loadedSubMenu)>
      saved_loaded{VS::Menu::Framework::loadedSubMenu};
  equipment_detail_tests::ValueRestore<
      decltype(VS::Menu::Framework::openedWithoutMainMenu)>
      saved_without_main{VS::Menu::Framework::openedWithoutMainMenu};
  equipment_detail_tests::ValueRestore<
      decltype(VS::Menu::Framework::shortcutInvoked)>
      saved_shortcut{VS::Menu::Framework::shortcutInvoked};
  equipment_detail_tests::ValueRestore<
      decltype(VS::Menu::Framework::menuBackgroundBuffer)>
      saved_background{VS::Menu::Framework::menuBackgroundBuffer};
  VS::Content::DiscQueueSlot* request;
  std::uint64_t generation = 0;
  int submenu;

  Load(int menu, VS::Content::DiscFile file)
      : request(VS::Content::AllocateLiveDiscRequest(&file)), submenu(menu) {
    if (request == nullptr) {
      throw std::runtime_error("overlay fixture could not own a disc request");
    }
    generation = VS::Content::LiveDiscRequestGeneration(request);
    // This controlled completion has no queued destination. Leave it allocated
    // so genuine owner cleanup can update queue counts without restoration.
    VS::Menu::Framework::loadedSubMenu = static_cast<signed char>(menu);
    VS::Menu::Framework::shortcutInvoked = 0;
    VS::Menu::Framework::openedWithoutMainMenu = 1;
    VS::Menu::Framework::menuBackgroundBuffer = nullptr;
    VS::Menu::Framework::menuState.control.loading = 1;
    VS::Menu::Framework::menuState.cd_queue = request;
  }
  ~Load() {
    if (VS::Content::LiveDiscRequestGeneration(request) == generation) {
      VS::Content::ReleaseLiveDiscRequest(request);
    }
  }
  int Poll() const { return VS::Menu::Framework::LoadLiveMenuOverlay(submenu); }
  int Complete() {
    request->state = VS::Content::DiscRequestState::kLoaded;
    return Poll();
  }
};

}  // namespace menu_overlay_tests
