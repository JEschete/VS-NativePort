#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/AudioDevice.hpp"
#include "Audio/SequencerOutput.h"
#include "Audio/StreamPlayback.h"
#include "Audio/VoiceEngine.hpp"
#include "Audio/XaPlayback.h"
#include "Content/AudioBank.hpp"
#include "Content/AudioStream.hpp"
#include "Content/DataStore.hpp"
#include "nlohmann/json.hpp"

extern "C" int TestsZoneMusicStateAliases(void);
extern "C" void TestsNativeNoiseChannel(void);
extern "C" void TestsNativeMonoOutput(int enabled);
extern "C" int TestsMenuSoundDispatch(void);
extern "C" void TestsPrepareStreamPlayback(void);
extern "C" void TestsRestoreStreamPlayback(void);
extern "C" int TestsBindEffectStream(void* bytes);
extern "C" int TestsPlayStreamSlot(int slot, int pan, int volume);
extern "C" void TestsAdvanceStreamTick(void);
extern "C" void TestsStreamControl(int command, int value, int target);
extern "C" unsigned TestsStreamVoiceMask(void);
extern "C" unsigned TestsStreamVolume(void);
extern "C" int TestsStreamSlotIsEmpty(void);
extern "C" void TestsPrepareXaPlayback(void);
extern "C" void TestsRestoreXaPlayback(void);
extern "C" int TestsXaCommand(int command, int first, int second, int third);
extern "C" int TestsSavedMusicVoiceConflicts(void);
extern "C" int TestsNativeMusicSnapshot(void);
extern "C" int TestsPendingModulationSlides(int effect, int kind);

TEST_CASE("Unconfigured modulation slides wait for their first waveform sample",
          "[audio][modulation]") {
  for (int effect = 0; effect < 2; ++effect) {
    for (int kind = 0; kind < 3; ++kind) {
      CAPTURE(effect, kind);
      CHECK(TestsPendingModulationSlides(effect, kind) == 0);
    }
  }
}

TEST_CASE("Music layer conflicts release only unreserved displaced voices",
          "[audio][music-state]") {
  CHECK(TestsSavedMusicVoiceConflicts() == 0);
}

TEST_CASE("Music snapshots preserve native channel arrays and trailing timing",
          "[audio][music-state]") {
  const int result = TestsNativeMusicSnapshot();
  CHECK((result & 1) != 0);
  CHECK((result & 2) != 0);
  CHECK((result & 4) != 0);
  CHECK((result & 8) != 0);
}

TEST_CASE("In-game menu sounds dispatch original IDs and channel tags",
          "[audio][menu-sfx]") {
  const int result = TestsMenuSoundDispatch();
  for (int action = 0; action < 5; ++action) {
    CHECK((result & (1 << action)) != 0);
  }
}

TEST_CASE("Zone music consumers share the initialized canonical state",
          "[audio][music-state]") {
  const int result = TestsZoneMusicStateAliases();
  CHECK((result & 1) != 0);
  CHECK((result & 2) != 0);
  CHECK((result & 4) != 0);
  CHECK((result & 8) != 0);
  CHECK((result & 16) != 0);
  CHECK((result & 32) != 0);
  CHECK((result & 64) != 0);
}

namespace {
using VS::i16;
using VS::u32;
using VS::u64;
using VS::u8;
using VS::usize;
namespace Audio = VS::Audio;
namespace Content = VS::Content;

struct AudioSubsystem {
  std::optional<std::string> previous_driver;
  bool set_dummy = SDL_WasInit(SDL_INIT_AUDIO) == 0;
  bool ready = false;

  AudioSubsystem() {
    if (set_dummy) {
      if (const auto* driver = SDL_GetHint(SDL_HINT_AUDIO_DRIVER)) {
        previous_driver = driver;
      }
      REQUIRE(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    }
    ready = SDL_InitSubSystem(SDL_INIT_AUDIO);
  }

  ~AudioSubsystem() {
    if (ready) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if (set_dummy) {
      SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);
      if (previous_driver &&
          !SDL_SetHint(SDL_HINT_AUDIO_DRIVER, previous_driver->c_str())) {
        std::fputs("audio driver hint could not be restored\n", stderr);
        std::abort();
      }
    }
  }
};

struct AudioDeviceFixture {
  AudioSubsystem subsystem;
  Audio::AudioDevice device;
  std::unique_ptr<SDL_AudioStream, decltype(&Audio::AudioDevice::CloseSource)>
      source{nullptr, Audio::AudioDevice::CloseSource};

  AudioDeviceFixture() {
    REQUIRE(subsystem.ready);
    REQUIRE(device.Open());
    Pause();
  }

  void Pause() {
    source.reset(device.OpenSource(37800, 1));
    REQUIRE(source != nullptr);
    REQUIRE(SDL_PauseAudioStreamDevice(source.get()));
  }

  void Reopen() {
    source.reset();
    device.Close();
    REQUIRE(device.Open());
    Pause();
  }
};

struct OutputProbe {
  std::vector<float> samples =
      std::vector<float>(static_cast<usize>(Audio::AudioDevice::SampleRate) *
                         Audio::AudioDevice::Channels);
  std::unique_ptr<SDL_Semaphore, decltype(&SDL_DestroySemaphore)> completed{
      SDL_CreateSemaphore(0), SDL_DestroySemaphore};
  SDL_AudioDeviceID device = 0;
  usize count = 0;
  bool format_ok = true;

  explicit OutputProbe(SDL_AudioStream* source) {
    REQUIRE(completed != nullptr);
    device = SDL_GetAudioStreamDevice(source);
    REQUIRE(device != 0);
    REQUIRE(SDL_SetAudioPostmixCallback(device, Receive, this));
  }

  ~OutputProbe() {
    if (!SDL_SetAudioPostmixCallback(device, nullptr, nullptr)) {
      std::fputs("audio probe could not detach\n", stderr);
      std::abort();
    }
  }

  static void SDLCALL Receive(void* context, const SDL_AudioSpec* spec,
                              float* data, int bytes) {
    auto& probe = *static_cast<OutputProbe*>(context);
    if (probe.count == probe.samples.size()) return;
    if (spec->freq != Audio::AudioDevice::SampleRate ||
        spec->channels != Audio::AudioDevice::Channels || bytes < 0) {
      probe.format_ok = false;
      SDL_SignalSemaphore(probe.completed.get());
      return;
    }
    const auto amount = std::min(static_cast<usize>(bytes) / sizeof(float),
                                 probe.samples.size() - probe.count);
    std::memcpy(probe.samples.data() + probe.count, data,
                amount * sizeof(float));
    probe.count += amount;
    if (probe.count == probe.samples.size()) {
      SDL_SignalSemaphore(probe.completed.get());
    }
  }

  void Wait() {
    REQUIRE(SDL_ResumeAudioDevice(device));
    const bool completed_in_time =
        SDL_WaitSemaphoreTimeout(completed.get(), 5000);
    REQUIRE(SDL_PauseAudioDevice(device));
    REQUIRE(completed_in_time);
    REQUIRE(format_ok);
    REQUIRE(count == samples.size());
  }
};

constexpr usize kFrameBytes = Audio::AudioDevice::Channels * sizeof(float);
constexpr usize kBufferBytes = Audio::AudioDevice::kBufferFrames * kFrameBytes;

void Put32(std::span<u8> bytes, usize offset, u32 value) {
  for (usize index = 0; index < 4; ++index) {
    bytes[offset + index] = static_cast<u8>(value >> (index * 8));
  }
}

std::array<u8, 112> BankBytes() {
  std::array<u8, 112> bytes{};
  Put32(bytes, 0, 0x4F414B41);
  Put32(bytes, 16, 0x3100);
  Put32(bytes, 20, 32);
  Put32(bytes, 28, 1);
  Put32(bytes, 68, 16);
  bytes[80] = 12;
  bytes[82] = 0x17;
  bytes[96] = 12;
  bytes[97] = 7;
  bytes[98] = 0xF8;
  return bytes;
}

std::array<u8, 160> LegacyBankBytes() {
  std::array<u8, 160> bytes{};
  const auto normal = BankBytes();
  std::copy_n(normal.begin(), 64, bytes.begin());
  Put32(bytes, 68, 16);
  for (usize note = 0; note < 12; ++note) {
    Put32(bytes, 72 + note * 4,
          static_cast<u32>(std::lround(
              1048576 * std::exp2(static_cast<double>(note) / 12.0))));
  }
  const std::array<u8, 8> envelope{0, 15, 15, 127, 7, 1, 3, 3};
  std::ranges::copy(envelope, bytes.begin() + 120);
  std::copy_n(normal.begin() + 80, 32, bytes.begin() + 128);
  return bytes;
}

std::vector<u8> StreamBytes(bool stereo = false) {
  constexpr usize kChunk = 2048;
  constexpr u32 kChannelBytes = 2080;
  const usize channels = stereo ? 2 : 1;
  std::vector<u8> bytes(64 + (stereo ? 4 * kChunk : kChannelBytes));
  Put32(bytes, 0, 0x4F414B41);
  Put32(bytes, 16, kChannelBytes);
  Put32(bytes, 24, stereo ? 1 : 0);
  Put32(bytes, 28, 2048);
  Put32(bytes, 32, 2048);
  for (usize channel = 0; channel < channels; ++channel) {
    for (usize byte = 0; byte < kChannelBytes; byte += 16) {
      const usize offset =
          64 + (byte / kChunk * channels + channel) * kChunk + byte % kChunk;
      bytes[offset] = byte < kChunk ? 12 : 0x1C;
      bytes[offset + 1] = byte % kChunk == kChunk - 16 ? 3 : 2;
      const u8 packed = byte >= kChunk ? 0 : (channel == 0 ? 0x11 : 0xFF);
      std::ranges::fill(std::span(bytes).subspan(offset + 2, 14), packed);
    }
  }
  return bytes;
}

struct AudioFixture {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("vs-audio-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  Content::AudioBank bank;

  AudioFixture() {
    bank.instruments.push_back({0, 0, 0, 60, 0x00FF, 0x5FC4});
    Content::AudioSample sample;
    sample.repeat = true;
    sample.pcm.resize(441);
    for (usize index = 0; index < sample.pcm.size(); ++index) {
      sample.pcm[index] = static_cast<i16>(
          12000 * std::sin(2 * std::numbers::pi * static_cast<double>(index) /
                           441.0 * 4));
    }
    bank.samples.push_back(std::move(sample));
  }

  ~AudioFixture() try {
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (error) {
      std::fprintf(stderr, "audio fixture cleanup failed: %d\n", error.value());
      std::abort();
    }
  } catch (const std::bad_alloc&) {
    std::fputs("audio fixture cleanup could not allocate memory\n", stderr);
    std::abort();
  }

  Audio::VoiceControls Controls() const {
    Audio::VoiceControls controls;
    controls.instrument = Content::AudioInstrumentPath(root, 0, 0);
    controls.envelope = Audio::DecodeAkaoEnvelope(0x00FF, 0x5FC4);
    return controls;
  }
};

struct StreamFixture {
  AudioFixture files;
  std::vector<u8> package;

  explicit StreamFixture(bool stereo = false, bool repeat = false) {
    auto bytes = StreamBytes(stereo);
    if (repeat) Put32(bytes, 24, stereo ? 3 : 2);
    for (usize offset = 64; offset < bytes.size(); offset += 16) {
      bytes[offset] &= 0xF0;
    }
    const auto stream = Content::DecodeAudioStream(bytes);
    REQUIRE(stream);
    REQUIRE(Content::ExportAudioStream(*stream, files.root));
    std::ofstream manifest(files.root / Content::DataStore::ManifestName);
    manifest << "lba\tsize\tform\tpath\n";
    manifest.close();
    REQUIRE(manifest.good());
    REQUIRE(Content::DataStore::Open(files.root));
    package.resize(bytes.size() + 8);
    Put32(package, 0, 0xD0000 | static_cast<u32>(bytes.size() + 4));
    std::ranges::copy(bytes, package.begin() + 4);
    TestsPrepareStreamPlayback();
  }

  ~StreamFixture() { TestsRestoreStreamPlayback(); }

  void BindAndPlay(int pan = 128) {
    REQUIRE(TestsBindEffectStream(package.data()) == 3);
    REQUIRE_FALSE(TestsStreamSlotIsEmpty());
    REQUIRE(TestsPlayStreamSlot(3, pan, 127) == 1);
    REQUIRE(TestsStreamVoiceMask() == 0xC00000);
  }

  static void Tick(int count = 1) {
    for (int tick = 0; tick < count; ++tick) TestsAdvanceStreamTick();
  }
};

struct XaFixture {
  static constexpr int kSector = 270000;
  AudioFixture files;
  std::filesystem::path asset =
      Content::XaAudioPath(files.root, "ENDING/ENDING.XA");

  explicit XaFixture(bool stereo) {
    const auto source = std::filesystem::path(SDL_GetBasePath()) / ".." / ".." /
                        "Native" / "Tests" / "data" / "audio" /
                        (stereo ? "stereo-tone.flac" : "mono-tone.flac");
    std::filesystem::create_directories(asset.parent_path());
    REQUIRE(std::filesystem::copy_file(source, asset));
    std::ofstream manifest(files.root / Content::DataStore::ManifestName);
    manifest << "lba\tsize\tform\tpath\n"
             << kSector << "\t614400\tstream\tENDING/ENDING.XA\n";
    manifest.close();
    REQUIRE(manifest.good());
    REQUIRE(Content::DataStore::Open(files.root));
    TestsPrepareXaPlayback();
  }

  ~XaFixture() { TestsRestoreXaPlayback(); }

  static int Command(int command, int first = 0, int second = 0,
                     int third = 0) {
    return TestsXaCommand(command, first, second, third);
  }

  static void Start(int volume = 0x7FFF, int sector = kSector) {
    REQUIRE(Command(0, sector, volume) == 1);
    REQUIRE(Command(1) == 7);
    REQUIRE(Audio_Native_XaStreamStatus() == 1);
  }

  static void Stop() {
    REQUIRE(Command(2) == 1);
    REQUIRE(Command(1) == 0);
    REQUIRE(Audio_Native_XaStreamStatus() == 0);
  }

  static std::vector<float> CaptureAt(SDL_AudioStream* source, int sector) {
    Start(0x7FFF, sector);
    OutputProbe probe(source);
    probe.Wait();
    return probe.samples;
  }
};

double Energy(std::span<const float> frames, usize channel) {
  double energy = 0;
  for (usize index = channel; index < frames.size(); index += 2) {
    energy += frames[index] * frames[index];
  }
  return energy;
}

void CheckStereoAudible(std::span<const float> frames) {
  CHECK(Energy(frames, 0) > 0.01);
  CHECK(Energy(frames, 1) > 0.01);
}

void RenderTick(
    Audio::VoiceEngine& engine,
    std::span<float, Audio::VoiceEngine::kFramesPerTick * 2> frames) {
  engine.CommitTick();
  engine.Render(frames);
}

void StartVoice(Audio::VoiceEngine& engine, usize voice,
                const Audio::VoiceControls& controls) {
  engine.Configure(voice, controls);
  REQUIRE(engine.NoteOn(voice));
}

void StartStereoPair(Audio::VoiceEngine& engine, const AudioFixture& fixture) {
  auto controls = fixture.Controls();
  controls.right = 0;
  StartVoice(engine, 0, controls);
  controls.left = 0;
  controls.right = 1;
  controls.rate = 2;
  StartVoice(engine, 1, controls);
}
constexpr usize kXaSectorBytes = 2352;

std::vector<u8> XaSectors() {
  std::vector<u8> sectors(8 * kXaSectorBytes);
  for (usize index = 0; index < 8; ++index) {
    auto sector =
        std::span(sectors).subspan(index * kXaSectorBytes, kXaSectorBytes);
    sector[15] = 2;
    sector[16] = sector[20] = 1;
    sector[18] = sector[22] = 0x64;
    sector[19] = sector[23] = 1;
    std::fill(sector.begin() + 24, sector.begin() + 2328,
              index < 4 ? 0x11 : 0x22);
  }
  return sectors;
}

void CheckXaVolumeFades() {
  CHECK(XaFixture::Command(6) == 0x4000);
  XaFixture::Command(4, 4, 0);
  StreamFixture::Tick(2);
  CHECK(XaFixture::Command(6) == 0x2000);
  StreamFixture::Tick(2);
  CHECK(XaFixture::Command(6) == 0);
  XaFixture::Command(5, 4, 0, 0x4000);
  StreamFixture::Tick(2);
  CHECK(XaFixture::Command(6) == 0x2000);
  StreamFixture::Tick(2);
  CHECK(XaFixture::Command(6) == 0x4000);
}

void CheckStreamVoicesStopped() {
  CHECK(TestsStreamVoiceMask() == 0);
  CHECK_FALSE(Audio_Native_VoiceActive(22));
  CHECK_FALSE(Audio_Native_VoiceActive(23));
}

void CheckStreamPauseToCompletion() {
  StreamFixture::Tick(5);
  CHECK(Audio_Native_VoiceActive(22));
  const int envelope = Audio_Native_Envelope(22);
  TestsStreamControl(3, 1, 0);
  StreamFixture::Tick(100);
  CHECK(TestsStreamVoiceMask() == 0xC00000);
  CHECK(Audio_Native_Envelope(22) == envelope);
  TestsStreamControl(3, 0, 0);
  StreamFixture::Tick(100);
  CheckStreamVoicesStopped();
}

void CheckStreamMetadata(const Content::AudioStreamInfo& info, bool stereo,
                         std::span<const u8> source) {
  CHECK(info.channels == (stereo ? 2 : 1));
  CHECK(info.sample_rate == 22050);
  CHECK(info.source_pitch == 2048);
  CHECK(info.frames == 3640);
  CHECK_FALSE(info.repeat);
  CHECK(std::equal(info.source_header.begin(), info.source_header.end(),
                   source.begin()));
}

void CheckStreamVolumeFade() {
  TestsStreamControl(1, 7, 0);
  StreamFixture::Tick(6);
  CHECK(TestsStreamVolume() > 0);
  StreamFixture::Tick();
  CHECK(TestsStreamVolume() == 0);
  CHECK(Audio_Native_VoiceActive(22));
  TestsStreamControl(0, 127, 0);
  CHECK(TestsStreamVolume() == 0x7F00);
}
}  // namespace

TEST_CASE("XA import preserves playback cadence rather than duplicate sectors",
          "[audio][xa-import]") {
  auto sectors = XaSectors();
  const auto selected = Content::SelectXaAudioSectors(sectors);
  REQUIRE(selected);
  REQUIRE(selected->size() == 2 * kXaSectorBytes);
  CHECK((*selected)[24] == 0x11);
  CHECK((*selected)[kXaSectorBytes + 24] == 0x22);
  for (usize index = 4; index < 8; ++index) {
    std::ranges::fill(
        std::span(sectors).subspan(index * kXaSectorBytes + 24, 2304), 0x11);
  }
  const auto silence = Content::SelectXaAudioSectors(sectors);
  REQUIRE(silence);
  CHECK(silence->size() == 2 * kXaSectorBytes);
}

TEST_CASE("XA import rejects invalid sectors and timing",
          "[audio][xa-import]") {
  auto sectors = XaSectors();
  SECTION("truncated sector") { sectors.pop_back(); }
  SECTION("truncated playback period") { sectors.resize(7 * kXaSectorBytes); }
  SECTION("mismatched subheader") { sectors[20] = 2; }
  SECTION("invalid coding") { sectors[19] = sectors[23] = 0xAA; }
  SECTION("nonredundant skipped audio") { sectors[kXaSectorBytes + 24] ^= 1; }
  SECTION("missing selected channel") { sectors[16] = sectors[20] = 2; }
  SECTION("channel timing gap") {
    sectors[4 * kXaSectorBytes + 16] = sectors[4 * kXaSectorBytes + 20] = 2;
  }
  CHECK_FALSE(Content::SelectXaAudioSectors(sectors));
}

TEST_CASE("XA native output preserves stereo and mono volume controls",
          "[audio][xa-playback]") {
  AudioDeviceFixture device;
  XaFixture fixture(true);
  auto capture = [&](int volume, bool mono, unsigned mix) {
    TestsNativeMonoOutput(mono ? 1 : 0);
    XaFixture::Start(volume);
    REQUIRE(Audio_Control_SetDiscMixVolume(mix) == 0);
    OutputProbe probe(device.source.get());
    probe.Wait();
    XaFixture::Stop();
    return probe.samples;
  };
  const auto full = capture(0x7FFF, false, 127);
  CheckStereoAudible(full);
  CHECK(std::abs(Energy(full, 0) / Energy(full, 1) - 4) < 0.01);
  const auto half = capture(0x4000, false, 127);
  CHECK(std::abs(Energy(half, 0) / Energy(full, 0) - 0.25) < 0.005);
  const auto mono = capture(0x7FFF, true, 127);
  CheckStereoAudible(mono);
  for (usize frame = 0; frame < mono.size(); frame += 2) {
    REQUIRE(mono[frame] == mono[frame + 1]);
  }
  const auto muted = capture(0x7FFF, false, 0);
  CHECK(Energy(muted, 0) == 0);
  CHECK(Energy(muted, 1) == 0);
}

TEST_CASE(
    "XA queue observes natural completion and supports repeated mono playback",
    "[audio][xa-playback]") {
  AudioDeviceFixture device;
  XaFixture fixture(false);
  for (int pass = 0; pass < 2; ++pass) {
    XaFixture::Start();
    CHECK(XaFixture::Command(0, XaFixture::kSector, 0x7FFF) == 0);
    OutputProbe probe(device.source.get());
    probe.Wait();
    CheckStereoAudible(probe.samples);
    for (usize frame = 0; frame < probe.samples.size(); frame += 2) {
      REQUIRE(probe.samples[frame] == probe.samples[frame + 1]);
    }
    CHECK(Audio_Native_XaStreamStatus() == 0);
    CHECK(XaFixture::Command(1) == 0);
    CHECK(XaFixture::Command(2) == 0);
  }
}

TEST_CASE("XA fades reset and device re-entry use the production control path",
          "[audio][xa-playback]") {
  AudioDeviceFixture device;
  XaFixture fixture(true);
  XaFixture::Start(0x4000);
  CheckXaVolumeFades();
  SECTION("driver reset") { CHECK(XaFixture::Command(7) == 0); }
  SECTION("disc reset") { CHECK(XaFixture::Command(8) == 0); }
  SECTION("device close") { device.Reopen(); }
  CHECK(Audio_Native_XaStreamStatus() == 0);
  XaFixture::Start();
  OutputProbe probe(device.source.get());
  probe.Wait();
  CheckStereoAudible(probe.samples);
  XaFixture::Stop();
}

TEST_CASE("Installed ending XA reaches native output and drains at EOF",
          "[audio][xa-playback][disc]") {
  const auto root = std::filesystem::path(SDL_GetBasePath()) / ".." / ".." /
                    "Native" / "data";
  if (!std::filesystem::exists(
          Content::XaAudioPath(root, "ENDING/ENDING.XA"))) {
    SKIP("Converted ending XA is unavailable");
  }
  AudioDeviceFixture device;
  XaFixture fixture(true);
  REQUIRE(Content::DataStore::Open(root));
  const auto* entry = Content::DataStore::Get()->Find("ENDING/ENDING.XA");
  REQUIRE(entry != nullptr);
  REQUIRE(entry->lba == XaFixture::kSector);
  REQUIRE(entry->SectorCount() == 33264);
  CheckStereoAudible(
      XaFixture::CaptureAt(device.source.get(), XaFixture::kSector));
  XaFixture::Stop();
  CheckStereoAudible(
      XaFixture::CaptureAt(device.source.get(), XaFixture::kSector + 30 * 75));
  XaFixture::Stop();
  const int last_sector =
      XaFixture::kSector + static_cast<int>(entry->SectorCount()) - 50;
  const auto tail = XaFixture::CaptureAt(device.source.get(), last_sector);
  const bool finite = std::ranges::all_of(
      tail, [](float sample) { return std::isfinite(sample); });
  CHECK(finite);
  CHECK(Audio_Native_XaStreamStatus() == 0);
  CHECK(XaFixture::Command(1) == 0);
}

TEST_CASE("XA failures reach the disc error state rather than false success",
          "[audio][xa-playback]") {
  AudioDeviceFixture device;
  XaFixture fixture(true);
  SECTION("missing converted file") {
    REQUIRE(std::filesystem::remove(fixture.asset));
  }
  SECTION("malformed converted file") {
    std::ofstream corrupt(fixture.asset, std::ios::binary);
    corrupt << "not a FLAC";
  }
  SECTION("truncated converted file") {
    std::filesystem::resize_file(fixture.asset,
                                 std::filesystem::file_size(fixture.asset) / 2);
  }
  SECTION("invalid sector") {
    CHECK_FALSE(Audio_Native_StartXaStream(-1, 0x7FFF));
    return;
  }
  SECTION("invalid initial volume") {
    CHECK_FALSE(Audio_Native_StartXaStream(XaFixture::kSector, 0x8000));
    return;
  }
  REQUIRE(XaFixture::Command(0, XaFixture::kSector, 0x7FFF) == 1);
  CHECK(XaFixture::Command(1) == 4);
  CHECK(XaFixture::Command(9) == 1);
  CHECK(Audio_Native_XaStreamStatus() == 0);
}

TEST_CASE("Native mix reset preserves scheduling headroom and movie sources",
          "[audio][device]") {
  AudioDeviceFixture fixture;
  std::array<i16, 200> movie{};
  REQUIRE(SDL_PutAudioStreamData(fixture.source.get(), movie.data(),
                                 sizeof(movie)));
  fixture.device.Clear();
  CHECK(fixture.device.QueuedBytes() == kBufferBytes);
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
  fixture.device.Submit(frames);
  CHECK(fixture.device.QueuedBytes() == kBufferBytes + sizeof(frames));
  fixture.device.Clear();
  CHECK(fixture.device.QueuedBytes() == kBufferBytes);
  CHECK(SDL_GetAudioStreamQueued(fixture.source.get()) == sizeof(movie));
}

TEST_CASE("Native audio device closes and reopens without stale queued output",
          "[audio][device]") {
  AudioDeviceFixture fixture;
  CHECK_FALSE(fixture.device.Open());
  fixture.source.reset();
  fixture.device.Close();
  CHECK_FALSE(fixture.device.IsOpen());
  CHECK(fixture.device.QueuedBytes() == 0);
  REQUIRE(fixture.device.Open());
  fixture.Pause();
  fixture.device.Clear();
  CHECK(fixture.device.QueuedBytes() == kBufferBytes);
}

TEST_CASE("Discarding stale native audio retains the scheduling buffer",
          "[audio][device]") {
  AudioDeviceFixture fixture;
  fixture.device.Clear();
  std::vector<float> stale(Audio::AudioDevice::SampleRate *
                           Audio::AudioDevice::Channels / 4);
  fixture.device.Submit(stale);
  CHECK(fixture.device.QueuedBytes() ==
        kBufferBytes + stale.size() * sizeof(float));
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
  fixture.device.Submit(frames);
  CHECK(fixture.device.QueuedBytes() == kBufferBytes + sizeof(frames));
}

TEST_CASE("AKAO wave import validates boundaries and decodes loop markers",
          "[audio][bank]") {
  auto bytes = BankBytes();
  const auto bank = Content::DecodeAudioBank(bytes);
  REQUIRE(bank);
  REQUIRE(bank->samples.size() == 1);
  CHECK(bank->samples[0].pcm.size() == 56);
  CHECK(bank->samples[0].pcm[0] == 7);
  CHECK(bank->samples[0].pcm[1] == 1);
  CHECK(bank->samples[0].pcm[28] == -8);
  CHECK(bank->samples[0].pcm[29] == -1);
  CHECK(bank->samples[0].repeat);
  CHECK(bank->samples[0].loop_frame == 28);
}

TEST_CASE("Cutscene streams preserve channel history across buffer end flags",
          "[audio][stream-import]") {
  for (const bool stereo : {false, true}) {
    const auto bytes = StreamBytes(stereo);
    const auto stream = Content::DecodeAudioStream(bytes);
    REQUIRE(stream);
    CheckStreamMetadata(stream->info, stereo, bytes);
    for (usize channel = 0; channel < stream->channels.size(); ++channel) {
      const auto& pcm = stream->channels[channel].pcm;
      REQUIRE(pcm.size() == 3640);
      const bool samples_match = std::ranges::all_of(pcm, [channel](i16 value) {
        return value == (channel == 0 ? 1 : -1);
      });
      CHECK(samples_match);
    }
    const auto repeated = Content::InspectAudioStream(bytes);
    REQUIRE(repeated);
    CHECK(repeated->id == stream->info.id);
  }
}

TEST_CASE("Cutscene stream validation rejects invalid headers and payloads",
          "[audio][stream-import]") {
  auto bytes = StreamBytes();
  SECTION("short header") { bytes.resize(63); }
  SECTION("magic") { bytes[0] = 0; }
  SECTION("zero length") { Put32(bytes, 16, 0); }
  SECTION("unaligned length") { Put32(bytes, 16, 17); }
  SECTION("truncated payload") { bytes.pop_back(); }
  SECTION("oversized payload") { Put32(bytes, 16, 0xFFFFFFF0); }
  CHECK_FALSE(Content::DecodeAudioStream(bytes));
}

TEST_CASE("Cutscene stream validation rejects invalid flags and pitch",
          "[audio][stream-import]") {
  auto bytes = StreamBytes();
  SECTION("unknown flags") { Put32(bytes, 24, 4); }
  SECTION("zero pitch") { Put32(bytes, 28, 0); }
  SECTION("excessive pitch") { Put32(bytes, 28, 16385); }
  CHECK_FALSE(Content::DecodeAudioStream(bytes));
}

TEST_CASE("Cutscene stream validation rejects invalid ADPCM",
          "[audio][stream-import]") {
  auto bytes = StreamBytes();
  SECTION("invalid predictor") { bytes[64] = 0x5C; }
  SECTION("invalid shift") { bytes[64] = 13; }
  CHECK_FALSE(Content::DecodeAudioStream(bytes));
}

TEST_CASE("Cutscene stream loops are native frame positions, not buffer loops",
          "[audio][stream-import]") {
  for (const bool stereo : {false, true}) {
    auto bytes = StreamBytes(stereo);
    Put32(bytes, 24, stereo ? 3 : 2);
    Put32(bytes, 20, stereo ? 4096 : 2048);
    const auto stream = Content::DecodeAudioStream(bytes);
    REQUIRE(stream);
    CHECK(stream->info.repeat);
    CHECK(stream->info.loop_frame == 3584);
    for (const auto& channel : stream->channels) {
      CHECK(channel.repeat);
      CHECK(channel.loop_frame == 3584);
    }
    Put32(bytes, 20, stereo ? 2048 : 17);
    CHECK_FALSE(Content::DecodeAudioStream(bytes));
    Put32(bytes, 20, 8192);
    CHECK_FALSE(Content::DecodeAudioStream(bytes));
  }
}

TEST_CASE("Effect stream discovery respects block bounds and termination",
          "[audio][stream-import]") {
  const auto bytes = StreamBytes();
  std::vector<u8> package(bytes.size() + 8);
  Put32(package, 0, 0xD0000 | static_cast<u32>(bytes.size() + 4));
  std::ranges::copy(bytes, package.begin() + 4);
  const auto found = Content::FindEffectAudioStreams(package);
  REQUIRE(found);
  REQUIRE(found->size() == 1);
  CHECK(found->front().offset == 4);
  CHECK(found->front().bytes.size() == bytes.size());
  REQUIRE(Content::DecodeAudioStream(found->front().bytes));
  SECTION("short block") { Put32(package, 0, 0xD0003); }
  SECTION("oversized block") { Put32(package, 0, 0xDFFFF); }
  SECTION("missing terminator") { package.resize(package.size() - 4); }
  CHECK_FALSE(Content::FindEffectAudioStreams(package));
}

TEST_CASE("Native stream assets retain provenance and regenerate identically",
          "[audio][stream-import]") {
  AudioFixture fixture;
  const auto stream = Content::DecodeAudioStream(StreamBytes(true));
  REQUIRE(stream);
  REQUIRE(Content::ExportAudioStream(*stream, fixture.root));
  REQUIRE(Content::ValidateAudioStreamAssets(stream->info, fixture.root));
  const auto instrument =
      Content::AudioStreamInstrumentPath(fixture.root, stream->info, 0);
  const auto metadata_path = instrument.parent_path() / "stream.json";
  std::ifstream first(metadata_path);
  const std::string before{std::istreambuf_iterator<char>(first), {}};
  first.close();
  REQUIRE(Content::ExportAudioStream(*stream, fixture.root));
  std::ifstream second(metadata_path);
  const std::string after{std::istreambuf_iterator<char>(second), {}};
  second.close();
  CHECK(before == after);
  CHECK(std::filesystem::file_size(instrument.parent_path() / "0.wav") ==
        44 + 3640 * 2);
  auto changed = StreamBytes(true);
  changed[66] = 0x22;
  const auto other = Content::InspectAudioStream(changed);
  REQUIRE(other);
  CHECK(other->id != stream->info.id);
}

TEST_CASE("Native stream assets reject mismatches and missing channels",
          "[audio][stream-import]") {
  AudioFixture fixture;
  const auto stream = Content::DecodeAudioStream(StreamBytes(true));
  REQUIRE(stream);
  REQUIRE(Content::ExportAudioStream(*stream, fixture.root));
  const auto directory =
      Content::AudioStreamInstrumentPath(fixture.root, stream->info, 0)
          .parent_path();
  const auto metadata_path = directory / "stream.json";
  SECTION("mismatched metadata") {
    std::ifstream input(metadata_path);
    auto metadata = nlohmann::json::parse(input);
    input.close();
    metadata["frames"] = -1;
    std::ofstream output(metadata_path);
    output << metadata;
  }
  SECTION("missing channel") {
    REQUIRE(std::filesystem::remove(directory / "1.wav"));
  }
  CHECK_FALSE(Content::ValidateAudioStreamAssets(stream->info, fixture.root));
}

TEST_CASE("Native voice pause freezes the first sample and envelope",
          "[audio][stream-playback]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine paused;
  Audio::VoiceEngine reference;
  auto controls = fixture.Controls();
  StartVoice(reference, 0, controls);
  controls.paused = true;
  StartVoice(paused, 0, controls);
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> actual{};
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> expected{};
  CHECK(paused.IsActive(0));
  for (int tick = 0; tick < 10; ++tick) {
    RenderTick(paused, actual);
    CHECK(Energy(actual, 0) == 0);
    CHECK(paused.IsActive(0));
    CHECK(paused.EnvelopeLevel(0) == 0);
  }
  controls.paused = false;
  paused.Configure(0, controls);
  RenderTick(paused, actual);
  RenderTick(reference, expected);
  CHECK(actual == expected);
  CheckStereoAudible(actual);
  paused.Stop(0);
  CHECK_FALSE(paused.IsActive(0));
}

TEST_CASE("Native final output preserves quiet additive mixes",
          "[audio][output-bounds]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine reference;
  Audio::VoiceEngine mixed;
  auto controls = fixture.Controls();
  controls.left = 0.2F;
  controls.right = -0.1F;
  StartVoice(reference, 0, controls);
  controls.left /= 2;
  controls.right /= 2;
  StartVoice(mixed, 0, controls);
  StartVoice(mixed, 1, controls);
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> expected{};
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> actual{};
  for (int tick = 0; tick < 8; ++tick) {
    RenderTick(reference, expected);
    RenderTick(mixed, actual);
    for (usize sample = 0; sample < actual.size(); ++sample) {
      CHECK(std::abs(actual[sample] - expected[sample]) < 0.000001F);
    }
  }
}

TEST_CASE("Native final dry and wet mix preserves signed PCM output bounds",
          "[audio][output-bounds]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  for (const bool mono : {false, true}) {
    Audio::VoiceEngine engine;
    engine.SetMonoOutput(mono);
    engine.SetReverb({Audio::ReverbPreset::kHall, true, 1, 1});
    auto controls = fixture.Controls();
    controls.reverb = true;
    controls.right = 0.5F;
    for (usize voice = 0; voice < 8; ++voice) {
      StartVoice(engine, voice, controls);
    }
    std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
    constexpr float kMaximum = 32767.0F / 32768.0F;
    bool positive_limit = false;
    bool negative_limit = false;
    for (int tick = 0; tick < 120; ++tick) {
      RenderTick(engine, frames);
      const auto [low, high] = std::ranges::minmax_element(frames);
      REQUIRE(*low >= -1.0F);
      REQUIRE(*high <= kMaximum);
      positive_limit |= *high == kMaximum;
      negative_limit |= *low == -1.0F;
      CHECK((!mono || frames[0] == frames[1]));
    }
    CHECK(positive_limit);
    CHECK(negative_limit);
  }
}

TEST_CASE(
    "Effect stream routing preserves native pointers and supports re-entry",
    "[audio][stream-playback]") {
  for (const bool stereo : {false, true}) {
    StreamFixture fixture(stereo);
    fixture.BindAndPlay();
    CHECK(TestsBindEffectStream(fixture.package.data()) == 0);
    CheckStreamPauseToCompletion();
    CHECK_FALSE(TestsStreamSlotIsEmpty());
    REQUIRE(TestsPlayStreamSlot(3, 128, 127) == 1);
    StreamFixture::Tick(5);
    CHECK(Audio_Native_VoiceActive(22));
    TestsStreamControl(4, 0, 0);
    StreamFixture::Tick(5);
    CheckStreamVoicesStopped();
    CHECK(Audio_UnbindRoomCutsceneStream() == 1);
    CHECK(TestsStreamSlotIsEmpty());
    CHECK(TestsPlayStreamSlot(3, 128, 127) == 0);
  }
}

TEST_CASE("Stream fades reach exact targets and reset releases borrowed data",
          "[audio][stream-playback]") {
  StreamFixture fixture(false, true);
  fixture.BindAndPlay();
  StreamFixture::Tick();
  CheckStreamVolumeFade();
  SECTION("driver reset") { TestsStreamControl(5, 0, 0); }
  SECTION("slot reset") { TestsStreamControl(6, 0, 0); }
  SECTION("unbind while paused") {
    TestsStreamControl(3, 1, 0);
    CHECK(Audio_UnbindRoomCutsceneStream() == 1);
  }
  StreamFixture::Tick(10);
  CheckStreamVoicesStopped();
  CHECK(TestsStreamSlotIsEmpty());
  fixture.BindAndPlay();
  StreamFixture::Tick(5);
  CHECK(Audio_Native_VoiceActive(22));
}

TEST_CASE("Production stream pan and Mono settings reach actual device samples",
          "[audio][stream-playback][device]") {
  AudioDeviceFixture device;
  StreamFixture fixture(false, true);
  fixture.BindAndPlay(0);
  StreamFixture::Tick();
  Audio_Native_SetReverbDepth(0, 0);
  Audio_Native_Tick();
  {
    OutputProbe output(device.source.get());
    output.Wait();
    CHECK(Energy(output.samples, 0) > 0.1);
    CHECK(Energy(output.samples, 1) == 0);
  }
  TestsStreamControl(2, 255, 0);
  StreamFixture::Tick();
  {
    OutputProbe output(device.source.get());
    output.Wait();
    const auto settled = std::span(output.samples).last(24000);
    CHECK(Energy(settled, 0) == 0);
    CHECK(Energy(settled, 1) > 0.1);
  }
  TestsNativeMonoOutput(1);
  StreamFixture::Tick();
  {
    OutputProbe output(device.source.get());
    output.Wait();
    const auto settled = std::span(output.samples).last(24000);
    CheckStereoAudible(settled);
    for (usize index = 0; index < settled.size(); index += 2) {
      REQUIRE(settled[index] == settled[index + 1]);
    }
  }
}

TEST_CASE("Wave import rejects truncated storage and instrument overflow",
          "[audio][bank]") {
  auto bytes = BankBytes();
  CHECK_FALSE(Content::DecodeAudioBank(std::span(bytes).first(63)));
  CHECK_FALSE(Content::DecodeAudioBank(std::span(bytes).first(111)));
  Put32(bytes, 24, 255);
  Put32(bytes, 28, 2);
  CHECK_FALSE(Content::DecodeAudioBank(bytes));
}

TEST_CASE("Wave import rejects invalid ADPCM predictors and termination",
          "[audio][bank]") {
  auto bytes = BankBytes();
  SECTION("invalid predictor") {
    bytes[80] = 0x5C;
    CHECK_FALSE(Content::DecodeAudioBank(bytes));
  }
  SECTION("invalid shift") {
    bytes[80] = 13;
    CHECK_FALSE(Content::DecodeAudioBank(bytes));
  }
  SECTION("missing end block") {
    bytes[97] = 0;
    CHECK_FALSE(Content::DecodeAudioBank(bytes));
  }
  SECTION("repeat without a valid loop target") {
    bytes[97] = 3;
    Put32(bytes, 68, 32);
    CHECK_FALSE(Content::DecodeAudioBank(bytes));
  }
}

TEST_CASE("Unavailable padding records remain unavailable after export",
          "[audio][bank]") {
  auto bytes = BankBytes();
  Put32(bytes, 20, 64);
  Put32(bytes, 64, 32);
  Put32(bytes, 68, 32);
  const auto unavailable = Content::DecodeAudioBank(bytes);
  REQUIRE(unavailable);
  CHECK_FALSE(unavailable->instruments[0].available);
  CHECK(unavailable->samples[0].pcm.empty());
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(*unavailable, fixture.root));
  CHECK_FALSE(std::filesystem::exists(
      Content::AudioInstrumentPath(fixture.root, 0, 0)));
  const auto restored = Content::LoadAudioBank(fixture.root, 0);
  REQUIRE(restored);
  CHECK_FALSE(restored->instruments[0].available);
}

TEST_CASE("Omitted DMA padding never supplies missing sample bytes",
          "[audio][bank]") {
  auto bytes = BankBytes();
  Put32(bytes, 20, 64);
  CHECK(Content::DecodeAudioBank(bytes));
  bytes[97] = 0;
  CHECK_FALSE(Content::DecodeAudioBank(bytes));
}

TEST_CASE("Unused instrument records can point exactly past the payload",
          "[audio][bank][bank-padding]") {
  auto bytes = BankBytes();
  Put32(bytes, 64, 32);
  Put32(bytes, 68, 32);
  const auto bank = Content::DecodeAudioBank(bytes);
  REQUIRE(bank);
  CHECK_FALSE(bank->instruments[0].available);
  CHECK(bank->samples[0].pcm.empty());
}

TEST_CASE("Unused instrument records recognize only erased DMA tail padding",
          "[audio][bank][bank-padding]") {
  std::array<u8, 144> bytes{};
  const auto original = BankBytes();
  std::ranges::copy(original, bytes.begin());
  Put32(bytes, 20, 64);
  Put32(bytes, 64, 16);
  Put32(bytes, 68, 16);
  std::fill(bytes.begin() + 96, bytes.end(), 0xFF);
  const auto bank = Content::DecodeAudioBank(bytes);
  REQUIRE(bank);
  CHECK_FALSE(bank->instruments[0].available);
  Put32(bytes, 20, 32);
  CHECK_FALSE(Content::DecodeAudioBank(bytes));
  Put32(bytes, 20, 64);
  bytes.back() = 0xFE;
  CHECK_FALSE(Content::DecodeAudioBank(bytes));
  Put32(bytes, 68, 0);
  CHECK_FALSE(Content::DecodeAudioBank(bytes));
}

TEST_CASE("Legacy wave articulations preserve tuning and envelope provenance",
          "[audio][bank][legacy-bank]") {
  const auto bytes = LegacyBankBytes();
  const auto bank = Content::DecodeAudioBank(bytes);
  REQUIRE(bank);
  REQUIRE(bank->instruments.size() == 1);
  const auto& instrument = bank->instruments.front();
  CHECK(instrument.root_note == 72);
  CHECK(instrument.fine_tune == 0);
  CHECK(instrument.adsr_lower == 0x00FF);
  CHECK(instrument.adsr_upper == 0x5FC7);
  CHECK(instrument.source_record ==
        std::vector<u8>(bytes.begin() + 64, bytes.begin() + 128));
  CHECK(bank->samples[0].pcm.size() == 56);
}

TEST_CASE("Exported legacy instrument provenance survives metadata loading",
          "[audio][bank][legacy-bank]") {
  const auto bank = Content::DecodeAudioBank(LegacyBankBytes());
  REQUIRE(bank);
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(*bank, fixture.root));
  const auto restored = Content::LoadAudioBank(fixture.root, 0);
  REQUIRE(restored);
  CHECK(restored->instruments[0].source_record ==
        bank->instruments[0].source_record);
}

TEST_CASE("Legacy zero-pitch articulations are explicitly unavailable",
          "[audio][bank][legacy-bank]") {
  auto bytes = LegacyBankBytes();
  std::fill(bytes.begin() + 72, bytes.begin() + 120, 0);
  const auto bank = Content::DecodeAudioBank(bytes);
  REQUIRE(bank);
  CHECK_FALSE(bank->instruments[0].available);
  CHECK(bank->instruments[0].source_record.size() == 64);
  CHECK(bank->samples[0].pcm.empty());
  Put32(bytes, 76, 1048576);
  CHECK_FALSE(Content::DecodeAudioBank(bytes));
}

TEST_CASE("Equal sample and loop offsets do not discard valid ADPCM",
          "[audio][bank][bank-padding]") {
  auto bytes = BankBytes();
  Put32(bytes, 68, 0);
  const auto bank = Content::DecodeAudioBank(bytes);
  REQUIRE(bank);
  CHECK(bank->instruments[0].available);
  CHECK(bank->samples[0].pcm.size() == 56);
}

TEST_CASE("ADPCM predictors round feedback and saturate signed samples",
          "[audio][bank]") {
  auto bytes = BankBytes();
  const std::array<std::array<i16, 3>, 4> expected{
      {{7, 8, 8}, {7, 14, 19}, {7, 12, 12}, {7, 14, 20}}};
  for (usize predictor = 1; predictor <= expected.size(); ++predictor) {
    bytes[80] = static_cast<u8>((predictor << 4) | 12);
    const auto bank = Content::DecodeAudioBank(bytes);
    REQUIRE(bank);
    for (usize sample = 0; sample < expected[predictor - 1].size(); ++sample) {
      CHECK(bank->samples[0].pcm[sample] == expected[predictor - 1][sample]);
    }
  }
}

TEST_CASE("ADPCM feedback saturates both signed sample extremes",
          "[audio][bank]") {
  auto bytes = BankBytes();
  bytes[80] = 0x10;
  bytes[82] = 0x77;
  const auto positive = Content::DecodeAudioBank(bytes);
  REQUIRE(positive);
  CHECK(positive->samples[0].pcm[0] == 28672);
  CHECK(positive->samples[0].pcm[1] == 32767);
  bytes[82] = 0x88;
  const auto negative = Content::DecodeAudioBank(bytes);
  REQUIRE(negative);
  CHECK(negative->samples[0].pcm[0] == -32768);
  CHECK(negative->samples[0].pcm[1] == -32768);
}

TEST_CASE("Exported native bank assets round-trip and regenerate identically",
          "[audio][bank]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  const auto path = Content::AudioInstrumentPath(fixture.root, 0, 0);
  std::ifstream first(path);
  const std::string before{std::istreambuf_iterator<char>(first), {}};
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  std::ifstream second(path);
  const std::string after{std::istreambuf_iterator<char>(second), {}};
  CHECK(before == after);
  const auto restored = Content::LoadAudioBank(fixture.root, 0);
  REQUIRE(restored);
  CHECK(restored->instruments.size() == 1);
  CHECK(restored->instruments[0].root_note == 60);
  CHECK(restored->instruments[0].adsr_upper == 0x5FC4);
  CHECK(std::filesystem::file_size(path.parent_path() / "0.wav") ==
        44 + 441 * 2);
}

TEST_CASE("Native envelope sustains, releases and restarts without old state",
          "[audio][envelope]") {
  Audio::VoiceEnvelope envelope;
  envelope.Start(Audio::DecodeAkaoEnvelope(0x00FF, 0x5FC4));
  for (int frame = 0; frame < 48000; ++frame) envelope.Advance();
  CHECK(envelope.Level() == 32767);
  CHECK_FALSE(envelope.Finished());
  envelope.Release();
  int release_frames = 0;
  while (!envelope.Finished() && release_frames < 48000) {
    envelope.Advance();
    ++release_frames;
  }
  CHECK(release_frames > 1);
  CHECK(release_frames < 100);
  CHECK(envelope.Level() == 0);
  envelope.Start(Audio::DecodeAkaoEnvelope(0x00FF, 0x5FC4));
  CHECK(envelope.Level() == 0);
  CHECK_FALSE(envelope.Finished());
}

TEST_CASE("Native bank metadata rejects malformed shapes and integer overflow",
          "[audio][bank]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  const auto path =
      Content::AudioInstrumentPath(fixture.root, 0, 0).parent_path() /
      "bank.json";
  nlohmann::json metadata;
  {
    std::ifstream in(path);
    in >> metadata;
  }
  SECTION("unsupported version") { metadata["version"] = 2; }
  SECTION("bank ID overflow") {
    metadata["bank_id"] = std::numeric_limits<u64>::max();
  }
  SECTION("negative unsigned provenance") {
    metadata["source_destination"] = -1;
  }
  SECTION("signed instrument overflow") {
    metadata["instruments"][0]["fine_tune"] = 32768;
  }
  SECTION("signed instrument underflow") {
    metadata["instruments"][0]["root_note"] = -32769;
  }
  SECTION("fractional instrument") {
    metadata["instruments"][0]["root_note"] = 1.5;
  }
  SECTION("envelope overflow") {
    metadata["instruments"][0]["adsr_upper"] = 65536;
  }
  SECTION("non-boolean availability") {
    metadata["instruments"][0]["available"] = 1;
  }
  SECTION("non-array instruments") {
    metadata["instruments"] = nlohmann::json::object();
  }
  SECTION("missing field") { metadata.erase("first_instrument"); }
  SECTION("non-object metadata") { metadata = nlohmann::json::array(); }
  std::ofstream out(path);
  out << metadata;
  out.close();
  REQUIRE(out.good());
  CHECK_FALSE(Content::LoadAudioBank(fixture.root, 0));
}

TEST_CASE("Noise is shared, pitch independent, and needs no sample program",
          "[audio][noise]") {
  Audio::VoiceEngine engine;
  Audio::VoiceControls controls;
  controls.noise = true;
  controls.envelope = Audio::DecodeAkaoEnvelope(0x00FF, 0x5FC4);
  controls.right = 0;
  controls.rate = 0;
  StartVoice(engine, 0, controls);
  controls.left = 0;
  controls.right = 1;
  controls.rate = 4;
  StartVoice(engine, 1, controls);
  engine.SetNoiseClock(63);
  std::array<float, 400> frames{};
  RenderTick(engine, frames);
  CheckStereoAudible(frames);
  for (usize frame = 0; frame < 200; ++frame) {
    CHECK(frames[frame * 2] == frames[frame * 2 + 1]);
  }
  CHECK(engine.EnvelopeLevel(0) > 0);
  engine.NoteOff(0);
  engine.NoteOff(1);
  for (int tick = 0; tick < 20; ++tick) RenderTick(engine, frames);
  CHECK(Energy(frames, 0) == 0);
  CHECK(Energy(frames, 1) == 0);
}

TEST_CASE("Noise reset and clock changes are deterministic", "[audio][noise]") {
  Audio::VoiceEngine engine;
  Audio::VoiceControls controls;
  controls.noise = true;
  controls.envelope = Audio::DecodeAkaoEnvelope(0x00FF, 0x5FC4);
  std::array<float, 400> first{}, reset{}, slow{};
  StartVoice(engine, 0, controls);
  engine.SetNoiseClock(63);
  RenderTick(engine, first);
  engine.Reset();
  StartVoice(engine, 0, controls);
  engine.SetNoiseClock(63);
  RenderTick(engine, reset);
  CHECK(first == reset);
  engine.SetNoiseClock(0);
  RenderTick(engine, slow);
  CHECK(first != slow);
}

TEST_CASE("Production noise and reverb remain audible through a control stall",
          "[audio][noise][reverb][device]") {
  AudioDeviceFixture fixture;
  TestsNativeNoiseChannel();
  {
    OutputProbe probe(fixture.source.get());
    probe.Wait();
    const auto tail = std::span(probe.samples).last(4800);
    CheckStereoAudible(tail);
    CHECK(std::ranges::all_of(tail, [](float sample) {
      return std::isfinite(sample) && std::abs(sample) < 1;
    }));
  }
  CHECK(Audio_Native_Envelope(0) > 0);
  Audio_Native_Reset();
}

TEST_CASE("FM uses the preceding envelope-scaled voice before speaker gains",
          "[audio][fm]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine modulated, audible, dry;
  auto source = fixture.Controls();
  source.left = source.right = 0;
  StartVoice(modulated, 0, source);
  StartVoice(dry, 0, source);
  source.right = 1;
  StartVoice(audible, 0, source);
  auto carrier = fixture.Controls();
  carrier.right = 0;
  StartVoice(dry, 1, carrier);
  carrier.frequency_modulation = true;
  StartVoice(modulated, 1, carrier);
  StartVoice(audible, 1, carrier);
  std::array<float, 400> fm{}, speaker{}, unmodulated{};
  for (int tick = 0; tick < 10; ++tick) {
    RenderTick(modulated, fm);
    RenderTick(audible, speaker);
    RenderTick(dry, unmodulated);
  }
  double difference = 0;
  for (usize frame = 0; frame < 200; ++frame) {
    CHECK(fm[frame * 2] == speaker[frame * 2]);
    difference += std::abs(fm[frame * 2] - unmodulated[frame * 2]);
  }
  CHECK(difference > 1);
  CHECK(Energy(fm, 0) > 0.01);
  CHECK(Energy(fm, 1) == 0);
}

TEST_CASE("Production mono includes reverb and can restore stereo tails",
          "[audio][reverb][mono]") {
  AudioDeviceFixture fixture;
  TestsNativeNoiseChannel();
  TestsNativeMonoOutput(1);
  {
    OutputProbe probe(fixture.source.get());
    probe.Wait();
    const auto tail = std::span(probe.samples).last(4800);
    double difference = 0;
    for (usize frame = 0; frame < tail.size(); frame += 2) {
      difference += std::abs(tail[frame] - tail[frame + 1]);
    }
    CheckStereoAudible(tail);
    CHECK(difference == 0);
  }
  TestsNativeMonoOutput(0);
  {
    OutputProbe probe(fixture.source.get());
    probe.Wait();
    const auto tail = std::span(probe.samples).last(4800);
    double difference = 0;
    for (usize frame = 0; frame < tail.size(); frame += 2) {
      difference += std::abs(tail[frame] - tail[frame + 1]);
    }
    CHECK(difference > 0.01);
  }
  Audio_Native_Reset();
}

TEST_CASE("Voice zero ignores FM and zero pitch holds the current sample",
          "[audio][fm]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine plain, modulated;
  auto controls = fixture.Controls();
  StartVoice(plain, 0, controls);
  controls.frequency_modulation = true;
  StartVoice(modulated, 0, controls);
  std::array<float, 400> first{}, second{};
  for (int tick = 0; tick < 10; ++tick) {
    RenderTick(plain, first);
    RenderTick(modulated, second);
  }
  CHECK(first == second);
  controls.rate = 0;
  modulated.Configure(0, controls);
  RenderTick(modulated, second);
  for (const auto value : second) CHECK(value == first.back());
  controls.rate = 1;
  modulated.Configure(0, controls);
  RenderTick(modulated, second);
  CHECK(second.front() != second.back());
}

TEST_CASE("FM clamps positive modulation at the original maximum pitch",
          "[audio][fm]") {
  AudioFixture fixture;
  fixture.bank.instruments.push_back(fixture.bank.instruments.front());
  auto constant = fixture.bank.samples.front();
  std::ranges::fill(constant.pcm, 32767);
  fixture.bank.samples.push_back(std::move(constant));
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine modulated, plain;
  auto source = fixture.Controls();
  source.instrument = Content::AudioInstrumentPath(fixture.root, 0, 1);
  source.left = source.right = 0;
  StartVoice(modulated, 0, source);
  auto carrier = fixture.Controls();
  carrier.rate = 4;
  StartVoice(plain, 1, carrier);
  carrier.frequency_modulation = true;
  StartVoice(modulated, 1, carrier);
  std::array<float, 400> first{}, second{};
  for (int tick = 0; tick < 10; ++tick) {
    RenderTick(plain, first);
    RenderTick(modulated, second);
    CHECK(first == second);
  }
}

TEST_CASE("Noise mode can toggle without losing a prepared sample or envelope",
          "[audio][noise]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine switched, plain;
  auto controls = fixture.Controls();
  StartVoice(switched, 0, controls);
  StartVoice(plain, 0, controls);
  std::array<float, 400> first{}, second{};
  RenderTick(switched, first);
  RenderTick(plain, second);
  CHECK(first == second);
  controls.noise = true;
  switched.Configure(0, controls);
  switched.SetNoiseClock(63);
  RenderTick(switched, first);
  RenderTick(plain, second);
  CHECK(first != second);
  controls.noise = false;
  switched.Configure(0, controls);
  RenderTick(switched, first);
  RenderTick(plain, second);
  CHECK(first == second);
  CHECK(switched.EnvelopeLevel(0) == plain.EnvelopeLevel(0));
}

TEST_CASE("Every shipped reverb preset has a finite, distinct impulse response",
          "[audio][reverb]") {
  std::array<double, 10> energy{};
  for (usize preset = 1; preset < energy.size(); ++preset) {
    auto reverb = std::make_unique<Audio::Reverb>();
    reverb->Configure({static_cast<Audio::ReverbPreset>(preset), true, 1, 1},
                      true);
    bool finite = true;
    usize peak_frame = 0;
    float peak = 0;
    for (usize frame = 0; frame < 96000; ++frame) {
      const auto wet = reverb->Process({frame == 0 ? 0.5F : 0, 0});
      finite &= std::isfinite(wet.left) && std::isfinite(wet.right);
      energy[preset] += wet.left * wet.left + wet.right * wet.right;
      if (std::abs(wet.left) > peak) {
        peak = std::abs(wet.left);
        peak_frame = frame;
      }
    }
    CHECK(finite);
    CHECK(peak < 1);
    CHECK(energy[preset] > 0.001);
    if (preset == static_cast<usize>(Audio::ReverbPreset::kDelay)) {
      CHECK(peak_frame >= 35660);
      CHECK(peak_frame <= 35685);
    }
  }
  for (usize preset = 2; preset < energy.size(); ++preset) {
    CHECK(energy[preset] != energy[preset - 1]);
  }
}

TEST_CASE("Reverb depth is signed stereo gain and preset changes clear tails",
          "[audio][reverb]") {
  auto full = std::make_unique<Audio::Reverb>();
  auto scaled = std::make_unique<Audio::Reverb>();
  Audio::ReverbControls controls{Audio::ReverbPreset::kStudioSmall, true, 1, 1};
  full->Configure(controls, true);
  controls.left = 0.5F;
  controls.right = -0.25F;
  scaled->Configure(controls, true);
  double difference = 0;
  double energy = 0;
  for (usize frame = 0; frame < 16000; ++frame) {
    const Audio::StereoSample input{frame < 100 ? 0.2F : 0, 0};
    const auto original = full->Process(input);
    const auto wet = scaled->Process(input);
    difference += std::abs(wet.left - original.left * 0.5F) +
                  std::abs(wet.right + original.right * 0.25F);
    energy += wet.left * wet.left + wet.right * wet.right;
  }
  CHECK(difference == 0);
  CHECK(energy > 0.01);
  controls.preset = Audio::ReverbPreset::kHall;
  scaled->Configure(controls, false);
  double stale = 0;
  for (int frame = 0; frame < 48000; ++frame) {
    const auto wet = scaled->Process({});
    stale += wet.left * wet.left + wet.right * wet.right;
  }
  CHECK(stale == 0);
}

TEST_CASE("Disabled reverb cannot record new input into its delay network",
          "[audio][reverb]") {
  auto reverb = std::make_unique<Audio::Reverb>();
  Audio::ReverbControls controls{Audio::ReverbPreset::kHall, false, 1, 1};
  reverb->Configure(controls, true);
  double energy = 0;
  for (int frame = 0; frame < 48000; ++frame) {
    const auto wet = reverb->Process({frame < 1000 ? 0.5F : 0, 0});
    energy += wet.left * wet.left + wet.right * wet.right;
  }
  CHECK(energy == 0);
  controls.enabled = true;
  reverb->Configure(controls, false);
  for (int frame = 0; frame < 48000; ++frame) {
    const auto wet = reverb->Process({});
    energy += wet.left * wet.left + wet.right * wet.right;
  }
  CHECK(energy == 0);
}

TEST_CASE("Native reverb sends survive note stops but never playback reset",
          "[audio][reverb]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  engine.SetReverb({Audio::ReverbPreset::kStudioSmall, true, 0.5F, 0.5F});
  auto controls = fixture.Controls();
  controls.reverb = true;
  StartVoice(engine, 0, controls);
  std::array<float, 400> frames{};
  for (int tick = 0; tick < 30; ++tick) RenderTick(engine, frames);
  engine.Stop(0);
  RenderTick(engine, frames);
  CheckStereoAudible(frames);
  engine.Reset();
  RenderTick(engine, frames);
  CHECK(Energy(frames, 0) == 0);
  CHECK(Energy(frames, 1) == 0);
  Audio_Native_Reset();
  Audio_Native_SetReverbMode(4);
  CHECK(Audio_Native_ReverbMode() == 4);
  Audio_Native_Reset();
  CHECK(Audio_Native_ReverbMode() == 0);
}

TEST_CASE("sfizz preserves measured pitch, loop playback and the output clock",
          "[audio][synth]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  StartStereoPair(engine, fixture);
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
  for (int tick = 0; tick < 24; ++tick) RenderTick(engine, frames);
  std::array<int, 2> crossings{};
  std::array<float, 2> previous{};
  usize rendered_frames = 0;
  usize nonfinite_samples = 0;
  for (int tick = 0; tick < 240; ++tick) {
    RenderTick(engine, frames);
    rendered_frames += frames.size() / 2;
    for (usize index = 0; index < frames.size(); ++index) {
      const auto channel = index % 2;
      if (previous[channel] <= 0 && frames[index] > 0) ++crossings[channel];
      previous[channel] = frames[index];
      if (!std::isfinite(frames[index])) ++nonfinite_samples;
    }
  }
  CHECK(rendered_frames == Audio::VoiceEngine::kSampleRate);
  CHECK(nonfinite_samples == 0);
  CHECK(std::abs(crossings[0] - 400) <= 1);
  CHECK(std::abs(crossings[1] - 800) <= 1);
}

TEST_CASE("Device demand sustains real voices while the control thread waits",
          "[audio][device][stall]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  StartVoice(engine, 0, fixture.Controls());
  AudioDeviceFixture output;
  OutputProbe probe(output.source.get());
  bool demand = true;
  SECTION("device renders during a one-second control-thread stall") {
    REQUIRE(output.device.RenderFrom(engine));
    engine.CommitTick();
  }
  SECTION("push-only negative control runs out of samples") {
    demand = false;
    std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
    RenderTick(engine, frames);
    output.device.Submit(frames);
  }
  probe.Wait();
  constexpr usize kWarmupSamples =
      static_cast<usize>(Audio::AudioDevice::SampleRate) / 10 * 2;
  constexpr usize kBlockSamples = Audio::VoiceEngine::kFramesPerTick * 2;
  for (usize offset = kWarmupSamples; offset + kBlockSamples <= probe.count;
       offset += kBlockSamples) {
    const auto block = std::span(probe.samples).subspan(offset, kBlockSamples);
    if (demand) {
      CheckStereoAudible(block);
    } else {
      CHECK(Energy(block, 0) == 0);
      CHECK(Energy(block, 1) == 0);
    }
  }
}

TEST_CASE("Voice commands preserve tick boundaries and pending envelopes",
          "[audio][synth][commands]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  StartVoice(engine, 0, fixture.Controls());
  engine.CommitTick();
  CHECK(engine.EnvelopeLevel(0) > 0);
  engine.Stop(0);
  engine.CommitTick();
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
  engine.Render(frames);
  CheckStereoAudible(frames);
  engine.Render(frames);
  CHECK(Energy(frames, 0) == 0);
  CHECK(Energy(frames, 1) == 0);
}

TEST_CASE("Preparing replacement programs does not interrupt another voice",
          "[audio][device][stall]") {
  AudioFixture fixture;
  fixture.bank.instruments.push_back(fixture.bank.instruments.front());
  fixture.bank.samples.push_back(fixture.bank.samples.front());
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  StartStereoPair(engine, fixture);
  engine.CommitTick();
  AudioDeviceFixture output;
  REQUIRE(output.device.RenderFrom(engine));
  OutputProbe probe(output.source.get());
  REQUIRE(SDL_ResumeAudioDevice(probe.device));
  auto controls = fixture.Controls();
  controls.right = 0;
  for (usize index = 0; index < 50; ++index) {
    controls.instrument =
        Content::AudioInstrumentPath(fixture.root, 0, index % 2);
    StartVoice(engine, 0, controls);
    engine.CommitTick();
  }
  probe.Wait();
  for (usize offset = 9600; offset + 400 <= probe.count; offset += 400) {
    CHECK(Energy(std::span(probe.samples).subspan(offset, 400), 1) > 0.01);
  }
}

TEST_CASE("Rendering never consumes a partially prepared control tick",
          "[audio][synth][commands]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
  StartVoice(engine, 0, fixture.Controls());
  engine.Render(frames);
  CHECK(Energy(frames, 0) == 0);
  RenderTick(engine, frames);
  CheckStereoAudible(frames);
  engine.Stop(0);
  engine.Render(frames);
  CheckStereoAudible(frames);
  RenderTick(engine, frames);
  CHECK(Energy(frames, 0) == 0);
}

TEST_CASE("Demand rendering resets and reopens without stale voices",
          "[audio][device][stall]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  StartVoice(engine, 0, fixture.Controls());
  engine.CommitTick();
  AudioDeviceFixture output;
  REQUIRE(output.device.RenderFrom(engine));
  {
    OutputProbe probe(output.source.get());
    probe.Wait();
    CHECK(Energy(std::span(probe.samples).subspan(9600), 0) > 1);
    engine.Reset();
    output.device.Clear();
    probe.count = 0;
    probe.Wait();
    CHECK(Energy(probe.samples, 0) == 0);
    CHECK(Energy(probe.samples, 1) == 0);
  }
  output.Reopen();
  StartVoice(engine, 0, fixture.Controls());
  engine.CommitTick();
  REQUIRE(output.device.RenderFrom(engine));
  OutputProbe restarted(output.source.get());
  restarted.Wait();
  CHECK(Energy(std::span(restarted.samples).subspan(9600), 0) > 1);
}

TEST_CASE("Reset discards queued programs and supports command-ring reuse",
          "[audio][synth][commands]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
  StartVoice(engine, 0, fixture.Controls());
  engine.CommitTick();
  engine.Reset();
  engine.Render(frames);
  CHECK(Energy(frames, 0) == 0);
  StartVoice(engine, 0, fixture.Controls());
  for (int tick = 0; tick < 4200; ++tick) {
    engine.CommitTick();
    engine.Render(frames);
  }
  CheckStereoAudible(frames);
}

TEST_CASE("sfizz overlapping voices release independently", "[audio][synth]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  StartStereoPair(engine, fixture);
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
  RenderTick(engine, frames);
  RenderTick(engine, frames);
  CheckStereoAudible(frames);
  for (int tick = 0; tick < 240; ++tick) RenderTick(engine, frames);
  CheckStereoAudible(frames);
  CHECK(engine.EnvelopeLevel(0) > 0);
  engine.NoteOff(0);
  RenderTick(engine, frames);
  RenderTick(engine, frames);
  CHECK(Energy(frames, 0) == 0);
  CHECK(Energy(frames, 1) > 0.01);
  CHECK(engine.EnvelopeLevel(0) == 0);
}

TEST_CASE("Native voice reset clears playback and can restart with mono gains",
          "[audio][synth]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  auto controls = fixture.Controls();
  controls.right = 0;
  StartVoice(engine, 0, controls);
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
  RenderTick(engine, frames);
  RenderTick(engine, frames);
  CHECK(Energy(frames, 0) > 0.01);
  CHECK(Energy(frames, 1) == 0);
  engine.Reset();
  engine.Render(frames);
  CHECK(Energy(frames, 0) == 0);
  CHECK(Energy(frames, 1) == 0);

  controls.left = controls.right = 1;
  StartVoice(engine, 0, controls);
  RenderTick(engine, frames);
  CHECK(Energy(frames, 0) > 0.01);
  CHECK(Energy(frames, 0) == Energy(frames, 1));
}

TEST_CASE("Missing SFZ fails instead of replaying the previous instrument",
          "[audio][synth]") {
  AudioFixture fixture;
  REQUIRE(Content::ExportAudioBank(fixture.bank, fixture.root));
  Audio::VoiceEngine engine;
  auto controls = fixture.Controls();
  StartVoice(engine, 0, controls);
  std::array<float, Audio::VoiceEngine::kFramesPerTick * 2> frames{};
  RenderTick(engine, frames);
  CHECK(Energy(frames, 0) > 0.01);
  controls.instrument = fixture.root / "missing.sfz";
  engine.Configure(0, controls);
  CHECK_FALSE(engine.NoteOn(0));
  RenderTick(engine, frames);
  CHECK(Energy(frames, 0) == 0);
  CHECK(Energy(frames, 1) == 0);
  StartVoice(engine, 0, fixture.Controls());
  RenderTick(engine, frames);
  CHECK(Energy(frames, 0) > 0.01);
}
