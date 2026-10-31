#pragma once

#include <filesystem>

#include "Launcher/Setup.hpp"

namespace VS::Launcher {
int ShowLauncher(const Settings& settings,
                 const std::filesystem::path& installation,
                 const std::filesystem::path& state, int show);
}
