#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "Menu/TextBoxState.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Menu {

class InformationBoxWorld {
 public:
  virtual ~InformationBoxWorld() = default;
  [[nodiscard]] virtual TextBox ReadBox() const = 0;
  [[nodiscard]] virtual bool AlternateFramebuffer() const = 0;
  virtual bool DrawHeader(std::size_t kind, std::int32_t x, std::int32_t y) = 0;
  virtual bool DrawFramebufferRow(std::int32_t x, std::int32_t y,
                                  std::int32_t width, std::int32_t source_x,
                                  std::uint8_t brightness) = 0;
  virtual bool DrawBlackFill(std::int32_t x, std::int32_t y, std::int32_t width,
                             std::int32_t height) = 0;
};

[[nodiscard]] bool DrawInformationBoxHeader(InformationBoxWorld& world,
                                            std::int32_t y, std::uint32_t kind);
[[nodiscard]] bool DrawInformationBoxBackdrop(InformationBoxWorld& world);
[[nodiscard]] std::uint32_t FlipTextBoxShapePoint(
    std::uint32_t packed_shape_point, std::uint32_t flip_axes);

inline constexpr std::size_t kFrameFaceCorners = 4;
using FrameFace = std::array<std::uint8_t, kFrameFaceCorners>;

enum class FrameDecodeError {
  kTruncated,
  kVertexCount,
  kOutline,
  kFace,
  kSelectedPart,
  kCapacity
};

struct FramePart {
  std::uint8_t start_edge = 0;
  std::uint8_t resume_edge = 0;
  std::size_t first_vertex = 0;
  std::size_t vertex_count = 0;
  std::size_t first_face = 0;
  std::size_t face_count = 0;
};

struct FrameDefinition {
  std::size_t base_vertices = 0;
  std::size_t base_faces = 0;
  std::vector<std::array<std::uint8_t, 2>> vertices;
  std::vector<FramePart> parts;
  std::vector<FrameFace> faces;
};

[[nodiscard]] std::expected<FrameDefinition, FrameDecodeError>
DecodeTextBoxFrame(std::span<const std::uint8_t> bytes);

struct FrameRequest {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::uint8_t alignment = 0;
  std::uint8_t brightness = 0;
  std::size_t part = 0;
};

[[nodiscard]] std::expected<void, FrameDecodeError> DrawTextBoxFrame(
    Render::PacketQueue& foreground, Render::PacketQueue& shadow,
    std::span<const std::uint8_t> bytes, const FrameRequest& request);

}  // namespace VS::Menu
