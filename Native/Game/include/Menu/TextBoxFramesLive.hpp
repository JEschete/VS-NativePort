#pragma once

#include <cstdint>

namespace VS::Menu {

void DrawLiveInformationBoxHeader(std::int32_t y, std::uint32_t kind);
void DrawLiveInformationBoxBackdrop(std::int32_t index);
[[nodiscard]] std::int32_t FlipLiveTextBoxShapePoint(
    std::int32_t packed_shape_point, std::int32_t flip_axes);
void DrawLiveTextBoxFrame(std::int32_t index);

}  // namespace VS::Menu
