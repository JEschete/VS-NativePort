#pragma once

#include <exception>

#include "Core/Types.hpp"

namespace VS::Platform {

// Unwinds a cancelled game fiber: thrown from the game's wait when the host
// cancels it. Deliberately not a std::exception, so only GameFiber handles it.
struct FiberCancelled {};

// Runs the game's original control flow (an endless loop that waits for
// vertical blank) as a coroutine: the game calls YieldToHost() where the PS1
// waited for the next frame, and the host loop resumes it once per logical tick
// after presenting and sampling input. One game fiber per process.
//
// Cancellation (Cancel() or destruction) resumes the game once more and throws
// FiberCancelled from its pending wait, so every frame unwinds and runs its
// destructors before the fiber is deleted. A wait reached while that unwinds
// returns at once. Any other exception that leaves the entry is logged with the
// throwing call stack and rethrown on the host by Resume(). Destroying the
// fiber from inside the game terminates the process.
class GameFiber {
 public:
  using Entry = void (*)();
  static constexpr usize kDefaultStackBytes = 16u << 20;

  explicit GameFiber(Entry entry, usize stack_bytes = kDefaultStackBytes);
  ~GameFiber();
  GameFiber(const GameFiber&) = delete;
  GameFiber& operator=(const GameFiber&) = delete;
  GameFiber(GameFiber&&) = delete;
  GameFiber& operator=(GameFiber&&) = delete;

  // Runs the game until it yields or its entry function returns. Returns false
  // once it has finished; rethrows (once) an exception that ended the entry.
  bool Resume();
  // Unwinds an unfinished game and waits for it to finish. Call from the host;
  // a game that never ran just finishes. Never rethrows the game's exception.
  void Cancel();
  bool Finished() const { return m_finished_; }
  bool Cancelled() const { return m_cancelled_; }

  // Called from game code: back to the host until the next Resume(). No-op
  // outside the game fiber; throws FiberCancelled once the host cancels.
  static void YieldToHost();
  static bool InGame();

 private:
  static void __stdcall Start(void* self);
  static void RunEntry(GameFiber* fiber);
  int LogThrowSite(u32 code) const;
  void SwitchIn();
  // Cancel() from the host without its diagnostics, so the destructor cannot
  // throw. Returns whether it discarded the game's exception.
  bool Unwind() noexcept;

  Entry m_entry_;
  void* m_fiber_ = nullptr;
  void* m_host_ = nullptr;
  std::exception_ptr m_failure_;
  bool m_started_ = false;
  bool m_cancel_requested_ = false;
  bool m_cancelled_ = false;
  bool m_finished_ = false;
};

}  // namespace VS::Platform
