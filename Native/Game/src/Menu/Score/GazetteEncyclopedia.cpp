#include "Menu/Score/GazetteEncyclopedia.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Core/String/GameText.hpp"

namespace VS::Menu::Score {
namespace {
namespace math = Core::Math;
constexpr int kSettled = 8;
constexpr int kListX = 60;
constexpr int kListWidth = 200;
constexpr int kFirstRowY = 56;
constexpr int kRowHeight = 16;
constexpr int kListSlideStride = 23;
constexpr int kCursorX = 48;
constexpr int kCursorRaise = 8;
constexpr int kNumberInset = 14;
constexpr std::uint8_t kLockedColor = 3;
constexpr std::uint8_t kScrollUp = 1;
constexpr std::uint8_t kScrollDown = 2;
constexpr int kDecimalBase = 10;
constexpr int kMaximumEnemyNumber = 999;
constexpr int kDigitCount = 3;
constexpr int kDigitRightOffset = 24;
constexpr int kDigitAdvance = 5;
constexpr int kDigitWidth = 6;
constexpr int kDigitRaise = 4;
constexpr int kDigitHeight = 10;
constexpr std::uint16_t kDigitPage = 0x0C;
constexpr std::uint16_t kDimmedDigitPage = 0x2C;
constexpr std::uint16_t kDigitClut = 0x37F4;
constexpr std::uint16_t kDimmedDigitClut = 0x37F7;
constexpr std::uint8_t kBright = 128;
constexpr std::uint8_t kDim = 32;
constexpr std::size_t kBitsPerWord = 32;
constexpr std::uint8_t kNoEnemy = 255;
constexpr int kNoPreview = 3;
constexpr int kSlideFromRight = 2;
constexpr int kProjectionStride = 32;
constexpr int kProjectionX = 160;
constexpr int kProjectionY = 104;
constexpr int kUiStride = 16;
constexpr int kNameX = 184;
constexpr int kNameY = 166;
constexpr int kNameWidth = 256;
constexpr std::size_t kNamePrefixBytes = 2;
constexpr int kButtonInset = 120;
constexpr int kRotationButtonY = 15;
constexpr int kZoomButtonY = 34;
constexpr int kLabelInset = 100;
constexpr int kRotationLabelY = 18;
constexpr int kZoomLabelY = 36;
constexpr int kBannerInset = 112;
constexpr int kRotationBannerWidth = 96;
constexpr int kZoomBannerWidth = 78;
constexpr int kBannerHeight = 12;
constexpr int kControlsGradient = 3;
constexpr int kIdentificationY = 160;
constexpr int kIdentificationInset = 128;
constexpr int kIdentificationWidth = 100;
constexpr int kIdentificationGradient = 2;
constexpr int kClassX = 188;
constexpr int kClassY = 155;
constexpr int kClassPanelX = 180;
constexpr int kClassPanelWidth = 140;
constexpr int kClassPanelHeight = 26;
constexpr int kClassGradient = 1;
constexpr auto kClasses = std::to_array<std::string_view>(
    {"HUMAN", "BEAST", "UNDEAD", "PHANTOM", "DRAGON", "EVIL"});
constexpr std::size_t kIdentificationDigits = 6;
constexpr std::size_t kIdentificationTextBytes = 14;
constexpr int kNavigationMidpoint = 9;
constexpr int kNavigationSize = 16;
constexpr int kNavigationInset = 8;
constexpr int kNavigationY = 96;
constexpr int kNavigationTextY = 98;
constexpr int kLeftTextX = 34;
constexpr int kRightTextX = 286;
constexpr int kLeftArrowX = 32;
constexpr int kLeftArrowReturnX = 26;
constexpr int kRightArrowX = 288;
constexpr int kRightArrowReturnX = 294;
constexpr std::uint8_t kArrowTextureY = 48;
constexpr std::uint16_t kArrowClut = 0x37F8;
constexpr std::uint16_t kTransitionArrowClut = 0x37F9;

bool Terminated(GazetteText text) {
  return std::ranges::find(text, Core::String::kGameTextTerminator) !=
         text.end();
}

bool HasEnemy(std::span<const GazetteEnemy> enemies, std::int32_t selected) {
  return selected >= 0 && static_cast<std::size_t>(selected) <
                              std::min(enemies.size(), kEncyclopediaEnemies);
}

GazetteText Description(const GazetteEnemy& enemy, GazetteText locked) {
  return enemy.unlocked != 0 ? enemy.description : locked;
}

bool ValidList(std::span<const GazetteEnemy> enemies,
               GazetteRecordCursor cursor, GazetteText locked_name,
               GazetteText locked_description) {
  if (enemies.size() < kEncyclopediaEnemies ||
      cursor.first_row > kEncyclopediaEnemies - kGazetteVisibleRecords) {
    return false;
  }
  for (const auto& enemy :
       enemies.subspan(cursor.first_row, kGazetteVisibleRecords)) {
    if (!Terminated(enemy.unlocked != 0 ? enemy.name : locked_name)) {
      return false;
    }
  }
  const std::size_t selected =
      static_cast<std::size_t>(cursor.first_row) + cursor.selected_row;
  return selected >= kEncyclopediaEnemies || enemies[selected].selected != 0 ||
         Terminated(Description(enemies[selected], locked_description));
}

void SelectEnemyRows(std::span<GazetteEnemy> enemies, std::size_t selected,
                     GazetteText locked_description, EnemyListHost& host) {
  for (std::size_t index = 0; index < kEncyclopediaEnemies; ++index) {
    auto& enemy = enemies[index];
    if (index == selected) {
      if (enemy.selected == 0) {
        host.SetInformation(Description(enemy, locked_description));
      }
      enemy.selected = 1;
      if (enemy.unlocked != 0) enemy.highlight_step = kSettled;
    } else {
      enemy.selected = 0;
      if (enemy.highlight_step > 0) --enemy.highlight_step;
    }
  }
}

void DrawViewerModel(std::int32_t step, std::int32_t transition,
                     EnemyViewerHost& host) {
  if (step < kSettled) host.DrawEdgeFade();
  if (transition == kNoPreview) return;
  const ProjectionOffset saved = host.GetProjection();
  const auto slide = math::WrappingMultiply(
      math::WrappingSubtract(kSettled, step), kProjectionStride);
  host.SetProjection({.x = transition == kSlideFromRight
                               ? math::WrappingAdd(kProjectionX, slide)
                               : math::WrappingSubtract(kProjectionX, slide),
                      .y = kProjectionY});
  const auto camera = host.BuildCamera();
  host.DrawPreview(camera);
  host.SetProjection(saved);
}

void DrawViewerControls(std::int32_t slide,
                        const EnemyViewerPresentation& presentation,
                        EnemyViewerHost& host) {
  const auto button_x = math::WrappingSubtract(slide, kButtonInset);
  const auto label_x = math::WrappingSubtract(slide, kLabelInset);
  const auto banner_x = math::WrappingSubtract(slide, kBannerInset);
  host.DrawButton(ViewerButton::kRotation, button_x, kRotationButtonY);
  host.DrawText("ROTATION", Framework::PackXy(label_x, kRotationLabelY),
                presentation.rotation_color, ViewerTextLayer::kControls);
  host.DrawBanner(
      {banner_x, kRotationLabelY, kRotationBannerWidth, kBannerHeight},
      kControlsGradient);
  host.DrawButton(ViewerButton::kZoom, button_x, kZoomButtonY);
  host.DrawText("ZOOM", Framework::PackXy(label_x, kZoomLabelY),
                presentation.zoom_color, ViewerTextLayer::kControls);
  host.DrawBanner({banner_x, kZoomLabelY, kZoomBannerWidth, kBannerHeight},
                  kControlsGradient);
}

void DrawIdentification(std::int32_t selected, std::int32_t slide,
                        EnemyViewerHost& host) {
  std::array<char, kIdentificationTextBytes> label{
      'N', 'O', '.', ' ', ' ', ' ', '0', '0', '0', '/', '0', '7', '8', '\0'};
  int number = selected + 1;
  for (std::size_t digit = kDigitCount; digit != 0; --digit) {
    label[kIdentificationDigits + digit - 1] =
        static_cast<char>('0' + number % kDecimalBase);
    number /= kDecimalBase;
  }
  host.DrawText(label.data(),
                Framework::PackXy(math::WrappingSubtract(slide, kButtonInset),
                                  kIdentificationY),
                kOverviewTextColor, ViewerTextLayer::kIdentification);
  host.DrawPanel({math::WrappingSubtract(slide, kIdentificationInset),
                  kIdentificationY, kIdentificationWidth, kBannerHeight},
                 kIdentificationGradient);
}
}  // namespace

// decomp: setEnemyDescription (src/MENU/MENU9.PRG/238.c:1419; was
// _setEnemyDescription), 80104B40.
bool ShowEnemyDescription(std::span<const GazetteEnemy> enemies,
                          std::int32_t selected, GazetteText locked_description,
                          EnemyListHost& host) {
  if (!HasEnemy(enemies, selected)) {
    host.Refuse("enemy description selection outside the list");
    return false;
  }
  const auto description = Description(
      enemies[static_cast<std::size_t>(selected)], locked_description);
  if (!Terminated(description)) {
    host.Refuse("enemy description has no end");
    return false;
  }
  host.SetInformation(description);
  return true;
}

// decomp: renderEnemyNo (src/MENU/MENU9.PRG/238.c:1883; was _renderEnemyNo),
// 80105BCC.
bool DrawEnemyNumber(const EnemyNumber& number, EnemyQuadTarget& target) {
  if (number.value < 0 || number.value > kMaximumEnemyNumber) return false;
  auto x = math::WrappingAdd(number.x, kDigitRightOffset);
  int value = number.value;
  const std::uint8_t brightness = number.dimmed ? kDim : kBright;
  const auto top = math::WrapSigned16(static_cast<std::uint32_t>(
      math::WrappingSubtract(number.y, kDigitRaise)));
  const auto bottom = math::WrapSigned16(static_cast<std::uint32_t>(
      math::WrappingAdd(number.y, kDigitHeight - kDigitRaise)));
  for (int digit = 0; digit < kDigitCount; ++digit) {
    const Render::UiTexturedQuad quad{
        .top_left = {math::WrapSigned16(static_cast<std::uint32_t>(x)), top},
        .bottom_right = {math::WrapSigned16(static_cast<std::uint32_t>(
                             math::WrappingAdd(x, kDigitWidth))),
                         bottom},
        .texture = {.u = static_cast<std::uint8_t>((value % kDecimalBase) *
                                                   kDigitWidth),
                    .v = 0,
                    .width = kDigitWidth,
                    .height = kDigitHeight,
                    .texture_page =
                        number.dimmed ? kDimmedDigitPage : kDigitPage,
                    .clut = number.dimmed ? kDimmedDigitClut : kDigitClut},
        .tint = Render::QuadColor{brightness, brightness, brightness},
        .raw_texture = false,
        .translucent = true};
    if (!target.DrawQuad(quad)) return false;
    value /= kDecimalBase;
    x = math::WrappingSubtract(x, kDigitAdvance);
  }
  return true;
}

// decomp: initEnemyList (src/MENU/MENU9.PRG/238.c:1921; was _initEnemyList),
// 80105D8C.
bool InitializeEnemyList(std::span<GazetteEnemy> enemies,
                         std::span<const std::uint32_t> flags) {
  if (enemies.size() < kEncyclopediaEnemies) return false;
  enemies = enemies.first(kEncyclopediaEnemies);
  for (const auto& enemy : enemies) {
    if (enemy.defeat_bit_count > 0 &&
        (enemy.first_defeat_bit < 0 ||
         static_cast<std::size_t>(enemy.first_defeat_bit) +
                 static_cast<std::size_t>(enemy.defeat_bit_count) >
             flags.size() * kBitsPerWord)) {
      return false;
    }
  }
  std::uint8_t first = kNoEnemy;
  std::uint8_t previous = kNoEnemy;
  for (std::size_t index = 0; index < enemies.size(); ++index) {
    auto& enemy = enemies[index];
    enemy.unlocked = 0;
    const int end = enemy.first_defeat_bit + enemy.defeat_bit_count;
    for (int bit = enemy.first_defeat_bit; bit < end; ++bit) {
      const auto offset = static_cast<std::size_t>(bit);
      if ((flags[offset / kBitsPerWord] &
           (std::uint32_t{1} << (offset % kBitsPerWord))) != 0) {
        enemy.unlocked = 1;
        break;
      }
    }
    if (enemy.unlocked == 0) continue;
    if (first == kNoEnemy) first = static_cast<std::uint8_t>(index);
    enemy.previous = previous;
    if (previous != kNoEnemy) {
      enemies[previous].next = static_cast<std::uint8_t>(index);
    }
    previous = static_cast<std::uint8_t>(index);
  }
  if (first != kNoEnemy) {
    enemies[first].previous = previous;
    enemies[previous].next = first;
  }
  return true;
}

// decomp: renderEnemyMenu (src/MENU/MENU9.PRG/238.c:1977; was
// _renderEnemyMenu), 80105F00.
bool RenderEnemyList(std::span<GazetteEnemy> enemies,
                     GazetteRecordCursor cursor, std::int32_t step,
                     GazetteRecordAnimation& animation, GazetteText locked_name,
                     GazetteText locked_description, EnemyListHost& host) {
  if (!ValidList(enemies, cursor, locked_name, locked_description)) {
    host.Refuse("enemy list/page or chosen text is invalid");
    return false;
  }
  SelectEnemyRows(
      enemies, static_cast<std::size_t>(cursor.first_row) + cursor.selected_row,
      locked_description, host);
  const auto slide = math::WrappingSubtract(
      math::WrappingMultiply(step, kListSlideStride), kRowHeight);
  for (std::size_t visible = 0; visible < kGazetteVisibleRecords; ++visible) {
    const auto index = cursor.first_row + visible;
    const auto& enemy = enemies[index];
    const int y =
        std::min(slide, kFirstRowY + static_cast<int>(visible) * kRowHeight);
    if (enemy.selected != 0 && step == kSettled) {
      animation.cursor_step = static_cast<std::uint8_t>(
          host.DrawCursor(animation.cursor_step, kCursorX,
                          math::WrappingSubtract(y, kCursorRaise)));
    }
    auto& row = host.CreateRow({0, kListX, y, kListWidth, 0},
                               enemy.unlocked != 0 ? enemy.name : locked_name);
    if (enemy.unlocked == 0) row.font_color = kLockedColor;
    row.gradient_state = static_cast<std::uint8_t>(enemy.highlight_step);
    if (visible == 0 && cursor.first_row != 0) row.is_scrollable = kScrollUp;
    if (visible == kGazetteVisibleRecords - 1 &&
        cursor.first_row != kEncyclopediaEnemies - kGazetteVisibleRecords) {
      row.is_scrollable = kScrollDown;
    }
    host.DrawNumber({kListX - kNumberInset, y, static_cast<int>(index + 1),
                     (row.is_scrollable & kScrollUp) != 0});
    host.DrawRow(row.is_scrollable != 0 && step == kSettled);
  }
  host.Row().state = Framework::RowState::kInactive;
  return true;
}

// decomp: renderEnemyDetailScreen (src/MENU/MENU9.PRG/238.c:2074; was
// _renderEnemyDetailScreen), 801061F8.
bool RenderEnemyViewer(std::span<const GazetteEnemy> enemies,
                       const EnemyViewerPresentation& presentation,
                       EnemyViewerHost& host) {
  if (!presentation.hide_ui) {
    if (!HasEnemy(enemies, presentation.selected_enemy)) {
      host.Refuse("viewer selection outside the enemy list");
      return false;
    }
    const auto& enemy =
        enemies[static_cast<std::size_t>(presentation.selected_enemy)];
    if (enemy.class_id < 0 ||
        static_cast<std::size_t>(enemy.class_id) >= kClasses.size() ||
        enemy.name.size() < kNamePrefixBytes ||
        !Terminated(enemy.name.subspan(kNamePrefixBytes))) {
      host.Refuse("viewer class/name outside its bounds");
      return false;
    }
  }
  DrawViewerModel(presentation.step, presentation.transition, host);
  if (presentation.hide_ui) return true;
  const int step = presentation.transition > 0 ? kSettled : presentation.step;
  const auto name_slide =
      math::WrappingMultiply(math::WrappingSubtract(kSettled, step), kUiStride);
  const auto control_slide = math::WrappingMultiply(step, kUiStride);
  const auto& enemy =
      enemies[static_cast<std::size_t>(presentation.selected_enemy)];
  host.CreateRow(
      {0, math::WrappingAdd(name_slide, kNameX), kNameY, kNameWidth, 0},
      enemy.name.subspan(kNamePrefixBytes));
  host.DrawRow();
  host.Row().state = Framework::RowState::kInactive;
  DrawViewerControls(control_slide, presentation, host);
  DrawIdentification(presentation.selected_enemy, control_slide, host);
  host.DrawText(
      kClasses[static_cast<std::size_t>(enemy.class_id)],
      Framework::PackXy(math::WrappingAdd(name_slide, kClassX), kClassY),
      kOverviewTextColor, ViewerTextLayer::kOverlay);
  host.DrawPanel({math::WrappingAdd(name_slide, kClassPanelX), kClassY,
                  kClassPanelWidth, kClassPanelHeight},
                 kClassGradient);
  return true;
}

// decomp: renderEnemyNavigation (src/MENU/MENU9.PRG/238.c:2785; was
// _renderEnemyNavigation), 80107A98.
bool DrawEnemyNavigation(std::int32_t direction, std::int32_t animation_step,
                         bool hide_ui, EnemyNavigationHost& host) {
  if (hide_ui) return true;
  if (direction < 0 || direction > kNoPreview) return false;
  const bool transitioning = direction >= kSlideFromRight;
  const int side = transitioning ? direction - kSlideFromRight : direction;
  const bool right = side != 0;
  host.DrawText(
      right ? "#R1" : "L1",
      Framework::PackXy(right ? kRightTextX : kLeftTextX, kNavigationTextY),
      kOverviewTextColor);
  const auto offset =
      animation_step >> (animation_step < kNavigationMidpoint ? 1 : 2);
  const auto base =
      animation_step < kNavigationMidpoint
          ? (right ? math::WrappingAdd(kRightArrowX, offset)
                   : math::WrappingSubtract(kLeftArrowX, offset))
          : (right ? math::WrappingSubtract(kRightArrowReturnX, offset)
                   : math::WrappingAdd(kLeftArrowReturnX, offset));
  const auto x = math::WrappingSubtract(base, kNavigationInset);
  return host.DrawQuad(
      {.top_left = {math::WrapSigned16(static_cast<std::uint32_t>(x)),
                    kNavigationY},
       .bottom_right = {math::WrapSigned16(static_cast<std::uint32_t>(
                            math::WrappingAdd(x, kNavigationSize))),
                        kNavigationY + kNavigationSize},
       .texture = {.u = static_cast<std::uint8_t>(side * kNavigationSize),
                   .v = kArrowTextureY,
                   .width = kNavigationSize,
                   .height = kNavigationSize,
                   .texture_page = kDigitPage,
                   .clut = transitioning ? kTransitionArrowClut : kArrowClut}});
}
}  // namespace VS::Menu::Score
