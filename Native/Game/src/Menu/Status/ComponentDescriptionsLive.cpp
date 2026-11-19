#include "Menu/Status/ComponentDescriptionsLive.hpp"

#include <cstdint>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Status/ComponentDescriptions.hpp"

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
namespace {
void Refuse(std::string_view message) {
  VS::Core::LogWarn("menu.status", "{}", message);
}

class LiveComponentDisplay final : public ComponentDisplay {
 public:
  void ShowBladeStats(const Items::WeaponLoadout& weapon) override {
    Framework::PopulateLiveBladeStats(&weapon);
  }
  void ShowGripStats(const Items::EquipmentPart& grip) override {
    Framework::PopulateLiveGripStats(&grip);
  }
  void ShowGemStats(const Items::EquipmentPart& gem) override {
    Framework::PopulateLiveGemStats(&gem);
  }
  void HideComponentBars() override {
    Framework::UpdateLiveDpPpBars(Framework::dp_pp_bars::kStatic);
  }
  void ClearComponentStats() override { Framework::ResetLiveEquipmentStats(); }
  bool SelectRow(int row) override {
    if (VS::Menu::Framework::menuItems == nullptr || row < 0 ||
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

void FormatPart(VS::Items::EquipmentPart* part, char* menu_text[],
                PartFormatter format) {
  if (part == nullptr || menu_text == nullptr) {
    Refuse("status component: missing part or description output");
    return;
  }
  Framework::LiveItemRowsHost host;
  menu_text[1] = Resolve(format(host, *part, host.StringBuffer()), host);
}
}  // namespace

// decomp: _initBladeDescription (src/MENU/MENU4.PRG/120.c:228)
void FormatLiveBladeDescription(VS::Items::EquipmentPart* blade,
                                char* menu_text[], int* row_types) {
  static_cast<void>(row_types);
  FormatPart(blade, menu_text, FormatBladeDescription);
}

// decomp: _initGripInfo (src/MENU/MENU4.PRG/120.c:254)
void FormatLiveGripDescription(VS::Items::EquipmentPart* grip,
                               char* menu_text[], int* row_types) {
  static_cast<void>(row_types);
  FormatPart(grip, menu_text, FormatGripDescription);
}

// decomp: _initGemInfo (src/MENU/MENU4.PRG/120.c:275)
void CopyLiveGemDescription(VS::Items::EquipmentPart* gem, char* menu_text[],
                            int* row_types) {
  static_cast<void>(row_types);
  FormatPart(gem, menu_text, CopyGemDescription);
}

// decomp: _drawWeaponInfoRow (src/MENU/MENU4.PRG/120.c:293)
char* SelectLiveWeaponComponent(int row, VS::Items::WeaponLoadout* weapon) {
  if (weapon == nullptr) {
    Refuse("status component: missing weapon");
    return nullptr;
  }
  Framework::LiveItemRowsHost host;
  LiveComponentDisplay display;
  ComponentContext context{host, host.StringBuffer(), display};
  return Resolve(SelectWeaponComponent(context, row, *weapon), host);
}

// decomp: _drawShieldInfoRow (src/MENU/MENU4.PRG/120.c:339)
char* SelectLiveShieldComponent(int row, VS::Items::ShieldLoadout* shield) {
  if (shield == nullptr) {
    Refuse("status component: missing shield");
    return nullptr;
  }
  Framework::LiveItemRowsHost host;
  LiveComponentDisplay display;
  ComponentContext context{host, host.StringBuffer(), display};
  return Resolve(SelectShieldComponent(context, row, *shield), host);
}

// decomp: _drawArmorInfoRow (src/MENU/MENU4.PRG/120.c:368)
char* LiveArmorDescription(VS::Items::ArmorLoadout* armor) {
  if (armor == nullptr) {
    Refuse("status component: missing armor");
    return nullptr;
  }
  Framework::LiveItemRowsHost host;
  return Resolve(ArmorDescription(host, *armor, host.StringBuffer()), host);
}

// decomp: _drawAccessoryInfoRow (src/MENU/MENU4.PRG/120.c:383)
char* LiveAccessoryDescription(VS::Items::AccessorySpawnLoadout* accessory) {
  if (accessory == nullptr) {
    Refuse("status component: missing accessory");
    return nullptr;
  }
  Framework::LiveItemRowsHost host;
  return Resolve(AccessoryDescription(host, *accessory, host.StringBuffer()),
                 host);
}

// decomp: _equipmentDetailElementSelectable (src/MENU/MENU4.PRG/120.c:401)
int IsLiveDetailElementSelectable(int element, int flags) {
  return static_cast<int>(IsDetailElementSelectable(
      element, static_cast<std::uint32_t>(flags),
      static_cast<std::uint8_t>(VS::Menu::Framework::itemStatPage)));
}
}  // namespace VS::Menu::Status
