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
#include "Battle/Action.hpp"
#include "Battle/ActionTable.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Log.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Ending/ClearResults.hpp"
#include "Ending/ClearResultsCore.hpp"
#include "Ending/Credits.hpp"
#include "Ending/Credits/EmbeddedArt.hpp"
#include "Ending/Credits/StaffRoll.hpp"
#include "Ending/Credits/StaffRollCore.hpp"
#include "Ending/Results.hpp"
#include "Field/Mpd.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Platform/LogicalTicks.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Images.hpp"
#include "Render/Render.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/GameClockLive.hpp"
#include "State/Location.hpp"
#include "State/PlayTime.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"

using VS::Content::disc::Locate;
using VS::Content::disc::LocateAs;

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::RotTransPers4;
using VS::Core::Gte::SetGeomOffset;
using VS::Core::Gte::SetGeomScreen;
using VS::Core::Gte::SetRotMatrix;
using VS::Core::Gte::SetTransMatrix;
using VS::Core::Gte::SVECTOR;

using VS::Battle::ActionDefinition;
using VS::Legacy::ClearImage;
using VS::Legacy::ClearOTagR;
using VS::Legacy::DISPENV;
using VS::Legacy::DRAWENV;
using VS::Legacy::DrawOTag;
using VS::Legacy::POLY_FT4;
using VS::Legacy::POLY_G4;
using VS::Legacy::POLY_GT4;
using VS::Legacy::PutDispEnv;
using VS::Legacy::PutDrawEnv;
using VS::Legacy::RECT;
using VS::Legacy::SetDefDispEnv;
using VS::Legacy::SetDefDrawEnv;
using VS::Legacy::SetDispMask;
using VS::Legacy::setPolyFT4;
using VS::Legacy::setPolyG4;
using VS::Legacy::setPolyGT4;
using VS::Legacy::setRGB0;
using VS::Legacy::setRGB1;
using VS::Legacy::setRGB2;
using VS::Legacy::setRGB3;
using VS::Legacy::setSemiTrans;
using VS::Legacy::setShadeTex;
using VS::Legacy::setSprt;
using VS::Legacy::setSprt8;
using VS::Legacy::setTile;
using VS::Legacy::setUV0;
using VS::Legacy::setUV4;
using VS::Legacy::setWH;
using VS::Legacy::setXY0;
using VS::Legacy::setXY4;
using VS::Legacy::SPRT;
using VS::Legacy::SPRT_8;
using VS::Legacy::TILE;

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
constexpr int kEndingMusicSector = Locate("ENDING/ENDING.XA").lba;
constexpr int kSoundGroup = 0x7E;
// RotTransPers4 packs a screen point as x in the low halfword, y in the high.
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr unsigned kHalfwordShift = 16;
// The playthrough flags after the clear count, difficulty and normal-mode
// clear's block are cleared by a clear.
constexpr std::size_t kRetainedFlagBytes = 0x10;

constexpr std::array<VS::Content::DiscFile, kResultsFileCount> kResultsFiles{{
    LocateAs<VS::Content::DiscFile>("SMALL/END.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR00.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR01.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR02.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR03.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR04.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR05.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR06.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR07.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR08.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR09.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR10.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR11.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR12.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR13.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR14.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ENDSCR15.DIS"),
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
         corners[2].x, corners[2].y, corners.back().x, corners.back().y);
}

template <typename Packet>
void SetTexels(Packet& packet,
               const std::array<TexelPoint, kQuadCorners>& texels) {
  setUV4(&packet, texels[0].u, texels[0].v, texels[1].u, texels[1].v,
         texels[2].u, texels[2].v, texels.back().u, texels.back().v);
}

// Entry `slot` from the scene table BeginFrame set, in whichever frame's
// table holds it: PresentFrame draws the table after flipping the frame
// buffer (D4.c:1480 takes the address before the flip).
std::uint32_t* SceneOrderingEntry(int slot) {
  const auto head = reinterpret_cast<std::uintptr_t>(
      VS::Render::FrameScratchpad().ordering_table);
  for (void* const frame_table : VS::Render::legacyOrderingTables) {
    const auto table = reinterpret_cast<std::uintptr_t>(frame_table);
    if (table == 0 || head < table ||
        (head - table) % sizeof(std::uint32_t) != 0) {
      continue;
    }
    const auto entry =
        static_cast<std::int64_t>((head - table) / sizeof(std::uint32_t)) +
        slot;
    if (entry >= 0 && entry < kOrderingTableEntries) {
      return std::span(static_cast<std::uint32_t*>(frame_table),
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
  void* Begin(const VS::Content::DiscFile& file, bool priority) {
    End();
    file_ = file;
    loaded_ = false;
    void* buffer = Core::Memory::AllocateLiveFromHighEnd(
        static_cast<std::size_t>(std::max(file.size, 0)));
    if (buffer == nullptr) {
      VS::Core::LogWarn(
          kChannel, "Disc read of sector {} refused: no {}-byte heap buffer",
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
    if (slot_->state == VS::Content::DiscRequestState::kLoaded) {
      loaded_ = true;
      return true;
    }
    if (slot_->state == VS::Content::DiscRequestState::kError) {
      VS::Core::LogWarn(
          kChannel, "Disc read of sector {} failed; it has no data", file_.lba);
      return true;
    }
    return false;
  }

  [[nodiscard]] bool IsLoaded() const { return loaded_; }
  [[nodiscard]] VS::Content::DiscQueueSlot* Slot() const { return slot_; }
  [[nodiscard]] std::size_t Size() const {
    return static_cast<std::size_t>(std::max(file_.size, 0));
  }

  void End() {
    if (slot_ != nullptr) Content::ReleaseLiveDiscRequest(slot_);
    slot_ = nullptr;
  }

 private:
  VS::Content::DiscFile file_{};
  VS::Content::DiscQueueSlot* slot_ = nullptr;
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
    setRGB3(&packet, shade.back(), shade.back(), shade.back());
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
  void SetGeometryOffset(int x, int y) override { SetGeomOffset(x, y); }
  void SetProjectionDistance(int distance) override { SetGeomScreen(distance); }

  void ApplyDisplayLayout(const DisplayLayout& layout) override {
    for (std::size_t buffer = 0; buffer < kFrameBufferCount; ++buffer) {
      DRAWENV& draw = VS::Render::legacyDrawEnvironments[buffer];
      DISPENV& display = VS::Render::legacyDisplayEnvironments[buffer];
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
    VS::Render::legacyViewportLeft = 0;
    VS::Render::legacyViewportTop = 0;
    VS::Render::viewportWidth = layout.width;
    VS::Render::viewportHeight =
        static_cast<std::uint16_t>(layout.viewport_height);
  }

  void ApplyDrawEnvironment(int buffer,
                            const DrawEnvironment& environment) override {
    DRAWENV& draw = VS::Render::legacyDrawEnvironments[BufferIndex(buffer)];
    draw.isbg = environment.clear_background ? 1 : 0;
    draw.dtd = environment.dither ? 1 : 0;
    draw.dfe = environment.draw_on_display ? 1 : 0;
    setRGB0(&draw, environment.background.red, environment.background.green,
            environment.background.blue);
    draw.tpage = environment.texture_page;
  }

  void SetInterlaced(int buffer, bool interlaced) override {
    VS::Render::legacyDisplayEnvironments[BufferIndex(buffer)].isinter =
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
      VS::Core::LogWarn(
          kChannel,
          "VRAM upload refused: {}x{} at ({}, {}) needs {} bytes, "
          "has {}",
          area.width, area.height, area.x, area.y, bytes, pixels.size());
      return;
    }
    RECT rect = ToRect(area);
    // libgpu's LoadImage only reads the pixels.
    VS::Legacy::LoadImage(&rect, reinterpret_cast<std::uint32_t*>(
                                     const_cast<std::byte*>(pixels.data())));
  }

  void ShowFrameBuffer(int buffer) override {
    const std::size_t index = BufferIndex(buffer);
    PutDispEnv(&VS::Render::legacyDisplayEnvironments[index]);
    PutDrawEnv(&VS::Render::legacyDrawEnvironments[index]);
  }
  [[nodiscard]] int FrameBuffer() const override {
    return VS::Render::frameBuf;
  }
  void SetFrameBuffer(int buffer) override { VS::Render::frameBuf = buffer; }

  void DrawOrderingTable(int ordering_slot) override {
    std::uint32_t* entry = SceneOrderingEntry(ordering_slot);
    if (entry == nullptr) {
      VS::Core::LogWarn(
          kChannel,
          "Frame draw refused: scene slot {} is outside the frame's "
          "ordering table",
          ordering_slot);
      return;
    }
    DrawOTag(entry);
  }

  void AdvanceFrameClock() override {
    PublishState();
    State::UpdateLivePlayTimeAndLoading();
  }
  void ResetPresentedFrameCount() override {
    VS::Ending::Credits::xaResumeFrameCount = 0;
  }
  void CountPresentedFrame() override {
    ++VS::Ending::Credits::xaResumeFrameCount;
  }
  void UpdateControllers() override { Input::UpdateLiveControllers(); }
  [[nodiscard]] std::uint32_t ButtonsPressed() const override {
    return static_cast<std::uint32_t>(VS::Input::buttonsPressed);
  }

  void BeginOrderingTables(const OrderingTableLayout& layout) override {
    const std::size_t buffer = BufferIndex(layout.buffer);
    VS::Render::BeginLegacyFrame(buffer);
    auto* base =
        static_cast<std::uint32_t*>(VS::Render::legacyOrderingTables[buffer]);
    VS::Render::FrameScratchpad().primitive_cursor =
        VS::Render::legacyPrimitiveBuffers[buffer];
    if (base == nullptr) {
      VS::Core::LogWarn(kChannel, "Frame {} has no ordering table", buffer);
      VS::Render::FrameScratchpad().overlay_ordering_table = nullptr;
      VS::Render::FrameScratchpad().ordering_table = nullptr;
      return;
    }
    const std::span<std::uint32_t> table(
        base, static_cast<std::size_t>(layout.entries));
    VS::Render::FrameScratchpad().overlay_ordering_table =
        table.subspan(static_cast<std::size_t>(layout.overlay_offset)).data();
    VS::Render::FrameScratchpad().ordering_table =
        table.subspan(static_cast<std::size_t>(layout.scene_offset)).data();
    ClearOTagR(base, layout.entries);
  }

  void ResetHeap() override { Boot::ResetLiveSessionHeap(); }

  void AllocateFrameBuffer(FrameBufferKind kind, int buffer,
                           std::size_t bytes) override {
    void* allocation = Core::Memory::AllocateLiveFromLowEnd(bytes);
    if (allocation == nullptr) {
      VS::Core::LogWarn(kChannel,
                        "Frame {} buffer of {} bytes refused by the heap",
                        buffer, bytes);
    }
    const std::size_t index = BufferIndex(buffer);
    VS::Render::SetLegacyFrameMemory(
        kind == FrameBufferKind::kPrimitives
            ? VS::Render::FrameMemory::kPrimitives
            : VS::Render::FrameMemory::kOrderingTable,
        index, allocation, bytes);
  }

  void WaitTick() override {
    PublishState();
    VS::Platform::WaitTicks();
  }
};

// A display with no state of its own (the rank background draw).
class LiveFrameHost final : public LiveDisplay<DisplayHost> {};

// ---- progress records

ScoreRecord LoadScore() {
  const VS::State::ScoreData& source = VS::State::scoreData;
  ScoreRecord score{.titles = static_cast<std::uint32_t>(source.titles),
                    .max_chain = source.max_chain,
                    .clear_count = source.clear_count,
                    .completion_time_minutes = source.completion_time_minutes,
                    .map_completion = source.map_completion,
                    .opened_chest_count = source.opened_chest_count,
                    .streak_score = source.streak_score,
                    .misc_score = source.misc_score};
  static_assert(std::size(source.enemy_kills) == kEnemyClassCount);
  static_assert(std::size(source.weapon_attacks) == kWeaponCategoryCount);
  static_assert(std::size(source.boss_time_trial_scores) == kBossTrialCount);
  std::ranges::copy(source.enemy_kills, score.enemy_kills.begin());
  std::ranges::copy(source.weapon_attacks, score.weapon_attacks.begin());
  for (std::size_t boss = 0; boss < kBossTrialCount; ++boss) {
    score.boss_trial_times[boss] = source.boss_time_trial_scores[boss][0].time;
  }
  return score;
}

PlaythroughFlags LoadFlags() {
  const VS::State::StateFlags& source = VS::State::stateFlags;
  PlaythroughFlags flags{
      .clear_count = static_cast<std::uint8_t>(source.clear_count),
      .difficulty = static_cast<std::uint8_t>(source.difficulty),
      .normal_mode_cleared =
          static_cast<std::uint8_t>(source.normal_mode_cleared),
      .magic_used = source.magic_used != 0,
      .battle_abilities_used = source.battle_abilities_used != 0,
      .break_arts_used = source.break_arts_used != 0,
      .bosses_defeated = {source.damascus_golem_defeated != 0,
                          source.damascus_crab_defeated != 0,
                          source.ravana_defeated != 0,
                          source.dragon_zombie_defeated != 0,
                          source.death_and_ogre_zombie_defeated != 0,
                          source.asura_defeated != 0},
      .gold_key_obtained = source.gold_key_obtained != 0,
      .chest_key_obtained = source.chest_key_obtained != 0};
  static_assert(std::size(source.chests_opened) == kChestCount);
  std::ranges::transform(
      source.chests_opened, flags.chests_opened.begin(),
      [](char opened) { return static_cast<std::uint8_t>(opened); });
  return flags;
}

ClearProgress LoadProgress() {
  ClearProgress progress{
      .play_time = {.hours = VS::State::playTime.hours,
                    .minutes = VS::State::playTime.minutes},
      .score = LoadScore(),
      .flags = LoadFlags(),
      .save_count = VS::Input::settings.current_game_save_count,
      .entry_zone = VS::State::savedEntryDoor.destination_zone_id,
      .entry_room = VS::State::savedEntryDoor.destination_room_id};
  static_assert(std::size(VS::State::roomVisitProgress.room_flags) ==
                kRoomFlagWords);
  std::ranges::transform(
      VS::State::roomVisitProgress.room_flags, progress.visited_rooms.begin(),
      [](int bits) { return static_cast<std::uint32_t>(bits); });
  static_assert(std::size(VS::Battle::actionDefinitions) == kActionCount);
  std::ranges::transform(
      VS::Battle::actionDefinitions, progress.unlocked_actions.begin(),
      [](const ActionDefinition& action) { return action.unlocked != 0; });
  return progress;
}

// Stores the records a clear writes; ResetPlaythroughRecords has already
// cleared the room visits and the later playthrough flags.
void StoreProgress(const ClearProgress& progress) {
  VS::State::ScoreData& score = VS::State::scoreData;
  score.titles = static_cast<int>(progress.score.titles);
  score.clear_count = progress.score.clear_count;
  score.completion_time_minutes = progress.score.completion_time_minutes;
  score.map_completion = progress.score.map_completion;
  score.opened_chest_count = progress.score.opened_chest_count;
  VS::State::StateFlags& flags = VS::State::stateFlags;
  flags.clear_count = static_cast<char>(progress.flags.clear_count);
  flags.difficulty = static_cast<char>(progress.flags.difficulty);
  flags.normal_mode_cleared =
      static_cast<char>(progress.flags.normal_mode_cleared);
  VS::State::savedEntryDoor.destination_zone_id = progress.entry_zone;
  VS::State::savedEntryDoor.destination_room_id = progress.entry_room;
}

class LiveProgressHost final : public ProgressHost {
 public:
  void ResetPlaythroughRecords() override {
    VS::State::roomVisitProgress = {};
    const auto flags =
        std::as_writable_bytes(std::span(&VS::State::stateFlags, 1));
    std::ranges::fill(flags.subspan(kRetainedFlagBytes), std::byte{0});
  }
};

using RoomBits = std::array<std::uint32_t, kRoomFlagWords>;

RoomBits CountableRoomBits() {
  RoomBits bits{};
  std::ranges::transform(
      VS::Ending::countableRoomBits, bits.begin(),
      [](std::int32_t word) { return static_cast<std::uint32_t>(word); });
  return bits;
}

// ---- clear results

ResultsTints LoadTints() {
  ResultsTints tints{
      .score = VS::Ending::scoreTint,
      .counting_score = VS::Ending::countingScoreTint,
      .map_completion = VS::Ending::mapCompletionTint,
      .counting_map_completion = VS::Ending::countingMapCompletionTint,
      .rank_heading = VS::Ending::rankHeadingTint,
      .rank_label = VS::Ending::rankLabelTint};
  std::ranges::copy(VS::Ending::congratulationsTint,
                    tints.congratulations.begin());
  return tints;
}

// The module image's results tables as the core's typed views.
class LiveResultsTables {
 public:
  LiveResultsTables() : rooms_(CountableRoomBits()) {
    std::ranges::copy(VS::Ending::resultAtlasRegions, atlas_.begin());
    std::ranges::copy(VS::Ending::rankLabelRegionIds, labels_.begin());
  }

  [[nodiscard]] ResultsTables Tables() const {
    return {.countable_room_bits = rooms_,
            .atlas = atlas_,
            .rank_labels = labels_,
            .rank_title_thresholds = VS::Ending::rankTitleThresholds,
            .rank_score_thresholds = VS::Ending::rankScoreThresholds,
            .rank_heading_width = atlas_[kRankHeadingRegion].width,
            .tints = LoadTints()};
  }

 private:
  RoomBits rooms_;
  std::array<Render::TextureRegion, std::size(VS::Ending::resultAtlasRegions)>
      atlas_{};
  std::array<RankLabel, std::size(VS::Ending::rankLabelRegionIds)> labels_{};
};

ResultsState LoadResultsState() {
  return {.load_phase = VS::Ending::resultsLoadPhase,
          .rank = VS::Ending::riskbreakerRankIndex,
          .clear_count = VS::Ending::clearCount,
          .final_score = VS::Ending::finalScore,
          .map_completion = VS::Ending::mapCompletion,
          .displayed_map_completion = VS::Ending::displayedMapCompletion,
          .displayed_score = VS::Ending::displayedScore,
          .animation_frame = VS::Ending::resultsAnimationFrame,
          .is_leaving = VS::Ending::isLeavingResults != 0,
          .is_score_incrementing = VS::Ending::isScoreIncrementing != 0,
          .is_map_completion_incrementing =
              VS::Ending::isMapCompletionIncrementing != 0};
}

void StoreResultsState(const ResultsState& state) {
  VS::Ending::resultsLoadPhase = state.load_phase;
  VS::Ending::riskbreakerRankIndex = state.rank;
  VS::Ending::clearCount = state.clear_count;
  VS::Ending::finalScore = state.final_score;
  VS::Ending::mapCompletion = state.map_completion;
  VS::Ending::displayedMapCompletion = state.displayed_map_completion;
  VS::Ending::displayedScore = state.displayed_score;
  VS::Ending::resultsAnimationFrame = state.animation_frame;
  VS::Ending::isLeavingResults = state.is_leaving ? 1 : 0;
  VS::Ending::isScoreIncrementing = state.is_score_incrementing ? 1 : 0;
  VS::Ending::isMapCompletionIncrementing =
      state.is_map_completion_incrementing ? 1 : 0;
}

class LiveResultsHost final : public LiveDisplay<ResultsHost> {
 public:
  LiveResultsHost(ResultsState& state, ClearProgress& progress)
      : state_(state), progress_(progress) {}

  void PublishState() override {
    StoreResultsState(state_);
    VS::State::scoreData.map_completion = progress_.score.map_completion;
    VS::State::scoreData.opened_chest_count =
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
      VS::Core::LogWarn(kChannel, "Results file {} refused: there are {}", file,
                        kResultsFiles.size());
      return;
    }
    VS::Ending::resultsTimBuffer = read_.Begin(kResultsFiles[file], false);
    VS::Ending::resultsDiscReadSlot = read_.Slot();
  }
  [[nodiscard]] bool IsResultsReadComplete() override {
    return read_.IsComplete();
  }
  [[nodiscard]] std::span<const std::byte> ResultsData() override {
    if (!read_.IsLoaded() || VS::Ending::resultsTimBuffer == nullptr) return {};
    return {static_cast<const std::byte*>(VS::Ending::resultsTimBuffer),
            read_.Size()};
  }
  void EndResultsRead() override {
    read_.End();
    VS::Ending::resultsDiscReadSlot = nullptr;
  }
  void FreeResultsData() override {
    if (VS::Ending::resultsTimBuffer != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(VS::Ending::resultsTimBuffer);
    }
    VS::Ending::resultsTimBuffer = nullptr;
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
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
  }

  [[nodiscard]] std::array<ScreenPoint, kQuadCorners> ProjectQuad(
      const std::array<Credits::ModelVertex, kQuadCorners>& vertices) override {
    std::array<SVECTOR, kQuadCorners> model{};
    std::ranges::transform(vertices, model.begin(),
                           [](const Credits::ModelVertex& vertex) {
                             return SVECTOR{vertex.x, vertex.y, vertex.z, 0};
                           });
    std::array<VS::Core::Gte::Word, kQuadCorners> screen{};
    VS::Core::Gte::Word depth = 0;
    VS::Core::Gte::Word flags = 0;
    auto& [model0, model1, model2, model3] = model;
    auto& [screen0, screen1, screen2, screen3] = screen;
    RotTransPers4(&model0, &model1, &model2, &model3, &screen0, &screen1,
                  &screen2, &screen3, &depth, &flags);
    std::array<ScreenPoint, kQuadCorners> points{};
    std::ranges::transform(
        screen, points.begin(), [](VS::Core::Gte::Word packed) {
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
    prepared_ = VS::Ending::Credits::PrepareEmbeddedArt() != 0;
    return prepared_;
  }
  [[nodiscard]] std::span<const std::byte> EmbeddedArt(
      std::size_t index) override {
    if (!prepared_ || index >= Credits::kEmbeddedArtCount) return {};
    return {reinterpret_cast<const std::byte*>(
                VS::Ending::Credits::EmbeddedArt(static_cast<unsigned>(index))),
            kEmbeddedTimBytes};
  }
  [[nodiscard]] std::span<std::byte> IllustrationWorkspace() override {
    if (!prepared_ ||
        VS::Ending::Credits::illustrationTimWorkspace == nullptr) {
      return {};
    }
    return {reinterpret_cast<std::byte*>(
                VS::Ending::Credits::illustrationTimWorkspace),
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
    return VS::Ending::Credits::scriptBytes;
  }
  [[nodiscard]] std::span<const std::uint8_t> GlyphWidths() override {
    return VS::Ending::Credits::glyphWidths;
  }
  void UpdateClearProgress() override { LiveCommitClearProgress(); }
  void ShowClearResults() override { RunLiveClearResults(); }

 private:
  // The disc illustrations' locations and sizes, from the first disc
  // illustration.
  static VS::Content::DiscFile DiscFile(std::size_t resource) {
    const std::size_t index = resource - Credits::kFirstDiscIllustration;
    const std::span<const std::int32_t, Credits::kIllustrationCount> locations(
        VS::Ending::Credits::illustrationDiscLocations);
    const std::span<const std::int32_t, Credits::kIllustrationCount> sizes(
        VS::Ending::Credits::illustrationDiscSizes);
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
  VS::Ending::Credits::hasFinished = state_.has_finished ? 1 : 0;
  VS::Ending::Credits::wasSkipRequested = state_.was_skip_requested ? 1 : 0;
  std::ranges::copy(state_.illustration.hold_frames,
                    std::begin(VS::Ending::Credits::illustrationHoldFrames));
}

void LiveCreditsHost::BeginIllustrationRead(std::size_t resource) {
  read_.End();
  reading_ = resource;
  if (!IsDiscIllustration(resource)) {
    VS::Core::LogWarn(
        kChannel, "Illustration {} refused: it is not on the disc", resource);
    return;
  }
  loaded_[resource] = false;
  VS::Ending::Credits::illustrationResources[resource] =
      read_.Begin(DiscFile(resource), true);
}

void LiveCreditsHost::FreeIllustration(std::size_t resource) {
  if (resource >= Credits::kIllustrationCount) return;
  void*& buffer = VS::Ending::Credits::illustrationResources[resource];
  if (buffer != nullptr) Core::Memory::FreeLiveFromHighEnd(buffer);
  buffer = nullptr;
  loaded_[resource] = false;
}

std::span<const std::byte> LiveCreditsHost::IllustrationResource(
    std::size_t resource) {
  if (!IsDiscIllustration(resource) || !loaded_[resource]) return {};
  const void* buffer = VS::Ending::Credits::illustrationResources[resource];
  if (buffer == nullptr) return {};
  const auto size =
      static_cast<std::size_t>(std::max(DiscFile(resource).size, 0));
  return {static_cast<const std::byte*>(buffer), size};
}
}  // namespace

// decomp: ENDING.PRG vs_ending_exec (src/ENDING/ENDING.PRG/D4.c:999)
void LiveRunStaffRoll() {
  Credits::State state;
  std::ranges::copy(VS::Ending::Credits::illustrationHoldFrames,
                    state.illustration.hold_frames.begin());
  LiveCreditsHost host(state);
  Credits::RunStaffRoll(state, host);
  host.PublishState();
}

// decomp: ENDING.PRG do_updateClearGameStats (src/ENDING/ENDING.PRG/D4.c:1560;
// was _do_updateClearGameStats)
void LiveCommitClearProgress() {
  ClearProgress progress = LoadProgress();
  const RoomBits rooms = CountableRoomBits();
  LiveProgressHost host;
  CommitClearProgress(progress, rooms, host);
  StoreProgress(progress);
}

// decomp: ENDING.PRG fadeIn (src/ENDING/ENDING.PRG/D4.c:2013; was
// func_8006C3CC)
void LiveDrawRankBackground(int fade_frame) {
  LiveFrameHost host;
  DrawRankBackground(fade_frame, host);
}
}  // namespace VS::Ending
