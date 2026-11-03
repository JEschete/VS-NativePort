// Hand-maintained (tools/port/owned.csv).
// Live binding for depth cueing (DepthCue.cpp): the GTE cue registers and far
// colour.
#include "Render/DepthCueLive.hpp"

#include <cstdint>

namespace VS::Render {
namespace {
class LiveDepthCueRegisters final : public DepthCueRegisters {
 public:
  void SetDepthCue(const DepthCue& cue) override {
    Core_Gte_SetDepthCue(cue.coefficient, cue.offset);
  }
};

FarColor LiveFarColor() {
  Core_GteWord red = 0;
  Core_GteWord green = 0;
  Core_GteWord blue = 0;
  Core_Gte_GetFarColor(&red, &green, &blue);
  return {static_cast<std::int32_t>(red), static_cast<std::int32_t>(green),
          static_cast<std::int32_t>(blue)};
}
}  // namespace

void ApplyLiveDepthCueColor(const CVECTOR* color, Core_GteWord cue,
                            CVECTOR* output) {
  if (color == nullptr || output == nullptr) {
    return;
  }
  // Read the whole colour before writing: callers may pass one record.
  const ColorCode blended = ApplyDepthCueColor(
      {.red = color->r, .green = color->g, .blue = color->b, .code = color->cd},
      static_cast<std::int32_t>(cue), LiveFarColor());
  output->r = blended.red;
  output->g = blended.green;
  output->b = blended.blue;
  output->cd = blended.code;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): SetFogNear's shape.
void SetLiveFogNearDistance(Core_GteWord near_distance,
                            Core_GteWord projection_distance) {
  LiveDepthCueRegisters registers;
  SetFogNearDistance(registers, static_cast<std::int32_t>(near_distance),
                     static_cast<std::int32_t>(projection_distance));
}
}  // namespace VS::Render
