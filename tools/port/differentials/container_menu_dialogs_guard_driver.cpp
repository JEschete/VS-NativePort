#include <exception>
#include <iostream>
#include <span>
#include <string_view>

#include "Menu/ContainerMenuDialogsLiveFixture.hpp"

namespace {
namespace dialogs = VS::Menu::container;
namespace input = VS::Input;
using LiveFixture = container_dialog_live_tests::DialogLiveFixture;

void MissingDiscardOwner(LiveFixture &fixture) {
  fixture.OpenQuantity(10);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Up);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Circle);
  fixture.RunFrames(dialogs::UpdateLiveDiscardMenu);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Up);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu);
  Menu_Container_containerData = nullptr;
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Circle);
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("expected guard name");
    LiveFixture fixture;
    const std::string_view probe(argv[1]);
    if (probe == "lazy-sort-entry") {
      Menu_Framework_itemHelp = nullptr;
      Menu_Framework_menuItems = nullptr;
      if (dialogs::UpdateLiveSortMenu(1) != 0)
        return 2;
      std::cout << "lazy-sort-entry preserved\n";
      return 0;
    }
    if (probe == "lazy-transfer-owners") {
      Menu_Container_containerData = nullptr;
      Menu_Container_otherStorage = nullptr;
      if (dialogs::UpdateLiveTransferConfirmation(1) != 0)
        return 2;
      if (dialogs::UpdateLiveTransferConfirmation(0) != 0)
        return 2;
      std::cout << "lazy-transfer-owners preserved\n";
      return 0;
    }
    if (probe == "missing-sort-text") {
      Menu_Framework_itemHelp = nullptr;
      dialogs::UpdateLiveSortMenu(1);
      dialogs::UpdateLiveSortMenu(0);
    } else if (probe == "missing-restore-rows") {
      Menu_Framework_menuItems = nullptr;
      dialogs::RestoreLiveItemRows(0);
    } else if (probe == "missing-discard-owner") {
      MissingDiscardOwner(fixture);
    } else if (probe == "missing-quantity-frame") {
      fixture.OpenQuantity(10);
      LiveFixture::Buttons(input::Up);
      Render_frameScratch.primitiveCursor = nullptr;
      dialogs::UpdateLiveDiscardMenu(0);
    } else {
      throw std::runtime_error("unknown guard");
    }
    std::cerr << "required guard did not refuse\n";
    return 3;
  } catch (const std::exception &error) {
    std::cerr << "guard setup failed: " << error.what() << '\n';
    return 2;
  }
}
