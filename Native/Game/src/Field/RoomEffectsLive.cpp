// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomEffectsLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Audio/SfxLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/RoomEffects.hpp"
#include "Field/RoomEffectsCore.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

// The polygon bytes the typed room effects rewrite in place, as the legacy
// views and renderer read them.
static_assert(sizeof(RECT) == sizeof(VS::Field::RoomImageRect));
static_assert(offsetof(RECT, x) == offsetof(VS::Field::RoomImageRect, x));
static_assert(offsetof(RECT, y) == offsetof(VS::Field::RoomImageRect, y));
static_assert(offsetof(RECT, w) == offsetof(VS::Field::RoomImageRect, w));
static_assert(offsetof(RECT, h) == offsetof(VS::Field::RoomImageRect, h));
static_assert(offsetof(Field_MpdPolygonUvView, type) ==
              VS::Field::room_effects::kPolygonTypeOffset);
static_assert(offsetof(Field_MpdPolygonUvView, u0) ==
              VS::Field::room_effects::kPolygonUOffsets[0]);
static_assert(offsetof(Field_MpdPolygonUvView, u1) ==
              VS::Field::room_effects::kPolygonUOffsets[1]);
static_assert(offsetof(Field_MpdPolygonUvView, u2) ==
              VS::Field::room_effects::kPolygonUOffsets[2]);
static_assert(offsetof(Field_MpdPolygonUvView, u3) ==
              VS::Field::room_effects::kPolygonUOffsets.back());
static_assert(offsetof(Field_MpdPolygonUvView, v0) ==
              VS::Field::room_effects::kPolygonVOffsets[0]);
static_assert(offsetof(Field_MpdPolygonUvView, v1) ==
              VS::Field::room_effects::kPolygonVOffsets[1]);
static_assert(offsetof(Field_MpdPolygonUvView, v2) ==
              VS::Field::room_effects::kPolygonVOffsets[2]);
static_assert(offsetof(Field_MpdPolygonUvView, v3) ==
              VS::Field::room_effects::kPolygonVOffsets.back());
static_assert(sizeof(Field_MpdPolygonUvView) ==
              VS::Field::room_effects::kQuadUvExtent);
static_assert(sizeof(Field_MpdTriangleColorView) ==
              VS::Render::kRoomTriangleRecordBytes);
static_assert(sizeof(Field_MpdQuadColorView) ==
              VS::Render::kRoomQuadRecordBytes);
static_assert(offsetof(Field_MpdTriangleColorView, red1) ==
              VS::Field::room_effects::kPolygonRedOffsets[0]);
static_assert(offsetof(Field_MpdTriangleColorView, green1) ==
              VS::Field::room_effects::kPolygonRedOffsets[0] + 1);
static_assert(offsetof(Field_MpdTriangleColorView, red2) ==
              VS::Field::room_effects::kPolygonRedOffsets[1]);
static_assert(offsetof(Field_MpdTriangleColorView, green2) ==
              VS::Field::room_effects::kPolygonRedOffsets[1] + 1);
static_assert(offsetof(Field_MpdTriangleColorView, red3) ==
              VS::Field::room_effects::kPolygonRedOffsets[2]);
static_assert(offsetof(Field_MpdTriangleColorView, green3) ==
              VS::Field::room_effects::kPolygonRedOffsets[2] + 1);
static_assert(offsetof(Field_MpdQuadColorView, red1) ==
              VS::Field::room_effects::kPolygonRedOffsets[0]);
static_assert(offsetof(Field_MpdQuadColorView, red2) ==
              VS::Field::room_effects::kPolygonRedOffsets[1]);
static_assert(offsetof(Field_MpdQuadColorView, red3) ==
              VS::Field::room_effects::kPolygonRedOffsets[2]);
static_assert(offsetof(Field_MpdQuadColorView, red4) ==
              VS::Field::room_effects::kPolygonRedOffsets.back());
static_assert(offsetof(Field_MpdQuadColorView, green4) ==
              VS::Field::room_effects::kPolygonRedOffsets.back() + 1);
static_assert(sizeof(Field_MpdSectionE) ==
              VS::Field::room_effects::kEntryPointHeaderBytes);

namespace VS::Field {
namespace {
constexpr std::string_view kChannel = "field.effects";
// The sound group the room bank's sounds play in.
constexpr int kRoomSoundGroup = 0x200;

// A section the room loader filled: empty when it is missing or its length
// is not positive, as the original's (int)length > 0 tests.
template <class Byte>
std::span<Byte> Section(void* data, std::uint32_t length) {
  if (data == nullptr || static_cast<std::int32_t>(length) <= 0) {
    return {};
  }
  return {static_cast<Byte*>(data), length};
}

// Section E code bound to a native routine by its FNV-1a hash and length.
struct EntryPointBinding {
  std::uint32_t hash = 0;
  std::size_t length = 0;
  bool (*run)() = nullptr;
};
// No shipped room's section E holds code (every MPD's section is empty), so
// nothing is bound and every request is refused.
constexpr std::array<EntryPointBinding, 0> kEntryPointBindings{};

std::uint32_t Fnv1a(std::span<const std::byte> bytes) {
  constexpr std::uint32_t kOffsetBasis = 2166136261U;
  constexpr std::uint32_t kPrime = 16777619U;
  std::uint32_t hash = kOffsetBasis;
  for (const std::byte value : bytes) {
    hash = (hash ^ static_cast<std::uint32_t>(value)) * kPrime;
  }
  return hash;
}

class LiveHost final : public RoomEffectHost {
 public:
  std::span<std::byte> Geometry() override {
    return Section<std::byte>(Field_roomData.geometry,
                              Field_roomData.header.geometryLength);
  }
  std::span<std::byte> TextureEffects() override {
    return Section<std::byte>(Field_roomData.textureEffects,
                              Field_roomData.header.textureEffectsLength);
  }
  std::span<const std::byte> EffectScripts() override {
    return Section<const std::byte>(Field_roomData.effectScripts,
                                    Field_roomData.header.effectScriptsLength);
  }
  std::span<const std::byte> EntryPointSection() override {
    return Section<const std::byte>(Field_roomData.sectionE,
                                    Field_roomData.header.sectionELength);
  }

  bool HasSoundBank() override { return Field_roomData.soundData != nullptr; }
  void PlayCenteredSound(std::int32_t sound_index) override {
    Audio::LivePlayBankSfxCentered(kRoomSoundGroup, Field_roomData.soundData,
                                   static_cast<unsigned>(sound_index));
  }
  void PlaySound(std::int32_t sound_index, std::int32_t pan,
                 std::int32_t volume) override {
    Audio::LivePlayBankSfx(kRoomSoundGroup, Field_roomData.soundData,
                           static_cast<unsigned>(sound_index), pan, volume);
  }
  void StopSound(std::int32_t sound_index) override {
    Audio::LiveStopBankSfx(kRoomSoundGroup, Field_roomData.soundData,
                           sound_index);
  }
  void FadeSound(std::int32_t sound_index, std::int32_t volume,
                 std::int32_t fade_ticks) override {
    Audio::LiveFadeBankSfxVolume(kRoomSoundGroup, Field_roomData.soundData,
                                 sound_index, volume, fade_ticks);
  }

  void MoveImage(const RoomImageRect& source, std::int32_t x,
                 std::int32_t y) override {
    RECT rect{source.x, source.y, source.w, source.h};
    ::MoveImage(&rect, x, y);
  }

  std::optional<RoomEffectStorageId> ReserveObjectStorage(
      std::size_t bytes) override {
    void* storage = Core::Memory::AllocateLiveFromLowEnd(bytes);
    if (storage == nullptr) {
      return std::nullopt;
    }
    // The id is the block's address: an opaque handle, not an enumerator.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<RoomEffectStorageId>(
        reinterpret_cast<std::uintptr_t>(storage));
  }
  void ReleaseObjectStorage(RoomEffectStorageId storage) override {
    Core::Memory::FreeLiveFromLowEnd(
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(storage)));
  }

  std::uint16_t FloorHeightCode(int world_x, int world_z) override {
    return static_cast<std::uint16_t>(LiveFloorHeightAt(world_x, world_z));
  }
  bool FastForwarding() override {
    return Audio_Seq_unusedStateFlags.opaqueA8 != 0;
  }
  bool RunRoomEntryPoint(std::span<const std::byte> code) override {
    const std::uint32_t hash = Fnv1a(code);
    for (const EntryPointBinding& binding : kEntryPointBindings) {
      if (binding.hash == hash && binding.length == code.size()) {
        return binding.run();
      }
    }
    VS_LOG_WARN(kChannel,
                "Room entry point refused: no binding for its {} bytes "
                "(FNV-1a {:08X})",
                code.size(), hash);
    return false;
  }
};

int Flag(bool value) { return value ? 1 : 0; }
}  // namespace

RoomEffectState& LiveRoomEffectState() {
  static RoomEffectState state;
  return state;
}

int* LiveFindRoomGeometryGroup(int group_index) {
  LiveHost host;
  const auto geometry = host.Geometry();
  const auto group = FindRoomGeometryGroup(geometry, group_index);
  if (!group) {
    return nullptr;
  }
  return reinterpret_cast<int*>(geometry.subspan(*group).data());
}

int LiveShowRoomGeometryGroup(int group_index) {
  LiveHost host;
  return Flag(ShowRoomGeometryGroup(host.Geometry(), group_index));
}

int LiveHideRoomGeometryGroup(int group_index) {
  LiveHost host;
  return Flag(HideRoomGeometryGroup(host.Geometry(), group_index));
}

int LiveSwapRoomGeometryGroupFlags(int first_group_index,
                                   int second_group_index) {
  LiveHost host;
  return Flag(SwapRoomGeometryGroupFlags(host.Geometry(), first_group_index,
                                         second_group_index));
}

int LiveAddRoomGeometryDepthBias(int group_index, int depth_bias) {
  LiveHost host;
  return Flag(
      AddRoomGeometryDepthBias(host.Geometry(), group_index, depth_bias));
}

int LiveDisableRoomGeometryGroup(int group_index) {
  LiveHost host;
  return Flag(DisableRoomGeometryGroup(host.Geometry(), group_index));
}

void LiveSwapRoomGeometryRedGreen(int group_index) {
  LiveHost host;
  SwapRoomGeometryRedGreen(host.Geometry(), group_index);
}

void LiveUpdateRoomEffects() {
  LiveHost host;
  UpdateRoomEffects(LiveRoomEffectState(), host);
}

void LiveInitializeRoomEffects() {
  LiveHost host;
  InitializeRoomEffects(LiveRoomEffectState(), host);
}

int LiveStartRoomEffect(int effect_index) {
  LiveHost host;
  return Flag(StartRoomEffect(LiveRoomEffectState(), host, effect_index));
}

int LiveReleaseRoomEffects() {
  LiveHost host;
  return Flag(ReleaseRoomEffects(LiveRoomEffectState(), host));
}

int LiveToggleRoomEffectUpdates(int effect_index) {
  return Flag(ToggleRoomEffectUpdates(LiveRoomEffectState(), effect_index));
}

int LiveIsRoomEffectActive(int effect_index) {
  return IsRoomEffectActive(LiveRoomEffectState(), effect_index);
}

void LiveFinishRoomDoorRotation(int effect_index) {
  LiveHost host;
  FinishRoomDoorRotation(LiveRoomEffectState(), host, effect_index);
}

void LiveFinishRoomTiltRotation(int effect_index) {
  LiveHost host;
  FinishRoomTiltRotation(LiveRoomEffectState(), host, effect_index);
}

void LiveDuckRoomEffectSound(int duck_enabled) {
  LiveHost host;
  DuckRoomEffectSound(LiveRoomEffectState(), host, duck_enabled != 0);
}

void LiveRestoreRoomEffectSoundVolume() {
  LiveHost host;
  RestoreRoomEffectSoundVolume(LiveRoomEffectState(), host);
}

void LiveRestartRoomEffectSound() {
  LiveHost host;
  RestartRoomEffectSound(LiveRoomEffectState(), host);
}

int LiveCallRoomEntryPoint() {
  LiveHost host;
  return Flag(CallRoomEntryPoint(host));
}
}  // namespace VS::Field
