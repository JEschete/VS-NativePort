// Hand-maintained (tools/port/owned.csv).
#include <cstdlib>
#include <iterator>

#include "Actors/ActorSpawningLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/SystemResourcesLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomStateLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Input/ControllerLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "State/SceneTimerLive.hpp"

extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Damage.h"
#include "Boot/GameplayInit.h"
#include "Core/Diagnostics.h"
#include "Effects/Runtime.h"
#include "Event/ScriptVm.h"
#include "Field/CameraLiveState.h"
#include "Field/GameModeLiveState.h"
#include "Field/Interactions.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/TransitionsLiveState.h"
#include "Items/LootLiveState.h"
#include "Legacy/Psx.h"
#include "OverlayWorkspaceLiveState.h"
#include "Render/Camera.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "State/Location.h"
}

namespace VS::Boot {
namespace {
class LiveGameplayOverlayHost final : public GameplayOverlayHost {
 public:
  void ResetAudioSlots() override { VS::Audio::LiveResetSoundSlots(); }
  void ResetGameHeap() override { VS::Core::Memory::ResetLiveGameHeap(); }
  void InitializeOrderingTables(
      const GameplayOverlayDefaults& defaults) override {
    Render_legacyPrimitiveBuffers[1] = nullptr;
    Render_legacyPrimitiveBuffers[0] = nullptr;
    for (std::size_t i = 0; i < 2; ++i) {
      Render_legacyOrderingTables[i] = VS::Core::Memory::AllocateLiveFromLowEnd(
          defaults.ordering_table_bytes);
      if (Render_legacyOrderingTables[i] == nullptr) {
        Core_Errorf("boot", "failed to allocate gameplay ordering table %u",
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
    Event_InitializeScriptOpcodeTable();
  }
  void InitializeEffectRuntime() override { Effects_InitEffectRuntime(); }
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
    Content_overlayWorkspaceOwner = 0;
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
    Battle_ApplyLearnedActionUnlocks();
  }
  void ApplyGameplayDefaults(const GameplayOverlayDefaults& defaults) override {
    Field_zoneContext.currentSceneId = 0;
    Field_zoneContext.reserved38 = 0;
    Field_zoneContext.mapId = 0;
    Field_zoneContext.zndId = 0;
    Field_zoneContext.zudDiscRequest = nullptr;
    Field_zoneContext.mpdDiscRequest = nullptr;
    Field_zoneContext.zndDiscRequest = nullptr;
    Field_zoneContext.mpdData = nullptr;
    Field_zoneContext.zndData = nullptr;
    Field_zoneContext.musicSlot = 0;
    Field_zoneContext.musicId = 0;
    Field_zoneContext.isMusicFadeEnabled =
        defaults.zone_music_fade_enabled ? 1 : 0;
    Field_zoneContext.loadedZudIndexPlusOne = 0;
    Field_pendingDestinationDoor = State_savedEntryDoor;
    Field_screenTransitionEffect = defaults.transition_effect;
    Field_screenTransitionStep = defaults.transition_step;
    Field_screenTransitionSpeed = defaults.transition_speed;
    Field_screenTransitionWipeAngle = defaults.transition_wipe_angle;
    Field_isInteractionPaused = defaults.interaction_paused ? 1 : 0;
    Items_lootListHead = nullptr;
    Boot_gameplayResetWordOne = 0;
    Field_controlMode = defaults.field_control_mode;
    VS::State::SetLiveTickSpeed(defaults.tick_speed);
    VS::Render::SetLiveCameraProjectionDistance(defaults.projection_distance);
    VS::Render::SetLiveCameraNearClip(defaults.near_clip);
    VS::State::SceneTimer timer = VS::State::LiveSceneTimer();
    timer.mode = VS::State::kSceneTimerStopped;
    timer.time = {};
    VS::State::StoreLiveSceneTimer(timer);
    Field_zoneContext.currentSceneId = State_savedEntryDoor.requiredFlagId;
    Field_cameraTransition = nullptr;
    VS::Field::LiveResetCameraToAshley();
    VS::Render::SetLiveCameraFarClip(defaults.far_clip, 1);
    Field_sphericalCamera.delta.pitch = defaults.camera_pitch_delta;
    Field_sphericalCamera.values.yaw = defaults.camera_yaw;
    Field_sphericalCamera.values.pitch = defaults.camera_pitch;
    Field_sphericalCamera.initialDistance = defaults.camera_distance;
    Field_sphericalCamera.values.distance = defaults.camera_distance;
    Field_sphericalCamera.delta.yaw = defaults.camera_yaw_delta;
    Field_sphericalCamera.delta.distance = defaults.camera_distance_delta;
    Field_sphericalCamera.delta.mode = defaults.camera_delta_mode;
    Field_sphericalCamera.zoomVelocity = defaults.camera_zoom_velocity;
  }
  void SetRoomActorScale(const VS::Core::Math::Vec3& scale) override {
    VECTOR legacy_scale{scale.x, scale.y, scale.z, 0};
    VS::Actors::SetLiveRoomActorScale(&legacy_scale);
  }
  void ClearActorSlots(std::size_t slot_count) override {
    constexpr std::size_t kActorCapacity = std::size(Battle_actors);
    if (slot_count > kActorCapacity) {
      Core_Errorf("boot", "gameplay actor slot count %u exceeds capacity %u",
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
      Core_Errorf("boot", "failed to allocate battle action state");
      std::abort();
    }
    VS::Core::Memory::ZeroLiveAlignedWords(
        action_state, static_cast<std::int32_t>(sizeof *action_state));
    Battle_actionState = action_state;
  }
  void SetFieldRenderingEnabled(bool enabled) override {
    VS::Field::LiveSetFieldRenderingEnabled(enabled ? 1 : 0);
  }
  void ResetGameplayResetWordTwo() override { Boot_gameplayResetWordTwo = 0; }
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
