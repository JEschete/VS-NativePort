#include "Field/GimmickResources.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <utility>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {
// decomp: BATTLE.PRG func_80099900 (30DB0.c:273),24MIPS LB/LBU/IDmask/store.
std::optional<std::size_t> FindSharedGimmickModel(
    std::span<const GimmickResourceSlot> slots, std::int8_t excluded_slot,
    std::uint8_t requested_kind) {
  constexpr std::uint8_t kObjectIdMask = 7;
  const auto count = std::min(slots.size(), kGimmickResourceSlots);
  for (std::size_t index = 0; index < count; ++index) {
    const auto& slot = slots[index];
    if (!std::cmp_equal(index, excluded_slot) && slot.present &&
        (slot.object_id & kObjectIdMask) == requested_kind) {
      return index;
    }
  }
  return std::nullopt;
}

namespace {
constexpr std::uint8_t kGimmickIdMask = 7;
constexpr std::uint8_t kLastGimmickUvKind = 5;
constexpr std::uint8_t kFirstStackedGimmickKind = 5;
constexpr std::uint8_t kNoGimmickCarrier = 255;
constexpr std::int16_t kGimmickScale = 64;
constexpr std::int32_t kGimmickTileUnit = 128;
constexpr std::int32_t kGimmickTileCenter = 64;
constexpr std::int32_t kNeutralGimmickColor = 0x808080;
constexpr std::uint32_t kGimmickPlacementMask = 0xFFFFFF;
constexpr unsigned kGimmickTileZShift = 16;

bool InitializeGimmickPlacement(Content::ActorAssetRequest& request,
                                std::size_t slot, GimmickCreationState& state,
                                GimmickCreationHost& host) {
  state.carrier = kNoGimmickCarrier;
  state.parent = kNoGimmickCarrier;
  if (state.object_id == 1) state.subtype = request.texture_slot;
  state.base_color = kNeutralGimmickColor;
  state.scale_x = kGimmickScale;
  state.scale_y = kGimmickScale;
  state.scale_z = kGimmickScale;
  state.mode = 0;
  state.pulse_kind = 0;
  state.skipped = true;
  state.visible = false;
  state.tile_word = request.placement & kGimmickPlacementMask;
  state.x = Core::Math::WrapSigned16(
      static_cast<std::uint8_t>(state.tile_word) * kGimmickTileUnit +
      kGimmickTileCenter);
  state.z = Core::Math::WrapSigned16(
      static_cast<std::uint8_t>(state.tile_word >> kGimmickTileZShift) *
          kGimmickTileUnit +
      kGimmickTileCenter);
  if (!host.StoreState(slot, state)) return false;
  const auto height = host.SupportHeight(state);
  if (!height) return false;
  state.y = Core::Math::WrapSigned16(Core::Math::WrappingAdd(
      *height, -static_cast<std::int32_t>(request.variant) * kGimmickTileUnit));
  return host.StoreState(slot, state) && host.Orient(slot, request.material) &&
         host.AddOccupancy(slot);
}

bool AdjustGimmickStack(std::size_t slot, GimmickCreationState& state,
                        GimmickCreationHost& host) {
  if (state.object_id < kFirstStackedGimmickKind) return true;
  for (std::size_t index = 0; index < slot; ++index) {
    const auto candidate = host.ReadState(index);
    if (!candidate || candidate->tile_word != state.tile_word ||
        candidate->y != static_cast<std::int32_t>(state.y) + kGimmickTileUnit ||
        candidate->object_id < kFirstStackedGimmickKind) {
      continue;
    }
    if (candidate->object_id != state.object_id) {
      state.interaction = 1;
      return host.StoreState(slot, state);
    }
    state.interaction = 2;
    state.y = Core::Math::WrapSigned16(static_cast<std::int32_t>(state.y) -
                                       kGimmickTileUnit);
    return host.StoreState(slot, state) && host.AddOccupancy(slot);
  }
  return true;
}
}  // namespace

// decomp: BATTLE.PRG func_8009998C (30DB0.c:290),248MIPS publish/init/stack.
int CreateGimmickInstance(Content::ActorAssetRequest& request,
                          GimmickCreationHost& host) {
  if (request.target_slot < 0) return Content::kAssetRefused;
  const auto slot =
      static_cast<std::size_t>(static_cast<std::uint8_t>(request.target_slot));
  if (slot >= kGimmickResourceSlots) {
    return Content::kAssetRefused;
  }
  if (host.Occupied(slot)) return Content::kAssetBusy;
  if (!host.PublishDestination(slot) || !host.ClearPrefix(slot)) {
    return Content::kAssetRefused;
  }
  const auto shared = FindSharedGimmickModel(host.Slots(), request.target_slot,
                                             request.sub_kind);
  if (shared) {
    request.data = Content::SharedAssetAddress(static_cast<int>(*shared));
    if (!host.CopySharedPrefix(slot, *shared)) return Content::kAssetRefused;
  } else if (!host.CreateModel(
                 slot, request.sub_kind,
                 std::min(request.sub_kind, kLastGimmickUvKind))) {
    return Content::kAssetRefused;
  }
  auto state = host.ReadState(slot);
  if (!state) return Content::kAssetRefused;
  state->slot = static_cast<std::uint8_t>(request.target_slot);
  state->object_id =
      static_cast<std::uint8_t>(request.sub_kind & kGimmickIdMask);
  if (!host.StoreState(slot, *state) ||
      !host.CopyPalette(slot, state->object_id) || !host.UploadPalette(slot) ||
      !InitializeGimmickPlacement(request, slot, *state, host)) {
    return Content::kAssetRefused;
  }
  state = host.ReadState(slot);
  if (!state) return Content::kAssetRefused;
  state->interaction = 0;
  if (!host.StoreState(slot, *state) ||
      !AdjustGimmickStack(slot, *state, host)) {
    return Content::kAssetRefused;
  }
  return Content::kAssetDone;
}

// decomp: BATTLE.PRG func_80099D6C (30DB0.c:399),68MIPS
// shared/free/remove/clear.
int ReleaseGimmickInstance(std::int32_t slot,
                           std::span<const GimmickResourceSlot> slots,
                           GimmickReleaseHost& host) {
  constexpr int kRefused = -1;
  if (slot < 0 || static_cast<std::size_t>(slot) >= kGimmickResourceSlots ||
      static_cast<std::size_t>(slot) >= slots.size()) {
    return kRefused;
  }
  const auto index = static_cast<std::size_t>(slot);
  const auto& released = slots[index];
  if (!released.present || released.bone_count == 0) return kRefused;
  constexpr std::uint8_t kObjectIdMask = 7;
  if (!FindSharedGimmickModel(
          slots, static_cast<std::int8_t>(slot),
          static_cast<std::uint8_t>(released.object_id & kObjectIdMask)) &&
      !host.FreeModel(index)) {
    return kRefused;
  }
  if (!host.RemoveOccupancy(index) || !host.ClearSlot(index)) return kRefused;
  return 0;
}

// decomp: BATTLE.PRG func_80099960 (30DB0.c:288),11MIPS copies0xE0bytes.
bool SetGimmickPaletteTable(GimmickPaletteTable& table,
                            std::span<const std::byte> palette_bytes) {
  if (palette_bytes.size() < sizeof(table)) return false;
  GimmickPaletteTable copy{};
  std::memcpy(copy.data(), palette_bytes.data(), sizeof(copy));
  table = copy;
  return true;
}
}  // namespace VS::Field
