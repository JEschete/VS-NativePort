#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/SoundBanks.hpp"
#include "Audio/SoundPlayback.hpp"

// The sound-effect front end (func_80045630 through func_80046494,
// SLUS_010.40 main.c:9785-10310): built-in effects and the selected and room
// banks' effects played by channel group, panned and attenuated from a screen
// projection, and their stops and fades; and the effects of a model's or a
// room's own bank. The channel groups' tag cursors and the bank slots are an
// SfxState; the playback front end, the sound control requests, the bank
// memory and the projection go through SfxHost.
namespace VS::Audio {
// The channel groups vs_main_playSfx takes besides the tag masks: an alias of
// the shared group, and an alias of group 0x200's cursor.
inline constexpr std::int32_t kSharedEffectGroupAlias = 0xFE;
inline constexpr std::int32_t kEffectTagGroup100 = 0x100;
// The id bits a built-in effect plays with.
inline constexpr std::int32_t kBuiltInSfxIdMask = 0xFFFF;
// vs_main_nop9's diagnostic for an unknown channel group.
inline constexpr std::uint32_t kUnknownSfxGroupDiagnostic = 0x95;
// The fade requests' ticks: the low byte counts.
inline constexpr std::int32_t kSfxFadeTicksMask = 0xFF;
// vs_main_playSfxDefault's centered pan and full volume.
inline constexpr std::int32_t kCenteredSfxPan = 0x80;
inline constexpr std::int32_t kFullSfxVolume = 0x7F;

// A sound effect's pan and volume; centered at full volume by default.
struct SfxMix {
  std::int32_t pan = kCenteredSfxPan;
  std::int32_t volume = kFullSfxVolume;

  friend bool operator==(const SfxMix&, const SfxMix&) = default;
};

// The channel groups' tag cursors (D_8005FE70-D_8005FE84), the selected
// sound-effect bank slot numbered from 1 (0 for none) and the bank slots'
// data (vs_main_soundData).
struct SfxState {
  EffectTagCursors cursors{};
  std::uint32_t selected_bank = 0;
  std::array<Playback::DataRef, kSfxBankSlotCount> banks{};
};

// A point in the world (an SVECTOR's coordinates).
struct SfxPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
};

// A point's projection: the screen position (x in the low halfword, y in the
// high halfword, RotTransPers' sxy) and its depth.
struct SfxProjection {
  std::int32_t screen_xy = 0;
  std::int32_t depth = 0;
};

// A stop or fade's effect: an effect id within a channel group, or the whole
// group when the id is 0.
struct SfxTarget {
  std::int32_t group = 0;
  std::int32_t effect_id = 0;
};

// A fade's target value and its ticks.
struct SfxFadeSpec {
  std::int32_t target = 0;
  std::int32_t ticks = 0;
};

// The value a fade moves (func_8001264C, func_80012824, func_80012738).
enum class SfxFade : std::uint8_t { kVolume, kPitch, kPan };

// An effect in a model's or a room's own bank: a count word, then one offset
// word per effect from the bank's start; `index` names the effect.
struct BankSfx {
  Playback::DataRef bank = Playback::DataRef::kNone;
  std::int32_t index = 0;
};

// What a request refused, and the values its report names.
enum class SfxRefusal : std::uint8_t {
  // vs_main_playSfx: a private effect with no bank selected (a warning).
  kNoSelectedBank,
  // The selected bank slot is past the slots.
  kSelectedBankOutOfRange,
  // The selected slot holds no bank, or the effect id is negative.
  kBankUnavailable,
  // A room effect with no room bank (a warning).
  kNoRoomBank,
  // A room effect id is negative.
  kRoomEffectNegative,
  // func_80045B28: a banked effect with no bank selected, or the selected
  // slot past the slots.
  kUnresolvableBank,
  // The selected slot holds no bank, or the effect id is outside its count.
  kProgramUnavailable,
  // The effect's offset is negative or points into the offset words.
  kProgramOffsetInvalid,
  // A bank's count or offset word lies outside readable memory.
  kBankUnreadable,
};
struct SfxRequest {
  std::int32_t group = 0;
  std::int32_t effect_id = 0;
  std::uint32_t bank = 0;
  std::int32_t offset = 0;
};

// The playback front end, the sound control requests, the bank memory and the
// projection the front end uses.
class SfxHost {
 public:
  SfxHost() = default;
  virtual ~SfxHost() = default;
  SfxHost(const SfxHost&) = delete;
  SfxHost& operator=(const SfxHost&) = delete;
  SfxHost(SfxHost&&) = delete;
  SfxHost& operator=(SfxHost&&) = delete;

  // func_8001217C: plays shared effect `effect_id` with tag `effect_tag`.
  virtual void PlaySoundEffect(std::int32_t effect_id, std::int32_t effect_tag,
                               const SfxMix& mix) = 0;
  // func_800121F0: plays the effect program at `program`.
  virtual void PlaySoundEffectProgram(Playback::DataRef program,
                                      std::int32_t effect_tag,
                                      const SfxMix& mix) = 0;
  // func_80012358: the effect a voice tagged `effect_tag` plays.
  virtual Playback::EffectSelector SoundEffectIdForTag(
      std::int32_t effect_tag) = 0;
  // func_800123C8 for a shared effect id, and for an effect program.
  virtual bool SharedEffectUsesTags(std::int32_t effect_id) = 0;
  virtual bool ProgramUsesTags(Playback::DataRef program) = 0;
  // func_80012288: stops `selector`'s voices, or the voices tagged from
  // `effect_tag_mask` when `selector` is kNone.
  virtual void StopSoundEffects(Playback::EffectSelector selector,
                                std::int32_t effect_tag_mask) = 0;
  // func_80012B98: stops the scene's sound effects.
  virtual void StopSceneSoundEffects() = 0;
  // Fades `selector`'s voices, or the voices tagged from `effect_tag_mask`.
  virtual void FadeSoundEffect(SfxFade fade, Playback::EffectSelector selector,
                               std::int32_t effect_tag_mask,
                               const SfxFadeSpec& spec) = 0;
  // The bytes from `bank`'s start to the end of the memory it lies in; empty
  // when it lies outside readable memory.
  virtual std::span<const std::byte> BankBytes(Playback::DataRef bank) = 0;
  // RotTransPers through the view matrix (the scratch pad's at 0x1F800014).
  virtual SfxProjection Project(const SfxPoint& point) = 0;
  // vs_main_nop9.
  virtual void Diagnose(std::uint32_t code) = 0;
  virtual void Refuse(SfxRefusal refusal, const SfxRequest& request) = 0;
};

// func_80045630: plays built-in effect `effect_id` on the primary group's
// tag, centered at full volume.
void PlaySfxOnPrimaryChannel(SfxState& state, SfxHost& host,
                             std::int32_t effect_id);
// func_80045684: plays built-in effect `effect_id`, unless `effect_tag` is
// group 0x200's and its voice already plays the effect.
void PlayBuiltInSfxChannel(SfxHost& host, std::int32_t effect_id,
                           std::int32_t effect_tag, const SfxMix& mix);
// func_800456EC: the same for the effect program at `program`.
void PlaySfxProgramChannel(SfxHost& host, Playback::DataRef program,
                           std::int32_t effect_tag, const SfxMix& mix);
// vs_main_playSfx: plays effect `effect_id` of channel group `group`, a
// built-in effect or one of the selected or the room bank's effects, on the
// group's next tag when the effect uses its tags.
void PlaySfx(SfxState& state, SfxHost& host, std::int32_t group,
             std::int32_t effect_id, const SfxMix& mix);
// vs_main_playSfxDefault: the same, centered at full volume.
void PlaySfxCentered(SfxState& state, SfxHost& host, std::int32_t group,
                     std::int32_t effect_id);
// vs_main_computeSfxPan: the pan from the screen x and the volume from the
// depth.
SfxMix SfxPanAndVolume(const SfxProjection& projection);
// _do_panSfx: plays the effect panned and attenuated from `projection`, when
// it is audible.
void PlayProjectedSfx(SfxState& state, SfxHost& host, std::int32_t group,
                      std::int32_t effect_id, const SfxProjection& projection);
// vs_main_panSfx: the same from the projection of `point`.
void PlayWorldSfx(SfxState& state, SfxHost& host, std::int32_t group,
                  std::int32_t effect_id, const SfxPoint& point);
// func_80045B28: the program of a banked group's effect, kNone for a
// built-in group's effect or an unknown group; empty when refused.
std::optional<Playback::DataRef> ResolveSfxProgram(const SfxState& state,
                                                   SfxHost& host,
                                                   std::int32_t group,
                                                   std::int32_t effect_id);
// func_80045BFC, func_80045C74 and func_80045CEC: fade the target's voices.
void FadeSfxVolume(const SfxState& state, SfxHost& host,
                   const SfxTarget& target, const SfxFadeSpec& spec);
void FadeSfxPitch(const SfxState& state, SfxHost& host, const SfxTarget& target,
                  const SfxFadeSpec& spec);
void FadeSfxPan(const SfxState& state, SfxHost& host, const SfxTarget& target,
                const SfxFadeSpec& spec);
// func_80045D64: stops the target's voices.
void StopSfx(const SfxState& state, SfxHost& host, const SfxTarget& target);
// func_80045DC0: stops the scene's sound effects.
void StopAllSfxPlayback(SfxHost& host);
// func_800461CC: plays the bank's effect when its index is within the bank's
// count, on the next tag of group 0x180, 0x200 or 0xF00000.
void PlayBankSfx(SfxState& state, SfxHost& host, std::int32_t group,
                 const BankSfx& effect, const SfxMix& mix);
// func_800462E8: the same, centered at full volume.
void PlayBankSfxCentered(SfxState& state, SfxHost& host, std::int32_t group,
                         const BankSfx& effect);
// func_80046310: the same, panned and attenuated from `projection`, when it
// is audible.
void PlayProjectedBankSfx(SfxState& state, SfxHost& host, std::int32_t group,
                          const BankSfx& effect,
                          const SfxProjection& projection);
// func_800463BC: the same from the projection of `point`.
void PlayWorldBankSfx(SfxState& state, SfxHost& host, std::int32_t group,
                      const BankSfx& effect, const SfxPoint& point);
// func_8004644C: stops the bank effect's voices, or the group's when the
// index is 0.
void StopBankSfx(SfxHost& host, std::int32_t group, const BankSfx& effect);
// func_80046494: fades the bank effect's volume, or the group's when the
// index is 0.
void FadeBankSfxVolume(SfxHost& host, std::int32_t group, const BankSfx& effect,
                       const SfxFadeSpec& spec);
}  // namespace VS::Audio
