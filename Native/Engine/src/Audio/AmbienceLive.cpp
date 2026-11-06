// Hand-maintained (tools/port/owned.csv).
#include "Audio/AmbienceLive.hpp"

#include "Audio/Ambience.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/RoomEffectsLive.hpp"

namespace VS::Audio {
namespace {
// Both controls test their flag against zero, so the flag carries as a bool.
class LiveAmbienceHost final : public AmbienceHost {
 public:
  void DuckRoomEffectSound(bool ducked) override {
    VS::Field::LiveDuckRoomEffectSound(ducked ? 1 : 0);
  }

  void DuckWeatherAmbientSound(bool ducked) override {
    VS::Field::LiveDuckWeatherAmbientSound(ducked ? 1 : 0);
  }
};
}  // namespace

// decomp: BATTLE.PRG func_8008E480 (src/BATTLE/BATTLE.PRG/146C.c:17323)
void LiveSetRoomAmbienceDucked(int duck_enabled) {
  LiveAmbienceHost host;
  SetRoomAmbienceDucked(host, duck_enabled != 0);
}
}  // namespace VS::Audio
