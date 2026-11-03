#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Audio/Mixer/MixControl.h"
#include "Audio/Seq/Channel.h"
}

namespace {
namespace Seq = VS::Audio::Seq;
using Seq::ChannelGroup;
using Seq::kUnassignedVoice;
using Seq::MusicCommand;
using Seq::MusicCommandState;
using Instrument = VS::Audio::Samples::InstrumentRecord;

constexpr Instrument kFirstInstrument{0x2000, 0x2100, 0, 60, 0x80FF, 0x1FC0};
constexpr std::int32_t kMinimum = std::numeric_limits<std::int32_t>::min();

void PutU32(std::vector<std::uint8_t>& data, std::size_t at,
            std::uint32_t value) {
  for (std::size_t i = 0; i < sizeof value; ++i) {
    data[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
  }
}

void PutU16(std::vector<std::uint8_t>& data, std::size_t at,
            std::uint16_t value) {
  data[at] = static_cast<std::uint8_t>(value);
  data[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

// A track on sample bank 7 enabling channels 0 and 2, the first of them
// allocated a voice; channel 2's program starts past the end of the data.
std::vector<std::uint8_t> TwoChannelSequence() {
  std::vector<std::uint8_t> data(0x50);
  PutU32(data, 0x14, 7);
  PutU32(data, 0x20, 0b101);
  PutU32(data, 0x24, 0x11);
  PutU32(data, 0x28, 0b001);
  PutU32(data, 0x30, 0x10);
  PutU32(data, 0x38, 0x22);
  PutU32(data, 0x3C, 0x33);
  PutU16(data, 0x40, 4);
  PutU16(data, 0x42, 0x100);
  return data;
}
}  // namespace

TEST_CASE("A sequence header is read only from complete header data",
          "[audio-seq-control][audio]") {
  std::vector<std::uint8_t> data = TwoChannelSequence();
  PutU32(data, 0x34, 0xFFFFFFF0U);
  CHECK(Seq::ReadSequenceHeader(data) ==
        Seq::SequenceHeader{7, 0b101, 0x11, 0b001, 0x10, -0x10, 0x22, 0x33});
  CHECK_FALSE(Seq::ReadSequenceHeader(std::span(data).first(0x3F)));
}

namespace {
// TwoChannelSequence loaded over a layer, scheduler and channels in use.
struct LoadedTrack {
  Seq::MusicStartState state;
  Seq::LoadResult result;
};

LoadedTrack LoadTwoChannelTrack() {
  const std::vector<std::uint8_t> data = TwoChannelSequence();
  LoadedTrack loaded;
  Seq::MusicStartState& state = loaded.state;
  state.alternate_bank_id = 7;
  state.layer.status_flags = 0x61 | 0x100;
  state.layer.pending_key_off = 5;
  state.scheduler.active_voices = 0xF00000;
  state.stream_voices = 0x0F0000;
  state.secondary_voices = 0x00F000;
  state.channels[0].assigned_voice = 3;
  loaded.result = Seq::LoadMusicSequence(state, data, kFirstInstrument);
  return loaded;
}
}  // namespace

TEST_CASE("Loading a track reports its tables and the channels outside it",
          "[audio-seq-control][audio]") {
  const LoadedTrack loaded = LoadTwoChannelTrack();
  CHECK(loaded.result.status == Seq::LoadStatus::kLoaded);
  CHECK(loaded.result.tables.patch_table == std::optional<std::int64_t>{0x40});
  CHECK_FALSE(loaded.result.tables.keymap_table);
  CHECK(loaded.result.channels_outside == 0b100);
}

TEST_CASE("Loading a track takes the layer's voice masks from its header",
          "[audio-seq-control][audio]") {
  const Seq::MusicLayerState layer = LoadTwoChannelTrack().state.layer;
  CHECK(layer.active_channels == 0b101);
  CHECK(layer.keyed_voices == 0x11);
  CHECK(layer.allocated_voices == 0b001);
  CHECK(layer.resume_key_on_mask == 0x22);
  CHECK(layer.push_release_mask == 0x33);
}

TEST_CASE("Loading a track resets the layer's flags, tempo and key-offs",
          "[audio-seq-control][audio]") {
  const Seq::MusicStartState state = LoadTwoChannelTrack().state;
  CHECK(state.layer.status_flags == (0x100 | 0x40));
  CHECK(state.layer.pending_key_off == 0);
  CHECK(state.layer.tempo == static_cast<std::int32_t>(0xFFFF0000U));
  CHECK(state.layer.tick_accumulator == 1);
  CHECK(state.driver_update_flags == 0x100);
  CHECK(state.scheduler.pending_key_off == 0x000FFF);
}

TEST_CASE("Loading a track starts its enabled channels inside the sequence",
          "[audio-seq-control][audio]") {
  const LoadedTrack loaded = LoadTwoChannelTrack();
  const Seq::SequenceChannel& started = loaded.state.channels[0];
  CHECK_FALSE(started.idle);
  CHECK(started.program.counter == 0x44);
  CHECK(started.channel.update_flags == 0);
  CHECK(started.channel.voice.sample_start == 0x2000);
  CHECK(started.assigned_voice == kUnassignedVoice);
}

TEST_CASE("A loaded channel starts at the default lengths, volume and pan",
          "[audio-seq-control][audio]") {
  const LoadedTrack loaded = LoadTwoChannelTrack();
  const Seq::SequenceChannel& started = loaded.state.channels[0];
  CHECK(started.channel.note_length == 4);
  CHECK(started.channel.gate_length == 2);
  CHECK(started.channel.volume == 0x3FFF0000);
  CHECK(started.channel.pan == 0x8000);
}

TEST_CASE("Loading a track idles channels it disables or that lie outside",
          "[audio-seq-control][audio]") {
  const LoadedTrack loaded = LoadTwoChannelTrack();
  const Seq::SequenceChannel& outside = loaded.state.channels[2];
  CHECK(outside.idle);
  CHECK(outside.program.counter == Seq::kOutsideProgram);
  CHECK(outside.channel.update_flags == Seq::kFullNoteLengthActive);

  const Seq::SequenceChannel& disabled = loaded.state.channels[1];
  CHECK(disabled.idle);
  CHECK(disabled.channel.note_length == 3);
  CHECK(disabled.channel.gate_length == 1);
  CHECK((disabled.channel.voice.dirty_flags & 0x4400) == 0x4400);
}

TEST_CASE("A track too short for its channel offsets changes nothing",
          "[audio-seq-control][audio]") {
  std::vector<std::uint8_t> data = TwoChannelSequence();
  data.resize(0x43);
  Seq::MusicStartState state;
  state.layer.active_channels = 0x8;
  const Seq::MusicStartState before = state;
  CHECK(Seq::LoadMusicSequence(state, data, kFirstInstrument).status ==
        Seq::LoadStatus::kHeaderOutside);
  CHECK(Seq::LoadMusicSequence(state, std::span(data).first(0x3F),
                               kFirstInstrument)
            .status == Seq::LoadStatus::kHeaderOutside);
  CHECK(state == before);
}

namespace {
// A layer playing track 5 on channels 0 and 1, one note waiting to key on.
Seq::MusicLayerState LayerPlayingTrackFive() {
  Seq::MusicLayerState layer;
  layer.active_channels = 0x3;
  layer.music_id = 5;
  layer.active_notes = 0x3;
  layer.pending_key_on = 0x1;
  return layer;
}
}  // namespace

TEST_CASE("Releasing a music layer ignores layers playing another track",
          "[audio-seq-control][audio]") {
  Seq::MusicLayerState idle;
  Seq::MusicChannels channels{};
  CHECK_FALSE(Seq::ReleaseMusicConfiguration(idle, channels, 0xFF, 0));
  Seq::MusicLayerState layer = LayerPlayingTrackFive();
  CHECK_FALSE(Seq::ReleaseMusicConfiguration(layer, channels, 0xFF, 6));
  CHECK(layer.music_id == 5);
}

TEST_CASE("Releasing a music layer stops only the track it names",
          "[audio-seq-control][audio]") {
  Seq::MusicLayerState layer = LayerPlayingTrackFive();
  Seq::MusicChannels channels{};
  CHECK(Seq::ReleaseMusicConfiguration(layer, channels, 0xFF, 5) ==
        std::optional<std::uint32_t>{0xFF});
  CHECK(layer.pending_key_off == -1);
  CHECK(layer.music_id == 0);
  CHECK(layer.active_notes == 0);
  CHECK(layer.pending_key_on == 0);
}

TEST_CASE("Releasing a music layer idles its channels at the default lengths",
          "[audio-seq-control][audio]") {
  Seq::MusicLayerState layer = LayerPlayingTrackFive();
  Seq::MusicChannels channels{};
  REQUIRE(Seq::ReleaseMusicConfiguration(layer, channels, 0xFF, 5));
  CHECK(channels[31].idle);
  CHECK(channels[31].channel.note_length == 3);
  CHECK(channels[31].channel.gate_length == 1);
}

namespace {
// A track on sample bank 5 playing channels 0 and 1 (channel 0 held at 0x80
// with a voice), restored from a header on the same bank.
Seq::MusicStartState RestoredTrack() {
  Seq::MusicStartState state;
  state.alternate_bank_id = 5;
  state.saved_music_id = 9;
  state.layer.status_flags = 0x61;
  state.layer.active_channels = 0b11;
  state.layer.active_notes = 0b10;
  state.layer.pending_key_off = 4;
  state.channels[0].program.counter = 0x80;
  state.channels[0].channel.note_length = 10;
  state.channels[0].channel.gate_length = 8;
  state.channels[0].assigned_voice = 3;
  state.channels[2].channel.note_length = 7;
  Seq::SequenceHeader header;
  header.sample_bank_id = 5;
  CHECK(Seq::RestoreMusicSequence(state, header, false) == 0b11);
  return state;
}
}  // namespace

TEST_CASE("Restoring a track resumes its layer's sounding notes",
          "[audio-seq-control][audio]") {
  const Seq::MusicStartState state = RestoredTrack();
  CHECK(state.layer.status_flags == (0x1 | 0x40));
  CHECK(state.layer.pending_key_on == 0b10);
  CHECK(state.layer.pending_key_off == 0);
  CHECK(state.saved_music_id == 0);
  CHECK(state.driver_update_flags == 0x190);
}

TEST_CASE("Restoring a track keeps its playing channels' positions",
          "[audio-seq-control][audio]") {
  const Seq::SequenceChannel channel = RestoredTrack().channels[0];
  CHECK(channel.program.counter == 0x80);
  CHECK(channel.channel.note_length == 12);
  CHECK(channel.channel.gate_length == 10);
  CHECK((channel.channel.voice.dirty_flags & 0x1FF93) == 0x1FF93);
  CHECK_FALSE(channel.idle);
  CHECK(channel.assigned_voice == kUnassignedVoice);
}

TEST_CASE("Restoring a track idles its other channels at the default lengths",
          "[audio-seq-control][audio]") {
  const Seq::SequenceChannel channel = RestoredTrack().channels[2];
  CHECK(channel.idle);
  CHECK(channel.channel.note_length == 4);
  CHECK(channel.channel.gate_length == 2);
}

TEST_CASE("A swapped-in track's reverb slide wraps at 32 bits",
          "[audio-seq-control][audio]") {
  // The original subtracts and divides the reverb depths as 32-bit words.
  Seq::MusicStartState state;
  state.layer.active_channels = 0x4;
  state.layer.reverb_depth = 0x7FF00000;
  state.secondary_reverb_depth = static_cast<std::int32_t>(0x80100000U);
  state.stream_control_flags = 1;
  CHECK(Seq::RestoreMusicSequence(state, {}, true) == 0x4);
  CHECK(state.layer.reverb_depth == static_cast<std::int32_t>(0x80100000U));
  CHECK(state.layer.reverb_depth_slide_step == -43690);
  CHECK(state.layer.reverb_depth_slide_length == 0x30);
  CHECK(state.layer.active_channels == 0);
  CHECK(state.layer.paused_channels == 0x4);

  // Without a change in depth no slide runs.
  Seq::MusicStartState same;
  same.layer.reverb_depth = 0x1000;
  same.layer.reverb_depth_slide_length = 3;
  same.secondary_reverb_depth = 0x1000;
  Seq::RestoreMusicSequence(same, {}, true);
  CHECK(same.layer.reverb_depth_slide_length == 0);
}

namespace {
// A busy sound-effect channel reset onto the program at 0x24.
Seq::SequenceChannel RestartedEffectChannel() {
  Seq::SequenceChannel channel;
  channel.idle = true;
  channel.channel.update_flags = 0xFF;
  channel.channel.instrument_index = 9;
  channel.channel.voice.volume_scale = 0x40;
  channel.program.loop_depth = 3;
  Seq::ResetSoundEffectChannel(channel, 0x24, kFirstInstrument);
  return channel;
}
}  // namespace

TEST_CASE("A sound-effect channel restarts on its program with instrument 0",
          "[audio-seq-control][audio]") {
  const Seq::SequenceChannel channel = RestartedEffectChannel();
  CHECK_FALSE(channel.idle);
  CHECK(channel.program.counter == 0x24);
  CHECK(channel.program.loop_depth == 0);
  CHECK(channel.program.step_counter == 0xFFFF);
  CHECK(channel.channel.instrument_index == 0);
  CHECK(channel.channel.voice.sample_start == 0x2000);
  CHECK(channel.channel.voice.adsr_upper == 0x1FC0);
}

TEST_CASE("A restarted sound-effect channel takes the default volume",
          "[audio-seq-control][audio]") {
  const Seq::SequenceChannel channel = RestartedEffectChannel();
  CHECK(channel.channel.volume == 0x32000000);
  CHECK(channel.channel.volume_balance == 0x6E00);
  CHECK(channel.channel.update_flags == 0);
  CHECK(channel.channel.voice.volume_scale == 0);
}

TEST_CASE("Assigned voice masks count only channels holding a voice",
          "[audio-seq-control][audio]") {
  std::array<std::uint32_t, Seq::kMusicChannelCount> voices{};
  voices.fill(kUnassignedVoice);
  voices[0] = 3;
  voices[2] = 5;
  voices[3] = 7;
  CHECK(Seq::AssignedVoiceMask(voices, 0b0111) == ((1U << 3) | (1U << 5)));
  CHECK(Seq::AssignedVoiceMask(voices, 0) == 0);
}

namespace {
enum class Call : std::uint8_t {
  kLoad,
  kRestore,
  kRestoreSwapped,
  kSave,
  kMapToBaseBank,
  kAttach,
  kReleaseConflicts,
  kInvalidateActive,
  kInvalidateSecondary,
  kWriteDisc,
};
using Calls = std::vector<Call>;

// Records the music driver effects; a load plays one channel and an attach
// makes an empty secondary layer.
class FakeMusicHost final : public Seq::MusicHost {
 public:
  void LoadSequence(MusicCommandState& state) override {
    calls.push_back(Call::kLoad);
    state.active.active_channels = 1;
  }
  void RestoreSequence(MusicCommandState& /*state*/,
                       bool swap_with_saved) override {
    calls.push_back(swap_with_saved ? Call::kRestoreSwapped : Call::kRestore);
  }
  void SaveSnapshot(MusicCommandState& /*state*/) override {
    calls.push_back(Call::kSave);
  }
  void MapSavedInstrumentsToBaseBank(MusicCommandState& /*state*/) override {
    calls.push_back(Call::kMapToBaseBank);
  }
  void AttachSavedLayer(MusicCommandState& state) override {
    calls.push_back(Call::kAttach);
    state.secondary = Seq::MusicLayerState{};
  }
  void ReleaseSavedVoiceConflicts(MusicCommandState& /*state*/) override {
    calls.push_back(Call::kReleaseConflicts);
  }
  void InvalidateMusicVolumes(MusicCommandState& /*state*/,
                              ChannelGroup group) override {
    calls.push_back(group == ChannelGroup::kMusic ? Call::kInvalidateActive
                                                  : Call::kInvalidateSecondary);
  }
  void WriteDiscVolume(MusicCommandState& /*state*/) override {
    calls.push_back(Call::kWriteDisc);
  }
  void Refuse(Seq::MusicRefusal refusal) override {
    refusals.push_back(refusal);
  }

  Calls calls;
  std::vector<Seq::MusicRefusal> refusals;
};
}  // namespace

TEST_CASE("Starting music resumes the saved track or loads a new one",
          "[audio-seq-control][audio]") {
  FakeMusicHost host;
  MusicCommandState state;
  state.saved_music_id = 7;
  Seq::StartOrResumeMusic(state, host, MusicCommand{.param3 = 7});
  CHECK(host.calls == Calls{Call::kRestore});
  CHECK(state.active.music_id == 0);

  host.calls.clear();
  Seq::StartMusicWithLoopCounter(state, host,
                                 MusicCommand{.param3 = 9, .param4 = 3});
  CHECK(host.calls == Calls{Call::kLoad});
  CHECK(state.active.music_id == 9);
  CHECK(state.music_loops_remaining == 2);
  Seq::StartMusicWithLoopCounter(state, host, MusicCommand{.param3 = 9});
  CHECK(state.music_loops_remaining == 0);

  host.calls.clear();
  Seq::SaveCurrentMusic(state, host);
  CHECK(host.calls == Calls{Call::kSave, Call::kMapToBaseBank});
  state.active.active_channels = 0;
  host.calls.clear();
  Seq::SaveCurrentMusic(state, host);
  CHECK(host.calls.empty());
}

TEST_CASE("Starting a music layer keeps the current track beside it",
          "[audio-seq-control][audio]") {
  FakeMusicHost host;
  MusicCommandState state;
  state.active.active_channels = 0x3;
  state.active.master_volume_slide_length = 5;
  state.mix_behavior = 0x101;
  Seq::StartMusicLayer(state, host,
                       MusicCommand{.param3 = 4, .ext_param1 = 0x1C5});
  CHECK(host.calls == Calls{Call::kAttach, Call::kSave, Call::kLoad,
                            Call::kReleaseConflicts});
  CHECK(state.active.master_volume == 0x450000);
  CHECK(state.active.master_volume_slide_length == 0);
  CHECK(state.active.music_id == 4);
  CHECK(state.mix_behavior == 0x1);

  // A secondary layer that still plays a track is not replaced.
  REQUIRE(state.secondary.has_value());
  Seq::MusicLayerState playing;
  playing.music_id = 4;
  state.secondary = playing;
  host.calls.clear();
  Seq::StartMusicLayer(state, host, MusicCommand{.param3 = 6});
  CHECK(host.calls == Calls{Call::kLoad, Call::kReleaseConflicts});
}

TEST_CASE("A crossfade fades the secondary layer out over its ticks",
          "[audio-seq-control][audio]") {
  FakeMusicHost host;
  MusicCommandState state;
  state.active.active_channels = 1;
  state.secondary.emplace().music_id = 3;
  Seq::CrossfadeMusicLayers(
      state, host,
      MusicCommand{.param3 = 8, .ext_param1 = 0x10, .ext_param2 = 0x7F});
  CHECK(host.calls == Calls{Call::kLoad, Call::kReleaseConflicts});
  CHECK(host.refusals.empty());
  CHECK(state.active.music_id == 8);
  CHECK(state.master_fade == Seq::FadeTimer{0x7F0000, -0x7F000, 0x10});
  CHECK(state.mix_behavior == 0x100);
}

TEST_CASE("A crossfade fades the new track in over its ticks",
          "[audio-seq-control][audio]") {
  FakeMusicHost host;
  MusicCommandState state;
  state.active.active_channels = 1;
  state.secondary.emplace().music_id = 3;
  Seq::CrossfadeMusicLayers(
      state, host,
      MusicCommand{.param3 = 8, .ext_param1 = 0x10, .ext_param2 = 0x7F});
  CHECK(state.active.master_volume == 0);
  CHECK(state.active.master_volume_slide_length == 0x10);
  CHECK(state.active.master_volume_step == 0x7F000);
}

TEST_CASE("A crossfade to the saved track resumes it beside the current one",
          "[audio-seq-control][audio]") {
  FakeMusicHost saved_host;
  MusicCommandState saved;
  saved.saved_music_id = 4;
  Seq::CrossfadeMusicLayers(saved, saved_host,
                            MusicCommand{.param3 = 4, .ext_param1 = 2});
  CHECK(saved_host.calls ==
        Calls{Call::kAttach, Call::kRestoreSwapped, Call::kReleaseConflicts});
  CHECK(saved.master_fade == Seq::FadeTimer{0x7F0000, -0x3F8000, 2});
}

TEST_CASE("A zero-length crossfade is refused and runs over one tick",
          "[audio-seq-control][audio]") {
  FakeMusicHost host;
  MusicCommandState state;
  state.mix_behavior = 0x100;
  Seq::CrossfadeMusicLayers(state, host,
                            MusicCommand{.param3 = 8, .ext_param2 = 0x40});
  CHECK(host.refusals == std::vector{Seq::MusicRefusal::kZeroLengthCrossfade});
  CHECK(host.calls == Calls{Call::kLoad, Call::kReleaseConflicts});
  CHECK(state.mix_behavior == 0);
  CHECK(state.master_fade == Seq::FadeTimer{});
  CHECK(state.active.master_volume_slide_length == 1);
  CHECK(state.active.master_volume_step == 0x400000);
}

namespace {
// Track 2 on the active layer and track 3 on the secondary one.
MusicCommandState TwoLayerVolumes() {
  MusicCommandState state;
  state.active.music_id = 2;
  state.active.master_volume = 0x100000;
  state.secondary.emplace().music_id = 3;
  return state;
}

// The secondary layer, or an empty layer when there is none.
Seq::MusicLayerState SecondaryOf(const MusicCommandState& state) {
  return state.secondary.value_or(Seq::MusicLayerState{});
}
}  // namespace

TEST_CASE("Music volume commands change the layer playing the track",
          "[audio-seq-control][audio]") {
  FakeMusicHost host;
  MusicCommandState state = TwoLayerVolumes();
  Seq::SetMusicVolume(state, host, MusicCommand{.param1 = 3, .param2 = 0x40});
  CHECK(SecondaryOf(state).master_volume == 0x400000);
  Seq::SetMusicVolume(state, host, MusicCommand{.param1 = 9, .param2 = 0x40});
  CHECK(host.calls == Calls{Call::kInvalidateSecondary});
}

TEST_CASE("Music volume fades slide the active layer toward their target",
          "[audio-seq-control][audio]") {
  FakeMusicHost host;
  MusicCommandState state = TwoLayerVolumes();
  Seq::FadeMusicVolume(state, host, MusicCommand{.param3 = 0x7F});
  CHECK(state.active.master_volume_slide_length == 1);
  CHECK(state.active.master_volume_step == 0x6F0000);
  Seq::FadeMusicVolume(state, host, MusicCommand{.param1 = 2, .param2 = 4});
  CHECK(state.active.master_volume_slide_length == 4);
  CHECK(state.active.master_volume_step == -0x40000);
  CHECK(state.active.master_volume == 0x100000);
  CHECK(host.calls == Calls{Call::kInvalidateActive, Call::kInvalidateActive});
}

TEST_CASE("A music fade from a level first sets the layer to that level",
          "[audio-seq-control][audio]") {
  FakeMusicHost host;
  MusicCommandState state = TwoLayerVolumes();
  Seq::FadeMusicVolumeFrom(
      state, host,
      MusicCommand{.param1 = 3, .param2 = 4, .param3 = 0x40, .param4 = 0});
  const Seq::MusicLayerState secondary = SecondaryOf(state);
  CHECK(secondary.master_volume == 0x400000);
  CHECK(secondary.master_volume_step == -0x100000);
  CHECK(secondary.master_volume_slide_length == 4);
  CHECK(host.calls == Calls{Call::kInvalidateSecondary});
}

TEST_CASE("Disc volume fades wrap their step instead of trapping",
          "[audio-seq-control][audio]") {
  FakeMusicHost host;
  MusicCommandState state;
  state.disc_volume.length = 5;
  Seq::SetDiscVolume(state, host, MusicCommand{.param1 = 0x12345});
  CHECK(state.disc_volume == Seq::LevelSlide{0x23450000, 0, 0});
  CHECK(host.calls == Calls{Call::kWriteDisc});

  state.disc_volume.value = 0;
  Seq::FadeDiscVolume(state, MusicCommand{.param2 = 0x8000});
  CHECK(state.disc_volume == Seq::LevelSlide{0, kMinimum, 1});
  // A tick count of -1 divides the minimum by -1, which MIPS leaves as is.
  Seq::FadeDiscVolume(state,
                      MusicCommand{.param1 = 0xFFFFFFFFU, .param2 = 0x8000});
  CHECK(state.disc_volume == Seq::LevelSlide{0, kMinimum, -1});

  Seq::FadeDiscVolumeFrom(
      state, MusicCommand{.param1 = 2, .param2 = 0x10, .param3 = 0x30});
  CHECK(state.disc_volume == Seq::LevelSlide{0x100000, 0x100000, 2});
}

namespace {
// A 16-byte live heap block, too short for an AKAO header, and an active
// music layer, over saved and restored layer pointers.
struct LiveMusicFixture {
  Audio_MusicConfig* saved_config = Audio_Seq_activeMusicConfig;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 256> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Audio_MusicConfig config{};
  Audio_AkaoSequenceFile* sequence = nullptr;

  LiveMusicFixture() {
    Audio_Seq_activeMusicConfig = &config;
    void* const data = VS::Core::Memory::AllocateLiveFromLowEnd(8);
    REQUIRE(data != nullptr);
    sequence = static_cast<Audio_AkaoSequenceFile*>(data);
  }

  ~LiveMusicFixture() { Audio_Seq_activeMusicConfig = saved_config; }

  LiveMusicFixture(const LiveMusicFixture&) = delete;
  LiveMusicFixture& operator=(const LiveMusicFixture&) = delete;
};
}  // namespace

TEST_CASE("Live track loads refuse sequences shorter than their header",
          "[audio-seq-control][audio]") {
  LiveMusicFixture fixture;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Seq::LiveLoadMusicSequence(fixture.sequence);
  CHECK(Seq::LiveRefusalCount() == refused + 1);
  CHECK(fixture.config.sequenceBase == 0);
  Seq::LiveRestoreMusicSequence(fixture.sequence, 0);
  CHECK(Seq::LiveRefusalCount() == refused + 2);
  CHECK(fixture.config.sequenceBase == 0);
  Seq::LiveSetMusicVolume(nullptr);
  CHECK(Seq::LiveRefusalCount() == refused + 3);
  Audio_Seq_activeMusicConfig = nullptr;
  Audio_CommandParams params{};
  Seq::LiveFadeDiscVolume(&params);
  CHECK(Seq::LiveRefusalCount() == refused + 4);
}

TEST_CASE("Music commands keep their live bindings and helpers keep none",
          "[audio-seq-control][audio]") {
  using CommandHandler = void (*)(Audio_CommandParams*);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const std::array<std::pair<std::uint32_t, CommandHandler>, 11> kept{{
      {0x80016DF8U, &Seq::LiveStartOrResumeMusic},
      {0x80016E74U, &Seq::LiveSaveCurrentMusic},
      {0x80016F0CU, &Seq::LiveStartMusicLayer},
      {0x80016FF4U, &Seq::LiveCrossfadeMusicLayers},
      {0x800171C8U, &Seq::LiveStartMusicWithLoopCounter},
      {0x80017498U, &Seq::LiveSetMusicVolume},
      {0x8001754CU, &Seq::LiveFadeMusicVolume},
      {0x8001766CU, &Seq::LiveFadeMusicVolumeFrom},
      {0x80017764U, &Seq::LiveSetDiscVolume},
      {0x80017798U, &Seq::LiveFadeDiscVolume},
      {0x80017800U, &Seq::LiveFadeDiscVolumeFrom},
  }};
  for (const auto& [address, handler] : kept) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          reinterpret_cast<void*>(handler));
  }
  for (const std::uint32_t address :
       {0x80015C9CU, 0x80015D38U, 0x80015D84U, 0x800160F0U, 0x80016A00U}) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          nullptr);
  }
}
