#pragma once

#include <cstdint>
#include <stdexcept>

#include "Content/CdDriveLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"

extern "C" {
#include "DiscQueue.h"
}

namespace menu_overlay_tests {

struct Load {
  equipment_detail_tests::ValueRestore<decltype(Menu_Framework_menuState)>
      saved_control{Menu_Framework_menuState};
  equipment_detail_tests::ValueRestore<decltype(Menu_Framework_loadedSubMenu)>
      saved_loaded{Menu_Framework_loadedSubMenu};
  equipment_detail_tests::ValueRestore<
      decltype(Menu_Framework_openedWithoutMainMenu)>
      saved_without_main{Menu_Framework_openedWithoutMainMenu};
  equipment_detail_tests::ValueRestore<decltype(Menu_Framework_shortcutInvoked)>
      saved_shortcut{Menu_Framework_shortcutInvoked};
  equipment_detail_tests::ValueRestore<
      decltype(Menu_Framework_menuBackgroundBuffer)>
      saved_background{Menu_Framework_menuBackgroundBuffer};
  Content_DiscQueueSlot* request;
  std::uint64_t generation = 0;
  int submenu;

  Load(int menu, Content_DiscFile file)
      : request(VS::Content::AllocateLiveDiscRequest(&file)), submenu(menu) {
    if (request == nullptr) {
      throw std::runtime_error("overlay fixture could not own a disc request");
    }
    generation = VS::Content::LiveDiscRequestGeneration(request);
    // This controlled completion has no queued destination. Leave it allocated
    // so genuine owner cleanup can update queue counts without restoration.
    Menu_Framework_loadedSubMenu = static_cast<signed char>(menu);
    Menu_Framework_shortcutInvoked = 0;
    Menu_Framework_openedWithoutMainMenu = 1;
    Menu_Framework_menuBackgroundBuffer = nullptr;
    Menu_Framework_menuState.loading = 1;
    Menu_Framework_menuState.cdQueue = request;
  }
  ~Load() {
    if (VS::Content::LiveDiscRequestGeneration(request) == generation) {
      VS::Content::ReleaseLiveDiscRequest(request);
    }
  }
  int Poll() const { return VS::Menu::Framework::LoadLiveMenuOverlay(submenu); }
  int Complete() {
    request->state = Content_DiscRequestState_Loaded;
    return Poll();
  }
};

}  // namespace menu_overlay_tests
