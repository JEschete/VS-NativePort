// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Menu/Score/Encyclopedia.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Content/DiscLayout.hpp"
#include "Content/EnemyPreviews.hpp"

using VS::Content::disc::LocateAs;

// decomp: _enemyNavigationState
// (_enemyNavigationState@src/MENU/MENU9.PRG/238.c)
std::int32_t VS::Menu::Score::enemyNavigationState;

// decomp: _paletteFlags (_paletteFlags@src/MENU/MENU9.PRG/592C.c)
std::uint8_t
    VS::Menu::Score::paletteFlags[VS::Menu::Score::kEnemyPreviewFiles] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1};

namespace {
namespace content = VS::Content;

struct PreviewZud {
  std::int32_t lba;
  std::int32_t size;
};

// The disc path of an archetype's ZUD (archetype.map.z009u16 is
// MAP/Z009U16.ZUD), the inverse of Content::DiscFileId.
constexpr std::size_t kZudPathCapacity = 32;
struct ZudPath {
  std::array<char, kZudPathCapacity> text{};
  std::size_t size = 0;
};

consteval ZudPath ZudPathOf(std::string_view archetype) noexcept {
  constexpr std::string_view kKind = "archetype.";
  constexpr std::string_view kExtension = ".ZUD";
  ZudPath path;
  for (const char value : archetype.substr(kKind.size())) {
    path.text.at(path.size++) =
        value == '.'
            ? '/'
            : static_cast<char>(value >= 'a' && value <= 'z' ? value - 'a' + 'A'
                                                             : value);
  }
  for (const char value : kExtension) path.text.at(path.size++) = value;
  return path;
}

using VS::Menu::Score::kEnemyPreviewFiles;
using PreviewColumn = std::array<std::int32_t, kEnemyPreviewFiles>;

// The original keeps one column per field (592C.c _zudLbas, _zudSizes); each
// code loads the archetype Content::kEnemyPreviews gives it.
consteval PreviewColumn PreviewTableColumn(
    std::int32_t PreviewZud::* field) noexcept {
  PreviewColumn column{};
  for (std::size_t code = 0; code < column.size(); ++code) {
    const ZudPath path = ZudPathOf(
        content::EnemyPreviewArchetype(static_cast<std::int64_t>(code)));
    column.at(code) =
        LocateAs<PreviewZud>(std::string_view(path.text.data(), path.size)).*
        field;
  }
  return column;
}
constexpr PreviewColumn kPreviewLbas = PreviewTableColumn(&PreviewZud::lba);
constexpr PreviewColumn kPreviewSizes = PreviewTableColumn(&PreviewZud::size);

// Code 4 previews an archetype of its own; code 5 has none, so it loads the
// default.
constexpr std::size_t kOwnPreview = 4;
constexpr std::size_t kDefaultedPreview = 5;
static_assert(kPreviewLbas.at(kOwnPreview) ==
              LocateAs<PreviewZud>("MAP/Z009U16.ZUD").lba);
static_assert(kPreviewLbas.at(kDefaultedPreview) ==
              LocateAs<PreviewZud>("MAP/Z003U00.ZUD").lba);
}  // namespace

// decomp: _zudLbas (_zudLbas@src/MENU/MENU9.PRG/592C.c)
constinit PreviewColumn VS::Menu::Score::zudLbas = kPreviewLbas;

// decomp: _zudSizes (_zudSizes@src/MENU/MENU9.PRG/592C.c)
constinit PreviewColumn VS::Menu::Score::zudSizes = kPreviewSizes;
