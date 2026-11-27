#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Content/ActorAssets.hpp"

namespace VS::Field {
inline constexpr std::size_t kGimmickResourceSlots = 16;
inline constexpr std::size_t kGimmickPaletteSets = 7;
inline constexpr std::size_t kGimmickPaletteColors = 16;
inline constexpr std::size_t kGimmickPaletteWordCount = 112;
inline constexpr std::size_t kGimmickPaletteBytes = 0xE0;

class GimmickModelResource final {
 public:
  GimmickModelResource(std::int32_t flags, std::int32_t word4,
                       std::vector<std::byte> geometry)
      : flags_(flags), word4_(word4), geometry_(std::move(geometry)) {}

  [[nodiscard]] std::int32_t InstanceFlags() const { return flags_; }
  [[nodiscard]] std::int32_t InstanceWord4() const { return word4_; }
  [[nodiscard]] std::span<const std::byte> Geometry() const {
    return geometry_;
  }

 private:
  std::int32_t flags_;
  std::int32_t word4_;
  std::vector<std::byte> geometry_;
};

using GimmickModelResourceRef = std::shared_ptr<const GimmickModelResource>;

struct GimmickResourceSlot {
  bool present = false;
  std::uint8_t object_id = 0;
  std::uint8_t bone_count = 0;
};

class GimmickReleaseHost {
 public:
  virtual ~GimmickReleaseHost() = default;
  virtual bool FreeModel(std::size_t slot) = 0;
  virtual bool RemoveOccupancy(std::size_t slot) = 0;
  virtual bool ClearSlot(std::size_t slot) = 0;
};

int ReleaseGimmickInstance(std::int32_t slot,
                           std::span<const GimmickResourceSlot> slots,
                           GimmickReleaseHost& host);

struct GimmickCreationState {
  std::uint8_t slot = 0;
  std::uint8_t object_id = 0;
  std::uint8_t subtype = 0;
  std::uint8_t carrier = 0;
  std::uint8_t parent = 0;
  std::uint8_t interaction = 0;
  std::uint8_t pulse_kind = 0;
  std::int32_t base_color = 0;
  std::int16_t scale_x = 0;
  std::int16_t scale_y = 0;
  std::int16_t scale_z = 0;
  std::int16_t mode = 0;
  bool skipped = false;
  bool visible = false;
  std::uint32_t tile_word = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
};

class GimmickCreationHost {
 public:
  virtual ~GimmickCreationHost() = default;
  virtual bool Occupied(std::size_t slot) const = 0;
  virtual bool PublishDestination(std::size_t slot) = 0;
  virtual bool ClearPrefix(std::size_t slot) = 0;
  virtual void Discard(std::size_t slot) = 0;
  virtual bool CreateModel(std::size_t slot, std::uint8_t kind,
                           std::uint8_t uv_kind) = 0;
  virtual std::optional<GimmickCreationState> ReadState(std::size_t slot) = 0;
  virtual bool StoreState(std::size_t slot,
                          const GimmickCreationState& state) = 0;
  virtual bool CopyPalette(std::size_t slot, std::uint8_t kind) = 0;
  virtual bool UploadPalette(std::size_t slot) = 0;
  virtual std::optional<std::int32_t> SupportHeight(
      const GimmickCreationState& state) = 0;
  virtual bool Orient(std::size_t slot, std::uint8_t material) = 0;
  virtual bool AddOccupancy(std::size_t slot) = 0;
};

int CreateGimmickInstance(const Content::ActorAssetRequest& request,
                          GimmickCreationHost& host);

using GimmickPaletteTable = std::array<std::uint16_t, kGimmickPaletteWordCount>;
static_assert(sizeof(GimmickPaletteTable) == kGimmickPaletteBytes);

std::optional<std::size_t> FindSharedGimmickModel(
    std::span<const GimmickResourceSlot> slots, std::int8_t excluded_slot,
    std::uint8_t requested_kind);
bool SetGimmickPaletteTable(GimmickPaletteTable& table,
                            std::span<const std::byte> palette_bytes);
}  // namespace VS::Field
