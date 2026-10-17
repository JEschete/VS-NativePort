#pragma once

#include "Core/Types.hpp"

#include <memory>

namespace VS::Content {
class Disc;
}
namespace VS::Input {
struct PadState;
}

namespace VS::Game {

// Top-level flow (plan §6.8): Boot -> Title -> Game <-> Battle/Menu -> Ending.
// Replaces the original overlay swaps (jumpToTitle / jumpToBattle).
enum class Mode : u8 { Boot, Title, Field, Battle, Menu, Ending };

const char* ModeName(Mode mode);

// Every module (formerly an overlay) implements this lifecycle.
class IModule {
public:
    virtual ~IModule() = default;
    virtual void Init() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(const Input::PadState& pad) = 0;  // one logical tick
    virtual void Reset() = 0;  // statics the original reset by reloading the overlay
};

struct Services {
    const Content::Disc* disc = nullptr;
};

class GameApp {
public:
    explicit GameApp(Services services);
    ~GameApp();
    // One logical tick: hands the pad to the game and runs it until it waits for the next frame.
    void Tick(const Input::PadState& pad, bool padConnected);
    Mode CurrentMode() const { return m_mode; }
    u64 Ticks() const { return m_ticks; }
    bool Finished() const;

private:
    Services m_services;
    Mode m_mode = Mode::Boot;
    u64 m_ticks = 0;
    struct Runner;
    std::unique_ptr<Runner> m_runner;
};

} // namespace VS::Game
