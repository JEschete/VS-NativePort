#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "Actors/ActorInstances.hpp"
#include "Content/ActorAssets.hpp"

// The asset loads actors own (#48). The original queued an actor's model,
// equipment and sequence loads without a record of whose they were or how
// they ended. Natively each load an actor queues carries a ticket; this table
// holds the ticket until the load ends, reports the outcome to the host and
// forgets the ticket, so every load completes explicitly and the shared
// ticket table never fills with finished loads.
namespace VS::Actors {

class ActorLoadHost {
 public:
  ActorLoadHost() = default;
  ActorLoadHost(const ActorLoadHost&) = delete;
  ActorLoadHost& operator=(const ActorLoadHost&) = delete;
  ActorLoadHost(ActorLoadHost&&) = delete;
  ActorLoadHost& operator=(ActorLoadHost&&) = delete;
  virtual ~ActorLoadHost() = default;

  [[nodiscard]] virtual Content::ActorAssetCompletion Completion(
      Content::ActorAssetTicket ticket) = 0;
  virtual void Forget(Content::ActorAssetTicket ticket) = 0;
  // A load of `actor` ended, or its owner released it: loaded, failed or
  // cancelled; still pending when released early; unknown when the ticket was
  // lost.
  virtual void Settled(SpawnedActor actor,
                       const Content::ActorAssetCompletion& completion) = 0;
};

class ActorLoadTable {
 public:
  // A tracked load holds a queued request until it ends, so the queue bounds
  // the table once ended loads are settled.
  static constexpr std::size_t kCapacity = Content::kActorAssetRequestCount;

  // Holds `ticket` for `actor`; false for kNone or a full table.
  [[nodiscard]] bool Track(SpawnedActor actor,
                           Content::ActorAssetTicket ticket) {
    if (actor == SpawnedActor::kNone ||
        ticket == Content::ActorAssetTicket::kNone) {
      return false;
    }
    const auto free = std::ranges::find(
        loads_, Content::ActorAssetTicket::kNone, &Load::ticket);
    if (free == loads_.end()) {
      return false;
    }
    *free = {.actor = actor, .ticket = ticket};
    return true;
  }

  // Reports and forgets every load that has ended.
  void Settle(ActorLoadHost& host) {
    for (Load& load : loads_) {
      if (load.ticket == Content::ActorAssetTicket::kNone) {
        continue;
      }
      const Content::ActorAssetCompletion completion =
          host.Completion(load.ticket);
      if (completion.outcome != Content::ActorAssetOutcome::kPending) {
        Finish(host, load, completion);
      }
    }
  }

  // Reports and forgets every load of `actor`, once its owner has cancelled or
  // finished them.
  void Release(ActorLoadHost& host, SpawnedActor actor) {
    for (Load& load : loads_) {
      if (load.ticket != Content::ActorAssetTicket::kNone &&
          load.actor == actor) {
        Finish(host, load, host.Completion(load.ticket));
      }
    }
  }

  // The loads of `actor` not yet settled.
  [[nodiscard]] std::size_t Tracked(SpawnedActor actor) const {
    return static_cast<std::size_t>(
        std::ranges::count_if(loads_, [actor](const Load& load) {
          return load.ticket != Content::ActorAssetTicket::kNone &&
                 load.actor == actor;
        }));
  }

 private:
  struct Load {
    SpawnedActor actor = SpawnedActor::kNone;
    Content::ActorAssetTicket ticket = Content::ActorAssetTicket::kNone;
  };

  static void Finish(ActorLoadHost& host, Load& load,
                     const Content::ActorAssetCompletion& completion) {
    const Load ended = load;
    load = {};
    host.Forget(ended.ticket);
    host.Settled(ended.actor, completion);
  }

  std::array<Load, kCapacity> loads_{};
};
}  // namespace VS::Actors
