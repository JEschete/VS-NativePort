#include "Save/NativeFilePresentation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Content/DataStore.hpp"
#include "Field/MinimapScene.h"

extern "C" {
#include "Battle/Feedback.h"
#include "Content/StringTables.h"
#include "Core/Text/TextCode.h"
#include "Field/MapProgress.h"
#include "Field/Mpd.h"
#include "Field/Znd.h"
#include "Menu/Files.h"
#include "Render/Font.h"
#include "Render/TextureAnimation.h"
#include "Save/MemoryCard/Resources.h"
#include "Title/SaveFiles.h"
}

namespace VS::Game::Save {
namespace {
using Bytes = std::span<const std::uint8_t>;
constexpr std::size_t kMaximumContentBytes = 4ull * 1024 * 1024;
constexpr std::size_t kRoomSectionCount = 24;
constexpr std::size_t kFloorTriggerSection = 8;
constexpr unsigned kSavePointKind = 16;
constexpr unsigned kArtworkCount = 48;
constexpr unsigned kLastScene = 31;
constexpr int kVisibleRows = 3;
constexpr int kCardLeft = 64;
constexpr int kCardTop = 72;
constexpr int kCardStride = 40;
constexpr int kCardWidth = 256;
constexpr int kCardHeight = 32;
constexpr int kGlyphExtent = 12;
constexpr int kNameRight = 170;
constexpr int kFooterTop = 209;
constexpr int kScreenWidth = 320;
constexpr int kGenericElementId = 255;
constexpr int kSelectedBlend = 8;
constexpr int kDimTextClut = 3;
constexpr unsigned kNegativeSpacingFloor = 0xF0;
constexpr int kByteValues = 256;
constexpr unsigned kDecimalRadix = 10;
constexpr unsigned kPercentScale = 100;
constexpr unsigned kFourDigitDivisor = 1000;
constexpr int kLastSlot = 99;
constexpr unsigned kMaximumStat = 999;
constexpr unsigned kMaximumSaveCount = 9999;
constexpr unsigned kMaximumClearCount = 99;
constexpr std::uint64_t kClockRadix = 60;
constexpr std::uint64_t kMaximumSeconds = 101 * kClockRadix * kClockRadix - 1;
constexpr unsigned kEligibleRooms = 361;
constexpr unsigned kGlyphUppercase = 10;
constexpr unsigned kGlyphLowercase = 36;
// Original font-table symbols, tools/etc/vsString.py: table[0xB0..0xB5].
// These belong to the file-menu font atlas, not MAINMENU's button texture page.
constexpr unsigned kCircleGlyph = 0xB0;
constexpr unsigned kCrossGlyph = 0xB1;
constexpr unsigned kUpGlyph = 0xB4;
constexpr unsigned kDownGlyph = 0xB5;

struct Point {
  int x;
  int y;
};

struct Extent {
  std::int16_t width;
  std::int16_t height;
};

struct SpriteAsset {
  int uv_clut;
  int size;
  int page;
};

using EncodedRoomName = decltype(NativeFileSummary::room_name);
constexpr int kCursorUv = vs_getUV0Clut(32, 48, 896, 223);
constexpr int kCursorSize = vs_getWH(16, 16);
constexpr int kCursorPage = getTPage(0, 0, 768, 0);
constexpr SpriteAsset kCursorSprite{kCursorUv, kCursorSize, kCursorPage};
constexpr Point kPalettePosition{768, 227};
constexpr int kPaletteSize = vs_getWH(256, 1);
constexpr std::size_t kPaletteColors = 256;
constexpr int kArtworkSize = vs_getWH(64, 32);

Point AtRow(Point position, int top) { return {position.x, position.y + top}; }

std::expected<std::vector<std::uint8_t>, std::string> ReadAsset(
    const Content::DataStore& store, std::string_view path) {
  const auto* entry = store.Find(path);
  if (entry == nullptr) {
    return std::unexpected(std::format("save summary: missing {}", path));
  }
  if (entry->form != "raw" || entry->size > kMaximumContentBytes) {
    return std::unexpected(std::format("save summary: unsupported {}", path));
  }
  return store.ReadAll(*entry);
}

template <typename T>
std::expected<T, std::string> Record(Bytes bytes, std::size_t offset = 0) {
  if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
    return std::unexpected("save summary: truncated content record");
  }
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

std::expected<Bytes, std::string> Slice(Bytes bytes, std::size_t offset,
                                        std::size_t size) {
  if (offset > bytes.size() || size > bytes.size() - offset) {
    return std::unexpected("save summary: content section exceeds file");
  }
  return bytes.subspan(offset, size);
}

bool IsRoomName(std::span<const char> name) {
  bool visible = false;
  for (std::size_t index = 0; index < name.size(); ++index) {
    const auto glyph = static_cast<unsigned char>(name[index]);
    if (glyph == Core_TextCode_Terminator) return visible;
    if (glyph == Core_TextCode_Spacing || glyph == Core_TextCode_ChunkSize ||
        glyph == Core_TextCode_FontTable) {
      if (++index == name.size()) return false;
    } else if (glyph >= Core_TextCode_NonPrinting) {
      return false;
    } else if (glyph != Core_TextCode_Space) {
      visible = true;
    }
  }
  return false;
}

std::expected<EncodedRoomName, std::string> RoomName(
    const Content::DataStore& store, const Location& location) {
  if (location.scene > kLastScene) {
    return std::unexpected("save summary: unsupported map scene");
  }
  const auto path = location.scene == 0
                        ? std::string("SMALL/DUMMY.ARM")
                        : std::format("SMALL/SCEN{:03}.ARM", location.scene);
  auto bytes = ReadAsset(store, path);
  if (!bytes) return std::unexpected(bytes.error());
  const auto count = Record<std::uint32_t>(*bytes);
  if (!count) return std::unexpected(count.error());

  // F00774 vs_battle_initSceneAndGetRoomNames, BATTLE/2EA3C.s:2085-2103;
  // MENU5/4D8.c consumes the resulting ARM names by matching zone/map records.
  Field_MapRoomName* names = nullptr;
  try {
    names = Field_ResolveSceneRoomData(bytes->data(), bytes->size());
  } catch (const std::runtime_error& error) {
    return std::unexpected(
        std::format("save summary: {}: {}", path, error.what()));
  }
  const auto names_offset =
      reinterpret_cast<const std::uint8_t*>(names) - bytes->data();
  const auto table =
      Slice(*bytes, static_cast<std::size_t>(names_offset),
            static_cast<std::size_t>(*count) * sizeof(Field_MapRoomName));
  if (!table) return std::unexpected(table.error());
  EncodedRoomName name{};
  bool found = false;
  for (std::uint32_t index = 0; index < *count; ++index) {
    const auto room = Record<Field_MapRoom>(
        *bytes, sizeof(*count) + index * sizeof(Field_MapRoom));
    if (!room) return std::unexpected(room.error());
    if (room->zoneId != location.zone ||
        room->mapId != static_cast<int>(location.room) + 1) {
      continue;
    }
    if (found) return std::unexpected("save summary: ambiguous ARM room");
    const auto entry =
        Record<Field_MapRoomName>(*table, index * sizeof(Field_MapRoomName));
    if (!entry) return std::unexpected(entry.error());
    std::copy_n(entry->name, name.size(), name.begin());
    found = true;
  }
  if (!found) return std::unexpected("save summary: room absent from ARM");
  if (!IsRoomName(name)) {
    return std::unexpected("save summary: invalid encoded ARM room name");
  }
  return name;
}

std::expected<std::vector<std::uint8_t>, std::string> RoomAsset(
    const Content::DataStore& store, const Location& location) {
  const auto zone =
      ReadAsset(store, std::format("MAP/ZONE{:03}.ZND", location.zone));
  if (!zone) return std::unexpected(zone.error());
  const auto zone_header = Record<Field_ZndHeader>(*zone);
  if (!zone_header) return std::unexpected(zone_header.error());
  if (zone_header->mpdListOffset < 0 || zone_header->mpdListSize < 0) {
    return std::unexpected("save summary: invalid zone room table");
  }
  const auto rooms =
      Slice(*zone, zone_header->mpdListOffset, zone_header->mpdListSize);
  if (!rooms) return std::unexpected(rooms.error());
  using DiscFile = std::array<std::uint32_t, 2>;
  if (rooms->size() % sizeof(DiscFile) != 0) {
    return std::unexpected("save summary: invalid zone room stride");
  }
  const auto file = Record<DiscFile>(*rooms, location.room * sizeof(DiscFile));
  if (!file) return std::unexpected(file.error());
  const auto* entry = store.FindBySector((*file)[0]);
  if (entry == nullptr || entry->lba != (*file)[0] ||
      !entry->path.ends_with(".MPD")) {
    return std::unexpected("save summary: room does not identify an MPD");
  }
  const auto allocation = static_cast<std::uint64_t>(entry->SectorCount()) *
                          Content::DataStore::kDataSectorBytes;
  if ((*file)[1] == 0 ||
      ((*file)[1] != entry->size && (*file)[1] != allocation)) {
    return std::unexpected("save summary: invalid MPD request size");
  }
  return ReadAsset(store, entry->path);
}

std::expected<int, std::string> LocationArtwork(const Content::DataStore& store,
                                                const Location& location) {
  const auto mpd = RoomAsset(store, location);
  if (!mpd) return std::unexpected(mpd.error());
  const auto header = Record<Field_MpdHeader>(*mpd);
  if (!header) return std::unexpected(header.error());
  if (header->roomSectionOffset < 0) {
    return std::unexpected("save summary: invalid MPD room section");
  }
  using Lengths = std::array<std::uint32_t, kRoomSectionCount>;
  const auto lengths = Record<Lengths>(*mpd, header->roomSectionOffset);
  if (!lengths) return std::unexpected(lengths.error());
  std::size_t cursor =
      static_cast<std::size_t>(header->roomSectionOffset) + sizeof(Lengths);
  Bytes triggers;
  for (std::size_t section = 0; section < lengths->size(); ++section) {
    const auto data = Slice(*mpd, cursor, (*lengths)[section]);
    if (!data) return std::unexpected(data.error());
    if (section == kFloorTriggerSection) triggers = *data;
    cursor += data->size();
  }
  if (triggers.size() % sizeof(Field_MpdFloorTrigger) != 0) {
    return std::unexpected("save summary: invalid floor-trigger stride");
  }
  int artwork = -1;
  // F00547 Field_CurrentSavePointId, BATTLE/146C.c:15530: kind16 activation is
  // the original saveLocations/artwork ID. F02756/F03623 subtract one; chest
  // IDs and arbitrary rooms have no image.
  for (std::size_t offset = 0; offset < triggers.size();
       offset += sizeof(Field_MpdFloorTrigger)) {
    const auto trigger = Record<Field_MpdFloorTrigger>(triggers, offset);
    if (!trigger) return std::unexpected(trigger.error());
    if (trigger->kind != kSavePointKind) continue;
    const auto id = static_cast<unsigned char>(trigger->activation);
    if (id == 0 || id > kArtworkCount) continue;
    const int candidate = id - 1;
    if (artwork != -1 && artwork != candidate) return -1;
    artwork = candidate;
  }
  return artwork;
}

class Frontend {
 public:
  explicit Frontend(bool title) : title_(title) {}

  int Glyph(unsigned glyph, Point position, int clut = 0) const {
    return title_
               ? Title_SaveFiles_DrawGlyph(glyph, position.x, position.y, clut)
               : Menu_Files_DrawGlyph(glyph, position.x, position.y, clut);
  }

  void Sprite(Point position, SpriteAsset asset) const {
    const int xy = vs_getXY(position.x, position.y);
    if (title_) {
      Title_SaveFiles_DrawImmediateSprite(xy, asset.uv_clut, asset.size,
                                          asset.page);
    } else {
      Render_DrawImmediateSprite(xy, asset.uv_clut, asset.size, asset.page);
    }
  }

  void Label(Point position, Menu_Files_FileMenuUiId label) const {
    const int xy = vs_getXY(position.x, position.y);
    if (title_) {
      Title_SaveFiles_DrawSummaryLabel(xy, label);
    } else {
      Menu_Files_DrawSummaryLabel(xy, label);
    }
  }

  void HpMp(Point position, Menu_Files_StatType stat, int current,
            int maximum) const {
    const unsigned limit =
        std::clamp(maximum, 0, static_cast<int>(kMaximumStat));
    const unsigned value = std::clamp(current, 0, static_cast<int>(limit));
    const int xy = vs_getXY(position.x, position.y);
    if (title_) {
      Title_SaveFiles_DrawHpMpSummary(xy, stat, value, limit);
    } else {
      Menu_Files_DrawHpMpSummary(xy, stat, value, limit);
    }
  }

  void Panel(Point position, Extent size, bool selected,
             bool breadcrumb = false) const {
    Menu_Files_FileMenuElement element{};
    element.state = Menu_Files_ElementState_Static;
    // Original C consumers use /J; 255 takes the generic-background path,
    // without manufacturing Save_CardFileInfoRecord data or file headers.
    element.slotId = static_cast<char>(kGenericElementId);
    element.text[0] = static_cast<char>(kGenericElementId);
    element.x = static_cast<std::int16_t>(position.x);
    element.y = static_cast<std::int16_t>(position.y);
    element.w = size.width;
    element.h = size.height;
    element.selected = static_cast<char>(selected);
    element.innertextBlendFactor = breadcrumb ? kSelectedBlend : 0;
    if (title_) {
      Title_SaveFiles_DrawElement(&element);
    } else {
      Menu_Files_DrawElement(&element);
    }
  }

  void Encoded(std::span<const char> text, Point position, int clut = 0,
               int right = kScreenWidth) const {
    for (std::size_t index = 0; index < text.size(); ++index) {
      const auto glyph = static_cast<unsigned char>(text[index]);
      if (glyph == Core_TextCode_Terminator ||
          glyph == Core_TextCode_PrintString) {
        return;
      }
      if (glyph == Core_TextCode_Spacing) {
        if (++index == text.size()) return;
        const auto spacing = static_cast<unsigned char>(text[index]);
        position.x += spacing >= kNegativeSpacingFloor
                          ? static_cast<int>(spacing) - kByteValues
                          : spacing;
      } else if (glyph == Core_TextCode_ChunkSize ||
                 glyph == Core_TextCode_FontTable) {
        if (++index == text.size()) return;
      } else if (glyph < Core_TextCode_NonPrinting) {
        if (position.x + kGlyphExtent > right) return;
        position.x = Glyph(glyph, position, clut);
      } else {
        return;
      }
    }
  }

  void Ascii(std::string_view text, Point position, int clut = 0) const {
    for (const unsigned char character : text) {
      unsigned glyph = Core_TextCode_Space;
      if (character >= 'A' && character <= 'Z') {
        glyph = character - 'A' + kGlyphUppercase;
      } else if (character >= 'a' && character <= 'z') {
        glyph = character - 'a' + kGlyphLowercase;
      }
      position.x = Glyph(glyph, position, clut);
    }
  }

 private:
  bool title_;
};

std::span<const char> OriginalText(std::size_t offset) {
  const auto& table = Save_MemoryCard_uiResources->textTable;
  if (offset >= std::size(table)) return {};
  return {reinterpret_cast<const char*>(table + offset),
          (std::size(table) - offset) * sizeof(table[0])};
}

void Integer(Point position, unsigned value, unsigned divisor) {
  Menu_Files_DrawInteger(vs_getXY(position.x, position.y), value, divisor);
}

unsigned MapPercent(const State_RoomVisitFlags& visits) {
  unsigned rooms = 0;
  // F00630 vs_battle_getMapCompletion, BATTLE/146C.c:17367. Mirror the
  // special room bit in a local value, not the live or saved visit flags.
  constexpr unsigned kMirrorWord = 1;
  constexpr unsigned kSourceRoom = 0x800000;
  constexpr unsigned kMirroredRoom = 0x400000;
  for (std::size_t index = 0; index < std::size(visits.roomFlags); ++index) {
    auto flags = static_cast<unsigned>(visits.roomFlags[index]);
    if (index == kMirrorWord && (flags & kSourceRoom)) flags |= kMirroredRoom;
    rooms += std::popcount(
        flags & static_cast<unsigned>(Field_countableRoomMask[index]));
  }
  return rooms * kPercentScale / kEligibleRooms;
}

void Artwork(const Frontend& frontend, int artwork, int y, bool selected) {
  constexpr int kFirstPaletteImages = 32;
  constexpr int kDimmingShift = 19;
  constexpr int kImageRowShift = 13;
  constexpr int kPlaceholderPage = 0x9C;
  constexpr int kImagePage = 0x90;
  constexpr int kImageColumnBit = 8;
  constexpr int kImageRowMask = 7;
  constexpr int kImageBankMask = 0x30;
  constexpr int kBankStride = 4;
  constexpr int kTextureBase = 832;
  constexpr int kVramXMask = 0x3FF;
  constexpr int kPageShift = 6;
  constexpr int kPlaceholderUv = vs_getUV0Clut(64, 0, 768, 227);
  constexpr int kImageUv = vs_getUV0Clut(0, 0, 768, 227);
  const int palette_xy = vs_getXY(kPalettePosition.x, kPalettePosition.y);
  std::array<std::uint16_t, kPaletteColors> previous{};
  Render_ReadPackedRectangle(palette_xy, previous.data(), kPaletteSize);
  const unsigned palette =
      artwork >= 0 && artwork < kFirstPaletteImages ? 0 : 1;
  Render_UploadPackedRectangle(
      palette_xy, Save_MemoryCard_uiResources->locationPalettes[palette],
      kPaletteSize);
  const int dimming = (selected ? 0 : kSelectedBlend) << kDimmingShift;
  if (artwork < 0) {
    frontend.Sprite({0, y}, {(~artwork << kImageRowShift) | kPlaceholderUv,
                             kArtworkSize, dimming | kPlaceholderPage});
  } else {
    const int uv = ((artwork & kImageColumnBit) * kImageColumnBit) |
                   ((artwork & kImageRowMask) << kImageRowShift) | kImageUv;
    const int page =
        ((((artwork & kImageBankMask) * kBankStride) + kTextureBase) &
         kVramXMask) >>
        kPageShift;
    frontend.Sprite({0, y}, {uv, kArtworkSize, dimming | kImagePage | page});
  }
  Render_UploadPackedRectangle(palette_xy, previous.data(), kPaletteSize);
}

void Stats(const Frontend& frontend, const Snapshot& state, int top) {
  constexpr Point kMap{172, 4}, kMapColon{189, 4}, kMapDigits{192, 4};
  constexpr Point kPercent{202, 3}, kHundredPercent{207, 3};
  constexpr Point kSave{217, 4}, kSaveColon{239, 4}, kSaveDigits{242, 4};
  constexpr Point kClear{267, 4}, kClearColon{293, 4}, kClearDigits{296, 4};
  constexpr Point kHp{88, 17}, kMp{158, 17}, kClearIcon{69, 17};
  constexpr Point kTime{240, 17}, kHours{268, 17}, kHundredHours{263, 17};
  constexpr Point kHourColon{279, 17}, kMinuteColon{293, 17};
  constexpr Point kMinutes{282, 17}, kSeconds{296, 17};
  constexpr SpriteAsset kClearSprite{vs_getUV0Clut(240, 16, 912, 223),
                                     vs_getWH(16, 16), getTPage(0, 0, 768, 0)};
  const unsigned percent = MapPercent(state.visits);
  frontend.Label(AtRow(kMap, top), Menu_Files_FileMenuUiId_Map);
  frontend.Label(AtRow(kMapColon, top), Menu_Files_FileMenuUiId_Colon);
  Integer(AtRow(kMapDigits, top), percent,
          percent == kPercentScale ? kPercentScale : kDecimalRadix);
  frontend.Label(
      AtRow(percent == kPercentScale ? kHundredPercent : kPercent, top),
      Menu_Files_FileMenuUiId_Percent);
  frontend.Label(AtRow(kSave, top), Menu_Files_FileMenuUiId_Save);
  frontend.Label(AtRow(kSaveColon, top), Menu_Files_FileMenuUiId_Colon);
  Integer(
      AtRow(kSaveDigits, top),
      std::min<unsigned>(state.settings.cumulativeSaveCount, kMaximumSaveCount),
      kFourDigitDivisor);
  frontend.Label(AtRow(kClear, top), Menu_Files_FileMenuUiId_Clear);
  frontend.Label(AtRow(kClearColon, top), Menu_Files_FileMenuUiId_Colon);
  // TITLE 22C.c:700 uses the current clear count, not the fastest-run record.
  const unsigned clears = state.script_flags[0];
  Integer(AtRow(kClearDigits, top), std::min(clears, kMaximumClearCount),
          kDecimalRadix);
  if (clears != 0) {
    frontend.Sprite(AtRow(kClearIcon, top), kClearSprite);
  }
  frontend.HpMp(AtRow(kHp, top), Menu_Files_StatType_HP, state.ashley.currentHp,
                state.ashley.maxHp);
  frontend.HpMp(AtRow(kMp, top), Menu_Files_StatType_MP, state.ashley.currentMp,
                state.ashley.maxMp);
  const auto seconds =
      std::min(state.play_ticks / kClockRadix, kMaximumSeconds);
  const unsigned hours =
      static_cast<unsigned>(seconds / (kClockRadix * kClockRadix));
  frontend.Label(AtRow(kTime, top), Menu_Files_FileMenuUiId_Time);
  Integer(AtRow(hours == kPercentScale ? kHundredHours : kHours, top), hours,
          hours == kPercentScale ? kPercentScale : kDecimalRadix);
  frontend.Label(AtRow(kHourColon, top), Menu_Files_FileMenuUiId_Colon);
  Integer(AtRow(kMinutes, top),
          static_cast<unsigned>(seconds / kClockRadix % kClockRadix),
          kDecimalRadix);
  frontend.Label(AtRow(kMinuteColon, top), Menu_Files_FileMenuUiId_Colon);
  Integer(AtRow(kSeconds, top), static_cast<unsigned>(seconds % kClockRadix),
          kDecimalRadix);
}

void Card(const Frontend& frontend, const NativeFileScreen& screen,
          std::size_t row) {
  constexpr Point kEmptyText{70, 10}, kInvalidText{94, 10};
  constexpr Point kUnavailableText{82, 10}, kRoomName{70, 4};
  constexpr Point kRecovered{4, 20}, kCursor{20, -8};
  constexpr int kNewArtwork = -1;
  constexpr int kEmptyArtwork = -5;
  constexpr int kInvalidArtwork = -3;
  constexpr int kNumberX = 42, kHundredNumberX = 37;
  constexpr int kNumberDigitsX = 55, kHundredDigitsX = 50;
  const auto& file = screen.files[row];
  const int y = kCardTop + static_cast<int>(row) * kCardStride;
  const bool selected = static_cast<int>(row) == screen.selected_row;
  frontend.Panel({kCardLeft, y}, {kCardWidth, kCardHeight}, selected);
  if (file.status == FileStatus::kEmpty) {
    Artwork(frontend, screen.save ? kNewArtwork : kEmptyArtwork, y, selected);
    frontend.Encoded(
        OriginalText(screen.save ? Text_Menu_MCMAN_BIN_Offset_new
                                 : Text_Menu_MCMAN_BIN_Offset_empty),
        AtRow(kEmptyText, y), screen.save ? 0 : kDimTextClut);
  } else if (file.status == FileStatus::kInvalid) {
    Artwork(frontend, kInvalidArtwork, y, selected);
    frontend.Ascii("Invalid save data", AtRow(kInvalidText, y));
  } else if (!file.document || row >= screen.summaries.size() ||
             !IsRoomName(screen.summaries[row].room_name)) {
    frontend.Ascii("Summary unavailable", AtRow(kUnavailableText, y));
  } else {
    const auto& summary = screen.summaries[row];
    if (summary.location_artwork >= 0 &&
        summary.location_artwork < static_cast<int>(kArtworkCount)) {
      Artwork(frontend, summary.location_artwork, y, selected);
    }
    frontend.Encoded(summary.room_name, AtRow(kRoomName, y), 0, kNameRight);
    Stats(frontend, file.document->state, y);
    if (file.status == FileStatus::kRecovered) {
      frontend.Ascii("Recovered", AtRow(kRecovered, y));
    }
  }
  const unsigned number =
      static_cast<unsigned>(std::clamp(file.slot, 0, kLastSlot)) + 1;
  frontend.Label({number == kPercentScale ? kHundredNumberX : kNumberX, y},
                 Menu_Files_FileMenuUiId_Number);
  Integer({number == kPercentScale ? kHundredDigitsX : kNumberDigitsX, y},
          number, number == kPercentScale ? kPercentScale : kDecimalRadix);
  if (selected) {
    frontend.Sprite(AtRow(kCursor, y), kCursorSprite);
  }
}

void Controls(const Frontend& frontend) {
  constexpr Point kUp{16, kFooterTop}, kDown{28, kFooterTop};
  constexpr Point kSelect{42, kFooterTop}, kConfirm{114, kFooterTop};
  constexpr Point kCircle{100, kFooterTop}, kCross{188, kFooterTop};
  constexpr Point kBack{202, kFooterTop};
  frontend.Glyph(kUpGlyph, kUp);
  frontend.Glyph(kDownGlyph, kDown);
  frontend.Ascii("Select", kSelect);
  frontend.Glyph(kCircleGlyph, kCircle);
  frontend.Ascii("Confirm", kConfirm);
  frontend.Glyph(kCrossGlyph, kCross);
  frontend.Ascii("Back", kBack);
}

void Confirmation(const Frontend& frontend, bool affirmative) {
  constexpr int kTop = 18, kStride = 16, kTextX = 6, kCursorYOffset = -8;
  constexpr Extent kChoiceSize{126, 12};
  // F02759 _promptConfirm, MENU7/260.c:2092; F03629, TITLE/22C.c:2330.
  // Retain the original vertically stacked choices and large selection arrow.
  for (int row = 0; row < 2; ++row) {
    const bool selected = affirmative == (row == 0);
    const int y = kTop + row * kStride;
    frontend.Panel({0, y}, kChoiceSize, selected);
    frontend.Encoded(
        OriginalText(row == 0 ? Text_Menu_MCMAN_BIN_Offset_yesIndent
                              : Text_Menu_MCMAN_BIN_Offset_noIndent),
        {kTextX, y});
    if (selected) {
      frontend.Sprite({0, y + kCursorYOffset}, kCursorSprite);
    }
  }
}
}  // namespace

std::expected<NativeFileSummary, std::string> DescribeNativeFile(
    const Document& document) {
  if (document.new_game_entry) {
    NativeFileSummary summary;
    constexpr std::string_view kClearName = "CLEAR GAME";
    for (std::size_t index = 0; index < kClearName.size(); ++index) {
      const auto c = kClearName[index];
      summary.room_name[index] = static_cast<char>(
          c == ' ' ? Core_TextCode_Space : c - 'A' + kGlyphUppercase);
    }
    summary.room_name[kClearName.size()] =
        static_cast<char>(Core_TextCode_Terminator);
    // F03601 sets saveLocation=0x30, outside the ordinary save-point artwork.
    summary.location_artwork = -1;
    return summary;
  }
  const auto* store = Content::DataStore::Get();
  if (store == nullptr) {
    return std::unexpected("save summary: extracted content is not open");
  }
  const auto name = RoomName(*store, document.state.location);
  if (!name) return std::unexpected(name.error());
  const auto artwork = LocationArtwork(*store, document.state.location);
  if (!artwork) return std::unexpected(artwork.error());
  return NativeFileSummary{*name, *artwork};
}

void DrawNativeFileScreen(const NativeFileScreen& screen) {
  constexpr Point kOperationHeader{180, 34}, kLocalHeader{180, 50};
  constexpr Point kOperationText{186, 34}, kLocalText{186, 50};
  constexpr Extent kHeaderSize{140, 12};
  if (Title_memoryCardUiResource == nullptr ||
      Save_MemoryCard_uiResources == nullptr ||
      (screen.title ? Title_saveMenuText : Menu_Files_textTable) == nullptr) {
    return;
  }
  const Frontend frontend(screen.title);
  // F02756/F03623 _drawFileMenuElement: original gradient, 64x32 artwork,
  // NO.xx, label/digit sprites and HP/MP bars; no PS1 card-summary backend.
  frontend.Panel(kOperationHeader, kHeaderSize, false, true);
  frontend.Encoded(OriginalText(screen.save ? Text_Menu_MCMAN_BIN_Offset_save
                                            : Text_Menu_MCMAN_BIN_Offset_load),
                   kOperationText);
  frontend.Panel(kLocalHeader, kHeaderSize, false, true);
  frontend.Ascii("LOCAL SAVES", kLocalText);
  const auto count =
      std::min(screen.files.size(), static_cast<std::size_t>(kVisibleRows));
  for (std::size_t row = 0; row < count; ++row) Card(frontend, screen, row);
  Controls(frontend);
  if (screen.confirmation) Confirmation(frontend, screen.affirmative);
}

}  // namespace VS::Game::Save
