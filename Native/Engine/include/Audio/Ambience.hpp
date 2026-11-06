#pragma once
// Hand-maintained (tools/port/owned.csv).

// The battle overlay's room ambience duck (BATTLE.PRG 146C.c): the room's
// effect sound and the weather's ambient sound are ducked together while
// a menu or reward screen is up and restored together after it.
namespace VS::Audio {
class AmbienceHost {
 public:
  AmbienceHost() = default;
  AmbienceHost(const AmbienceHost&) = delete;
  AmbienceHost& operator=(const AmbienceHost&) = delete;
  AmbienceHost(AmbienceHost&&) = delete;
  AmbienceHost& operator=(AmbienceHost&&) = delete;
  virtual ~AmbienceHost() = default;

  // func_800938AC: the room's effect sound.
  virtual void DuckRoomEffectSound(bool ducked) = 0;
  // func_800918E8: the weather's ambient sound.
  virtual void DuckWeatherAmbientSound(bool ducked) = 0;
};

// func_8008E480: ducks or restores both, the room's effects first.
void SetRoomAmbienceDucked(AmbienceHost& host, bool ducked);
}  // namespace VS::Audio
