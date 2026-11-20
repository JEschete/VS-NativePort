// Hand-maintained (tools/port/owned.csv).
// Live binding for depth cueing (DepthCue.cpp): the GTE cue registers and far
// colour.
#include "Render/DepthCueLive.hpp"

#include <cstdint>

using VS::Core::Gte::CVECTOR;
using VS::Core::Gte::GetFarColor;

namespace VS::Render {
namespace {
class LiveDepthCueRegisters final : public DepthCueRegisters {
 public:
  void SetDepthCue(const DepthCue& cue) override {
    VS::Core::Gte::SetDepthCue(cue.coefficient, cue.offset);
  }
};

FarColor LiveFarColor() {
  VS::Core::Gte::Word red = 0;
  VS::Core::Gte::Word green = 0;
  VS::Core::Gte::Word blue = 0;
  GetFarColor(&red, &green, &blue);
  return {static_cast<std::int32_t>(red), static_cast<std::int32_t>(green),
          static_cast<std::int32_t>(blue)};
}
}  // namespace

void ApplyLiveDepthCueColor(const CVECTOR* color, VS::Core::Gte::Word cue,
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
void SetLiveFogNearDistance(VS::Core::Gte::Word near_distance,
                            VS::Core::Gte::Word projection_distance) {
  LiveDepthCueRegisters registers;
  SetFogNearDistance(registers, static_cast<std::int32_t>(near_distance),
                     static_cast<std::int32_t>(projection_distance));
}
}  // namespace VS::Render
