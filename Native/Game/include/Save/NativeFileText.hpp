#pragma once

#include <string_view>

namespace VS::Game::Save {
void BeginNativeFileText();
void ResetNativeFileText();
void SelectNativeFileText(std::string_view id);
void DrawNativeFileText(bool confirmation);
}  // namespace VS::Game::Save
