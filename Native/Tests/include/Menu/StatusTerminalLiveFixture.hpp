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

#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AppearanceLiveState.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Actors/Movement.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/Textures.hpp"
#include "Actors/WeaponModel.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hud.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Status/ActorStatus.hpp"
#include "Menu/Status/ActorSwitchingLive.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLiveInternal.hpp"
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
inline constexpr std::size_t kPrimitiveWords = 0x20000 / sizeof(std::uint32_t);
inline constexpr std::size_t kOrderingWords = 0x2088 / sizeof(std::uint32_t);
inline constexpr int kSourceModel = 1;
inline constexpr int kSourceBattleSequence = 2;
inline constexpr int kAssetPollLimit = 256;
using Row = fw::MenuRow;
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
  VS::Core::Gte::Word offset_x = 0;
  VS::Core::Gte::Word offset_y = 0;
  GeometryRestore() {
    VS::Core::Gte::GetGeomOffset(&offset_x, &offset_y);
    VS::Core::Gte::SetGeomOffset(160, 112);
  }
  ~GeometryRestore() { VS::Core::Gte::SetGeomOffset(offset_x, offset_y); }
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
  ValueRestore<std::uint16_t*> items{VS::Menu::Framework::itemsText};
  MainMenuScope() { fw::ResetLiveMainMenuStatics(); }
  ~MainMenuScope() { fw::ResetLiveMainMenuStatics(); }
};

struct VramRestore {
  VS::Legacy::RECT area{0, 0, 1024, 512};
  std::vector<std::uint32_t> pixels =
      std::vector<std::uint32_t>(1024 * 512 / 2);
  VramRestore() { VS::Legacy::StoreImage(&area, pixels.data()); }
  ~VramRestore() { VS::Legacy::LoadImage(&area, pixels.data()); }
};

struct AssetStateRestore {
  ArrayRestore<VS::Content::LegacyActorAssetRequest,
               VS::Content::kActorAssetRequestCount>
      requests{VS::Content::legacyActorAssetRequests.data()};
  ValueRestore<std::uint8_t> model_phase{VS::Content::legacyModelLoadPhase};
  ValueRestore<unsigned char> parse_phase{VS::Content::modelParseResetFlag};
  ValueRestore<bool> parse_bound{VS::Content::modelParseBoundRecord};
  ValueRestore<std::uint8_t> sequence_phase{
      VS::Content::legacySequenceParsePhase};
  ValueRestore<std::uint8_t> effect_phase{VS::Content::legacyEtmLoadPhase};
  ValueRestore<std::uint8_t*> model_cursor{VS::Content::legacyShpParseCursor};
  ValueRestore<std::uint8_t*> sequence_cursor{
      VS::Content::legacySequenceParseData};
  ValueRestore<std::uint8_t> palette_words{
      VS::Content::legacyShpCopiedWordCount};
  ValueRestore<int> header_word{VS::Content::legacyShpHeaderWord};
  ValueRestore<int> texture_width{VS::Content::legacyShpTextureWidth};
  ValueRestore<int> texture_height{VS::Content::legacyShpTextureHeight};
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
  ArrayRestore<std::uint8_t, VS::Actors::kTextureSlotOriginBytes>
      texture_origins{VS::Actors::textureSlotOrigins.data()};
  ArrayRestore<std::uint8_t, VS::Actors::kTextureSlotRowOffsetBytes>
      texture_rows{VS::Actors::textureSlotRowOffsets.data()};
  ArrayRestore<VS::Actors::WeaponModelData*,
               std::size(VS::Content::legacyWeaponModelBuffers)>
      weapon_buffers{VS::Content::legacyWeaponModelBuffers.data()};
  ArrayRestore<VS::Field::GimmickInstance*,
               std::size(VS::Field::gimmickInstances)>
      gimmicks{VS::Field::gimmickInstances.data()};
  ValueRestore<decltype(VS::Actors::sharedSequenceBankTexturePages)> bank_pages{
      VS::Actors::sharedSequenceBankTexturePages};
  ValueRestore<decltype(VS::Actors::sharedSequenceData)> shared_sequences{
      VS::Actors::sharedSequenceData};
  ValueRestore<char> forced_overrides{VS::Actors::forcedModelOverrides};
  ValueRestore<std::uint8_t> bank_index{VS::Actors::sharedSequenceBankIndex};
  ValueRestore<char> update_complete{VS::Actors::isActorUpdateComplete};
  ValueRestore<std::uint8_t> pushed_gimmick{VS::Actors::pushedGimmickIndex};
  ValueRestore<decltype(VS::Content::objectDataLoadStep)> equipment_step{
      VS::Content::objectDataLoadStep};
  ValueRestore<void*> sequence_work{VS::Actors::sequenceWorkBuffer};
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
};

struct PreviewOwner {
  std::span<std::byte> arena;
  VS::Actors::ActorRecord* source = nullptr;

  ~PreviewOwner() {
    const bool source_live =
        source != nullptr && memory::LivePayloadCapacity(source).has_value();
    const void* model = source_live ? source->instance.model : nullptr;
    if (VS::Actors::actorRecords[1] != nullptr &&
        memory::LivePayloadCapacity(VS::Actors::actorRecords[1])) {
      fw::ReleaseLiveActorPreview();
    }
    if (source_live) {
      if (VS::Actors::actorRecords[0] == source) {
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
      st::terminal_live::detailState};
  ValueRestore<st::EquipmentListState> saved_list{st::terminal_live::listState};
  ValueRestore<st::StatusTerminalState> saved_screen{
      st::terminal_live::screenState};
  ValueRestore<VS::Render::FrameScratch> saved_scratch{
      VS::Render::FrameScratchpad()};
  ValueRestore<int> saved_projection{VS::Render::projectionDistance};
  ValueRestore<int> saved_framebuffer{VS::Render::frameBuf};
  ValueRestore<void*> saved_primitives{VS::Render::legacyPrimitiveBuffers[0]};
  ValueRestore<void*> saved_ordering{VS::Render::legacyOrderingTables[0]};
  ValueRestore<void*> saved_current_ordering{
      VS::Render::legacyCurrentOrderingTable};
  ValueRestore<char> saved_owner{VS::Content::overlayWorkspaceOwner};
  ValueRestore<decltype(VS::Battle::hudHidden)> saved_hud{
      VS::Battle::hudHidden};
  ValueRestore<decltype(VS::Battle::lowerScreenUiState)> saved_lower_ui{
      VS::Battle::lowerScreenUiState};
  ValueRestore<decltype(VS::Field::isFieldRenderingEnabled)> saved_field{
      VS::Field::isFieldRenderingEnabled};
  ValueRestore<VS::Menu::Framework::LiveMenuControl> saved_control{
      VS::Menu::Framework::menuState};
  ValueRestore<fw::MenuRow*> saved_rows{VS::Menu::Framework::menuItems};
  ValueRestore<char*> saved_strings{VS::Menu::Framework::stringBuf};
  ValueRestore<int*> saved_types{VS::Menu::Framework::rowTypeBuf};
  ValueRestore<std::uint16_t*> saved_list_text{VS::Menu::Framework::itemsText};
  ValueRestore<decltype(VS::Menu::Framework::itemNames)> saved_names{
      VS::Menu::Framework::itemNames};
  ValueRestore<std::uint16_t*> saved_help{VS::Menu::Framework::itemHelp};
  ValueRestore<std::uint16_t*> saved_menu12{VS::Menu::Framework::menu12Text};
  ValueRestore<VS::Menu::Framework::MenuMemory> saved_memory{
      VS::Menu::Framework::menuMemory};
  ValueRestore<VS::Menu::StringContext> saved_context{
      VS::Battle::stringContext};
  ArrayRestore<char, VS::Menu::Framework::kSubmenuStateCount> saved_submenus{
      VS::Menu::Framework::submenuStates};
  ArrayRestore<VS::Battle::Actor*, std::size(VS::Battle::actors)> saved_actors{
      VS::Battle::actors.data()};
  ArrayRestore<VS::Actors::ActorRecord*, std::size(VS::Actors::actorRecords)>
      saved_models{VS::Actors::actorRecords.data()};
  ArrayRestore<VS::Actors::WeaponModel*, std::size(VS::Actors::weaponModels)>
      saved_weapons{VS::Actors::weaponModels};
  ValueRestore<void*> saved_primary_sequence{VS::Actors::primarySequenceBuffer};
  ValueRestore<VS::Core::Gte::VECTOR> saved_scale{VS::Actors::roomActorScale};
  ValueRestore<VS::Actors::SkeletonPose> saved_pose{VS::Actors::skeletonPose};
  ValueRestore<VS::Core::Gte::MATRIX> saved_root_matrix{
      VS::Actors::skeletonRootMatrix};
  ValueRestore<VS::Core::Gte::MATRIX> saved_bone_camera{
      VS::Actors::boneMatrixScratch};
  ValueRestore<VS::Core::Gte::MATRIX> saved_camera_basis{
      VS::Render::cameraBasisMatrix};
  ValueRestore<VS::Actors::ModelRenderContext> saved_model_context{
      VS::Actors::modelRenderContext};
  ValueRestore<char> saved_sequence_index{
      VS::Actors::lastResolvedSequenceIndex};
  ValueRestore<std::remove_reference_t<decltype(*VS::Render::ModelWorkspace())>>
      saved_model_workspace{*VS::Render::ModelWorkspace()};
  AssetStateRestore saved_assets;
  AudioRestore saved_audio;
  ArrayRestore<std::uint16_t, std::size(st::statusStrings)> saved_status_text{
      st::statusStrings};
  ArrayRestore<std::uint16_t, std::size(VS::Battle::statusStrings)>
      saved_battle_text{VS::Battle::statusStrings.data()};
  ArrayRestore<int16_t, fw::kEquipmentStatCount> saved_stats{
      VS::Menu::Framework::equipmentStats};
  ArrayRestore<std::uint8_t, std::size(VS::Menu::Framework::animationIndices)>
      saved_animation{VS::Menu::Framework::animationIndices};
  std::vector<std::byte> arena = std::vector<std::byte>(kArenaBytes);
  memory::ScopedLiveGameHeap heap{arena};
  PreviewOwner preview_owner{arena};
  std::vector<std::uint32_t> primitives =
      std::vector<std::uint32_t>(kPrimitiveWords);
  std::array<std::uint32_t, kOrderingWords> ordering{};
  VS::Battle::ActorStats stats{};
  VS::Battle::Actor actor{};
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
    VS::Battle::actors[0] = &actor;
    for (auto& limb : stats.limbs) {
      limb.name_index = 1;
      limb.hp = limb.max_hp = 100;
    }
    workspace = memory::AllocateLiveFromHighEnd(kWorkspaceBytes);
    text = memory::AllocateLiveFromHighEnd(fw::kItemTextBytes);
    if (workspace == nullptr || text == nullptr) {
      throw std::runtime_error("terminal Live fixture allocation failed");
    }
    std::memset(workspace, 0, kWorkspaceBytes);
    VS::Menu::Framework::menuItems = static_cast<fw::MenuRow*>(workspace);
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
    VS::Core::Gte::Reset();
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
    for (void* target :
         {static_cast<void*>(VS::Actors::textureSlotOrigins.data()),
          static_cast<void*>(VS::Actors::textureSlotRowOffsets.data())}) {
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
            VS::Content::legacyActorAssetRequests,
            [](const auto& request) { return request.asset_kind != 0; }) ||
        VS::Content::legacyModelLoadPhase != 0 ||
        VS::Content::modelParseResetFlag != 0 ||
        VS::Content::modelParseBoundRecord ||
        VS::Content::legacySequenceParsePhase != 0) {
      throw std::runtime_error("terminal fixture requires an idle asset queue");
    }
    LoadTextureOrigins();
    VS::Actors::InitializeLiveActorModelResources();
    preview_owner.source = static_cast<VS::Actors::ActorRecord*>(
        memory::AllocateLiveFromLowEnd(sizeof(VS::Actors::ActorRecord)));
    if (preview_owner.source == nullptr) {
      throw std::runtime_error("source actor owner allocation failed");
    }
    *preview_owner.source = {};
    if (VS::Actors::primarySequenceBuffer == nullptr) {
      throw std::runtime_error("source sequence owner allocation failed");
    }
    ParseAsset(R"(Native\data\disc\OBJ\01.SHP)",
               {.kind = static_cast<std::uint8_t>(
                    content::ActorAssetKind::kActorModelParse),
                .target_slot = 0,
                .model_id = kSourceModel,
                .destination = Address(preview_owner.source),
                .shared_record_slot = {},
                .sub_kind = 0xFF});
    ParseAsset(R"(Native\data\disc\OBJ\01_COM.SEQ)",
               {.kind = static_cast<std::uint8_t>(
                    content::ActorAssetKind::kSequenceParse),
                .target_slot = 0,
                .shared_record_slot = {}});
    ParseAsset(R"(Native\data\disc\OBJ\01_BT2.SEQ)",
               {.kind = static_cast<std::uint8_t>(
                    content::ActorAssetKind::kSequenceParse),
                .target_slot = 0,
                .model_id = kSourceBattleSequence,
                .shared_record_slot = {}});
    const auto refused = content::LiveActorAssetRefusals();
    fw::LoadLiveActorPreview(0);
    const auto* preview = VS::Actors::actorRecords[1];
    if (content::LiveActorAssetRefusals() != refused || preview == nullptr ||
        preview->instance.bone_count == 0 ||
        !VS::Render::ValidateLiveActorModel(*preview) ||
        VS::Actors::SequenceBankBytes(preview->common_sequence).empty() ||
        VS::Actors::SequenceBankBytes(preview->battle_sequence).empty()) {
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
        reinterpret_cast<std::uint16_t*>(bytes.data() + fw::kItemNamesBytes);
    VS::Menu::Framework::menu12Text = reinterpret_cast<std::uint16_t*>(
        bytes.data() + fw::kItemNamesBytes + fw::kItemHelpBytes);
    std::fill_n(VS::Menu::Framework::itemHelp,
                equipment_detail_tests::kHelpEntries, std::uint16_t{4000});
    VS::Menu::Framework::itemHelp[4000] = 0xE751;
    std::ranges::fill(st::statusStrings, std::uint16_t{0xE7E7});
    std::fill_n(st::statusStrings, 61, std::uint16_t{400});
    st::statusStrings[400] = 0xE741;
    st::statusStrings[253] = 0x00FF;
    st::statusStrings[254] = 0xFF8F;
    st::statusStrings[255] = 0xE701;
    std::ranges::fill(VS::Battle::statusStrings, std::uint16_t{0xE7E7});
    VS::Battle::statusStrings[1] = 400;
    VS::Battle::statusStrings[400] = 0xE731;
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
    VS::Legacy::ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
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
