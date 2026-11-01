#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <span>
#include <type_traits>

#include "Battle/TargetPages.hpp"
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
static_assert(kSlots == VS::Battle::kMaxSphereTargets);

// The bound candidates and their pages. The candidates are viewed, not owned.
std::span<Battle_TargetPreview> targets;
VS::Battle::TargetPageBook book;

Battle_TargetSphere* Sphere() {
  return reinterpret_cast<Battle_TargetSphere*>(Battle_targetSphere);
}

void ApplyPage(VS::Battle::TargetPage page, bool reset) {
  auto* sphere = Sphere();
  sphere->targetCount = static_cast<char>(page.count);
  sphere->targetLists[0] =
      targets.data() == nullptr ? nullptr : targets.subspan(page.first).data();
  if (reset ||
      static_cast<unsigned char>(sphere->selectedTarget) >= page.count) {
    sphere->selectedTarget = static_cast<char>(page.count == 0 ? -1 : 0);
  }
  Battle_UpdateTargetSelectionMenu(nullptr);
}

// The active battle sphere, showing pages of entries.
class LiveSphere final : public VS::Battle::TargetSphere {
 public:
  explicit LiveSphere(std::span<Battle_TargetPreview> entries)
      : entries_(entries) {}

  [[nodiscard]] bool Active() const override { return Sphere() != nullptr; }

  [[nodiscard]] std::size_t VisibleSlots() const override { return kPageSize; }

  void ShowPage(VS::Battle::TargetPage page, int mode) override {
    targets = entries_;
    Sphere()->targetMode = static_cast<char>(mode);
    ApplyPage(page, false);
  }

 private:
  std::span<Battle_TargetPreview> entries_;
};
}  // namespace

void Battle_ResetTargetPages(void) {
  targets = {};
  book = {};
}

namespace VS::Battle {
void SetLiveSphereTargets(std::span<Battle_TargetPreview> entries, int mode) {
  Battle_ResetTargetPages();
  if (entries.size() > kMaxSphereTargets) {
    VS_LOG_ERROR("targeting.page", "invalid targets count={}", entries.size());
    std::abort();
  }
  std::array<std::uint8_t, kMaxSphereTargets> actor_ids{};
  std::transform(entries.begin(), entries.end(), actor_ids.begin(),
                 [](const Battle_TargetPreview& entry) {
                   return static_cast<std::uint8_t>(entry.actorId);
                 });
  LiveSphere sphere(entries);
  SetSphereTargets(book, std::span(actor_ids).first(entries.size()), mode,
                   sphere);
}

std::span<Battle_TargetPreview> CheckedTargetPreviews(
    std::span<Battle_TargetPreview> previews, int count) {
  if (count < 0 || static_cast<std::size_t>(count) > previews.size()) {
    VS_LOG_ERROR("targeting.page", "invalid target preview count={}", count);
    std::abort();
  }
  return previews.first(static_cast<std::size_t>(count));
}
}  // namespace VS::Battle

int Battle_AdvanceTargetPage(int direction) {
  if (direction != -1 && direction != 1) {
    VS_LOG_ERROR("targeting.page", "invalid page direction={}", direction);
    std::abort();
  }
  if (book.pages.size() < 2 || Sphere() == nullptr) return 0;
  Battle_UpdateTargetSelectionHighlight(1);
  if (direction > 0) {
    book.current = (book.current + 1) % book.pages.size();
  } else {
    book.current = book.current == 0 ? book.pages.size() - 1 : book.current - 1;
  }
  std::fill(std::begin(Sphere()->labels), std::end(Sphere()->labels),
            Battle_SphereTargetLabel{});
  ApplyPage(book.pages[book.current], true);
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
  return book.pages.size() > 1 && Sphere() != nullptr &&
         Sphere()->targetCount > 0 && Field_controlMode == 2 &&
         (Battle_actionFlowMode == kChooseDirectTarget ||
          Battle_actionFlowMode == kChooseAreaTarget);
}

unsigned char Battle_MapTargetPageIndex(unsigned char index) {
  if (index == kNoTarget) return index;
  if (book.pages.empty() || index >= book.pages[book.current].count) {
    VS_LOG_ERROR("targeting.page", "invalid local target index={}",
                 static_cast<unsigned>(index));
    std::abort();
  }
  return static_cast<unsigned char>(book.pages[book.current].first + index);
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
  if (book.pages.empty() || Sphere() == nullptr) return info;
  const VS::Battle::TargetPage page = book.pages[book.current];
  info.total = static_cast<int>(book.total);
  info.page = static_cast<int>(book.current + 1);
  info.pages = static_cast<int>(book.pages.size());
  info.first = static_cast<int>(page.first);
  info.visible = static_cast<int>(page.count);
  const auto selected = static_cast<unsigned char>(Sphere()->selectedTarget);
  if (selected < info.visible) info.selected = info.first + selected;
  return info;
}

void Battle_DrawTargetPageHint(void) {
  if (book.pages.size() < 2 || Sphere() == nullptr) return;
  const auto info = Battle_GetTargetPageInfo();
  std::array<char, kHintBufferSize> text{};
  std::snprintf(text.data(), text.size(), "L1  %d/%d  R1", info.page,
                info.pages);
  Render_DrawCompactTextColor(text.data(), vs_getXY_2(kHintX, kHintY),
                              kHintColor, nullptr);
}
