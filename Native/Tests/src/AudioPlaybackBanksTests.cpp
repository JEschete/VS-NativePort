#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/SampleBanks.hpp"
#include "Audio/SampleBanksLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Audio/SoundPlayback.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Content/CdDrive.hpp"
#include "Content/ModuleData.h"

extern "C" {
#include "Audio/SoundBanks.h"
}

namespace {
namespace Audio = VS::Audio;
namespace Content = VS::Content;
namespace Playback = VS::Audio::Playback;
namespace Samples = VS::Audio::Samples;
namespace Seq = VS::Audio::Seq;
using Content::DiscRequestState;
using Playback::Command;
using Playback::CommandArguments;
using Playback::DataRef;
using Playback::PlaybackRefusal;

void PutWord(std::span<std::byte> bytes, std::size_t word,
             std::uint32_t value) {
  std::memcpy(bytes.subspan(word * sizeof(value), sizeof(value)).data(), &value,
              sizeof(value));
}

// The opaque handle enums name only kNone; the fakes' handles are made here.
template <typename Handle, typename Value>
Handle MakeHandle(Value value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Handle>(value);
}

DataRef Data(std::uintptr_t address) { return MakeHandle<DataRef>(address); }

Playback::EffectSelector Selector(std::intptr_t id) {
  return MakeHandle<Playback::EffectSelector>(id);
}

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
const T& Required(const std::optional<T>& value) {
  REQUIRE(value);
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

// ---------------------------------------------------------------------------
// Playback front end

struct IssuedCommand {
  Command command = Command::kStartMusic;
  CommandArguments arguments;

  friend bool operator==(const IssuedCommand&, const IssuedCommand&) = default;
};

class FakePlayback final : public Playback::PlaybackHost {
 public:
  std::vector<std::string> calls;
  std::vector<IssuedCommand> dispatched;
  std::vector<PlaybackRefusal> refusals;
  std::vector<std::pair<std::uint32_t, std::int32_t>> volumes;
  Playback::CommandResult result;
  Playback::PlaybackStatus status;
  std::array<Playback::EffectVoice, Playback::kEffectVoiceCount> voices{};
  std::vector<std::byte> program;
  std::optional<DataRef> program_asked;
  std::optional<Playback::EffectTables> tables;
  std::optional<Playback::EffectProgramBlob> bound;
  bool bind_succeeds = true;
  Playback::MusicBankIds banks;

  void StartDriver() override { calls.emplace_back("start"); }
  void StopDriver() override { calls.emplace_back("stop"); }
  Playback::CommandResult Dispatch(Command command,
                                   const CommandArguments& arguments) override {
    dispatched.push_back({.command = command, .arguments = arguments});
    return result;
  }
  void SetMusicVolume(std::uint32_t music_id, std::int32_t volume) override {
    volumes.emplace_back(music_id, volume);
  }
  Playback::PlaybackStatus Status() override { return status; }
  Playback::EffectVoice EffectVoiceAt(std::size_t voice) override {
    return voices.at(voice);
  }
  std::span<const std::byte> ProgramBytes(DataRef data) override {
    program_asked = data;
    return program;
  }
  std::optional<Playback::EffectTables> Tables() override { return tables; }
  bool BindTables(const Playback::EffectProgramBlob& blob) override {
    bound = blob;
    return bind_succeeds;
  }
  void ClearSavedMusicId() override { calls.emplace_back("clear-saved"); }
  Playback::MusicBankIds MusicBanks() override { return banks; }
  void Refuse(PlaybackRefusal refusal) override { refusals.push_back(refusal); }
};

std::vector<std::byte> AkaoBytes(std::size_t size) {
  std::vector<std::byte> bytes(size);
  PutWord(bytes, 0, Playback::kAkaoMagic);
  return bytes;
}
}  // namespace

TEST_CASE("Shared sound effects play from the bound tables only",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  std::array<std::uint32_t, 3> offsets{0x40, Playback::kNoEffectProgram, 0x80};
  std::array<std::uint16_t, 3> selectors{0, 0x8000, 0};
  CHECK(Playback::PlaySoundEffect(host, 2, 1, 2, 3) == -1);
  CHECK(host.refusals == std::vector{PlaybackRefusal::kEffectTablesUnbound});
  host.tables = Playback::EffectTables{.program_offsets = offsets,
                                       .bank_selectors = selectors};
  // The id keeps its low ten bits; tag, pan and volume keep their masks.
  CHECK(Playback::PlaySoundEffect(host, 0x402, 0x7123456, 0x1FF, 0xFF) == 0x80);
  CHECK(host.dispatched ==
        std::vector<IssuedCommand>{{.command = Command::kPlaySoundEffect,
                                    .arguments = {.first = std::intptr_t{2},
                                                  .second = 0x123456U,
                                                  .third = 0xFFU,
                                                  .fourth = 0x7FU}}});
  // An effect with no program returns none and dispatches nothing.
  CHECK(Playback::PlaySoundEffect(host, 1, 0, 0, 0) == -1);
  CHECK(host.dispatched.size() == 1);
  // An id past the bound tables is refused rather than read past them.
  CHECK(Playback::PlaySoundEffect(host, 3, 0, 0, 0) == -1);
  CHECK(host.refusals.back() == PlaybackRefusal::kEffectOutOfRange);
  CHECK(host.dispatched.size() == 1);
}

TEST_CASE("Effect programs play only when readable and AKAO",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  const DataRef program = Data(0x4000);
  host.program = std::vector<std::byte>(3);
  CHECK_FALSE(Playback::PlaySoundEffectBlob(host, program, 1, 2, 3));
  CHECK(host.refusals == std::vector{PlaybackRefusal::kProgramUnreadable});
  CHECK(host.program_asked == program);
  host.program = AkaoBytes(16);
  CHECK(Playback::PlaySoundEffectBlob(host, program, 0x1000005, 0x180, 0x80) ==
        program);
  CHECK(
      host.dispatched ==
      std::vector<IssuedCommand>{
          {.command = Command::kPlaySoundEffectProgram,
           .arguments = {
               .first = program, .second = 5U, .third = 0x80U, .fourth = 0U}}});
  host.program[0] = std::byte{'B'};
  CHECK_FALSE(Playback::PlaySoundEffectBlob(host, program, 0, 0, 0));
  CHECK(host.dispatched.size() == 1);
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Effect tag use reads a program's flags byte",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  const DataRef program = Data(0x4000);
  host.program = std::vector<std::byte>(Playback::kProgramFlagsByte);
  CHECK(Playback::UsesSoundEffectTags(host, program) == 0);
  CHECK(host.refusals == std::vector{PlaybackRefusal::kProgramUnreadable});
  host.program.push_back(std::byte{0x80});
  CHECK(Playback::UsesSoundEffectTags(host, program) == 1);
  host.program.back() = std::byte{0x7F};
  CHECK(Playback::UsesSoundEffectTags(host, program) == 0);
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Effect tag use reads a shared effect's bound selector",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  const Playback::SharedEffect tagged{.id = 1};
  CHECK(Playback::UsesSoundEffectTags(host, tagged) == 0);
  CHECK(host.refusals == std::vector{PlaybackRefusal::kEffectTablesUnbound});
  std::array<std::uint32_t, 2> offsets{0, 0};
  std::array<std::uint16_t, 2> selectors{0x7FFF, 0x8001};
  host.tables = Playback::EffectTables{.program_offsets = offsets,
                                       .bank_selectors = selectors};
  CHECK(Playback::UsesSoundEffectTags(host, tagged) == 1);
  CHECK(Playback::UsesSoundEffectTags(host, Playback::SharedEffect{}) == 0);
  CHECK(Playback::UsesSoundEffectTags(host, Playback::SharedEffect{.id = 2}) ==
        0);
  CHECK(host.refusals.back() == PlaybackRefusal::kEffectOutOfRange);
  CHECK(host.refusals.size() == 2);
}

TEST_CASE("Effect blobs shorter than their tables bind nothing",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  std::vector<std::byte> blob =
      AkaoBytes(Playback::kEffectProgramDataStart + 8);
  const std::span<std::byte> bytes(blob);
  CHECK(Playback::BindSoundEffectPrograms(host, bytes.first(3)) == -1);
  CHECK(host.refusals == std::vector{PlaybackRefusal::kBlobTooSmall});
  CHECK(Playback::BindSoundEffectPrograms(
            host, bytes.first(Playback::kEffectProgramDataStart - 1)) == 0);
  CHECK(host.refusals == std::vector(2, PlaybackRefusal::kBlobTooSmall));
  CHECK_FALSE(host.bound);
}

TEST_CASE("Effect blobs bind the tables behind their AKAO header",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  std::vector<std::byte> blob =
      AkaoBytes(Playback::kEffectProgramDataStart + 8);
  CHECK(Playback::BindSoundEffectPrograms(host, blob) == 0);
  const Playback::EffectProgramBlob& bound = Required(host.bound);
  CHECK(bound.program_offsets.data() == blob.data() + 0x10);
  CHECK(bound.program_offsets.size() == 0x600);
  CHECK(bound.bank_selectors.data() == blob.data() + 0x610);
  CHECK(bound.bank_selectors.size() == 0x300);
  CHECK(bound.program_data.data() == blob.data() + 0x910);
  CHECK(bound.program_data.size() == 8);
  CHECK(host.refusals.empty());
}

TEST_CASE("Effect blobs that are misaligned or not AKAO are refused",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  std::vector<std::byte> blob =
      AkaoBytes(Playback::kEffectProgramDataStart + 8);
  host.bind_succeeds = false;
  CHECK(Playback::BindSoundEffectPrograms(host, blob) == 0);
  CHECK(host.refusals == std::vector{PlaybackRefusal::kBlobMisaligned});
  // A blob that is not AKAO returns its magic difference and binds nothing.
  host.bound.reset();
  blob[0] = std::byte{'B'};
  CHECK(Playback::BindSoundEffectPrograms(host, blob) == 1);
  CHECK_FALSE(host.bound);
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Playback flags combine the music layers, voices and stream",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  CHECK(Playback::ActivePlaybackFlags(host) == 0);
  host.status.active.active_channels = 1;
  CHECK(Playback::ActivePlaybackFlags(host) == 1);
  host.status.secondary = Seq::MusicLayerState{};
  CHECK(Playback::ActivePlaybackFlags(host) == 1);
  Seq::MusicLayerState secondary{};
  secondary.active_channels = 4;
  host.status.secondary = secondary;
  host.status.transition_queued = true;
  host.status.active_voices = 0x1000;
  host.status.stream_voices = 2;
  CHECK(Playback::ActivePlaybackFlags(host) == 0x10113);
}

TEST_CASE("Music volume reads the layer that plays the music id",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  host.status.active.music_id = 7;
  host.status.active.master_volume = 0x7F0000;
  Seq::MusicLayerState secondary{};
  secondary.music_id = 9;
  secondary.master_volume = 0x400000;
  host.status.secondary = secondary;
  CHECK(Playback::MusicVolume(host, 7) == 0x7F);
  CHECK(Playback::MusicVolume(host, 9) == 0x40);
  CHECK(Playback::MusicVolume(host, 8) == 0);
}

TEST_CASE("Music queries pack the bank ids, longest fade and position",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  host.banks = {.bank = 0x12345, .alternate = 0x6789A};
  CHECK(Playback::PackedMusicSampleBankIds(host) == 0x789A2345);

  host.status.active.master_volume_slide_length = 5;
  host.status.tempo_slide_length = 9;
  host.status.pitch_slide_length = 7;
  CHECK(Playback::LongestMusicFade(host, 0) == 0);
  CHECK(Playback::LongestMusicFade(host, 1) == 5);
  CHECK(Playback::LongestMusicFade(host, 5) == 7);
  CHECK(Playback::LongestMusicFade(host, 7) == 9);

  host.status.active.timer_top = 0x12;
  host.status.active.timer_upper_count = 0x33;
  CHECK(Playback::MusicPosition(host) == 0x120034);
}

TEST_CASE("Music starts dispatch their commands and arguments",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  host.result = {.value = 0x21, .first = 4, .second = 5};
  const DataRef sequence = Data(0x10);
  CHECK(Playback::StartMusic(host, sequence) == 0x21);
  Playback::StopMusic(host, 3);
  Playback::SaveMusicState(host);
  CHECK(Playback::StartLayeredMusic(host, sequence, 0x40) == 0x21);
  const std::vector<IssuedCommand> expected{
      {.command = Command::kStartMusic, .arguments = {.first = sequence}},
      {.command = Command::kStopMusic,
       .arguments = {.first = std::intptr_t{3}}},
      {.command = Command::kSaveMusic, .arguments = {}},
      {.command = Command::kStartLayeredMusic,
       .arguments = {.first = sequence}},
  };
  CHECK(host.dispatched == expected);
  CHECK(host.volumes ==
        std::vector<std::pair<std::uint32_t, std::int32_t>>{{0x21, 0x40}});
}

TEST_CASE("Music transitions dispatch their commands and arguments",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  host.result = {.value = 0x21, .first = 4, .second = 5};
  const DataRef sequence = Data(0x10);
  const DataRef second = Data(0x20);
  CHECK(Playback::CrossfadeMusic(host, sequence, 30, 0xFF) == 0x21);
  Playback::StartMusicAtLoop(host, sequence, 2);
  CHECK(Playback::QueueMusicTransition(host, sequence) == 0x21);
  CHECK(Playback::StartMusicPair(host, sequence, second) ==
        Playback::MusicPair{.first = 4, .second = 5});
  Playback::RequestMusicLayerSwap(host);
  const std::vector<IssuedCommand> expected{
      {.command = Command::kCrossfadeMusic,
       .arguments = {.first = sequence, .second = 30U, .third = 0x7FU}},
      {.command = Command::kStartMusicAtLoop,
       .arguments = {.first = sequence, .second = 2U}},
      {.command = Command::kQueueMusicTransition,
       .arguments = {.first = sequence}},
      {.command = Command::kStartMusicPair,
       .arguments = {.first = sequence, .second_sequence = second}},
      {.command = Command::kSwapMusicLayers, .arguments = {}},
  };
  CHECK(host.dispatched == expected);
}

TEST_CASE("Playback starts and stops the driver and clears the saved music",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  CHECK(Playback::Initialize(host) == 0);
  CHECK(Playback::InvalidateSavedMusicId(host) == 0);
  CHECK(Playback::Shutdown(host) == 0);
  CHECK(host.calls == std::vector<std::string>{"start", "clear-saved", "stop"});
  CHECK(host.dispatched.empty());
}

TEST_CASE("Effect stops, interface sounds and output modes keep their masks",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  Playback::StopSoundEffects(host, Audio::kUntaggedEffects, 0x1FF000F);
  Playback::PlayInterfaceSound(host, 0x7FF);
  Playback::SetMonoOutput(host, Playback::kMonoRequested);
  Playback::SetMonoOutput(host, 0);
  Playback::SetMonoOutput(host, 2);
  const std::vector<IssuedCommand> expected{
      {.command = Command::kStopSoundEffects,
       .arguments = {.first = std::intptr_t{-2}, .second = 0xFF000FU}},
      {.command = Command::kPlayInterfaceSound,
       .arguments = {.first = std::intptr_t{0x3FF}}},
      {.command = Command::kMonoOutput, .arguments = {}},
      {.command = Command::kStereoOutput, .arguments = {}},
      {.command = Command::kStereoOutput, .arguments = {}},
  };
  CHECK(host.dispatched == expected);
}

TEST_CASE("Effect voice queries scan the twelve voices from voice bit 12",
          "[audio-playback-banks][audio]") {
  FakePlayback host;
  host.voices[0] = {.tags = 0x1000, .effect = Selector(11)};
  host.voices[3] = {.tags = 0x2000002, .effect = Selector(22)};
  host.voices[11] = {.tags = 4, .effect = Selector(33)};
  CHECK(Playback::ActiveSoundEffectTags(host) == 0);
  CHECK(Playback::SoundEffectIdForTag(host, 4) ==
        Playback::EffectSelector::kNone);
  // Voice bit 0 is a music voice; its record is never read.
  host.status.active_voices = 0x1 | 0x1000 | 0x8000 | 0x800000;
  CHECK(Playback::ActiveSoundEffectTags(host) == 0x1006);
  CHECK(Playback::SoundEffectIdForTag(host, 4) == Selector(33));
  CHECK(Playback::SoundEffectIdForTag(host, 0x1000) == Selector(11));
  CHECK(Playback::SoundEffectIdForTag(host, 0) ==
        Playback::EffectSelector::kNone);
  CHECK(Playback::SoundEffectIdForTag(host, 7) ==
        Playback::EffectSelector::kNone);
  host.status.active_voices = 0x1000 | 0x8000;
  CHECK(Playback::SoundEffectIdForTag(host, 4) ==
        Playback::EffectSelector::kNone);
}

// ---------------------------------------------------------------------------
// Sample banks

namespace {
struct Upload {
  std::size_t bytes = 0;
  bool wait = false;
  Samples::BankDestination destination;

  friend bool operator==(const Upload&, const Upload&) = default;
};

struct SampleWrite {
  std::uint32_t address = 0;
  std::size_t bytes = 0;

  friend bool operator==(const SampleWrite&, const SampleWrite&) = default;
};

class FakeBanks final : public Samples::BankLoadHost {
 public:
  Samples::MusicBankState music;
  Samples::SoundEffectBankIds effects{};
  std::vector<Upload> uploads;
  std::int32_t transfer = 0;
  Samples::BankStreamState stream;
  std::array<Samples::InstrumentRecord, Samples::kInstrumentSlotCount>
      instruments{};
  std::vector<SampleWrite> sample_writes;
  int transfer_waits = 0;
  std::vector<Samples::BankRefusal> refusals;

  Samples::MusicBankState MusicBanks() override { return music; }
  void StoreMusicBanks(const Samples::MusicBankState& state) override {
    music = state;
  }
  Samples::SoundEffectBankIds SoundEffectBanks() override { return effects; }
  void StoreSoundEffectBanks(const Samples::SoundEffectBankIds& ids) override {
    effects = ids;
  }
  void UploadBank(std::span<const std::byte> bank, bool wait,
                  const Samples::BankDestination& destination) override {
    uploads.push_back(
        {.bytes = bank.size(), .wait = wait, .destination = destination});
  }
  std::int32_t TransferStatus() override { return transfer; }
  Samples::BankStreamState Stream() override { return stream; }
  void StoreStream(const Samples::BankStreamState& state) override {
    stream = state;
  }
  void WriteInstrumentBytes(std::size_t offset,
                            std::span<const std::byte> bytes) override {
    const std::span<std::byte> table =
        std::as_writable_bytes(std::span(instruments));
    if (offset > table.size() || bytes.size() > table.size() - offset) {
      throw std::out_of_range("instrument bytes past the table");
    }
    std::memcpy(table.subspan(offset).data(), bytes.data(), bytes.size());
  }
  Samples::InstrumentRecord Instrument(std::size_t slot) override {
    return instruments.at(slot);
  }
  void StoreInstrument(std::size_t slot,
                       const Samples::InstrumentRecord& record) override {
    instruments.at(slot) = record;
  }
  void WriteSampleMemory(std::uint32_t address,
                         std::span<const std::byte> bytes) override {
    sample_writes.push_back({.address = address, .bytes = bytes.size()});
  }
  void WaitForSampleTransfer() override { ++transfer_waits; }
  void Refuse(Samples::BankRefusal refusal) override {
    refusals.push_back(refusal);
  }
};

struct HeaderWords {
  std::uint32_t bank_id = 0;
  std::uint32_t sample_address = 0;
  std::uint32_t sample_bytes = 0;
  std::uint32_t first_instrument = 0;
  std::uint32_t instrument_count = 0;
};

std::vector<std::byte> BankBytes(const HeaderWords& words, std::size_t size) {
  std::vector<std::byte> bytes = AkaoBytes(size);
  PutWord(bytes, 1, words.bank_id);
  PutWord(bytes, Samples::kHeaderSampleAddressWord, words.sample_address);
  PutWord(bytes, Samples::kHeaderSampleBytesWord, words.sample_bytes);
  PutWord(bytes, Samples::kHeaderFirstInstrumentWord, words.first_instrument);
  PutWord(bytes, Samples::kHeaderInstrumentCountWord, words.instrument_count);
  return bytes;
}

Samples::BankDeclaration Declared(std::int32_t bank_id,
                                  std::uint32_t instrument_count) {
  Samples::BankDeclaration bank;
  bank.bank_id = bank_id;
  bank.instrument_count = instrument_count;
  bank.sample_bytes = 0x1000;
  return bank;
}

std::vector<std::byte> HeaderBank() {
  return BankBytes({.bank_id = 7,
                    .sample_address = 0x35100,
                    .sample_bytes = 0x100,
                    .first_instrument = 0x40,
                    .instrument_count = 4},
                   Samples::kBankHeaderBytes);
}

// A header, two instrument records and the first 16 sample bytes.
std::vector<std::byte> StreamChunk() {
  std::vector<std::byte> chunk = BankBytes({.bank_id = 3,
                                            .sample_address = 0x50000,
                                            .sample_bytes = 0x20,
                                            .first_instrument = 0x10,
                                            .instrument_count = 2},
                                           64 + 32 + 16);
  PutWord(chunk, 16, 0x100);
  PutWord(chunk, 17, 0x180);
  PutWord(chunk, 20, 0x200);
  PutWord(chunk, 21, 0x280);
  return chunk;
}

std::array<Samples::InstrumentRecord, 3> RelocationSource() {
  std::array<Samples::InstrumentRecord, 3> source{};
  for (std::size_t i = 0; i < source.size(); ++i) {
    source.at(i).sample_start = static_cast<std::uint32_t>(0x100 * i);
    source.at(i).loop_start = static_cast<std::uint32_t>(0x100 * i + 0x80);
    source.at(i).fine_tune = static_cast<std::int16_t>(i);
  }
  return source;
}
}  // namespace

TEST_CASE("Music banks load into the music bank, then the alternate",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  const Samples::BankDeclaration bank = Declared(7, 0x10);
  host.music = {.active_channels = 1,
                .music_flags = 0,
                .music_bank_selected = false,
                .bank_id = 3,
                .alternate_bank_id = 7};
  Samples::MusicBankChoice choice =
      Samples::ChooseMusicBankDestination(host, bank);
  CHECK_FALSE(choice.alternate);
  CHECK(choice.destination == Samples::kMusicBank);
  CHECK(host.music == Samples::MusicBankState{.active_channels = 1,
                                              .music_flags = 0,
                                              .music_bank_selected = true,
                                              .bank_id = 7,
                                              .alternate_bank_id = 0});
  // A selected music bank that the music does not play from takes the next
  // bank into the alternate.
  choice = Samples::ChooseMusicBankDestination(host, bank);
  CHECK(choice.alternate);
  CHECK(choice.destination == Samples::kAlternateMusicBank);
  CHECK(host.music.bank_id == 0);
  CHECK(host.music.alternate_bank_id == 7);
  CHECK_FALSE(host.music.music_bank_selected);
}

TEST_CASE("Music bank flags choose the alternate only while music plays",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  const Samples::BankDeclaration bank = Declared(7, 0x10);
  host.music = {.active_channels = 0,
                .music_flags = Samples::kLoadIntoAlternateBank,
                .music_bank_selected = false,
                .bank_id = 0,
                .alternate_bank_id = 0};
  // Music with no active channels drops its bank flags.
  CHECK_FALSE(Samples::ChooseMusicBankDestination(host, bank).alternate);
  CHECK(host.music.music_flags == 0);
  host.music.active_channels = 1;
  host.music.music_flags = Samples::kLoadIntoAlternateBank;
  host.music.music_bank_selected = false;
  CHECK(Samples::ChooseMusicBankDestination(host, bank).alternate);
  CHECK(host.music.alternate_bank_id == 7);
  CHECK(host.music.bank_id == 0);
}

TEST_CASE("Banks larger than one music bank take it and clear both ids",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  host.music = {.active_channels = 1,
                .music_flags = Samples::kLoadIntoAlternateBank,
                .music_bank_selected = true,
                .bank_id = 3,
                .alternate_bank_id = 9};
  const Samples::MusicBankChoice choice =
      Samples::ChooseMusicBankDestination(host, Declared(5, 0x21));
  CHECK_FALSE(choice.alternate);
  CHECK(choice.destination == Samples::kMusicBank);
  CHECK(host.music ==
        Samples::MusicBankState{.active_channels = 1,
                                .music_flags = Samples::kLoadIntoAlternateBank,
                                .music_bank_selected = false,
                                .bank_id = 5,
                                .alternate_bank_id = 0});
}

TEST_CASE("Sound-effect banks take their slot and drop the duplicate id",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  host.effects = {5, 9, 5};
  CHECK(Samples::ChooseSoundEffectBankDestination(host, Declared(5, 1), 2) ==
        Samples::kSoundEffectBanks[2]);
  CHECK(host.effects == Samples::SoundEffectBankIds{0, 9, 5});
  CHECK(Samples::ChooseSoundEffectBankDestination(host, Declared(5, 1), 7) ==
        Samples::kSoundEffectBanks[0]);
  CHECK(host.effects == Samples::SoundEffectBankIds{5, 9, 0});
  CHECK(Samples::SoundEffectBankSlot(host, 0x8009) == 1);
  // The first bank is never a lookup's answer.
  CHECK(Samples::SoundEffectBankSlot(host, 0x8005) == 0);
  // Bank id 0 names no bank even where an empty slot holds 0.
  CHECK(Samples::SoundEffectBankSlot(host, 0x8000) == 0);
}

TEST_CASE("Music bank selection and clearing keep the bank ids",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  host.music = {.active_channels = 0,
                .music_flags = 0,
                .music_bank_selected = false,
                .bank_id = 4,
                .alternate_bank_id = 6};
  CHECK(Samples::SelectMusicBank(host, 4));
  CHECK_FALSE(Samples::SelectMusicBank(host, 6));
  CHECK_FALSE(Samples::SelectMusicBank(host, 9));
  host.music.music_bank_selected = true;
  CHECK(Samples::SelectMusicBank(host, 9));
  Samples::ClearMusicBankIds(host);
  CHECK(host.music == Samples::MusicBankState{});
}

TEST_CASE("Declared banks upload where their header names",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  host.transfer = -1;
  const std::vector<std::byte> bank = HeaderBank();
  std::vector<std::byte> other = bank;
  other[0] = std::byte{'B'};
  const std::vector<std::byte> truncated = AkaoBytes(16);
  CHECK(Samples::LoadDeclaredBank(host, bank, true) == 0);
  CHECK(Samples::LoadDeclaredBank(host, other, true) == -1);
  CHECK(Samples::LoadDeclaredBank(host, truncated, true) == -1);
  Samples::LoadBank(host, bank, false);
  CHECK(Samples::TransferStatus(host) == -1);
  const std::vector<Upload> expected{
      {.bytes = 32, .wait = true, .destination = Samples::kMusicBank},
      {.bytes = 32, .wait = false, .destination = Samples::kMusicBank}};
  CHECK(host.uploads == expected);
  CHECK(host.refusals == std::vector{Samples::BankRefusal::kHeaderTruncated});
}

TEST_CASE("Music and sound-effect banks upload where the choice names",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  const std::vector<std::byte> bank = HeaderBank();
  const std::vector<std::byte> truncated = AkaoBytes(16);
  host.music.music_bank_selected = true;
  CHECK(Samples::LoadMusicBank(host, bank, true));
  CHECK_FALSE(Samples::LoadMusicBank(host, truncated, true));
  Samples::LoadSoundEffectBank(host, bank, 1, false);
  Samples::LoadSoundEffectBank(host, truncated, 1, false);
  const std::vector<Upload> expected{
      {.bytes = 32, .wait = true, .destination = Samples::kAlternateMusicBank},
      {.bytes = 32,
       .wait = false,
       .destination = Samples::kSoundEffectBanks[1]}};
  CHECK(host.uploads == expected);
  CHECK(host.effects == Samples::SoundEffectBankIds{0, 7, 0});
  CHECK(host.refusals == std::vector<Samples::BankRefusal>(
                             2, Samples::BankRefusal::kHeaderTruncated));
}

TEST_CASE("Streamed bank headers relocate the instruments once all arrived",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  Samples::BeginBankStream(host);
  CHECK(host.stream.active);
  CHECK(Samples::UploadBankChunk(host, StreamChunk(), 0, true) == 0x10);
  // The instruments move by the bank's sample address.
  CHECK(host.instruments[0x10].sample_start == 0x50100);
  CHECK(host.instruments[0x10].loop_start == 0x50180);
  CHECK(host.instruments[0x11].sample_start == 0x50200);
  CHECK(host.instruments[0x11].loop_start == 0x50280);
  CHECK(host.stream.active);
  CHECK(host.sample_writes ==
        std::vector<SampleWrite>{{.address = 0x50000, .bytes = 16}});
}

TEST_CASE("Streamed bank samples upload chunk by chunk until the stream ends",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  Samples::BeginBankStream(host);
  CHECK(Samples::UploadBankChunk(host, StreamChunk(), 0, true) == 0x10);
  const std::vector<std::byte> rest(16);
  CHECK(Samples::UploadBankChunk(host, rest, 0, false) == 0);
  CHECK_FALSE(host.stream.active);
  CHECK(host.sample_writes ==
        std::vector<SampleWrite>{{.address = 0x50000, .bytes = 16},
                                 {.address = 0x50010, .bytes = 16}});
  CHECK(host.transfer_waits == 1);
  // An ended stream uploads nothing more.
  CHECK(Samples::UploadBankChunk(host, rest, 0, true) == 0);
  CHECK(host.sample_writes.size() == 2);
  CHECK(host.refusals.empty());
}

TEST_CASE("Streamed banks refuse a short header or instruments past the table",
          "[audio-playback-banks][audio]") {
  FakeBanks host;
  Samples::BeginBankStream(host);
  CHECK(Samples::UploadBankChunk(host, AkaoBytes(40), 0, true) == 0);
  CHECK_FALSE(host.stream.active);
  CHECK(host.refusals == std::vector{Samples::BankRefusal::kHeaderTruncated});

  Samples::BeginBankStream(host);
  const std::vector<std::byte> past = BankBytes({.bank_id = 3,
                                                 .sample_address = 0x50000,
                                                 .sample_bytes = 0x20,
                                                 .first_instrument = 0xF0,
                                                 .instrument_count = 0x20},
                                                64);
  CHECK(Samples::UploadBankChunk(host, past, 0, true) == 0);
  CHECK(host.refusals.back() == Samples::BankRefusal::kInstrumentsOutOfRange);
  CHECK_FALSE(host.stream.active);

  // A chunk that does not start an AKAO bank ends the stream unrefused.
  Samples::BeginBankStream(host);
  CHECK(Samples::UploadBankChunk(host, std::vector<std::byte>(64), 0, true) ==
        0);
  CHECK(host.refusals.size() == 2);
  CHECK(host.sample_writes.empty());
}

TEST_CASE("Relocated instrument copies need a positive count",
          "[audio-playback-banks][audio]") {
  const std::array<Samples::InstrumentRecord, 3> source = RelocationSource();
  std::array<Samples::InstrumentRecord, 2> destination{};
  CHECK(Samples::CopyRelocatedInstruments(source, destination, 0x1000, 0) == 0);
  CHECK(Samples::CopyRelocatedInstruments(source, destination, 0x1000, -1) ==
        0);
  CHECK(destination[0].sample_start == 0);
  CHECK(destination[1].sample_start == 0);
  CHECK(destination[1].loop_start == 0);
}

TEST_CASE("Relocated instrument copies stop at either span's end",
          "[audio-playback-banks][audio]") {
  const std::array<Samples::InstrumentRecord, 3> source = RelocationSource();
  std::array<Samples::InstrumentRecord, 2> destination{};
  CHECK(Samples::CopyRelocatedInstruments(source, destination, 0x1000, 5) == 2);
  CHECK(destination[0].sample_start == 0x1000);
  CHECK(destination[1].sample_start == 0x1100);
  CHECK(destination[1].loop_start == 0x1180);
  CHECK(destination[1].fine_tune == 1);
  CHECK(Samples::CopyRelocatedInstruments(std::span(source).first(1),
                                          destination, 0, 3) == 1);
  CHECK(destination[0].sample_start == 0);
  CHECK(destination[1].sample_start == 0x1100);
}

TEST_CASE("AKAO magic differences need a whole word",
          "[audio-playback-banks][audio]") {
  std::vector<std::byte> bytes = AkaoBytes(4);
  CHECK_FALSE(Samples::SequenceMagicDifference(std::span(bytes).first(3)));
  CHECK(Samples::SequenceMagicDifference(bytes) == 0);
  bytes[0] = std::byte{'B'};
  CHECK(Samples::SequenceMagicDifference(bytes) == 1);
  CHECK(Samples::LiveSequenceMagicDifference(nullptr) != 0);
}

TEST_CASE("Music instruments move banks only when music plays the alternate",
          "[audio-playback-banks][audio]") {
  Samples::ChannelInstrument channel{
      .instrument_index = 0x45, .sample_start = 0x35200, .loop_start = 0x35300};
  CHECK(Samples::MapInstrumentToAlternateBank(0, channel) == 0x45);
  CHECK(channel.sample_start == 0x35200);
  CHECK(Samples::MapInstrumentToAlternateBank(0x40, channel) == 0x65);
  CHECK(channel == Samples::ChannelInstrument{.instrument_index = 0x65,
                                              .sample_start = 0x55200,
                                              .loop_start = 0x55300});
  CHECK(Samples::MapInstrumentToAlternateBank(0x40, channel) == 0x65);
  CHECK(Samples::MapInstrumentToBaseBank(0, channel) == 0x65);
  CHECK(Samples::MapInstrumentToBaseBank(0x40, channel) == 0x45);
  CHECK(channel.sample_start == 0x35200);
  CHECK(Samples::MapInstrumentToBaseBank(0x40, channel) == 0x45);
}

TEST_CASE("Effect and low instruments never move banks",
          "[audio-playback-banks][audio]") {
  Samples::ChannelInstrument effect{.instrument_index = 0x80};
  CHECK(Samples::MapInstrumentToAlternateBank(0x40, effect) == 0x80);
  CHECK(Samples::MapInstrumentToBaseBank(0x40, effect) == 0x80);
  Samples::ChannelInstrument below{.instrument_index = 0x3F};
  CHECK(Samples::MapInstrumentToAlternateBank(0x40, below) == 0x3F);
  CHECK(Samples::MapInstrumentToBaseBank(0x40, below) == 0x3F);
  CHECK(below == Samples::ChannelInstrument{.instrument_index = 0x3F});
}

// ---------------------------------------------------------------------------
// Sound bank slots

namespace {
using Audio::BankBuffer;
using Audio::DiscRequestRef;
using Audio::RequestMark;

constexpr int kMaxWaitFrames = 64;

BankBuffer Buffer(std::uintptr_t address) {
  return MakeHandle<BankBuffer>(address);
}

DiscRequestRef RequestRef(std::uintptr_t record) {
  return MakeHandle<DiscRequestRef>(record);
}

std::string Text(BankBuffer buffer) {
  return std::to_string(std::to_underlying(buffer));
}

std::string Text(DiscRequestRef request) {
  return std::to_string(std::to_underlying(request));
}

class FakeSoundBanks final : public Audio::SoundBankHost {
 public:
  std::array<Audio::SfxBankSlot, Audio::kSfxBankSlotCount> sfx{};
  std::uint32_t current = 0;
  Audio::WaveBankSlot wave;
  std::optional<Audio::SoundSlots> slots;
  std::optional<Audio::EffectTagCursors> cursors;
  std::int32_t file_entries = 4;
  std::optional<DiscRequestRef> next_request = RequestRef(0x100);
  BankBuffer next_buffer = Buffer(0x2000);
  std::map<DiscRequestRef, DiscRequestState> states;
  int frames_until_loaded = 1;
  int waits = 0;
  std::int32_t transfer = Audio::kTransferFinished;
  Playback::MusicBankIds music;
  std::vector<std::string> log;
  std::vector<std::uint32_t> diagnostics;
  std::vector<Audio::SoundBankRefusal> refusals;

  Audio::SfxBankSlot SfxBank(std::size_t index) override {
    return sfx.at(index);
  }
  void StoreSfxBank(std::size_t index,
                    const Audio::SfxBankSlot& slot) override {
    sfx.at(index) = slot;
  }
  std::uint32_t CurrentSfxBank() override { return current; }
  void StoreCurrentSfxBank(std::uint32_t slot) override { current = slot; }
  Audio::WaveBankSlot WaveBank() override { return wave; }
  void StoreWaveBank(const Audio::WaveBankSlot& bank) override { wave = bank; }
  void StoreSoundSlots(const Audio::SoundSlots& all) override {
    log.emplace_back("slots");
    slots = all;
  }
  void StoreEffectTagCursors(const Audio::EffectTagCursors& next) override {
    log.emplace_back("cursors");
    cursors = next;
  }

  std::optional<Content::DiscFileExtent> SfxBankFile(std::int32_t id) override {
    if (id < 0 || id >= file_entries) return std::nullopt;
    return Content::DiscFileExtent{.sector = 100 + id,
                                   .bytes = 0x40 * (id + 1)};
  }
  std::optional<Content::DiscFileExtent> WaveBankFile(
      std::int32_t file) override {
    if (file < 0 || file >= file_entries) return std::nullopt;
    return Content::DiscFileExtent{.sector = 200 + file, .bytes = 0x80};
  }
  std::optional<std::int32_t> MappedWaveBankFile(std::int32_t id) override {
    if (id < 0 || id >= 3) return std::nullopt;
    return id + 1;
  }
  Content::DiscFileExtent MenuFile(Audio::MenuSoundFile file) override {
    const int index = std::to_underlying(file);
    return {.sector = 10 + index, .bytes = 0x100 + index};
  }
  BankBuffer MenuSoundEffects() override { return Buffer(0xF000); }

  std::optional<DiscRequestRef> AllocateRequest(
      const Content::DiscFileExtent& file) override {
    log.push_back(std::format("request {} {}", file.sector, file.bytes));
    if (next_request) states[*next_request] = DiscRequestState::kEnqueued;
    return next_request;
  }
  void EnqueueRequest(DiscRequestRef request, BankBuffer destination) override {
    log.push_back("enqueue " + Text(request) + " " + Text(destination));
  }
  DiscRequestRef CompleteRefusedRequest(const Content::DiscFileExtent& file,
                                        BankBuffer destination) override {
    log.push_back(std::format("refused-read {} ", file.sector) +
                  Text(destination));
    const DiscRequestRef record = RequestRef(0x900);
    states[record] = DiscRequestState::kLoaded;
    return record;
  }
  DiscRequestState RequestState(DiscRequestRef request) override {
    return states.at(request);
  }
  void MarkRequestDispatched(DiscRequestRef request) override {
    log.push_back("dispatched " + Text(request));
    states[request] = DiscRequestState::kDispatched;
  }
  void ReleaseRequest(DiscRequestRef request) override {
    log.push_back("release " + Text(request));
  }
  void ReadFileBlocking(const Content::DiscFileExtent& file,
                        BankBuffer destination) override {
    log.push_back(std::format("read {} ", file.sector) + Text(destination));
  }
  // Requests still reading load once enough frames have passed.
  void WaitFrame() override {
    if (++waits > kMaxWaitFrames) throw std::runtime_error("never loaded");
    if (waits < frames_until_loaded) return;
    for (auto& [request, state] : states) {
      if (state == DiscRequestState::kEnqueued) {
        state = DiscRequestState::kLoaded;
      }
    }
  }

  BankBuffer AllocateBuffer(std::int32_t bytes) override {
    log.push_back(std::format("alloc {}", bytes));
    const BankBuffer buffer = next_buffer;
    next_buffer = Buffer(std::to_underlying(next_buffer) + 0x1000);
    return buffer;
  }
  void FreeBuffer(BankBuffer buffer) override {
    log.push_back("free " + Text(buffer));
  }

  void InitializePlayback() override { log.emplace_back("init-playback"); }
  void SetDiscMixVolume(std::uint32_t volume) override {
    log.push_back(std::format("mix {}", volume));
  }
  void BindSoundEffectPrograms(BankBuffer blob) override {
    log.push_back("bind " + Text(blob));
  }
  void StopSoundEffects(Playback::EffectSelector selector,
                        std::int32_t effect_tag_mask) override {
    log.push_back(std::format("stop {} {:#x}", std::to_underlying(selector),
                              effect_tag_mask));
  }
  void SetMonoOutput(std::int32_t mono_enabled) override {
    log.push_back(std::format("mono {}", mono_enabled));
  }
  Playback::MusicBankIds MusicBanks() override { return music; }
  void LoadBank(BankBuffer bank, bool wait) override {
    log.push_back("load-bank " + Text(bank) + std::format(" {}", wait));
  }
  void LoadSoundEffectBank(BankBuffer bank, std::int32_t slot,
                           bool wait) override {
    log.push_back("sfx-bank " + Text(bank) + std::format(" {} {}", slot, wait));
  }
  void LoadMusicBank(BankBuffer bank, bool wait) override {
    log.push_back("music-bank " + Text(bank) + std::format(" {}", wait));
  }
  std::int32_t TransferStatus() override { return transfer; }
  void ResetCutsceneStream() override { log.emplace_back("reset-stream"); }
  void Diagnose(std::uint32_t code) override { diagnostics.push_back(code); }
  void Refuse(Audio::SoundBankRefusal refusal) override {
    refusals.push_back(refusal);
  }
};

Audio::BankRequest Active(std::uintptr_t request) {
  return {.mark = RequestMark::kActive, .request = RequestRef(request)};
}

// Each channel group's cursor starts at the lowest tag of its mask.
constexpr Audio::EffectTagCursors kInitialTagCursors{
    .group_1 = 1,
    .group_7e = 2,
    .group_180 = 0x80,
    .group_200 = 0x200,
    .group_ff000 = 0x1000,
    .group_f00000 = 0x100000,
};
static_assert(Audio::EffectTagCursors() == kInitialTagCursors);
}  // namespace

TEST_CASE("Sound-effect bank loads queue a read into an empty slot",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  CHECK(Audio::LoadSfxBankSlot(host, 2, 0) == 0);
  CHECK(Audio::LoadSfxBankSlot(host, 2, 4) == 0);
  CHECK(host.log.empty());
  CHECK(Audio::LoadSfxBankSlot(host, 2, 2) == 2);
  CHECK(host.sfx[1] == Audio::SfxBankSlot{.id = 2,
                                          .data = Buffer(0x2000),
                                          .request = Active(0x100)});
  CHECK(host.log == std::vector<std::string>{"request 102 192", "alloc 192",
                                             "enqueue 256 8192"});
  // A slot holding a bank is taken.
  CHECK(Audio::LoadSfxBankSlot(host, 3, 2) == 0);
  CHECK(host.log.size() == 3);
  CHECK(host.diagnostics.empty());
}

TEST_CASE("Sound-effect bank ids past the file table are refused",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  // The id is refused rather than read past the table.
  CHECK(Audio::LoadSfxBankSlot(host, 9, 1) == 0);
  CHECK(host.refusals ==
        std::vector{Audio::SoundBankRefusal::kSfxBankFileOutOfRange});
  CHECK(host.sfx[0] == Audio::SfxBankSlot{});
  CHECK(host.log.empty());
}

TEST_CASE("A full disc queue reads the sound-effect bank without it",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  host.sfx[1] = {.id = 2, .data = Buffer(0x5000), .request = {}};
  host.next_request.reset();
  CHECK(Audio::LoadSfxBankIntoFreeSlot(host, 1) == 1);
  CHECK(host.sfx[0].request == Active(0x900));
  CHECK(host.log == std::vector<std::string>{"request 101 128", "alloc 128",
                                             "refused-read 101 8192"});
  CHECK(host.diagnostics.empty());
  CHECK(Audio::LoadSfxBankIntoFreeSlot(host, 0) == 3);
  CHECK(Audio::LoadSfxBankIntoFreeSlot(host, 0) == 0);
}

TEST_CASE("Reloads report a request or buffer the slot still held",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  host.sfx[0].request = Active(0x55);
  CHECK(Audio::LoadSfxBankSlot(host, 0, 1) == 1);
  CHECK(host.diagnostics == std::vector<std::uint32_t>{0x8F});
  host.wave = {.file = 1, .data = Buffer(0x7000), .request = Active(0x66)};
  Audio::LoadWaveBankFile(host, 2);
  CHECK(host.diagnostics == std::vector<std::uint32_t>{0x8F, 0x93, 0x94});
  CHECK(host.wave == Audio::WaveBankSlot{.file = 2,
                                         .data = Buffer(0x3000),
                                         .request = Active(0x100)});
}

TEST_CASE("Sound-effect load polls release requests and keep the fail mark",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  host.sfx[0] = {.id = 1, .data = Buffer(0x2000), .request = Active(0x100)};
  host.states[RequestRef(0x100)] = DiscRequestState::kReading;
  CHECK(Audio::PollSfxBankSlotLoad(host, 1) == Audio::kLoadInFlight);
  host.states[RequestRef(0x100)] = DiscRequestState::kError;
  CHECK(Audio::PollSfxBankSlotLoad(host, 1) == Audio::kLoadFailed);
  CHECK(host.sfx[0].request ==
        Audio::BankRequest{.mark = RequestMark::kFailed});
  CHECK(host.sfx[0].data == Buffer(0x2000));
  // The failed mark polls as idle: nothing is in flight, and it is not read.
  CHECK(Audio::PollSfxBankSlotLoad(host, 1) == Audio::kLoadIdle);
  CHECK(host.log == std::vector<std::string>{"release 256"});
}

TEST_CASE("Polling every sound-effect slot releases the loaded requests",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  host.sfx[0] = {.id = 1,
                 .data = Buffer(0x2000),
                 .request = {.mark = RequestMark::kFailed}};
  host.sfx[1] = {.id = 2, .data = Buffer(0x3000), .request = Active(0x200)};
  host.states[RequestRef(0x200)] = DiscRequestState::kLoaded;
  host.sfx[2] = {.id = 3, .data = Buffer(0x4000), .request = Active(0x300)};
  host.states[RequestRef(0x300)] = DiscRequestState::kReading;
  CHECK(Audio::PollSfxBankLoads(host) == Audio::kLoadInFlight);
  CHECK(host.sfx[1].request == Audio::BankRequest{});
  CHECK(host.sfx[0].request ==
        Audio::BankRequest{.mark = RequestMark::kFailed});
  CHECK(host.log == std::vector<std::string>{"release 512"});
  CHECK(Audio::PollSfxBankSlotLoad(host, 0) == Audio::kLoadIdle);
  CHECK(Audio::PollSfxBankSlotLoad(host, 4) == Audio::kLoadIdle);
}

TEST_CASE("Blocking sound-effect loads wait until no load is in flight",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  host.frames_until_loaded = 3;
  CHECK(Audio::LoadSfxBankBlocking(host, 1) == 1);
  CHECK(host.waits == 3);
  CHECK(host.sfx[0].request == Audio::BankRequest{});
  for (Audio::SfxBankSlot& slot : host.sfx) slot.data = Buffer(0x8000);
  CHECK(Audio::LoadSfxBankBlocking(host, 1) == 0);
  CHECK(host.waits == 3);
}

TEST_CASE("Selecting a slot needs a loaded bank in range",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  CHECK(Audio::SelectSfxBankSlot(host, 1) == 0);
  host.sfx[0] = {.id = 4, .data = Buffer(0x2000), .request = {}};
  CHECK(Audio::SelectSfxBankSlot(host, 0) == 0);
  CHECK(Audio::SelectSfxBankSlot(host, 4) == 0);
  CHECK(host.current == 0);
  CHECK(Audio::SelectSfxBankSlot(host, 1) == 1);
  CHECK(host.current == 1);
  CHECK(host.log.empty());
}

TEST_CASE("Freeing slots stop the bank's tagged effects",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  host.sfx[0] = {.id = 4, .data = Buffer(0x2000), .request = {}};
  host.current = 1;
  CHECK(Audio::FreeSfxBankSlot(host, 1) == 1);
  CHECK(host.current == 0);
  CHECK(host.sfx[0] == Audio::SfxBankSlot{});
  CHECK(host.log ==
        std::vector<std::string>{"stop -2 0x0", "stop 0 0xff000", "free 8192"});
  CHECK(Audio::FreeSfxBankSlot(host, 1) == 0);
  CHECK(Audio::FreeSfxBankSlot(host, 0) == 0);
  CHECK(host.log.size() == 3);
}

TEST_CASE("The room effect bank binds only an empty third slot",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  CHECK(Audio::BindRoomSfxBank(host, Buffer(0x9000)) == 1);
  CHECK(Audio::BindRoomSfxBank(host, Buffer(0xA000)) == 0);
  CHECK(host.sfx[2].data == Buffer(0x9000));
  host.sfx[2].id = 3;
  CHECK(Audio::UnbindRoomSfxBank(host) == 1);
  CHECK(host.sfx[2] ==
        Audio::SfxBankSlot{.id = 3, .data = BankBuffer::kNone, .request = {}});
  // The room bank is not the slots' to free.
  CHECK(host.log == std::vector<std::string>{"stop -2 0x0", "stop 0 0xf00000"});
}

TEST_CASE("Wave bank loads skip a loaded music bank before any refusal",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  host.music = {.bank = 12, .alternate = 13};
  Audio::LoadWaveBankFile(host, 12);
  Audio::LoadWaveBankFile(host, 13);
  CHECK(host.refusals.empty());
  CHECK(host.log.empty());
  Audio::LoadWaveBankFile(host, 9);
  CHECK(host.refusals ==
        std::vector{Audio::SoundBankRefusal::kWaveBankFileOutOfRange});
  Audio::LoadMappedWaveBank(host, 5);
  CHECK(host.refusals.back() ==
        Audio::SoundBankRefusal::kWaveBankMapOutOfRange);
  Audio::LoadMappedWaveBank(host, 1);
  CHECK(host.wave == Audio::WaveBankSlot{.file = 2,
                                         .data = Buffer(0x2000),
                                         .request = Active(0x100)});
  CHECK(host.log == std::vector<std::string>{"request 202 128", "alloc 128",
                                             "enqueue 256 8192"});
  CHECK(host.diagnostics.empty());
}

TEST_CASE("A full disc queue reads the wave bank without it",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  host.next_request.reset();
  Audio::LoadMappedWaveBank(host, 1);
  CHECK(host.wave == Audio::WaveBankSlot{.file = 2,
                                         .data = Buffer(0x2000),
                                         .request = Active(0x900)});
  CHECK(host.log == std::vector<std::string>{"request 202 128", "alloc 128",
                                             "refused-read 202 8192"});
  CHECK(host.diagnostics.empty());
}

TEST_CASE("Wave bank updates upload a loaded bank and discard it after",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  CHECK(Audio::UpdateWaveBankLoad(host) == Audio::kLoadIdle);
  const DiscRequestRef request = RequestRef(0x100);
  host.wave = {.file = 2, .data = Buffer(0x2000), .request = Active(0x100)};
  host.states[request] = DiscRequestState::kReading;
  CHECK(Audio::UpdateWaveBankLoad(host) == Audio::kLoadInFlight);
  host.states[request] = DiscRequestState::kLoaded;
  CHECK(Audio::UpdateWaveBankLoad(host) == Audio::kLoadInFlight);
  host.transfer = 1;
  CHECK(Audio::UpdateWaveBankLoad(host) == Audio::kLoadInFlight);
  host.transfer = Audio::kTransferFinished;
  CHECK(Audio::UpdateWaveBankLoad(host) == Audio::kLoadIdle);
  CHECK(host.wave == Audio::WaveBankSlot{});
  CHECK(host.log == std::vector<std::string>{"dispatched 256",
                                             "music-bank 8192 false",
                                             "release 256", "free 8192"});
}

TEST_CASE("Wave bank updates discard a bank that failed to read or upload",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  const Audio::WaveBankSlot loading{
      .file = 2, .data = Buffer(0x2000), .request = Active(0x100)};
  const DiscRequestRef request = RequestRef(0x100);
  host.wave = loading;
  host.states[request] = DiscRequestState::kDispatched;
  host.transfer = Audio::kTransferFailed;
  CHECK(Audio::UpdateWaveBankLoad(host) == Audio::kLoadFailed);
  CHECK(host.wave == Audio::WaveBankSlot{});
  host.wave = loading;
  host.states[request] = DiscRequestState::kError;
  CHECK(Audio::UpdateWaveBankLoad(host) == Audio::kLoadFailed);
  CHECK(host.wave == Audio::WaveBankSlot{});
  CHECK(host.log == std::vector<std::string>{"release 256", "free 8192",
                                             "release 256", "free 8192"});
}

TEST_CASE("Blocking wave loads wait until the bank uploads",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  host.frames_until_loaded = 2;
  Audio::LoadWaveBankBlocking(host, 1);
  CHECK(host.waits == 3);
  CHECK(host.wave == Audio::WaveBankSlot{});
  CHECK(host.log.back() == "free 8192");
}

TEST_CASE("Menu sound banks load in the original's order",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  Audio::InitializeMenuSoundBanks(host);
  const std::vector<std::string> expected{
      "init-playback",
      "mix 127",
      "alloc 256",
      "read 10 8192",
      "load-bank 8192 true",
      "free 8192",
      "alloc 257",
      "read 11 12288",
      "load-bank 12288 true",
      "free 12288",
      "alloc 258",
      "read 12 16384",
      "sfx-bank 16384 0 true",
      "free 16384",
      "read 13 61440",
      "bind 61440",
      "cursors",
  };
  CHECK(host.log == expected);
  CHECK(host.cursors == Audio::EffectTagCursors{});
}

TEST_CASE("Sound slot resets release no buffer or request",
          "[audio-playback-banks][audio]") {
  FakeSoundBanks host;
  Audio::ResetSoundSlots(host);
  Audio::SetMonoOutput(host, 1);
  CHECK(host.log ==
        std::vector<std::string>{"reset-stream", "slots", "cursors", "mono 1"});
  CHECK(host.slots == Audio::SoundSlots{});
  CHECK(host.cursors == Audio::EffectTagCursors{});
}

// ---------------------------------------------------------------------------
// Live bindings

TEST_CASE("Scoped live effect tables bind for the scope",
          "[audio-playback-banks][audio]") {
  std::array<std::uint32_t, 2> offsets{0x10, Playback::kNoEffectProgram};
  std::array<std::uint16_t, 2> selectors{0x8000, 0};
  std::array<char, 4> data{};
  const Playback::ScopedLiveEffectTables scope({.program_offsets = offsets,
                                                .bank_selectors = selectors,
                                                .program_data = data});
  const std::optional<Playback::LiveEffectTables> bound =
      Playback::BoundLiveEffectTables();
  const Playback::LiveEffectTables& tables = Required(bound);
  CHECK(tables.program_offsets.data() == offsets.data());
  CHECK(tables.bank_selectors.size() == 2);
  CHECK(Playback::LiveSharedEffectUsesTags(0) == 1);
  CHECK(Playback::LiveSharedEffectUsesTags(1) == 0);
  const std::uint64_t refused = Seq::LiveRefusalCount();
  CHECK(Playback::LiveSharedEffectUsesTags(2) == 0);
  CHECK(Seq::LiveRefusalCount() == refused + 1);
  CHECK(Playback::LivePlaySoundEffect(1, 0, 0, 0) == -1);
}

TEST_CASE("Scoped live effect tables restore the earlier binding",
          "[audio-playback-banks][audio]") {
  const std::optional<Playback::LiveEffectTables> before =
      Playback::BoundLiveEffectTables();
  std::array<std::uint32_t, 1> offsets{0x10};
  std::array<std::uint16_t, 1> selectors{0};
  std::array<char, 4> data{};
  {
    const Playback::ScopedLiveEffectTables scope({.program_offsets = offsets,
                                                  .bank_selectors = selectors,
                                                  .program_data = data});
    CHECK(Playback::BoundLiveEffectTables().has_value());
  }
  const std::optional<Playback::LiveEffectTables> after =
      Playback::BoundLiveEffectTables();
  REQUIRE(after.has_value() == before.has_value());
  if (before && after) {
    CHECK(after->program_offsets.data() == before->program_offsets.data());
  }
}

namespace {
struct SoundStateScope {
  Audio_SoundState saved = Audio_soundState;

  SoundStateScope() { Audio_soundState = {}; }
  ~SoundStateScope() { Audio_soundState = saved; }
  SoundStateScope(const SoundStateScope&) = delete;
  SoundStateScope& operator=(const SoundStateScope&) = delete;
  SoundStateScope(SoundStateScope&&) = delete;
  SoundStateScope& operator=(SoundStateScope&&) = delete;
};
}  // namespace

TEST_CASE("Live slots keep the room bank's address and the failed marker",
          "[audio-playback-banks][audio]") {
  const SoundStateScope scope;
  std::array<std::byte, 16> bank{};
  std::array<std::byte, 16> other{};
  CHECK(Audio::LiveBindRoomSfxBank(
            std::bit_cast<std::uintptr_t>(bank.data())) == 1);
  CHECK(Audio_soundState.sfxBankData[2] == bank.data());
  CHECK(Audio::LiveBindRoomSfxBank(
            std::bit_cast<std::uintptr_t>(other.data())) == 0);
  CHECK(Audio::LiveSelectSfxBankSlot(3) == 1);
  CHECK(Audio_soundState.currentSfxBank == 3U);

  auto* const failed =
      std::bit_cast<Content_DiscQueueSlot*>(~std::uintptr_t{0});
  Audio_soundState.sfxBankData[0] = bank.data();
  Audio_soundState.sfxBankRequests[0] = failed;
  CHECK(Audio::LivePollSfxBankSlotLoad(1) == Audio::kLoadIdle);
  CHECK(Audio_soundState.sfxBankRequests[0] == failed);
  CHECK(Audio::LivePollSfxBankLoads() == Audio::kLoadIdle);
}

TEST_CASE("Promoted playback and bank functions keep no module binding",
          "[audio-playback-banks][audio][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const std::array<std::uint32_t, 58> retired{
      0x80011DACU, 0x80011DCCU, 0x80011DECU, 0x80011E40U, 0x80011EBCU,
      0x80011F08U, 0x80011F24U, 0x80011F90U, 0x80011FB4U, 0x80011FDCU,
      0x80012004U, 0x80012024U, 0x80012034U, 0x80012080U, 0x800120B8U,
      0x800120E8U, 0x80012110U, 0x8001215CU, 0x8001217CU, 0x800121F0U,
      0x80012288U, 0x800122C4U, 0x800122F0U, 0x80012358U, 0x800123C8U,
      0x8001240CU, 0x80012BB8U, 0x80012C04U, 0x80012C14U, 0x80012D9CU,
      0x80012E50U, 0x80012EBCU, 0x80012EF0U, 0x80012F10U, 0x80013188U,
      0x800131DCU, 0x80013418U, 0x80013468U, 0x80013588U, 0x80015AFCU,
      0x80015B54U, 0x80016DA8U, 0x80045DE0U, 0x80045F0CU, 0x80045F64U,
      0x80045FECU, 0x80046038U, 0x80046084U, 0x800460C0U, 0x80046168U,
      0x80046194U, 0x80046678U, 0x80046770U, 0x800467A0U, 0x8004687CU,
      0x800468BCU, 0x800468FCU, 0x80046A38U,
  };
  for (const std::uint32_t address : retired) {
    CAPTURE(address);
    CHECK(Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          nullptr);
  }
}
