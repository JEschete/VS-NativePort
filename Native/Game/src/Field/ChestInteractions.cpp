#include "Field/ChestInteractions.hpp"

namespace VS::Field {
namespace {

constexpr std::int32_t kItemUnlockFlagBase = 0x1B6;
constexpr std::int16_t kConsumedSigilFirst = 0x1D2;
constexpr std::uint8_t kOpeningPose = 2;
constexpr std::uint8_t kRefusedPose = 3;
constexpr std::int16_t kKeyLock = 1;
constexpr std::int16_t kBreakableLock = 2;
constexpr std::int16_t kSigilLock = 3;
constexpr std::uint8_t kClosingPose = 4;
constexpr std::int32_t kStorageFlagFirst = 0x40;
constexpr std::int32_t kOpenedFlagBase = 0x3C0;
constexpr std::int32_t kIdleActorCount = 16;

}  // namespace

// decomp: BATTLE.PRG func_80073E30 (src/BATTLE/BATTLE.PRG/146C.c:4935).
bool TryOpenChest(ChestTarget target, ChestInteractionHost& host) {
  const auto chest = host.Chest(target);
  if (!chest) {
    return false;
  }
  bool can_open = chest->lock_level == 0;
  if (chest->lock_level == kKeyLock || chest->lock_level == kSigilLock) {
    const auto flag = kItemUnlockFlagBase + chest->required_item;
    const bool unlocked_before = host.ScriptFlag(flag);
    if (unlocked_before || host.HasItem(chest->required_item)) {
      if (!unlocked_before) {
        host.SetScriptFlag(flag);
        if (chest->required_item >= kConsumedSigilFirst) {
          host.ConsumeItem(chest->required_item);
        }
      }
      if (!host.Unlock(target)) {
        return false;
      }
      host.ShowUnlocked(chest->required_item);
      can_open = true;
    }
  }
  host.PlayPose(*chest, can_open ? kOpeningPose : kRefusedPose);
  return can_open && host.StartAnimation(target);
}

// decomp: BATTLE.PRG func_80074050 (src/BATTLE/BATTLE.PRG/146C.c:4993).
bool BeginChestInteraction(ChestTarget target, std::int32_t actor,
                           ChestInteractionHost& host) {
  const auto chest = host.Chest(target);
  if (!chest || !host.SelectChest(target, actor)) {
    return false;
  }
  host.BeginChestFlow();
  host.RestoreIdlePoses();
  host.PlayPose(*chest, 1);
  host.ResetActionSelection();
  return true;
}

// decomp: BATTLE.PRG func_800742A4 (src/BATTLE/BATTLE.PRG/146C.c:5066).
bool FinishChestInteraction(ChestInteractionHost& host) {
  const auto target = host.SelectedChest();
  const auto chest = target ? host.Chest(*target) : std::nullopt;
  if (!chest) {
    return false;
  }
  host.FinishChestFlow();
  host.PlayPose(*chest, kClosingPose);
  host.ReleaseNotification();
  return true;
}

// decomp: BATTLE.PRG func_800743E0 (src/BATTLE/BATTLE.PRG/146C.c:5107).
bool OpenChestContents(ChestInteractionHost& host) {
  const auto target = host.SelectedChest();
  const auto chest = target ? host.Chest(*target) : std::nullopt;
  if (!chest) {
    return false;
  }
  if (chest->lock_level != 0) {
    switch (chest->lock_level) {
      case kKeyLock:
        host.ShowLocked(2, chest->required_item);
        break;
      case kBreakableLock:
        host.ShowLocked(1, chest->required_item);
        break;
      case kSigilLock:
        host.ShowLocked(kSigilLock, chest->required_item);
        break;
      default:
        return false;
    }
    host.PauseForDialog();
  } else {
    if (chest->opened_flag < kStorageFlagFirst) {
      host.OpenChestLoot(*target);
    } else {
      host.OpenChestStorage(chest->opened_flag - kStorageFlagFirst);
    }
    host.PauseForMenu();
  }
  return true;
}

// decomp: BATTLE.PRG func_800744B8 (src/BATTLE/BATTLE.PRG/146C.c:5139).
bool UpdateChestContentsSelection(ChestInteractionHost& host) {
  const auto target = host.SelectedChest();
  const auto chest = target ? host.Chest(*target) : std::nullopt;
  if (!chest) {
    return false;
  }
  if (chest->lock_level != 0) {
    if (host.NotificationPending()) {
      return true;
    }
    host.ResumeField();
    return false;
  }
  const auto selection = host.UpdateContentsSelection();
  if (selection == ChestSelectionResult::kPending) {
    return true;
  }
  if (selection == ChestSelectionResult::kAccepted) {
    host.SetScriptFlag(chest->opened_flag + kOpenedFlagBase);
  } else {
    static_cast<void>(host.StartAnimation(*target));
  }
  host.ReleaseContentsRows();
  host.ResumeField();
  return false;
}

// decomp: BATTLE.PRG func_800741D4 (src/BATTLE/BATTLE.PRG/146C.c:5044).
bool DispatchTrapOrChestInteraction(InteractionDispatchHost& host) {
  const auto target = host.Target();
  if (host.TrapMode()) {
    const auto* trap = std::get_if<TrapTarget>(&target);
    if (trap == nullptr) {
      return false;
    }
    host.BeginDispatch();
    return host.DispatchTrap(*trap);
  }
  const auto* chest = std::get_if<ChestTarget>(&target);
  if (chest == nullptr) {
    return false;
  }
  host.BeginDispatch();
  return host.DispatchChest(*chest);
}

// decomp: BATTLE.PRG func_80074314 (src/BATTLE/BATTLE.PRG/146C.c:5080).
bool FinishTrapInteraction(TrapFinishHost& host) {
  const auto target = host.Target();
  if (!target) {
    return false;
  }
  host.FinishFlow();
  host.ReleaseNotification();
  const bool disabled = host.DisableTrap(*target);
  host.ClearTarget();
  return disabled;
}

// decomp: BATTLE.PRG func_80073F7C (src/BATTLE/BATTLE.PRG/146C.c:4973).
bool BeginTrapInteraction(TrapTarget target, std::int32_t actor,
                          TrapSelectionHost& host) {
  if (!host.Valid(target, actor)) {
    return false;
  }
  host.BeginFlow();
  host.Select(target, actor);
  for (std::int32_t slot = 0; slot < kIdleActorCount; ++slot) {
    host.RestoreIdle(slot);
  }
  host.ShowInformation(target);
  host.ResetActionSelection();
  return true;
}

}  // namespace VS::Field
