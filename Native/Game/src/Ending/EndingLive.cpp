// Hand-maintained (tools/port/owned.csv).
// Live binding for ENDING.PRG's staff roll (Credits/StaffRollCore.cpp) and
// clear results (ClearResultsCore.cpp): the frame's primitive buffer and
// ordering tables, the display environments, the GTE projection, the XA
// stream, the disc reads into the high end of the game heap, the progress
// records a clear commits and the module image's tables. The cores' state is
// published to the module globals at every frame yield, where the session and
// test harnesses read it.
#include "Ending/EndingLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>

#include "Audio/SfxLive.hpp"
#include "Audio/StreamLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Ending/ClearResultsCore.hpp"
#include "Ending/Credits/EmbeddedArt.h"
#include "Ending/Credits/StaffRollCore.hpp"
#include "Input/ControllerLive.hpp"
#include "Legacy/Psx.h"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/GameClockLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Ending/ClearResults.h"
#include "Ending/Credits.h"
#include "Ending/Credits/StaffRoll.h"
#include "Ending/Results.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Input/ControllerLiveState.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Images.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Location.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/StateFlags.h"
}

namespace VS::Ending {
namespace {
constexpr const char* kChannel = "ending";
constexpr std::size_t kFrameBufferCount = 2;
// AllocateRenderBuffers' 0x2088-byte ordering tables.
constexpr std::int64_t kOrderingTableEntries = 0x822;
// Region quads sit seven ordering-table words before the scene table
// (renderTextures.h: `p[1] - 0x1C`).
constexpr int kRegionQuadSlot = -7;
// The writable TIM each embedded art slot holds (EmbeddedArt.cpp's
// kTimCapacity, the original module's span for the illustration workspace).
constexpr std::size_t kEmbeddedTimBytes = 72224;
constexpr int kEndingMusicSector = VS_ENDING_XA_LBA;
constexpr int kSoundGroup = 0x7E;
// RotTransPers4 packs a screen point as x in the low halfword, y in the high.
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr unsigned kHalfwordShift = 16;
// The playthrough flags after the clear count, difficulty and normal-mode
// clear's block are cleared by a clear.
constexpr std::size_t kStateFlagsBytes = 0x440;
constexpr std::size_t kRetainedFlagBytes = 0x10;
static_assert(sizeof(State_StateFlags) == kStateFlagsBytes);
static_assert(sizeof(Ending_Credits_rippleDistanceGrid) ==
              Credits::kRippleRows * Credits::kRippleColumns);
static_assert(std::size(Ending_Credits_illustrationHoldFrames) ==
              Credits::kIllustrationCount);
static_assert(std::size(Ending_Credits_illustrationResources) ==
              Credits::kIllustrationCount);

constexpr std::array<Content_DiscFile, kResultsFileCount> kResultsFiles{{
    {VS_END_DIS_LBA, VS_END_DIS_SIZE},
    {VS_ENDSCR00_DIS_LBA, VS_ENDSCR00_DIS_SIZE},
    {VS_ENDSCR01_DIS_LBA, VS_ENDSCR01_DIS_SIZE},
    {VS_ENDSCR02_DIS_LBA, VS_ENDSCR02_DIS_SIZE},
    {VS_ENDSCR03_DIS_LBA, VS_ENDSCR03_DIS_SIZE},
    {VS_ENDSCR04_DIS_LBA, VS_ENDSCR04_DIS_SIZE},
    {VS_ENDSCR05_DIS_LBA, VS_ENDSCR05_DIS_SIZE},
    {VS_ENDSCR06_DIS_LBA, VS_ENDSCR06_DIS_SIZE},
    {VS_ENDSCR07_DIS_LBA, VS_ENDSCR07_DIS_SIZE},
    {VS_ENDSCR08_DIS_LBA, VS_ENDSCR08_DIS_SIZE},
    {VS_ENDSCR09_DIS_LBA, VS_ENDSCR09_DIS_SIZE},
    {VS_ENDSCR10_DIS_LBA, VS_ENDSCR10_DIS_SIZE},
    {VS_ENDSCR11_DIS_LBA, VS_ENDSCR11_DIS_SIZE},
    {VS_ENDSCR12_DIS_LBA, VS_ENDSCR12_DIS_SIZE},
    {VS_ENDSCR13_DIS_LBA, VS_ENDSCR13_DIS_SIZE},
    {VS_ENDSCR14_DIS_LBA, VS_ENDSCR14_DIS_SIZE},
    {VS_ENDSCR15_DIS_LBA, VS_ENDSCR15_DIS_SIZE},
}};

std::size_t BufferIndex(int buffer) {
  return static_cast<std::size_t>(buffer & 1);
}

// Links the packet in front of the scene ordering slot (addPrim) in the
// frame's primitive buffer; a full buffer or a slot outside the table drops
// it (LivePacketQueue counts the drop).
template <typename Packet>
void QueuePacket(const Packet& packet, int ordering_slot) {
  static_assert(sizeof(Packet) % sizeof(std::uint32_t) == 0);
  std::array<std::uint32_t, sizeof(Packet) / sizeof(std::uint32_t)> words{};
  std::memcpy(words.data(), &packet, sizeof packet);
  Render::LivePacketQueue queue(Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene, ordering_slot));
  words.front() = (words.front() & ~Render::kPacketLinkMask) |
                  (queue.ChainLink() & Render::kPacketLinkMask);
  queue.Emit(words);
}

template <typename Packet>
void SetCorners(Packet& packet,
                const std::array<ScreenPoint, kQuadCorners>& corners) {
  setXY4(&packet, corners[0].x, corners[0].y, corners[1].x, corners[1].y,
         corners[2].x, corners[2].y, corners[3].x, corners[3].y);
}

template <typename Packet>
void SetTexels(Packet& packet,
               const std::array<TexelPoint, kQuadCorners>& texels) {
  setUV4(&packet, texels[0].u, texels[0].v, texels[1].u, texels[1].v,
         texels[2].u, texels[2].v, texels[3].u, texels[3].v);
}

// Entry `slot` from the scene table BeginFrame set, in whichever frame's
// table holds it: PresentFrame draws the table after flipping the frame
// buffer (D4.c:1480 takes the address before the flip).
u_long* SceneOrderingEntry(int slot) {
  const auto head =
      reinterpret_cast<std::uintptr_t>(Render_frameScratch.orderingTable);
  for (void* const frame_table : Render_legacyOrderingTables) {
    const auto table = reinterpret_cast<std::uintptr_t>(frame_table);
    if (table == 0 || head < table || (head - table) % sizeof(u_long) != 0) {
      continue;
    }
    const auto entry =
        static_cast<std::int64_t>((head - table) / sizeof(u_long)) + slot;
    if (entry >= 0 && entry < kOrderingTableEntries) {
      return std::span(static_cast<u_long*>(frame_table),
                       static_cast<std::size_t>(kOrderingTableEntries))
          .subspan(static_cast<std::size_t>(entry))
          .data();
    }
  }
  return nullptr;
}

RECT ToRect(const Render::ScreenRect& area) {
  return {area.x, area.y, area.width, area.height};
}

// One file read into a high-end heap buffer through the disc queue. A read
// the heap cannot hold is refused; a request the drive fails completes
// without data.
class LiveDiscRead {
 public:
  LiveDiscRead() = default;
  LiveDiscRead(const LiveDiscRead&) = delete;
  LiveDiscRead& operator=(const LiveDiscRead&) = delete;
  LiveDiscRead(LiveDiscRead&&) = delete;
  LiveDiscRead& operator=(LiveDiscRead&&) = delete;
  ~LiveDiscRead() { End(); }

  // Allocates the file's buffer and queues its read; returns the buffer, or
  // null when the heap refused it.
  void* Begin(const Content_DiscFile& file, bool priority) {
    End();
    file_ = file;
    loaded_ = false;
    void* buffer = Core::Memory::AllocateLiveFromHighEnd(
        static_cast<std::size_t>(std::max(file.size, 0)));
    if (buffer == nullptr) {
      VS_LOG_WARN(kChannel,
                  "Disc read of sector {} refused: no {}-byte heap buffer",
                  file.lba, file.size);
      return nullptr;
    }
    slot_ = Content::AllocateLiveDiscRequest(&file_);
    if (slot_ == nullptr) {
      slot_ = Content::CompleteLiveRefusedDiscRequest(&file_, buffer);
    } else if (priority) {
      Content::EnqueueLivePriorityDiscRequest(slot_, buffer);
    } else {
      Content::EnqueueLiveDiscRequest(slot_, buffer);
    }
    return buffer;
  }

  // True once the read has loaded or failed, or when none was queued.
  [[nodiscard]] bool IsComplete() {
    if (slot_ == nullptr) return true;
    if (slot_->state == Content_DiscRequestState_Loaded) {
      loaded_ = true;
      return true;
    }
    if (slot_->state == Content_DiscRequestState_Error) {
      VS_LOG_WARN(kChannel, "Disc read of sector {} failed; it has no data",
                  file_.lba);
      return true;
    }
    return false;
  }

  [[nodiscard]] bool IsLoaded() const { return loaded_; }
  [[nodiscard]] Content_DiscQueueSlot* Slot() const { return slot_; }
  [[nodiscard]] std::size_t Size() const {
    return static_cast<std::size_t>(std::max(file_.size, 0));
  }

  void End() {
    if (slot_ != nullptr) Content::ReleaseLiveDiscRequest(slot_);
    slot_ = nullptr;
  }

 private:
  Content_DiscFile file_{};
  Content_DiscQueueSlot* slot_ = nullptr;
  bool loaded_ = false;
};

// The DisplayHost services over the legacy frame globals, libgpu and the
// GTE. PublishState runs before every frame yield.
template <typename Host>
class LiveDisplay : public Host {
 public:
  virtual void PublishState() {}

  void DrawSprite(const SpriteDraw& sprite) override {
    SPRT packet{};
    setSprt(&packet);
    setSemiTrans(&packet, sprite.semi_transparent ? 1 : 0);
    setXY0(&packet, sprite.position.x, sprite.position.y);
    setUV0(&packet, sprite.texel.u, sprite.texel.v);
    setWH(&packet, sprite.width, sprite.height);
    packet.clut = sprite.palette;
    setRGB0(&packet, sprite.brightness, sprite.brightness, sprite.brightness);
    QueuePacket(packet, sprite.ordering_slot);
  }

  void DrawGlyph(const GlyphDraw& glyph) override {
    SPRT_8 packet{};
    setSprt8(&packet);
    setShadeTex(&packet, 1);
    setXY0(&packet, glyph.position.x, glyph.position.y);
    setUV0(&packet, glyph.texel.u, glyph.texel.v);
    packet.clut = glyph.palette;
    QueuePacket(packet, glyph.ordering_slot);
  }

  void DrawTexturedQuad(const TexturedQuadDraw& quad) override {
    POLY_FT4 packet{};
    setPolyFT4(&packet);
    setSemiTrans(&packet, quad.semi_transparent ? 1 : 0);
    SetCorners(packet, quad.corners);
    SetTexels(packet, quad.texels);
    packet.clut = quad.palette;
    packet.tpage = quad.texture_page;
    setRGB0(&packet, quad.brightness, quad.brightness, quad.brightness);
    QueuePacket(packet, quad.ordering_slot);
  }

  void DrawGradientQuad(const GradientQuadDraw& quad) override {
    POLY_G4 packet{};
    setPolyG4(&packet);
    setSemiTrans(&packet, quad.semi_transparent ? 1 : 0);
    SetCorners(packet, quad.corners);
    const auto& shade = quad.brightness;
    setRGB0(&packet, shade[0], shade[0], shade[0]);
    setRGB1(&packet, shade[1], shade[1], shade[1]);
    setRGB2(&packet, shade[2], shade[2], shade[2]);
    setRGB3(&packet, shade[3], shade[3], shade[3]);
    QueuePacket(packet, quad.ordering_slot);
  }

  void DrawTile(const TileDraw& tile) override {
    TILE packet{};
    setTile(&packet);
    setSemiTrans(&packet, tile.semi_transparent ? 1 : 0);
    setXY0(&packet, tile.position.x, tile.position.y);
    setWH(&packet, tile.width, tile.height);
    setRGB0(&packet, tile.brightness, tile.brightness, tile.brightness);
    QueuePacket(packet, tile.ordering_slot);
  }

  void ChangeTexturePage(const TexturePageChange& change) override {
    Render::AppendLiveTexturePage(change.texture_page, change.ordering_slot);
  }

  void SetDisplayEnabled(bool enabled) override {
    SetDispMask(enabled ? 1 : 0);
  }
  void SetGeometryOffset(int x, int y) override {
    Core_Gte_SetGeomOffset(x, y);
  }
  void SetProjectionDistance(int distance) override {
    Core_Gte_SetGeomScreen(distance);
  }

  void ApplyDisplayLayout(const DisplayLayout& layout) override {
    for (std::size_t buffer = 0; buffer < kFrameBufferCount; ++buffer) {
      DRAWENV& draw = Render_legacyDrawEnvironments[buffer];
      DISPENV& display = Render_legacyDisplayEnvironments[buffer];
      const ScreenPoint draw_origin = layout.draw_origins[buffer];
      const ScreenPoint display_origin = layout.display_origins[buffer];
      SetDefDrawEnv(&draw, draw_origin.x, draw_origin.y, layout.width,
                    layout.viewport_height);
      SetDefDispEnv(&display, display_origin.x, display_origin.y, layout.width,
                    layout.viewport_height);
      display.screen = ToRect(layout.screen);
      draw.dtd = 0;
      draw.isbg = 1;
      setRGB0(&draw, layout.background.red, layout.background.green,
              layout.background.blue);
    }
    Render_legacyViewportLeft = 0;
    Render_legacyViewportTop = 0;
    Render_viewportWidth = layout.width;
    Render_viewportHeight = static_cast<u_short>(layout.viewport_height);
  }

  void ApplyDrawEnvironment(int buffer,
                            const DrawEnvironment& environment) override {
    DRAWENV& draw = Render_legacyDrawEnvironments[BufferIndex(buffer)];
    draw.isbg = environment.clear_background ? 1 : 0;
    draw.dtd = environment.dither ? 1 : 0;
    draw.dfe = environment.draw_on_display ? 1 : 0;
    setRGB0(&draw, environment.background.red, environment.background.green,
            environment.background.blue);
    draw.tpage = environment.texture_page;
  }

  void SetInterlaced(int buffer, bool interlaced) override {
    Render_legacyDisplayEnvironments[BufferIndex(buffer)].isinter =
        interlaced ? 1 : 0;
  }

  void ClearVram(const Render::ScreenRect& area,
                 Render::ClearColor color) override {
    RECT rect = ToRect(area);
    ClearImage(&rect, color.red, color.green, color.blue);
  }

  void LoadImage(const Render::ScreenRect& area,
                 std::span<const std::byte> pixels) override {
    constexpr std::size_t kPixelBytes = 2;
    const std::size_t bytes = area.width < 0 || area.height < 0
                                  ? 0
                                  : static_cast<std::size_t>(area.width) *
                                        static_cast<std::size_t>(area.height) *
                                        kPixelBytes;
    if (area.width < 0 || area.height < 0 || pixels.size() < bytes) {
      VS_LOG_WARN(kChannel,
                  "VRAM upload refused: {}x{} at ({}, {}) needs {} bytes, "
                  "has {}",
                  area.width, area.height, area.x, area.y, bytes,
                  pixels.size());
      return;
    }
    RECT rect = ToRect(area);
    // libgpu's LoadImage only reads the pixels.
    ::LoadImage(&rect, reinterpret_cast<u_long*>(
                           const_cast<std::byte*>(pixels.data())));
  }

  void ShowFrameBuffer(int buffer) override {
    const std::size_t index = BufferIndex(buffer);
    PutDispEnv(&Render_legacyDisplayEnvironments[index]);
    PutDrawEnv(&Render_legacyDrawEnvironments[index]);
  }
  [[nodiscard]] int FrameBuffer() const override { return Render_frameBuf; }
  void SetFrameBuffer(int buffer) override { Render_frameBuf = buffer; }

  void DrawOrderingTable(int ordering_slot) override {
    u_long* entry = SceneOrderingEntry(ordering_slot);
    if (entry == nullptr) {
      VS_LOG_WARN(kChannel,
                  "Frame draw refused: scene slot {} is outside the frame's "
                  "ordering table",
                  ordering_slot);
      return;
    }
    DrawOTag(entry);
  }

  int AdvanceFrameClock() override {
    PublishState();
    return State::UpdateLivePlayTimeAndLoading(0);
  }
  void ResetPresentedFrameCount() override {
    Ending_Credits_xaResumeFrameCount = 0;
  }
  void CountPresentedFrame() override { ++Ending_Credits_xaResumeFrameCount; }
  void UpdateControllers() override { Input::UpdateLiveControllers(); }
  [[nodiscard]] std::uint32_t ButtonsPressed() const override {
    return static_cast<std::uint32_t>(Input_buttonsPressed.allButtons);
  }

  void BeginOrderingTables(const OrderingTableLayout& layout) override {
    const std::size_t buffer = BufferIndex(layout.buffer);
    auto* base = static_cast<u_long*>(Render_legacyOrderingTables[buffer]);
    Render_frameScratch.primitiveCursor = Render_legacyPrimitiveBuffers[buffer];
    if (base == nullptr) {
      VS_LOG_WARN(kChannel, "Frame {} has no ordering table", buffer);
      Render_frameScratch.overlayOrderingTable = nullptr;
      Render_frameScratch.orderingTable = nullptr;
      return;
    }
    const std::span<u_long> table(base,
                                  static_cast<std::size_t>(layout.entries));
    Render_frameScratch.overlayOrderingTable =
        table.subspan(static_cast<std::size_t>(layout.overlay_offset)).data();
    Render_frameScratch.orderingTable =
        table.subspan(static_cast<std::size_t>(layout.scene_offset)).data();
    ClearOTagR(base, layout.entries);
  }

  void ResetHeap() override { Core::Memory::ResetLiveGameHeap(); }

  void AllocateFrameBuffer(FrameBufferKind kind, int buffer,
                           std::size_t bytes) override {
    void* allocation = Core::Memory::AllocateLiveFromLowEnd(bytes);
    if (allocation == nullptr) {
      VS_LOG_WARN(kChannel, "Frame {} buffer of {} bytes refused by the heap",
                  buffer, bytes);
    }
    const std::size_t index = BufferIndex(buffer);
    if (kind == FrameBufferKind::kPrimitives) {
      Render_legacyPrimitiveBuffers[index] = allocation;
    } else {
      Render_legacyOrderingTables[index] = allocation;
    }
  }

  [[nodiscard]] int VerticalBlankCounter() override { return VSync(1); }
  void WaitForVerticalBlank() override {
    PublishState();
    VSync(0);
  }
};

// A display with no state of its own (the rank background draw).
class LiveFrameHost final : public LiveDisplay<DisplayHost> {};

// ---- progress records

ScoreRecord LoadScore() {
  const State_ScoreData& source = Audio_Seq_unusedScoredata;
  ScoreRecord score{.titles = static_cast<std::uint32_t>(source.titles),
                    .max_chain = source.maxChain,
                    .clear_count = source.clearCount,
                    .completion_time_minutes = source.completionTimeMinutes,
                    .map_completion = source.mapCompletion,
                    .opened_chest_count = source.openedChestCount,
                    .streak_score = source.streakScore,
                    .misc_score = source.miscScore};
  static_assert(std::size(source.enemyKills) == kEnemyClassCount);
  static_assert(std::size(source.weaponAttacks) == kWeaponCategoryCount);
  static_assert(std::size(source.bossTimeTrialScores) == kBossTrialCount);
  std::ranges::copy(source.enemyKills, score.enemy_kills.begin());
  std::ranges::copy(source.weaponAttacks, score.weapon_attacks.begin());
  for (std::size_t boss = 0; boss < kBossTrialCount; ++boss) {
    score.boss_trial_times[boss] = source.bossTimeTrialScores[boss][0].time;
  }
  return score;
}

PlaythroughFlags LoadFlags() {
  const State_StateFlags& source = Audio_Seq_unusedStateFlags;
  PlaythroughFlags flags{
      .clear_count = static_cast<std::uint8_t>(source.clearCount),
      .difficulty = static_cast<std::uint8_t>(source.difficulty),
      .normal_mode_cleared =
          static_cast<std::uint8_t>(source.normalModeCleared),
      .magic_used = source.magicUsed != 0,
      .battle_abilities_used = source.battleAbilitiesUsed != 0,
      .break_arts_used = source.breakArtsUsed != 0,
      .bosses_defeated = {source.damascusGolemDefeated != 0,
                          source.damascusCrabDefeated != 0,
                          source.ravanaDefeated != 0,
                          source.dragonZombieDefeated != 0,
                          source.deathAndOgreZombieDefeated != 0,
                          source.asuraDefeated != 0},
      .gold_key_obtained = source.goldKeyObtained != 0,
      .chest_key_obtained = source.chestKeyObtained != 0};
  static_assert(std::size(source.chestsOpened) == kChestCount);
  std::ranges::transform(
      source.chestsOpened, flags.chests_opened.begin(),
      [](char opened) { return static_cast<std::uint8_t>(opened); });
  return flags;
}

ClearProgress LoadProgress() {
  ClearProgress progress{.play_time = {.hours = State_playTime.t.hours,
                                       .minutes = State_playTime.t.minutes},
                         .score = LoadScore(),
                         .flags = LoadFlags(),
                         .save_count = Input_settings.currentGameSaveCount,
                         .entry_zone = State_savedEntryDoor.destinationZoneId,
                         .entry_room = State_savedEntryDoor.destinationRoomId};
  static_assert(std::size(State_roomVisitProgress.roomFlags) == kRoomFlagWords);
  std::ranges::transform(
      State_roomVisitProgress.roomFlags, progress.visited_rooms.begin(),
      [](int bits) { return static_cast<std::uint32_t>(bits); });
  static_assert(std::size(Battle_actionDefinitions) == kActionCount);
  std::ranges::transform(Battle_actionDefinitions,
                         progress.unlocked_actions.begin(),
                         [](const Battle_ActionDefinition& action) {
                           return action.unlocked != 0;
                         });
  return progress;
}

// Stores the records a clear writes; ResetPlaythroughRecords has already
// cleared the room visits and the later playthrough flags.
void StoreProgress(const ClearProgress& progress) {
  State_ScoreData& score = Audio_Seq_unusedScoredata;
  score.titles = static_cast<int>(progress.score.titles);
  score.clearCount = progress.score.clear_count;
  score.completionTimeMinutes = progress.score.completion_time_minutes;
  score.mapCompletion = progress.score.map_completion;
  score.openedChestCount = progress.score.opened_chest_count;
  State_StateFlags& flags = Audio_Seq_unusedStateFlags;
  flags.clearCount = static_cast<char>(progress.flags.clear_count);
  flags.difficulty = static_cast<char>(progress.flags.difficulty);
  flags.normalModeCleared =
      static_cast<char>(progress.flags.normal_mode_cleared);
  State_savedEntryDoor.destinationZoneId = progress.entry_zone;
  State_savedEntryDoor.destinationRoomId = progress.entry_room;
}

class LiveProgressHost final : public ProgressHost {
 public:
  void ResetPlaythroughRecords() override {
    State_roomVisitProgress = {};
    const auto flags =
        std::as_writable_bytes(std::span(&Audio_Seq_unusedStateFlags, 1));
    std::ranges::fill(flags.subspan(kRetainedFlagBytes), std::byte{0});
  }
};

using RoomBits = std::array<std::uint32_t, kRoomFlagWords>;

RoomBits CountableRoomBits() {
  static_assert(std::size(Ending_countableRoomBits) == kRoomFlagWords);
  RoomBits bits{};
  std::ranges::transform(Ending_countableRoomBits, bits.begin(), [](int word) {
    return static_cast<std::uint32_t>(word);
  });
  return bits;
}

// ---- clear results

Render::RegionColor Color(const CVECTOR& color) {
  return {color.r, color.g, color.b, color.cd};
}

RegionColorPair Pair(std::span<const CVECTOR> colors) {
  return {Color(colors[0]), Color(colors[1])};
}

ResultsTints LoadTints() {
  ResultsTints tints{
      .score = Pair(Ending_scoreTint),
      .counting_score = Pair(Ending_countingScoreTint),
      .map_completion = Pair(Ending_mapCompletionTint),
      .counting_map_completion = Pair(Ending_countingMapCompletionTint),
      .rank_heading = Pair(Ending_rankHeadingTint),
      .rank_label = Pair(Ending_rankLabelTint)};
  std::ranges::transform(Ending_congratulationsTint,
                         tints.congratulations.begin(), Color);
  return tints;
}

// The module image's results tables as the core's typed views.
class LiveResultsTables {
 public:
  LiveResultsTables() : rooms_(CountableRoomBits()) {
    // Read the atlas bytes as unsigned whatever this unit's plain char is.
    std::ranges::transform(Ending_resultAtlasRegions, atlas_.begin(),
                           [](const Render_TextureRegion& region) {
                             return Render::TextureRegion{
                                 static_cast<std::uint8_t>(region.x),
                                 static_cast<std::uint8_t>(region.y),
                                 static_cast<std::uint8_t>(region.w),
                                 static_cast<std::uint8_t>(region.h),
                                 region.tpage,
                                 region.clut};
                           });
    for (std::size_t rank = 0; rank < labels_.size(); ++rank) {
      std::ranges::copy(Ending_rankLabelRegionIds[rank], labels_[rank].begin());
    }
  }

  [[nodiscard]] ResultsTables Tables() const {
    return {.countable_room_bits = rooms_,
            .atlas = atlas_,
            .rank_labels = labels_,
            .rank_title_thresholds = Ending_rankTitleThresholds,
            .rank_score_thresholds = Ending_rankScoreThresholds,
            .rank_heading_width = Ending_kRankHeadingWidth,
            .tints = LoadTints()};
  }

 private:
  RoomBits rooms_;
  std::array<Render::TextureRegion, std::size(Ending_resultAtlasRegions)>
      atlas_{};
  std::array<RankLabel, std::size(Ending_rankLabelRegionIds)> labels_{};
};

ResultsState LoadResultsState() {
  return {.load_phase = Ending_resultsLoadPhase,
          .rank = Ending_riskbreakerRankIndex,
          .clear_count = Ending_clearCount,
          .final_score = Ending_finalScore,
          .map_completion = Ending_mapCompletion,
          .displayed_map_completion = Ending_displayedMapCompletion,
          .displayed_score = Ending_displayedScore,
          .animation_frame = Ending_resultsAnimationFrame,
          .is_leaving = Ending_isLeavingResults != 0,
          .is_score_incrementing = Ending_isScoreIncrementing != 0,
          .is_map_completion_incrementing =
              Ending_isMapCompletionIncrementing != 0};
}

void StoreResultsState(const ResultsState& state) {
  Ending_resultsLoadPhase = state.load_phase;
  Ending_riskbreakerRankIndex = state.rank;
  Ending_clearCount = state.clear_count;
  Ending_finalScore = state.final_score;
  Ending_mapCompletion = state.map_completion;
  Ending_displayedMapCompletion = state.displayed_map_completion;
  Ending_displayedScore = state.displayed_score;
  Ending_resultsAnimationFrame = state.animation_frame;
  Ending_isLeavingResults = state.is_leaving ? 1 : 0;
  Ending_isScoreIncrementing = state.is_score_incrementing ? 1 : 0;
  Ending_isMapCompletionIncrementing =
      state.is_map_completion_incrementing ? 1 : 0;
}

class LiveResultsHost final : public LiveDisplay<ResultsHost> {
 public:
  LiveResultsHost(ResultsState& state, ClearProgress& progress)
      : state_(state), progress_(progress) {}

  void PublishState() override {
    StoreResultsState(state_);
    Audio_Seq_unusedScoredata.mapCompletion = progress_.score.map_completion;
    Audio_Seq_unusedScoredata.openedChestCount =
        progress_.score.opened_chest_count;
  }

  void Draw(const Render::RegionQuad& quad) override {
    POLY_GT4 packet{};
    setPolyGT4(&packet);
    setXY4(&packet, quad.left, quad.top, quad.right, quad.top, quad.left,
           quad.bottom, quad.right, quad.bottom);
    setUV4(&packet, quad.u_left, quad.v_top, quad.u_right, quad.v_top,
           quad.u_left, quad.v_bottom, quad.u_right, quad.v_bottom);
    const Render::QuadColor& left = quad.left_color;
    const Render::QuadColor& right = quad.right_color;
    setRGB0(&packet, left.r, left.g, left.b);
    setRGB1(&packet, right.r, right.g, right.b);
    setRGB2(&packet, left.r, left.g, left.b);
    setRGB3(&packet, right.r, right.g, right.b);
    setSemiTrans(&packet, 1);
    packet.clut = quad.clut;
    packet.tpage = quad.texture_page;
    QueuePacket(packet, kRegionQuadSlot);
  }

  void PlaySound(int sound) override {
    Audio::LivePlaySfxCentered(kSoundGroup, sound);
  }
  void StopSound(int sound) override { Audio::LiveStopSfx(kSoundGroup, sound); }

  void BeginResultsRead(std::size_t file) override {
    if (file >= kResultsFiles.size()) {
      VS_LOG_WARN(kChannel, "Results file {} refused: there are {}", file,
                  kResultsFiles.size());
      return;
    }
    Ending_resultsTimBuffer = read_.Begin(kResultsFiles[file], false);
    Ending_resultsDiscReadSlot = read_.Slot();
  }
  [[nodiscard]] bool IsResultsReadComplete() override {
    return read_.IsComplete();
  }
  [[nodiscard]] std::span<const std::byte> ResultsData() override {
    if (!read_.IsLoaded() || Ending_resultsTimBuffer == nullptr) return {};
    return {static_cast<const std::byte*>(Ending_resultsTimBuffer),
            read_.Size()};
  }
  void EndResultsRead() override {
    read_.End();
    Ending_resultsDiscReadSlot = nullptr;
  }
  void FreeResultsData() override {
    if (Ending_resultsTimBuffer != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(Ending_resultsTimBuffer);
    }
    Ending_resultsTimBuffer = nullptr;
  }

 private:
  ResultsState& state_;
  ClearProgress& progress_;
  LiveDiscRead read_;
};

void RunLiveClearResults() {
  const LiveResultsTables tables;
  ResultsState state = LoadResultsState();
  ClearProgress progress = LoadProgress();
  LiveResultsHost host(state, progress);
  ShowClearResults(state, progress, tables.Tables(), host);
  host.PublishState();
}

// ---- staff roll

class LiveCreditsHost final : public LiveDisplay<Credits::StaffRollHost> {
 public:
  explicit LiveCreditsHost(Credits::State& state) : state_(state) {}

  void PublishState() override;

  void BeginRippleProjection(int depth) override {
    const SVECTOR rotation{};
    MATRIX matrix{};
    Core::Math::BuildLiveRotationXyz(&rotation, &matrix);
    matrix.t[0] = 0;
    matrix.t[1] = 0;
    matrix.t[2] = depth;
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);
  }

  [[nodiscard]] std::array<ScreenPoint, kQuadCorners> ProjectQuad(
      const std::array<Credits::ModelVertex, kQuadCorners>& vertices) override {
    std::array<SVECTOR, kQuadCorners> model{};
    std::ranges::transform(vertices, model.begin(),
                           [](const Credits::ModelVertex& vertex) {
                             return SVECTOR{vertex.x, vertex.y, vertex.z, 0};
                           });
    std::array<Core_GteWord, kQuadCorners> screen{};
    Core_GteWord depth = 0;
    Core_GteWord flags = 0;
    auto& [model0, model1, model2, model3] = model;
    auto& [screen0, screen1, screen2, screen3] = screen;
    Core_Gte_RotTransPers4(&model0, &model1, &model2, &model3, &screen0,
                           &screen1, &screen2, &screen3, &depth, &flags);
    std::array<ScreenPoint, kQuadCorners> points{};
    std::ranges::transform(screen, points.begin(), [](Core_GteWord packed) {
      const auto word = static_cast<std::uint32_t>(packed);
      return ScreenPoint{static_cast<std::int16_t>(word & kHalfwordMask),
                         static_cast<std::int16_t>(word >> kHalfwordShift)};
    });
    return points;
  }

  void StartEndingMusic(int byte_count) override {
    Audio::LiveBeginXaStream(kEndingMusicSector, byte_count);
  }
  void StopMusic() override { Audio::LiveRequestXaStreamStop(); }

  [[nodiscard]] bool PrepareEmbeddedArt() override {
    prepared_ = Ending_Credits_PrepareEmbeddedArt() != 0;
    return prepared_;
  }
  [[nodiscard]] std::span<const std::byte> EmbeddedArt(
      std::size_t index) override {
    if (!prepared_ || index >= Credits::kEmbeddedArtCount) return {};
    return {reinterpret_cast<const std::byte*>(
                Ending_Credits_EmbeddedArt(static_cast<unsigned>(index))),
            kEmbeddedTimBytes};
  }
  [[nodiscard]] std::span<std::byte> IllustrationWorkspace() override {
    if (!prepared_ || Ending_Credits_illustrationTimWorkspace == nullptr) {
      return {};
    }
    return {
        reinterpret_cast<std::byte*>(Ending_Credits_illustrationTimWorkspace),
        kEmbeddedTimBytes};
  }

  void BeginIllustrationRead(std::size_t resource) override;
  [[nodiscard]] bool IsIllustrationReadComplete() override {
    return read_.IsComplete();
  }
  void EndIllustrationRead() override {
    if (reading_ < loaded_.size()) loaded_[reading_] = read_.IsLoaded();
    read_.End();
  }
  void FreeIllustration(std::size_t resource) override;
  [[nodiscard]] std::span<const std::byte> IllustrationResource(
      std::size_t resource) override;

  [[nodiscard]] std::span<const std::uint8_t> Script() override {
    return Ending_Credits_scriptBytes;
  }
  [[nodiscard]] std::span<const std::uint8_t> GlyphWidths() override {
    return Ending_Credits_glyphWidths;
  }
  void UpdateClearProgress() override { LiveCommitClearProgress(); }
  void ShowClearResults() override { RunLiveClearResults(); }

 private:
  // The disc illustrations' locations and sizes, from the first disc
  // illustration.
  static Content_DiscFile DiscFile(std::size_t resource) {
    const std::size_t index = resource - Credits::kFirstDiscIllustration;
    const std::span<const int, Credits::kIllustrationCount> locations(
        Ending_Credits_illustrationDiscLocations, Credits::kIllustrationCount);
    const std::span<const int, Credits::kIllustrationCount> sizes(
        Ending_Credits_illustrationDiscSizes, Credits::kIllustrationCount);
    return {locations[index], sizes[index]};
  }
  static bool IsDiscIllustration(std::size_t resource) {
    return resource >= Credits::kFirstDiscIllustration &&
           resource < Credits::kIllustrationCount;
  }

  Credits::State& state_;
  bool prepared_ = false;
  std::size_t reading_ = Credits::kIllustrationCount;
  std::array<bool, Credits::kIllustrationCount> loaded_{};
  LiveDiscRead read_;
};

void LiveCreditsHost::PublishState() {
  Ending_Credits_hasFinished = state_.has_finished ? 1 : 0;
  Ending_Credits_isBackgroundFading = state_.is_background_fading ? 1 : 0;
  Ending_Credits_wasSkipRequested = state_.was_skip_requested ? 1 : 0;
  Ending_Credits_elapsedFrames = state_.elapsed_frames;
  Ending_Credits_scriptDelayFrames = state_.script_delay_frames;
  const std::span<u_char> script(Ending_Credits_scriptBytes);
  Ending_Credits_scriptCursor =
      script.subspan(std::min(state_.script_cursor, script.size())).data();
  Ending_Credits_scrollSpeedFixed8 = state_.scroll_speed_fixed8;
  Ending_Credits_backgroundBrightness = state_.background_brightness;
  Ending_Credits_ripplePhaseAngle = state_.ripple.phase_angle;
  Ending_Credits_rippleScaleFrame = state_.ripple.scale_frame;
  std::memcpy(Ending_Credits_rippleDistanceGrid,
              state_.ripple.distance_grid.data(),
              sizeof Ending_Credits_rippleDistanceGrid);
  const Credits::IllustrationState& illustration = state_.illustration;
  Ending_Credits_illustrationBrightness = illustration.brightness;
  Ending_Credits_illustrationTexturePageX = illustration.texture_page_x;
  Ending_Credits_illustrationPaletteY = illustration.palette_y;
  std::ranges::copy(illustration.hold_frames,
                    std::begin(Ending_Credits_illustrationHoldFrames));
  Ending_Credits_illustrationDecodePhase =
      static_cast<u_char>(state_.decoder.phase);
  Ending_Credits_decodeBudgetStartTick = state_.decoder.budget_start_tick;
}

void LiveCreditsHost::BeginIllustrationRead(std::size_t resource) {
  read_.End();
  reading_ = resource;
  if (!IsDiscIllustration(resource)) {
    VS_LOG_WARN(kChannel, "Illustration {} refused: it is not on the disc",
                resource);
    return;
  }
  loaded_[resource] = false;
  Ending_Credits_illustrationResources[resource] =
      read_.Begin(DiscFile(resource), true);
}

void LiveCreditsHost::FreeIllustration(std::size_t resource) {
  if (resource >= Credits::kIllustrationCount) return;
  void*& buffer = Ending_Credits_illustrationResources[resource];
  if (buffer != nullptr) Core::Memory::FreeLiveFromHighEnd(buffer);
  buffer = nullptr;
  loaded_[resource] = false;
}

std::span<const std::byte> LiveCreditsHost::IllustrationResource(
    std::size_t resource) {
  if (!IsDiscIllustration(resource) || !loaded_[resource]) return {};
  const void* buffer = Ending_Credits_illustrationResources[resource];
  if (buffer == nullptr) return {};
  const auto size =
      static_cast<std::size_t>(std::max(DiscFile(resource).size, 0));
  return {static_cast<const std::byte*>(buffer), size};
}
}  // namespace

// decomp: ENDING.PRG vs_ending_exec (src/ENDING/ENDING.PRG/D4.c:999)
void LiveRunStaffRoll() {
  Credits::State state;
  std::ranges::copy(Ending_Credits_illustrationHoldFrames,
                    state.illustration.hold_frames.begin());
  LiveCreditsHost host(state);
  Credits::RunStaffRoll(state, host);
  host.PublishState();
}

// decomp: ENDING.PRG _do_updateClearGameStats (src/ENDING/ENDING.PRG/D4.c:1502)
void LiveCommitClearProgress() {
  ClearProgress progress = LoadProgress();
  const RoomBits rooms = CountableRoomBits();
  LiveProgressHost host;
  CommitClearProgress(progress, rooms, host);
  StoreProgress(progress);
}

// decomp: ENDING.PRG func_8006C3CC (src/ENDING/ENDING.PRG/D4.c:1887)
void LiveDrawRankBackground(int fade_frame) {
  LiveFrameHost host;
  DrawRankBackground(fade_frame, host);
}
}  // namespace VS::Ending
