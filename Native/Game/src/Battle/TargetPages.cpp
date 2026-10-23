#include "Battle/TargetPages.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <type_traits>
#include <vector>

#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Sphere.h"
#include "Battle/Targeting.h"
#include "Field/Interactions.h"
#include "Input/Controller.h"
#include "Menu/Sounds.h"
#include "Render/Font.h"
}

namespace {
constexpr auto kSlots = std::extent_v<decltype(Battle_ActionState::previews)>;
using ResultBuffer = decltype(Battle_ActionRequest::targets);
constexpr auto kPageSize = std::extent_v<decltype(Battle_TargetSphere::labels)>;
constexpr unsigned char kNoTarget = std::numeric_limits<unsigned char>::max();
constexpr u_int kChooseDirectTarget = 1;
constexpr u_int kChooseAreaTarget = 3;
constexpr int kHintX = 112;
constexpr int kHintY = 32;
constexpr int kHintColor = 0x808080;
constexpr std::size_t kHintBufferSize = 32;
static_assert(kSlots == kNoTarget);
static_assert(kSlots == std::extent_v<ResultBuffer>);

struct Page {
  int first;
  int count;
};
Battle_TargetPreview* targets;
int total;
std::vector<Page> pages;
std::size_t current;

Battle_TargetSphere* Sphere() {
  return reinterpret_cast<Battle_TargetSphere*>(Battle_targetSphere);
}

void ApplyPage(bool reset) {
  auto* sphere = Sphere();
  const Page page = pages.empty() ? Page{0, 0} : pages[current];
  sphere->targetCount = static_cast<char>(page.count);
  sphere->targetLists[0] = targets == nullptr ? nullptr : targets + page.first;
  if (reset ||
      static_cast<unsigned char>(sphere->selectedTarget) >= page.count) {
    sphere->selectedTarget = static_cast<char>(page.count == 0 ? -1 : 0);
  }
  Battle_UpdateTargetSelectionMenu(nullptr);
}
}  // namespace

void Battle_ResetTargetPages(void) {
  targets = nullptr;
  total = 0;
  current = 0;
  pages.clear();
}

// func_800C1664, BATTLE/BATTLE.PRG/58578.c:446. The original visible list
// remains bounded; native pages retain every candidate and whole actor groups.
void Battle_SetSphereTargets(int count, Battle_TargetPreview* entries,
                             int mode) {
  Battle_ResetTargetPages();
  if (Sphere() == nullptr) {
    VS_LOG_DEBUG("targeting.page", "no active sphere for target binding");
    return;
  }
  if (count < 0 || static_cast<std::size_t>(count) > kSlots ||
      (count != 0 && entries == nullptr)) {
    VS_LOG_ERROR("targeting.page", "invalid targets count={} data={}", count,
                 static_cast<void*>(entries));
    std::abort();
  }
  targets = entries;
  total = count;
  for (int first = 0; first < count;) {
    int end = first;
    while (end < count) {
      int group_end = end + 1;
      while (group_end < count &&
             entries[group_end].actorId == entries[end].actorId) {
        ++group_end;
      }
      if (static_cast<std::size_t>(group_end - end) > kPageSize) {
        VS_LOG_ERROR("targeting.page", "actor group exceeds visible capacity");
        std::abort();
      }
      if (static_cast<std::size_t>(group_end - first) > kPageSize) break;
      end = group_end;
    }
    pages.push_back({first, end - first});
    first = end;
  }
  Sphere()->targetMode = static_cast<char>(mode);
  ApplyPage(false);
}

int Battle_AdvanceTargetPage(int direction) {
  if (direction != -1 && direction != 1) {
    VS_LOG_ERROR("targeting.page", "invalid page direction={}", direction);
    std::abort();
  }
  if (pages.size() < 2 || Sphere() == nullptr) return 0;
  Battle_UpdateTargetSelectionHighlight(1);
  if (direction > 0) {
    current = (current + 1) % pages.size();
  } else {
    current = current == 0 ? pages.size() - 1 : current - 1;
  }
  std::fill(std::begin(Sphere()->labels), std::end(Sphere()->labels),
            Battle_SphereTargetLabel{});
  ApplyPage(true);
  const auto info = Battle_GetTargetPageInfo();
  VS_LOG_INFO("targeting.page", "page={}/{} first={} visible={} total={}",
              info.page, info.pages, info.first, info.visible, info.total);
  return 1;
}

int Battle_HandleTargetPageButtons(void) {
  const auto pressed = Input_buttonsPressed.allButtons & (PADL1 | PADR1);
  int changed = 0;
  if (pressed == PADL1) changed = Battle_AdvanceTargetPage(-1);
  if (pressed == PADR1) changed = Battle_AdvanceTargetPage(1);
  if (changed) Menu_PlayChangeSound();
  return changed;
}

int Battle_TargetPagesUseShoulders(void) {
  return pages.size() > 1 && Sphere() != nullptr && Sphere()->targetCount > 0 &&
         Field_controlMode == 2 &&
         (Battle_actionFlowMode == kChooseDirectTarget ||
          Battle_actionFlowMode == kChooseAreaTarget);
}

unsigned char Battle_MapTargetPageIndex(unsigned char index) {
  if (index == kNoTarget) return index;
  if (pages.empty() || index >= pages[current].count) {
    VS_LOG_ERROR("targeting.page", "invalid local target index={}",
                 static_cast<unsigned>(index));
    std::abort();
  }
  return static_cast<unsigned char>(pages[current].first + index);
}

int Battle_ValidateTargetPreviewIndex(int index) {
  if (!Battle_TargetPreviewIndexIsValid(index)) {
    VS_LOG_ERROR("targeting", "target preview index exceeds capacity: {}",
                 index);
    std::abort();
  }
  return index;
}

int Battle_TargetPreviewIndexIsValid(int index) {
  return index >= 0 && static_cast<std::size_t>(index) < kSlots;
}

Battle_TargetPageInfo Battle_GetTargetPageInfo(void) {
  Battle_TargetPageInfo info{0, 0, 0, 0, 0, -1};
  if (pages.empty() || Sphere() == nullptr) return info;
  info.total = total;
  info.page = static_cast<int>(current + 1);
  info.pages = static_cast<int>(pages.size());
  info.first = pages[current].first;
  info.visible = pages[current].count;
  const auto selected = static_cast<unsigned char>(Sphere()->selectedTarget);
  if (selected < info.visible) info.selected = info.first + selected;
  return info;
}

void Battle_DrawTargetPageHint(void) {
  if (pages.size() < 2 || Sphere() == nullptr) return;
  const auto info = Battle_GetTargetPageInfo();
  std::array<char, kHintBufferSize> text{};
  std::snprintf(text.data(), text.size(), "L1  %d/%d  R1", info.page,
                info.pages);
  Render_DrawCompactTextColor(text.data(), vs_getXY_2(kHintX, kHintY),
                              kHintColor, nullptr);
}
