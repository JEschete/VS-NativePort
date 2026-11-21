#include <process.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ShpModel.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/MixControlLive.hpp"
#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Damage.hpp"
#include "Boot/GameLoop.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/RandomLive.hpp"
#include "Ending/EndingLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/Doors.hpp"
#include "Field/GameModeLiveState.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Interactions.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Field/RoomLoading.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/Categories.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Render/Camera.hpp"
#include "Render/UiPrimitives.hpp"
#include "Save/RuntimeSnapshot.hpp"
#include "State/Location.hpp"
#include "State/PlayTime.hpp"
#include "State/Progress.hpp"
#include "State/RoomMemory.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/StateFlags.hpp"
#include "State/Timers.hpp"
#include "Title/NewGame.hpp"

using VS::Core::Math::kQ12Unit;

namespace {
namespace Save = VS::Game::Save;
namespace Content = VS::Content;
using CategoryEnds = std::array<unsigned, 8>;
constexpr CategoryEnds kBounds{96, 127, 254, 261, 308, 323, 458, 512};

std::filesystem::path CreateFixtureRoot() {
  static std::atomic<std::uint64_t> sequence{0};
  const auto base =
      std::filesystem::current_path() / "build" / "runtime-snapshot-tests";
  std::filesystem::create_directories(base);
  for (;;) {
    const auto root =
        base / ("process-" + std::to_string(_getpid()) + "-fixture-" +
                std::to_string(sequence.fetch_add(1)));
    std::error_code error;
    const bool created = std::filesystem::create_directory(root, error);
    REQUIRE_FALSE(error);
    if (created) return root;
  }
}

template <typename T>
void Put(std::vector<std::uint8_t>& bytes, std::size_t offset, const T& value) {
  if (bytes.size() < offset + sizeof(value)) {
    bytes.resize(offset + sizeof(value));
  }
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void Write(const std::filesystem::path& path,
           std::span<const std::uint8_t> bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  REQUIRE(file.good());
  file.write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  REQUIRE(file.good());
}

struct ContentFixture {
  Content::DataStore::ScopedOverride service;
  std::filesystem::path root = CreateFixtureRoot();
  std::vector<std::uint8_t> room;
  std::size_t collision_offset = 0;
  std::size_t environment_offset = 0;
  std::size_t categories_offset = 0;
  std::string categories_module;
  std::size_t minimum_room_bytes = 0;

  explicit ContentFixture(std::size_t minimum_bytes = 0)
      : minimum_room_bytes(minimum_bytes) {
    BuildRoom();
    BuildIndex();
    REQUIRE(Content::DataStore::Open(root).has_value());
  }

  void BuildRoom() {
    std::array<std::uint32_t, 24> lengths{};
    constexpr std::size_t kSide = 4;
    lengths[1] = sizeof(VS::Field::MpdCollisionHeader) + kSide * kSide * 4 + 16;
    // A short region table is valid, independent of collision grid area.
    lengths[2] = 3;
    lengths[10] = sizeof(VS::Field::MpdEnvironment);
    VS::Field::MpdHeader header{};
    header.room_section_offset = sizeof(header);
    Put(room, sizeof(header), lengths);
    collision_offset = sizeof(header) + sizeof(lengths);
    VS::Field::MpdCollisionHeader collision{kSide, kSide, 0, 1};
    Put(room, collision_offset, collision);
    room.resize(collision_offset + lengths[1] + lengths[2], 0);
    environment_offset = room.size();
    VS::Field::MpdEnvironment environment{};
    environment.scene_id = 37;
    Put(room, environment_offset, environment);
    header.enemy_section_offset = static_cast<int>(room.size());
    header.enemy_section_size = 8 * sizeof(VS::Field::MpdEnemySpawn);
    room.resize(room.size() + header.enemy_section_size);
    Put(room, 0, header);
    if (room.size() < minimum_room_bytes) room.resize(minimum_room_bytes);
    WriteRoom();
  }

  void BuildIndex() {
    for (const auto& slice : VS::Content::ModuleDataEntries()) {
      if (slice.target != &VS::Items::itemCategoryUpperIds) continue;
      categories_offset = slice.offset;
      categories_module = slice.module;
      break;
    }
    REQUIRE_FALSE(categories_module.empty());
    std::vector<std::uint8_t> battle;
    Put(battle, categories_offset, kBounds);
    Write(root / "disc" / categories_module, battle);

    VS::Field::ZoneFileHeader zone{};
    zone.room_files_offset = sizeof(zone);
    zone.room_files_size = 8;
    std::vector<std::uint8_t> zone_bytes;
    Put(zone_bytes, 0, zone);
    Put(zone_bytes, sizeof(zone), std::uint32_t{200});
    const auto sector_bytes = Content::DataStore::kDataSectorBytes;
    const auto allocation =
        ((room.size() + sector_bytes - 1) / sector_bytes) * sector_bytes;
    Put(zone_bytes, sizeof(zone) + 4, static_cast<std::uint32_t>(allocation));
    for (const auto name : {"ZONE001.ZND", "ZONE009.ZND", "ZONE010.ZND",
                            "ZONE011.ZND", "ZONE012.ZND"}) {
      Write(root / "disc" / "MAP" / name, zone_bytes);
    }
    const std::array<std::uint8_t, 4> model{1, 2, 3, 4};
    Write(root / "disc" / "OBJ" / "01.WEP", model);
    Write(root / "disc" / "OBJ" / "02.WEP", model);
    Write(root / "disc" / "OBJ" / "10.WEP", model);
    std::ofstream manifest(root / Content::DataStore::ManifestName);
    REQUIRE(manifest.good());
    manifest << "lba\tsize\tform\tpath\n";
    unsigned sector = 100;
    for (const auto name : {"ZONE001.ZND", "ZONE009.ZND", "ZONE010.ZND",
                            "ZONE011.ZND", "ZONE012.ZND"}) {
      manifest << sector++ << '\t' << zone_bytes.size() << "\traw\tMAP/" << name
               << '\n';
    }
    manifest << "200\t" << room.size() << "\traw\tMAP/MAP009.MPD\n";
    manifest << "10000\t" << battle.size() << "\traw\t" << categories_module
             << "\n";
    manifest << "20000\t4\traw\tOBJ/01.WEP\n";
    manifest << "20001\t4\traw\tOBJ/02.WEP\n";
    manifest << "20002\t4\traw\tOBJ/10.WEP\n";
    manifest.close();
  }

  void WriteRoom() { Write(root / "disc" / "MAP" / "MAP009.MPD", room); }

  void WriteZoneRequest(std::uint32_t sector, std::uint32_t bytes) {
    VS::Field::ZoneFileHeader zone{};
    zone.room_files_offset = sizeof(zone);
    zone.room_files_size = 8;
    std::vector<std::uint8_t> data;
    Put(data, 0, zone);
    Put(data, sizeof(zone), sector);
    Put(data, sizeof(zone) + sizeof(sector), bytes);
    Write(root / "disc" / "MAP" / "ZONE009.ZND", data);
  }

  ~ContentFixture() noexcept(false) {
    service.Restore();
    std::error_code error;
    std::filesystem::remove_all(root, error);
    CHECK_FALSE(error);
  }
};

struct RuntimeFixture {
  ContentFixture content;
  decltype(VS::State::stateFlags) flags = VS::State::stateFlags;
  decltype(VS::Items::inventory) inventory = VS::Items::inventory;
  decltype(VS::Items::inventoryIndices) order = VS::Items::inventoryIndices;
  decltype(VS::Input::settings) settings = VS::Input::settings;
  decltype(VS::State::roomVisitProgress) visits = VS::State::roomVisitProgress;
  decltype(VS::State::artsStatus) arts = VS::State::artsStatus;
  decltype(VS::State::scoreData) score = VS::State::scoreData;
  decltype(VS::State::ashleySnapshot) ashley = VS::State::ashleySnapshot;
  decltype(VS::State::savedEntryDoor) entry = VS::State::savedEntryDoor;
  decltype(VS::Field::snapshotDoorFlagSource) detached_scene =
      VS::Field::snapshotDoorFlagSource;
  decltype(VS::State::playTime) time = VS::State::playTime;
  decltype(VS::State::sceneTimer) timer = VS::State::sceneTimer;
  decltype(VS::Field::zoneContext) zone = VS::Field::zoneContext;
  decltype(VS::Field::roomData) room = VS::Field::roomData;
  decltype(VS::Field::sphericalCamera) camera = VS::Field::sphericalCamera;
  decltype(VS::Render::FrameScratchpad().camera) rendered_camera =
      VS::Render::FrameScratchpad().camera;
  decltype(std::to_array(VS::State::recentRoomEnemyStates)) caches =
      std::to_array(VS::State::recentRoomEnemyStates);
  decltype(std::to_array(VS::State::learnedActionFlags)) learned =
      std::to_array(VS::State::learnedActionFlags);
  decltype(VS::Battle::actionDefinitions) actions =
      VS::Battle::actionDefinitions;
  decltype(VS::Battle::actors) actors = VS::Battle::actors;
  decltype(VS::Actors::actorRecords) models = VS::Actors::actorRecords;
  decltype(VS::Battle::Ai::actorAiStates) ai = VS::Battle::Ai::actorAiStates;
  decltype(std::to_array(VS::Core::randomShuffleTable)) shuffle =
      std::to_array(VS::Core::randomShuffleTable);
  decltype(std::to_array(VS::Items::itemCategoryUpperIds)) bounds =
      std::to_array(VS::Items::itemCategoryUpperIds);
  VS::Battle::Actor* character = VS::Battle::characterState;
  unsigned bios = VS::Core::BiosRand().State();
  unsigned interface_random = VS::Core::interfaceRandomState;
  int shuffle_index = VS::Core::randomShuffleIndex;
  int movement_random = VS::Battle::Ai::savedMovementRandomState;
  unsigned frame_random = VS::Battle::Ai::LiveFrameContext().randomState;
  int current_cache = VS::State::currentRoomStateSlot;
  int gameplay = VS::Boot::isGameplayActive;
  int clear_pending = VS::Boot::isClearSavePending;
  int near_clip = VS::Render::nearClip;
  int projection = VS::Render::projectionDistance;
  int first_person = VS::Field::isFirstPersonViewEnabled;
  unsigned control_mode = VS::Field::controlMode;
  int interaction_paused = VS::Field::isInteractionPaused;
  VS::Field::CameraTransitionRecord* transition = VS::Field::cameraTransition;
  VS::Audio::DriverFlags audio_flags = VS::Audio::Driver::controlLatches;
  VS::Audio::LegacyCommandArguments audio_arguments =
      VS::Audio::Driver::commandArguments;
  VS::Audio::CommandParams audio_params = VS::Audio::Mixer::commandParams;
  VS::Audio::MusicConfig* active_music = VS::Audio::Seq::activeMusicConfig;
  VS::Audio::MusicConfig* secondary_music =
      VS::Audio::Seq::secondaryMusicConfig;
  VS::Audio::CommandHandler stereo_handler =
      VS::Audio::Mixer::kCommandHandlers[0x80];
  VS::Audio::CommandHandler mono_handler =
      VS::Audio::Mixer::kCommandHandlers[0x81];
  unsigned active_effect_voices =
      VS::Audio::Mixer::voiceScheduler.active_voices;
  VS::Audio::MusicConfig silent_music{};
  VS::Battle::Actor player{};
  VS::Battle::Actor enemy{};
  VS::Battle::ActorStats player_stats{};
  VS::Battle::ActorStats enemy_stats{};
  VS::Actors::ActorRecord player_model{};
  VS::Actors::ActorRecord enemy_model{};
  VS::Battle::Ai::ActorAiState enemy_ai{};
  VS::Field::MpdEnvironment environment{};
  VS::Field::CameraTransitionRecord camera_transition{};
  VS::Items::ContainerData storage{};

  RuntimeFixture() {
    REQUIRE_FALSE(Save::ResumePending());
    std::copy(kBounds.begin(), kBounds.end(), VS::Items::itemCategoryUpperIds);
    std::fill_n(VS::Battle::actors.begin(), actors.size(), nullptr);
    std::fill_n(VS::Actors::actorRecords.begin(), models.size(), nullptr);
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), ai.size(), nullptr);
    VS::Battle::characterState = &player;
    VS::Battle::actors[0] = &player;
    VS::Actors::actorRecords[0] = &player_model;
    // The three-byte textureRowOffsets union member does not initialize the
    // high byte of momentumDistance. Match VS::Actors::PlaceActorInRoom's
    // reset.
    VS::Actors::ResetLiveActorRunMomentum(0);
    player.stats = &player_stats;
    player.model = &player_model.instance;
    player.room_spawn_index = 0x80;
    player_model.instance.position = {193, -73, 325, 0};
    player_model.instance.facing = 2051;
    player_model.instance.current_tile_x = 1;
    player_model.instance.current_tile_z = 2;
    player_model.instance.previous_tile_x = 1;
    player_model.instance.previous_tile_z = 2;
    player_model.instance.previous_facing = 2016;
    VS::Actors::TileTarget grounded{};
    grounded.tile_x = 1;
    grounded.tile_z = 2;
    grounded.facing = 128;
    player_model.saved_placement = std::bit_cast<int>(grounded);
    player_stats.current_hp = 123;
    player_stats.max_hp = 300;
    player_stats.current_mp = 31;
    player_stats.max_mp = 77;
    player_stats.strength = 300;
    player_stats.total_strength = 330;
    player_stats.intelligence = 222;
    player_stats.total_intelligence = 225;
    player_stats.agility = 111;
    player_stats.total_agility = 119;
    player_stats.risk = 77;
    player_stats.action_recovery_timer = 19;
    player_stats.statuses = 0x21000;
    player_stats.status_timers[7] = 255;
    for (auto& limb : player_stats.limbs) {
      limb.hp = 45;
      limb.max_hp = 100;
      limb.types[3] = -29;
      limb.affinities[7] = -32768;
    }
    VS::Items::inventory = {};
    VS::Items::inventoryIndices = {};
    SetEquipment();
    SetProgress();
    SetWorld();
    SetAudio();
    SetClocksAndCaches();
    SetStorage();
  }

  void SetProgress() {
    VS::State::stateFlags = {};
    VS::State::SetLiveScriptFlag(0x43F, static_cast<char>(0xAB));
    VS::State::SetLiveScriptFlag(64, 99);
    VS::State::SetLiveScriptFlag(161, 17);
    VS::Input::settings = {};
    VS::Input::settings.mapped_chain_abilities[3] = static_cast<char>(255);
    VS::State::roomVisitProgress = {};
    VS::State::roomVisitProgress.room_flags[3] = 0x10203040;
    VS::State::artsStatus = {};
    VS::State::artsStatus.kills.total_kills = 1234;
    VS::State::scoreData = {};
    VS::State::scoreData.boss_time_trial_scores[7][2].time = 0xABCDEF;
    std::fill_n(VS::State::learnedActionFlags, learned.size(),
                static_cast<char>(0xFF));
    for (auto& action : VS::Battle::actionDefinitions) action.unlocked = 0;
    for (const auto index : {0, 7, 8, 97, 255}) {
      VS::Battle::actionDefinitions[index].unlocked = 1;
    }
  }

  void SetWorld() {
    VS::Field::controlMode = 1;
    VS::Field::isInteractionPaused = 0;
    VS::Field::isFirstPersonViewEnabled = 0;
    VS::Field::cameraTransition = nullptr;
    VS::Field::zoneContext = {};
    VS::Field::zoneContext.zone_id = 9;
    VS::Field::zoneContext.room_id = 0;
    VS::Field::zoneContext.scene_id = 37;
    VS::Field::zoneContext.bounds = {0, 0, 4, 4};
    environment.scene_id = 37;
    VS::Field::roomData = {};
    VS::Field::roomData.is_loaded = 1;
    VS::Field::roomData.environment = &environment;
    VS::Field::roomData.header.environment_length = sizeof environment;
    VS::Field::sphericalCamera = {};
    VS::Field::sphericalCamera.values.yaw = 1027;
    VS::Field::sphericalCamera.values.pitch = -384;
    VS::Field::sphericalCamera.values.distance = 1536;
    VS::Field::sphericalCamera.motion.target_yaw = 2050;
    VS::Field::sphericalCamera.preferred_distance = 1536;
    VS::Field::sphericalCamera.motion.zoom_mode = 1;
    VS::Render::nearClip = 64;
    VS::Render::projectionDistance = 512;
    VS::Render::FrameScratchpad().camera = {};
    VS::Render::FrameScratchpad().camera.far_clip = kQ12Unit;
    VS::Render::FrameScratchpad().camera.look_at = {100000, -200000, 300000, 0};
    VS::Render::FrameScratchpad().camera.angles = {-384, 1027, 0, 0};
    UpdateOrbitPosition();
  }

  void UpdateOrbitPosition() {
    VS::Field::LiveSetCameraPositionFromAngles(
        &VS::Field::sphericalCamera.values);
  }

  void SetAudio() {
    VS::Audio::Seq::activeMusicConfig = &silent_music;
    VS::Audio::Seq::secondaryMusicConfig = nullptr;
    VS::Audio::Mixer::voiceScheduler.active_voices = 0;
    VS::Audio::Mixer::kCommandHandlers[0x80] =
        VS::Audio::Mixer::LiveEnableStereoOutput;
    VS::Audio::Mixer::kCommandHandlers[0x81] =
        VS::Audio::Mixer::LiveEnableMonoOutput;
    VS::Audio::LiveSetMonoOutput(0);
  }

  void SetClocksAndCaches() {
    VS::State::playTime = {59, 58, 57, 12};
    VS::State::sceneTimer = {};
    VS::State::sceneTimer.mode = 2;
    VS::State::sceneTimer.reset_scope = 2;
    VS::State::sceneTimer.time.seconds = 17;
    VS::State::sceneTimer.time.frames = 29;
    VS::Core::SeedBiosRand(0x12345678);
    VS::Core::interfaceRandomState = 0x87654321;
    VS::Core::randomShuffleIndex = 96;
    for (std::size_t index = 0; index < shuffle.size(); ++index) {
      VS::Core::randomShuffleTable[index] = static_cast<int>(index) * 337;
    }
    VS::Battle::Ai::savedMovementRandomState = std::bit_cast<int>(0xFEDCBA98u);
    VS::State::currentRoomStateSlot = 0;
    for (auto& cache : VS::State::recentRoomEnemyStates) cache = {};
    VS::State::recentRoomEnemyStates[0].zone_id = 9;
    VS::State::recentRoomEnemyStates[0].map_id = 0;
    VS::State::recentRoomEnemyStates[0].recency = 4;
    VS::State::recentRoomEnemyStates[0].next_slot = 7;
    VS::Boot::isGameplayActive = 1;
  }

  void SetStorage() {
    storage.misc[255].id = 511;
    storage.misc[255].count = 99;
    auto& stored_weapon = storage.weapons[31];
    stored_weapon.index = 32;
    stored_weapon.blade = stored_weapon.grip = 64;
    stored_weapon.gems[0] = 192;
    storage.blades[63].id = 95;
    storage.blades[63].weapon_model_id = 1;
    storage.blades[63].assembled_weapon_index = 32;
    storage.grips[63].id = 126;
    storage.grips[63].gem_slots = 1;
    storage.grips[63].assembled_weapon_index = 32;
    storage.gems[191].id = 307;
    storage.gems[191].set_item_index = 32;
    storage.gems[191].index = 192;
  }

  void SetEquipment() {
    auto& items = VS::Items::inventory;
    auto& blade = items.blades[0];
    blade.id = 14;
    blade.index = 1;
    blade.weapon_model_id = 1;
    blade.category = 3;
    blade.assembled_weapon_index = 1;
    blade.max_dp = 500;
    blade.current_dp = 400;
    blade.max_pp = 200;
    blade.current_pp = 150;
    auto& grip = items.grips[0];
    grip.id = 96;
    grip.index = 1;
    grip.gem_slots = 3;
    grip.assembled_weapon_index = 1;
    auto& weapon = items.weapons[0];
    weapon.index = 1;
    weapon.is_equipped = 1;
    weapon.blade = weapon.grip = 1;
    weapon.gems[0] = 1;
    auto& shield = items.shields[0];
    shield.index = shield.is_equipped = 1;
    shield.base.id = 127;
    shield.base.weapon_model_id = 2;
    shield.base.gem_slots = 1;
    shield.base.max_dp = shield.base.current_dp = 300;
    shield.base.max_pp = shield.base.current_pp = 100;
    shield.gems[0] = 2;
    items.gems[0].id = 261;
    items.gems[0].index = 1;
    items.gems[0].set_item_index = 1;
    items.gems[1].id = 262;
    items.gems[1].index = 2;
    items.gems[1].set_item_index = 0x81;
    auto& armor = items.armor[0];
    armor.id = 140;
    armor.index = armor.limb = 1;
    armor.max_dp = armor.current_dp = 500;
    auto& accessory = items.armor[1];
    accessory.id = 150;
    accessory.category = 7;
    accessory.index = 2;
    accessory.limb = 7;
    items.misc[0].id = 308;
    items.misc[0].count = 2;
    items.misc[0].index = 1;
    auto& indices = VS::Items::inventoryIndices;
    indices.weapons[0] = indices.shields[0] = indices.blades[0] = 1;
    indices.grips[0] = indices.armor[0] = indices.gems[0] = indices.misc[0] = 1;
    indices.armor[1] = indices.gems[1] = 2;
    auto& runtime_weapon = player_stats.weapon;
    runtime_weapon.index = 1;
    VS::Items::ReadLiveInventoryBladeStats(&runtime_weapon.blade, &blade);
    VS::Items::ReadLiveInventoryGripStats(&runtime_weapon.grip, &grip);
    VS::Items::ReadLiveInventoryGemStats(&runtime_weapon.gems[0],
                                         &items.gems[0]);
    runtime_weapon.current_dp = 101;
    runtime_weapon.current_pp = 67;
    runtime_weapon.name[3] = static_cast<char>(0xE7);
    player_stats.shield.index = 1;
    VS::Items::ReadLiveInventoryArmorStats(&player_stats.shield.base,
                                           &shield.base);
    VS::Items::ReadLiveInventoryGemStats(&player_stats.shield.gems[0],
                                         &items.gems[1]);
    player_stats.shield.current_dp = 201;
    player_stats.shield.current_pp = 37;
    auto& runtime_armor = player_stats.limbs[0].armor;
    runtime_armor.index = 1;
    VS::Items::ReadLiveInventoryArmorStats(&runtime_armor.armor, &armor);
    runtime_armor.current_dp = 321;
    player_stats.accessory.index = 2;
    VS::Items::ReadLiveInventoryArmorStats(&player_stats.accessory.accessory,
                                           &accessory);
  }

  void AddEnemy() {
    VS::Battle::actors[2] = &enemy;
    VS::Actors::actorRecords[2] = &enemy_model;
    VS::Battle::Ai::actorAiStates[2] = &enemy_ai;
    enemy.room_spawn_index = 0;
    enemy.stats = &enemy_stats;
    enemy.model = &enemy_model.instance;
    enemy_stats.current_hp = 43;
    enemy_stats.max_hp = 90;
    enemy_stats.current_mp = 12;
    enemy_stats.max_mp = 44;
    enemy_stats.total_strength = 123;
    enemy_stats.total_intelligence = 234;
    enemy_stats.total_agility = 345;
    enemy_stats.limbs[5].hp = 32;
    enemy_model.instance.facing = 2864;
    VS::Actors::TileTarget placement{};
    placement.tile_x = 2;
    placement.tile_layer = 1;
    placement.tile_z = 3;
    enemy_model.saved_placement = std::bit_cast<int>(placement);
    enemy_ai.temporary_ashley_alliance = 1;
    enemy_ai.behavior_mode = 0;
    enemy_ai.relations[0].classification = 3;
    enemy_ai.relations[0].previous_classification = 2;
    enemy_ai.relations[0].classification_timer = 77;
  }

  Save::Snapshot Capture() const {
    const auto result = Save::Capture(storage);
    INFO((result ? "" : result.error()));
    INFO("momentumDistance=" << player_model.momentum_distance
                             << ", transitionFrames="
                             << player_model.transition_frames);
    REQUIRE(result.has_value());
    return *result;
  }

  Save::Document ClearGame() {
    const auto state = Capture();
    VS::State::ashleySnapshot.stats = state.ashley;
    VS::State::savedEntryDoor = {};
    VS::State::savedEntryDoor.entry_tile_x = 3;
    VS::State::savedEntryDoor.entry_tile_z = 2;
    VS::State::savedEntryDoor.entry_facing = 1;
    VS::State::savedEntryDoor.tile_location.entry_surface = 1;
    VS::State::savedEntryDoor.required_flag_id = 37;
    VS::Input::settings.current_game_save_count = 7;
    VS::Input::settings.cumulative_save_count = 19;
    VS::Input::settings.menu_flags = 0xFFFF;
    VS::Input::settings.mode_flags = 8;
    VS::Ending::LiveCommitClearProgress();
    VS::Boot::isGameplayActive = 0;
    VS::Boot::isClearSavePending = 1;
    const auto document = Save::CaptureClearGame();
    INFO((document ? "" : document.error()));
    REQUIRE(document);
    return *document;
  }

  ~RuntimeFixture() {
    Save::CancelResume();
    VS::Boot::isClearSavePending = clear_pending;
    VS::State::stateFlags = flags;
    VS::Items::inventory = inventory;
    VS::Items::inventoryIndices = order;
    VS::Input::settings = settings;
    VS::State::roomVisitProgress = visits;
    VS::State::artsStatus = arts;
    VS::State::scoreData = score;
    VS::State::ashleySnapshot = ashley;
    VS::State::savedEntryDoor = entry;
    VS::Field::snapshotDoorFlagSource = detached_scene;
    VS::State::playTime = time;
    VS::State::sceneTimer = timer;
    VS::Field::zoneContext = zone;
    VS::Field::roomData = room;
    VS::Field::sphericalCamera = camera;
    VS::Render::FrameScratchpad().camera = rendered_camera;
    std::copy(caches.begin(), caches.end(), VS::State::recentRoomEnemyStates);
    std::copy(learned.begin(), learned.end(), VS::State::learnedActionFlags);
    std::copy(actions.begin(), actions.end(),
              VS::Battle::actionDefinitions.data());
    std::copy(actors.begin(), actors.end(), VS::Battle::actors.begin());
    std::copy(models.begin(), models.end(), VS::Actors::actorRecords.begin());
    std::copy(ai.begin(), ai.end(), VS::Battle::Ai::actorAiStates.begin());
    std::copy(shuffle.begin(), shuffle.end(), VS::Core::randomShuffleTable);
    std::copy(bounds.begin(), bounds.end(), VS::Items::itemCategoryUpperIds);
    VS::Battle::characterState = character;
    VS::Core::SeedBiosRand(bios);
    VS::Core::interfaceRandomState = interface_random;
    VS::Core::randomShuffleIndex = shuffle_index;
    VS::Battle::Ai::savedMovementRandomState = movement_random;
    VS::Battle::Ai::LiveFrameContext().randomState = frame_random;
    VS::State::currentRoomStateSlot = current_cache;
    VS::Boot::isGameplayActive = gameplay;
    VS::Render::nearClip = near_clip;
    VS::Render::projectionDistance = projection;
    VS::Field::isFirstPersonViewEnabled = first_person;
    VS::Field::controlMode = control_mode;
    VS::Field::isInteractionPaused = interaction_paused;
    VS::Field::cameraTransition = transition;
    VS::Audio::Driver::controlLatches = audio_flags;
    VS::Audio::Driver::commandArguments = audio_arguments;
    VS::Audio::Mixer::commandParams = audio_params;
    VS::Audio::Seq::activeMusicConfig = active_music;
    VS::Audio::Seq::secondaryMusicConfig = secondary_music;
    VS::Audio::Mixer::kCommandHandlers[0x80] = stereo_handler;
    VS::Audio::Mixer::kCommandHandlers[0x81] = mono_handler;
    VS::Audio::Mixer::voiceScheduler.active_voices = active_effect_voices;
  }
};

template <typename T>
void Append(std::vector<std::uint8_t>& bytes, const T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  const auto* begin = reinterpret_cast<const std::uint8_t*>(&value);
  bytes.insert(bytes.end(), begin, begin + sizeof(value));
}

// Test-only fingerprints, never serialized as native save data.
std::vector<std::uint8_t> LiveFingerprint(const RuntimeFixture& fixture) {
  std::vector<std::uint8_t> bytes;
  Append(bytes, VS::State::stateFlags);
  Append(bytes, VS::Items::inventory);
  Append(bytes, VS::Items::inventoryIndices);
  Append(bytes, VS::Input::settings);
  Append(bytes, VS::State::roomVisitProgress);
  Append(bytes, VS::State::artsStatus);
  Append(bytes, VS::State::scoreData);
  Append(bytes, VS::State::recentRoomEnemyStates);
  Append(bytes, VS::State::currentRoomStateSlot);
  Append(bytes, VS::State::learnedActionFlags);
  Append(bytes, VS::Battle::actionDefinitions);
  Append(bytes, VS::State::ashleySnapshot);
  Append(bytes, VS::State::savedEntryDoor);
  Append(bytes, VS::State::playTime);
  Append(bytes, VS::State::sceneTimer);
  Append(bytes, VS::Field::sphericalCamera);
  Append(bytes, VS::Render::FrameScratchpad().camera);
  Append(bytes, VS::Field::zoneContext);
  Append(bytes, VS::Field::roomData);
  Append(bytes, VS::Field::isFirstPersonViewEnabled);
  Append(bytes, VS::Field::controlMode);
  Append(bytes, VS::Field::isInteractionPaused);
  Append(bytes, VS::Field::cameraTransition != nullptr);
  Append(bytes, VS::Render::nearClip);
  Append(bytes, VS::Render::projectionDistance);
  Append(bytes, fixture.player_stats);
  Append(bytes, fixture.player_model);
  Append(bytes, fixture.enemy_stats);
  Append(bytes, fixture.enemy_model);
  Append(bytes, fixture.enemy_ai);
  Append(bytes, fixture.storage);
  Append(bytes, VS::Core::BiosRand().State());
  Append(bytes, VS::Core::interfaceRandomState);
  Append(bytes, VS::Core::randomShuffleTable);
  Append(bytes, VS::Core::randomShuffleIndex);
  Append(bytes, VS::Battle::Ai::savedMovementRandomState);
  Append(bytes, VS::Battle::Ai::LiveFrameContext().randomState);
  Append(bytes, VS::Audio::Driver::controlLatches.mix_behavior);
  return bytes;
}
}  // namespace

namespace {
void CheckCapturedLocation(const Save::Snapshot& state) {
  CHECK(state.location.zone == 9);
  CHECK(state.location.room == 0);
  CHECK(state.location.scene == 37);
  CHECK(state.location.position == std::array<std::int32_t, 3>{193, -73, 325});
  CHECK(state.location.facing == 2051);
  CHECK(state.location.camera_yaw == 1027);
}

void CheckCapturedStats(const Save::Snapshot& state) {
  CHECK(state.ashley.current_hp == 123);
  CHECK(state.ashley.total_strength == 330);
  CHECK(state.ashley.status_timers[7] == 255);
  CHECK(state.ashley.limbs[5].types[3] == static_cast<std::uint16_t>(-29));
  CHECK(state.ashley.limbs[5].affinities[7] == 32768);
  CHECK(state.script_flags.back() == 0xAB);
  for (std::size_t index = 0; index < 256; ++index) {
    CHECK((state.learned_actions[index] ==
           (index == 0 || index == 7 || index == 8 || index == 97 ||
            index == 255)));
  }
}

void CheckCapturedEquipment(const Save::Snapshot& state) {
  CHECK(state.inventory.blades[0].current_dp == 101);
  CHECK(state.inventory.blades[0].current_pp == 67);
  CHECK(state.inventory.shields[0].base.current_dp == 201);
  CHECK(state.inventory.shields[0].base.current_pp == 37);
  CHECK(state.inventory.armor[0].current_dp == 321);
  CHECK(state.inventory.weapons[0].name[3] == static_cast<char>(0xE7));
  CHECK(state.inventory_order.armor[1] == 2);
}

void CheckCapturedProgress(const Save::Snapshot& state) {
  CHECK(state.storage.misc[255].id == 511);
  CHECK(state.storage.weapons[31].gems[0] == 192);
  CHECK(state.storage.gems[191].id == 307);
  CHECK(state.visits.room_flags[3] == 0x10203040);
  CHECK(state.arts.kills.total_kills == 1234);
  CHECK(state.score.boss_time_trial_scores[7][2].time == 0xABCDEF);
  CHECK(state.room_cache[0].next_slot == 0);
}

void CheckCapturedCache(const Save::Snapshot& state) {
  const auto& actor = state.room_cache[0].actors[7];
  CHECK(actor.spawn_slot == 1);
  CHECK(actor.current_hp == 43);
  CHECK(actor.strength == 123);
  CHECK(actor.limb_hp[5] == 32);
  CHECK(actor.tile_x == 2);
  CHECK(actor.surface == 1);
  CHECK(actor.tile_z == 3);
  CHECK(actor.facing == 179);
}

void CheckCapturedRandom(const Save::Snapshot& state) {
  const auto& actor = state.room_cache[0].actors[7];
  CHECK(actor.ai_flags == (1 | 6 | 16 | 32 | (77 << 6)));
  CHECK(state.random.bios_state == 0x12345678);
  CHECK(state.random.movement_state == 0xFEDCBA98);
}
}  // namespace

TEST_CASE("Runtime capture reads canonical gameplay without modifying it",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  fixture.AddEnemy();
  VS::Field::snapshotDoorFlagSource.required_flag_id = 999;
  const auto before = LiveFingerprint(fixture);
  const auto state = fixture.Capture();
  CHECK(LiveFingerprint(fixture) == before);
  CheckCapturedLocation(state);
  CheckCapturedStats(state);
  CheckCapturedEquipment(state);
  CheckCapturedProgress(state);
  CheckCapturedCache(state);
  CheckCapturedRandom(state);
}

TEST_CASE("Runtime fixtures clear reused momentum union storage",
          "[save][runtime-snapshot]") {
  alignas(RuntimeFixture) std::array<std::byte, sizeof(RuntimeFixture)> storage;
  storage.fill(std::byte{0x65});
  const auto destroy = [](RuntimeFixture* fixture) {
    std::destroy_at(fixture);
  };
  std::unique_ptr<RuntimeFixture, decltype(destroy)> fixture(
      std::construct_at(reinterpret_cast<RuntimeFixture*>(storage.data())),
      destroy);
  CHECK(fixture->player_model.momentum_distance == 0);
  const auto before = LiveFingerprint(*fixture);
  (void)fixture->Capture();
  CHECK(LiveFingerprint(*fixture) == before);
  fixture->player_model.momentum_distance = 0x6500;
  const auto moving = LiveFingerprint(*fixture);
  CHECK_FALSE(Save::Capture(fixture->storage).has_value());
  CHECK(LiveFingerprint(*fixture) == moving);
}

TEST_CASE(
    "Runtime content fixtures clean only their uniquely claimed directory",
    "[save][runtime-snapshot]") {
  std::filesystem::path outer_root;
  std::filesystem::path inner_root;
  {
    ContentFixture outer;
    outer_root = outer.root;
    {
      ContentFixture inner;
      inner_root = inner.root;
      CHECK(inner_root != outer_root);
      CHECK(inner_root.filename().string().starts_with(
          "process-" + std::to_string(_getpid()) + "-fixture-"));
      CHECK(std::filesystem::exists(outer_root /
                                    Content::DataStore::ManifestName));
    }
    CHECK_FALSE(std::filesystem::exists(inner_root));
    CHECK(
        std::filesystem::exists(outer_root / Content::DataStore::ManifestName));
    REQUIRE(Content::DataStore::Get() != nullptr);
    CHECK(Content::DataStore::Get()->Root() == outer_root);
  }
  CHECK_FALSE(std::filesystem::exists(outer_root));
}

TEST_CASE(
    "Scoped content ownership restores null rather than an empty replacement",
    "[save][runtime-snapshot]") {
  Content::DataStore::ScopedOverride isolate;
  REQUIRE(Content::DataStore::Get() == nullptr);
  {
    ContentFixture fixture;
    REQUIRE(Content::DataStore::Get() != nullptr);
  }
  CHECK(Content::DataStore::Get() == nullptr);
}

TEST_CASE(
    "Scoped content ownership retains exact instances despite deleted files",
    "[save][runtime-snapshot]") {
  ContentFixture outer;
  const auto* previous = Content::DataStore::Get();
  REQUIRE(previous != nullptr);
  const auto* borrowed = previous->Find("MAP/MAP009.MPD");
  REQUIRE(borrowed != nullptr);
  std::filesystem::remove_all(outer.root);
  {
    ContentFixture inner;
    CHECK(Content::DataStore::Get() != previous);
  }
  REQUIRE(Content::DataStore::Get() == previous);
  CHECK(previous->Find("MAP/MAP009.MPD") == borrowed);
  CHECK(borrowed->lba == 200);
  CHECK_FALSE(previous->ReadAll(*borrowed).has_value());
}

TEST_CASE(
    "Scoped content restore is idempotent and failed opens retain ownership",
    "[save][runtime-snapshot]") {
  ContentFixture fixture;
  const auto* previous = Content::DataStore::Get();
  {
    Content::DataStore::ScopedOverride scope;
    REQUIRE(Content::DataStore::Get() == nullptr);
    CHECK_FALSE(Content::DataStore::Open(fixture.root / "missing").has_value());
    scope.Restore();
    REQUIRE(Content::DataStore::Get() == previous);
    scope.Restore();
    CHECK(Content::DataStore::Get() == previous);
  }
  CHECK(Content::DataStore::Get() == previous);
}

TEST_CASE("ZND room requests accept exact or sector-rounded MPD byte counts",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  const auto state = fixture.Capture();
  ContentFixture content(65904);
  const auto request = GENERATE(std::uint32_t{65904}, std::uint32_t{67584});
  content.WriteZoneRequest(200, request);
  const auto valid = Save::ValidateContent(state);
  INFO((valid ? "" : valid.error()));
  CHECK(valid.has_value());
}

TEST_CASE(
    "ZND allocation padding cannot hide truncated or invalid MPD references",
    "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  const auto state = fixture.Capture();
  const auto invalid = GENERATE(0, 1, 2, 3, 4);
  const auto file_size =
      static_cast<std::uint32_t>(fixture.content.room.size());
  if (invalid == 0) fixture.content.WriteZoneRequest(200, 0);
  if (invalid == 1) fixture.content.WriteZoneRequest(200, file_size - 1);
  if (invalid == 2) fixture.content.WriteZoneRequest(200, 4096);
  if (invalid == 3) fixture.content.WriteZoneRequest(201, 2048);
  if (invalid == 4) {
    Put(fixture.content.room,
        sizeof(VS::Field::MpdHeader) + 23 * sizeof(std::uint32_t),
        std::uint32_t{1024});
    fixture.content.WriteRoom();
  }
  const auto before = LiveFingerprint(fixture);
  const auto valid = Save::ValidateContent(state);
  REQUIRE_FALSE(valid.has_value());
  CHECK(LiveFingerprint(fixture) == before);
}

namespace {
void InvalidateLiveMotion(RuntimeFixture& fixture, int defect) {
  if (defect == 11) fixture.player_model.instance.movement_pending = 1;
  if (defect == 12) fixture.player_model.instance.movement_mode = 3;
  if (defect == 13) fixture.player_model.instance.locomotion_state = 1;
  if (defect == 14) fixture.player_model.instance.is_jumping = 1;
  if (defect == 15) fixture.player_model.instance.turn_frames = 2;
  if (defect == 16) fixture.player_model.instance.move_frames = 1;
  if (defect == 17) fixture.player_model.instance.root_motion.vx = 1;
  if (defect == 18) fixture.player_model.route_phase = 1;
  if (defect == 19) fixture.player_model.momentum_distance = 2;
  if (defect == 20) fixture.player_model.instance.placement_layer = 2;
  if (defect == 21) fixture.player_model.instance.is_attached = 1;
  if (defect == 31) fixture.player_model.transition_frames = 1;
}

void InvalidateLiveFixture(RuntimeFixture& fixture, int defect) {
  InvalidateLiveMotion(fixture, defect);
  if (defect == 0) VS::Field::zoneContext.scene_id = 38;
  if (defect == 1) VS::Boot::isGameplayActive = 0;
  if (defect == 2) fixture.player_stats.weapon.index = 9;
  if (defect == 3) VS::Items::inventory.weapons[0].grip = 255;
  if (defect == 4) VS::State::currentRoomStateSlot = 4;
  if (defect == 5) VS::Field::roomData.environment = nullptr;
  if (defect == 6) fixture.player_model.instance.current_tile_layer = 2;
  if (defect == 7) fixture.player_model.instance.support_layer = 2;
  if (defect == 8) VS::Field::isFirstPersonViewEnabled = 1;
  if (defect == 9) VS::Field::sphericalCamera.values.distance = 2000;
  if (defect == 10) VS::Field::sphericalCamera.values.pitch = -768;
  if (defect == 22) VS::Field::sphericalCamera.motion.zoom_mode = 2;
  if (defect == 23) VS::Field::sphericalCamera.motion.turn = 1;
  if (defect == 24) ++VS::Render::FrameScratchpad().camera.position.vx;
  if (defect == 25) VS::Render::FrameScratchpad().camera.angles.vz = 1;
  if (defect == 26) VS::Render::nearClip = 128;
  if (defect == 27) VS::Field::sphericalCamera.values.pitch = 1024;
  if (defect == 28) VS::Render::projectionDistance = 256;
  if (defect == 29) VS::Field::sphericalCamera.motion.turn_speed = 65;
  if (defect == 30) VS::Field::cameraTransition = &fixture.camera_transition;
}
}  // namespace

TEST_CASE("Runtime capture refuses detached scene identity and unready state",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  const auto defect =
      GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
               19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
  InvalidateLiveFixture(fixture, defect);
  const auto before = LiveFingerprint(fixture);
  CHECK_FALSE(Save::Capture(fixture.storage).has_value());
  CHECK(LiveFingerprint(fixture) == before);
}

namespace {
void InvalidateCameraSnapshot(Save::Snapshot& state, int defect) {
  if (defect == 19) state.camera.zoom_mode = 2;
  if (defect == 20) state.camera.pitch = 1024;
  if (defect == 21) {
    state.camera.look_at[0] = std::numeric_limits<std::int32_t>::max();
  }
  if (defect == 22) state.camera.rotation_step = 41;
  if (defect == 23) state.camera.zoom_velocity = 65;
  if (defect == 24) state.camera.tracking_speed = 41;
}

void InvalidateSnapshot(Save::Snapshot& state, int defect) {
  InvalidateCameraSnapshot(state, defect);
  if (defect == 0) state.inventory.weapons[0].blade = 17;
  if (defect == 1) state.inventory.grips[0].id = 127;
  if (defect == 2) state.inventory.gems[1].set_item_index = 0xFFFF;
  if (defect == 3) state.inventory.shields[0].gems[0] = 0x80;
  if (defect == 4) state.inventory_order.gems[1] = 1;
  if (defect == 5) state.inventory.blades[0].weapon_model_id = 127;
  if (defect == 6) state.location.zone = 13;
  if (defect == 7) state.location.room = 1;
  if (defect == 8) state.location.scene = 38;
  if (defect == 9) state.location.position[0] = 512;
  if (defect == 10) state.location.position[1] = 32768;
  if (defect == 11) state.random.shuffle_index = 97;
  if (defect == 12) state.storage.misc[255].id = 512;
  if (defect == 13) state.inventory.armor[1].limb = 1;
  if (defect == 14) state.inventory.blades[0].material = 8;
  if (defect == 15) state.room_cache[0].actors[0].spawn_slot = 9;
  if (defect == 16) state.inventory_order.gems[0] = 0;
  if (defect == 17) state.room_cache_slot = 1;
  if (defect == 18) state.room_cache[1] = state.room_cache[0];
  if (defect == 25) state.location.floor_layer = 2;
  if (defect == 26) state.location.previous_tile.layer = 2;
}

void CheckStagedInputs() {
  CHECK(Save::ResumePending());
  CHECK(static_cast<unsigned char>(VS::State::LiveScriptFlag(64)) == 177);
  CHECK(VS::State::savedEntryDoor.required_flag_id == 37);
  CHECK(VS::State::savedEntryDoor.destination_zone_id == 9);
  CHECK(VS::State::savedEntryDoor.entry_tile_x == 1);
  CHECK(VS::State::savedEntryDoor.entry_tile_z == 2);
  CHECK(VS::State::ashleySnapshot.stats.is_pending == 1);
  CHECK(static_cast<unsigned char>(VS::State::learnedActionFlags[12]) == 0x40);
  CHECK(VS::State::learnedActionFlags[0] == 0);
}
}  // namespace

TEST_CASE("Runtime validation resolves only referenced content and is atomic",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto state = fixture.Capture();
  REQUIRE(Save::ValidateContent(state).has_value());
  const auto defect = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                               15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26);
  InvalidateSnapshot(state, defect);
  const auto before = LiveFingerprint(fixture);
  const auto valid = Save::ValidateContent(state);
  INFO((valid ? "" : valid.error()));
  CHECK_FALSE(valid.has_value());
  CHECK_FALSE(Save::PrepareResume({1, 0, state}).has_value());
  CHECK_FALSE(Save::ResumePending());
  CHECK(LiveFingerprint(fixture) == before);
}

TEST_CASE("Runtime validation checks content extents without mutating globals",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  const auto state = fixture.Capture();
  const auto defect = GENERATE(0, 1, 2, 3);
  if (defect == 0) {
    Put(fixture.content.room, sizeof(VS::Field::MpdHeader) + 4,
        std::uint32_t{0xFFFFFFFF});
  }
  if (defect == 1) {
    Put(fixture.content.room, fixture.content.collision_offset + 8,
        std::uint16_t{0x7FF});
  }
  if (defect == 2) {
    std::filesystem::remove(fixture.content.root / "disc" / "OBJ" / "01.WEP");
  }
  if (defect == 3) {
    auto collision = VS::Field::MpdCollisionHeader{33, 4, 0, 1};
    Put(fixture.content.room, fixture.content.collision_offset, collision);
  }
  fixture.content.WriteRoom();
  const auto before = LiveFingerprint(fixture);
  CHECK_FALSE(Save::ValidateContent(state).has_value());
  CHECK(LiveFingerprint(fixture) == before);
}

TEST_CASE("Runtime content validation works before BATTLE globals initialize",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  const auto state = fixture.Capture();
  std::fill_n(VS::Items::itemCategoryUpperIds, 8, 0);
  REQUIRE(Save::ValidateContent(state).has_value());
  CHECK(std::ranges::all_of(VS::Items::itemCategoryUpperIds,
                            [](auto value) { return value == 0; }));
}

TEST_CASE("Original New Game inventory accepts its lazy display-index state",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto state = fixture.Capture();
  static_assert(sizeof(VS::Title::kNewGameInventoryTemplate) ==
                sizeof(state.inventory));
  std::memcpy(&state.inventory, VS::Title::kNewGameInventoryTemplate,
              sizeof(state.inventory));
  state.inventory_order = {};
  const auto before = LiveFingerprint(fixture);
  const auto valid = Save::ValidateContent(state);
  INFO((valid ? "" : valid.error()));
  REQUIRE(valid.has_value());
  CHECK(LiveFingerprint(fixture) == before);
  const auto encoded = Save::Encode({1, 0, state});
  REQUIRE(encoded.has_value());
  const auto decoded = Save::Decode(*encoded);
  REQUIRE(decoded.has_value());
  CHECK(decoded->state.inventory_order.weapons[0] == 0);
  CHECK(decoded->state.inventory.weapons[0].blade == 1);
  CHECK(Save::ValidateContent(decoded->state).has_value());
}

TEST_CASE(
    "Partial display caches retain missing entries without hiding bad "
    "references",
    "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto state = fixture.Capture();
  state.inventory_order.gems[0] = 2;
  state.inventory_order.gems[1] = 0;
  REQUIRE(Save::ValidateContent(state).has_value());
  const auto invalid = GENERATE(0, 1, 2, 3, 4, 5, 6);
  if (invalid == 0) state.inventory_order.gems[1] = 2;
  if (invalid == 1) state.inventory_order.gems[0] = 49;
  if (invalid == 2) state.inventory_order.gems[0] = 48;
  if (invalid == 3) state.inventory.gems[0].index = 0;
  if (invalid == 4) state.inventory.weapons[0].blade = 2;
  if (invalid == 5) state.inventory.weapons[0].gems[0] = 48;
  if (invalid == 6) state.inventory.weapons[0].gems[1] = 1;
  const auto before = LiveFingerprint(fixture);
  const auto result = Save::ValidateContent(state);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find(invalid < 4 ? "gems/slot " : "weapons/slot ") !=
        std::string::npos);
  CHECK(LiveFingerprint(fixture) == before);
}

namespace {
void ConsumeOrderedMiscItems() {
  auto& items = VS::Items::inventory.misc;
  auto& order = VS::Items::inventoryIndices.misc;
  items[9].id = 425;
  items[9].index = 10;
  items[9].count = 1;
  items[10].id = 432;
  items[10].index = 11;
  items[10].count = 1;
  items[11].id = 323;
  items[11].index = 12;
  items[11].count = 2;
  order[0] = 11;
  order[1] = 12;
  order[2] = 10;
  REQUIRE(VS::Items::ConsumeLiveMiscItem(432) == 1);
  REQUIRE(VS::Items::ConsumeLiveMiscItem(323) == 1);
}

void CheckNoStaleDisplayRows(const Save::Snapshot& state) {
  CHECK(state.inventory_order.weapons[7] == 0);
  CHECK(state.inventory_order.blades[15] == 0);
  CHECK(state.inventory_order.grips[15] == 0);
  CHECK(state.inventory_order.shields[7] == 0);
  CHECK(state.inventory_order.armor[15] == 0);
  CHECK(state.inventory_order.gems[47] == 0);
  CHECK(state.inventory_order.misc[63] == 0);
  CHECK(state.inventory_order.armor[1] == 2);
}

void CheckConsumedMiscResume(const Save::Snapshot& state) {
  const auto encoded = Save::Encode({1, 0, state});
  REQUIRE(encoded.has_value());
  const auto decoded = Save::Decode(*encoded);
  REQUIRE(decoded.has_value());
  REQUIRE(Save::PrepareResume(*decoded).has_value());
  CHECK(VS::Items::inventoryIndices.misc[0] == 12);
  CHECK(VS::Items::inventoryIndices.misc[1] == 10);
  CHECK(VS::Items::inventory.misc[10].id == 0);
}
}  // namespace

TEST_CASE("Runtime capture projects consumed misc items out of display order",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  ConsumeOrderedMiscItems();
  const auto before = LiveFingerprint(fixture);
  const auto state = fixture.Capture();
  CHECK(LiveFingerprint(fixture) == before);
  CHECK(state.inventory.misc[10].id == 0);
  CHECK(state.inventory.misc[11].count == 1);
  CHECK(state.inventory_order.misc[0] == 12);
  CHECK(state.inventory_order.misc[1] == 10);
  CHECK(state.inventory_order.misc[2] == 0);
  CheckConsumedMiscResume(state);
}

TEST_CASE("Runtime capture removes stale display rows in every category",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto& order = VS::Items::inventoryIndices;
  const auto add_stale = [](auto& category) {
    category[std::size(category) - 1] = std::size(category);
  };
  add_stale(order.weapons);
  add_stale(order.blades);
  add_stale(order.grips);
  add_stale(order.shields);
  add_stale(order.armor);
  add_stale(order.gems);
  add_stale(order.misc);
  const auto before = LiveFingerprint(fixture);
  const auto state = fixture.Capture();
  CHECK(LiveFingerprint(fixture) == before);
  CheckNoStaleDisplayRows(state);
}

TEST_CASE("Runtime capture does not repair corrupt display references",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto& order = VS::Items::inventoryIndices.misc;
  const auto defect = GENERATE(0, 1);
  order[0] = 64;
  order[1] = defect == 0 ? 65 : 64;
  const auto before = LiveFingerprint(fixture);
  const auto state = Save::Capture(fixture.storage);
  REQUIRE_FALSE(state.has_value());
  CHECK(state.error().find("inventory_order.misc") != std::string::npos);
  CHECK(LiveFingerprint(fixture) == before);
}

TEST_CASE("Runtime capture accepts horizontal motion frozen by a field menu",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  VS::Field::controlMode = GENERATE(5, 6);
  VS::Field::isInteractionPaused = 1;
  fixture.player_model.instance.turn_frames = 11;
  fixture.player_model.instance.root_motion.vx = -18;
  fixture.player_model.instance.root_motion.vz = 3;
  fixture.player_model.momentum_distance = 18;
  const auto before = LiveFingerprint(fixture);
  const auto state = fixture.Capture();
  CHECK(LiveFingerprint(fixture) == before);
  CheckCapturedLocation(state);
  const auto encoded = Save::Encode({1, 0, state});
  REQUIRE(encoded.has_value());
  const auto decoded = Save::Decode(*encoded);
  REQUIRE(decoded.has_value());
  CHECK(Save::ValidateContent(decoded->state).has_value());
}

TEST_CASE("Menu-paused capture still rejects airborne and supported motion",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  VS::Field::controlMode = GENERATE(5, 6);
  VS::Field::isInteractionPaused = 1;
  const auto defect = GENERATE(6, 7, 11, 12, 13, 14, 16, 18, 20, 21, 31, 32);
  InvalidateLiveFixture(fixture, defect);
  if (defect == 32) fixture.player_model.instance.root_motion.vy = 1;
  const auto before = LiveFingerprint(fixture);
  CHECK_FALSE(Save::Capture(fixture.storage).has_value());
  CHECK(LiveFingerprint(fixture) == before);
}

TEST_CASE("Only an actually paused field menu permits residual walking",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  const auto defect = GENERATE(0, 1, 2, 3);
  VS::Field::controlMode = defect == 0 ? 1 : (defect == 1 ? 3 : 5);
  VS::Field::isInteractionPaused = defect == 3 ? 0 : 1;
  fixture.player_model.instance.root_motion.vx = 18;
  if (defect == 2) fixture.player_model.support_gimmick = 1;
  const auto before = LiveFingerprint(fixture);
  CHECK_FALSE(Save::Capture(fixture.storage).has_value());
  CHECK(LiveFingerprint(fixture) == before);
}

namespace {
void CheckAudioSettings(bool mono) {
  CHECK((VS::Input::settings.mono_sound == 1) == mono);
  CHECK(((VS::Audio::Driver::controlLatches.mix_behavior & 2) != 0) == mono);
  CHECK((VS::Audio::Driver::controlLatches.mix_behavior & 0x10) != 0);
  CHECK(VS::Input::settings.slot_state == 0xFEDCBA98u);
  CHECK(VS::Input::settings.key == 0xF00DCAFEu);
}
}  // namespace

TEST_CASE(
    "Resume synchronizes the actual mono consumer through apply cancel and "
    "finish",
    "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto state = fixture.Capture();
  const bool mono = GENERATE(false, true);
  VS::Input::settings.mono_sound = static_cast<std::uint8_t>(!mono);
  VS::Audio::LiveSetMonoOutput(mono ? 0 : 1);
  VS::Audio::Driver::controlLatches.mix_behavior |= 0x10;
  const auto before = LiveFingerprint(fixture);
  state.settings.mono_sound = static_cast<std::uint8_t>(mono);
  state.settings.slot_state = 0xFEDCBA98u;
  state.settings.key = 0xF00DCAFEu;
  REQUIRE(Save::PrepareResume({1, 0, state}).has_value());
  CheckAudioSettings(mono);
  Save::CancelResume();
  CHECK(LiveFingerprint(fixture) == before);
  REQUIRE(Save::PrepareResume({1, 0, state}).has_value());
  VS::State::ashleySnapshot.stats.is_pending = 0;
  VS::Audio::LiveSetMonoOutput(mono ? 0 : 1);
  VS::Audio::Driver::controlLatches.mix_behavior |= 0x10;
  REQUIRE(Save::FinishResume().has_value());
  CheckAudioSettings(mono);
}

TEST_CASE("Uninitialized audio consumer cannot partially prepare a resume",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  const auto state = fixture.Capture();
  VS::Audio::Mixer::kCommandHandlers[0x81] = nullptr;
  const auto before = LiveFingerprint(fixture);
  const auto prepared = Save::PrepareResume({1, 0, state});
  REQUIRE_FALSE(prepared.has_value());
  CHECK(prepared.error().find("audio consumer") != std::string::npos);
  CHECK_FALSE(Save::ResumePending());
  CHECK(LiveFingerprint(fixture) == before);
}

TEST_CASE("Failed room-ready audio validation leaves cancellation safe",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto state = fixture.Capture();
  state.settings.mono_sound = 1;
  REQUIRE(Save::PrepareResume({1, 0, state}).has_value());
  VS::State::ashleySnapshot.stats.is_pending = 0;
  VS::Audio::Mixer::kCommandHandlers[0x81] = nullptr;
  const auto before = LiveFingerprint(fixture);
  REQUIRE_FALSE(Save::FinishResume().has_value());
  CHECK(LiveFingerprint(fixture) == before);
  Save::CancelResume();
  CHECK_FALSE(Save::ResumePending());
  CHECK((VS::Audio::Driver::controlLatches.mix_behavior & 2) == 0);
  CHECK(VS::Input::settings.mono_sound == 0);
}

TEST_CASE("Resume staging can be canceled repeatedly without leaking inputs",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto state = fixture.Capture();
  state.script_flags[64] = 177;
  state.learned_actions.fill(false);
  state.learned_actions[97] = true;
  const auto before = LiveFingerprint(fixture);
  for (int iteration = 0; iteration < 3; ++iteration) {
    REQUIRE(Save::PrepareResume({1, 0, state}).has_value());
    CheckStagedInputs();
    const auto staged = LiveFingerprint(fixture);
    CHECK_FALSE(Save::PrepareResume({1, 0, state}).has_value());
    CHECK(LiveFingerprint(fixture) == staged);
    CHECK_FALSE(Save::Capture(fixture.storage).has_value());
    Save::CancelResume();
    CHECK_FALSE(Save::ResumePending());
    CHECK(LiveFingerprint(fixture) == before);
    Save::CancelResume();
    CHECK(LiveFingerprint(fixture) == before);
  }
}

namespace {
void SimulateRoomEntryWork(RuntimeFixture& fixture) {
  // Exercise the finalization contract without running the original boot or
  // pretending these scalar fixture writes are controller acceptance tests.
  VS::State::ashleySnapshot.stats.is_pending = 0;
  fixture.player_model.instance.position = {64, 0, 64, 0};
  fixture.player_stats.current_hp = 7;
  fixture.player_stats.statuses = 0;
  VS::State::playTime = {};
  VS::State::sceneTimer = {};
  VS::Field::sphericalCamera.values.yaw = 0;
  VS::Core::SeedBiosRand(1);
  VS::Core::interfaceRandomState = 1;
  std::fill_n(VS::Core::randomShuffleTable, 97, 0);
  VS::Core::randomShuffleIndex = 0;
  VS::Battle::Ai::savedMovementRandomState = 1;
  VS::Battle::Ai::LiveFrameContext().randomState = 1;
  VS::State::SetLiveScriptFlag(64, 0);
  VS::State::recentRoomEnemyStates[0].next_slot = 3;
}

void CheckRestoredPlayer(const RuntimeFixture& fixture) {
  CHECK(fixture.player_stats.current_hp == 123);
  CHECK(fixture.player_stats.statuses == 0x21000);
  CHECK(fixture.player_stats.status_timers[7] == 255);
  CHECK(fixture.player_stats.limbs[5].types[3] == -29);
  CHECK(fixture.player_model.instance.position.vx == 193);
  CHECK(fixture.player_model.instance.position.vy == -73);
  CHECK(fixture.player_model.instance.position.vz == 325);
  CHECK(fixture.player_model.instance.facing == 2051);
  CHECK(fixture.player_model.instance.current_tile_x == 1);
  CHECK(fixture.player_model.instance.current_tile_z == 2);
}

void CheckRestoredClockCamera() {
  CHECK(VS::State::playTime.hours == 12);
  CHECK(VS::State::playTime.minutes == 57);
  CHECK(VS::State::playTime.seconds == 58);
  CHECK(VS::State::playTime.frames == 59);
  CHECK(VS::State::sceneTimer.time.frames == 29);
  CHECK(VS::State::sceneTimer.time.seconds == 17);
  CHECK(VS::Field::sphericalCamera.values.yaw == 1027);
  CHECK(VS::Render::FrameScratchpad().camera.angles.vy == 1027);
  CHECK(VS::Field::sphericalCamera.preferred_distance == 1536);
}

void CheckRestoredRandom(const Save::Snapshot& state) {
  CHECK(VS::Core::BiosRand().State() == state.random.bios_state);
  CHECK(VS::Core::interfaceRandomState == state.random.interface_state);
  CHECK(std::equal(state.random.shuffle.begin(), state.random.shuffle.end(),
                   VS::Core::randomShuffleTable));
  CHECK(VS::Core::randomShuffleIndex == 96);
  CHECK(VS::Battle::Ai::LiveFrameContext().randomState == 0xFEDCBA98);
  CHECK(std::bit_cast<unsigned>(VS::Battle::Ai::savedMovementRandomState) ==
        0xFEDCBA98);
}

void CheckRestoredCacheFlags() {
  CHECK(static_cast<unsigned char>(VS::State::LiveScriptFlag(64)) == 99);
  CHECK(VS::State::recentRoomEnemyStates[0].next_slot == 0);
  CHECK(static_cast<unsigned char>(
            VS::State::recentRoomEnemyStates[3].zone_id) == 12);
}

void CheckCompletedResumeStable(const RuntimeFixture& fixture,
                                const std::string& saved) {
  const auto recaptured = fixture.Capture();
  const auto reencoded = Save::Encode({1, 0, recaptured});
  REQUIRE(reencoded.has_value());
  CHECK(*reencoded == saved);
  const auto before = LiveFingerprint(fixture);
  CHECK_FALSE(Save::FinishResume().has_value());
  Save::CancelResume();
  CHECK(LiveFingerprint(fixture) == before);
}

void ConfigureStaticOrbit(RuntimeFixture& fixture, unsigned layer) {
  auto& model = fixture.player_model.instance;
  model.current_tile_layer = static_cast<unsigned char>(layer);
  model.previous_tile_x = 0;
  model.previous_tile_z = 1;
  model.previous_tile_layer = static_cast<unsigned char>(layer);
  model.previous_facing = -144;
  VS::Actors::TileTarget saved{};
  saved.tile_x = 2;
  saved.tile_z = 3;
  saved.tile_layer = layer;
  saved.facing = 77;
  fixture.player_model.saved_placement = std::bit_cast<int>(saved);
  VS::Field::sphericalCamera.values.pitch = -768;
  VS::Field::sphericalCamera.values.distance = 2304;
  VS::Field::sphericalCamera.values.mode = 17;
  VS::Field::sphericalCamera.motion.target_yaw = -512;
  VS::Field::sphericalCamera.motion.turn_speed = 32;
  VS::Field::sphericalCamera.motion.zoom_mode = 3;
  VS::Field::sphericalCamera.preferred_distance = 2304;
  VS::Field::sphericalCamera.zoom_velocity = 48;
  VS::Field::sphericalCamera.tracking_speed = 24;
  VS::Render::FrameScratchpad().camera.look_at = {111111, -333333, 222222, 0};
  VS::Render::FrameScratchpad().camera.angles = {-764, 1027, 0, 0};
  fixture.UpdateOrbitPosition();
}

void CheckStaticLayer(const RuntimeFixture& fixture,
                      const Save::Snapshot& state) {
  const auto& model = fixture.player_model.instance;
  CHECK(model.current_tile_layer == state.location.floor_layer);
  CHECK(model.previous_tile_layer == state.location.previous_tile.layer);
  CHECK(model.previous_tile_x == 0);
  CHECK(model.previous_tile_z == 1);
  CHECK(model.previous_facing == -144);
  const auto saved = std::bit_cast<VS::Actors::TileTarget>(
      fixture.player_model.saved_placement);
  CHECK(saved.tile_x == 2);
  CHECK(saved.tile_z == 3);
  CHECK(saved.tile_layer == state.location.floor_layer);
  CHECK(saved.facing == 77);
}

void CheckOrbitRestored(const VS::Field::SphericalCamera& expected,
                        const VS::Render::Camera& rendered) {
  CHECK(VS::Field::sphericalCamera == expected);
  CHECK(VS::Render::FrameScratchpad().camera.position.vx ==
        rendered.position.vx);
  CHECK(VS::Render::FrameScratchpad().camera.position.vy ==
        rendered.position.vy);
  CHECK(VS::Render::FrameScratchpad().camera.position.vz ==
        rendered.position.vz);
  CHECK(VS::Render::FrameScratchpad().camera.look_at.vx == rendered.look_at.vx);
  CHECK(VS::Render::FrameScratchpad().camera.look_at.vy == rendered.look_at.vy);
  CHECK(VS::Render::FrameScratchpad().camera.look_at.vz == rendered.look_at.vz);
  CHECK(VS::Render::FrameScratchpad().camera.angles.vx == rendered.angles.vx);
  CHECK(VS::Render::FrameScratchpad().camera.angles.vy == rendered.angles.vy);
}

void CheckCanceledStaticResume(const RuntimeFixture& fixture,
                               const Save::Document& document,
                               const std::vector<std::uint8_t>& before) {
  REQUIRE(Save::PrepareResume(document).has_value());
  CHECK(VS::State::savedEntryDoor.tile_location.entry_surface ==
        document.state.location.floor_layer);
  Save::CancelResume();
  CHECK(LiveFingerprint(fixture) == before);
}
}  // namespace

TEST_CASE("Schema3 resumes both static floors and a settled nondefault orbit",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  ConfigureStaticOrbit(fixture, GENERATE(0u, 1u));
  const auto before = LiveFingerprint(fixture);
  const auto camera = VS::Field::sphericalCamera;
  const auto rendered = VS::Render::FrameScratchpad().camera;
  const auto state = fixture.Capture();
  CHECK(LiveFingerprint(fixture) == before);
  const auto encoded = Save::Encode({1, 0, state});
  REQUIRE(encoded.has_value());
  const auto decoded = Save::Decode(*encoded);
  REQUIRE(decoded.has_value());
  CheckCanceledStaticResume(fixture, *decoded, before);
  for (int repeat = 0; repeat < 2; ++repeat) {
    REQUIRE(Save::PrepareResume(*decoded).has_value());
    SimulateRoomEntryWork(fixture);
    fixture.player_model.instance.current_tile_layer = 0;
    fixture.player_model.instance.previous_tile_layer = 0;
    fixture.player_model.saved_placement = 0;
    VS::Field::sphericalCamera = {};
    VS::Render::FrameScratchpad().camera.look_at = {};
    REQUIRE(Save::FinishResume().has_value());
    CheckStaticLayer(fixture, state);
    CheckOrbitRestored(camera, rendered);
    CheckCompletedResumeStable(fixture, *encoded);
  }
}

TEST_CASE("Room-ready resume restores exact clocks camera RNG and cached state",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  fixture.AddEnemy();
  auto state = fixture.Capture();
  for (unsigned index = 1; index < 4; ++index) {
    state.room_cache[index] = state.room_cache[0];
    state.room_cache[index].zone = static_cast<std::uint8_t>(9 + index);
    state.room_cache[index].recency = static_cast<std::uint8_t>(4 - index);
  }
  const auto saved = Save::Encode({1, 0, state});
  REQUIRE(saved.has_value());
  const auto decoded = Save::Decode(*saved);
  REQUIRE(decoded.has_value());
  for (int iteration = 0; iteration < 2; ++iteration) {
    REQUIRE(Save::PrepareResume(*decoded).has_value());
    SimulateRoomEntryWork(fixture);
    REQUIRE(Save::FinishResume().has_value());
    CHECK_FALSE(Save::ResumePending());
    CheckRestoredPlayer(fixture);
    CheckRestoredClockCamera();
    CheckRestoredRandom(state);
    CheckRestoredCacheFlags();
    CheckCompletedResumeStable(fixture, *saved);
  }
}

TEST_CASE("Wrong room or unfinished spawn cannot partially finish a resume",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  const auto state = fixture.Capture();
  REQUIRE(Save::PrepareResume({1, 0, state}).has_value());
  const auto defect = GENERATE(0, 1, 2, 3, 4);
  VS::State::ashleySnapshot.stats.is_pending = 0;
  if (defect == 0) VS::State::ashleySnapshot.stats.is_pending = 1;
  if (defect == 1) VS::Field::zoneContext.room_id = 1;
  if (defect == 2) VS::Field::zoneContext.scene_id = 38;
  if (defect == 3) VS::Field::roomData.is_loaded = 0;
  if (defect == 4) VS::Boot::isGameplayActive = 0;
  const auto before = LiveFingerprint(fixture);
  CHECK_FALSE(Save::FinishResume().has_value());
  CHECK(Save::ResumePending());
  CHECK(LiveFingerprint(fixture) == before);
  Save::CancelResume();
  CHECK_FALSE(Save::ResumePending());
}

namespace {
void CheckClearProgress(const Save::Document& document) {
  CHECK(document.new_game_entry);
  CHECK(document.state.script_flags[0] == 1);
  CHECK(document.state.script_flags[1] == 1);
  CHECK(document.state.script_flags[64] == 0);
  CHECK(document.state.settings.current_game_save_count == 0);
  CHECK(document.state.settings.cumulative_save_count == 19);
  CHECK(document.state.settings.menu_flags == 0xFFDF);
  CHECK(document.state.play_ticks == 0);
}

void CheckClearCarryover(const Save::Document& document) {
  CHECK(document.state.ashley.current_hp == 123);
  CHECK(document.state.ashley.current_mp == 31);
  CHECK(document.state.inventory.misc[0].id == 0);
  CHECK(document.state.inventory_order.misc[0] == 0);
  CHECK(document.state.inventory.weapons[0].blade == 1);
  CHECK(document.state.score.completion_time_minutes == 12 * 60 + 57);
  CHECK(document.state.arts.kills.total_kills == 1234);
  CHECK(document.state.learned_actions[0]);
  // The packed original capture differs deliberately from the now-unavailable
  // overlay's action-table view.
  CHECK(document.state.learned_actions[1]);
}

void CheckResumeStaged() {
  CHECK(VS::State::savedEntryDoor.destination_zone_id == 1);
  CHECK(VS::State::savedEntryDoor.entry_tile_x == 3);
  CHECK(VS::State::savedEntryDoor.entry_tile_z == 2);
  CHECK(VS::State::savedEntryDoor.tile_location.entry_surface == 1);
  CHECK(VS::State::savedEntryDoor.required_flag_id == 37);
  CHECK(VS::State::ashleySnapshot.stats.is_pending == 1);
}
}  // namespace

TEST_CASE("Ending component capture preserves carryover without live actors",
          "[save][runtime-snapshot][save-clear]") {
  RuntimeFixture fixture;
  const auto document = fixture.ClearGame();
  CheckClearProgress(document);
  CheckClearCarryover(document);
  VS::Battle::characterState = nullptr;
  VS::Battle::actors[0] = nullptr;
  VS::Actors::actorRecords[0] = nullptr;
  VS::Field::roomData = {};
  const auto before = LiveFingerprint(fixture);
  const auto captured = Save::CaptureClearGame();
  REQUIRE(captured);
  CHECK(Save::SameDocument(document, *captured) == true);
  CHECK(LiveFingerprint(fixture) == before);
}

namespace {
// The value of an optional a test has established must be present.
template <class Optional>
auto& Engaged(Optional& optional) {
  if (!optional.has_value()) throw std::runtime_error("expected a value");
  return *optional;
}

void CheckResumeCancels(RuntimeFixture& fixture,
                        const Save::Document& decoded) {
  const auto before = LiveFingerprint(fixture);
  for (int iteration = 0; iteration < 2; ++iteration) {
    REQUIRE(Save::PrepareResume(decoded));
    CheckResumeStaged();
    Save::CancelResume();
    CHECK_FALSE(Save::ResumePending());
    CHECK(LiveFingerprint(fixture) == before);
  }
}

void CheckResumeRestoresAshley(RuntimeFixture& fixture) {
  // Component setup only: the production spawn restoration consumes the
  // actual persistent Ashley data; this is not an ending playthrough.
  VS::Actors::RestoreLiveAshleyRuntimeState();
  CHECK(fixture.player_stats.current_hp == 123);
  CHECK(fixture.player_stats.current_mp == 31);
  CHECK(VS::State::ashleySnapshot.stats.is_pending == 0);
  const auto staged = LiveFingerprint(fixture);
  CHECK_FALSE(
      Save::FinishResume());  // Old campaign room is not the start room.
  CHECK(LiveFingerprint(fixture) == staged);
}

void CheckResumeKeptLiveState(const RuntimeFixture& fixture) {
  CHECK(VS::State::playTime.frames == 8);
  CHECK(fixture.player_model.instance.position.vx == 77);
  CHECK(fixture.player_model.instance.position.vy == -88);
  CHECK(VS::Field::sphericalCamera.values.yaw == 333);
  CHECK(VS::Core::BiosRand().State() == 777);
  CHECK(VS::State::recentRoomEnemyStates[0].next_slot == 5);
}
}  // namespace

TEST_CASE("Clear resume uses original spawn and is cancelable and repeatable",
          "[save][runtime-snapshot][save-clear]") {
  RuntimeFixture fixture;
  const auto document = fixture.ClearGame();
  const auto encoded = Save::Encode(document);
  REQUIRE(encoded);
  const auto decoded = Save::Decode(*encoded);
  REQUIRE(decoded);
  CheckResumeCancels(fixture, *decoded);
  REQUIRE(Save::PrepareResume(*decoded));
  CheckResumeRestoresAshley(fixture);
  VS::Boot::isGameplayActive = 1;
  VS::Field::zoneContext.zone_id = 1;
  VS::State::recentRoomEnemyStates[0].next_slot = 5;
  VS::State::playTime.frames = 8;
  fixture.player_model.instance.position = {77, -88, 99, 0};
  VS::Field::sphericalCamera.values.yaw = 333;
  VS::Core::SeedBiosRand(777);
  REQUIRE(Save::FinishResume());
  CHECK_FALSE(Save::ResumePending());
  CheckResumeKeptLiveState(fixture);
  CHECK_FALSE(Save::FinishResume());
}

TEST_CASE("Original ending advances clear difficulty and saturates its count",
          "[save][runtime-snapshot][save-clear]") {
  RuntimeFixture fixture;
  const auto previous_clears = GENERATE(0, 1, 98, 99);
  VS::State::stateFlags.clear_count = static_cast<char>(previous_clears);
  VS::State::stateFlags.difficulty = previous_clears == 0 ? 0 : 1;
  const auto document = fixture.ClearGame();
  CHECK(document.state.script_flags[0] == std::min(previous_clears + 1, 99));
  CHECK(document.state.script_flags[1] == 1);
  CHECK(document.state.script_flags[2] == (previous_clears == 0 ? 0 : 1));
  CHECK(document.state.score.clear_count == previous_clears);
}

TEST_CASE("Clear-game validation rejects unreset state and missing content",
          "[save][runtime-snapshot][save-clear]") {
  RuntimeFixture fixture;
  auto document = fixture.ClearGame();
  const auto defect = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  if (defect == 0) document.state.script_flags[0] = 0;
  if (defect == 1) document.state.script_flags[64] = 1;
  if (defect == 2) document.state.settings.current_game_save_count = 1;
  if (defect == 3) document.state.play_ticks = 1;
  if (defect == 4) document.state.inventory.misc[0].id = 308;
  if (defect == 5) Engaged(document.new_game_entry).destination_zone_id = 9;
  if (defect == 6) document.state.visits.room_flags[0] = 1;
  if (defect == 7) {
    REQUIRE(std::filesystem::remove(fixture.content.root / "disc" / "MAP" /
                                    "ZONE001.ZND"));
  }
  if (defect == 8) Engaged(document.new_game_entry).entry_tile_x = 31;
  if (defect == 9) {
    Engaged(document.new_game_entry).tile_location.entry_surface = 2;
  }
  const auto before = LiveFingerprint(fixture);
  CHECK_FALSE(Save::ValidateContent(document));
  CHECK_FALSE(Save::PrepareResume(document));
  CHECK_FALSE(Save::ResumePending());
  CHECK(LiveFingerprint(fixture) == before);
}
