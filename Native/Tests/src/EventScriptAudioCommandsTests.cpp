#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Event/ScriptAudioCommands.hpp"
#include "TestSupport/ScriptSteps.hpp"

namespace {
namespace event = VS::Event;
using event::ScriptAudioRefusal;
using event::ScriptStep;
using Calls = std::vector<std::string>;
using Refusal = std::pair<ScriptAudioRefusal, int>;
using Bytes = std::vector<std::uint8_t>;

class RecordingHost final : public event::ScriptAudioHost {
 public:
  Calls calls;
  std::vector<Refusal> refusals;
  bool skip = false;
  std::uint8_t music_slot = 0;
  bool ambient_suspended = false;
  int suspended_ambient = -1;
  int sfx_poll = 0;
  int music_poll = 0;
  int wave_poll = 0;
  int zone_sound = 0;
  int queried_volume = 0;
  int volume = 0;
  int beat = 0;

  template <typename... Values>
  void Log(std::string_view name, const Values&... values) {
    std::string line(name);
    ((line += std::format(" {}", values)), ...);
    calls.push_back(std::move(line));
  }

  void Refuse(ScriptAudioRefusal refusal, int value) override {
    refusals.emplace_back(refusal, value);
  }
  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    return reference + 100;
  }
  [[nodiscard]] bool SkipActive() const override { return skip; }
  [[nodiscard]] std::uint8_t MusicSlot() const override { return music_slot; }
  void SetMusicSlot(std::uint8_t slot) override { music_slot = slot; }
  [[nodiscard]] bool AmbientSoundSuspended() const override {
    return ambient_suspended;
  }
  void SetSuspendedAmbientSound(std::int16_t sound) override {
    suspended_ambient = sound;
  }
  void PlaySfx(std::uint32_t group, int effect, int pan, int vol) override {
    Log("PlaySfx", group, effect, pan, vol);
  }
  void PlaySfxCentered(std::uint32_t group, int effect) override {
    Log("PlaySfxCentered", group, effect);
  }
  void FadeSfxVolume(std::uint32_t group, int effect, int vol,
                     int ticks) override {
    Log("FadeSfxVolume", group, effect, vol, ticks);
  }
  void FadeSfxPitch(std::uint32_t group, int effect, int pitch,
                    int ticks) override {
    Log("FadeSfxPitch", group, effect, pitch, ticks);
  }
  void FadeSfxPan(std::uint32_t group, int effect, int pan,
                  int ticks) override {
    Log("FadeSfxPan", group, effect, pan, ticks);
  }
  void StopSfx(std::uint32_t group, int effect) override {
    Log("StopSfx", group, effect);
  }
  void StopAllSfx() override { Log("StopAllSfx"); }
  void PlayActorSound(int actor, int sound) override {
    Log("PlayActorSound", actor, sound);
  }
  void LoadSfxBank(int bank, unsigned slot) override {
    Log("LoadSfxBank", bank, slot);
  }
  [[nodiscard]] int PollSfxBankLoad(unsigned slot) override {
    Log("PollSfxBankLoad", slot);
    return sfx_poll;
  }
  void FreeSfxBank(unsigned slot) override { Log("FreeSfxBank", slot); }
  void SelectSfxBank(int slot) override { Log("SelectSfxBank", slot); }
  void LoadWaveBank(int bank) override { Log("LoadWaveBank", bank); }
  [[nodiscard]] int UpdateWaveBankLoad() override { return wave_poll; }
  [[nodiscard]] int ZoneSoundFile() override { return zone_sound; }
  void LoadMusic(int music, int slot) override {
    Log("LoadMusic", music, slot);
  }
  [[nodiscard]] int PollMusicLoad(unsigned slot) override {
    Log("PollMusicLoad", slot);
    return music_poll;
  }
  void FreeMusic(int slot) override { Log("FreeMusic", slot); }
  void PlayMusic(int slot, int vol, int ticks) override {
    Log("PlayMusic", slot, vol, ticks);
  }
  void CrossfadeMusic(int slot, int vol, unsigned ticks) override {
    Log("CrossfadeMusic", slot, vol, ticks);
  }
  void ResumeMusic(int slot) override { Log("ResumeMusic", slot); }
  void FadeMusicVolume(int slot, int vol, int ticks) override {
    Log("FadeMusicVolume", slot, vol, ticks);
  }
  void FadeMusicTempo(int slot, int scale, int ticks) override {
    Log("FadeMusicTempo", slot, scale, ticks);
  }
  void FadeMusicPitch(int slot, int scale, int ticks) override {
    Log("FadeMusicPitch", slot, scale, ticks);
  }
  void StopMusic(int slot) override { Log("StopMusic", slot); }
  [[nodiscard]] int QueryMusicVolume(int /*slot*/) override {
    return queried_volume;
  }
  [[nodiscard]] int MusicVolume(int /*slot*/) override { return volume; }
  [[nodiscard]] int MusicBeatPosition() override { return beat; }
  void SaveMusicState() override { Log("SaveMusicState"); }
  void SetMusicJumpThreshold(int threshold) override {
    Log("SetMusicJumpThreshold", threshold);
  }
};

using Handler = ScriptStep (*)(event::ScriptCommand, event::ScriptAudioHost&);

ScriptStep Run(Handler handler, const Bytes& bytes, RecordingHost& host) {
  return handler(event::ScriptCommand{bytes}, host);
}

using Step = VS::test_support::ScriptStepCase<event::ScriptAudioHost>;
using Results = std::vector<ScriptStep>;

Results RunSteps(const std::vector<Step>& steps, RecordingHost& host) {
  return VS::test_support::RunScriptSteps<event::ScriptAudioHost>(steps, host);
}

struct Command {
  Handler handler;
  std::size_t length;
};

constexpr std::array kCommands{
    Command{event::ScriptPlaySoundCommand, event::kPlaySoundLength},
    Command{event::CommandPlayActorSfx, event::kPlayActorSfxLength},
    Command{event::CommandSlideSfxVolume, event::kSlideSfxVolumeLength},
    Command{event::CommandStopAllSfx, event::kStopAllSfxLength},
    Command{event::CommandStopSfx, event::kStopSfxLength},
    Command{event::CommandLoadSfxSlot, event::kLoadSfxSlotLength},
    Command{event::CommandWaitSfxSlotRelease, event::kWaitSfxSlotReleaseLength},
    Command{event::CommandFreeSfx, event::kFreeSfxLength},
    Command{event::CommandSelectSfxBank, event::kSelectSfxBankLength},
    Command{event::CommandLoadEventMusic, event::kLoadEventMusicLength},
    Command{event::CommandCrossfadeMusic, event::kCrossfadeMusicLength},
    Command{event::CommandLoadMusicSlot, event::kLoadMusicSlotLength},
    Command{event::CommandFreeMusic, event::kFreeMusicLength},
    Command{event::CommandPlayMusic, event::kPlayMusicLength},
    Command{event::CommandSlideMusicVolume, event::kSlideMusicVolumeLength},
    Command{event::CommandSlideMusicTempo, event::kSlideMusicTempoLength},
    Command{event::CommandSlideMusicPitch, event::kSlideMusicPitchLength},
    Command{event::CommandStopMusic, event::kStopMusicLength},
    Command{event::CommandWaitMusicSlot, event::kWaitMusicSlotLength},
    Command{event::CommandPushMusicState, event::kPushMusicStateLength},
    Command{event::CommandWaitMusicSlotLoad, event::kWaitMusicSlotLoadLength},
    Command{event::CommandLoadSoundFile, event::kLoadSoundFileLength},
    Command{event::CommandWaitSoundQueue, event::kWaitSoundQueueLength},
    Command{event::CommandWaitMusicTimer, event::kWaitMusicTimerLength},
    Command{event::CommandSlideSfxPitch, event::kSlideSfxPitchLength},
    Command{event::CommandSlideSfxPan, event::kSlideSfxPanLength},
    Command{event::CommandResumeMusicSlot, event::kResumeMusicSlotLength},
    Command{event::CommandSetMusicJumpThreshold,
            event::kSetMusicJumpThresholdLength},
    Command{event::CommandPlayMenuSelectSfx, event::kPlayMenuSelectSfxLength},
};
}  // namespace

TEST_CASE("Event script sound commands refuse short instructions",
          "[event-script-audio]") {
  for (const Command& command : kCommands) {
    RecordingHost host;
    const Bytes bytes(command.length - 1, 0x01);
    CHECK(Run(command.handler, bytes, host) == ScriptStep::kAdvance);
    CHECK(host.calls.empty());
    REQUIRE(host.refusals.size() == 1);
    CHECK(host.refusals[0] == Refusal{ScriptAudioRefusal::kShortCommand,
                                      static_cast<int>(command.length - 1)});
  }
}

TEST_CASE("Event script sound effects play on their source's channel group",
          "[event-script-audio]") {
  RecordingHost host;
  CHECK(Run(event::ScriptPlaySoundCommand, {0x80, 3, 0x21, 0x40, 0x7F}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandSlideSfxVolume, {0x82, 0, 9, 0x30, 12}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandStopSfx, {0x84, 5, 7}, host) == ScriptStep::kAdvance);
  CHECK(Run(event::CommandSlideSfxPitch, {0x9B, 4, 2, 0xF0, 6}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandSlideSfxPan, {0x9C, 1, 2, 0x80, 6}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandPlayActorSfx, {0x81, 0x34, 0x12, 9}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandPlayMenuSelectSfx, {0xF2, 9, 9, 9, 9}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"PlaySfx 1044480 33 64 127",
                            "FadeSfxVolume 1 9 48 12", "StopSfx 16777215 7",
                            "FadeSfxPitch 15728640 2 -16 6",
                            "FadeSfxPan 126 2 128 6", "PlayActorSound 4760 9",
                            "PlaySfxCentered 126 5"});
  CHECK(host.refusals.empty());
}

TEST_CASE("Event script ambient sounds play only while the ambience waits",
          "[event-script-audio]") {
  RecordingHost host;
  const Bytes ambient{0x80, event::kAmbientSfxSource, 0x44, 1, 2};
  CHECK(Run(event::ScriptPlaySoundCommand, ambient, host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls.empty());
  CHECK(host.suspended_ambient == -1);

  host.ambient_suspended = true;
  CHECK(Run(event::ScriptPlaySoundCommand, ambient, host) ==
        ScriptStep::kAdvance);
  CHECK(host.suspended_ambient == 0x44);
  CHECK(host.calls == Calls{"PlaySfx 512 68 1 2"});
}

// #307: the original indexed its channel group table with the script's byte.
TEST_CASE("Event script sound effects refuse sources past the groups",
          "[event-script-audio]") {
  const auto source =
      static_cast<std::uint8_t>(event::kScriptSfxChannelGroups.size());
  const std::array<std::pair<Handler, Bytes>, 5> commands{{
      {event::ScriptPlaySoundCommand, {0x80, source, 1, 2, 3}},
      {event::CommandSlideSfxVolume, {0x82, source, 1, 2, 3}},
      {event::CommandStopSfx, {0x84, source, 1}},
      {event::CommandSlideSfxPitch, {0x9B, source, 1, 2, 3}},
      {event::CommandSlideSfxPan, {0x9C, 0xFF, 1, 2, 3}},
  }};
  for (const auto& [handler, bytes] : commands) {
    RecordingHost host;
    host.ambient_suspended = true;
    CHECK(Run(handler, bytes, host) == ScriptStep::kAdvance);
    CHECK(host.calls.empty());
    REQUIRE(host.refusals.size() == 1);
    CHECK(host.refusals[0] ==
          Refusal{ScriptAudioRefusal::kSfxSource, bytes[1]});
  }
}

TEST_CASE("Event script music commands address the event music slot",
          "[event-script-audio]") {
  RecordingHost host;
  host.music_slot = 2;
  host.zone_sound = 0x1234;
  CHECK(Run(event::CommandCrossfadeMusic, {0x8F, 1, 0x7F, 0x90, 30}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.music_slot == 1);
  CHECK(RunSteps({{event::CommandSlideMusicVolume, {0x93, 0x40, 8}},
                  {event::CommandSlideMusicTempo, {0x94, 0xF6, 8}},
                  {event::CommandSlideMusicPitch, {0x95, 0x0A, 8}},
                  {event::CommandPlayMusic, {0x92, 3, 0x60, 4}},
                  {event::CommandStopMusic, {0x96}},
                  {event::CommandResumeMusicSlot, {0x9F, 0}}},
                 host) == Results(6, ScriptStep::kAdvance));
  CHECK(host.music_slot == 0);
  CHECK(RunSteps({{event::CommandLoadEventMusic, {0x89}},
                  {event::CommandLoadMusicSlot, {0x90, 44, 1}},
                  {event::CommandSetMusicJumpThreshold, {0xA8, 5}}},
                 host) == Results(3, ScriptStep::kAdvance));
  CHECK(host.calls ==
        Calls{"FadeMusicVolume 2 0 30", "CrossfadeMusic 1 127 144",
              "FadeMusicVolume 1 64 8", "FadeMusicTempo 1 -10 8",
              "FadeMusicPitch 1 10 8", "PlayMusic 3 96 4", "StopMusic 3",
              "ResumeMusic 0", "LoadMusic 4660 3", "LoadWaveBank 4660",
              "LoadMusic 44 1", "SetMusicJumpThreshold 5"});
}

TEST_CASE("Event script sound waits hold until their condition clears",
          "[event-script-audio]") {
  RecordingHost host;
  host.sfx_poll = 2;
  CHECK(Run(event::CommandWaitSfxSlotRelease, {0x86, 1}, host) ==
        ScriptStep::kHold);
  host.sfx_poll = 0;
  CHECK(Run(event::CommandWaitSfxSlotRelease, {0x86, 1}, host) ==
        ScriptStep::kAdvance);
  host.music_poll = 1;
  CHECK(Run(event::CommandWaitMusicSlotLoad, {0x99, 2}, host) ==
        ScriptStep::kHold);
  host.music_poll = 2;
  CHECK(Run(event::CommandWaitMusicSlotLoad, {0x99, 2}, host) ==
        ScriptStep::kAdvance);
  host.wave_poll = 1;
  CHECK(Run(event::CommandWaitSoundQueue, {0x9E}, host) == ScriptStep::kHold);
  host.wave_poll = 0;
  CHECK(Run(event::CommandWaitSoundQueue, {0x9E}, host) ==
        ScriptStep::kAdvance);
  host.beat = (3 << 16) | 0xFFFF;
  CHECK(Run(event::CommandWaitMusicTimer, {0x9A, 4, 0}, host) ==
        ScriptStep::kHold);
  CHECK(Run(event::CommandWaitMusicTimer, {0x9A, 3, 0}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"PollSfxBankLoad 1", "PollSfxBankLoad 1",
                            "PollMusicLoad 2", "PollMusicLoad 2"});
}

TEST_CASE("Event script music waits stop a slot once it falls silent",
          "[event-script-audio]") {
  RecordingHost host;
  CHECK(Run(event::CommandWaitMusicSlot, {0x97, 2}, host) ==
        ScriptStep::kAdvance);
  host.queried_volume = 1;
  host.volume = 5;
  CHECK(Run(event::CommandWaitMusicSlot, {0x97, 2}, host) == ScriptStep::kHold);
  host.volume = 0;
  CHECK(Run(event::CommandWaitMusicSlot, {0x97, 2}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"StopMusic 2"});
}

TEST_CASE("Event script sound releases end the frame during a skip",
          "[event-script-audio]") {
  RecordingHost host;
  CHECK(RunSteps({{event::CommandFreeSfx, {0x87, 1}},
                  {event::CommandFreeMusic, {0x91, 2}}},
                 host) == Results(2, ScriptStep::kAdvance));
  host.skip = true;
  CHECK(RunSteps({{event::CommandFreeSfx, {0x87, 3}},
                  {event::CommandFreeMusic, {0x91, 4}}},
                 host) == Results(2, ScriptStep::kAdvanceAndYield));
  CHECK(RunSteps({{event::CommandLoadSfxSlot, {0x85, 7, 1}},
                  {event::CommandSelectSfxBank, {0x88, 1}},
                  {event::CommandLoadSoundFile, {0x9D, 9}},
                  {event::CommandStopAllSfx, {0x83}},
                  {event::CommandPushMusicState, {0x98}}},
                 host) == Results(5, ScriptStep::kAdvance));
  CHECK(host.calls == Calls{"FreeSfxBank 1", "FreeMusic 2", "FreeSfxBank 3",
                            "FreeMusic 4", "LoadSfxBank 7 1", "SelectSfxBank 1",
                            "LoadWaveBank 9", "StopAllSfx", "SaveMusicState"});
}
