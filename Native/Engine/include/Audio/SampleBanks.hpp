#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/VoiceOutput.hpp"

// The sample bank loaders (func_80012BB8 through func_80013588,
// SLUS_010.40/25AC.c:719-1030), the instrument bank remaps
// (Sound_MapInstrumentTo*SampleBank, 25AC.c:2303-2325) and the sound-effect
// bank lookup (func_80016DA8, 25AC.c:2941): where a bank's instruments and
// samples go, the streamed bank upload and the music and sound-effect bank
// ids. The ids, the active music's bank flags, the stream, the instrument
// table, sample memory and the whole-bank upload go through BankLoadHost.
namespace VS::Audio::Samples {
// Where a bank's instruments start in the instrument table and its samples
// in sample memory.
struct BankDestination {
  std::int32_t first_instrument = 0;
  std::uint32_t sample_address = 0;

  friend bool operator==(const BankDestination&,
                         const BankDestination&) = default;
};
inline constexpr BankDestination kMusicBank{
    .first_instrument = 0x40,
    .sample_address = 0x35100,
};
inline constexpr BankDestination kAlternateMusicBank{
    .first_instrument = 0x60,
    .sample_address = 0x55100,
};
inline constexpr std::size_t kSoundEffectBankSlots = 3;
inline constexpr auto kSoundEffectBanks = std::to_array<BankDestination>({
    {.first_instrument = 0x80, .sample_address = 0xB100},
    {.first_instrument = 0xA0, .sample_address = 0x13100},
    {.first_instrument = 0xC0, .sample_address = 0x1B100},
});
static_assert(kSoundEffectBanks.size() == kSoundEffectBankSlots);
// The instruments past the alternate music bank, where the sound-effect
// banks start.
inline constexpr std::int32_t kMusicBanksEnd(
    kSoundEffectBanks[0].first_instrument);
// The alternate music bank mirrors the music bank this far along the
// instrument table and sample memory.
inline constexpr std::uint16_t kAlternateBankInstrumentOffset = 0x20;
inline constexpr std::uint32_t kAlternateBankSampleOffset = 0x20000;

// A music bank with more instruments or sample bytes than one music bank
// holds; it takes the music bank and clears both music bank ids.
inline constexpr std::uint32_t kMusicBankInstrumentLimit = 0x20;
inline constexpr std::uint32_t kMusicBankSampleLimit = 0x20000;

// The active music's flags (Audio_MusicConfig.statusFlags) that choose a
// music bank: load the next bank into the alternate bank, and play the
// music's instruments from the alternate bank. Music with no active channels
// drops both.
inline constexpr std::int32_t kLoadIntoAlternateBank = 0x20;
inline constexpr std::int32_t kPlaysFromAlternateBank = 0x40;
inline constexpr std::int32_t kMusicBankFlags(kLoadIntoAlternateBank |
                                              kPlaysFromAlternateBank);

// A sound effect's bank selector names its bank id in these bits.
inline constexpr std::int32_t kSoundEffectBankIdMask = 0x7FFF;

// One instrument table entry's size, and the stream's copy of a bank's
// leading header words.
inline constexpr std::size_t kInstrumentRecordBytes = 16;
inline constexpr std::size_t kChunkHeaderWords = 16;
inline constexpr std::size_t kChunkHeaderBytes(sizeof(std::uint32_t) *
                                               kChunkHeaderWords);
// The header words the stream reads and rewrites.
inline constexpr std::size_t kHeaderSampleAddressWord = 4;
inline constexpr std::size_t kHeaderSampleBytesWord = 5;
inline constexpr std::size_t kHeaderFirstInstrumentWord = 6;
inline constexpr std::size_t kHeaderInstrumentCountWord = 7;

// The AKAO bank header words the loaders read.
struct BankDeclaration {
  std::int32_t bank_id = 0;
  BankDestination destination;
  std::uint32_t sample_bytes = 0;
  std::uint32_t instrument_count = 0;
};

// The active music's bank flags and the music bank ids (D_80036770) the
// music bank choice reads and writes.
struct MusicBankState {
  std::int32_t active_channels = 0;
  std::int32_t music_flags = 0;
  // The mixer's music bank bit: the music bank, not the alternate, holds
  // the music that last loaded or was selected.
  bool music_bank_selected = false;
  std::int32_t bank_id = 0;
  std::int32_t alternate_bank_id = 0;

  friend bool operator==(const MusicBankState&,
                         const MusicBankState&) = default;
};

using SoundEffectBankIds = std::array<std::int32_t, kSoundEffectBankSlots>;

// The streamed bank upload (D_80039BC8, the D_80037850 header copy and the
// driver's stream latch). A stream with no sample address starts a bank at
// its next chunk.
struct BankStreamState {
  bool active = false;
  std::uint32_t sample_address = 0;
  std::uint32_t sample_bytes_remaining = 0;
  std::uint32_t instrument_bytes_remaining = 0;
  // Where the next instrument bytes go, in bytes into the instrument table.
  std::size_t instrument_offset = 0;
  std::array<std::uint32_t, kChunkHeaderWords> header{};

  friend bool operator==(const BankStreamState&,
                         const BankStreamState&) = default;
};

// A sequence channel's instrument and the sample addresses it plays.
struct ChannelInstrument {
  std::uint16_t instrument_index = 0;
  std::uint32_t sample_start = 0;
  std::uint32_t loop_start = 0;

  friend bool operator==(const ChannelInstrument&,
                         const ChannelInstrument&) = default;
};

enum class BankRefusal : std::uint8_t {
  kHeaderTruncated,
  kInstrumentsOutOfRange,
};

class BankLoadHost {
 public:
  virtual ~BankLoadHost() = default;

  virtual MusicBankState MusicBanks() = 0;
  virtual void StoreMusicBanks(const MusicBankState& state) = 0;
  virtual SoundEffectBankIds SoundEffectBanks() = 0;
  virtual void StoreSoundEffectBanks(const SoundEffectBankIds& ids) = 0;

  // func_800135D8: uploads the whole bank whose header `bank` holds.
  virtual void UploadBank(std::span<const std::byte> bank, bool wait,
                          const BankDestination& destination) = 0;
  // vs_sound_spuTransferring: 0 when the last upload finished, -1 when it
  // failed.
  virtual std::int32_t TransferStatus() = 0;

  virtual BankStreamState Stream() = 0;
  virtual void StoreStream(const BankStreamState& state) = 0;
  // Copies `bytes` into the instrument table `offset` bytes in.
  virtual void WriteInstrumentBytes(std::size_t offset,
                                    std::span<const std::byte> bytes) = 0;
  // Instrument table entry `slot` (below kInstrumentSlotCount).
  virtual InstrumentRecord Instrument(std::size_t slot) = 0;
  virtual void StoreInstrument(std::size_t slot,
                               const InstrumentRecord& record) = 0;
  virtual void WriteSampleMemory(std::uint32_t address,
                                 std::span<const std::byte> bytes) = 0;
  virtual void WaitForSampleTransfer() = 0;

  virtual void Refuse(BankRefusal refusal) = 0;
};

// The bank header words; nullopt when `header` is shorter than
// kBankHeaderBytes. The magic is not checked.
[[nodiscard]] std::optional<BankDeclaration> ReadBankDeclaration(
    std::span<const std::byte> header);

// func_80012BB8: loads the bank at its declared destination. The original
// repeats while func_80013588 returns 1, which it never does.
void LoadBank(BankLoadHost& host, std::span<const std::byte> bank, bool wait);
std::int32_t TransferStatus(BankLoadHost& host);

struct MusicBankChoice {
  bool alternate = false;
  BankDestination destination;
};
MusicBankChoice ChooseMusicBankDestination(BankLoadHost& host,
                                           const BankDeclaration& bank);
// Slot 1 and slot 2 name the second and third banks; any other slot the
// first.
BankDestination ChooseSoundEffectBankDestination(BankLoadHost& host,
                                                 const BankDeclaration& bank,
                                                 std::int32_t slot);
// Selects the music bank holding `bank_id`; returns whether the music bank,
// not the alternate, is selected.
bool SelectMusicBank(BankLoadHost& host, std::int32_t bank_id);
void ClearMusicBankIds(BankLoadHost& host);

void BeginBankStream(BankLoadHost& host);
// func_80012F10: uploads the next chunk of a streamed bank and returns the
// sample bytes still to come. A first chunk shorter than its header words,
// or one whose instruments would run past the instrument table, is refused
// and ends the stream as a non-AKAO chunk does.
std::uint32_t UploadBankChunk(BankLoadHost& host,
                              std::span<const std::byte> chunk,
                              std::int32_t slot, bool wait);

// Returns whether the bank went to the alternate music bank. A bank shorter
// than its header is refused and not uploaded.
bool LoadMusicBank(BankLoadHost& host, std::span<const std::byte> bank,
                   bool wait);
void LoadSoundEffectBank(BankLoadHost& host, std::span<const std::byte> bank,
                         std::int32_t slot, bool wait);

// Sound_CopyAndRelocateInstruments: copies `count` instruments with their
// sample addresses moved by `address_offset`, stopping at the end of either
// span; returns how many it copied. The original repeated a zero or negative
// count until it wrapped, so none is copied.
std::size_t CopyRelocatedInstruments(std::span<const InstrumentRecord> source,
                                     std::span<InstrumentRecord> destination,
                                     std::uint32_t address_offset,
                                     std::int32_t count);
// Sound_IsNotAkaoFile: the first word less "AKAO"; nullopt when `data` is
// shorter than a word.
[[nodiscard]] std::optional<std::int32_t> SequenceMagicDifference(
    std::span<const std::byte> data);
// func_80013588: 0 when the bank is AKAO and was uploaded, else -1.
std::int32_t LoadDeclaredBank(BankLoadHost& host,
                              std::span<const std::byte> bank, bool wait);

// Move a music instrument between the music and alternate banks when the
// music plays from the alternate bank; both return the instrument index.
std::uint16_t MapInstrumentToAlternateBank(std::uint32_t music_flags,
                                           ChannelInstrument& channel);
std::uint16_t MapInstrumentToBaseBank(std::uint32_t music_flags,
                                      ChannelInstrument& channel);

// The sound-effect bank (1 or 2) holding the bank id a selector names, else
// 0.
std::int32_t SoundEffectBankSlot(BankLoadHost& host, std::int32_t selector);
}  // namespace VS::Audio::Samples
