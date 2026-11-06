// Hand-maintained (tools/port/owned.csv).
// Typed sample bank loaders (SampleBanks.hpp).
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

#include "Audio/SampleBanks.hpp"

namespace VS::Audio::Samples {
namespace {
constexpr std::size_t kWordBytes = sizeof(std::uint32_t);
constexpr std::size_t kBankIdWord = 1;

std::uint32_t ReadWord(std::span<const std::byte> bytes, std::size_t word) {
  std::uint32_t value = 0;
  std::memcpy(&value, bytes.subspan(word * kWordBytes, kWordBytes).data(),
              sizeof(value));
  if constexpr (std::endian::native == std::endian::big) {
    value = std::byteswap(value);
  }
  return value;
}

InstrumentRecord Relocated(InstrumentRecord record,
                           std::uint32_t address_offset) {
  record.sample_start += address_offset;
  record.loop_start += address_offset;
  return record;
}

// The bytes of `chunk` from `position`, at most `size` of them.
std::span<const std::byte> Within(std::span<const std::byte> chunk,
                                  std::size_t position, std::size_t size) {
  if (position >= chunk.size()) return {};
  return chunk.subspan(position, std::min(size, chunk.size() - position));
}

// Where an AKAO bank's first chunk sends it: the music banks and the
// sound-effect banks choose their own halves; any other bank goes where it
// declares.
BankDestination StreamDestination(BankLoadHost& host,
                                  const BankDeclaration& bank,
                                  std::int32_t slot) {
  if (bank.destination.sample_address == kMusicBank.sample_address) {
    return ChooseMusicBankDestination(host, bank).destination;
  }
  if (bank.destination.sample_address == kSoundEffectBanks[0].sample_address) {
    return ChooseSoundEffectBankDestination(host, bank, slot);
  }
  return bank.destination;
}

bool InstrumentsFit(const BankDestination& destination,
                    std::uint32_t instrument_count) {
  if (destination.first_instrument < 0) return false;
  const auto first = static_cast<std::size_t>(destination.first_instrument);
  return first < kInstrumentSlotCount &&
         instrument_count <= kInstrumentSlotCount - first;
}

// The stream's first chunk: its header words, then where its instruments
// and samples go. Returns the bytes the header took, or nullopt when the
// chunk does not start an AKAO bank.
std::optional<std::size_t> StartStream(BankLoadHost& host,
                                       BankStreamState& stream,
                                       std::span<const std::byte> chunk,
                                       std::int32_t slot) {
  const std::optional<std::int32_t> not_akao = SequenceMagicDifference(chunk);
  if (not_akao && *not_akao != 0) return std::nullopt;
  if (chunk.size() < kChunkHeaderBytes) {
    host.Refuse(BankRefusal::kHeaderTruncated);
    return std::nullopt;
  }
  const std::optional<BankDeclaration> bank = ReadBankDeclaration(chunk);
  if (!bank) return std::nullopt;
  const BankDestination destination = StreamDestination(host, *bank, slot);
  if (!InstrumentsFit(destination, bank->instrument_count)) {
    host.Refuse(BankRefusal::kInstrumentsOutOfRange);
    return std::nullopt;
  }
  for (std::size_t word = 0; word < kChunkHeaderWords; ++word) {
    stream.header.at(word) = ReadWord(chunk, word);
  }
  stream.header.at(kHeaderSampleAddressWord) = destination.sample_address;
  stream.header.at(kHeaderFirstInstrumentWord) =
      static_cast<std::uint32_t>(destination.first_instrument);
  stream.sample_address = destination.sample_address;
  stream.sample_bytes_remaining = stream.header.at(kHeaderSampleBytesWord);
  stream.instrument_offset =
      static_cast<std::size_t>(destination.first_instrument) *
      kInstrumentRecordBytes;
  stream.instrument_bytes_remaining =
      bank->instrument_count *
      static_cast<std::uint32_t>(kInstrumentRecordBytes);
  return kChunkHeaderBytes;
}

// The streamed instruments once all have arrived, relocated in place by the
// bank's sample address.
void RelocateStreamedInstruments(BankLoadHost& host,
                                 const BankStreamState& stream) {
  const std::size_t first = stream.header.at(kHeaderFirstInstrumentWord);
  const std::size_t count = stream.header.at(kHeaderInstrumentCountWord);
  const std::uint32_t offset = stream.header.at(kHeaderSampleAddressWord);
  const std::size_t end = std::min(first + count, kInstrumentSlotCount);
  for (std::size_t slot = first; slot < end; ++slot) {
    host.StoreInstrument(slot, Relocated(host.Instrument(slot), offset));
  }
}
}  // namespace

std::optional<BankDeclaration> ReadBankDeclaration(
    std::span<const std::byte> header) {
  if (header.size() < kBankHeaderBytes) return std::nullopt;
  return BankDeclaration{
      .bank_id = static_cast<std::int32_t>(ReadWord(header, kBankIdWord)),
      .destination = {.first_instrument = static_cast<std::int32_t>(
                          ReadWord(header, kHeaderFirstInstrumentWord)),
                      .sample_address =
                          ReadWord(header, kHeaderSampleAddressWord)},
      .sample_bytes = ReadWord(header, kHeaderSampleBytesWord),
      .instrument_count = ReadWord(header, kHeaderInstrumentCountWord)};
}

// decomp: SLUS_010.40 func_80012BB8 (src/SLUS_010.40/25AC.c:719)
void LoadBank(BankLoadHost& host, std::span<const std::byte> bank, bool wait) {
  LoadDeclaredBank(host, bank, wait);
}

// decomp: SLUS_010.40 vs_sound_spuTransferring (src/SLUS_010.40/25AC.c:725)
std::int32_t TransferStatus(BankLoadHost& host) {
  return host.TransferStatus();
}

// decomp: SLUS_010.40 func_80012C14 (src/SLUS_010.40/25AC.c:727)
MusicBankChoice ChooseMusicBankDestination(BankLoadHost& host,
                                           const BankDeclaration& bank) {
  MusicBankState state = host.MusicBanks();
  if (state.active_channels == 0) state.music_flags &= ~kMusicBankFlags;
  bool alternate = false;
  if (bank.instrument_count > kMusicBankInstrumentLimit ||
      bank.sample_bytes > kMusicBankSampleLimit) {
    state.music_bank_selected = false;
    state.alternate_bank_id = 0;
    state.bank_id = 0;
  } else if ((state.music_flags & kLoadIntoAlternateBank) != 0 ||
             ((state.music_flags & kPlaysFromAlternateBank) == 0 &&
              state.music_bank_selected)) {
    alternate = true;
    state.music_bank_selected = false;
  } else {
    state.music_bank_selected = true;
  }
  if (alternate) {
    state.alternate_bank_id = bank.bank_id;
    if (state.bank_id == bank.bank_id) state.bank_id = 0;
  } else {
    state.bank_id = bank.bank_id;
    if (state.alternate_bank_id == bank.bank_id) state.alternate_bank_id = 0;
  }
  host.StoreMusicBanks(state);
  return {.alternate = alternate,
          .destination = alternate ? kAlternateMusicBank : kMusicBank};
}

// decomp: SLUS_010.40 func_80012D9C (src/SLUS_010.40/25AC.c:768)
BankDestination ChooseSoundEffectBankDestination(BankLoadHost& host,
                                                 const BankDeclaration& bank,
                                                 std::int32_t slot) {
  SoundEffectBankIds ids = host.SoundEffectBanks();
  for (std::int32_t& id : ids) {
    if (id == bank.bank_id) id = 0;
  }
  const std::size_t index =
      slot == 1 || slot == 2 ? static_cast<std::size_t>(slot) : 0;
  ids.at(index) = bank.bank_id;
  host.StoreSoundEffectBanks(ids);
  return kSoundEffectBanks.at(index);
}

// decomp: SLUS_010.40 func_80012E50 (src/SLUS_010.40/25AC.c:797)
bool SelectMusicBank(BankLoadHost& host, std::int32_t bank_id) {
  MusicBankState state = host.MusicBanks();
  if (state.bank_id == bank_id) {
    state.music_bank_selected = true;
  } else if (state.alternate_bank_id == bank_id) {
    state.music_bank_selected = false;
  }
  host.StoreMusicBanks(state);
  return state.music_bank_selected;
}

// decomp: SLUS_010.40 func_80012EBC (src/SLUS_010.40/25AC.c:807)
void ClearMusicBankIds(BankLoadHost& host) {
  MusicBankState state = host.MusicBanks();
  state.music_bank_selected = false;
  state.alternate_bank_id = 0;
  state.bank_id = 0;
  host.StoreMusicBanks(state);
}

// decomp: SLUS_010.40 func_80012EF0 (src/SLUS_010.40/25AC.c:815)
void BeginBankStream(BankLoadHost& host) {
  BankStreamState stream = host.Stream();
  stream.sample_address = 0;
  stream.active = true;
  host.StoreStream(stream);
}

// decomp: SLUS_010.40 func_80012F10 (src/SLUS_010.40/25AC.c:822)
std::uint32_t UploadBankChunk(BankLoadHost& host,
                              std::span<const std::byte> chunk,
                              std::int32_t slot, bool wait) {
  BankStreamState stream = host.Stream();
  if (!stream.active) return stream.sample_bytes_remaining;
  std::size_t position = 0;
  auto size = static_cast<std::uint32_t>(chunk.size());
  if (stream.sample_address == 0) {
    if (const std::optional<std::size_t> header =
            StartStream(host, stream, chunk, slot)) {
      position = *header;
      size -= static_cast<std::uint32_t>(*header);
    } else {
      size = 0;
      stream.sample_bytes_remaining = 0;
      stream.instrument_bytes_remaining = 0;
    }
  }
  if (stream.instrument_bytes_remaining != 0 && size != 0) {
    const std::uint32_t part =
        std::min(stream.instrument_bytes_remaining, size);
    // The original copies whole words but counts the part's every byte.
    const std::size_t copied = part / kWordBytes * kWordBytes;
    host.WriteInstrumentBytes(stream.instrument_offset,
                              Within(chunk, position, copied));
    position += copied;
    size -= part;
    stream.instrument_offset += copied;
    stream.instrument_bytes_remaining -= part;
    if (stream.instrument_bytes_remaining == 0) {
      RelocateStreamedInstruments(host, stream);
    }
  }
  if (size != 0 && stream.sample_bytes_remaining != 0) {
    const std::uint32_t part = std::min(stream.sample_bytes_remaining, size);
    host.WriteSampleMemory(stream.sample_address,
                           Within(chunk, position, part));
    stream.sample_address += part;
    stream.sample_bytes_remaining -= part;
    if (wait) host.WaitForSampleTransfer();
  }
  if (stream.sample_bytes_remaining == 0) stream.active = false;
  host.StoreStream(stream);
  return stream.sample_bytes_remaining;
}

// decomp: SLUS_010.40 func_80013188 (src/SLUS_010.40/25AC.c:894)
bool LoadMusicBank(BankLoadHost& host, std::span<const std::byte> bank,
                   bool wait) {
  const std::optional<BankDeclaration> declaration = ReadBankDeclaration(bank);
  if (!declaration) {
    host.Refuse(BankRefusal::kHeaderTruncated);
    return false;
  }
  const MusicBankChoice choice = ChooseMusicBankDestination(host, *declaration);
  host.UploadBank(bank, wait, choice.destination);
  return choice.alternate;
}

// decomp: SLUS_010.40 func_800131DC (src/SLUS_010.40/25AC.c:903)
void LoadSoundEffectBank(BankLoadHost& host, std::span<const std::byte> bank,
                         std::int32_t slot, bool wait) {
  const std::optional<BankDeclaration> declaration = ReadBankDeclaration(bank);
  if (!declaration) {
    host.Refuse(BankRefusal::kHeaderTruncated);
    return;
  }
  host.UploadBank(bank, wait,
                  ChooseSoundEffectBankDestination(host, *declaration, slot));
}

// decomp: SLUS_010.40 Sound_CopyAndRelocateInstruments
// (src/SLUS_010.40/25AC.c:970)
std::size_t CopyRelocatedInstruments(std::span<const InstrumentRecord> source,
                                     std::span<InstrumentRecord> destination,
                                     std::uint32_t address_offset,
                                     std::int32_t count) {
  if (count <= 0) return 0;
  const std::size_t copied = std::min(
      {static_cast<std::size_t>(count), source.size(), destination.size()});
  for (std::size_t i = 0; i < copied; ++i) {
    destination[i] = Relocated(source[i], address_offset);
  }
  return copied;
}

// decomp: SLUS_010.40 Sound_IsNotAkaoFile (src/SLUS_010.40/25AC.c:984)
std::optional<std::int32_t> SequenceMagicDifference(
    std::span<const std::byte> data) {
  if (data.size() < kWordBytes) return std::nullopt;
  return static_cast<std::int32_t>(ReadWord(data, 0) - kAkaoMagic);
}

// decomp: SLUS_010.40 func_80013588 (src/SLUS_010.40/25AC.c:1023)
std::int32_t LoadDeclaredBank(BankLoadHost& host,
                              std::span<const std::byte> bank, bool wait) {
  if (SequenceMagicDifference(bank) != 0) return -1;
  const std::optional<BankDeclaration> declaration = ReadBankDeclaration(bank);
  if (!declaration) {
    host.Refuse(BankRefusal::kHeaderTruncated);
    return -1;
  }
  host.UploadBank(bank, wait, declaration->destination);
  return 0;
}

// decomp: SLUS_010.40 Sound_MapInstrumentToAltSampleBank
// (src/SLUS_010.40/25AC.c:2303)
std::uint16_t MapInstrumentToAlternateBank(std::uint32_t music_flags,
                                           ChannelInstrument& channel) {
  if ((music_flags & static_cast<std::uint32_t>(kPlaysFromAlternateBank)) !=
          0 &&
      channel.instrument_index >= kMusicBank.first_instrument &&
      channel.instrument_index < kAlternateMusicBank.first_instrument) {
    channel.sample_start += kAlternateBankSampleOffset;
    channel.loop_start += kAlternateBankSampleOffset;
    channel.instrument_index = static_cast<std::uint16_t>(
        channel.instrument_index + kAlternateBankInstrumentOffset);
  }
  return channel.instrument_index;
}

// decomp: SLUS_010.40 Sound_MapInstrumentToBaseSampleBank
// (src/SLUS_010.40/25AC.c:2315)
std::uint16_t MapInstrumentToBaseBank(std::uint32_t music_flags,
                                      ChannelInstrument& channel) {
  if ((music_flags & static_cast<std::uint32_t>(kPlaysFromAlternateBank)) !=
          0 &&
      channel.instrument_index >= kAlternateMusicBank.first_instrument &&
      channel.instrument_index < kMusicBanksEnd) {
    channel.sample_start -= kAlternateBankSampleOffset;
    channel.loop_start -= kAlternateBankSampleOffset;
    channel.instrument_index = static_cast<std::uint16_t>(
        channel.instrument_index - kAlternateBankInstrumentOffset);
  }
  return channel.instrument_index;
}

// decomp: SLUS_010.40 func_80016DA8 (src/SLUS_010.40/25AC.c:2941)
std::int32_t SoundEffectBankSlot(BankLoadHost& host, std::int32_t selector) {
  const std::int32_t bank_id = selector & kSoundEffectBankIdMask;
  if (bank_id == 0) return 0;
  const SoundEffectBankIds ids = host.SoundEffectBanks();
  if (bank_id == ids[1]) return 1;
  if (bank_id == ids[2]) return 2;
  return 0;
}
}  // namespace VS::Audio::Samples
