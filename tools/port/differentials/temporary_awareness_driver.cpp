// Native side of tools\port\differentials\temporary_awareness.py (F01630): one vector per stdin
// line (the initial frames and marker, then the marker and frame arguments) and one line with
// the resulting frames and marker.
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "Battle/Ai/Engagement.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        long long frames = 0, marker = 0, marker_argument = 0, frames_argument = 0;
        in >> frames >> marker >> marker_argument >> frames_argument;
        if (!in)
            std::abort();
        VS::Battle::Ai::TemporaryAwareness awareness { static_cast<std::uint8_t>(frames),
            static_cast<std::uint16_t>(marker) };
        VS::Battle::Ai::SetTemporaryAwarenessIndicator(
            awareness, static_cast<int>(marker_argument), static_cast<int>(frames_argument));
        std::cout << static_cast<unsigned>(awareness.frames_remaining) << ' ' << awareness.marker << '\n';
    }
}
