#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>

#include "Save/NativeFiles.hpp"
#include "Save/NativeSnapshot.hpp"

namespace VS::Game::Save {

inline constexpr std::size_t kNativeFileRoomNameBytes = 32;

struct NativeFileSummary {
  std::array<char, kNativeFileRoomNameBytes> room_name{};
  int location_artwork = -1;
};

// Resolves owned summary bytes from extracted content, never live room globals.
std::expected<NativeFileSummary, std::string> DescribeNativeFile(
    const Document& document);

struct NativeFileScreen {
  bool title = false;
  bool save = false;
  bool select_container = false;
  // The caller supplies the visible window (up to three rows), in slot order.
  std::span<const FileInfo> files;
  std::span<const NativeFileSummary> summaries;
  int selected_row = 0;
  bool confirmation = false;
  bool affirmative = false;
};

std::optional<NativeFileScreen> FileMenuPresentation();

// Called after the original frame/message and before its final fade. Does no IO
// and leaves the original element arrays, cursor and UI palettes unchanged.
void DrawNativeFileScreen(const NativeFileScreen& screen);

}  // namespace VS::Game::Save
