// Hand-maintained (tools/port/owned.csv).
#include <cstdlib>
#include <iterator>

#include "Actors/ActorSpawningLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Damage.h"
#include "Boot/SessionLive.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Content/SystemResourcesLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/Runtime.h"
#include "Event/ScriptVm.h"
#include "Event/ScriptVmLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/GameModeLiveState.hpp"
#include "Field/Interactions.h"
#include "Field/RoomLoadingLiveState.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomStateLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Field/TransitionsLiveState.hpp"
#include "Input/ControllerLive.hpp"
#include "Items/LootLiveState.hpp"
#include "Legacy/Psx.h"
#include "Render/Camera.h"
#include "Render/CameraViewLive.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/Frame.h"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/Render.h"
#include "Render/ScreenOverlayLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "State/Location.h"
#include "State/SceneTimerLive.hpp"

namespace VS::Boot {
namespace {
class LiveGameplayOverlayHost final : public GameplayOverlayHost {
 public:
  void ResetAudioSlots() override { VS::Audio::LiveResetSoundSlots(); }
  void ResetGameHeap() override { ResetLiveSessionHeap(); }
  void InitializeOrderingTables(
      const GameplayOverlayDefaults& defaults) override {
    Render_legacyPrimitiveBuffers[1] = nullptr;
    Render_legacyPrimitiveBuffers[0] = nullptr;
    for (std::size_t i = 0; i < 2; ++i) {
      Render_legacyOrderingTables[i] = VS::Core::Memory::AllocateLiveFromLowEnd(
          defaults.ordering_table_bytes);
      if (Render_legacyOrderingTables[i] == nullptr) {
        VS::Core::Errorf("boot",
                         "failed to allocate gameplay ordering table %u",
                         static_cast<unsigned>(i));
        std::abort();
      }
    }
    SetDrawStp(&Render_legacyMaskBitOffPackets[0], 0);
    SetDrawStp(&Render_legacyMaskBitOffPackets[1], 0);
    SetDrawStp(&Render_legacyMaskBitOnPackets[0], 1);
    SetDrawStp(&Render_legacyMaskBitOnPackets[1], 1);
  }
  void InitializePaletteAnimation() override {
    VS::Render::InitializeLiveClutAnimation();
  }
  void InitializeRoomTransitionPrimitives() override {
    VS::Field::LiveInitializeRoomTransitionPrimitives();
  }
  void InitializeFramebufferEffect() override {
    VS::Render::InitializeLiveFramebufferEffect();
  }
  void InitializeScriptOpcodeTable() override {
    if (!VS::Event::InitializeLiveScriptOpcodeTable()) {
      VS::Core::Errorf("boot", "Native event VM initialization refused");
      std::abort();
    }
  }
  void InitializeEffectRuntime() override {
    VS::Effects::LiveInitEffectRuntime();
  }
  void InitializeRoomRuntimeState() override {
    VS::Field::LiveInitializeRoomRuntimeState();
  }
  void InitializeActorModelResources() override {
    VS::Actors::InitializeLiveActorModelResources();
  }
  void InitializeTexturePages() override {
    VS::Render::InitializeLiveTexturePages();
  }
  void ResetOverlayWorkspaceOwner() override {
    VS::Content::overlayWorkspaceOwner = 0;
  }
  void LoadSystemResources() override {
    VS::Content::LoadLiveSystemResources();
  }
  void InitializeActorAiHook() override {
    VS::Battle::Ai::InitializeLiveActorAiExtensionHook();
  }
  void ResetVibrationPatterns() override {
    VS::Input::ResetLiveVibrationPatterns();
  }
  void ApplyLearnedActionUnlocks() override {
    VS::Battle::ApplyLiveLearnedActionUnlocks();
  }
  void ApplyGameplayDefaults(const GameplayOverlayDefaults& defaults) override {
    VS::Field::zoneContext.currentSceneId = 0;
    VS::Field::zoneContext.reserved38 = 0;
    VS::Field::zoneContext.mapId = 0;
    VS::Field::zoneContext.zndId = 0;
    VS::Field::zoneContext.zudDiscRequest = nullptr;
    VS::Field::zoneContext.mpdDiscRequest = nullptr;
    VS::Field::zoneContext.zndDiscRequest = nullptr;
    VS::Field::zoneContext.mpdData = nullptr;
    VS::Field::zoneContext.zndData = nullptr;
    VS::Field::zoneContext.musicSlot = 0;
    VS::Field::zoneContext.musicId = 0;
    VS::Field::zoneContext.isMusicFadeEnabled =
        defaults.zone_music_fade_enabled ? 1 : 0;
    VS::Field::zoneContext.loadedZudIndexPlusOne = 0;
    Field_pendingDestinationDoor = State_savedEntryDoor;
    VS::Field::screenTransitionEffect = defaults.transition_effect;
    VS::Field::screenTransitionStep = defaults.transition_step;
    VS::Field::screenTransitionSpeed = defaults.transition_speed;
    VS::Field::screenTransitionWipeAngle = defaults.transition_wipe_angle;
    VS::Field::isInteractionPaused = defaults.interaction_paused ? 1 : 0;
    VS::Items::lootListHead = nullptr;
    // The original also cleared the write-only D_800F1874 (18.c:570).
    Field_controlMode = defaults.field_control_mode;
    VS::State::SetLiveTickSpeed(defaults.tick_speed);
    VS::Render::SetLiveCameraProjectionDistance(defaults.projection_distance);
    VS::Render::SetLiveCameraNearClip(defaults.near_clip);
    VS::State::SceneTimer timer = VS::State::LiveSceneTimer();
    timer.mode = VS::State::kSceneTimerStopped;
    timer.time = {};
    VS::State::StoreLiveSceneTimer(timer);
    VS::Field::zoneContext.currentSceneId = State_savedEntryDoor.requiredFlagId;
    VS::Field::cameraTransition = nullptr;
    VS::Field::LiveResetCameraToAshley();
    VS::Render::SetLiveCameraFarClip(defaults.far_clip, 1);
    VS::Field::sphericalCamera.delta.pitch = defaults.camera_pitch_delta;
    VS::Field::sphericalCamera.values.yaw = defaults.camera_yaw;
    VS::Field::sphericalCamera.values.pitch = defaults.camera_pitch;
    VS::Field::sphericalCamera.initialDistance = defaults.camera_distance;
    VS::Field::sphericalCamera.values.distance = defaults.camera_distance;
    VS::Field::sphericalCamera.delta.yaw = defaults.camera_yaw_delta;
    VS::Field::sphericalCamera.delta.distance = defaults.camera_distance_delta;
    VS::Field::sphericalCamera.delta.mode = defaults.camera_delta_mode;
    VS::Field::sphericalCamera.zoomVelocity = defaults.camera_zoom_velocity;
  }
  void SetRoomActorScale(const VS::Core::Math::Vec3& scale) override {
    VECTOR legacy_scale{scale.x, scale.y, scale.z, 0};
    VS::Actors::SetLiveRoomActorScale(&legacy_scale);
  }
  void ClearActorSlots(std::size_t slot_count) override {
    constexpr std::size_t kActorCapacity = std::size(Battle_actors);
    if (slot_count > kActorCapacity) {
      VS::Core::Errorf("boot",
                       "gameplay actor slot count %u exceeds capacity %u",
                       static_cast<unsigned>(slot_count),
                       static_cast<unsigned>(kActorCapacity));
      std::abort();
    }
    for (std::size_t i = 0; i < slot_count; ++i) {
      Battle_actors[i] = nullptr;
    }
  }
  void SpawnAshleyWithEquipment() override {
    VS::Actors::SpawnLiveAshleyWithEquipment();
  }
  void AllocateBattleActionState() override {
    auto* action_state = static_cast<Battle_ActionState*>(
        VS::Core::Memory::AllocateLiveFromHighEnd(sizeof(Battle_ActionState)));
    if (action_state == nullptr) {
      VS::Core::Errorf("boot", "failed to allocate battle action state");
      std::abort();
    }
    VS::Core::Memory::ZeroLiveAlignedWords(
        action_state, static_cast<std::int32_t>(sizeof *action_state));
    Battle_actionState = action_state;
  }
  void SetFieldRenderingEnabled(bool enabled) override {
    VS::Field::LiveSetFieldRenderingEnabled(enabled ? 1 : 0);
  }
  void ConfigureFieldDisplay(const GameplayOverlayDefaults& defaults) override {
    VS::Render::ConfigureLiveFieldDisplay(
        defaults.display_width, defaults.display_height,
        defaults.projection_distance, defaults.display_x, defaults.display_y,
        defaults.display_mode);
  }
};

LiveGameplayOverlayHost live_gameplay_overlay_host;
}  // namespace

void InitializeGameplayOverlayLive([[maybe_unused]] int start_state) {
  InitializeGameplayOverlay(live_gameplay_overlay_host);
}
}  // namespace VS::Boot
