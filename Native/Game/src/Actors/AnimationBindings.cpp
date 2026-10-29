#include "Actors/AnimationBindings.h"

#include <bit>
#include <cstddef>

#include "Core/Diagnostics.h"
#include "Legacy/OnDisc.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
}

namespace {
constexpr int kSharedAnimationFirst = 220;
constexpr int kSpecialAnimationFirst = 254;
constexpr unsigned kSharedBankCount = 3;
constexpr unsigned kMissingAnimation = 255;
constexpr unsigned kHeaderWords = 5;
constexpr unsigned kBoneHeaderWords = 2;
}  // namespace

// F00998/44F14.c:610 and F00279/146C.c:9008 must undo the bound old pose.
// The selector used for new clips can change before either consumer runs.
intptr_t Actors_FindBoundAnimationHeader(Actors_ActorRecord* actor,
                                         u_short** header_out) {
  if (actor == nullptr || header_out == nullptr) {
    Core_Errorf("actors.sequence",
                "Bound animation lookup requires an actor and header output");
    return 0;
  }
  *header_out = nullptr;
  const int animation = actor->animationId;
  if (animation < kSharedAnimationFirst ||
      animation >= kSpecialAnimationFirst) {
    const auto bank =
        Actors_FindActorAnimationHeader(actor, animation, header_out);
    if (bank != actor->animationHeader ||
        static_cast<u_char>(Actors_lastResolvedSequenceIndex) !=
            actor->animationSet) {
      *header_out = nullptr;
      return 0;
    }
    return bank;
  }
  auto* bound =
      reinterpret_cast<Actors_SeqAnimationBank*>(actor->animationHeader);
  if (bound == nullptr) return 0;
  bool owned = false;
  for (unsigned index = 0; index < kSharedBankCount; ++index) {
    owned = owned || actor->sharedSequences[index] == bound;
  }
  if (!owned) return 0;
  const auto* map = VS_ONDISC(bound, animationMap, const u_char*);
  const unsigned physical = map[animation - kSharedAnimationFirst];
  if (physical == kMissingAnimation || physical != actor->animationSet) {
    return 0;
  }
  const std::size_t stride =
      kHeaderWords + kBoneHeaderWords * actor->instance.boneCount;
  *header_out = &bound->headers[stride * physical];
  Actors_lastResolvedSequenceIndex =
      std::bit_cast<char>(static_cast<u_char>(physical));
  return actor->animationHeader;
}
