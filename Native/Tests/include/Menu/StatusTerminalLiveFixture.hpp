#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/AppearanceLiveState.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Actors/Movement.h"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/Textures.h"
#include "Actors/WeaponModel.h"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/Gimmicks.h"
#include "Field/RoomRenderingLiveState.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/ActorSwitchingLive.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLiveInternal.hpp"
#include "ModelLoading.h"
#include "Render/Camera.hpp"
#include "Render/ModelDraw.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/ModelWorkspace.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/Textures.hpp"

namespace status_terminal_live_tests {
namespace st = VS::Menu::Status;
namespace fw = VS::Menu::Framework;
namespace memory = VS::Core::Memory;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
inline constexpr std::size_t kArenaBytes = 0x400000;
inline constexpr std::size_t kPrimitiveWords = 0x20000 / sizeof(u_long);
inline constexpr std::size_t kOrderingWords = 0x2088 / sizeof(u_long);
inline constexpr int kSourceModel = 1;
inline constexpr int kSourceBattleSequence = 2;
inline constexpr int kAssetPollLimit = 256;
using Row = Menu_Framework_MenuRow;
inline constexpr auto kRowBytes = fw::kRowCount * sizeof(Row);
inline constexpr auto kTypeBytes = fw::kRowTypeCount * sizeof(int);
inline constexpr auto kDataBytes = fw::kStringBufferBytes + kTypeBytes;
inline constexpr auto kWorkspaceBytes = kRowBytes + kDataBytes;

[[nodiscard]] inline std::vector<std::uint8_t> ReadOriginal(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error(std::string("missing original asset: ") + path);
  }
  return {std::istreambuf_iterator<char>(stream), {}};
}

struct FrameRestore {
  st::TerminalFrame previous = st::terminal_live::LoadFrame();
  ~FrameRestore() { st::terminal_live::StoreFrame(previous); }
};

struct GeometryRestore {
  Core_GteWord offset_x = 0;
  Core_GteWord offset_y = 0;
  GeometryRestore() {
    Core_Gte_GetGeomOffset(&offset_x, &offset_y);
    Core_Gte_SetGeomOffset(160, 112);
  }
  ~GeometryRestore() { Core_Gte_SetGeomOffset(offset_x, offset_y); }
};

struct MainMenuScope {
  ValueRestore<decltype(VS::Menu::Framework::menuRowYOffset)> y{
      VS::Menu::Framework::menuRowYOffset};
  ValueRestore<decltype(VS::Menu::Framework::selectedRow)> selected{
      VS::Menu::Framework::selectedRow};
  ValueRestore<decltype(VS::Menu::Framework::subMenuId)> submenu{
      VS::Menu::Framework::subMenuId};
  ValueRestore<decltype(VS::Menu::Framework::subMenuPage)> page{
      VS::Menu::Framework::subMenuPage};
  ValueRestore<decltype(VS::Menu::Framework::subMenuRowCount)> rows{
      VS::Menu::Framework::subMenuRowCount};
  ValueRestore<decltype(VS::Menu::Framework::subMenuRowInfo)> info{
      VS::Menu::Framework::subMenuRowInfo};
  ValueRestore<decltype(VS::Menu::Framework::actionMenuState)> action{
      VS::Menu::Framework::actionMenuState};
  ValueRestore<decltype(VS::Menu::Framework::backgroundFadeStep)> fade{
      VS::Menu::Framework::backgroundFadeStep};
  ValueRestore<decltype(VS::Menu::Framework::cursorColor)> cursor{
      VS::Menu::Framework::cursorColor};
  ValueRestore<decltype(VS::Menu::Framework::displayActionCost)> cost{
      VS::Menu::Framework::displayActionCost};
  ValueRestore<decltype(VS::Menu::Framework::hideMenu)> hidden{
      VS::Menu::Framework::hideMenu};
  ValueRestore<decltype(VS::Menu::Framework::itemsListRow)> list{
      VS::Menu::Framework::itemsListRow};
  ValueRestore<u_short*> items{VS::Menu::Framework::itemsText};
  MainMenuScope() { fw::ResetLiveMainMenuStatics(); }
  ~MainMenuScope() { fw::ResetLiveMainMenuStatics(); }
};

struct VramRestore {
  RECT area{0, 0, 1024, 512};
  std::vector<u_long> pixels = std::vector<u_long>(1024 * 512 / 2);
  VramRestore() { StoreImage(&area, pixels.data()); }
  ~VramRestore() { LoadImage(&area, pixels.data()); }
};

struct AssetStateRestore {
  ArrayRestore<Content_ActorAssetRequest, VS::Content::kActorAssetRequestCount>
      requests{Content_legacyActorAssetRequests};
  ValueRestore<u_char> model_phase{Content_legacyModelLoadPhase};
  ValueRestore<unsigned char> parse_phase{VS::Content::modelParseResetFlag};
  ValueRestore<u_char> sequence_phase{Content_legacySequenceParsePhase};
  ValueRestore<u_char> effect_phase{Content_legacyEtmLoadPhase};
  ValueRestore<u_char*> model_cursor{Content_legacyShpParseCursor};
  ValueRestore<u_char*> sequence_cursor{Content_legacySequenceParseData};
  ValueRestore<u_char> palette_words{Content_legacyShpCopiedWordCount};
  ValueRestore<int> header_word{Content_legacyShpHeaderWord};
  ValueRestore<int> texture_width{Content_legacyShpTextureWidth};
  ValueRestore<int> texture_height{Content_legacyShpTextureHeight};
  ValueRestore<int> peak{VS::Content::assetQueuePeakUsage};
  ValueRestore<int> load_phase{VS::Content::lastLoadPhaseSnapshot};
  ValueRestore<int> ticks{VS::Content::assetQueueFrameTicks};
  ValueRestore<int> kind{VS::Content::lastAssetRequestType};
  ArrayRestore<VS::Render::TextureSlot, VS::Content::kActorTextureSlots>
      actor_textures{VS::Render::legacyActorTextureSlots};
  ArrayRestore<VS::Render::TextureSlot, VS::Content::kWeaponTextureSlots>
      weapon_textures{VS::Render::legacyWeaponTextureSlots};
  ValueRestore<VS::Render::TexturePageTable> texture_pages{
      VS::Render::LiveTexturePages()};
  ArrayRestore<u_char, std::size(Actors_textureSlotOrigins)> texture_origins{
      Actors_textureSlotOrigins};
  ArrayRestore<u_char, std::size(Actors_textureSlotRowOffsets)> texture_rows{
      Actors_textureSlotRowOffsets};
  ArrayRestore<Actors_WeaponModelData*,
               std::size(Content_legacyWeaponModelBuffers)>
      weapon_buffers{Content_legacyWeaponModelBuffers};
  ArrayRestore<Field_GimmickInstance*, std::size(Field_gimmickInstances)>
      gimmicks{Field_gimmickInstances};
  ArrayRestore<u_char, std::size(Actors_sharedSequenceBankTexturePages)>
      bank_pages{Actors_sharedSequenceBankTexturePages};
  ArrayRestore<void*, std::size(Actors_sharedSequenceData)> shared_sequences{
      Actors_sharedSequenceData};
  ValueRestore<char> forced_overrides{Actors_forcedModelOverrides};
  ValueRestore<u_char> bank_index{Actors_sharedSequenceBankIndex};
  ValueRestore<char> update_complete{Actors_isActorUpdateComplete};
  ValueRestore<u_char> pushed_gimmick{Actors_pushedGimmickIndex};
  ValueRestore<decltype(Menu_Inventory_equipmentModelLoadStep)> equipment_step{
      Menu_Inventory_equipmentModelLoadStep};
  ValueRestore<void*> sequence_work{Actors_sequenceWorkBuffer};
};

struct AudioRestore {
  ArrayRestore<VS::Audio::CommandHandler,
               std::size(VS::Audio::Mixer::kCommandHandlers)>
      handlers{VS::Audio::Mixer::kCommandHandlers};
  ValueRestore<VS::Audio::LegacyCommandArguments> arguments{
      VS::Audio::Driver::commandArguments};
  ValueRestore<VS::Audio::CommandParams> parameters{
      VS::Audio::Mixer::commandParams};
  ValueRestore<VS::Audio::DriverFlags> controls{
      VS::Audio::Driver::controlLatches};
  ValueRestore<VS::Audio::VoiceScheduler> voices{
      VS::Audio::Mixer::voiceScheduler};
  ArrayRestore<VS::Audio::SeqChannel,
               std::size(VS::Audio::SoundEffects::channels)>
      effects{VS::Audio::SoundEffects::channels};

  AudioRestore() {
    const auto image = ReadOriginal(R"(Native\data\disc\SLUS_010.40)");
    const auto tables = VS::Content::ModulePointerTables();
    const auto table = std::ranges::find(
        tables, reinterpret_cast<void**>(VS::Audio::Mixer::kCommandHandlers),
        &VS::Content::ModulePointerTable::target);
    if (table == tables.end() ||
        !VS::Content::RelocateModulePointerTable(
            *table, image, VS::Content::ModuleSymbols())) {
      throw std::runtime_error("could not relocate production audio callbacks");
    }
  }
};

struct PreviewOwner {
  std::span<std::byte> arena;
  Actors_ActorRecord* source = nullptr;

  ~PreviewOwner() {
    const bool source_live =
        source != nullptr && memory::LivePayloadCapacity(source).has_value();
    const void* model = source_live ? source->instance.model : nullptr;
    if (Actors_actorRecords[1] != nullptr &&
        memory::LivePayloadCapacity(Actors_actorRecords[1])) {
      fw::ReleaseLiveActorPreview();
    }
    if (source_live) {
      if (Actors_actorRecords[0] == source) {
        static_cast<void>(VS::Content::ReleaseLiveActorModel(0));
      }
      memory::FreeLiveFromLowEnd(source);
    }
    VS::Render::ForgetLiveModelExtent(model);
    VS::Actors::ForgetSequenceBanks(arena.data(), arena.size());
  }
};

struct Fixture {
  GeometryRestore geometry;
  VramRestore vram;
  equipment_detail_tests::LiveFixture framework;
  MainMenuScope main_menu;
  FrameRestore saved_frame;
  ValueRestore<st::EquipmentDetailState> saved_detail{
      st::terminal_live::detail_state};
  ValueRestore<st::EquipmentListState> saved_list{
      st::terminal_live::list_state};
  ValueRestore<st::StatusTerminalState> saved_screen{
      st::terminal_live::screen_state};
  ValueRestore<VS::Render::FrameScratch> saved_scratch{
      VS::Render::FrameScratchpad()};
  ValueRestore<int> saved_projection{VS::Render::projectionDistance};
  ValueRestore<int> saved_framebuffer{VS::Render::frameBuf};
  ValueRestore<void*> saved_primitives{VS::Render::legacyPrimitiveBuffers[0]};
  ValueRestore<void*> saved_ordering{VS::Render::legacyOrderingTables[0]};
  ValueRestore<void*> saved_current_ordering{
      VS::Render::legacyCurrentOrderingTable};
  ValueRestore<char> saved_owner{VS::Content::overlayWorkspaceOwner};
  ValueRestore<char> saved_hud{Battle_hudHidden};
  ValueRestore<char> saved_lower_ui{Battle_lowerScreenUiState};
  ValueRestore<decltype(VS::Field::isFieldRenderingEnabled)> saved_field{
      VS::Field::isFieldRenderingEnabled};
  ValueRestore<Menu_Framework_MenuControlState> saved_control{
      VS::Menu::Framework::menuState};
  ValueRestore<Menu_Framework_MenuRow*> saved_rows{
      VS::Menu::Framework::menuItems};
  ValueRestore<char*> saved_strings{VS::Menu::Framework::stringBuf};
  ValueRestore<int*> saved_types{VS::Menu::Framework::rowTypeBuf};
  ValueRestore<u_short*> saved_list_text{VS::Menu::Framework::itemsText};
  ValueRestore<decltype(VS::Menu::Framework::itemNames)> saved_names{
      VS::Menu::Framework::itemNames};
  ValueRestore<u_short*> saved_help{VS::Menu::Framework::itemHelp};
  ValueRestore<u_short*> saved_menu12{VS::Menu::Framework::menu12Text};
  ValueRestore<Menu_Framework_MenuMemory> saved_memory{
      VS::Menu::Framework::menuMemory};
  ValueRestore<Menu_StringContext> saved_context{Battle_stringContext};
  ArrayRestore<char, VS::Menu::Framework::kSubmenuStateCount> saved_submenus{
      VS::Menu::Framework::submenuStates};
  ArrayRestore<Battle_Actor*, std::size(Battle_actors)> saved_actors{
      Battle_actors};
  ArrayRestore<Actors_ActorRecord*, std::size(Actors_actorRecords)>
      saved_models{Actors_actorRecords};
  ArrayRestore<Actors_WeaponModel*, std::size(VS::Actors::weaponModels)>
      saved_weapons{VS::Actors::weaponModels};
  ValueRestore<void*> saved_primary_sequence{Actors_primarySequenceBuffer};
  ValueRestore<VECTOR> saved_scale{VS::Actors::roomActorScale};
  ValueRestore<Actors_SkeletonPose> saved_pose{VS::Actors::skeletonPose};
  ValueRestore<MATRIX> saved_root_matrix{VS::Actors::skeletonRootMatrix};
  ValueRestore<MATRIX> saved_bone_camera{VS::Actors::boneMatrixScratch};
  ValueRestore<MATRIX> saved_camera_basis{VS::Render::cameraBasisMatrix};
  ValueRestore<Actors_ModelRenderContext> saved_model_context{
      VS::Actors::modelRenderContext};
  ValueRestore<char> saved_sequence_index{Actors_lastResolvedSequenceIndex};
  ValueRestore<std::remove_reference_t<decltype(*VS::Render::ModelWorkspace())>>
      saved_model_workspace{*VS::Render::ModelWorkspace()};
  AssetStateRestore saved_assets;
  AudioRestore saved_audio;
  ArrayRestore<u_short, std::size(Menu_Status_statusStrings)> saved_status_text{
      Menu_Status_statusStrings};
  ArrayRestore<u_short, std::size(Battle_statusStrings)> saved_battle_text{
      Battle_statusStrings};
  ArrayRestore<int16_t, fw::kEquipmentStatCount> saved_stats{
      VS::Menu::Framework::equipmentStats};
  ArrayRestore<std::uint8_t, std::size(VS::Menu::Framework::animationIndices)>
      saved_animation{VS::Menu::Framework::animationIndices};
  std::vector<std::byte> arena = std::vector<std::byte>(kArenaBytes);
  memory::ScopedLiveGameHeap heap{arena};
  PreviewOwner preview_owner{arena};
  std::vector<u_long> primitives = std::vector<u_long>(kPrimitiveWords);
  std::array<u_long, kOrderingWords> ordering{};
  Battle_ActorStats stats{};
  Battle_Actor actor{};
  void* workspace = nullptr;
  void* text = nullptr;

  Fixture() {
    st::ResetLiveTerminalControllersModuleData();
    st::ResetLiveLimbControllersModuleData();
    st::ResetLiveActorSwitchPhase();
    st::terminal_live::StoreFrame({.selected_actor = 1});
    actor.stats = &stats;
    actor.kind = 1;
    std::ranges::fill(stats.name, static_cast<char>(0xE7));
    stats.name[0] = 0x51;
    Battle_actors[0] = &actor;
    for (auto& limb : stats.limbs) {
      limb.nameIndex = 1;
      limb.hp = limb.maxHp = 100;
    }
    workspace = memory::AllocateLiveFromHighEnd(kWorkspaceBytes);
    text = memory::AllocateLiveFromHighEnd(fw::kItemTextBytes);
    if (workspace == nullptr || text == nullptr) {
      throw std::runtime_error("terminal Live fixture allocation failed");
    }
    std::memset(workspace, 0, kWorkspaceBytes);
    VS::Menu::Framework::menuItems =
        static_cast<Menu_Framework_MenuRow*>(workspace);
    VS::Menu::Framework::stringBuf =
        reinterpret_cast<char*>(VS::Menu::Framework::menuItems + fw::kRowCount);
    VS::Menu::Framework::rowTypeBuf = reinterpret_cast<int*>(
        VS::Menu::Framework::stringBuf + fw::kStringBufferBytes);
    VS::Content::overlayWorkspaceOwner = 6;
    BindText();
    BindFrame();
    BindPreview();
    BindFrame();
  }
  ~Fixture() {
    st::ResetLiveLimbControllersModuleData();
    st::ResetLiveActorSwitchPhase();
    // The preview draw sets the GTE screen distance.
    // VS::Render::projectionDistance is not that distance's previous value (0
    // versus 1000 at start-up), so reset the GTE; saved_projection and geometry
    // restore their own state.
    Core_Gte_Reset();
  }

  static VS::Content::AssetAddress Address(const void* pointer) {
    return static_cast<VS::Content::AssetAddress>(
        reinterpret_cast<std::uintptr_t>(pointer));
  }
  static void DrainAssets() {
    const auto refused = VS::Content::LiveActorAssetRefusals();
    for (int tick = 0; tick < kAssetPollLimit; ++tick) {
      const int pending = VS::Content::UpdateLiveActorAssetQueue();
      if (VS::Content::LiveActorAssetRefusals() != refused) {
        throw std::runtime_error("production actor asset queue refused setup");
      }
      if (pending == 0) {
        return;
      }
    }
    throw std::runtime_error("production actor asset queue did not drain");
  }
  static void ParseAsset(const char* path,
                         VS::Content::ActorAssetRequest request) {
    const auto bytes = ReadOriginal(path);
    void* file = memory::AllocateLiveFromHighEnd(bytes.size());
    if (file == nullptr || bytes.empty()) {
      throw std::runtime_error("actor asset input allocation failed");
    }
    std::memcpy(file, bytes.data(), bytes.size());
    request.data = Address(file);
    if (VS::Content::QueueLiveActorAsset(request) < 0) {
      throw std::runtime_error("production actor asset queue is full");
    }
    DrainAssets();
    memory::FreeLiveFromHighEnd(file);
  }
  static void LoadTextureOrigins() {
    const auto image = ReadOriginal(R"(Native\data\disc\BATTLE\BATTLE.PRG)");
    const auto entries = VS::Content::ModuleDataEntries();
    for (void* target : {static_cast<void*>(Actors_textureSlotOrigins),
                         static_cast<void*>(Actors_textureSlotRowOffsets)}) {
      const auto entry = std::ranges::find(
          entries, target, &VS::Content::ModuleDataEntry::target);
      if (entry == entries.end() ||
          !VS::Content::CopyModuleDataEntry(*entry, image)) {
        throw std::runtime_error(
            "could not load original texture slot origins");
      }
    }
  }
  void BindPreview() {
    namespace content = VS::Content;
    if (std::ranges::any_of(
            Content_legacyActorAssetRequests,
            [](const auto& request) { return request.assetKind != 0; }) ||
        Content_legacyModelLoadPhase != 0 ||
        VS::Content::modelParseResetFlag != 0 ||
        Content_legacySequenceParsePhase != 0) {
      throw std::runtime_error("terminal fixture requires an idle asset queue");
    }
    LoadTextureOrigins();
    VS::Actors::InitializeLiveActorModelResources();
    preview_owner.source = static_cast<Actors_ActorRecord*>(
        memory::AllocateLiveFromLowEnd(sizeof(Actors_ActorRecord)));
    if (preview_owner.source == nullptr) {
      throw std::runtime_error("source actor owner allocation failed");
    }
    *preview_owner.source = {};
    if (Actors_primarySequenceBuffer == nullptr) {
      throw std::runtime_error("source sequence owner allocation failed");
    }
    ParseAsset(R"(Native\data\disc\OBJ\01.SHP)",
               {.kind = static_cast<std::uint8_t>(
                    content::ActorAssetKind::kActorModelParse),
                .target_slot = 0,
                .model_id = kSourceModel,
                .destination = Address(preview_owner.source),
                .sub_kind = 0xFF});
    ParseAsset(R"(Native\data\disc\OBJ\01_COM.SEQ)",
               {.kind = static_cast<std::uint8_t>(
                    content::ActorAssetKind::kSequenceParse),
                .target_slot = 0});
    ParseAsset(R"(Native\data\disc\OBJ\01_BT2.SEQ)",
               {.kind = static_cast<std::uint8_t>(
                    content::ActorAssetKind::kSequenceParse),
                .target_slot = 0,
                .model_id = kSourceBattleSequence});
    const auto refused = content::LiveActorAssetRefusals();
    fw::LoadLiveActorPreview(0);
    const auto* preview = Actors_actorRecords[1];
    if (content::LiveActorAssetRefusals() != refused || preview == nullptr ||
        preview->instance.boneCount == 0 ||
        !VS::Render::ValidateLiveActorModel(*preview) ||
        VS::Actors::SequenceBankBytes(preview->commonSequence).empty() ||
        VS::Actors::SequenceBankBytes(preview->battleSequence).empty()) {
      throw std::runtime_error(
          "production actor preview did not acquire its owners");
    }
  }

  void BindText() const {
    auto bytes =
        std::span(static_cast<std::uint8_t*>(text), fw::kItemTextBytes);
    std::ranges::fill(bytes, std::uint8_t{0xE7});
    VS::Menu::Framework::itemNames =
        static_cast<char (*)[fw::kItemNameRowBytes]>(text);
    VS::Menu::Framework::itemHelp =
        reinterpret_cast<u_short*>(bytes.data() + fw::kItemNamesBytes);
    VS::Menu::Framework::menu12Text = reinterpret_cast<u_short*>(
        bytes.data() + fw::kItemNamesBytes + fw::kItemHelpBytes);
    std::fill_n(VS::Menu::Framework::itemHelp,
                equipment_detail_tests::kHelpEntries, u_short{4000});
    VS::Menu::Framework::itemHelp[4000] = 0xE751;
    std::ranges::fill(Menu_Status_statusStrings, u_short{0xE7E7});
    std::fill_n(Menu_Status_statusStrings, 61, u_short{400});
    Menu_Status_statusStrings[400] = 0xE741;
    Menu_Status_statusStrings[253] = 0x00FF;
    Menu_Status_statusStrings[254] = 0xFF8F;
    Menu_Status_statusStrings[255] = 0xE701;
    std::ranges::fill(Battle_statusStrings, u_short{0xE7E7});
    Battle_statusStrings[1] = 400;
    Battle_statusStrings[400] = 0xE731;
    std::ranges::fill(VS::Menu::Framework::animationIndices, std::uint8_t{10});
  }
  void BindFrame() {
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = primitives.data();
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::legacyCurrentOrderingTable = ordering.data();
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + equipment_detail_tests::kSceneOrderingEntry;
    VS::Render::FrameScratchpad().overlay_ordering_table = ordering.data() + 2;
    VS::Render::FrameScratchpad().view_matrix = {};
    VS::Render::FrameScratchpad().camera = {};
    BeginFrame();
  }
  void BeginFrame() {
    VS::Render::FrameScratchpad().primitive_cursor = primitives.data();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }
  void IdleRows() const {
    for (std::size_t row = 0; row < fw::kRowCount; ++row) {
      fw::LiveMenuRow(static_cast<int>(row)).state = fw::RowState::kIdle;
    }
  }
  [[nodiscard]] std::uint8_t* State() const {
    return reinterpret_cast<std::uint8_t*>(
        &VS::Menu::Framework::submenuStates[4]);
  }
};
}  // namespace status_terminal_live_tests
