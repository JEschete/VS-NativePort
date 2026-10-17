#pragma once

#include "Core/Types.hpp"

namespace VS::Platform {

// Runs the copied game's original control flow (an endless loop that waits for vertical blank) as a coroutine:
// the game calls YieldToHost() where the PS1 waited for the next frame, and the host loop resumes it once per logical
// tick after presenting and sampling input. One game fiber per process.
class GameFiber {
public:
    using Entry = void (*)();

    explicit GameFiber(Entry entry, usize stackBytes = 16u << 20);
    ~GameFiber();
    GameFiber(const GameFiber&) = delete;
    GameFiber& operator=(const GameFiber&) = delete;

    // Runs the game until it yields or its entry function returns. Returns false once it has finished.
    bool Resume();
    bool Finished() const { return m_finished; }

    // Called from game code: back to the host until the next Resume(). No-op outside the game fiber.
    static void YieldToHost();
    static bool InGame();

private:
    static void __stdcall Start(void* self);

    Entry m_entry;
    void* m_fiber = nullptr;
    void* m_host = nullptr;
    bool m_finished = false;
};

} // namespace VS::Platform
