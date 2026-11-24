#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "Menu/Score/GazetteCamera.hpp"
#include "Menu/Score/GazetteStatistics.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kOriginalEncyclopediaEnemies = 78;
inline constexpr std::size_t kEnemyDefeatFlagWords = 26;

struct GazetteEnemy {
  std::int16_t model_id = 0;
  std::int16_t class_id = 0;
  std::int16_t first_defeat_bit = 0;
  std::int16_t defeat_bit_count = 0;
  std::int16_t selected = 0;
  std::int16_t highlight_step = 0;
  std::uint8_t unlocked = 0;
  std::uint32_t previous = 0;
  std::uint32_t next = 0;
  GazetteText name;
  GazetteText description;
  std::string name_id{};
  std::string description_id{};
  bool always_unlocked = false;
};

struct EnemyNumber {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t value = 0;
  bool dimmed = false;
};

class EnemyQuadTarget {
 public:
  virtual ~EnemyQuadTarget() = default;
  virtual bool DrawQuad(const Render::UiTexturedQuad& quad) = 0;
};

class EnemyListHost {
 public:
  virtual ~EnemyListHost() = default;
  virtual void SetInformation(GazetteText text) = 0;
  virtual void SetNativeInformation(std::string_view) {
    Refuse("native information is not bound");
  }
  virtual Framework::MenuRow& CreateRow(
      const Framework::RowPlacement& placement, GazetteText text) = 0;
  virtual Framework::MenuRow& CreateNativeRow(
      const Framework::RowPlacement& placement, std::string_view) {
    Refuse("native row is not bound");
    return CreateRow(placement, {});
  }
  virtual Framework::MenuRow& Row() = 0;
  virtual std::int32_t DrawCursor(std::uint8_t step, int x, int y) = 0;
  virtual void DrawNumber(const EnemyNumber& number) = 0;
  virtual void DrawRow(bool scrolling) = 0;
  virtual void Refuse(std::string_view reason) = 0;
};

enum class ViewerTextLayer : std::uint8_t {
  kOverlay,
  kControls,
  kIdentification,
};
enum class ViewerButton : std::uint8_t { kRotation, kZoom };

struct EnemyViewerPresentation {
  std::int32_t selected_enemy = 0;
  std::int32_t step = 0;
  // 0: ordinary entrance/exit, 1/2: left/right switch, 3: no model.
  std::int32_t transition = 0;
  bool hide_ui = false;
  std::uint32_t rotation_color = 0;
  std::uint32_t zoom_color = 0;
};

class EnemyViewerHost {
 public:
  virtual ~EnemyViewerHost() = default;
  virtual void DrawEdgeFade() = 0;
  virtual ProjectionOffset GetProjection() = 0;
  virtual void SetProjection(const ProjectionOffset& offset) = 0;
  virtual Core::Math::Transform3 BuildCamera() = 0;
  virtual void DrawPreview(const Core::Math::Transform3& camera) = 0;
  virtual Framework::MenuRow& CreateRow(
      const Framework::RowPlacement& placement, GazetteText text) = 0;
  virtual Framework::MenuRow& CreateNativeRow(
      const Framework::RowPlacement& placement, std::string_view) {
    Refuse("native row is not bound");
    return CreateRow(placement, {});
  }
  virtual void DrawRow() = 0;
  virtual Framework::MenuRow& Row() = 0;
  virtual void DrawButton(ViewerButton button, int x, int y) = 0;
  virtual void DrawText(std::string_view text, std::uint32_t position,
                        std::uint32_t color, ViewerTextLayer layer) = 0;
  virtual void DrawBanner(const PanelRect& rect, int gradient) = 0;
  virtual void DrawPanel(const PanelRect& rect, int gradient) = 0;
  virtual void Refuse(std::string_view reason) = 0;
};

class EnemyNavigationHost : public EnemyQuadTarget {
 public:
  virtual void DrawText(std::string_view text, std::uint32_t position,
                        std::uint32_t color) = 0;
};

// F02805. Only the chosen description needs an end. Invalid selection/text
// is a native refusal, not an original-parity case.
[[nodiscard]] bool ShowEnemyDescription(std::span<const GazetteEnemy> enemies,
                                        std::int32_t selected,
                                        GazetteText locked_description,
                                        EnemyListHost& host);

// F02817. Three digits, units first, low-halfword coordinates. Values
// outside 0..999 and a failed quad emission are refused.
[[nodiscard]] bool DrawEnemyNumber(const EnemyNumber& number,
                                   EnemyQuadTarget& target);

// F02818. The supplied entries form a circular list of unlocked enemies.
// Locked links, names, selection and signed highlights are preserved.
// Positive defeat ranges must fit the supplied flags; nonpositive counts
// do not read flags, just as in the original.
[[nodiscard]] bool InitializeEnemyList(std::span<GazetteEnemy> enemies,
                                       std::span<const std::uint32_t> flags);

// F02819. All selections/highlights are updated, then eight rows are
// rendered. A negative signed highlight is retained, not decremented.
[[nodiscard]] bool RenderEnemyList(std::span<GazetteEnemy> enemies,
                                   GazetteRecordCursor cursor,
                                   std::int32_t step,
                                   GazetteRecordAnimation& animation,
                                   GazetteText locked_name,
                                   GazetteText locked_description,
                                   EnemyListHost& host);

// F02820. Projection is restored before drawing UI. Transition 3 omits the
// preview, but not the edge fade; positive transitions settle only the UI.
[[nodiscard]] bool RenderEnemyViewer(
    std::span<const GazetteEnemy> enemies,
    const EnemyViewerPresentation& presentation, EnemyViewerHost& host);

// F02834 draws controls; it does not change the selection. Hidden UI is a
// no-op. Otherwise directions outside the four original modes are refused.
[[nodiscard]] bool DrawEnemyNavigation(std::int32_t direction,
                                       std::int32_t animation_step,
                                       bool hide_ui, EnemyNavigationHost& host);
}  // namespace VS::Menu::Score
