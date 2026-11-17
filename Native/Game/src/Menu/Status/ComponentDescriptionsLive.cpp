#include "Menu/Status/ComponentDescriptionsLive.hpp"

#include <bit>
#include <cstdint>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Status/ComponentDescriptions.hpp"

extern "C" {
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
}

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
namespace {
void Refuse(std::string_view message) {
  VS_LOG_WARN("menu.status", "{}", message);
}

class LiveComponentDisplay final : public ComponentDisplay {
 public:
  void ShowBladeStats(const Items::WeaponLoadout& weapon) override {
    auto legacy = std::bit_cast<Items_WeaponLoadout>(weapon);
    Framework::PopulateLiveBladeStats(&legacy);
  }
  void ShowGripStats(const Items::EquipmentPart& grip) override {
    auto legacy = std::bit_cast<Items_EquipmentPartStats>(grip);
    Framework::PopulateLiveGripStats(&legacy);
  }
  void ShowGemStats(const Items::EquipmentPart& gem) override {
    auto legacy = std::bit_cast<Items_EquipmentPartStats>(gem);
    Framework::PopulateLiveGemStats(&legacy);
  }
  void HideComponentBars() override {
    Framework::UpdateLiveDpPpBars(Framework::dp_pp_bars::kStatic);
  }
  void ClearComponentStats() override { Framework::ResetLiveEquipmentStats(); }
  bool SelectRow(int row) override {
    if (Menu_Framework_menuItems == nullptr || row < 0 ||
        static_cast<std::size_t>(row) >= Framework::kRowCount) {
      return false;
    }
    Framework::LiveMenuRow(row).selected = 1;
    return true;
  }
};

char* Resolve(ComponentDescription description,
              Framework::LiveItemRowsHost& host) {
  if (!description) {
    return nullptr;
  }
  return Framework::ResolveLiveItemRowText(*description,
                                           host.StringBuffer().data(), nullptr);
}

using PartFormatter = ComponentDescription (*)(Framework::ItemTextHost&,
                                               const Items::EquipmentPart&,
                                               std::span<std::uint8_t>);

void FormatPart(Items_EquipmentPartStats* part, char* menu_text[],
                PartFormatter format) {
  if (part == nullptr || menu_text == nullptr) {
    Refuse("status component: missing part or description output");
    return;
  }
  Framework::LiveItemRowsHost host;
  menu_text[1] = Resolve(
      format(host, Items::Live::Typed(*part), host.StringBuffer()), host);
}
}  // namespace

// decomp: _initBladeDescription (src/MENU/MENU4.PRG/120.c:228)
void FormatLiveBladeDescription(Items_EquipmentPartStats* blade,
                                char* menu_text[], int* row_types) {
  static_cast<void>(row_types);
  FormatPart(blade, menu_text, FormatBladeDescription);
}

// decomp: _initGripInfo (src/MENU/MENU4.PRG/120.c:254)
void FormatLiveGripDescription(Items_EquipmentPartStats* grip,
                               char* menu_text[], int* row_types) {
  static_cast<void>(row_types);
  FormatPart(grip, menu_text, FormatGripDescription);
}

// decomp: _initGemInfo (src/MENU/MENU4.PRG/120.c:275)
void CopyLiveGemDescription(Items_EquipmentPartStats* gem, char* menu_text[],
                            int* row_types) {
  static_cast<void>(row_types);
  FormatPart(gem, menu_text, CopyGemDescription);
}

// decomp: _drawWeaponInfoRow (src/MENU/MENU4.PRG/120.c:293)
char* SelectLiveWeaponComponent(int row, Items_WeaponLoadout* weapon) {
  if (weapon == nullptr) {
    Refuse("status component: missing weapon");
    return nullptr;
  }
  Framework::LiveItemRowsHost host;
  LiveComponentDisplay display;
  ComponentContext context{host, host.StringBuffer(), display};
  return Resolve(
      SelectWeaponComponent(context, row, Items::Live::Typed(*weapon)), host);
}

// decomp: _drawShieldInfoRow (src/MENU/MENU4.PRG/120.c:339)
char* SelectLiveShieldComponent(int row, Items_ShieldLoadout* shield) {
  if (shield == nullptr) {
    Refuse("status component: missing shield");
    return nullptr;
  }
  Framework::LiveItemRowsHost host;
  LiveComponentDisplay display;
  ComponentContext context{host, host.StringBuffer(), display};
  return Resolve(
      SelectShieldComponent(context, row, Items::Live::Typed(*shield)), host);
}

// decomp: _drawArmorInfoRow (src/MENU/MENU4.PRG/120.c:368)
char* LiveArmorDescription(Items_ArmorLoadout* armor) {
  if (armor == nullptr) {
    Refuse("status component: missing armor");
    return nullptr;
  }
  Framework::LiveItemRowsHost host;
  return Resolve(
      ArmorDescription(host, Items::Live::Typed(*armor), host.StringBuffer()),
      host);
}

// decomp: _drawAccessoryInfoRow (src/MENU/MENU4.PRG/120.c:383)
char* LiveAccessoryDescription(Items_AccessorySpawnLoadout* accessory) {
  if (accessory == nullptr) {
    Refuse("status component: missing accessory");
    return nullptr;
  }
  Framework::LiveItemRowsHost host;
  return Resolve(AccessoryDescription(host, Items::Live::Typed(*accessory),
                                      host.StringBuffer()),
                 host);
}

// decomp: _equipmentDetailElementSelectable (src/MENU/MENU4.PRG/120.c:401)
int IsLiveDetailElementSelectable(int element, int flags) {
  return static_cast<int>(IsDetailElementSelectable(
      element, static_cast<std::uint32_t>(flags),
      static_cast<std::uint8_t>(Menu_Framework_itemStatPage)));
}
}  // namespace VS::Menu::Status
