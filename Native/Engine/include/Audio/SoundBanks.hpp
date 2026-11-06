#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Audio/SoundPlayback.hpp"
#include "Content/CdDrive.hpp"

// The sound bank slots (vs_main_loadSfxSlot through vs_main_resetSound,
// SLUS_010.40 main.c:10045-10510): three sound-effect bank slots, the third
// shared with the room's effect bank, and one wave (music sample) bank, each
// loaded from the disc into the high end of the game heap. The slot record
// (vs_main_soundData), the file tables, the disc requests, the heap, the
// playback front end and the sample banks go through SoundBankHost.
namespace VS::Audio {
inline constexpr std::size_t kSfxBankSlotCount = 3;
inline constexpr std::size_t kMusicSlotCount = 4;
inline constexpr std::size_t kEffectStreamSlotCount = 3;
// The slot the room's effect bank binds (func_80046168), numbered from 1 as
// the slot functions number them.
inline constexpr std::uint32_t kRoomSfxBankSlot = 3;
// vs_main_resetSound's empty music handle and id.
inline constexpr std::uint32_t kNoMusicHandle = 0xFFFF;

// The original's diagnostics (vs_main_nop9) for a slot reloaded while its
// request or buffer was still held.
inline constexpr std::uint32_t kSfxRequestHeldDiagnostic = 0x8F;
inline constexpr std::uint32_t kSfxBufferHeldDiagnostic = 0x90;
inline constexpr std::uint32_t kWaveRequestHeldDiagnostic = 0x93;
inline constexpr std::uint32_t kWaveBufferHeldDiagnostic = 0x94;

// The sound-effect stops a released bank issues (func_80012288): the voices
// without tags, then the voices tagged from the bank's channel groups.
inline constexpr Playback::EffectSelector kUntaggedEffects{-2};
inline constexpr std::int32_t kSfxBankEffectTags = 0xFF000;
inline constexpr std::int32_t kRoomSfxBankEffectTags = 0xF00000;

// The other sound-effect channel groups' tag masks (vs_main_playSfx).
inline constexpr std::int32_t kPrimaryEffectTags = 0x1;
inline constexpr std::int32_t kSharedEffectTags = 0x7E;
inline constexpr std::int32_t kEffectTagGroup180 = 0x180;
inline constexpr std::int32_t kEffectTagGroup200 = 0x200;

// A channel group's first tag: the lowest bit of its mask.
constexpr std::int32_t LowestEffectTag(std::int32_t tags) {
  return tags & -tags;
}

// _loadMenuSound's disc mix volume, and the sound-effect bank slot its
// effect bank uploads to.
inline constexpr std::uint32_t kMenuDiscMixVolume = 0x7F;
inline constexpr std::int32_t kMenuSfxBankUploadSlot = 0;

// The load polls' results.
inline constexpr std::int32_t kLoadIdle = 0;
inline constexpr std::int32_t kLoadInFlight = 1;
inline constexpr std::int32_t kLoadFailed = -1;
// The sample upload's status (vs_sound_spuTransferring).
inline constexpr std::int32_t kTransferFinished = 0;
inline constexpr std::int32_t kTransferFailed = -1;

// A heap or static buffer a bank lies in, as an opaque handle: the core
// hands it to the host and never dereferences it.
enum class BankBuffer : std::uintptr_t { kNone = 0 };
// A disc request the host allocated, as an opaque handle.
enum class DiscRequestRef : std::uintptr_t { kNone = 0 };

// A slot's disc request: none, one the queue holds, or the failed marker the
// original stored as (vs_main_CdQueueSlot*)-1 once a read failed.
enum class RequestMark : std::uint8_t { kNone, kActive, kFailed };
struct BankRequest {
  RequestMark mark = RequestMark::kNone;
  DiscRequestRef request = DiscRequestRef::kNone;

  friend bool operator==(const BankRequest&, const BankRequest&) = default;
};

// One sound-effect bank slot (sfxBankIds, sfxBankData, sfxBankRequests).
struct SfxBankSlot {
  std::int8_t id = 0;
  BankBuffer data = BankBuffer::kNone;
  BankRequest request;

  friend bool operator==(const SfxBankSlot&, const SfxBankSlot&) = default;
};

// The wave bank (waveBankId, waveBankData, waveBankRequest).
struct WaveBankSlot {
  std::int32_t file = 0;
  BankBuffer data = BankBuffer::kNone;
  BankRequest request;

  friend bool operator==(const WaveBankSlot&, const WaveBankSlot&) = default;
};

// One music slot (musicIds, musicHandles, musicData, musicRequests).
struct MusicSlot {
  // The original reads musicIds unsigned (lbu): ids run past 127.
  std::uint8_t id = 0;
  std::uint32_t handle = kNoMusicHandle;
  BankBuffer data = BankBuffer::kNone;
  BankRequest request;

  friend bool operator==(const MusicSlot&, const MusicSlot&) = default;
};

// One sound-effect stream slot (sfxStreamData and the unread reserved70).
struct EffectStreamSlot {
  BankBuffer data = BankBuffer::kNone;
  BankBuffer reserved = BankBuffer::kNone;

  friend bool operator==(const EffectStreamSlot&,
                         const EffectStreamSlot&) = default;
};

// vs_main_soundData as a whole, for vs_main_resetSound.
struct SoundSlots {
  std::int32_t current_music_slot = 0;
  std::uint32_t playing_music_handle = kNoMusicHandle;
  std::uint32_t current_music_id = kNoMusicHandle;
  BankBuffer current_music_data = BankBuffer::kNone;
  std::array<MusicSlot, kMusicSlotCount> music{};
  std::uint32_t current_sfx_bank = 0;
  std::array<SfxBankSlot, kSfxBankSlotCount> sfx_banks{};
  std::array<EffectStreamSlot, kEffectStreamSlotCount> effect_streams{};
  WaveBankSlot wave_bank;

  friend bool operator==(const SoundSlots&, const SoundSlots&) = default;
};

// The next effect tag each sound-effect channel group hands out
// (D_8005FE70-D_8005FE84), named for the tag masks the groups cover.
struct EffectTagCursors {
  std::int32_t group_1 = LowestEffectTag(kPrimaryEffectTags);
  std::int32_t group_7e = LowestEffectTag(kSharedEffectTags);
  std::int32_t group_180 = LowestEffectTag(kEffectTagGroup180);
  std::int32_t group_200 = LowestEffectTag(kEffectTagGroup200);
  std::int32_t group_ff000 = LowestEffectTag(kSfxBankEffectTags);
  std::int32_t group_f00000 = LowestEffectTag(kRoomSfxBankEffectTags);

  friend bool operator==(const EffectTagCursors&,
                         const EffectTagCursors&) = default;
};

// The files _loadMenuSound reads.
enum class MenuSoundFile : std::uint8_t {
  kWave0000,
  kWave0005,
  kWave0200,
  kEffect00,
};

enum class SoundBankRefusal : std::uint8_t {
  kSfxBankFileOutOfRange,
  kWaveBankFileOutOfRange,
  kWaveBankMapOutOfRange,
};

class SoundBankHost {
 public:
  SoundBankHost() = default;
  SoundBankHost(const SoundBankHost&) = delete;
  SoundBankHost& operator=(const SoundBankHost&) = delete;
  SoundBankHost(SoundBankHost&&) = delete;
  SoundBankHost& operator=(SoundBankHost&&) = delete;
  virtual ~SoundBankHost() = default;

  // Sound-effect bank slot `index` (below kSfxBankSlotCount).
  virtual SfxBankSlot SfxBank(std::size_t index) = 0;
  virtual void StoreSfxBank(std::size_t index, const SfxBankSlot& slot) = 0;
  // The selected slot, numbered from 1 (0 when none is).
  virtual std::uint32_t CurrentSfxBank() = 0;
  virtual void StoreCurrentSfxBank(std::uint32_t slot) = 0;
  virtual WaveBankSlot WaveBank() = 0;
  virtual void StoreWaveBank(const WaveBankSlot& bank) = 0;
  virtual void StoreSoundSlots(const SoundSlots& slots) = 0;
  virtual void StoreEffectTagCursors(const EffectTagCursors& cursors) = 0;

  // The file tables; nullopt for an index past them.
  virtual std::optional<Content::DiscFileExtent> SfxBankFile(
      std::int32_t id) = 0;
  virtual std::optional<Content::DiscFileExtent> WaveBankFile(
      std::int32_t file) = 0;
  // _soundFileMap: the wave bank file a zone or script names.
  virtual std::optional<std::int32_t> MappedWaveBankFile(std::int32_t id) = 0;
  virtual Content::DiscFileExtent MenuFile(MenuSoundFile file) = 0;
  // The static buffer _loadMenuSound reads EFFECT00 into (_sfxData).
  virtual BankBuffer MenuSoundEffects() = 0;

  // The disc request queue. A full queue allocates none; a refused request
  // is read without the queue into a record that reports Loaded or Error.
  virtual std::optional<DiscRequestRef> AllocateRequest(
      const Content::DiscFileExtent& file) = 0;
  virtual void EnqueueRequest(DiscRequestRef request,
                              BankBuffer destination) = 0;
  virtual DiscRequestRef CompleteRefusedRequest(
      const Content::DiscFileExtent& file, BankBuffer destination) = 0;
  virtual Content::DiscRequestState RequestState(DiscRequestRef request) = 0;
  virtual void MarkRequestDispatched(DiscRequestRef request) = 0;
  virtual void ReleaseRequest(DiscRequestRef request) = 0;
  virtual void ReadFileBlocking(const Content::DiscFileExtent& file,
                                BankBuffer destination) = 0;
  // vs_main_updatePlayTimeAndLoading(0): one frame of waiting on a load.
  virtual void WaitFrame() = 0;

  // The high end of the game heap; kNone when it cannot satisfy `bytes`.
  virtual BankBuffer AllocateBuffer(std::int32_t bytes) = 0;
  virtual void FreeBuffer(BankBuffer buffer) = 0;

  // The playback front end (SoundPlayback.hpp) and the sample banks
  // (SampleBanks.hpp).
  virtual void InitializePlayback() = 0;
  virtual void SetDiscMixVolume(std::uint32_t volume) = 0;
  virtual void BindSoundEffectPrograms(BankBuffer blob) = 0;
  virtual void StopSoundEffects(Playback::EffectSelector selector,
                                std::int32_t effect_tag_mask) = 0;
  virtual void SetMonoOutput(std::int32_t mono_enabled) = 0;
  virtual Playback::MusicBankIds MusicBanks() = 0;
  virtual void LoadBank(BankBuffer bank, bool wait) = 0;
  virtual void LoadSoundEffectBank(BankBuffer bank, std::int32_t slot,
                                   bool wait) = 0;
  virtual void LoadMusicBank(BankBuffer bank, bool wait) = 0;
  // vs_sound_spuTransferring: kTransferFinished, kTransferFailed or still
  // uploading.
  virtual std::int32_t TransferStatus() = 0;
  // Audio_Native_ResetCutsceneStream.
  virtual void ResetCutsceneStream() = 0;

  virtual void Diagnose(std::uint32_t code) = 0;
  virtual void Refuse(SoundBankRefusal refusal) = 0;
};

// Starts loading sound-effect bank `id` into `slot` (1 to 3) when the slot
// holds no bank. Returns the slot, or 0 when it is taken, out of range or
// the id is past the file table (refused).
std::int32_t LoadSfxBankSlot(SoundBankHost& host, std::int32_t id,
                             std::uint32_t slot);
std::int32_t LoadSfxBankIntoFreeSlot(SoundBankHost& host, std::int32_t id);
// kLoadInFlight while the slot's load is in flight, kLoadIdle once it loaded
// (or when the slot has none), kLoadFailed when the read failed. The slot
// then keeps the failed marker, which polls as kLoadIdle: nothing is in
// flight. The original read the request through the marker instead.
std::int32_t PollSfxBankSlotLoad(SoundBankHost& host, std::uint32_t slot);
std::int32_t PollSfxBankLoads(SoundBankHost& host);
// Loads `id` into a free slot and waits until no slot's load is in flight.
std::int32_t LoadSfxBankBlocking(SoundBankHost& host, std::int32_t id);
std::int32_t SelectSfxBankSlot(SoundBankHost& host, std::int32_t slot);
std::int32_t FreeSfxBankSlot(SoundBankHost& host, std::uint32_t slot);
// Binds the room's effect bank into the third slot when it is empty.
std::int32_t BindRoomSfxBank(SoundBankHost& host, BankBuffer bank);
std::int32_t UnbindRoomSfxBank(SoundBankHost& host);
// Starts loading wave bank `file` unless a music bank already holds it. A
// file past the table is refused.
void LoadWaveBankFile(SoundBankHost& host, std::int32_t file);
void LoadMappedWaveBank(SoundBankHost& host, std::int32_t id);
// kLoadInFlight while the wave bank loads or uploads, kLoadIdle once it
// uploaded (or when none is loading), kLoadFailed when the read or the
// upload failed; both of the last release its request and buffer.
std::int32_t UpdateWaveBankLoad(SoundBankHost& host);
void LoadWaveBankBlocking(SoundBankHost& host, std::int32_t id);
void SetMonoOutput(SoundBankHost& host, std::int32_t mono_enabled);
void InitializeMenuSoundBanks(SoundBankHost& host);
void ResetSoundSlots(SoundBankHost& host);
}  // namespace VS::Audio
