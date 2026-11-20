#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <span>
#include <type_traits>

#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/Sphere.hpp"
#include "Battle/TargetPages.hpp"
#include "Battle/TargetVolume.hpp"
#include "Battle/TargetingLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Log.hpp"
#include "Field/Interactions.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPackets.hpp"

using VS::Battle::ActionRequestRecord;
using VS::Battle::ActionState;

using VS::Legacy::kPadL1;
using VS::Legacy::kPadR1;

namespace {
constexpr auto kSlots = std::tuple_size_v<decltype(ActionState::previews)>;
using ResultBuffer = decltype(ActionRequestRecord::targets);
using SphereRecord = VS::Battle::TargetSphereRecord;
constexpr auto kPageSize = std::extent_v<decltype(SphereRecord::labels)>;
constexpr unsigned char kNoTarget = std::numeric_limits<unsigned char>::max();
constexpr std::uint32_t kChooseDirectTarget = 1;
constexpr std::uint32_t kChooseAreaTarget = 3;
constexpr int kHintX = 112;
constexpr int kHintY = 32;
constexpr int kHintColor = 0x808080;
constexpr std::size_t kHintBufferSize = 32;
static_assert(kSlots == kNoTarget);
static_assert(kSlots == std::tuple_size_v<ResultBuffer>);
static_assert(kSlots == VS::Battle::kMaxSphereTargets);

// The bound candidates and their pages. The candidates are viewed, not owned.
std::span<VS::Battle::TargetPreview> targets;
VS::Battle::TargetPageBook book;

VS::Battle::TargetSphereRecord* Sphere() {
  return reinterpret_cast<VS::Battle::TargetSphereRecord*>(
      VS::Battle::targetSphere);
}

void ApplyPage(VS::Battle::TargetPage page, bool reset) {
  auto* sphere = Sphere();
  sphere->target_count = static_cast<char>(page.count);
  sphere->target_lists[0] =
      targets.data() == nullptr ? nullptr : targets.subspan(page.first).data();
  if (reset ||
      static_cast<unsigned char>(sphere->selected_target) >= page.count) {
    sphere->selected_target = static_cast<char>(page.count == 0 ? -1 : 0);
  }
  VS::Battle::LiveUpdateTargetSelectionMenu(nullptr);
}

// The active battle sphere, showing pages of entries.
class LiveSphere final : public VS::Battle::TargetSphere {
 public:
  explicit LiveSphere(std::span<VS::Battle::TargetPreview> entries)
      : entries_(entries) {}

  [[nodiscard]] bool Active() const override { return Sphere() != nullptr; }

  [[nodiscard]] std::size_t VisibleSlots() const override { return kPageSize; }

  void ShowPage(VS::Battle::TargetPage page, int mode) override {
    targets = entries_;
    Sphere()->target_mode = static_cast<char>(mode);
    ApplyPage(page, false);
  }

 private:
  std::span<VS::Battle::TargetPreview> entries_;
};
}  // namespace

void VS::Battle::ResetTargetPages(void) {
  targets = {};
  book = {};
}

namespace VS::Battle {
void SetLiveSphereTargets(std::span<VS::Battle::TargetPreview> entries,
                          int mode) {
  ResetTargetPages();
  if (entries.size() > kMaxSphereTargets) {
    VS::Core::LogError("targeting.page", "invalid targets count={}",
                       entries.size());
    std::abort();
  }
  std::array<std::uint8_t, kMaxSphereTargets> actor_ids{};
  std::transform(entries.begin(), entries.end(), actor_ids.begin(),
                 [](const VS::Battle::TargetPreview& entry) {
                   return static_cast<std::uint8_t>(entry.actor_id);
                 });
  LiveSphere sphere(entries);
  SetSphereTargets(book, std::span(actor_ids).first(entries.size()), mode,
                   sphere);
}

std::span<VS::Battle::TargetPreview> CheckedTargetPreviews(
    std::span<VS::Battle::TargetPreview> previews, int count) {
  if (count < 0 || static_cast<std::size_t>(count) > previews.size()) {
    VS::Core::LogError("targeting.page", "invalid target preview count={}",
                       count);
    std::abort();
  }
  return previews.first(static_cast<std::size_t>(count));
}
}  // namespace VS::Battle

int VS::Battle::AdvanceTargetPage(int direction) {
  if (direction != -1 && direction != 1) {
    VS::Core::LogError("targeting.page", "invalid page direction={}",
                       direction);
    std::abort();
  }
  if (book.pages.size() < 2 || Sphere() == nullptr) return 0;
  VS::Battle::LiveUpdateTargetSelectionHighlight(1);
  if (direction > 0) {
    book.current = (book.current + 1) % book.pages.size();
  } else {
    book.current = book.current == 0 ? book.pages.size() - 1 : book.current - 1;
  }
  std::fill(std::begin(Sphere()->labels), std::end(Sphere()->labels),
            VS::Battle::SphereTargetLabelRecord{});
  ApplyPage(book.pages[book.current], true);
  const auto info = VS::Battle::GetTargetPageInfo();
  VS::Core::LogInfo("targeting.page", "page={}/{} first={} visible={} total={}",
                    info.page, info.pages, info.first, info.visible,
                    info.total);
  return 1;
}

int VS::Battle::HandleTargetPageButtons(void) {
  const auto pressed = VS::Input::buttonsPressed & (kPadL1 | kPadR1);
  int changed = 0;
  if (pressed == kPadL1) changed = VS::Battle::AdvanceTargetPage(-1);
  if (pressed == kPadR1) changed = VS::Battle::AdvanceTargetPage(1);
  if (changed) VS::Menu::PlayChangeSound();
  return changed;
}

int VS::Battle::TargetPagesUseShoulders(void) {
  return book.pages.size() > 1 && Sphere() != nullptr &&
         Sphere()->target_count > 0 && VS::Field::controlMode == 2 &&
         (VS::Battle::actionFlowMode == kChooseDirectTarget ||
          VS::Battle::actionFlowMode == kChooseAreaTarget);
}

unsigned char VS::Battle::MapTargetPageIndex(unsigned char index) {
  if (index == kNoTarget) return index;
  if (book.pages.empty() || index >= book.pages[book.current].count) {
    VS::Core::LogError("targeting.page", "invalid local target index={}",
                       static_cast<unsigned>(index));
    std::abort();
  }
  return static_cast<unsigned char>(book.pages[book.current].first + index);
}

int VS::Battle::ValidateTargetPreviewIndex(int index) {
  if (!VS::Battle::TargetPreviewIndexIsValid(index)) {
    VS::Core::LogError("targeting", "target preview index exceeds capacity: {}",
                       index);
    std::abort();
  }
  return index;
}

int VS::Battle::TargetPreviewIndexIsValid(int index) {
  return index >= 0 && static_cast<std::size_t>(index) < kSlots;
}

VS::Battle::TargetPageInfo VS::Battle::GetTargetPageInfo(void) {
  VS::Battle::TargetPageInfo info{0, 0, 0, 0, 0, -1};
  if (book.pages.empty() || Sphere() == nullptr) return info;
  const VS::Battle::TargetPage page = book.pages[book.current];
  info.total = static_cast<int>(book.total);
  info.page = static_cast<int>(book.current + 1);
  info.pages = static_cast<int>(book.pages.size());
  info.first = static_cast<int>(page.first);
  info.visible = static_cast<int>(page.count);
  const auto selected = static_cast<unsigned char>(Sphere()->selected_target);
  if (selected < info.visible) info.selected = info.first + selected;
  return info;
}

void VS::Battle::DrawTargetPageHint(void) {
  if (book.pages.size() < 2 || Sphere() == nullptr) return;
  const auto info = VS::Battle::GetTargetPageInfo();
  std::array<char, kHintBufferSize> text{};
  std::snprintf(text.data(), text.size(), "L1  %d/%d  R1", info.page,
                info.pages);
  VS::Render::DrawLiveCompactTextColor(
      text.data(), VS::Render::PackScreenWord(kHintX, kHintY), kHintColor,
      nullptr);
}
