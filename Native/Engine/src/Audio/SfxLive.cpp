// Hand-maintained (tools/port/owned.csv).
// Live binding for Sfx.hpp: the channel groups' tag cursors, the sound-effect
// bank slots, the playback front end, the sound control requests, the
// sequence memory and the geometry transform through the view matrix.
#include "Audio/SfxLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

#include "Audio/SeqRecordsLive.hpp"
#include "Audio/Sfx.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Core/Log.hpp"
#include "Core/Runtime.hpp"

extern "C" {
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Legacy/Psx.h"
#include "Render/Render.h"
}

namespace VS::Audio {
namespace {
using Playback::DataRef;
using Playback::EffectSelector;

static_assert(std::size(Audio_soundState.sfxBankData) == kSfxBankSlotCount);
static_assert(sizeof(EffectSelector) == sizeof(std::intptr_t));
// The pan's place in vs_main_computeSfxPan's result.
constexpr unsigned kPanShift = 16;
// LiveResolveSfxProgram's refused result.
constexpr std::intptr_t kRefusedProgram = -1;
constexpr const char* kLogChannel = "audio.sfx";

// Pins a legacy field to the width of its typed copy.
template <typename To, typename From>
To Same(From value) {
  static_assert(sizeof(To) == sizeof(From));
  return static_cast<To>(value);
}

DataRef Reference(const void* data) {
  return static_cast<DataRef>(std::bit_cast<std::uintptr_t>(data));
}

void* Pointer(DataRef data) {
  return std::bit_cast<void*>(static_cast<std::uintptr_t>(data));
}

SfxState LoadState() {
  SfxState state{
      .cursors = {.group_1 = Audio_nextSfxChannelGroup1,
                  .group_7e = Audio_nextSfxChannelGroup7E,
                  .group_180 = Audio_nextSfxChannelGroup180,
                  .group_200 = Audio_nextSfxChannelGroup200,
                  .group_ff000 = Audio_nextSfxChannelGroupFF000,
                  .group_f00000 = Audio_nextSfxChannelGroupF00000},
      .selected_bank = Same<std::uint32_t>(Audio_soundState.currentSfxBank)};
  for (std::size_t slot = 0; slot < kSfxBankSlotCount; ++slot) {
    state.banks[slot] = Reference(Audio_soundState.sfxBankData[slot]);
  }
  return state;
}

void StoreCursors(const EffectTagCursors& cursors) {
  Audio_nextSfxChannelGroup1 = cursors.group_1;
  Audio_nextSfxChannelGroup7E = cursors.group_7e;
  Audio_nextSfxChannelGroup180 = cursors.group_180;
  Audio_nextSfxChannelGroup200 = cursors.group_200;
  Audio_nextSfxChannelGroupFF000 = cursors.group_ff000;
  Audio_nextSfxChannelGroupF00000 = cursors.group_f00000;
}

// The reports the original's native checks made (a warning for an effect
// with no bank to play from, an error otherwise).
void Report(SfxRefusal refusal, const SfxRequest& request) {
  switch (refusal) {
    case SfxRefusal::kNoSelectedBank:
      VS_LOG_WARN(kLogChannel, "private SFX id={} skipped: no selected bank",
                  request.effect_id);
      return;
    case SfxRefusal::kSelectedBankOutOfRange:
      VS_LOG_ERROR(kLogChannel, "invalid selected SFX bank={}", request.bank);
      return;
    case SfxRefusal::kBankUnavailable:
      VS_LOG_ERROR(kLogChannel, "unavailable SFX bank={} or invalid id={}",
                   request.bank, request.effect_id);
      return;
    case SfxRefusal::kNoRoomBank:
      VS_LOG_WARN(kLogChannel, "room SFX id={} skipped: no room bank",
                  request.effect_id);
      return;
    case SfxRefusal::kRoomEffectNegative:
      VS_LOG_ERROR(kLogChannel, "invalid room SFX id={}", request.effect_id);
      return;
    case SfxRefusal::kUnresolvableBank:
      VS_LOG_ERROR(kLogChannel,
                   "cannot resolve banked SFX group=0x{:X} id={} bank={}",
                   request.group, request.effect_id, request.bank);
      return;
    case SfxRefusal::kProgramUnavailable:
      VS_LOG_ERROR(kLogChannel, "unavailable bank or invalid SFX id={} bank={}",
                   request.effect_id, request.bank);
      return;
    case SfxRefusal::kProgramOffsetInvalid:
      VS_LOG_ERROR(kLogChannel, "invalid banked SFX offset={} id={}",
                   request.offset, request.effect_id);
      return;
    case SfxRefusal::kBankUnreadable:
      VS_LOG_ERROR(kLogChannel,
                   "SFX bank entry group=0x{:X} id={} lies outside readable "
                   "memory",
                   request.group, request.effect_id);
      return;
  }
}

class LiveSfxHost final : public SfxHost {
 public:
  void PlaySoundEffect(std::int32_t effect_id, std::int32_t effect_tag,
                       const SfxMix& mix) override {
    Playback::LivePlaySoundEffect(static_cast<std::uint32_t>(effect_id),
                                  effect_tag, mix.pan, mix.volume);
  }

  // The program's result (-1 when it is not AKAO) is unused, as the
  // original's is.
  void PlaySoundEffectProgram(DataRef program, std::int32_t effect_tag,
                              const SfxMix& mix) override {
    Playback::LivePlaySoundEffectBlob(Pointer(program), effect_tag, mix.pan,
                                      mix.volume);
  }

  EffectSelector SoundEffectIdForTag(std::int32_t effect_tag) override {
    return static_cast<EffectSelector>(
        Playback::LiveSoundEffectIdForTag(effect_tag));
  }

  bool SharedEffectUsesTags(std::int32_t effect_id) override {
    return Playback::LiveSharedEffectUsesTags(effect_id) != 0;
  }

  bool ProgramUsesTags(DataRef program) override {
    return Playback::LiveProgramUsesTags(Pointer(program)) != 0;
  }

  void StopSoundEffects(EffectSelector selector,
                        std::int32_t effect_tag_mask) override {
    Playback::LiveStopSoundEffects(static_cast<std::intptr_t>(selector),
                                   effect_tag_mask);
  }

  void StopSceneSoundEffects() override {
    Control::LiveStopSceneSoundEffects();
  }

  void FadeSoundEffect(SfxFade fade, EffectSelector selector,
                       std::int32_t effect_tag_mask,
                       const SfxFadeSpec& spec) override {
    const auto effect = static_cast<std::intptr_t>(selector);
    switch (fade) {
      case SfxFade::kVolume:
        Control::LiveFadeSoundEffectVolume(effect, effect_tag_mask, spec.ticks,
                                           spec.target);
        return;
      case SfxFade::kPitch:
        Control::LiveFadeSoundEffectPitch(effect, effect_tag_mask, spec.ticks,
                                          spec.target);
        return;
      case SfxFade::kPan:
        Control::LiveFadeSoundEffectPan(effect, effect_tag_mask, spec.ticks,
                                        spec.target);
        return;
    }
  }

  std::span<const std::byte> BankBytes(DataRef bank) override {
    auto* const start = static_cast<char*>(Pointer(bank));
    if (start == nullptr) return {};
    const std::optional<Seq::LiveProgramMemory> region =
        Seq::LiveProgramRegion(start);
    if (!region) return {};
    const Seq::LiveProgramMemory from = region->From(start);
    return {reinterpret_cast<const std::byte*>(from.base), from.size};
  }

  // The original loads vs_scratch_t::viewMatrix at 0x1F800014; the native
  // scratch keeps the field but not the PS1 offset.
  SfxProjection Project(const SfxPoint& point) override {
    auto* const scratch = static_cast<Render_FrameScratch*>(Core_ScratchBase());
    Core_Gte_SetRotMatrix(&scratch->viewMatrix);
    Core_Gte_SetTransMatrix(&scratch->viewMatrix);
    SVECTOR vector{.vx = point.x, .vy = point.y, .vz = point.z, .pad = 0};
    Core_GteWord screen_xy = 0;
    Core_GteWord interpolation = 0;
    Core_GteWord flag = 0;
    const Core_GteWord depth =
        Core_Gte_RotTransPers(&vector, &screen_xy, &interpolation, &flag);
    return {.screen_xy = static_cast<std::int32_t>(screen_xy),
            .depth = static_cast<std::int32_t>(depth)};
  }

  void Diagnose(std::uint32_t code) override {
    Core::IgnoreDiagnostic(static_cast<int>(code), 0);
  }

  void Refuse(SfxRefusal refusal, const SfxRequest& request) override {
    Report(refusal, request);
  }
};

// Runs a request that advances the tag cursors over the live state, then
// stores the cursors back.
template <typename Request>
void RunPlay(const Request& request) {
  LiveSfxHost host;
  SfxState state = LoadState();
  request(state, host);
  StoreCursors(state.cursors);
}

BankSfx BankEffect(const void* bank, std::int32_t index) {
  return {.bank = Reference(bank), .index = index};
}
}  // namespace

// decomp: SLUS_010.40 vs_main_playSfx (src/SLUS_010.40/main.c:9816)
void LivePlaySfx(int group, int effect_id, int pan, int volume) {
  RunPlay([&](SfxState& state, SfxHost& host) {
    PlaySfx(state, host, group, effect_id, {.pan = pan, .volume = volume});
  });
}

// decomp: SLUS_010.40 vs_main_playSfxDefault (src/SLUS_010.40/main.c:9877)
void LivePlaySfxCentered(int group, int effect_id) {
  RunPlay([&](SfxState& state, SfxHost& host) {
    PlaySfxCentered(state, host, group, effect_id);
  });
}

// decomp: SLUS_010.40 vs_main_computeSfxPan (src/SLUS_010.40/main.c:9879)
int LiveSfxPanAndVolume(int screen_xy, int depth) {
  const SfxMix mix = SfxPanAndVolume({.screen_xy = screen_xy, .depth = depth});
  return static_cast<int>((static_cast<std::uint32_t>(mix.pan) << kPanShift) +
                          static_cast<std::uint32_t>(mix.volume));
}

// decomp: SLUS_010.40 vs_main_panSfx (src/SLUS_010.40/main.c:9937)
void LivePlayWorldSfx(int group, int effect_id, SVECTOR* point) {
  if (point == nullptr) {
    VS_LOG_ERROR(kLogChannel, "world SFX group=0x{:X} id={} has no position",
                 group, effect_id);
    return;
  }
  const SfxPoint world{.x = point->vx, .y = point->vy, .z = point->vz};
  RunPlay([&](SfxState& state, SfxHost& host) {
    PlayWorldSfx(state, host, group, effect_id, world);
  });
}

// decomp: SLUS_010.40 func_80045B28 (src/SLUS_010.40/main.c:9950)
std::intptr_t LiveResolveSfxProgram(int group, int effect_id) {
  LiveSfxHost host;
  const std::optional<DataRef> program =
      ResolveSfxProgram(LoadState(), host, group, effect_id);
  if (!program) return kRefusedProgram;
  return static_cast<std::intptr_t>(static_cast<std::uintptr_t>(*program));
}

// decomp: SLUS_010.40 func_80045BFC (src/SLUS_010.40/main.c:9975)
void LiveFadeSfxVolume(int group, int effect_id, int target_volume,
                       int fade_ticks) {
  LiveSfxHost host;
  FadeSfxVolume(LoadState(), host, {.group = group, .effect_id = effect_id},
                {.target = target_volume, .ticks = fade_ticks});
}

// decomp: SLUS_010.40 func_80045C74 (src/SLUS_010.40/main.c:9994)
void LiveFadeSfxPitch(int group, int effect_id, int target_pitch_scale,
                      int fade_ticks) {
  LiveSfxHost host;
  FadeSfxPitch(LoadState(), host, {.group = group, .effect_id = effect_id},
               {.target = target_pitch_scale, .ticks = fade_ticks});
}

// decomp: SLUS_010.40 func_80045CEC (src/SLUS_010.40/main.c:10012)
void LiveFadeSfxPan(int group, int effect_id, int target_pan, int fade_ticks) {
  LiveSfxHost host;
  FadeSfxPan(LoadState(), host, {.group = group, .effect_id = effect_id},
             {.target = target_pan, .ticks = fade_ticks});
}

// decomp: SLUS_010.40 func_80045D64 (src/SLUS_010.40/main.c:10028)
void LiveStopSfx(int group, int effect_id) {
  LiveSfxHost host;
  StopSfx(LoadState(), host, {.group = group, .effect_id = effect_id});
}

// decomp: SLUS_010.40 func_80045DC0 (src/SLUS_010.40/main.c:10043)
void LiveStopAllSfxPlayback() {
  LiveSfxHost host;
  StopAllSfxPlayback(host);
}

// decomp: SLUS_010.40 func_800461CC (src/SLUS_010.40/main.c:10178)
void LivePlayBankSfx(int group, const void* bank, unsigned index, int pan,
                     int volume) {
  RunPlay([&](SfxState& state, SfxHost& host) {
    PlayBankSfx(state, host, group,
                BankEffect(bank, static_cast<std::int32_t>(index)),
                {.pan = pan, .volume = volume});
  });
}

// decomp: SLUS_010.40 func_800462E8 (src/SLUS_010.40/main.c:10215)
void LivePlayBankSfxCentered(int group, const void* bank, unsigned index) {
  RunPlay([&](SfxState& state, SfxHost& host) {
    PlayBankSfxCentered(state, host, group,
                        BankEffect(bank, static_cast<std::int32_t>(index)));
  });
}

// decomp: SLUS_010.40 func_8004644C (src/SLUS_010.40/main.c:10285)
void LiveStopBankSfx(int group, const void* bank, int index) {
  LiveSfxHost host;
  StopBankSfx(host, group, BankEffect(bank, index));
}

// decomp: SLUS_010.40 func_80046494 (src/SLUS_010.40/main.c:10299)
void LiveFadeBankSfxVolume(int group, const void* bank, int index,
                           int target_volume, int fade_ticks) {
  LiveSfxHost host;
  FadeBankSfxVolume(host, group, BankEffect(bank, index),
                    {.target = target_volume, .ticks = fade_ticks});
}
}  // namespace VS::Audio
