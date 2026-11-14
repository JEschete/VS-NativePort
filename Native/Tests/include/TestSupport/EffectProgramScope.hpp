#pragma once

#include <cstddef>
#include <span>

#include "Effects/PackageExtents.hpp"

namespace VS::test_support {
// Records a test program as a bound block-8 payload while it lives, so the
// effect script loop reads it the way it reads a published package's
// programs.
class EffectProgramScope {
 public:
  explicit EffectProgramScope(std::span<const std::byte> program)
      : program_(program.data()) {
    Effects::RecordLiveEffectPrograms(program);
  }
  EffectProgramScope(const EffectProgramScope&) = delete;
  EffectProgramScope& operator=(const EffectProgramScope&) = delete;
  EffectProgramScope(EffectProgramScope&&) = delete;
  EffectProgramScope& operator=(EffectProgramScope&&) = delete;
  ~EffectProgramScope() { Effects::ForgetLiveEffectPrograms(program_); }

 private:
  const void* program_;
};
}  // namespace VS::test_support
