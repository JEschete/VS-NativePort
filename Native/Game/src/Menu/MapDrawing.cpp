// Hand-maintained (tools/port/owned.csv).
#include "Menu/MapDrawing.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <utility>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::map {
namespace {
using Core::Math::R3000Quotient;
using Core::Math::R3000Remainder;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingNegate;
using Core::Math::WrappingSubtract;

[[nodiscard]] std::int16_t Half(std::int32_t value) {
  return Core::Math::WrapSigned16(static_cast<std::uint32_t>(value));
}

[[nodiscard]] ScreenPoint Point(std::int32_t x, std::int32_t y) {
  return {.x = Half(x), .y = Half(y)};
}

// getTPage(tp, abr, x, y) and getClut(x, y).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): libgpu's order.
[[nodiscard]] constexpr std::uint16_t TexturePage(std::uint32_t depth,
                                                  std::uint32_t blend,
                                                  std::uint32_t x,
                                                  std::uint32_t y) {
  constexpr std::uint32_t kDepthShift = 7;
  constexpr std::uint32_t kBlendShift = 5;
  constexpr std::uint32_t kModeMask = 3;
  constexpr std::uint32_t kPageRowBit = 0x100;
  constexpr std::uint32_t kPageRowShift = 4;
  constexpr std::uint32_t kPageColumnMask = 0x3FF;
  constexpr std::uint32_t kPageColumnShift = 6;
  constexpr std::uint32_t kPageRowHighBit = 0x200;
  return static_cast<std::uint16_t>(
      ((depth & kModeMask) << kDepthShift) |
      ((blend & kModeMask) << kBlendShift) |
      ((y & kPageRowBit) >> kPageRowShift) |
      ((x & kPageColumnMask) >> kPageColumnShift) |
      ((y & kPageRowHighBit) << 2));
}

[[nodiscard]] constexpr std::uint16_t Clut(std::uint32_t x, std::uint32_t y) {
  constexpr std::uint32_t kRowShift = 6;
  constexpr std::uint32_t kColumnShift = 4;
  constexpr std::uint32_t kColumnMask = 0x3F;
  return static_cast<std::uint16_t>((y << kRowShift) |
                                    ((x >> kColumnShift) & kColumnMask));
}

// ----------------------------------------------------------------- rooms --

constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kVertexBytes = 8;
constexpr std::size_t kEntryBytes = 4;
constexpr std::int32_t kNearestDepth = 0x40;
constexpr std::uint32_t kDepthSpan = 0x7BF;
constexpr Rgb kCurrentFill{.r = 0, .g = 0x80, .b = 0x40};
constexpr Rgb kOtherFill{.r = 0, .g = 0x40, .b = 0x80};
constexpr Rgb kOutlineGlow{.r = 0, .g = 0x80, .b = 0xFF};
constexpr Rgb kCurrentOutline{.r = 0, .g = 0xFF, .b = 0x80};
constexpr Rgb kCurrentDimEdge{.r = 0, .g = 0xFF, .b = 0x80};
constexpr Rgb kOtherDimEdge{.r = 0, .g = 0x40, .b = 0x80};
constexpr Rgb kEnteredDoorBlink{.r = 0xFF, .g = 0, .b = 0x80};
constexpr Rgb kDoorCross{.r = 0, .g = 0xC0, .b = 0xC0};
constexpr Rgb kPlainCross{.r = 0xC0, .g = 0xC0, .b = 0xC0};
constexpr Rgb kFeatureCross{.r = 0xA0, .g = 0xA0, .b = 0};
// A marker's icon sits 14 pixels above its vertex; Ashley's 8 left and up.
constexpr std::int32_t kIconRise = 14;
constexpr std::int32_t kAshleyInset = 8;
constexpr std::uint32_t kAshleyRowBias = 0xFFF80000U;
constexpr std::uint32_t kLowHalf = 0xFFFF;
constexpr std::uint32_t kHalfShift = 16;
constexpr std::int32_t kBlinkBit = 0x10;
constexpr std::int32_t kKindPeriodShift = 5;
constexpr std::uint32_t kBrightnessStepMask = 0xF;
constexpr std::int16_t kCrossLength = 3;
// The marker flag bits.
constexpr std::uint8_t kSceneFlags = 0xB;
constexpr std::uint8_t kSaveFlag = 2;
constexpr std::uint8_t kDoorFlag = 4;
constexpr std::uint8_t kWorkshopFlag = 8;
constexpr std::uint8_t kChestFlag = 0x10;
constexpr std::int32_t kDoorIcon = 0;
constexpr std::int32_t kWorkshopIcon = 1;
constexpr std::int32_t kSaveIcon = 2;
constexpr std::int32_t kChestIcon = 3;
constexpr std::int32_t kAreaLabel = 1;
// The marker kinds a flag set offers; up to six, where the original's
// four-entry array spills into its next two stack words.
enum class MarkerKind : std::uint8_t { kAshley, kSave, kWorkshop, kChest };
constexpr std::size_t kMaxMarkerKinds = 6;

// A count-prefixed table of 4-byte entries.
struct Section {
  std::size_t offset = 0;
  std::size_t count = 0;
};

struct RoomLayout {
  Section vertices;
  Section triangles;
  Section quads;
  Section edges;
  Section dim_edges;
  Section markers;
};

template <typename T>
[[nodiscard]] T Read(std::span<const std::byte> bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.subspan(offset, sizeof(T)).data(), sizeof(T));
  return value;
}

// The section whose count word is at `offset`; a negative count has no
// entries, as the original's loops run none.
[[nodiscard]] std::optional<Section> SectionAt(std::span<const std::byte> room,
                                               std::size_t offset,
                                               std::size_t entry_bytes) {
  if (offset > room.size() || room.size() - offset < kWordBytes) {
    return std::nullopt;
  }
  const auto count = Read<std::int32_t>(room, offset);
  const std::size_t entries = count <= 0 ? 0 : static_cast<std::size_t>(count);
  const std::size_t start = offset + kWordBytes;
  if (entries > (room.size() - start) / entry_bytes) return std::nullopt;
  return Section{.offset = start, .count = entries};
}

[[nodiscard]] std::size_t EndOf(const Section& section,
                                std::size_t entry_bytes) {
  return section.offset + (section.count * entry_bytes);
}

[[nodiscard]] std::optional<RoomLayout> LayoutOf(
    std::span<const std::byte> room) {
  RoomLayout layout;
  const auto vertices = SectionAt(room, 0, kVertexBytes);
  if (!vertices || vertices->count > kRoomVertexCapacity) return std::nullopt;
  layout.vertices = *vertices;
  std::size_t next = EndOf(layout.vertices, kVertexBytes);
  for (Section* section : {&layout.triangles, &layout.quads, &layout.edges,
                           &layout.dim_edges, &layout.markers}) {
    const auto parsed = SectionAt(room, next, kEntryBytes);
    if (!parsed) return std::nullopt;
    *section = *parsed;
    next = EndOf(*section, kEntryBytes);
  }
  return layout;
}

// An entry's four index or field bytes.
using Entry = std::array<std::uint8_t, kEntryBytes>;

[[nodiscard]] Entry EntryAt(std::span<const std::byte> room,
                            const Section& section, std::size_t index) {
  return Read<Entry>(room, section.offset + (index * kEntryBytes));
}

struct RoomDraw {
  RoomDrawHost& host;
  std::span<const std::byte> room;
  const RoomContext& context;
  const RoomWorkspace& rows;

  [[nodiscard]] bool IsCurrent() const {
    return context.room_index == context.current_room;
  }

  [[nodiscard]] const ProjectedVertex& Row(std::uint8_t index) const {
    return rows.at(index);
  }

  [[nodiscard]] Rgb Cue(const Rgb& color, std::uint8_t index) const {
    return host.DepthCue(color, Row(index).interpolation);
  }

  [[nodiscard]] ScreenPoint At(std::uint8_t index, std::int32_t dx,
                               std::int32_t dy) const {
    return Point(Row(index).x + dx, Row(index).y + dy);
  }
};

// decomp: MENU5.PRG _renderRoom (4D8.c:1132-1145): the vertex pass.
[[nodiscard]] bool ProjectVertices(RoomDrawHost& host,
                                   std::span<const std::byte> room,
                                   const Section& vertices,
                                   RoomWorkspace& workspace) {
  constexpr std::size_t kYOffset = 2;
  constexpr std::size_t kZOffset = 4;
  for (std::size_t index = 0; index < vertices.count; ++index) {
    const std::size_t at = vertices.offset + (index * kVertexBytes);
    const RoomVertex vertex{.x = Read<std::int16_t>(room, at),
                            .y = Read<std::int16_t>(room, at + kYOffset),
                            .z = Read<std::int16_t>(room, at + kZOffset)};
    const Projection projected = host.Project(vertex);
    workspace.at(index) = {.x = projected.screen.x,
                           .y = projected.screen.y,
                           .depth = Half(projected.depth),
                           .interpolation = Half(projected.interpolation)};
    if (static_cast<std::uint32_t>(projected.depth - kNearestDepth) >=
        kDepthSpan) {
      return false;
    }
  }
  return true;
}

// decomp: MENU5.PRG _renderRoom (4D8.c:1159-1238): the G3 and G4 fills, each
// at the farthest of its corners.
void DrawFills(const RoomDraw& draw, const RoomLayout& layout,
               const Rgb& fill) {
  for (std::size_t index = 0; index < layout.triangles.count; ++index) {
    const Entry entry = EntryAt(draw.room, layout.triangles, index);
    std::int32_t depth = draw.Row(entry[0]).depth;
    depth = std::max<std::int32_t>(depth, draw.Row(entry[1]).depth);
    depth = std::max<std::int32_t>(depth, draw.Row(entry[2]).depth);
    RoomTriangle triangle;
    for (std::size_t corner = 0; corner < triangle.corners.size(); ++corner) {
      triangle.corners.at(corner) = draw.At(entry.at(corner), 0, 0);
      triangle.colors.at(corner) = draw.Cue(fill, entry.at(corner));
    }
    draw.host.AddTriangle(triangle, depth);
  }
  // The packet's third and fourth corners are the entry's fourth and third.
  constexpr std::array<std::size_t, kQuadCorners> kQuadOrder{0, 1, 3, 2};
  for (std::size_t index = 0; index < layout.quads.count; ++index) {
    const Entry entry = EntryAt(draw.room, layout.quads, index);
    std::int32_t depth = draw.Row(entry[0]).depth;
    for (std::size_t corner = 1; corner < kQuadCorners; ++corner) {
      depth = std::max<std::int32_t>(depth, draw.Row(entry.at(corner)).depth);
    }
    RoomQuad quad;
    for (std::size_t corner = 0; corner < kQuadCorners; ++corner) {
      const std::uint8_t vertex = entry.at(kQuadOrder.at(corner));
      quad.corners.at(corner) = draw.At(vertex, 0, 0);
      quad.colors.at(corner) = draw.Cue(fill, vertex);
    }
    draw.host.AddQuad(quad, depth);
  }
}

// An edge's ordering-table word: a step nearer than its first end, or than
// its second when the first lies nearer than that.
[[nodiscard]] std::int32_t EdgeDepth(const RoomDraw& draw, const Entry& entry) {
  const std::int32_t first = draw.Row(entry[0]).depth;
  const std::int32_t second = draw.Row(entry[1]).depth;
  return first - 1 < second ? second - 1 : first - 1;
}

// decomp: MENU5.PRG _renderRoom (4D8.c:1240-1339): each outline as four
// semi-transparent glow lines a pixel left, up, right and down, then the
// core line, brighter in the current room.
void DrawOutlines(const RoomDraw& draw, const RoomLayout& layout) {
  constexpr std::array<std::array<std::int32_t, 2>, kQuadCorners> kGlowOffsets{
      {{-1, 0}, {0, -1}, {1, 0}, {0, 1}}};
  for (std::size_t index = 0; index < layout.edges.count; ++index) {
    const Entry entry = EntryAt(draw.room, layout.edges, index);
    const std::int32_t depth = EdgeDepth(draw, entry);
    RoomEdge edge{.from = {},
                  .to = {},
                  .from_color = draw.Cue(kOutlineGlow, entry[0]),
                  .to_color = draw.Cue(kOutlineGlow, entry[1]),
                  .semi_transparent = true};
    for (const auto& [dx, dy] : kGlowOffsets) {
      edge.from = draw.At(entry[0], dx, dy);
      edge.to = draw.At(entry[1], dx, dy);
      draw.host.AddEdge(edge, depth);
    }
    if (draw.IsCurrent()) {
      edge.from_color = draw.Cue(kCurrentOutline, entry[0]);
      edge.to_color = draw.Cue(kCurrentOutline, entry[1]);
    }
    edge.from = draw.At(entry[0], 0, 0);
    edge.to = draw.At(entry[1], 0, 0);
    edge.semi_transparent = false;
    draw.host.AddEdge(edge, depth);
  }
}

// decomp: MENU5.PRG _renderRoom (4D8.c:1341-1377): the dim edges.
void DrawDimEdges(const RoomDraw& draw, const RoomLayout& layout) {
  const Rgb& color = draw.IsCurrent() ? kCurrentDimEdge : kOtherDimEdge;
  for (std::size_t index = 0; index < layout.dim_edges.count; ++index) {
    const Entry entry = EntryAt(draw.room, layout.dim_edges, index);
    draw.host.AddEdge({.from = draw.At(entry[0], 0, 0),
                       .to = draw.At(entry[1], 0, 0),
                       .from_color = draw.Cue(color, entry[0]),
                       .to_color = draw.Cue(color, entry[1]),
                       .semi_transparent = true},
                      EdgeDepth(draw, entry));
  }
}

// The kinds a marker's flags offer, in the order the original lists them.
struct MarkerKinds {
  std::array<MarkerKind, kMaxMarkerKinds> kinds{};
  std::size_t count = 0;

  void Add(MarkerKind kind) { kinds.at(count++) = kind; }
};

[[nodiscard]] MarkerKinds KindsOf(const RoomDraw& draw, std::uint8_t flags) {
  MarkerKinds offered;
  const bool in_scene = draw.context.room_index == draw.context.scene_room;
  if ((flags & kSceneFlags) != 0 && in_scene) offered.Add(MarkerKind::kAshley);
  if (!draw.IsCurrent()) return offered;
  if ((flags & kSaveFlag) != 0) offered.Add(MarkerKind::kSave);
  if ((flags & kWorkshopFlag) != 0) {
    offered.Add(MarkerKind::kWorkshop);
    offered.Add(MarkerKind::kSave);
    offered.Add(MarkerKind::kChest);
  }
  if ((flags & kChestFlag) != 0) offered.Add(MarkerKind::kChest);
  return offered;
}

// The cross colour of a door or plain marker: the entered door blinks.
[[nodiscard]] std::optional<Rgb> CrossColorOf(const RoomDraw& draw,
                                              std::size_t marker,
                                              const Rgb& otherwise) {
  const RoomContext& context = draw.context;
  if (context.room_index == context.scene_room &&
      static_cast<std::int64_t>(marker) == context.entered_door) {
    if ((context.frame_counter & kBlinkBit) == 0) return std::nullopt;
    return kEnteredDoorBlink;
  }
  return otherwise;
}

// decomp: MENU5.PRG _renderRoom (4D8.c:1445-1472): one of the offered
// kinds, cycling every 32 frames. A negative frame counter picks none,
// where the original indexes before its array.
void DrawOfferedKind(const RoomDraw& draw, const MarkerKinds& offered,
                     const ProjectedVertex& row) {
  const RoomContext& context = draw.context;
  const std::int32_t pick =
      R3000Remainder(context.frame_counter >> kKindPeriodShift,
                     static_cast<std::int32_t>(offered.count));
  if (pick < 0) return;
  const std::int32_t icon_y = row.y - kIconRise;
  switch (offered.kinds.at(static_cast<std::size_t>(pick))) {
    case MarkerKind::kAshley: {
      const std::uint32_t position =
          (static_cast<std::uint32_t>(row.x - kAshleyInset) & kLowHalf) |
          ((static_cast<std::uint32_t>(static_cast<std::uint16_t>(row.y))
            << kHalfShift) +
           kAshleyRowBias);
      const auto step = static_cast<std::uint32_t>(context.frame_counter) &
                        kBrightnessStepMask;
      draw.host.DrawAshley(static_cast<std::int32_t>(position),
                           context.brightness[step]);
      break;
    }
    case MarkerKind::kSave:
      draw.host.DrawFeature(kSaveIcon, row.x, icon_y);
      break;
    case MarkerKind::kWorkshop:
      draw.host.DrawFeature(kWorkshopIcon, row.x, icon_y);
      break;
    case MarkerKind::kChest:
      draw.host.DrawFeature(kChestIcon, row.x, icon_y);
      break;
  }
}

// decomp: MENU5.PRG _renderRoom (4D8.c:1387-1497): one marker. A shown door
// state draws its own icon and no cross.
void DrawMarker(const RoomDraw& draw, std::size_t marker, const Entry& entry) {
  const auto [vertex, label_id, flags, state_id] = entry;
  const ProjectedVertex& row = draw.Row(vertex);
  const RoomContext& context = draw.context;
  if (state_id != 0 &&
      draw.host.ShouldDrawDoorState(state_id, context.zone_id,
                                    context.map_id) &&
      draw.IsCurrent()) {
    draw.host.DrawDoorState(row.x, row.y - kIconRise, state_id);
    return;
  }
  std::optional<Rgb> cross;
  if ((flags & kDoorFlag) != 0 && draw.IsCurrent()) {
    draw.host.DrawFeature(kDoorIcon, row.x, row.y - kIconRise);
    draw.host.QueueLabel(row.x, row.y, kAreaLabel, label_id);
    cross = CrossColorOf(draw, marker, kDoorCross);
  }
  if (draw.IsCurrent() && flags == 0) {
    cross = CrossColorOf(draw, marker, kPlainCross);
  }
  const MarkerKinds offered = KindsOf(draw, flags);
  if (offered.count > 0) {
    cross = kFeatureCross;
    DrawOfferedKind(draw, offered, row);
  }
  if (!cross) return;
  const std::int32_t depth = row.depth - 1;
  draw.host.AddTile({.position = Point(row.x - 1, row.y),
                     .width = kCrossLength,
                     .height = 1,
                     .color = *cross},
                    depth);
  draw.host.AddTile({.position = Point(row.x, row.y - 1),
                     .width = 1,
                     .height = kCrossLength,
                     .color = *cross},
                    depth);
}

// ------------------------------------------------------------- legend --

constexpr std::int32_t kSlideStart = -128;
constexpr std::int32_t kSlideStep = 16;
constexpr std::int32_t kTextBoxStep = 8;
constexpr std::int32_t kCaptionColumns = 5;
constexpr std::size_t kCommandsStates = 6;
// Which of BACK, NEXT, ROTATION, ZOOM and MENU each commands state shows
// bright.
using CaptionRow = std::array<bool, kCaptionColumns>;
using CaptionTable = std::array<CaptionRow, kCommandsStates>;
constexpr CaptionTable kBrightCaptions{{
    {true, true, true, true, true},
    {true, true, false, false, true},
    {false, false, false, true, true},
    {false, false, true, false, true},
    {false, false, false, false, true},
    {false, false, false, false, false},
}};

struct LegendPlace {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

[[nodiscard]] std::int32_t PackXy(std::int32_t x, std::int32_t y) {
  return static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(y) << kHalfShift) |
      (static_cast<std::uint32_t>(x) & kLowHalf));
}

void StepSlide(ControlsHost& host, ControlsState& state) {
  switch (state.animation_state) {
    case 0:
      state.x_offset = kSlideStart;
      ++state.animation_state;
      break;
    case 1:
      if (state.x_offset < 0) {
        state.x_offset = WrappingAdd(state.x_offset, kSlideStep);
        host.ShiftTextBox(-kTextBoxStep);
      }
      break;
    case 2:
      if (state.x_offset > kSlideStart) {
        state.x_offset = WrappingSubtract(state.x_offset, kSlideStep);
        host.ShiftTextBox(kTextBoxStep);
      }
      break;
    default:
      break;
  }
}

// decomp: MENU5.PRG _drawUIControls (4D8.c:2032-2148): the buttons and the
// captions the commands state shades.
void DrawLegend(ControlsHost& host, std::int32_t offset,
                std::int16_t commands_state) {
  constexpr std::array<std::pair<Framework::ButtonIcon, LegendPlace>,
                       kCaptionColumns>
      kButtons{{{Framework::ButtonIcon::kL1, {.x = 8, .y = 15}},
                {Framework::ButtonIcon::kR1, {.x = 64, .y = 15}},
                {Framework::ButtonIcon::kL2, {.x = 8, .y = 33}},
                {Framework::ButtonIcon::kR2, {.x = 8, .y = 52}},
                {Framework::ButtonIcon::kCircle, {.x = 8, .y = 70}}}};
  constexpr std::array<std::pair<ControlCaption, LegendPlace>, kCaptionColumns>
      kCaptions{{{ControlCaption::kBack, {.x = 28, .y = 18}},
                 {ControlCaption::kNext, {.x = 84, .y = 18}},
                 {ControlCaption::kRotation, {.x = 28, .y = 36}},
                 {ControlCaption::kZoom, {.x = 28, .y = 54}},
                 {ControlCaption::kMenu, {.x = 28, .y = 72}}}};
  for (const auto& [button, place] : kButtons) {
    host.DrawButton(button, WrappingAdd(offset, place.x), place.y);
  }
  const auto row = static_cast<std::uint16_t>(commands_state);
  if (row >= kCommandsStates) return;
  for (std::size_t column = 0; column < kCaptions.size(); ++column) {
    const auto& [caption, place] = kCaptions.at(column);
    host.DrawCaption(
        caption, PackXy(WrappingAdd(offset, place.x), place.y),
        kBrightCaptions.at(row).at(column) ? kCaptionBright : kCaptionDim);
  }
}

// ------------------------------------------------------------- compass --

constexpr std::int32_t kCompassFraction = 4096;
constexpr std::int32_t kTicks = 8;
constexpr std::int32_t kTickTurn = 256;
constexpr std::int32_t kQuarterTurn = 0x200;
constexpr std::int32_t kTickLength = 43;
constexpr std::int32_t kTickWidth = 3;
constexpr std::int32_t kLetterRadius = 39;
constexpr std::int32_t kHubSize = 15;
constexpr std::int32_t kNeedleLength = 17;
constexpr std::uint32_t kTickMajorMask = 3;
constexpr std::uint8_t kTickLeft = 2;
constexpr std::uint8_t kTickRight = 45;
constexpr std::uint8_t kTickMajorTop = 128;
constexpr std::uint8_t kTickMinorTop = 132;
constexpr std::uint8_t kTickHeight = 3;
constexpr std::int32_t kTickSlotBytes = 28;
constexpr std::int32_t kLetterSlotBytes = 24;
constexpr std::uint32_t kAtlasX = 448;
constexpr std::uint32_t kAtlasY = 256;
constexpr std::uint32_t kClutColumn = 960;
constexpr std::uint32_t kClutRow = 222;
using LetterColumns = std::array<std::uint8_t, kQuadCorners>;
constexpr LetterColumns kLetterColumns{0x30, 0x40, 0x48, 0x38};
constexpr std::uint8_t kLetterTop = 128;
constexpr std::uint8_t kLetterSize = 7;
constexpr std::int32_t kLetterNear = -3;
constexpr std::int32_t kLetterFar = 4;
constexpr std::int32_t kGlowNear = -6;
constexpr std::int32_t kGlowFar = 7;
constexpr std::uint8_t kGlowLeft = 1;
constexpr std::uint8_t kGlowTop = 137;
constexpr std::uint8_t kGlowSize = 13;
constexpr std::uint8_t kNeedleLeft = 0x14;
constexpr std::uint8_t kNeedleTop = 0x88;
constexpr std::uint8_t kNeedleRight = 0x23;
constexpr std::uint8_t kNeedleBottom = 0x99;

[[nodiscard]] std::array<TexturePoint, kQuadCorners> TextureBox(
    std::uint8_t left, std::uint8_t top, std::uint8_t right,
    std::uint8_t bottom) {
  return {TexturePoint{.u = left, .v = top}, TexturePoint{.u = right, .v = top},
          TexturePoint{.u = left, .v = bottom},
          TexturePoint{.u = right, .v = bottom}};
}

[[nodiscard]] std::array<ScreenPoint, kQuadCorners> CornersOf(
    const CompassCorners& corners) {
  std::array<ScreenPoint, kQuadCorners> points{};
  for (std::size_t corner = 0; corner < kQuadCorners; ++corner) {
    points.at(corner) = Point(corners.at(corner).x, corners.at(corner).y);
  }
  return points;
}

// A square from `near` to `far` about `center`.
[[nodiscard]] std::array<ScreenPoint, kQuadCorners> SquareAbout(
    const CompassPoint& center, std::int32_t near, std::int32_t far) {
  return {Point(center.x + near, center.y + near),
          Point(center.x + far, center.y + near),
          Point(center.x + near, center.y + far),
          Point(center.x + far, center.y + far)};
}

[[nodiscard]] CompassCorners Rotated(CompassHost& host, std::int32_t x,
                                     std::int32_t y, std::int32_t width,
                                     std::int32_t height, std::int32_t angle) {
  const std::int32_t sine = host.Sine(angle);
  const std::int32_t cosine = host.Cosine(angle);
  return ComputeCompassCorners(x, y, width, height, sine, cosine);
}

void DrawLetters(CompassHost& host, const CompassCorners& corners) {
  constexpr std::uint32_t kNorthClutColumn = 960;
  constexpr std::uint32_t kNorthClutRow = 221;
  constexpr std::uint32_t kLetterClutColumn = 1008;
  constexpr std::uint32_t kLetterClutRow = 220;
  constexpr std::uint32_t kNorthGlowClutColumn = 976;
  for (std::size_t letter = 0; letter < kQuadCorners; ++letter) {
    const CompassPoint& center = corners.at(letter);
    const std::uint8_t column = kLetterColumns.at(letter);
    const bool north = letter == 0;
    host.AddCompassQuad(
        {.quad = {.corners = SquareAbout(center, kLetterNear, kLetterFar),
                  .texture = TextureBox(
                      column, kLetterTop,
                      static_cast<std::uint8_t>(column + kLetterSize),
                      kLetterTop + kLetterSize),
                  .clut = north ? Clut(kNorthClutColumn, kNorthClutRow)
                                : Clut(kLetterClutColumn, kLetterClutRow),
                  .texture_page = TexturePage(0, 0, kAtlasX, kAtlasY)},
         .semi_transparent = false,
         .slot_bytes = kLetterSlotBytes});
    host.AddCompassQuad(
        {.quad = {.corners = SquareAbout(center, kGlowNear, kGlowFar),
                  .texture =
                      TextureBox(kGlowLeft, kGlowTop, kGlowLeft + kGlowSize,
                                 kGlowTop + kGlowSize),
                  .clut = north ? Clut(kNorthGlowClutColumn, kClutRow)
                                : Clut(kClutColumn, kClutRow),
                  .texture_page = TexturePage(0, 1, kAtlasX, kAtlasY)},
         .semi_transparent = true,
         .slot_bytes = kLetterSlotBytes});
  }
}
}  // namespace

// decomp: MENU5.PRG _renderRoom (4D8.c:1112).
RoomDrawResult DrawRoom(RoomDrawHost& host, std::span<const std::byte> room,
                        const RoomContext& context, RoomWorkspace& workspace) {
  const auto layout = LayoutOf(room);
  if (!layout || context.brightness.size() < kAshleyBrightnessSteps) {
    return RoomDrawResult::kMalformed;
  }
  if (!ProjectVertices(host, room, layout->vertices, workspace)) {
    return RoomDrawResult::kOutOfDepthRange;
  }
  const RoomDraw draw{
      .host = host, .room = room, .context = context, .rows = workspace};
  DrawFills(draw, *layout, draw.IsCurrent() ? kCurrentFill : kOtherFill);
  DrawOutlines(draw, *layout);
  DrawDimEdges(draw, *layout);
  for (std::size_t marker = 0; marker < layout->markers.count; ++marker) {
    DrawMarker(draw, marker, EntryAt(room, layout->markers, marker));
  }
  return RoomDrawResult::kDrawn;
}

// decomp: MENU5.PRG _renderMap (4D8.c:1081).
void DrawVisitedRooms(VisitedRoomsHost& host, std::int32_t geom_x,
                      std::int32_t geom_y, std::span<const MapRoom> rooms) {
  const ScreenOffset centered{.x = WrappingAdd(geom_x, kMapCenter.x),
                              .y = WrappingAdd(geom_y, kMapCenter.y)};
  const ScreenOffset previous = host.GetScreenOffset();
  host.SetScreenOffset(centered);
  host.ClearLabels();
  for (std::size_t index = 0; index < rooms.size(); ++index) {
    const MapRoom& room = rooms[index];
    if (room.visited != 0) host.DrawRoom(index, room.zone_id, room.map_id);
  }
  host.DrawLabels();
  host.SetScreenOffset(previous);
}

// decomp: MENU5.PRG renderMapLabels (4D8.c:2450).
bool DrawLabels(LabelDrainHost& host, std::int32_t commands_state, bool hide_ui,
                std::span<const MapLabel> labels, std::int32_t count) {
  if (commands_state == kHiddenCommandsState || hide_ui) return true;
  const std::size_t queued =
      count <= 0 ? 0 : std::min(static_cast<std::size_t>(count), labels.size());
  for (const MapLabel& label : labels.first(queued)) {
    if (label.kind == kDoorLabelKind) {
      host.DrawDoorLabel(label.x, label.y, label.id);
    }
  }
  for (const MapLabel& label : labels.first(queued)) {
    if (label.kind != kDoorLabelKind) {
      host.DrawAreaLabel(label.x, label.y, label.id);
    }
  }
  return count <= 0 || static_cast<std::size_t>(count) <= labels.size();
}

// decomp: MENU5.PRG _drawUIControlsBackground (4D8.c:1965).
bool DrawControlBackdrop(BackdropHost& host, std::int32_t x, std::int32_t y,
                         std::int32_t width, std::int32_t height) {
  const std::int32_t last = WrappingSubtract(WrappingAdd(y, height), 1);
  const std::int64_t rows =
      std::max<std::int64_t>(0, static_cast<std::int64_t>(last) - y);
  if (rows > kMaxBackdropLines) return false;
  for (std::int32_t line = 0; line < rows; ++line) {
    const std::int32_t row = WrappingAdd(y, line);
    const std::int32_t line_width = WrappingSubtract(width, line);
    host.AddBackdropLine(
        {.from = Point(x, row),
         .to = Point(WrappingSubtract(WrappingAdd(x, line_width), 1), row)});
  }
  const std::int32_t left = WrappingAdd(x, 2);
  const std::int32_t top = WrappingAdd(y, 2);
  const std::int32_t right = WrappingSubtract(WrappingAdd(left, width), 1);
  const std::int32_t bottom = WrappingSubtract(WrappingAdd(top, height), 1);
  host.AddBackdropShadow(
      {Point(left, top), Point(right, top), Point(left, bottom),
       Point(WrappingSubtract(WrappingAdd(left, width), height), bottom)});
  return true;
}

// decomp: MENU5.PRG _drawUIControls (4D8.c:1991).
bool DrawControls(ControlsHost& host, ControlsState& state,
                  const ControlsFrame& frame) {
  constexpr LegendPlace kCaptionPlace{.x = 28, .y = 18};
  constexpr std::int32_t kBackdropInset = 16;
  constexpr std::size_t kBackdropCount = 4;
  constexpr std::array<std::array<std::int32_t, 2>, kBackdropCount> kBackdrops{
      {{18, 114}, {36, 96}, {54, 78}, {72, 60}}};
  constexpr std::int32_t kBackdropHeight = 12;
  constexpr LegendPlace kCompassPlace{.x = 62, .y = 184};
  StepSlide(host, state);
  if (frame.hide_ui) return true;
  const std::optional<bool> jammed = host.IsJammed();
  if (!jammed) return false;
  const std::int32_t offset = state.x_offset;
  if (*jammed) {
    host.DrawCaption(
        ControlCaption::kJamming,
        PackXy(WrappingAdd(offset, kCaptionPlace.x), kCaptionPlace.y),
        kCaptionBright);
    return true;
  }
  DrawLegend(host, offset, frame.commands_state);
  for (const auto& [y, width] : kBackdrops) {
    host.DrawBackdrop(WrappingAdd(offset, kBackdropInset), y, width,
                      kBackdropHeight);
  }
  host.AddTexturePage();
  host.DrawCompass(kCompassPlace.x, kCompassPlace.y, frame.compass_angle);
  return true;
}

// decomp: MENU5.PRG _computeRotatedRect (4D8.c:2377).
CompassCorners ComputeCompassCorners(std::int32_t center_x,
                                     std::int32_t center_y, std::int32_t width,
                                     std::int32_t height, std::int32_t sine,
                                     std::int32_t cosine) {
  const std::int32_t width_cos = WrappingMultiply(width, cosine);
  const std::int32_t width_sin = WrappingMultiply(width, sine);
  const std::int32_t height_cos = WrappingMultiply(height, cosine);
  const std::int32_t height_sin = WrappingMultiply(height, sine);
  const CompassPoint first{
      .x = WrappingAdd(WrappingNegate(width_cos), height_sin),
      .y = WrappingSubtract(WrappingNegate(height_cos), width_sin)};
  const CompassPoint second{
      .x = WrappingAdd(width_cos, height_sin),
      .y = WrappingAdd(WrappingNegate(height_cos), width_sin)};
  CompassCorners corners{
      first, second,
      CompassPoint{.x = WrappingNegate(second.x),
                   .y = WrappingNegate(second.y)},
      CompassPoint{.x = WrappingNegate(first.x), .y = WrappingNegate(first.y)}};
  for (CompassPoint& corner : corners) {
    corner.x = WrappingAdd(
        R3000Quotient(R3000Quotient(corner.x, kCompassFraction), 2), center_x);
    corner.y = WrappingAdd(
        R3000Quotient(R3000Quotient(corner.y, kCompassFraction), 2), center_y);
  }
  return corners;
}

// decomp: MENU5.PRG _renderCompass (4D8.c:2272).
CompassCorners DrawCompass(CompassHost& host, std::int32_t x, std::int32_t y,
                           std::int32_t angle, std::int32_t roll) {
  for (std::int32_t tick = 0; tick < kTicks; ++tick) {
    const CompassCorners corners =
        Rotated(host, x, y, kTickLength, kTickWidth,
                WrappingAdd(angle, tick * kTickTurn));
    const std::uint8_t top =
        (static_cast<std::uint32_t>(tick) & kTickMajorMask) == 0
            ? kTickMajorTop
            : kTickMinorTop;
    host.AddCompassQuad(
        {.quad = {.corners = CornersOf(corners),
                  .texture =
                      TextureBox(kTickLeft, top, kTickRight,
                                 static_cast<std::uint8_t>(top + kTickHeight)),
                  .clut = Clut(kClutColumn, kClutRow),
                  .texture_page = TexturePage(0, 1, kAtlasX, kAtlasY)},
         .semi_transparent = true,
         .slot_bytes = kTickSlotBytes});
  }
  DrawLetters(host, Rotated(host, x, y, kLetterRadius, kLetterRadius,
                            WrappingAdd(angle, kQuarterTurn)));
  const std::int32_t heading = WrappingSubtract(angle, roll);
  const CompassCorners hub = Rotated(host, x, y, kHubSize, kHubSize,
                                     WrappingAdd(heading, kQuarterTurn));
  const CompassCorners needle =
      Rotated(host, hub[0].x, hub[0].y, kHubSize, kNeedleLength, heading);
  constexpr std::uint32_t kNeedleClutColumn = 992;
  host.AddCompassQuad(
      {.quad = {.corners = CornersOf(needle),
                .texture = TextureBox(kNeedleLeft, kNeedleTop, kNeedleRight,
                                      kNeedleBottom),
                .clut = Clut(kNeedleClutColumn, kClutRow),
                .texture_page = TexturePage(0, 0, kAtlasX, kAtlasY)},
       .semi_transparent = true,
       .slot_bytes = kLetterSlotBytes});
  return needle;
}
}  // namespace VS::Menu::map
