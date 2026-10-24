#pragma once

#include <filesystem>

namespace VS::Game::Save {

void ConfigureDirectory(std::filesystem::path root);
void ResetFileMenuSession();
void ShutdownFileMenus();

}  // namespace VS::Game::Save
