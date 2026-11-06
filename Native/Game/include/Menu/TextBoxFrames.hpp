#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "Render/UiPackets.hpp"

namespace VS::Menu {

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
