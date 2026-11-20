// Hand-maintained (tools/port/owned.csv).
// Live binding for SampleBanks.hpp: the music and sound-effect bank ids
// (D_80036770, D_800377E0), the active music's flags, the mixer's music bank
// bit, the stream (D_80039BC8, D_80037850 and the driver's stream latch), the
// instrument table (g_InstrumentInfo), sample memory and func_800135D8.
#include "Audio/SampleBanksLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/SampleBanks.hpp"
#include "Audio/Samples/SampleBanks.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundDriverLive.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Legacy/Psx.h"

namespace VS::Audio::Samples {
namespace {
static_assert(sizeof(VS::Audio::Instrument) == kInstrumentRecordBytes);
static_assert(std::size(VS::Audio::Samples::instruments) ==
              kInstrumentSlotCount);
static_assert(std::size(VS::Audio::Samples::soundEffectBankIds) ==
              kSoundEffectBankSlots);
static_assert(std::size(VS::Audio::Samples::bankChunkHeader) ==
              kChunkHeaderWords);

// The mixer's music bank bit (VS::Audio::DriverFlags.mixBehavior) and the
// driver's bank stream latch (VS::Audio::DriverFlags.controlLatches).
constexpr std::uint32_t kMusicBankSelected = 0x10000;
constexpr std::uint32_t kBankStreamActive = 1;

const char* RefusalReason(BankRefusal refusal) {
  switch (refusal) {
    case BankRefusal::kHeaderTruncated:
      return "the bank is shorter than its header";
    case BankRefusal::kInstrumentsOutOfRange:
      return "the bank's instruments run past the instrument table";
  }
  return "refused";
}

std::span<std::byte> InstrumentTableBytes() {
  return std::as_writable_bytes(std::span(VS::Audio::Samples::instruments));
}

class LiveBankLoads final : public BankLoadHost {
 public:
  explicit LiveBankLoads(const char* operation) : operation_(operation) {}

  MusicBankState MusicBanks() override {
    MusicBankState state{
        .music_bank_selected = (VS::Audio::Driver::controlLatches.mix_behavior &
                                kMusicBankSelected) != 0,
        .bank_id = VS::Audio::Samples::musicSampleBankIds.bank_id,
        .alternate_bank_id =
            VS::Audio::Samples::musicSampleBankIds.alternate_bank_id};
    if (const VS::Audio::MusicConfig* active =
            VS::Audio::Seq::activeMusicConfig) {
      state.active_channels = active->active_channels;
      state.music_flags = active->status_flags;
    } else {
      Seq::ReportRefusal(operation_, "no music is active");
    }
    return state;
  }

  void StoreMusicBanks(const MusicBankState& state) override {
    if (VS::Audio::MusicConfig* active = VS::Audio::Seq::activeMusicConfig) {
      active->status_flags = state.music_flags;
    }
    if (state.music_bank_selected) {
      VS::Audio::Driver::controlLatches.mix_behavior |= kMusicBankSelected;
    } else {
      VS::Audio::Driver::controlLatches.mix_behavior &= ~kMusicBankSelected;
    }
    VS::Audio::Samples::musicSampleBankIds.bank_id = state.bank_id;
    VS::Audio::Samples::musicSampleBankIds.alternate_bank_id =
        state.alternate_bank_id;
  }

  SoundEffectBankIds SoundEffectBanks() override {
    SoundEffectBankIds ids{};
    std::ranges::copy(VS::Audio::Samples::soundEffectBankIds, ids.begin());
    return ids;
  }

  void StoreSoundEffectBanks(const SoundEffectBankIds& ids) override {
    std::ranges::copy(ids, std::begin(VS::Audio::Samples::soundEffectBankIds));
  }

  void UploadBank(std::span<const std::byte> bank, bool wait,
                  const BankDestination& destination) override {
    UploadLiveBank(bank.data(), wait ? 1 : 0, destination.first_instrument,
                   static_cast<int>(destination.sample_address));
  }

  std::int32_t TransferStatus() override {
    return VS::Audio::Cutscene::legacySpuTransferBusy;
  }

  BankStreamState Stream() override {
    const VS::Audio::LegacyBankStreamState& legacy =
        VS::Audio::Samples::bankStream;
    BankStreamState state{
        .active = (VS::Audio::Driver::controlLatches.control_latches &
                   kBankStreamActive) != 0,
        .sample_address = static_cast<std::uint32_t>(legacy.spu_address),
        .sample_bytes_remaining = legacy.spu_bytes_remaining,
        .instrument_bytes_remaining = legacy.instrument_bytes_remaining,
        .instrument_offset = InstrumentOffset(legacy.instrument_destination)};
    std::ranges::transform(
        VS::Audio::Samples::bankChunkHeader, state.header.begin(),
        [](int word) { return static_cast<std::uint32_t>(word); });
    return state;
  }

  void StoreStream(const BankStreamState& state) override {
    VS::Audio::LegacyBankStreamState& legacy = VS::Audio::Samples::bankStream;
    if (state.active) {
      VS::Audio::Driver::controlLatches.control_latches |= kBankStreamActive;
    } else {
      VS::Audio::Driver::controlLatches.control_latches &= ~kBankStreamActive;
    }
    legacy.spu_address = static_cast<int>(state.sample_address);
    legacy.spu_bytes_remaining = state.sample_bytes_remaining;
    legacy.instrument_bytes_remaining = state.instrument_bytes_remaining;
    const std::span<std::byte> table = InstrumentTableBytes();
    legacy.instrument_destination = reinterpret_cast<int*>(
        table.data() + std::min(state.instrument_offset, table.size()));
    std::ranges::transform(
        state.header, std::begin(VS::Audio::Samples::bankChunkHeader),
        [](std::uint32_t word) { return static_cast<int>(word); });
  }

  void WriteInstrumentBytes(std::size_t offset,
                            std::span<const std::byte> bytes) override {
    const std::span<std::byte> table = InstrumentTableBytes();
    if (offset > table.size() || bytes.size() > table.size() - offset) {
      Refuse(BankRefusal::kInstrumentsOutOfRange);
      return;
    }
    std::ranges::copy(bytes, table.subspan(offset).begin());
  }

  InstrumentRecord Instrument(std::size_t slot) override {
    if (slot >= kInstrumentSlotCount) return {};
    const VS::Audio::Instrument& legacy = VS::Audio::Samples::instruments[slot];
    return {.sample_start = legacy.sample_start,
            .loop_start = legacy.loop_start,
            .fine_tune = legacy.fine_tune,
            .root_note = legacy.root_note,
            .adsr_lower = legacy.adsr_lower,
            .adsr_upper = legacy.adsr_upper};
  }

  void StoreInstrument(std::size_t slot,
                       const InstrumentRecord& record) override {
    if (slot >= kInstrumentSlotCount) return;
    VS::Audio::Instrument& legacy = VS::Audio::Samples::instruments[slot];
    legacy.sample_start = record.sample_start;
    legacy.loop_start = record.loop_start;
    legacy.fine_tune = record.fine_tune;
    legacy.root_note = record.root_note;
    legacy.adsr_lower = record.adsr_lower;
    legacy.adsr_upper = record.adsr_upper;
  }

  void WriteSampleMemory(std::uint32_t address,
                         std::span<const std::byte> bytes) override {
    VS::Legacy::SpuSetTransferStartAddr(address);
    // The SPU write only reads the bytes.
    Driver::LiveWriteSampleMemory(
        const_cast<char*>(reinterpret_cast<const char*>(bytes.data())),
        static_cast<unsigned>(bytes.size()));
  }

  void WaitForSampleTransfer() override { Driver::LiveWaitForSampleTransfer(); }

  void Refuse(BankRefusal refusal) override {
    Seq::ReportRefusal(operation_, RefusalReason(refusal));
  }

 private:
  // The instrument table offset the stream's destination pointer names; a
  // pointer outside the table names its end, where no instrument bytes fit.
  static std::size_t InstrumentOffset(const int* destination) {
    const std::span<std::byte> table = InstrumentTableBytes();
    const auto start = std::bit_cast<std::uintptr_t>(table.data());
    const auto at = std::bit_cast<std::uintptr_t>(destination);
    if (at < start || at - start > table.size()) return table.size();
    return at - start;
  }

  const char* operation_;
};

// The header words of the bank `bank` points at, as func_800135D8 trusts
// them; empty for a null bank.
std::span<const std::byte> HeaderBytes(const void* bank) {
  if (bank == nullptr) return {};
  return {static_cast<const std::byte*>(bank), kBankHeaderBytes};
}

ChannelInstrument LoadChannelInstrument(const VS::Audio::SeqChannel& channel) {
  return {.instrument_index = channel.instrument_index,
          .sample_start = channel.voice.sample_start,
          .loop_start = channel.voice.loop_start};
}

void StoreChannelInstrument(VS::Audio::SeqChannel& channel,
                            const ChannelInstrument& instrument) {
  channel.instrument_index = instrument.instrument_index;
  channel.voice.sample_start = instrument.sample_start;
  channel.voice.loop_start = instrument.loop_start;
}
}  // namespace

// decomp: SLUS_010.40 func_80012BB8 (src/SLUS_010.40/25AC.c:719)
void LiveLoadBank(void* bank, int wait) {
  LiveBankLoads host("LoadBank");
  LoadBank(host, HeaderBytes(bank), wait != 0);
}

// decomp: SLUS_010.40 vs_sound_spuTransferring (src/SLUS_010.40/25AC.c:725)
int LiveTransferStatus() {
  LiveBankLoads host("TransferStatus");
  return TransferStatus(host);
}

// decomp: SLUS_010.40 func_80012EBC (src/SLUS_010.40/25AC.c:807)
int LiveClearMusicBankIds() {
  LiveBankLoads host("ClearMusicBankIds");
  ClearMusicBankIds(host);
  return 0;
}

// decomp: SLUS_010.40 func_80013188 (src/SLUS_010.40/25AC.c:894)
int LiveLoadMusicBank(void* bank, int wait) {
  LiveBankLoads host("LoadMusicBank");
  return LoadMusicBank(host, HeaderBytes(bank), wait != 0) ? 1 : 0;
}

// decomp: SLUS_010.40 func_800131DC (src/SLUS_010.40/25AC.c:903)
int LiveLoadSoundEffectBank(void* bank, int slot, int wait) {
  LiveBankLoads host("LoadSoundEffectBank");
  LoadSoundEffectBank(host, HeaderBytes(bank), slot, wait != 0);
  return 0;
}

// decomp: SLUS_010.40 Sound_IsNotAkaoFile (src/SLUS_010.40/25AC.c:984)
int LiveSequenceMagicDifference(const void* data) {
  if (data == nullptr) {
    Seq::ReportRefusal("SequenceMagicDifference", "the sequence is null");
    return -1;
  }
  return SequenceMagicDifference(
             {static_cast<const std::byte*>(data), sizeof(std::uint32_t)})
      .value_or(-1);
}

// decomp: SLUS_010.40 Sound_MapInstrumentToAltSampleBank
// (src/SLUS_010.40/25AC.c:2303)
std::uint16_t LiveMapInstrumentToAlternateBank(unsigned music_flags,
                                               VS::Audio::SeqChannel* channel) {
  if (channel == nullptr) return 0;
  ChannelInstrument instrument = LoadChannelInstrument(*channel);
  const std::uint16_t index =
      MapInstrumentToAlternateBank(music_flags, instrument);
  StoreChannelInstrument(*channel, instrument);
  return index;
}

// decomp: SLUS_010.40 Sound_MapInstrumentToBaseSampleBank
// (src/SLUS_010.40/25AC.c:2315)
std::uint16_t LiveMapInstrumentToBaseBank(unsigned music_flags,
                                          VS::Audio::SeqChannel* channel) {
  if (channel == nullptr) return 0;
  ChannelInstrument instrument = LoadChannelInstrument(*channel);
  const std::uint16_t index = MapInstrumentToBaseBank(music_flags, instrument);
  StoreChannelInstrument(*channel, instrument);
  return index;
}

// decomp: SLUS_010.40 func_80016DA8 (src/SLUS_010.40/25AC.c:2941)
int LiveSoundEffectBankSlot(int selector) {
  LiveBankLoads host("SoundEffectBankSlot");
  return SoundEffectBankSlot(host, selector);
}
}  // namespace VS::Audio::Samples
