// Hand-maintained (tools/port/owned.csv).
// Live binding for the script flags (State/ScriptFlags.hpp): the flag record
// and the scene trace.
#include "State/ScriptFlagsLive.hpp"

#include <cstdint>
#include <span>

#include "State/ScriptFlags.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Core/Diagnostics.h"
#include "Diagnostics/SceneTrace.h"
#include "State/StateFlags.h"
}

namespace VS::State {
namespace {
static_assert(sizeof Audio_Seq_unusedStateFlags == kScriptFlagCount);

class SceneTraceObserver final : public ScriptFlagObserver {
 public:
  void FlagWritten(const ScriptFlagWrite& write) override {
    Diagnostics_RecordScriptFlagWrite(write.id, write.before, write.after);
  }
};

SceneTraceObserver& SceneTrace() noexcept {
  static SceneTraceObserver observer;
  return observer;
}

// The original indexed with the id's signed low halfword.
[[nodiscard]] int FlagIdOf(int id) noexcept {
  return static_cast<std::int16_t>(id);
}

void WarnFlagRefused(const char* operation, int id) {
  Core_Warnf("state", "%s refused: flag %d is outside the %u flag bytes",
             operation, id, static_cast<unsigned>(kScriptFlagCount));
}
}  // namespace

ScriptFlags LiveScriptFlags() noexcept {
  return ScriptFlags(
      std::as_writable_bytes(std::span{&Audio_Seq_unusedStateFlags, 1}),
      &SceneTrace());
}

std::uint8_t LiveScriptFlag(int id) {
  const int flag = FlagIdOf(id);
  const ScriptFlags flags = LiveScriptFlags();
  if (!flags.Contains(flag)) {
    WarnFlagRefused("ScriptFlag", flag);
    return 0;
  }
  return flags.Get(flag);
}

void SetLiveScriptFlag(int id, int value) {
  const int flag = FlagIdOf(id);
  ScriptFlags flags = LiveScriptFlags();
  if (!flags.Set(flag, static_cast<std::uint8_t>(value))) {
    WarnFlagRefused("SetScriptFlag", flag);
  }
}

void ResetLiveZoneScriptFlags(int preserve) {
  LiveScriptFlags().ResetZone(preserve != 0);
}
}  // namespace VS::State
