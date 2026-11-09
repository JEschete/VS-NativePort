// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomEffects.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "Core/Log.hpp"
#include "Field/RoomEffectsCore.hpp"
#include "Render/RoomDraw.hpp"

namespace VS::Field {
namespace {
using room_effects::Fits;
using room_effects::Load;
using room_effects::Store;
using room_effects::WrappingAbs;
using room_effects::WrappingAdd;
using room_effects::WrappingNegate;

constexpr std::string_view kChannel = "field.effects";

// Positions are kept in sixteenths of a geometry unit; angles in 4096ths of
// a turn (ONE).
constexpr std::int32_t kSubunitScale = 16;
constexpr std::uint32_t kAngleMask = 0xFFF;
constexpr std::uint32_t kHalfTurn = 0x800;
constexpr std::uint32_t kFullTurn = 0x1000;

// The group words a swing writes: the flags and the three rotation words.
constexpr std::size_t kRotationXWord = room_effects::kGroupRotationWord;
constexpr std::size_t kRotationYWord = kRotationXWord + 1;
constexpr std::size_t kRotationZWord = kRotationXWord + 2;
constexpr std::size_t kSwingGroupWords = kRotationZWord + 1;

std::uint32_t GroupWord(std::span<const std::byte> geometry,
                        std::size_t group_offset, std::size_t word) {
  return Load<std::uint32_t>(
      geometry, group_offset + word * room_effects::kGroupWordBytes);
}

void SetGroupWord(std::span<std::byte> geometry, std::size_t group_offset,
                  std::size_t word, std::uint32_t value) {
  Store(geometry, group_offset + word * room_effects::kGroupWordBytes, value);
}

// Clears then sets flag bits in a group's first word.
bool UpdateGroupFlags(std::span<std::byte> geometry, int group_index,
                      std::uint32_t clear_bits, std::uint32_t set_bits) {
  const auto group = FindRoomGeometryGroup(geometry, group_index);
  if (!group) {
    return false;
  }
  const std::uint32_t flags = GroupWord(geometry, *group, 0);
  SetGroupWord(geometry, *group, 0, (flags & ~clear_bits) | set_bits);
  return true;
}

void SwapColorPairs(std::span<std::byte> polygon, std::size_t corners) {
  for (std::size_t corner = 0; corner < corners; ++corner) {
    const std::size_t red = room_effects::kPolygonRedOffsets.at(corner);
    std::swap(polygon[red], polygon[red + 1]);
  }
}
}  // namespace

// decomp: BATTLE.PRG func_8009195C (src/BATTLE/BATTLE.PRG/2842C.c:550)
std::optional<std::size_t> FindRoomGeometryGroup(
    std::span<const std::byte> geometry, int group_index) {
  if (geometry.empty()) {
    return std::nullopt;
  }
  if (!Fits(geometry.size(), 0, room_effects::kGeometryCountBytes)) {
    VS_LOG_WARN(kChannel, "Room geometry of {} bytes has no group count",
                geometry.size());
    return std::nullopt;
  }
  const auto group_count = Load<std::int32_t>(geometry, 0);
  if (group_index < 0 || group_index >= group_count) {
    return std::nullopt;
  }
  const std::size_t offset =
      room_effects::kGeometryCountBytes +
      (static_cast<std::size_t>(group_index) * room_effects::kGroupRecordBytes);
  if (!Fits(geometry.size(), offset, room_effects::kGroupRecordBytes)) {
    VS_LOG_WARN(kChannel,
                "Geometry group {} refused: its record at {} leaves the {} "
                "byte geometry",
                group_index, offset, geometry.size());
    return std::nullopt;
  }
  return offset;
}

// decomp: BATTLE.PRG func_80091998 (src/BATTLE/BATTLE.PRG/2842C.c:562)
bool ShowRoomGeometryGroup(std::span<std::byte> geometry, int group_index) {
  return UpdateGroupFlags(geometry, group_index, room_effects::kGroupHiddenFlag,
                          0);
}

// decomp: BATTLE.PRG func_800919D8 (src/BATTLE/BATTLE.PRG/2842C.c:572)
bool HideRoomGeometryGroup(std::span<std::byte> geometry, int group_index) {
  return UpdateGroupFlags(geometry, group_index, 0,
                          room_effects::kGroupHiddenFlag);
}

// decomp: BATTLE.PRG func_80091A1C (src/BATTLE/BATTLE.PRG/2842C.c:582)
bool SetRoomGeometryGroupPosition(std::span<std::byte> geometry,
                                  int group_index,
                                  const RoomEffectPosition& position) {
  const auto group = FindRoomGeometryGroup(geometry, group_index);
  if (!group) return false;
  const auto offset =
      *group + room_effects::kGroupPositionWord * room_effects::kGroupWordBytes;
  Store(geometry, offset, position.x);
  Store(geometry, offset + room_effects::kGroupWordBytes, position.y);
  Store(geometry, offset + 2 * room_effects::kGroupWordBytes, position.z);
  return true;
}

bool SwapRoomGeometryGroupFlags(std::span<std::byte> geometry,
                                int first_group_index, int second_group_index) {
  const auto first = FindRoomGeometryGroup(geometry, first_group_index);
  const auto second = FindRoomGeometryGroup(geometry, second_group_index);
  if (!first || !second) {
    return false;
  }
  const std::uint32_t first_flags = GroupWord(geometry, *first, 0);
  SetGroupWord(geometry, *first, 0, GroupWord(geometry, *second, 0));
  SetGroupWord(geometry, *second, 0, first_flags);
  return true;
}

// decomp: BATTLE.PRG func_80091A78 (src/BATTLE/BATTLE.PRG/2842C.c:596)
bool AddRoomGeometryDepthBias(std::span<std::byte> geometry, int group_index,
                              int depth_bias) {
  const auto group = FindRoomGeometryGroup(geometry, group_index);
  if (!group) {
    return false;
  }
  const std::size_t offset = *group + room_effects::kGroupDepthBiasOffset;
  const auto bias = Load<std::uint16_t>(geometry, offset);
  Store(geometry, offset,
        static_cast<std::uint16_t>(static_cast<std::uint32_t>(bias) +
                                   static_cast<std::uint32_t>(depth_bias)));
  return true;
}

// decomp: BATTLE.PRG func_80091AC0 (src/BATTLE/BATTLE.PRG/2842C.c:606)
bool DisableRoomGeometryGroup(std::span<std::byte> geometry, int group_index) {
  return UpdateGroupFlags(geometry, group_index, 0,
                          room_effects::kGroupDisabledFlag);
}

// decomp: BATTLE.PRG func_80093914 (src/BATTLE/BATTLE.PRG/2842C.c:1400)
void SwapRoomGeometryRedGreen(std::span<std::byte> geometry, int group_index) {
  const auto group = FindRoomGeometryGroup(geometry, group_index);
  if (!group) {
    return;
  }
  const auto block = static_cast<std::int32_t>(
      GroupWord(geometry, *group, room_effects::kGroupPolygonBlockWord));
  if (block < 0 || !Fits(geometry.size(), static_cast<std::size_t>(block),
                         room_effects::kPolygonBlockCountsBytes)) {
    VS_LOG_WARN(kChannel,
                "Red-green swap of group {} refused: its polygon block at {} "
                "leaves the {} byte geometry",
                group_index, block, geometry.size());
    return;
  }
  const auto block_offset = static_cast<std::size_t>(block);
  const auto triangles = static_cast<std::uint64_t>(
      std::max(Load<std::int32_t>(geometry, block_offset), 0));
  const auto quads = static_cast<std::uint64_t>(std::max(
      Load<std::int32_t>(geometry, block_offset + sizeof(std::int32_t)), 0));
  const std::size_t first =
      block_offset + room_effects::kPolygonBlockCountsBytes;
  const std::uint64_t triangle_bytes =
      triangles * Render::kRoomTriangleRecordBytes;
  const std::uint64_t extent =
      triangle_bytes + (quads * Render::kRoomQuadRecordBytes);
  if (extent > geometry.size() - first) {
    VS_LOG_WARN(kChannel,
                "Red-green swap of group {} refused: {} triangles and {} quads "
                "at {} leave the {} byte geometry",
                group_index, triangles, quads, first, geometry.size());
    return;
  }
  const auto polygons = geometry.subspan(first);
  for (std::size_t index = 0; index < triangles; ++index) {
    SwapColorPairs(polygons.subspan(index * Render::kRoomTriangleRecordBytes),
                   room_effects::kTriangleCorners);
  }
  const auto quad_polygons =
      polygons.subspan(static_cast<std::size_t>(triangle_bytes));
  for (std::size_t index = 0; index < quads; ++index) {
    SwapColorPairs(quad_polygons.subspan(index * Render::kRoomQuadRecordBytes),
                   kRoomEffectUvCorners);
  }
}

namespace {
// The polygon a texture effect rewrites.
struct PolygonUv {
  std::size_t offset = 0;
  bool triangle = false;
};

// The polygon at a texture effect's geometry offset, refused when its UV
// bytes leave the geometry.
std::optional<PolygonUv> CheckedPolygon(std::span<const std::byte> geometry,
                                        std::int32_t geometry_offset,
                                        std::string_view effect) {
  const auto offset = static_cast<std::size_t>(geometry_offset);
  if (geometry_offset < 0 ||
      !Fits(geometry.size(), offset, room_effects::kPolygonTypeOffset + 1)) {
    VS_LOG_WARN(kChannel,
                "{} effect stopped: its polygon at {} leaves the {} byte "
                "geometry",
                effect, geometry_offset, geometry.size());
    return std::nullopt;
  }
  const auto type =
      Load<std::uint8_t>(geometry, offset + room_effects::kPolygonTypeOffset);
  const bool triangle = (type & room_effects::kPolygonShapeMask) ==
                        room_effects::kPolygonTriangleShape;
  const std::size_t extent =
      triangle ? room_effects::kTriangleUvExtent : room_effects::kQuadUvExtent;
  if (!Fits(geometry.size(), offset, extent)) {
    VS_LOG_WARN(kChannel,
                "{} effect stopped: its polygon's UVs at {} leave the {} byte "
                "geometry",
                effect, geometry_offset, geometry.size());
    return std::nullopt;
  }
  return PolygonUv{offset, triangle};
}

// Writes each corner's UV offset plus the cell origin; a triangle has three
// corners, a quad four.
template <class Effect>
void WritePolygonUv(std::span<std::byte> geometry, const PolygonUv& polygon,
                    const Effect& effect, std::uint32_t cell_u,
                    std::uint32_t cell_v) {
  const std::size_t corners =
      polygon.triangle ? room_effects::kTriangleCorners : kRoomEffectUvCorners;
  for (std::size_t corner = 0; corner < corners; ++corner) {
    Store(geometry, polygon.offset + room_effects::kPolygonUOffsets.at(corner),
          static_cast<std::uint8_t>(effect.u.at(corner) + cell_u));
    Store(geometry, polygon.offset + room_effects::kPolygonVOffsets.at(corner),
          static_cast<std::uint8_t>(effect.v.at(corner) + cell_v));
  }
}

// Advances an effect's delay counter; true when the delay has passed and
// the counter restarts.
bool TickPassed(std::uint8_t& tick, std::uint8_t delay) {
  tick = static_cast<std::uint8_t>(tick + 1);
  if (tick <= delay) {
    return false;
  }
  tick = 0;
  return true;
}

// The frame's cell in a sheet of columns, as the R3000's divu leaves it.
room_effects::UnsignedDivision DivideCell(std::uint8_t frame,
                                          std::uint8_t columns,
                                          std::string_view effect) {
  if (columns == 0) {
    VS_LOG_WARN(kChannel,
                "{} effect has no columns; frame {} uses the hardware's "
                "divide-by-zero result",
                effect, frame);
  }
  return room_effects::DivideUnsigned(frame, columns);
}

// Wraps a scrolled coordinate inside [base, base + extent - window]: below
// the base it jumps to the far edge, past the far edge back to the base.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the record's fields
int WrapScroll(int position, int base, int extent, int window) {
  if (position < base) {
    position = base + extent - window;
  }
  if (base + extent < position + window) {
    position = base;
  }
  return position;
}

// The VRAM sheet keeps its texture pages 320 columns right of and 256 rows
// above the copy targets; each target also has a half-size copy.
constexpr int kImageSheetColumnOffset = 320;
constexpr int kImageSheetRowOffset = 256;
constexpr int kHalfImageColumnSpan = 64;
constexpr int kHalfImageColumnMask = 0x3C0;
constexpr int kHalfImageRowSpan = 256;
constexpr int kHalfImageRowMask = 0x100;

int HalfImageX(int x) {
  return ((x % kHalfImageColumnSpan) / 2) - kHalfImageColumnSpan +
         (x & kHalfImageColumnMask);
}

int HalfImageY(int y) {
  return ((y % kHalfImageRowSpan) / 2) + (y & kHalfImageRowMask);
}

std::int16_t Short(std::int64_t value) {
  return static_cast<std::int16_t>(value);
}

// The next animation cell: frames run 1 to frame_count.
RoomImageRect NextImageCell(TextureImageEffect& effect,
                            const RoomImageRect& sheet) {
  effect.frame = static_cast<std::uint8_t>(effect.frame + 1);
  if (effect.frame > effect.frame_count) {
    effect.frame = 1;
  }
  const auto cell = DivideCell(effect.frame, effect.columns, "Texture image");
  const std::uint32_t x =
      static_cast<std::uint32_t>(sheet.x) +
      (static_cast<std::uint32_t>(sheet.w) * cell.remainder);
  const std::uint32_t y = static_cast<std::uint32_t>(sheet.y) +
                          (static_cast<std::uint32_t>(sheet.h) * cell.quotient);
  return {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), sheet.w,
          sheet.h};
}

// The next band of a scrolling sheet: the right half from the scroll row
// down.
RoomImageRect NextImageBand(TextureImageEffect& effect,
                            const RoomImageRect& sheet) {
  effect.frame =
      static_cast<std::uint8_t>(static_cast<std::uint32_t>(effect.frame) +
                                static_cast<std::uint32_t>(effect.scroll_step));
  if (effect.frame >= sheet.h) {
    effect.frame = 0;
  }
  return {Short(sheet.x + (sheet.w / 2)), Short(sheet.y + effect.frame),
          Short(sheet.w / 2), Short(sheet.h - effect.frame)};
}

// A swing's geometry group, refused when its rotation words leave the
// geometry.
std::optional<std::size_t> CheckedSwingGroup(
    std::span<const std::byte> geometry, std::int32_t geometry_offset,
    std::string_view effect) {
  const auto offset = static_cast<std::size_t>(geometry_offset);
  if (geometry_offset < 0 ||
      !Fits(geometry.size(), offset,
            kSwingGroupWords * room_effects::kGroupWordBytes)) {
    VS_LOG_WARN(kChannel,
                "{} refused: its group at {} leaves the {} byte geometry",
                effect, geometry_offset, geometry.size());
    return std::nullopt;
  }
  return offset;
}

// Axis 1 turns the group about Y and marks its rotation for the renderer;
// axis 2 turns it about X; any other about Z.
constexpr std::uint8_t kSwingAxisY = 1;
constexpr std::uint8_t kSwingAxisX = 2;

void WriteSwingAngle(std::span<std::byte> geometry, std::size_t group,
                     std::uint8_t axis, std::int32_t angle) {
  const auto value = static_cast<std::uint32_t>(angle);
  if (axis == kSwingAxisY) {
    SetGroupWord(geometry, group, kRotationYWord, value);
    SetGroupWord(
        geometry, group, 0,
        GroupWord(geometry, group, 0) | room_effects::kGroupSwingRotationFlags);
  } else if (axis == kSwingAxisX) {
    SetGroupWord(geometry, group, kRotationXWord, value);
  } else {
    SetGroupWord(geometry, group, kRotationZWord, value);
  }
}

// The swing heads back the other way next time it starts.
void ReverseSwing(SwingEffect& effect) {
  const std::int32_t previous_start = effect.start_angle;
  effect.initial_velocity = WrappingNegate(effect.initial_velocity);
  effect.angular_velocity = effect.initial_velocity;
  effect.start_angle = effect.rest_angle;
  effect.rest_angle = previous_start;
  effect.acceleration = WrappingNegate(effect.acceleration);
}

constexpr std::uint32_t kSwingAccelerationScale = 8;
constexpr std::uint32_t kSwingShiftMask = 31;
constexpr std::int32_t kSwingRestSpeed = 0x21;
}  // namespace

// decomp: BATTLE.PRG func_80091C6C (src/BATTLE/BATTLE.PRG/2842C.c:640)
bool AdvanceRoomTextureFrames(TextureFrameEffect& effect,
                              std::span<std::byte> geometry) {
  const auto polygon =
      CheckedPolygon(geometry, effect.geometry_offset, "Texture frame");
  if (!polygon) {
    return false;
  }
  if (!TickPassed(effect.tick, effect.delay)) {
    return true;
  }
  effect.frame = static_cast<std::uint8_t>(effect.frame + 1);
  if (effect.frame >= effect.frame_count) {
    effect.frame = 0;
  }
  const auto cell = DivideCell(effect.frame, effect.columns, "Texture frame");
  const std::uint32_t cell_u =
      effect.base_u +
      (static_cast<std::uint32_t>(effect.frame_width) * cell.remainder);
  const std::uint32_t cell_v =
      effect.base_v +
      (static_cast<std::uint32_t>(effect.frame_height) * cell.quotient);
  WritePolygonUv(geometry, *polygon, effect, cell_u, cell_v);
  return true;
}

// decomp: BATTLE.PRG func_80091E10 (src/BATTLE/BATTLE.PRG/2842C.c:682)
bool ScrollRoomTextureCoordinates(TextureScrollEffect& effect,
                                  std::span<std::byte> geometry) {
  const auto polygon =
      CheckedPolygon(geometry, effect.geometry_offset, "Texture scroll");
  if (!polygon) {
    return false;
  }
  if (!TickPassed(effect.tick, effect.delay)) {
    return true;
  }
  const int u = WrapScroll(effect.current_u + effect.step_u, effect.base_u,
                           effect.width, effect.window_width);
  const int v = WrapScroll(effect.current_v + effect.step_v, effect.base_v,
                           effect.height, effect.window_height);
  effect.current_u = static_cast<std::uint8_t>(u);
  effect.current_v = static_cast<std::uint8_t>(v);
  WritePolygonUv(geometry, *polygon, effect, static_cast<std::uint32_t>(u),
                 static_cast<std::uint32_t>(v));
  return true;
}

// decomp: BATTLE.PRG func_80091FE8 (src/BATTLE/BATTLE.PRG/2842C.c:738)
bool AnimateRoomTextureImages(TextureImageEffect& effect,
                              RoomEffectHost& host) {
  if (!TickPassed(effect.tick, effect.delay)) {
    return true;
  }
  const RoomImageRect sheet{Short(effect.rect.x - kImageSheetColumnOffset),
                            Short(effect.rect.y + kImageSheetRowOffset),
                            effect.rect.w, effect.rect.h};
  RoomImageRect source = effect.scroll_step == 0 ? NextImageCell(effect, sheet)
                                                 : NextImageBand(effect, sheet);
  const int half_x = HalfImageX(sheet.x);
  RoomImageRect half_source{Short(HalfImageX(source.x)),
                            Short(HalfImageY(source.y)), Short(source.w / 2),
                            Short(source.h / 2)};
  host.MoveImage(source, sheet.x, sheet.y);
  host.MoveImage(half_source, half_x, HalfImageY(sheet.y));
  if (effect.scroll_step != 0 && effect.frame != 0) {
    // The rows scrolled off the top wrap to the band's foot.
    source.y = sheet.y;
    source.h = effect.frame;
    const int wrap_y = sheet.y + sheet.h - effect.frame;
    half_source.y = Short(HalfImageY(source.y));
    half_source.h = Short(source.h / 2);
    host.MoveImage(source, sheet.x, wrap_y);
    host.MoveImage(half_source, half_x, HalfImageY(wrap_y));
  }
  return true;
}

// decomp: BATTLE.PRG func_800923F8 (src/BATTLE/BATTLE.PRG/2842C.c:801)
bool UpdateRoomRotationEffect(SwingEffect& effect,
                              std::span<std::byte> geometry) {
  const auto group =
      CheckedSwingGroup(geometry, effect.geometry_offset, "Swing effect");
  if (!group) {
    return false;
  }
  if (effect.needs_reset != 0) {
    effect.needs_reset = 0;
    effect.angle = effect.start_angle;
    effect.angular_velocity = effect.initial_velocity;
  }
  effect.angle = WrappingAdd(effect.angle, effect.angular_velocity);
  effect.angular_velocity =
      WrappingAdd(effect.angular_velocity,
                  static_cast<std::int32_t>(
                      static_cast<std::uint32_t>(effect.acceleration) *
                      kSwingAccelerationScale));
  std::uint32_t delta = (static_cast<std::uint32_t>(effect.angle) -
                         static_cast<std::uint32_t>(effect.rest_angle)) &
                        kAngleMask;
  if (delta > kHalfTurn) {
    delta = kFullTurn - delta;
  }
  bool at_rest = false;
  if (static_cast<std::int32_t>(delta) < WrappingAbs(effect.angular_velocity)) {
    const auto shift =
        static_cast<std::uint32_t>(effect.damping_shift) & kSwingShiftMask;
    effect.angular_velocity = WrappingNegate(effect.angular_velocity >> shift);
    if (WrappingAbs(effect.angular_velocity) < kSwingRestSpeed) {
      effect.angle = effect.rest_angle;
      at_rest = true;
    }
  }
  WriteSwingAngle(geometry, *group, effect.axis, effect.angle);
  if (!at_rest) {
    return true;
  }
  ReverseSwing(effect);
  return false;
}

// decomp: BATTLE.PRG func_80092540 (src/BATTLE/BATTLE.PRG/2842C.c:855),
// func_80092F6C (2842C.c:1156) and func_800936F0 (2842C.c:1315) share this
// machine code.
bool SkipRoomEffectCommand() { return false; }

namespace {
// The record codec for each texture effect kind.
template <class Record>
struct RecordCodec;

template <>
struct RecordCodec<TextureFrameEffect> {
  static constexpr std::size_t kBytes = room_effects::kTextureFrameEffectBytes;
  static constexpr auto kDecode = &room_effects::DecodeTextureFrameEffect;
  static constexpr auto kEncode = &room_effects::EncodeTextureFrameEffect;
};

template <>
struct RecordCodec<TextureImageEffect> {
  static constexpr std::size_t kBytes = room_effects::kTextureImageEffectBytes;
  static constexpr auto kDecode = &room_effects::DecodeTextureImageEffect;
  static constexpr auto kEncode = &room_effects::EncodeTextureImageEffect;
};

template <>
struct RecordCodec<TextureScrollEffect> {
  static constexpr std::size_t kBytes = room_effects::kTextureScrollEffectBytes;
  static constexpr auto kDecode = &room_effects::DecodeTextureScrollEffect;
  static constexpr auto kEncode = &room_effects::EncodeTextureScrollEffect;
};

template <>
struct RecordCodec<SwingEffect> {
  static constexpr std::size_t kBytes = room_effects::kSwingEffectBytes;
  static constexpr auto kDecode = &room_effects::DecodeSwingEffect;
  static constexpr auto kEncode = &room_effects::EncodeSwingEffect;
};

// Decodes the texture effect record at offset, runs step on it and stores
// it back; a record outside the section is refused and reported stopped.
template <class Record, class Step>
bool StepRecord(std::span<std::byte> records, std::size_t offset, Step step) {
  using Codec = RecordCodec<Record>;
  if (!Fits(records.size(), offset, Codec::kBytes)) {
    VS_LOG_WARN(kChannel,
                "Texture effect record at {} refused: it leaves the {} byte "
                "section",
                offset, records.size());
    return false;
  }
  const auto bytes = records.subspan(offset).template first<Codec::kBytes>();
  Record record = Codec::kDecode(bytes);
  const bool running = step(record);
  Codec::kEncode(record, bytes);
  return running;
}

// D_800E85CC: runs one texture effect; false stops it.
bool RunTextureEffect(const RoomEffectInstance& instance,
                      RoomEffectHost& host) {
  const auto records = host.TextureEffects();
  const std::size_t offset = instance.data_offset;
  switch (static_cast<RoomTextureEffectKind>(instance.kind_or_object_count)) {
    case RoomTextureEffectKind::kFrames:
      return StepRecord<TextureFrameEffect>(
          records, offset, [&host](TextureFrameEffect& effect) {
            return AdvanceRoomTextureFrames(effect, host.Geometry());
          });
    case RoomTextureEffectKind::kImages:
      return StepRecord<TextureImageEffect>(
          records, offset, [&host](TextureImageEffect& effect) {
            return AnimateRoomTextureImages(effect, host);
          });
    case RoomTextureEffectKind::kScroll:
      return StepRecord<TextureScrollEffect>(
          records, offset, [&host](TextureScrollEffect& effect) {
            return ScrollRoomTextureCoordinates(effect, host.Geometry());
          });
    case RoomTextureEffectKind::kSwing:
      return StepRecord<SwingEffect>(
          records, offset, [&host](SwingEffect& effect) {
            return UpdateRoomRotationEffect(effect, host.Geometry());
          });
    default:
      return SkipRoomEffectCommand();
  }
}

constexpr auto kScriptSlots = kRoomEffectSlotCount - kRoomEffectScriptSlotBase;

// The slot counts, held to the slots that exist.
std::size_t TextureSlotCount(const RoomEffectState& state) {
  return std::min(state.texture_effect_count, kRoomEffectScriptSlotBase);
}

std::size_t ScriptSlotCount(const RoomEffectState& state) {
  return std::min(state.script_count, kScriptSlots);
}

std::size_t ReportedSlotCount(std::size_t count, std::size_t limit,
                              std::string_view what) {
  if (count > limit) {
    VS_LOG_WARN(kChannel, "{} count {} held to the {} slots", what, count,
                limit);
    return limit;
  }
  return count;
}

// Frees a script's objects (FreeLiveFromLowEnd in the original).
void ReleaseObjects(RoomEffectInstance& instance, RoomEffectHost& host) {
  if (instance.storage != RoomEffectStorageId::kNone) {
    host.ReleaseObjectStorage(instance.storage);
    instance.storage = RoomEffectStorageId::kNone;
  }
  instance.objects.clear();
  ++instance.generation;
}

// Walks the texture effect section into the first slots; returns the count.
std::size_t SetUpTextureEffects(RoomEffectState& state,
                                std::span<std::byte> effects) {
  std::size_t consumed = 0;
  std::size_t count = 0;
  while (consumed < effects.size()) {
    if (!Fits(effects.size(), consumed, room_effects::kEffectKindOffset + 1)) {
      VS_LOG_WARN(kChannel,
                  "Texture effect at {} refused: its kind leaves the {} byte "
                  "section",
                  consumed, effects.size());
      break;
    }
    const auto kind =
        Load<std::uint8_t>(effects, consumed + room_effects::kEffectKindOffset);
    if (kind >= kRoomTextureEffectKindCount ||
        room_effects::kTextureEffectStrides.at(kind) == 0) {
      break;
    }
    if (count >= kRoomEffectScriptSlotBase) {
      VS_LOG_WARN(kChannel, "Texture effects past slot {} refused",
                  kRoomEffectScriptSlotBase);
      break;
    }
    if (!Fits(effects.size(), consumed,
              room_effects::kTextureEffectRecordBytes.at(kind))) {
      VS_LOG_WARN(kChannel,
                  "Texture effect at {} refused: its kind {} record leaves "
                  "the {} byte section",
                  consumed, kind, effects.size());
      break;
    }
    RoomEffectInstance& instance = state.instances.at(count);
    instance.data_offset = consumed;
    instance.active =
        Load<std::uint8_t>(
            effects, consumed + room_effects::kEffectStartsInactiveOffset) == 0
            ? 1
            : 0;
    instance.update_enabled = 1;
    instance.kind_or_object_count = kind;
    Store<std::uint8_t>(effects,
                        consumed + room_effects::kEffectNeedsResetOffset, 1);
    consumed += room_effects::kTextureEffectStrides.at(kind);
    ++count;
  }
  return count;
}

// The bytes one effect script takes: its preamble, its commands (a jump
// counts as its header) and the zero-size terminator; none when it leaves
// the section.
std::optional<std::size_t> MeasureEffectScript(
    std::span<const std::byte> scripts, std::size_t start) {
  if (!Fits(scripts.size(), start, room_effects::kScriptHeaderBytes)) {
    VS_LOG_WARN(kChannel,
                "Effect script at {} refused: its header leaves the "
                "{} byte section",
                start, scripts.size());
    return std::nullopt;
  }
  std::size_t offset = start + room_effects::ScriptPreambleBytes(
                                   Load<std::uint8_t>(scripts, start));
  while (true) {
    if (!Fits(scripts.size(), offset, kRoomEffectCommandHeaderBytes)) {
      VS_LOG_WARN(kChannel,
                  "Effect script at {} refused: its command at {} leaves the "
                  "{} byte section",
                  start, offset, scripts.size());
      return std::nullopt;
    }
    const auto opcode = static_cast<RoomEffectOpcode>(Load<std::int16_t>(
        scripts, offset + room_effects::kCommandOpcodeOffset));
    const int size = opcode == RoomEffectOpcode::kJump
                         ? static_cast<int>(kRoomEffectCommandHeaderBytes)
                         : Load<std::int16_t>(scripts, offset);
    if (size == 0) {
      break;
    }
    if (size < 0) {
      VS_LOG_WARN(kChannel,
                  "Effect script at {} refused: its command at {} has size {}",
                  start, offset, size);
      return std::nullopt;
    }
    offset += static_cast<std::size_t>(size);
  }
  return offset + kRoomEffectCommandHeaderBytes - start;
}

void SetUpEffectScript(RoomEffectInstance& instance, RoomEffectHost& host,
                       std::span<const std::byte> scripts, std::size_t start) {
  instance.active =
      Load<std::uint8_t>(scripts, start + room_effects::kScriptActiveOffset);
  instance.kind_or_object_count = Load<std::uint8_t>(scripts, start);
  instance.update_enabled = 1;
  instance.data_offset = start;
  if (instance.storage != RoomEffectStorageId::kNone) {
    // The original overwrites the pointer without freeing it.
    VS_LOG_WARN(kChannel,
                "Effect script at {} replaces objects that were never "
                "released; leaving them to the heap",
                start);
    instance.storage = RoomEffectStorageId::kNone;
  }
  instance.objects.clear();
  ++instance.generation;
  if (instance.active != 0) {
    CreateRoomEffectObjects(instance, host);
  }
}

// One script object as func_800927AC fills it in from its group id.
RoomEffectObject MakeEffectObject(std::span<const std::byte> geometry,
                                  std::int8_t group_id, std::size_t cursor) {
  RoomEffectObject object;
  object.cursor = cursor;
  const auto group =
      group_id >= 0 ? FindRoomGeometryGroup(geometry, group_id) : std::nullopt;
  if (!group) {
    return object;
  }
  const auto scaled = [&](std::size_t word) {
    return static_cast<std::int32_t>(GroupWord(geometry, *group, word) *
                                     static_cast<std::uint32_t>(kSubunitScale));
  };
  const auto angle = [&](std::size_t word) {
    return static_cast<std::int16_t>(GroupWord(geometry, *group, word));
  };
  // The id is the script's signed char widened; negative ids stop above.
  // NOLINTNEXTLINE(bugprone-signed-char-misuse)
  object.object_id = group_id;
  object.group_offset = *group;
  object.position = {scaled(room_effects::kGroupPositionWord),
                     scaled(room_effects::kGroupPositionWord + 1),
                     scaled(room_effects::kGroupPositionWord + 2)};
  object.rotation = {angle(kRotationXWord), angle(kRotationYWord),
                     angle(kRotationZWord)};
  return object;
}
}  // namespace

// decomp: BATTLE.PRG func_80091B04 (src/BATTLE/BATTLE.PRG/2842C.c:616)
void UpdateRoomEffects(RoomEffectState& state, RoomEffectHost& host) {
  const std::size_t texture_count = ReportedSlotCount(
      state.texture_effect_count, kRoomEffectScriptSlotBase, "Texture effect");
  for (std::size_t index = 0; index < texture_count; ++index) {
    RoomEffectInstance& instance = state.instances.at(index);
    if (instance.active != 0 &&
        instance.kind_or_object_count < kRoomTextureEffectKindCount &&
        instance.update_enabled != 0) {
      instance.active = RunTextureEffect(instance, host) ? 1 : 0;
    }
  }
  const std::size_t script_count =
      ReportedSlotCount(state.script_count, kScriptSlots, "Effect script");
  for (std::size_t index = 0; index < script_count; ++index) {
    const std::size_t slot = kRoomEffectScriptSlotBase + index;
    if (state.instances.at(slot).active == 0 ||
        state.instances.at(slot).update_enabled == 0) {
      continue;
    }
    const bool running = ExecuteRoomEffectCommands(state, host, slot);
    RoomEffectInstance& instance = state.instances.at(slot);
    instance.active = running ? 1 : 0;
    if (!running) {
      ReleaseObjects(instance, host);
    }
  }
}

// decomp: BATTLE.PRG func_80092548 (src/BATTLE/BATTLE.PRG/2842C.c:857)
void InitializeRoomEffects(RoomEffectState& state, RoomEffectHost& host) {
  state.sound_id = -1;
  state.sound_restored = false;
  state.texture_effect_count =
      SetUpTextureEffects(state, host.TextureEffects());
  state.script_count = 0;
  const auto scripts = host.EffectScripts();
  std::size_t consumed = 0;
  while (consumed < scripts.size()) {
    const std::size_t slot = kRoomEffectScriptSlotBase + state.script_count;
    if (slot >= kRoomEffectSlotCount) {
      VS_LOG_WARN(kChannel, "Effect scripts past slot {} refused",
                  kRoomEffectSlotCount);
      return;
    }
    const auto extent = MeasureEffectScript(scripts, consumed);
    if (!extent) {
      return;
    }
    SetUpEffectScript(state.instances.at(slot), host, scripts, consumed);
    consumed += *extent;
    ++state.script_count;
  }
}

// decomp: BATTLE.PRG func_800927AC (src/BATTLE/BATTLE.PRG/2842C.c:951)
void CreateRoomEffectObjects(RoomEffectInstance& instance,
                             RoomEffectHost& host) {
  instance.objects.clear();
  ++instance.generation;
  if (instance.storage != RoomEffectStorageId::kNone) {
    VS_LOG_WARN(kChannel,
                "Effect script at {} rebuilt its objects without releasing "
                "them; releasing them now",
                instance.data_offset);
    host.ReleaseObjectStorage(instance.storage);
    instance.storage = RoomEffectStorageId::kNone;
  }
  const auto scripts = host.EffectScripts();
  const std::uint8_t count = instance.kind_or_object_count;
  const std::size_t preamble = room_effects::ScriptPreambleBytes(count);
  if (!Fits(scripts.size(), instance.data_offset, preamble)) {
    VS_LOG_WARN(kChannel,
                "Effect script at {} refused: its {} group ids leave the {} "
                "byte section",
                instance.data_offset, count, scripts.size());
    return;
  }
  const auto storage =
      host.ReserveObjectStorage(count * kRoomEffectObjectRecordBytes);
  if (!storage) {
    VS_LOG_WARN(kChannel, "Effect script at {}: the heap refused {} objects",
                instance.data_offset, count);
    return;
  }
  instance.storage = *storage;
  const auto geometry = host.Geometry();
  const std::size_t ids =
      instance.data_offset + room_effects::kScriptHeaderBytes;
  const std::size_t cursor = instance.data_offset + preamble;
  instance.objects.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    instance.objects.push_back(MakeEffectObject(
        geometry, Load<std::int8_t>(scripts, ids + index), cursor));
  }
}

// decomp: BATTLE.PRG func_80092914 (src/BATTLE/BATTLE.PRG/2842C.c:988)
void FinishRoomEffectSetup() {}

// decomp: BATTLE.PRG func_8009291C (src/BATTLE/BATTLE.PRG/2842C.c:990)
bool StartRoomEffect(RoomEffectState& state, RoomEffectHost& host,
                     int effect_index) {
  if (effect_index < 0) {
    VS_LOG_WARN(kChannel, "Room effect {} refused: it is negative",
                effect_index);
    return false;
  }
  const auto index = static_cast<std::size_t>(effect_index);
  if (index < TextureSlotCount(state)) {
    state.instances.at(index).active = 1;
    return true;
  }
  if (index < kRoomEffectScriptSlotBase ||
      index >= kRoomEffectScriptSlotBase + ScriptSlotCount(state)) {
    return false;
  }
  RoomEffectInstance& instance = state.instances.at(index);
  ReleaseObjects(instance, host);
  instance.active = 1;
  CreateRoomEffectObjects(instance, host);
  return true;
}

// decomp: BATTLE.PRG func_80092A18 (src/BATTLE/BATTLE.PRG/2842C.c:1012)
bool ReleaseRoomEffects(RoomEffectState& state, RoomEffectHost& host) {
  if (state.script_count == 0) {
    return false;
  }
  const std::size_t script_count = ScriptSlotCount(state);
  for (std::size_t index = 0; index < script_count; ++index) {
    RoomEffectInstance& instance =
        state.instances.at(kRoomEffectScriptSlotBase + index);
    if (instance.active != 0 ||
        instance.storage != RoomEffectStorageId::kNone) {
      instance.active = 0;
      ReleaseObjects(instance, host);
    }
  }
  if (state.sound_id != -1 && host.HasSoundBank()) {
    host.StopSound(state.sound_id);
    state.sound_id = -1;
  }
  state.script_count = 0;
  return true;
}

namespace {
// The most handler calls one object may make in one pass; past it the
// script is taken to loop without waiting and is ended.
constexpr std::size_t kHandlerCallLimit = std::size_t{1} << 20U;

// One pass over a script slot's objects.
struct ScriptRun {
  RoomEffectState& state;
  RoomEffectHost& host;
  std::size_t slot = 0;
  std::uint32_t generation = 0;
  std::size_t handler_calls = 0;
};

RoomEffectObject& RunObject(ScriptRun& run, std::size_t object_index) {
  return run.state.instances.at(run.slot).objects.at(object_index);
}

// D_800E85E8: the handler for each opcode.
RoomEffectCommandResult Dispatch(ScriptRun& run, std::size_t object_index,
                                 const RoomEffectCommand& command) {
  RoomEffectState& state = run.state;
  RoomEffectHost& host = run.host;
  RoomEffectObject& object = RunObject(run, object_index);
  switch (command.opcode) {
    case RoomEffectOpcode::kRadialVelocity:
      return SetRoomEffectRadialVelocity(object, command);
    case RoomEffectOpcode::kVelocity:
      return SetRoomEffectVelocity(object, command);
    case RoomEffectOpcode::kHideObject:
      return HideRoomEffectObject(object, host.Geometry());
    case RoomEffectOpcode::kShowGroup:
      return ExecuteRoomEffectShowGroup(command, host.Geometry());
    case RoomEffectOpcode::kAdvanceMotion:
      return AdvanceRoomEffectMotion(object, command, host);
    case RoomEffectOpcode::kPlaySound:
      return ExecuteRoomEffectPlaySound(command, state, host);
    case RoomEffectOpcode::kStopSound:
      return ExecuteRoomEffectStopSound(command, state, host);
    case RoomEffectOpcode::kSetTransform:
      return SetRoomEffectTransform(object, command, host.Geometry());
    case RoomEffectOpcode::kStartEffect:
      return ExecuteRoomEffectStartEffect(command, state, host);
    case RoomEffectOpcode::kHideGroup:
      return ExecuteRoomEffectHideGroup(command, host.Geometry());
    case RoomEffectOpcode::kStopScript:
      return ExecuteRoomEffectStopScript(command, state, host);
    case RoomEffectOpcode::kSkip:
    case RoomEffectOpcode::kJump:
      break;
  }
  return SkipRoomEffectCommand() ? RoomEffectCommandResult::kWait
                                 : RoomEffectCommandResult::kAdvance;
}

enum class CommandOutcome : std::uint8_t {
  kAdvance,
  // The object waits on the command until the next pass.
  kYield,
  // The object's script ended.
  kEnded,
  // A handler rebuilt or released this slot's objects.
  kInstanceChanged,
};

// Runs one command's handler, repeating a waiting handler while the scripts
// fast-forward.
CommandOutcome RunCommand(ScriptRun& run, std::size_t object_index,
                          const RoomEffectCommand& command, bool fast_forward) {
  while (true) {
    if (++run.handler_calls > kHandlerCallLimit) {
      VS_LOG_WARN(kChannel,
                  "Effect script slot {} object {} ended: {} handler calls "
                  "without waiting",
                  run.slot, object_index, kHandlerCallLimit);
      RunObject(run, object_index).cursor.reset();
      return CommandOutcome::kEnded;
    }
    const RoomEffectCommandResult result = Dispatch(run, object_index, command);
    if (run.state.instances.at(run.slot).generation != run.generation) {
      VS_LOG_WARN(kChannel,
                  "Effect script slot {} rebuilt its own objects; ending the "
                  "pass",
                  run.slot);
      return CommandOutcome::kInstanceChanged;
    }
    if (result == RoomEffectCommandResult::kAdvance) {
      return CommandOutcome::kAdvance;
    }
    if (!fast_forward) {
      return CommandOutcome::kYield;
    }
  }
}

// Steps one object's script until it waits or ends; false when a handler
// rebuilt the slot's objects.
bool RunObjectScript(ScriptRun& run, std::size_t object_index) {
  run.handler_calls = 0;
  while (true) {
    const std::size_t cursor = *RunObject(run, object_index).cursor;
    const auto decoded =
        room_effects::DecodeCommand(run.host.EffectScripts(), cursor);
    if (decoded.status != room_effects::CommandStatus::kDecoded) {
      if (decoded.status != room_effects::CommandStatus::kEnd) {
        VS_LOG_WARN(kChannel,
                    "Effect script slot {} object {} ended: command at {} "
                    "(size {}, opcode {}) refused",
                    run.slot, object_index, cursor, decoded.command.size,
                    static_cast<int>(decoded.command.opcode));
      }
      RunObject(run, object_index).cursor.reset();
      return true;
    }
    const bool fast_forward = run.host.FastForwarding();
    if (fast_forward && decoded.command.opcode == RoomEffectOpcode::kJump) {
      return true;
    }
    switch (RunCommand(run, object_index, decoded.command, fast_forward)) {
      case CommandOutcome::kAdvance:
        break;
      case CommandOutcome::kYield:
      case CommandOutcome::kEnded:
        return true;
      case CommandOutcome::kInstanceChanged:
        return false;
    }
    const std::int64_t next =
        static_cast<std::int64_t>(cursor) + decoded.command.size;
    if (next < 0) {
      VS_LOG_WARN(kChannel,
                  "Effect script slot {} object {} ended: the command at {} "
                  "jumps before the section",
                  run.slot, object_index, cursor);
      RunObject(run, object_index).cursor.reset();
      return true;
    }
    RunObject(run, object_index).cursor = static_cast<std::size_t>(next);
  }
}

// The command operands' byte offsets (Field_RoomEffectCommand, 2842C.c:34).
// One-operand commands read the word at 4; the sound and motion commands
// read two halfwords there.
constexpr std::size_t kWordOperand = 4;
constexpr std::size_t kFirstHalfOperand = 4;
constexpr std::size_t kSecondHalfOperand = 6;
// func_80092C68: the centre, the speed and the damping.
constexpr std::size_t kRadialCentreX = 4;
constexpr std::size_t kRadialCentreY = 6;
constexpr std::size_t kRadialCentreZ = 8;
constexpr std::size_t kRadialSpeed = 0xA;
constexpr std::size_t kRadialDamping = 0xC;
// func_80092E14: the velocity, its damping, the spin and its damping.
constexpr std::size_t kVelocityX = 4;
constexpr std::size_t kVelocityY = 6;
constexpr std::size_t kVelocityZ = 8;
constexpr std::size_t kVelocityDamping = 0xA;
constexpr std::size_t kSpinX = 0xC;
constexpr std::size_t kSpinY = 0xE;
constexpr std::size_t kSpinZ = 0x10;
constexpr std::size_t kSpinDamping = 0x12;
// func_80092E7C: the position words, then the rotation's low halfwords.
constexpr std::size_t kTransformX = 4;
constexpr std::size_t kTransformY = 8;
constexpr std::size_t kTransformZ = 0xC;
constexpr std::size_t kTransformRotationX = 0x10;
constexpr std::size_t kTransformRotationY = 0x14;
constexpr std::size_t kTransformRotationZ = 0x18;

// One axis of a radial push: the speed less the distance from the centre,
// pointing away from it, or nothing past the speed's reach.
std::int16_t RadialAxisVelocity(std::int32_t position, std::int16_t centre,
                                std::uint16_t speed) {
  const std::int32_t distance = (position / kSubunitScale) - centre;
  const auto velocity = static_cast<std::int16_t>(
      static_cast<std::int32_t>(speed) - WrappingAbs(distance));
  if (velocity <= 0) {
    return 0;
  }
  const std::int32_t relative = WrappingAdd(
      position, -(static_cast<std::int32_t>(centre) * kSubunitScale));
  return relative < 0 ? static_cast<std::int16_t>(-velocity) : velocity;
}

constexpr std::int32_t kRadialVelocityDivisor = 4;
constexpr std::int32_t kRadialSpinXMask = 0x1F;
constexpr std::int32_t kRadialSpinXBias = 16;
constexpr std::int32_t kRadialSpinYMask = 0x3F;
constexpr std::int32_t kRadialSpinYBias = 32;
}  // namespace

// decomp: BATTLE.PRG func_80092B04 (src/BATTLE/BATTLE.PRG/2842C.c:1038)
bool ExecuteRoomEffectCommands(RoomEffectState& state, RoomEffectHost& host,
                               std::size_t instance_index) {
  if (instance_index >= kRoomEffectSlotCount) {
    VS_LOG_WARN(kChannel, "Effect script slot {} refused: there are {}",
                instance_index, kRoomEffectSlotCount);
    return false;
  }
  ScriptRun run{state, host, instance_index,
                state.instances.at(instance_index).generation, 0};
  const std::size_t object_count =
      state.instances.at(instance_index).objects.size();
  std::size_t idle = 0;
  for (std::size_t index = 0; index < object_count; ++index) {
    if (!RunObject(run, index).cursor) {
      ++idle;
      continue;
    }
    if (!RunObjectScript(run, index)) {
      return state.instances.at(instance_index).active != 0;
    }
  }
  return idle != object_count;
}

// decomp: BATTLE.PRG func_80092C68 (src/BATTLE/BATTLE.PRG/2842C.c:1070)
RoomEffectCommandResult SetRoomEffectRadialVelocity(
    RoomEffectObject& object, const RoomEffectCommand& command) {
  const std::uint16_t speed = command.Half(kRadialSpeed);
  object.velocity.x = RadialAxisVelocity(
      object.position.x, command.SignedHalf(kRadialCentreX), speed);
  object.velocity.y = RadialAxisVelocity(
      object.position.y, command.SignedHalf(kRadialCentreY), speed);
  object.velocity.z = RadialAxisVelocity(
      object.position.z, command.SignedHalf(kRadialCentreZ), speed);
  object.velocity.x =
      static_cast<std::int16_t>(object.velocity.x / kRadialVelocityDivisor);
  object.velocity.y =
      static_cast<std::int16_t>(object.velocity.y / kRadialVelocityDivisor);
  object.velocity.z =
      static_cast<std::int16_t>(object.velocity.z / kRadialVelocityDivisor);
  object.velocity_damping =
      static_cast<std::int16_t>(command.Half(kRadialDamping) + 1);
  object.angular_velocity = {
      static_cast<std::int16_t>((object.velocity.x & kRadialSpinXMask) -
                                kRadialSpinXBias),
      static_cast<std::int16_t>((object.velocity.y & kRadialSpinYMask) -
                                kRadialSpinYBias),
      0};
  object.angular_damping = 0;
  return RoomEffectCommandResult::kAdvance;
}

// decomp: BATTLE.PRG func_80092E14 (src/BATTLE/BATTLE.PRG/2842C.c:1123)
RoomEffectCommandResult SetRoomEffectVelocity(
    RoomEffectObject& object, const RoomEffectCommand& command) {
  object.velocity = {command.SignedHalf(kVelocityX),
                     command.SignedHalf(kVelocityY),
                     command.SignedHalf(kVelocityZ)};
  object.velocity_damping = command.SignedHalf(kVelocityDamping);
  object.angular_velocity = {command.SignedHalf(kSpinX),
                             command.SignedHalf(kSpinY),
                             command.SignedHalf(kSpinZ)};
  object.angular_damping = command.SignedHalf(kSpinDamping);
  return RoomEffectCommandResult::kAdvance;
}

// decomp: BATTLE.PRG func_80092E7C (src/BATTLE/BATTLE.PRG/2842C.c:1134)
RoomEffectCommandResult SetRoomEffectTransform(RoomEffectObject& object,
                                               const RoomEffectCommand& command,
                                               std::span<std::byte> geometry) {
  object.position = {command.Word(kTransformX), command.Word(kTransformY),
                     command.Word(kTransformZ)};
  object.rotation = {command.SignedHalf(kTransformRotationX),
                     command.SignedHalf(kTransformRotationY),
                     command.SignedHalf(kTransformRotationZ)};
  WriteRoomEffectTransform(object, geometry);
  return RoomEffectCommandResult::kAdvance;
}

namespace {
// The group record an object moves, refused when it leaves the geometry.
std::optional<std::size_t> ObjectGroup(const RoomEffectObject& object,
                                       std::span<const std::byte> geometry) {
  if (object.object_id < 0) {
    return std::nullopt;
  }
  if (!Fits(geometry.size(), object.group_offset,
            room_effects::kGroupRecordBytes)) {
    VS_LOG_WARN(kChannel,
                "Effect object {} refused: its group at {} leaves the {} byte "
                "geometry",
                object.object_id, object.group_offset, geometry.size());
    return std::nullopt;
  }
  return object.group_offset;
}

constexpr std::uint16_t kGravityMotion = 1;
constexpr std::uint16_t kDampedMotion = 2;
constexpr std::uint16_t kUntilStill = 0x8000;
constexpr std::uint32_t kFloorHeightShift = 17;
constexpr std::int16_t kMaxBounces = 3;
constexpr std::int32_t kBounceDampingBias = 2;
constexpr std::int32_t kGravityStep = 16;
constexpr std::int32_t kDampingScale = 16;

std::int16_t Damp(std::int16_t value, std::int16_t damping) {
  return static_cast<std::int16_t>(value - ((value * damping) / kDampingScale));
}

void DampVector(RoomEffectVector& vector, std::int16_t damping) {
  if (damping == 0) {
    return;
  }
  vector.x = Damp(vector.x, damping);
  vector.y = Damp(vector.y, damping);
  vector.z = Damp(vector.z, damping);
}

std::int16_t TurnAngle(std::int16_t angle, std::int16_t turn) {
  return static_cast<std::int16_t>(static_cast<std::uint32_t>(angle + turn) &
                                   kAngleMask);
}

// addVector on the position and the rotation, the rotation kept in a turn.
void MoveObject(RoomEffectObject& object) {
  object.position.x = WrappingAdd(object.position.x, object.velocity.x);
  object.position.y = WrappingAdd(object.position.y, object.velocity.y);
  object.position.z = WrappingAdd(object.position.z, object.velocity.z);
  object.rotation.x = TurnAngle(object.rotation.x, object.angular_velocity.x);
  object.rotation.y = TurnAngle(object.rotation.y, object.angular_velocity.y);
  object.rotation.z = TurnAngle(object.rotation.z, object.angular_velocity.z);
}

// Lands the object on the floor and throws it back up, slower.
void Bounce(RoomEffectObject& object, std::int32_t floor_height) {
  object.position.y = floor_height * kSubunitScale;
  const std::int32_t bounce_velocity =
      object.velocity.y /
      -(WrappingAbs(object.velocity_damping) + kBounceDampingBias);
  object.velocity.x = static_cast<std::int16_t>(object.velocity.x / 2);
  object.velocity.z = static_cast<std::int16_t>(object.velocity.z / 2);
  object.angular_velocity.x =
      static_cast<std::int16_t>(object.angular_velocity.x / 2);
  object.angular_velocity.y =
      static_cast<std::int16_t>(object.angular_velocity.y / 2);
  object.angular_velocity.z =
      static_cast<std::int16_t>(object.angular_velocity.z / 2);
  object.velocity.y = static_cast<std::int16_t>(bounce_velocity);
}

bool HasSound(const RoomEffectState& state, RoomEffectHost& host) {
  return state.sound_id != -1 && host.HasSoundBank();
}

constexpr std::int32_t kCentredPan = 0x80;
constexpr std::int32_t kSilentVolume = 0;
constexpr std::int32_t kFullVolume = 0x7F;
constexpr std::int32_t kDuckedVolume = 0x20;
constexpr std::int32_t kDuckFadeTicks = 8;
constexpr auto kSwingKind = std::to_underlying(RoomTextureEffectKind::kSwing);
}  // namespace

// decomp: BATTLE.PRG func_80092EDC (src/BATTLE/BATTLE.PRG/2842C.c:1142)
void WriteRoomEffectTransform(const RoomEffectObject& object,
                              std::span<std::byte> geometry) {
  const auto group = ObjectGroup(object, geometry);
  if (!group) {
    return;
  }
  const auto& position = object.position;
  const auto& rotation = object.rotation;
  const auto word = [&](std::size_t index, std::int32_t value) {
    SetGroupWord(geometry, *group, index, static_cast<std::uint32_t>(value));
  };
  word(room_effects::kGroupPositionWord, position.x / kSubunitScale);
  word(room_effects::kGroupPositionWord + 1, position.y / kSubunitScale);
  word(room_effects::kGroupPositionWord + 2, position.z / kSubunitScale);
  word(kRotationXWord, rotation.x);
  word(kRotationYWord, rotation.y);
  word(kRotationZWord, rotation.z);
}

// decomp: BATTLE.PRG func_80092F74 (src/BATTLE/BATTLE.PRG/2842C.c:1162)
RoomEffectCommandResult HideRoomEffectObject(const RoomEffectObject& object,
                                             std::span<std::byte> geometry) {
  const auto group = ObjectGroup(object, geometry);
  if (group) {
    SetGroupWord(geometry, *group, 0, room_effects::kGroupHiddenFlag);
  }
  return RoomEffectCommandResult::kAdvance;
}

// decomp: BATTLE.PRG func_80092F98 (src/BATTLE/BATTLE.PRG/2842C.c:1170)
RoomEffectCommandResult ExecuteRoomEffectShowGroup(
    const RoomEffectCommand& command, std::span<std::byte> geometry) {
  ShowRoomGeometryGroup(geometry, command.Word(kWordOperand));
  return RoomEffectCommandResult::kAdvance;
}

// decomp: BATTLE.PRG func_80092FBC (src/BATTLE/BATTLE.PRG/2842C.c:1176)
RoomEffectCommandResult AdvanceRoomEffectMotion(
    RoomEffectObject& object, const RoomEffectCommand& command,
    RoomEffectHost& host) {
  const std::uint16_t duration = command.Half(kFirstHalfOperand);
  const std::uint16_t motion = command.Half(kSecondHalfOperand);
  bool moving = false;
  if (motion == kGravityMotion) {
    moving = StepRoomEffectGravity(object, host);
  } else if (motion == kDampedMotion) {
    moving = StepRoomEffectDampedMotion(object, host.Geometry());
  }
  if (duration == kUntilStill) {
    return moving ? RoomEffectCommandResult::kWait
                  : RoomEffectCommandResult::kAdvance;
  }
  object.elapsed = WrappingAdd(object.elapsed, 1);
  if (object.elapsed >= duration) {
    object.elapsed = 0;
    return RoomEffectCommandResult::kAdvance;
  }
  return RoomEffectCommandResult::kWait;
}

// decomp: BATTLE.PRG func_8009306C (src/BATTLE/BATTLE.PRG/2842C.c:1201)
bool StepRoomEffectGravity(RoomEffectObject& object, RoomEffectHost& host) {
  MoveObject(object);
  const std::uint16_t floor_code = host.FloorHeightCode(
      object.position.x / kSubunitScale, object.position.z / kSubunitScale);
  // The low 15 bits hold the signed height.
  const std::int32_t floor_height =
      static_cast<std::int32_t>(static_cast<std::uint32_t>(floor_code)
                                << kFloorHeightShift) >>
      kFloorHeightShift;
  if (floor_height < object.position.y / kSubunitScale) {
    object.bounces = static_cast<std::int16_t>(object.bounces + 1);
    if (object.bounces >= kMaxBounces || object.velocity_damping <= 0) {
      object.bounces = 0;
      return false;
    }
    Bounce(object, floor_height);
  }
  if (object.velocity_damping != 0) {
    object.velocity.x = Damp(object.velocity.x, object.velocity_damping);
    object.velocity.z = Damp(object.velocity.z, object.velocity_damping);
  }
  object.velocity.y =
      static_cast<std::int16_t>(object.velocity.y + kGravityStep);
  DampVector(object.angular_velocity, object.angular_damping);
  WriteRoomEffectTransform(object, host.Geometry());
  return true;
}

// decomp: BATTLE.PRG func_80093364 (src/BATTLE/BATTLE.PRG/2842C.c:1247)
bool StepRoomEffectDampedMotion(RoomEffectObject& object,
                                std::span<std::byte> geometry) {
  MoveObject(object);
  DampVector(object.velocity, object.velocity_damping);
  DampVector(object.angular_velocity, object.angular_damping);
  WriteRoomEffectTransform(object, geometry);
  return false;
}

// decomp: BATTLE.PRG func_8009352C (src/BATTLE/BATTLE.PRG/2842C.c:1269)
RoomEffectCommandResult ExecuteRoomEffectPlaySound(
    const RoomEffectCommand& command, RoomEffectState& state,
    RoomEffectHost& host) {
  if (!host.HasSoundBank()) {
    return RoomEffectCommandResult::kAdvance;
  }
  const std::uint16_t sound = command.Half(kSecondHalfOperand);
  if (command.Half(kFirstHalfOperand) == 0 || state.sound_restored) {
    host.PlayCenteredSound(sound);
  } else {
    host.PlaySound(sound, kCentredPan, kSilentVolume);
  }
  state.sound_id = static_cast<std::int16_t>(sound);
  return RoomEffectCommandResult::kAdvance;
}

// decomp: BATTLE.PRG func_800935B8 (src/BATTLE/BATTLE.PRG/2842C.c:1283)
RoomEffectCommandResult ExecuteRoomEffectStopSound(
    const RoomEffectCommand& command, RoomEffectState& state,
    RoomEffectHost& host) {
  if (host.HasSoundBank()) {
    host.StopSound(command.Half(kSecondHalfOperand));
    state.sound_id = -1;
  }
  return RoomEffectCommandResult::kAdvance;
}

// decomp: BATTLE.PRG func_800935F8 (src/BATTLE/BATTLE.PRG/2842C.c:1292)
RoomEffectCommandResult ExecuteRoomEffectStartEffect(
    const RoomEffectCommand& command, RoomEffectState& state,
    RoomEffectHost& host) {
  StartRoomEffect(state, host, command.Word(kWordOperand));
  return RoomEffectCommandResult::kAdvance;
}

// decomp: BATTLE.PRG func_8009361C (src/BATTLE/BATTLE.PRG/2842C.c:1298)
RoomEffectCommandResult ExecuteRoomEffectHideGroup(
    const RoomEffectCommand& command, std::span<std::byte> geometry) {
  HideRoomGeometryGroup(geometry, command.Word(kWordOperand));
  return RoomEffectCommandResult::kAdvance;
}

// decomp: BATTLE.PRG func_80093640 (src/BATTLE/BATTLE.PRG/2842C.c:1304)
RoomEffectCommandResult ExecuteRoomEffectStopScript(
    const RoomEffectCommand& command, RoomEffectState& state,
    RoomEffectHost& host) {
  const std::int64_t slot =
      static_cast<std::int64_t>(command.Word(kWordOperand)) +
      static_cast<std::int64_t>(kRoomEffectScriptSlotBase);
  if (slot < static_cast<std::int64_t>(kRoomEffectScriptSlotBase) ||
      slot >= static_cast<std::int64_t>(kRoomEffectSlotCount)) {
    VS_LOG_WARN(kChannel,
                "Stopping effect script {} refused: it is not a script slot",
                command.Word(kWordOperand));
    return RoomEffectCommandResult::kAdvance;
  }
  RoomEffectInstance& instance =
      state.instances.at(static_cast<std::size_t>(slot));
  if (instance.active != 0) {
    ReleaseObjects(instance, host);
    instance.active = 0;
  }
  return RoomEffectCommandResult::kAdvance;
}

// decomp: BATTLE.PRG func_800936F8 (src/BATTLE/BATTLE.PRG/2842C.c:1321)
bool ToggleRoomEffectUpdates(RoomEffectState& state, int effect_index) {
  if (effect_index < 0) {
    VS_LOG_WARN(kChannel, "Toggling room effect {} refused: it is negative",
                effect_index);
    return false;
  }
  const auto index = static_cast<std::size_t>(effect_index);
  const bool texture = index < TextureSlotCount(state);
  const bool script =
      index >= kRoomEffectScriptSlotBase &&
      index < kRoomEffectScriptSlotBase + ScriptSlotCount(state);
  if (!texture && !script) {
    return false;
  }
  RoomEffectInstance& instance = state.instances.at(index);
  instance.update_enabled = instance.update_enabled == 0 ? 1 : 0;
  return true;
}

// decomp: BATTLE.PRG func_80093764 (src/BATTLE/BATTLE.PRG/2842C.c:1334)
std::uint8_t IsRoomEffectActive(const RoomEffectState& state,
                                int effect_index) {
  if (effect_index < 0 ||
      static_cast<std::size_t>(effect_index) >= kRoomEffectSlotCount) {
    VS_LOG_WARN(kChannel, "Room effect {} refused: there are {} slots",
                effect_index, kRoomEffectSlotCount);
    return 0;
  }
  return state.instances.at(static_cast<std::size_t>(effect_index)).active;
}

// decomp: BATTLE.PRG func_80093788 (src/BATTLE/BATTLE.PRG/2842C.c:1336)
void FinishRoomDoorRotation(RoomEffectState& state, RoomEffectHost& host,
                            int effect_index) {
  if (effect_index < 0) {
    VS_LOG_WARN(kChannel, "Door rotation {} refused: it is negative",
                effect_index);
    return;
  }
  const auto index = static_cast<std::size_t>(effect_index);
  if (index >= kRoomEffectScriptSlotBase) {
    return;
  }
  RoomEffectInstance& instance = state.instances.at(index);
  if (instance.kind_or_object_count != kSwingKind || instance.active == 0) {
    return;
  }
  if (index >= TextureSlotCount(state)) {
    VS_LOG_WARN(kChannel,
                "Door rotation {} refused: the room has {} texture effects",
                effect_index, state.texture_effect_count);
    return;
  }
  const bool finished = StepRecord<SwingEffect>(
      host.TextureEffects(), instance.data_offset, [&host](SwingEffect& swing) {
        const auto geometry = host.Geometry();
        const auto group =
            CheckedSwingGroup(geometry, swing.geometry_offset, "Door rotation");
        if (group) {
          WriteSwingAngle(geometry, *group, kSwingAxisY, swing.rest_angle);
        }
        ReverseSwing(swing);
        return true;
      });
  if (finished) {
    instance.active = 0;
  }
}

// decomp: BATTLE.PRG func_80093824 (src/BATTLE/BATTLE.PRG/2842C.c:1365)
void FinishRoomTiltRotation(const RoomEffectState& state, RoomEffectHost& host,
                            int effect_index) {
  if (effect_index < 0 ||
      static_cast<std::size_t>(effect_index) >= TextureSlotCount(state)) {
    VS_LOG_WARN(kChannel,
                "Tilt rotation {} refused: the room has {} texture effects",
                effect_index, state.texture_effect_count);
    return;
  }
  const RoomEffectInstance& instance =
      state.instances.at(static_cast<std::size_t>(effect_index));
  if (instance.kind_or_object_count != kSwingKind) {
    VS_LOG_WARN(kChannel, "Tilt rotation {} refused: it is a kind {} effect",
                effect_index, instance.kind_or_object_count);
    return;
  }
  StepRecord<SwingEffect>(
      host.TextureEffects(), instance.data_offset, [&host](SwingEffect& swing) {
        const auto geometry = host.Geometry();
        const auto group =
            CheckedSwingGroup(geometry, swing.geometry_offset, "Tilt rotation");
        if (group) {
          SetGroupWord(
              geometry, *group,
              swing.axis == kSwingAxisX ? kRotationXWord : kRotationZWord,
              static_cast<std::uint32_t>(swing.rest_angle));
        }
        ReverseSwing(swing);
        return true;
      });
}

// decomp: BATTLE.PRG func_800938AC (src/BATTLE/BATTLE.PRG/2842C.c:1389)
void DuckRoomEffectSound(const RoomEffectState& state, RoomEffectHost& host,
                         bool ducked) {
  if (HasSound(state, host)) {
    host.FadeSound(state.sound_id, ducked ? kDuckedVolume : kFullVolume,
                   kDuckFadeTicks);
  }
}

// decomp: BATTLE.PRG func_80093A14 (src/BATTLE/BATTLE.PRG/2842C.c:1448)
void RestoreRoomEffectSoundVolume(RoomEffectState& state,
                                  RoomEffectHost& host) {
  state.sound_restored = true;
  if (HasSound(state, host)) {
    host.FadeSound(state.sound_id, kFullVolume, 0);
  }
}

// decomp: BATTLE.PRG func_80093A70 (src/BATTLE/BATTLE.PRG/2842C.c:1456)
void RestartRoomEffectSound(const RoomEffectState& state,
                            RoomEffectHost& host) {
  if (HasSound(state, host)) {
    host.PlayCenteredSound(state.sound_id);
  }
}

// decomp: BATTLE.PRG func_80098B1C (src/BATTLE/BATTLE.PRG/2EA3C.s:1770): a
// tail jump to the code 0x14 bytes into section E.
bool CallRoomEntryPoint(RoomEffectHost& host) {
  const auto section = host.EntryPointSection();
  if (section.size() <= room_effects::kEntryPointHeaderBytes) {
    VS_LOG_WARN(kChannel,
                "Room entry point refused: section E has {} bytes and no code",
                section.size());
    return false;
  }
  return host.RunRoomEntryPoint(
      section.subspan(room_effects::kEntryPointHeaderBytes));
}
}  // namespace VS::Field
