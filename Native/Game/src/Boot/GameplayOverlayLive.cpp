// Hand-maintained (tools/port/owned.csv).
#include <cstdlib>
#include <iterator>

#include "Actors/ActorSpawningLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.hpp"
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
#include "Effects/Runtime.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/GameModeLiveState.hpp"
#include "Field/Interactions.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomStateLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Field/TransitionsLiveState.hpp"
#include "Input/ControllerLive.hpp"
#include "Items/LootLiveState.hpp"
#include "Legacy/Psx.h"
#include "Render/Camera.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/Frame.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/Render.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "State/Location.hpp"
#include "State/SceneTimerLive.hpp"
#include "State/Timers.hpp"

namespace VS::Boot {
namespace {
class LiveGameplayOverlayHost final : public GameplayOverlayHost {
 public:
  void ResetAudioSlots() override { VS::Audio::LiveResetSoundSlots(); }
  void ResetGameHeap() override { ResetLiveSessionHeap(); }
  void InitializeOrderingTables(
      const GameplayOverlayDefaults& defaults) override {
    VS::Render::legacyPrimitiveBuffers[1] = nullptr;
    VS::Render::legacyPrimitiveBuffers[0] = nullptr;
    for (std::size_t i = 0; i < 2; ++i) {
      VS::Render::legacyOrderingTables[i] =
          VS::Core::Memory::AllocateLiveFromLowEnd(
              defaults.ordering_table_bytes);
      if (VS::Render::legacyOrderingTables[i] == nullptr) {
        VS::Core::Errorf("boot",
                         "failed to allocate gameplay ordering table %u",
                         static_cast<unsigned>(i));
        std::abort();
      }
    }
    SetDrawStp(&VS::Render::legacyMaskBitOffPackets[0], 0);
    SetDrawStp(&VS::Render::legacyMaskBitOffPackets[1], 0);
    SetDrawStp(&VS::Render::legacyMaskBitOnPackets[0], 1);
    SetDrawStp(&VS::Render::legacyMaskBitOnPackets[1], 1);
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
    VS::Field::zoneContext.scene_id = 0;
    VS::Field::zoneContext.reserved38 = 0;
    VS::Field::zoneContext.room_id = 0;
    VS::Field::zoneContext.zone_id = 0;
    VS::Field::zoneContext.zud_disc_request = nullptr;
    VS::Field::zoneContext.room_disc_request = nullptr;
    VS::Field::zoneContext.zone_disc_request = nullptr;
    VS::Field::zoneContext.room_data = nullptr;
    VS::Field::zoneContext.zone_data = nullptr;
    VS::Field::zoneContext.music_slot = 0;
    VS::Field::zoneContext.music_id = 0;
    VS::Field::zoneContext.music_fade_enabled =
        defaults.zone_music_fade_enabled ? 1 : 0;
    VS::Field::zoneContext.loaded_zud_index_plus_one = 0;
    Field_pendingDestinationDoor = VS::State::savedEntryDoor;
    VS::Field::screenTransitionEffect = defaults.transition_effect;
    VS::Field::screenTransitionStep = defaults.transition_step;
    VS::Field::screenTransitionSpeed = defaults.transition_speed;
    VS::Field::screenTransitionWipeAngle = defaults.transition_wipe_angle;
    VS::Field::isInteractionPaused = defaults.interaction_paused ? 1 : 0;
    VS::Items::lootListHead = nullptr;
    // The original also cleared the write-only D_800F1874 (18.c:570).
    VS::Field::controlMode = defaults.field_control_mode;
    VS::State::SetLiveTickSpeed(defaults.tick_speed);
    VS::Render::SetLiveCameraProjectionDistance(defaults.projection_distance);
    VS::Render::SetLiveCameraNearClip(defaults.near_clip);
    VS::State::sceneTimer.mode = VS::State::kSceneTimerStopped;
    VS::State::sceneTimer.time = {};
    VS::Field::zoneContext.scene_id = VS::State::savedEntryDoor.requiredFlagId;
    VS::Field::cameraTransition = nullptr;
    VS::Field::LiveResetCameraToAshley();
    VS::Render::SetLiveCameraFarClip(defaults.far_clip, 1);
    VS::Field::sphericalCamera.motion.target_yaw = defaults.camera_pitch_delta;
    VS::Field::sphericalCamera.values.yaw = defaults.camera_yaw;
    VS::Field::sphericalCamera.values.pitch = defaults.camera_pitch;
    VS::Field::sphericalCamera.preferred_distance = defaults.camera_distance;
    VS::Field::sphericalCamera.values.distance = defaults.camera_distance;
    VS::Field::sphericalCamera.motion.turn = defaults.camera_yaw_delta;
    VS::Field::sphericalCamera.motion.turn_speed =
        defaults.camera_distance_delta;
    VS::Field::sphericalCamera.motion.zoom_mode = defaults.camera_delta_mode;
    VS::Field::sphericalCamera.zoom_velocity = defaults.camera_zoom_velocity;
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
    VS::Battle::actionState = action_state;
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
