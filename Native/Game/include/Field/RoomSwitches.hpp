#pragma once

#include <cstdint>
#include <optional>

namespace VS::Field {

struct RoomSwitchState {
  std::uint16_t switch_value = 0;
  std::uint8_t marker_fade = 0;
  std::uint8_t activation = 0;
  std::int32_t effect_kind = 0;
  std::int32_t tile_x = 0;
  std::int32_t tile_z = 0;
  std::int32_t floor_layer = 0;
};

class RoomSwitchHost {
 public:
  virtual ~RoomSwitchHost() = default;
  virtual std::optional<RoomSwitchState> State() const = 0;
  virtual void PlaySound(std::int32_t sound) = 0;
  virtual void StartEffect(std::int32_t kind) = 0;
  virtual void StoreSwitch(std::uint16_t value, std::uint8_t marker_fade) = 0;
  virtual void SetCellPassability(const RoomSwitchState& state,
                                  std::uint8_t passability) = 0;
};

bool ToggleRoomSwitch(RoomSwitchHost& host);

class RoomSwitchStartHost {
 public:
  virtual ~RoomSwitchStartHost() = default;
  virtual std::optional<RoomSwitchState> State() const = 0;
  virtual bool StoreStartState(std::uint8_t marker,
                               std::uint8_t activation) = 0;
  virtual bool QueueToggle() = 0;
  virtual bool ToggleImmediately() = 0;
};

bool BeginSwitchToggle(RoomSwitchStartHost& host);

}  // namespace VS::Field
