// Isolated test-link endpoints only. Heap, queue, TIM decoding, page ownership,
// framebuffer-effect allocation/lifecycle, scene tick speed, stat awards and
// both terminal/game-clear Live adapters are real production implementations.
// Physical disc commands, audio, drawing, GTE transforms and logging are
// endpoints, not evidence of physical I/O or interactive presentation.
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

#include "Audio/MusicLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Gte.h"
#include "Core/Runtime.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Results/CubePuzzleLifecycleLive.hpp"
#include "Menu/Results/GameClearScoreLive.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Results/TimeAttackHeadingLive.hpp"
#include "Menu/Results/TimeAttackRatingLive.hpp"
#include "Menu/Score/ScoreRankLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/TextureRegionDrawLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/GameClockLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Audio/SoundBanks.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/StatusEffects.h"
#include "Content/DiscLayout.h"
#include "Core/BiosRand.h"
#include "Core/Diagnostics.h"
#include "Diagnostics/SceneTrace.h"
#include "DiscQueue.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Field/MapProgress.h"
#include "Field/TransitionsLiveState.h"
#include "Input/ControllerLiveState.h"
#include "Legacy/LibDs.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/Display.h"
#include "Render/ScreenTransition.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

decltype(Audio_Seq_unusedScoredata) Audio_Seq_unusedScoredata{};
decltype(Audio_Seq_unusedStateFlags) Audio_Seq_unusedStateFlags{};
decltype(Audio_soundState) Audio_soundState{};
decltype(State_roomVisitProgress) State_roomVisitProgress{};
decltype(State_playTime) State_playTime{};
decltype(Input_buttonsPressed) Input_buttonsPressed{};
decltype(Content_discRequestSlots) Content_discRequestSlots{};
decltype(Content_discRequestCounts) Content_discRequestCounts{};
decltype(Content_discDriveState) Content_discDriveState{};
decltype(Content_legacyDiscReadBuffer) Content_legacyDiscReadBuffer{};
decltype(Content_legacyDiscReadLocation) Content_legacyDiscReadLocation{};
decltype(Content_discControlResponseBytes) Content_discControlResponseBytes{};
decltype(Content_legacyXaReadyBcdField) Content_legacyXaReadyBcdField{};
decltype(Content_legacyXaReadyByteThree) Content_legacyXaReadyByteThree{};
decltype(Content_legacyXaReadyByteFour) Content_legacyXaReadyByteFour{};
decltype(Content_xaReadyMetadataWord) Content_xaReadyMetadataWord{};
decltype(Field_countableRoomMask) Field_countableRoomMask{};
decltype(Render_legacyDrawEnvironments) Render_legacyDrawEnvironments{};
decltype(Render_frameBuf) Render_frameBuf{};
decltype(Field_screenTransitionStep) Field_screenTransitionStep{};
decltype(Field_screenTransitionEffect) Field_screenTransitionEffect{};
decltype(Field_screenTransitionAlpha) Field_screenTransitionAlpha{};
decltype(Field_screenTransitionWipeAngle) Field_screenTransitionWipeAngle{};
decltype(Field_screenTransitionSpeed) Field_screenTransitionSpeed{};
decltype(Render_legacyTransitionFadePackets)
    Render_legacyTransitionFadePackets{};
decltype(Render_legacyTransitionDrawModes) Render_legacyTransitionDrawModes{};
decltype(Render_legacyTransitionWipePackets)
    Render_legacyTransitionWipePackets{};
decltype(Render_legacyTransitionEdgePackets)
    Render_legacyTransitionEdgePackets{};
decltype(Battle_characterState) Battle_characterState{};
decltype(Battle_unusedSubmenuState) Battle_unusedSubmenuState{};
decltype(Menu_Results_clutBuf) Menu_Results_clutBuf{};
decltype(Menu_Results_disMap) Menu_Results_disMap{};
decltype(Menu_Results_buffReels) Menu_Results_buffReels{};
int Menu_Results_newTimeSlot{};
int Menu_Results_timeTrialTime{};
int Menu_Results_screenTimer{};
int Menu_Results_screenState{};
int Menu_Results_buffReelIndex{};
u_int Menu_Results_buffReelSelection{};
u_int Menu_Results_elementAnimationState{};
u_int Menu_Results_score{};
u_int Menu_Results_mapCompletion{};
int Menu_Results_rank{};
int Menu_Results_clearCount{};
int Menu_Results_onScreenScore{};
int Menu_Results_incrementingScore{};
int Menu_Results_onScreenMapCompletion{};
int Menu_Results_incrementingMapCompletion{};
extern const Content_DiscFile Menu_Results_kDisFiles[]{
    {VS_RANK_DIS_LBA, VS_RANK_DIS_SIZE},
    {VS_TIME_DIS_LBA, VS_TIME_DIS_SIZE},
    {VS_ATTACK_DIS_LBA, VS_ATTACK_DIS_SIZE},
    {VS_IQ_DIS_LBA, VS_IQ_DIS_SIZE},
    {VS_ESC_DIS_LBA, VS_ESC_DIS_SIZE}};

void Core_Warnf(const char* area, const char* format, ...) {
  std::fprintf(stderr, "%s: ", area);
  va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
}
void Core_Errorf(const char* area, const char* format, ...) {
  std::fprintf(stderr, "%s: ", area);
  va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
}
int Core_Random_Rand() { return 0; }
int DsGetSector(void*, int) { return 0; }
void SetDrawArea(DR_AREA*, RECT*) {}
void SetDrawMode(DR_MODE*, int, int, int, RECT*) {}
void AddPrim(void*, void*) {}
void Core_Gte_SetRotMatrix(const MATRIX*) {}
void Core_Gte_SetTransMatrix(const MATRIX*) {}
// The published GTE signature uses Win32 long, not an int32_t alias.
// NOLINTNEXTLINE(runtime/int,google-runtime-int)
void Core_Gte_RotTrans(SVECTOR*, VECTOR*, long*) {}
std::int16_t Event_EvaluateRoomEventTriggersEntry(int, int, std::uint8_t*,
                                                  int) {
  return 0;
}
void Diagnostics_RecordScriptFlagWrite(std::int32_t, std::uint8_t,
                                       std::uint8_t) {}
}

namespace VS::Core {
void IgnoreDiagnostic(int, int) {}
}  // namespace VS::Core

namespace VS::Content {
bool BeginLiveDiscRead(std::int32_t, std::uint32_t, void*) { return false; }
std::uint64_t LiveDiscReadGeneration() noexcept { return 0; }
bool LiveDiscReadCompleted(std::uint64_t) noexcept { return false; }
void CancelLiveDiscRead(const void*) {}
void UpdateLiveDiscCommands() {}
}  // namespace VS::Content

namespace VS::State {
int UpdateLivePlayTimeAndLoading(int) { return 0; }
}  // namespace VS::State

namespace VS::Battle {
void ShowLivePuzzleTimer() {}
void HideLivePuzzleTimer() {}
}  // namespace VS::Battle

namespace VS::Audio {
int LivePollMusicLoads() { return 0; }
int LivePlayMusicSlot(int, int, int) { return 0; }
int LiveLoadMusicSlot(int, int) { return 0; }
void LivePlaySfxCentered(int, int) {}
void LiveStopSfx(int, int) {}
void LiveFadeSfxVolume(int, int, int, int) {}
void LiveFadeSfxPitch(int, int, int, int) {}
}  // namespace VS::Audio

namespace VS::Menu::Framework {
void NoOperation() {}
}  // namespace VS::Menu::Framework

namespace VS::Menu::Results {
std::array<Render::TextureRegion, kResultsAtlasEntries> LiveResultsAtlas() {
  return {};
}
void LiveDrawCongratulations(int, int, int) {}
void LiveDrawScore(int, int, int, int) {}
void LiveDrawCountingScore(int, int, int, int) {}
void LiveDrawMapCompletion(int, int, int, int) {}
void LiveDrawCountingMapCompletion(int, int, int, int) {}
void LiveDrawRiskbreakerRankHeading(int, int, int) {}
void LiveDrawRiskbreakerRank(int, int, int) {}
void LiveDrawPressButtonPrompt(int, int, int, int) {}
void LiveDrawFadingTextureClut(int, int, int, int, int) {}
void LiveDrawTimeAttackResultsHeading(int, int, int) {}
void LiveDrawTimeAttackTime(int, int, int) {}
void LiveDrawCountingTimeAttackTime(int, int, int) {}
void LiveDrawTimeAttackRating(int, int, int) {}
void LiveDrawTimeAttackRatingWipe(int, int, int) {}
void LiveDrawNewRecord(int, int, int) {}
void LiveDrawLeaderboard(int, int, int) {}
void LiveDrawLeaderboardEntry(int, int, int, int) {}
void LiveDrawTimeAttackHeading(int, int, int) {}
void LiveDrawCourseNumber(int, int, int) {}
void LiveDrawBossName(int, int, int) {}
void LiveDrawBestTime(int, int, int) {}
void LiveDrawStartCountdown(int, int, int) {}
void LivePulseTimeAttackClut(int) {}
int LiveInitializeCubePuzzleStart() { return 0; }
int LiveInitializeCubePuzzleResults() { return 0; }
int LiveInitializeCubePuzzleQuit() { return 0; }
int LiveUpdateCubePuzzleStart() { return 0; }
int LiveUpdateCubePuzzleResults() { return 0; }
int LiveUpdateCubePuzzleQuit() { return 0; }
}  // namespace VS::Menu::Results

namespace VS::Menu::Score {
ScoreRecord ReadLiveScoreRecord() {
  return {
      .titles = static_cast<std::uint32_t>(Audio_Seq_unusedScoredata.titles),
      .map_completion = Audio_Seq_unusedScoredata.mapCompletion,
      .opened_chest_count = Audio_Seq_unusedScoredata.openedChestCount,
      .streak_score = Audio_Seq_unusedScoredata.streakScore,
      .misc_score = Audio_Seq_unusedScoredata.miscScore};
}
}  // namespace VS::Menu::Score

namespace VS::Render {
u_long* LiveFrameOrderingEntry(LiveOrderingBase, int) { return nullptr; }
u_long* ReserveLivePrimitiveWords(std::size_t) { return nullptr; }
std::uint32_t LivePacketQueue::ChainLink() const { return 0; }
bool LivePacketQueue::Emit(std::span<const std::uint32_t>) { return false; }
void DrawLiveTextureTintFade(std::span<const Render_TextureRegion>, int, int,
                             int, std::span<const CVECTOR>) {}
void UploadLiveImageRectangle(int, const void*, int) {}
}  // namespace VS::Render
