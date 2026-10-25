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
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
#include "Core/BiosRand.h"
#include "Save/RuntimeSnapshot.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundBanks.h"
#include "Battle/Abilities.h"
#include "Battle/ActionTable.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Damage.h"
#include "Boot/GameLoop.h"
#include "Core/Random.h"
#include "Field/Camera.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoading.h"
#include "Field/Znd.h"
#include "Input/Controller.h"
#include "Items/Categories.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Render/Camera.h"
#include "Render/UiPrimitives.h"
#include "State/Flags.h"
#include "State/Location.h"
#include "State/PlayTime.h"
#include "State/RoomMemory.h"
#include "State/Timers.h"
#include "Title/NewGame.h"
}

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
    lengths[1] = sizeof(Field_MpdCollisionHeader) + kSide * kSide * 4 + 16;
    // A short region table is valid, independent of collision grid area.
    lengths[2] = 3;
    lengths[10] = sizeof(Field_MpdEnvironment);
    Field_MpdHeader header{};
    header.roomSectionOffset = sizeof(header);
    Put(room, sizeof(header), lengths);
    collision_offset = sizeof(header) + sizeof(lengths);
    Field_MpdCollisionHeader collision{kSide, kSide, 0, 1};
    Put(room, collision_offset, collision);
    room.resize(collision_offset + lengths[1] + lengths[2], 0);
    environment_offset = room.size();
    Field_MpdEnvironment environment{};
    environment.sceneId = 37;
    Put(room, environment_offset, environment);
    header.enemySectionOffset = static_cast<int>(room.size());
    header.enemySectionSize = 8 * sizeof(Field_MpdEnemySpawn);
    room.resize(room.size() + header.enemySectionSize);
    Put(room, 0, header);
    if (room.size() < minimum_room_bytes) room.resize(minimum_room_bytes);
    WriteRoom();
  }

  void BuildIndex() {
    for (unsigned index = 0; index < Content_moduleDataCount; ++index) {
      const auto& slice = Content_moduleData[index];
      if (slice.target != &Items_itemCategoryUpperIds) continue;
      categories_offset = slice.offset;
      categories_module = slice.module;
      break;
    }
    REQUIRE_FALSE(categories_module.empty());
    std::vector<std::uint8_t> battle;
    Put(battle, categories_offset, kBounds);
    Write(root / "disc" / categories_module, battle);

    Field_ZndHeader zone{};
    zone.mpdListOffset = sizeof(zone);
    zone.mpdListSize = 8;
    std::vector<std::uint8_t> zone_bytes;
    Put(zone_bytes, 0, zone);
    Put(zone_bytes, sizeof(zone), std::uint32_t{200});
    const auto sector_bytes = Content::DataStore::kDataSectorBytes;
    const auto allocation =
        ((room.size() + sector_bytes - 1) / sector_bytes) * sector_bytes;
    Put(zone_bytes, sizeof(zone) + 4, static_cast<std::uint32_t>(allocation));
    for (const auto name :
         {"ZONE009.ZND", "ZONE010.ZND", "ZONE011.ZND", "ZONE012.ZND"}) {
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
    for (const auto name :
         {"ZONE009.ZND", "ZONE010.ZND", "ZONE011.ZND", "ZONE012.ZND"}) {
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
    Field_ZndHeader zone{};
    zone.mpdListOffset = sizeof(zone);
    zone.mpdListSize = 8;
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
  decltype(Audio_Seq_unusedStateFlags) flags = Audio_Seq_unusedStateFlags;
  decltype(Audio_Seq_unusedInventory) inventory = Audio_Seq_unusedInventory;
  decltype(Audio_Seq_unusedInventoryIndices) order =
      Audio_Seq_unusedInventoryIndices;
  decltype(Input_settings) settings = Input_settings;
  decltype(State_roomVisitProgress) visits = State_roomVisitProgress;
  decltype(Audio_Seq_unusedArtsStatus) arts = Audio_Seq_unusedArtsStatus;
  decltype(Audio_Seq_unusedScoredata) score = Audio_Seq_unusedScoredata;
  decltype(State_ashleySnapshot) ashley = State_ashleySnapshot;
  decltype(State_savedEntryDoor) entry = State_savedEntryDoor;
  decltype(Field_snapshotDoorFlagSource) detached_scene =
      Field_snapshotDoorFlagSource;
  decltype(State_playTime) time = State_playTime;
  decltype(State_sceneTimer) timer = State_sceneTimer;
  decltype(Field_zoneContext) zone = Field_zoneContext;
  decltype(Field_roomData) room = Field_roomData;
  decltype(Field_sphericalCamera) camera = Field_sphericalCamera;
  decltype(Render_frameScratch.camera) rendered_camera =
      Render_frameScratch.camera;
  decltype(std::to_array(State_recentRoomEnemyStates)) caches =
      std::to_array(State_recentRoomEnemyStates);
  decltype(std::to_array(State_learnedActionFlags)) learned =
      std::to_array(State_learnedActionFlags);
  decltype(std::to_array(Battle_actionDefinitions)) actions =
      std::to_array(Battle_actionDefinitions);
  decltype(std::to_array(Battle_actors)) actors = std::to_array(Battle_actors);
  decltype(std::to_array(Actors_actorRecords)) models =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Battle_Ai_actorAiStates)) ai =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Core_randomShuffleTable)) shuffle =
      std::to_array(Core_randomShuffleTable);
  decltype(std::to_array(Items_itemCategoryUpperIds)) bounds =
      std::to_array(Items_itemCategoryUpperIds);
  Battle_Actor* character = Battle_characterState;
  unsigned bios = Core_Random_State();
  unsigned interface_random = Core_interfaceRandomState;
  int shuffle_index = Core_randomShuffleIndex;
  int movement_random = Battle_Ai_savedMovementRandomState;
  unsigned frame_random = Battle_Ai_FrameState()->randomState;
  int current_cache = State_currentRoomStateSlot;
  int gameplay = Boot_isGameplayActive;
  int near_clip = Render_nearClip;
  int projection = Render_projectionDistance;
  int first_person = Field_isFirstPersonViewEnabled;
  Field_CameraTransition* transition = Field_cameraTransition;
  unsigned scratch = Actors_stateFlagsScratch;
  Audio_DriverFlags audio_flags = Audio_Driver_controlLatches;
  Audio_CommandArguments audio_arguments = Audio_Driver_commandArguments;
  Audio_CommandParams audio_params = Audio_Mixer_commandParams;
  Audio_MusicConfig* active_music = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* secondary_music = Audio_Seq_secondaryMusicConfig;
  Audio_CommandHandler stereo_handler = Audio_Mixer_kCommandHandlers[0x80];
  Audio_CommandHandler mono_handler = Audio_Mixer_kCommandHandlers[0x81];
  unsigned active_effect_voices = Audio_Mixer_voiceScheduler.activeVoices;
  Audio_MusicConfig silent_music{};
  Battle_Actor player{};
  Battle_Actor enemy{};
  Battle_ActorStats player_stats{};
  Battle_ActorStats enemy_stats{};
  Actors_ActorRecord player_model{};
  Actors_ActorRecord enemy_model{};
  Battle_Ai_ActorAiState enemy_ai{};
  Field_MpdEnvironment environment{};
  Field_CameraTransition camera_transition{};
  Items_ContainerData storage{};

  RuntimeFixture() {
    REQUIRE_FALSE(Save::ResumePending());
    std::copy(kBounds.begin(), kBounds.end(), Items_itemCategoryUpperIds);
    std::fill_n(Battle_actors, actors.size(), nullptr);
    std::fill_n(Actors_actorRecords, models.size(), nullptr);
    std::fill_n(Battle_Ai_actorAiStates, ai.size(), nullptr);
    Battle_characterState = &player;
    Battle_actors[0] = &player;
    Actors_actorRecords[0] = &player_model;
    // The three-byte textureRowOffsets union member does not initialize the
    // high byte of momentumDistance. Match Actors_PlaceActorInRoom's reset.
    Actors_ResetActorRunMomentum(0);
    player.stats = &player_stats;
    player.model = &player_model.instance;
    player.roomSpawnIndex = 0x80;
    player_model.instance.position = {193, -73, 325, 0};
    player_model.instance.facing = 2051;
    player_model.instance.currentTileX = 1;
    player_model.instance.currentTileZ = 2;
    player_model.instance.previousTileX = 1;
    player_model.instance.previousTileZ = 2;
    player_model.instance.previousFacing = 2016;
    Actors_TileTarget grounded{};
    grounded.tileX = 1;
    grounded.tileZ = 2;
    grounded.facing = 128;
    player_model.savedPlacement = std::bit_cast<int>(grounded);
    player_stats.currentHp = 123;
    player_stats.maxHp = 300;
    player_stats.currentMp = 31;
    player_stats.maxMp = 77;
    player_stats.strength = 300;
    player_stats.totalStrength = 330;
    player_stats.intelligence = 222;
    player_stats.totalIntelligence = 225;
    player_stats.agility = 111;
    player_stats.totalAgility = 119;
    player_stats.risk = 77;
    player_stats.actionRecoveryTimer = 19;
    player_stats.statuses = 0x21000;
    player_stats.statusTimers[7] = 255;
    for (auto& limb : player_stats.limbs) {
      limb.hp = 45;
      limb.maxHp = 100;
      limb.types[3] = -29;
      limb.affinities[7] = -32768;
    }
    Audio_Seq_unusedInventory = {};
    Audio_Seq_unusedInventoryIndices = {};
    SetEquipment();
    SetProgress();
    SetWorld();
    SetAudio();
    SetClocksAndCaches();
    SetStorage();
  }

  void SetProgress() {
    Audio_Seq_unusedStateFlags = {};
    State_SetScriptFlag(0x43F, static_cast<char>(0xAB));
    State_SetScriptFlag(64, 99);
    State_SetScriptFlag(161, 17);
    Input_settings = {};
    Input_settings.mappedChainAbilities[3] = static_cast<char>(255);
    State_roomVisitProgress = {};
    State_roomVisitProgress.roomFlags[3] = 0x10203040;
    Audio_Seq_unusedArtsStatus = {};
    Audio_Seq_unusedArtsStatus.kills.totalKills = 1234;
    Audio_Seq_unusedScoredata = {};
    Audio_Seq_unusedScoredata.bossTimeTrialScores[7][2].time = 0xABCDEF;
    std::fill_n(State_learnedActionFlags, learned.size(),
                static_cast<char>(0xFF));
    for (auto& action : Battle_actionDefinitions) action.unlocked = 0;
    for (const auto index : {0, 7, 8, 97, 255}) {
      Battle_actionDefinitions[index].unlocked = 1;
    }
  }

  void SetWorld() {
    Field_isFirstPersonViewEnabled = 0;
    Field_cameraTransition = nullptr;
    Field_zoneContext = {};
    Field_zoneContext.zndId = 9;
    Field_zoneContext.mapId = 0;
    Field_zoneContext.currentSceneId = 37;
    Field_zoneContext.bounds = {0, 0, 4, 4};
    environment.sceneId = 37;
    Field_roomData = {};
    Field_roomData.isLoaded = 1;
    Field_roomData.environment = &environment;
    Field_sphericalCamera = {};
    Field_sphericalCamera.values.yaw = 1027;
    Field_sphericalCamera.values.pitch = -384;
    Field_sphericalCamera.values.distance = 1536;
    Field_sphericalCamera.delta.pitch = 2050;
    Field_sphericalCamera.initialDistance = 1536;
    Field_sphericalCamera.delta.mode = 1;
    Render_nearClip = 64;
    Render_projectionDistance = 512;
    Render_frameScratch.camera = {};
    Render_frameScratch.camera.farClip = ONE;
    Render_frameScratch.camera.lookAt = {100000, -200000, 300000, 0};
    Render_frameScratch.camera.angles = {-384, 1027, 0, 0};
    UpdateOrbitPosition();
  }

  void UpdateOrbitPosition() {
    Field_SetCameraPositionFromAngles(&Field_sphericalCamera.values);
  }

  void SetAudio() {
    Audio_Seq_activeMusicConfig = &silent_music;
    Audio_Seq_secondaryMusicConfig = nullptr;
    Audio_Mixer_voiceScheduler.activeVoices = 0;
    Audio_Mixer_kCommandHandlers[0x80] = Audio_Mixer_EnableStereoOutput;
    Audio_Mixer_kCommandHandlers[0x81] = Audio_Mixer_EnableMonoOutput;
    Audio_SetMonoOutput(0);
  }

  void SetClocksAndCaches() {
    State_playTime.t = {59, 58, 57, 12};
    State_sceneTimer = {};
    State_sceneTimer.mode = 2;
    State_sceneTimer.resetScope = 2;
    State_sceneTimer.seconds = 17;
    State_sceneTimer.frames = 29;
    Core_Random_Seed(0x12345678);
    Core_interfaceRandomState = 0x87654321;
    Core_randomShuffleIndex = 96;
    for (std::size_t index = 0; index < shuffle.size(); ++index) {
      Core_randomShuffleTable[index] = static_cast<int>(index) * 337;
    }
    Battle_Ai_savedMovementRandomState = std::bit_cast<int>(0xFEDCBA98u);
    State_currentRoomStateSlot = 0;
    for (auto& cache : State_recentRoomEnemyStates) cache = {};
    State_recentRoomEnemyStates[0].zoneId = 9;
    State_recentRoomEnemyStates[0].mapId = 0;
    State_recentRoomEnemyStates[0].recency = 4;
    State_recentRoomEnemyStates[0].nextSlot = 7;
    Boot_isGameplayActive = 1;
    Actors_stateFlagsScratch = 0x456789;
  }

  void SetStorage() {
    storage.misc[255].id = 511;
    storage.misc[255].count = 99;
    auto& stored_weapon = storage.weapons[31];
    stored_weapon.index = 32;
    stored_weapon.blade = stored_weapon.grip = 64;
    stored_weapon.gems[0] = 192;
    storage.blades[63].id = 95;
    storage.blades[63].wepId = 1;
    storage.blades[63].assembledWeaponIndex = 32;
    storage.grips[63].id = 126;
    storage.grips[63].gemSlots = 1;
    storage.grips[63].assembledWeaponIndex = 32;
    storage.gems[191].id = 307;
    storage.gems[191].setItemIndex = 32;
    storage.gems[191].index = 192;
  }

  void SetEquipment() {
    auto& items = Audio_Seq_unusedInventory;
    auto& blade = items.blades[0];
    blade.id = 14;
    blade.index = 1;
    blade.wepId = 1;
    blade.category = 3;
    blade.assembledWeaponIndex = 1;
    blade.maxDp = 500;
    blade.currentDp = 400;
    blade.maxPp = 200;
    blade.currentPp = 150;
    auto& grip = items.grips[0];
    grip.id = 96;
    grip.index = 1;
    grip.gemSlots = 3;
    grip.assembledWeaponIndex = 1;
    auto& weapon = items.weapons[0];
    weapon.index = 1;
    weapon.isEquipped = 1;
    weapon.blade = weapon.grip = 1;
    weapon.gems[0] = 1;
    auto& shield = items.shields[0];
    shield.index = shield.isEquipped = 1;
    shield.base.id = 127;
    shield.base.wepId = 2;
    shield.base.gemSlots = 1;
    shield.base.maxDp = shield.base.currentDp = 300;
    shield.base.maxPp = shield.base.currentPp = 100;
    shield.gems[0] = 2;
    items.gems[0].id = 261;
    items.gems[0].index = 1;
    items.gems[0].setItemIndex = 1;
    items.gems[1].id = 262;
    items.gems[1].index = 2;
    items.gems[1].setItemIndex = 0x81;
    auto& armor = items.armor[0];
    armor.id = 140;
    armor.index = armor.limb = 1;
    armor.maxDp = armor.currentDp = 500;
    auto& accessory = items.armor[1];
    accessory.id = 150;
    accessory.category = 7;
    accessory.index = 2;
    accessory.limb = 7;
    items.misc[0].id = 308;
    items.misc[0].count = 2;
    items.misc[0].index = 1;
    auto& indices = Audio_Seq_unusedInventoryIndices;
    indices.weapons[0] = indices.shields[0] = indices.blades[0] = 1;
    indices.grips[0] = indices.armor[0] = indices.gems[0] = indices.misc[0] = 1;
    indices.armor[1] = indices.gems[1] = 2;
    auto& runtime_weapon = player_stats.weapon;
    runtime_weapon.index = 1;
    Items_ReadInventoryBladeStats(&runtime_weapon.blade, &blade);
    Items_ReadInventoryGripStats(&runtime_weapon.grip, &grip);
    Items_ReadInventoryGemStats(&runtime_weapon.gems[0], &items.gems[0]);
    runtime_weapon.currentDp = 101;
    runtime_weapon.currentPp = 67;
    runtime_weapon.name[3] = static_cast<char>(0xE7);
    player_stats.shield.index = 1;
    Items_ReadInventoryArmorStats(&player_stats.shield.base, &shield.base);
    Items_ReadInventoryGemStats(&player_stats.shield.gems[0], &items.gems[1]);
    player_stats.shield.currentDp = 201;
    player_stats.shield.currentPp = 37;
    auto& runtime_armor = player_stats.limbs[0].armor;
    runtime_armor.index = 1;
    Items_ReadInventoryArmorStats(&runtime_armor.armor, &armor);
    runtime_armor.currentDp = 321;
    player_stats.accessory.index = 2;
    Items_ReadInventoryArmorStats(&player_stats.accessory.accessory,
                                  &accessory);
  }

  void AddEnemy() {
    Battle_actors[2] = &enemy;
    Actors_actorRecords[2] = &enemy_model;
    Battle_Ai_actorAiStates[2] = &enemy_ai;
    enemy.roomSpawnIndex = 0;
    enemy.stats = &enemy_stats;
    enemy.model = &enemy_model.instance;
    enemy_stats.currentHp = 43;
    enemy_stats.maxHp = 90;
    enemy_stats.currentMp = 12;
    enemy_stats.maxMp = 44;
    enemy_stats.totalStrength = 123;
    enemy_stats.totalIntelligence = 234;
    enemy_stats.totalAgility = 345;
    enemy_stats.limbs[5].hp = 32;
    enemy_model.instance.facing = 2864;
    Actors_TileTarget placement{};
    placement.tileX = 2;
    placement.tileLayer = 1;
    placement.tileZ = 3;
    enemy_model.savedPlacement = std::bit_cast<int>(placement);
    enemy_ai.usesRoomActionDelay = 1;
    enemy_ai.behaviorMode = 0;
    enemy_ai.relations[0].classification = 3;
    enemy_ai.relations[0].previousClassification = 2;
    enemy_ai.relations[0].classificationTimer = 77;
  }

  Save::Snapshot Capture() const {
    const auto result = Save::Capture(storage);
    INFO((result ? "" : result.error()));
    INFO("momentumDistance=" << player_model.momentumDistance
                             << ", transitionFrames="
                             << player_model.transitionFrames);
    REQUIRE(result.has_value());
    return *result;
  }

  ~RuntimeFixture() {
    Save::CancelResume();
    Audio_Seq_unusedStateFlags = flags;
    Audio_Seq_unusedInventory = inventory;
    Audio_Seq_unusedInventoryIndices = order;
    Input_settings = settings;
    State_roomVisitProgress = visits;
    Audio_Seq_unusedArtsStatus = arts;
    Audio_Seq_unusedScoredata = score;
    State_ashleySnapshot = ashley;
    State_savedEntryDoor = entry;
    Field_snapshotDoorFlagSource = detached_scene;
    State_playTime = time;
    State_sceneTimer = timer;
    Field_zoneContext = zone;
    Field_roomData = room;
    Field_sphericalCamera = camera;
    Render_frameScratch.camera = rendered_camera;
    std::copy(caches.begin(), caches.end(), State_recentRoomEnemyStates);
    std::copy(learned.begin(), learned.end(), State_learnedActionFlags);
    std::copy(actions.begin(), actions.end(), Battle_actionDefinitions);
    std::copy(actors.begin(), actors.end(), Battle_actors);
    std::copy(models.begin(), models.end(), Actors_actorRecords);
    std::copy(ai.begin(), ai.end(), Battle_Ai_actorAiStates);
    std::copy(shuffle.begin(), shuffle.end(), Core_randomShuffleTable);
    std::copy(bounds.begin(), bounds.end(), Items_itemCategoryUpperIds);
    Battle_characterState = character;
    Core_Random_Seed(bios);
    Core_interfaceRandomState = interface_random;
    Core_randomShuffleIndex = shuffle_index;
    Battle_Ai_savedMovementRandomState = movement_random;
    Battle_Ai_FrameState()->randomState = frame_random;
    State_currentRoomStateSlot = current_cache;
    Boot_isGameplayActive = gameplay;
    Render_nearClip = near_clip;
    Render_projectionDistance = projection;
    Field_isFirstPersonViewEnabled = first_person;
    Field_cameraTransition = transition;
    Actors_stateFlagsScratch = scratch;
    Audio_Driver_controlLatches = audio_flags;
    Audio_Driver_commandArguments = audio_arguments;
    Audio_Mixer_commandParams = audio_params;
    Audio_Seq_activeMusicConfig = active_music;
    Audio_Seq_secondaryMusicConfig = secondary_music;
    Audio_Mixer_kCommandHandlers[0x80] = stereo_handler;
    Audio_Mixer_kCommandHandlers[0x81] = mono_handler;
    Audio_Mixer_voiceScheduler.activeVoices = active_effect_voices;
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
  Append(bytes, Audio_Seq_unusedStateFlags);
  Append(bytes, Audio_Seq_unusedInventory);
  Append(bytes, Audio_Seq_unusedInventoryIndices);
  Append(bytes, Input_settings);
  Append(bytes, State_roomVisitProgress);
  Append(bytes, Audio_Seq_unusedArtsStatus);
  Append(bytes, Audio_Seq_unusedScoredata);
  Append(bytes, State_recentRoomEnemyStates);
  Append(bytes, State_currentRoomStateSlot);
  Append(bytes, State_learnedActionFlags);
  Append(bytes, Battle_actionDefinitions);
  Append(bytes, State_ashleySnapshot);
  Append(bytes, State_savedEntryDoor);
  Append(bytes, State_playTime);
  Append(bytes, State_sceneTimer);
  Append(bytes, Field_sphericalCamera);
  Append(bytes, Render_frameScratch.camera);
  Append(bytes, Field_zoneContext);
  Append(bytes, Field_roomData);
  Append(bytes, Field_isFirstPersonViewEnabled);
  Append(bytes, Field_cameraTransition != nullptr);
  Append(bytes, Render_nearClip);
  Append(bytes, Render_projectionDistance);
  Append(bytes, fixture.player_stats);
  Append(bytes, fixture.player_model);
  Append(bytes, fixture.enemy_stats);
  Append(bytes, fixture.enemy_model);
  Append(bytes, fixture.enemy_ai);
  Append(bytes, fixture.storage);
  Append(bytes, Core_Random_State());
  Append(bytes, Core_interfaceRandomState);
  Append(bytes, Core_randomShuffleTable);
  Append(bytes, Core_randomShuffleIndex);
  Append(bytes, Battle_Ai_savedMovementRandomState);
  Append(bytes, Battle_Ai_FrameState()->randomState);
  Append(bytes, Actors_stateFlagsScratch);
  Append(bytes, Audio_Driver_controlLatches.mixBehavior);
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
  CHECK(state.ashley.currentHp == 123);
  CHECK(state.ashley.totalStrength == 330);
  CHECK(state.ashley.statusTimers[7] == static_cast<char>(255));
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
  CHECK(state.inventory.blades[0].currentDp == 101);
  CHECK(state.inventory.blades[0].currentPp == 67);
  CHECK(state.inventory.shields[0].base.currentDp == 201);
  CHECK(state.inventory.shields[0].base.currentPp == 37);
  CHECK(state.inventory.armor[0].currentDp == 321);
  CHECK(state.inventory.weapons[0].name[3] == static_cast<char>(0xE7));
  CHECK(state.inventory_order.armor[1] == 2);
}

void CheckCapturedProgress(const Save::Snapshot& state) {
  CHECK(state.storage.misc[255].id == 511);
  CHECK(state.storage.weapons[31].gems[0] == 192);
  CHECK(state.storage.gems[191].id == 307);
  CHECK(state.visits.roomFlags[3] == 0x10203040);
  CHECK(state.arts.kills.totalKills == 1234);
  CHECK(state.score.bossTimeTrialScores[7][2].time == 0xABCDEF);
  CHECK(state.room_cache[0].next_slot == 0);
}

void CheckCapturedCache(const Save::Snapshot& state) {
  const auto& actor = state.room_cache[0].actors[7];
  CHECK(actor.spawnSlot == 1);
  CHECK(actor.currentHp == 43);
  CHECK(actor.strength == 123);
  CHECK(actor.limbHp[5] == 32);
  CHECK(actor.tileX == 2);
  CHECK(actor.direction == 1);
  CHECK(actor.tileZ == 3);
  CHECK(actor.height == 179);
}

void CheckCapturedRandom(const Save::Snapshot& state) {
  const auto& actor = state.room_cache[0].actors[7];
  CHECK(actor.aiFlags == (1 | 6 | 16 | 32 | (77 << 6)));
  CHECK(state.random.bios_state == 0x12345678);
  CHECK(state.random.movement_state == 0xFEDCBA98);
}
}  // namespace

TEST_CASE("Runtime capture reads canonical gameplay without modifying it",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  fixture.AddEnemy();
  Field_snapshotDoorFlagSource.requiredFlagId = 999;
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
  CHECK(fixture->player_model.momentumDistance == 0);
  const auto before = LiveFingerprint(*fixture);
  (void)fixture->Capture();
  CHECK(LiveFingerprint(*fixture) == before);
  fixture->player_model.momentumDistance = 0x6500;
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
        sizeof(Field_MpdHeader) + 23 * sizeof(std::uint32_t),
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
  if (defect == 11) fixture.player_model.instance.movementPending = 1;
  if (defect == 12) fixture.player_model.instance.movementMode = 3;
  if (defect == 13) fixture.player_model.instance.locomotionState = 1;
  if (defect == 14) fixture.player_model.instance.isJumping = 1;
  if (defect == 15) fixture.player_model.instance.turnFrames = 2;
  if (defect == 16) fixture.player_model.instance.currentAnimation = 1;
  if (defect == 17) fixture.player_model.instance.rootMotion.vx = 1;
  if (defect == 18) fixture.player_model.routePhase = 1;
  if (defect == 19) fixture.player_model.momentumDistance = 2;
  if (defect == 20) fixture.player_model.instance.placementLayer = 2;
  if (defect == 21) fixture.player_model.instance.isAttached = 1;
  if (defect == 31) fixture.player_model.transitionFrames = 1;
}

void InvalidateLiveFixture(RuntimeFixture& fixture, int defect) {
  InvalidateLiveMotion(fixture, defect);
  if (defect == 0) Field_zoneContext.currentSceneId = 38;
  if (defect == 1) Boot_isGameplayActive = 0;
  if (defect == 2) fixture.player_stats.weapon.index = 9;
  if (defect == 3) Audio_Seq_unusedInventory.weapons[0].grip = 255;
  if (defect == 4) State_currentRoomStateSlot = 4;
  if (defect == 5) Field_roomData.environment = nullptr;
  if (defect == 6) fixture.player_model.instance.currentTileLayer = 2;
  if (defect == 7) fixture.player_model.instance.supportLayer = 2;
  if (defect == 8) Field_isFirstPersonViewEnabled = 1;
  if (defect == 9) Field_sphericalCamera.values.distance = 2000;
  if (defect == 10) Field_sphericalCamera.values.pitch = -768;
  if (defect == 22) Field_sphericalCamera.delta.mode = 2;
  if (defect == 23) Field_sphericalCamera.delta.yaw = 1;
  if (defect == 24) ++Render_frameScratch.camera.position.vx;
  if (defect == 25) Render_frameScratch.camera.angles.vz = 1;
  if (defect == 26) Render_nearClip = 128;
  if (defect == 27) Field_sphericalCamera.values.pitch = 1024;
  if (defect == 28) Render_projectionDistance = 256;
  if (defect == 29) Field_sphericalCamera.delta.distance = 65;
  if (defect == 30) Field_cameraTransition = &fixture.camera_transition;
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
  if (defect == 2) state.inventory.gems[1].setItemIndex = 0xFFFF;
  if (defect == 3) state.inventory.shields[0].gems[0] = 0x80;
  if (defect == 4) state.inventory_order.gems[1] = 1;
  if (defect == 5) state.inventory.blades[0].wepId = 127;
  if (defect == 6) state.location.zone = 13;
  if (defect == 7) state.location.room = 1;
  if (defect == 8) state.location.scene = 38;
  if (defect == 9) state.location.position[0] = 512;
  if (defect == 10) state.location.position[1] = 32768;
  if (defect == 11) state.random.shuffle_index = 97;
  if (defect == 12) state.storage.misc[255].id = 512;
  if (defect == 13) state.inventory.armor[1].limb = 1;
  if (defect == 14) state.inventory.blades[0].material = 8;
  if (defect == 15) state.room_cache[0].actors[0].spawnSlot = 9;
  if (defect == 16) state.inventory_order.gems[0] = 0;
  if (defect == 17) state.room_cache_slot = 1;
  if (defect == 18) state.room_cache[1] = state.room_cache[0];
  if (defect == 25) state.location.floor_layer = 2;
  if (defect == 26) state.location.previous_tile.layer = 2;
}

void CheckStagedInputs() {
  CHECK(Save::ResumePending());
  CHECK(static_cast<unsigned char>(State_ScriptFlag(64)) == 177);
  CHECK(State_savedEntryDoor.requiredFlagId == 37);
  CHECK(State_savedEntryDoor.destinationZoneId == 9);
  CHECK(State_savedEntryDoor.entryTileX == 1);
  CHECK(State_savedEntryDoor.entryTileZ == 2);
  CHECK(State_ashleySnapshot.stats.isPending == 1);
  CHECK(static_cast<unsigned char>(State_learnedActionFlags[12]) == 0x40);
  CHECK(State_learnedActionFlags[0] == 0);
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
    Put(fixture.content.room, sizeof(Field_MpdHeader) + 4,
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
    auto collision = Field_MpdCollisionHeader{33, 4, 0, 1};
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
  std::fill_n(Items_itemCategoryUpperIds, 8, 0);
  REQUIRE(Save::ValidateContent(state).has_value());
  CHECK(std::ranges::all_of(Items_itemCategoryUpperIds,
                            [](auto value) { return value == 0; }));
}

TEST_CASE("Original New Game inventory accepts its lazy display-index state",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto state = fixture.Capture();
  static_assert(sizeof(Title_newGameInventoryTemplate) ==
                sizeof(state.inventory));
  std::memcpy(&state.inventory, Title_newGameInventoryTemplate,
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
  auto& items = Audio_Seq_unusedInventory.misc;
  auto& order = Audio_Seq_unusedInventoryIndices.misc;
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
  REQUIRE(Items_ConsumeMiscItem(432) == 1);
  REQUIRE(Items_ConsumeMiscItem(323) == 1);
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
  CHECK(Audio_Seq_unusedInventoryIndices.misc[0] == 12);
  CHECK(Audio_Seq_unusedInventoryIndices.misc[1] == 10);
  CHECK(Audio_Seq_unusedInventory.misc[10].id == 0);
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
  auto& order = Audio_Seq_unusedInventoryIndices;
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
  auto& order = Audio_Seq_unusedInventoryIndices.misc;
  const auto defect = GENERATE(0, 1);
  order[0] = 64;
  order[1] = defect == 0 ? 65 : 64;
  const auto before = LiveFingerprint(fixture);
  const auto state = Save::Capture(fixture.storage);
  REQUIRE_FALSE(state.has_value());
  CHECK(state.error().find("inventory_order.misc") != std::string::npos);
  CHECK(LiveFingerprint(fixture) == before);
}

namespace {
void CheckAudioSettings(bool mono) {
  CHECK((Input_settings.monoSound == 1) == mono);
  CHECK(((Audio_Driver_controlLatches.mixBehavior & 2) != 0) == mono);
  CHECK((Audio_Driver_controlLatches.mixBehavior & 0x10) != 0);
  CHECK(Input_settings.slotState == 0xFEDCBA98u);
  CHECK(Input_settings.key == 0xF00DCAFEu);
}
}  // namespace

TEST_CASE(
    "Resume synchronizes the actual mono consumer through apply cancel and "
    "finish",
    "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  auto state = fixture.Capture();
  const bool mono = GENERATE(false, true);
  Input_settings.monoSound = static_cast<char>(!mono);
  Audio_SetMonoOutput(mono ? 0 : 1);
  Audio_Driver_controlLatches.mixBehavior |= 0x10;
  const auto before = LiveFingerprint(fixture);
  state.settings.monoSound = static_cast<char>(mono);
  state.settings.slotState = 0xFEDCBA98u;
  state.settings.key = 0xF00DCAFEu;
  REQUIRE(Save::PrepareResume({1, 0, state}).has_value());
  CheckAudioSettings(mono);
  Save::CancelResume();
  CHECK(LiveFingerprint(fixture) == before);
  REQUIRE(Save::PrepareResume({1, 0, state}).has_value());
  State_ashleySnapshot.stats.isPending = 0;
  Audio_SetMonoOutput(mono ? 0 : 1);
  Audio_Driver_controlLatches.mixBehavior |= 0x10;
  REQUIRE(Save::FinishResume().has_value());
  CheckAudioSettings(mono);
}

TEST_CASE("Uninitialized audio consumer cannot partially prepare a resume",
          "[save][runtime-snapshot]") {
  RuntimeFixture fixture;
  const auto state = fixture.Capture();
  Audio_Mixer_kCommandHandlers[0x81] = nullptr;
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
  state.settings.monoSound = 1;
  REQUIRE(Save::PrepareResume({1, 0, state}).has_value());
  State_ashleySnapshot.stats.isPending = 0;
  Audio_Mixer_kCommandHandlers[0x81] = nullptr;
  const auto before = LiveFingerprint(fixture);
  REQUIRE_FALSE(Save::FinishResume().has_value());
  CHECK(LiveFingerprint(fixture) == before);
  Save::CancelResume();
  CHECK_FALSE(Save::ResumePending());
  CHECK((Audio_Driver_controlLatches.mixBehavior & 2) == 0);
  CHECK(Input_settings.monoSound == 0);
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
  State_ashleySnapshot.stats.isPending = 0;
  fixture.player_model.instance.position = {64, 0, 64, 0};
  fixture.player_stats.currentHp = 7;
  fixture.player_stats.statuses = 0;
  State_playTime.all = 0;
  State_sceneTimer = {};
  Field_sphericalCamera.values.yaw = 0;
  Core_Random_Seed(1);
  Core_interfaceRandomState = 1;
  std::fill_n(Core_randomShuffleTable, 97, 0);
  Core_randomShuffleIndex = 0;
  Battle_Ai_savedMovementRandomState = 1;
  Battle_Ai_FrameState()->randomState = 1;
  State_SetScriptFlag(64, 0);
  State_recentRoomEnemyStates[0].nextSlot = 3;
}

void CheckRestoredPlayer(const RuntimeFixture& fixture) {
  CHECK(fixture.player_stats.currentHp == 123);
  CHECK(fixture.player_stats.statuses == 0x21000);
  CHECK(fixture.player_stats.statusTimers[7] == 255);
  CHECK(fixture.player_stats.limbs[5].types[3] == -29);
  CHECK(fixture.player_model.instance.position.vx == 193);
  CHECK(fixture.player_model.instance.position.vy == -73);
  CHECK(fixture.player_model.instance.position.vz == 325);
  CHECK(fixture.player_model.instance.facing == 2051);
  CHECK(fixture.player_model.instance.currentTileX == 1);
  CHECK(fixture.player_model.instance.currentTileZ == 2);
}

void CheckRestoredClockCamera() {
  CHECK(State_playTime.t.hours == 12);
  CHECK(State_playTime.t.minutes == 57);
  CHECK(State_playTime.t.seconds == 58);
  CHECK(State_playTime.t.frames == 59);
  CHECK(State_sceneTimer.frames == 29);
  CHECK(State_sceneTimer.seconds == 17);
  CHECK(Field_sphericalCamera.values.yaw == 1027);
  CHECK(Render_frameScratch.camera.angles.vy == 1027);
  CHECK(Field_sphericalCamera.initialDistance == 1536);
}

void CheckRestoredRandom(const Save::Snapshot& state) {
  CHECK(Core_Random_State() == state.random.bios_state);
  CHECK(Core_interfaceRandomState == state.random.interface_state);
  CHECK(std::equal(state.random.shuffle.begin(), state.random.shuffle.end(),
                   Core_randomShuffleTable));
  CHECK(Core_randomShuffleIndex == 96);
  CHECK(Battle_Ai_FrameState()->randomState == 0xFEDCBA98);
  CHECK(std::bit_cast<unsigned>(Battle_Ai_savedMovementRandomState) ==
        0xFEDCBA98);
}

void CheckRestoredCacheFlags() {
  CHECK(static_cast<unsigned char>(State_ScriptFlag(64)) == 99);
  CHECK(State_recentRoomEnemyStates[0].nextSlot == 0);
  CHECK(static_cast<unsigned char>(State_recentRoomEnemyStates[3].zoneId) ==
        12);
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
  model.currentTileLayer = static_cast<unsigned char>(layer);
  model.previousTileX = 0;
  model.previousTileZ = 1;
  model.previousTileLayer = static_cast<unsigned char>(layer);
  model.previousFacing = -144;
  Actors_TileTarget saved{};
  saved.tileX = 2;
  saved.tileZ = 3;
  saved.tileLayer = layer;
  saved.facing = 77;
  fixture.player_model.savedPlacement = std::bit_cast<int>(saved);
  Field_sphericalCamera.values.pitch = -768;
  Field_sphericalCamera.values.distance = 2304;
  Field_sphericalCamera.values.mode = 17;
  Field_sphericalCamera.delta.pitch = -512;
  Field_sphericalCamera.delta.distance = 32;
  Field_sphericalCamera.delta.mode = 3;
  Field_sphericalCamera.initialDistance = 2304;
  Field_sphericalCamera.zoomVelocity = 48;
  Field_sphericalCamera.speed = 24;
  Render_frameScratch.camera.lookAt = {111111, -333333, 222222, 0};
  Render_frameScratch.camera.angles = {-764, 1027, 0, 0};
  fixture.UpdateOrbitPosition();
}

void CheckStaticLayer(const RuntimeFixture& fixture,
                      const Save::Snapshot& state) {
  const auto& model = fixture.player_model.instance;
  CHECK(model.currentTileLayer == state.location.floor_layer);
  CHECK(model.previousTileLayer == state.location.previous_tile.layer);
  CHECK(model.previousTileX == 0);
  CHECK(model.previousTileZ == 1);
  CHECK(model.previousFacing == -144);
  const auto saved =
      std::bit_cast<Actors_TileTarget>(fixture.player_model.savedPlacement);
  CHECK(saved.tileX == 2);
  CHECK(saved.tileZ == 3);
  CHECK(saved.tileLayer == state.location.floor_layer);
  CHECK(saved.facing == 77);
}

void CheckOrbitRestored(const Field_SphericalCamera& expected,
                        const Render_Camera& rendered) {
  CHECK(std::memcmp(&Field_sphericalCamera, &expected, sizeof(expected)) == 0);
  CHECK(Render_frameScratch.camera.position.vx == rendered.position.vx);
  CHECK(Render_frameScratch.camera.position.vy == rendered.position.vy);
  CHECK(Render_frameScratch.camera.position.vz == rendered.position.vz);
  CHECK(Render_frameScratch.camera.lookAt.vx == rendered.lookAt.vx);
  CHECK(Render_frameScratch.camera.lookAt.vy == rendered.lookAt.vy);
  CHECK(Render_frameScratch.camera.lookAt.vz == rendered.lookAt.vz);
  CHECK(Render_frameScratch.camera.angles.vx == rendered.angles.vx);
  CHECK(Render_frameScratch.camera.angles.vy == rendered.angles.vy);
}

void CheckCanceledStaticResume(const RuntimeFixture& fixture,
                               const Save::Document& document,
                               const std::vector<std::uint8_t>& before) {
  REQUIRE(Save::PrepareResume(document).has_value());
  CHECK(State_savedEntryDoor.tileLocation.entryDirection ==
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
  const auto camera = Field_sphericalCamera;
  const auto rendered = Render_frameScratch.camera;
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
    fixture.player_model.instance.currentTileLayer = 0;
    fixture.player_model.instance.previousTileLayer = 0;
    fixture.player_model.savedPlacement = 0;
    Field_sphericalCamera = {};
    Render_frameScratch.camera.lookAt = {};
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
  State_ashleySnapshot.stats.isPending = 0;
  if (defect == 0) State_ashleySnapshot.stats.isPending = 1;
  if (defect == 1) Field_zoneContext.mapId = 1;
  if (defect == 2) Field_zoneContext.currentSceneId = 38;
  if (defect == 3) Field_roomData.isLoaded = 0;
  if (defect == 4) Boot_isGameplayActive = 0;
  const auto before = LiveFingerprint(fixture);
  CHECK_FALSE(Save::FinishResume().has_value());
  CHECK(Save::ResumePending());
  CHECK(LiveFingerprint(fixture) == before);
  Save::CancelResume();
  CHECK_FALSE(Save::ResumePending());
}
