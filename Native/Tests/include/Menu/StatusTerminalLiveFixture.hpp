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

#include "Actors/ModelRenderingLive.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Status/ActorSwitchingLive.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLiveInternal.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/TexturePagesLive.hpp"

extern "C" {
#include "ActorAssetsLiveState.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/AppearanceLiveState.h"
#include "Actors/ModelRenderingLiveState.h"
#include "Actors/Movement.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/Textures.h"
#include "Actors/WeaponModel.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Field/Gimmicks.h"
#include "Field/RoomRenderingLiveState.h"
#include "Menu/Status/ActorStatus.h"
#include "ModelLoading.h"
#include "OverlayWorkspaceLiveState.h"
#include "Render/Camera.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Textures.h"
}

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
  ValueRestore<decltype(Menu_Framework_menuRowYOffset)> y{
      Menu_Framework_menuRowYOffset};
  ValueRestore<decltype(Menu_Framework_selectedRow)> selected{
      Menu_Framework_selectedRow};
  ValueRestore<decltype(Menu_Framework_subMenuId)> submenu{
      Menu_Framework_subMenuId};
  ValueRestore<decltype(Menu_Framework_subMenuPage)> page{
      Menu_Framework_subMenuPage};
  ValueRestore<decltype(Menu_Framework_subMenuRowCount)> rows{
      Menu_Framework_subMenuRowCount};
  ValueRestore<decltype(Menu_Framework_subMenuRowInfo)> info{
      Menu_Framework_subMenuRowInfo};
  ValueRestore<decltype(Menu_Framework_actionMenuState)> action{
      Menu_Framework_actionMenuState};
  ValueRestore<decltype(Menu_Framework_backgroundFadeStep)> fade{
      Menu_Framework_backgroundFadeStep};
  ValueRestore<decltype(Menu_Framework_cursorColor)> cursor{
      Menu_Framework_cursorColor};
  ValueRestore<decltype(Menu_Framework_displayActionCost)> cost{
      Menu_Framework_displayActionCost};
  ValueRestore<decltype(Menu_Framework_hideMenu)> hidden{
      Menu_Framework_hideMenu};
  ValueRestore<decltype(Menu_Framework_itemsListRow)> list{
      Menu_Framework_itemsListRow};
  ValueRestore<u_short*> items{Menu_Framework_itemsText};
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
  ValueRestore<unsigned char> parse_phase{Content_modelParseResetFlag};
  ValueRestore<u_char> sequence_phase{Content_legacySequenceParsePhase};
  ValueRestore<u_char> effect_phase{Content_legacyEtmLoadPhase};
  ValueRestore<u_char*> model_cursor{Content_legacyShpParseCursor};
  ValueRestore<u_char*> sequence_cursor{Content_legacySequenceParseData};
  ValueRestore<u_char> palette_words{Content_legacyShpCopiedWordCount};
  ValueRestore<int> header_word{Content_legacyShpHeaderWord};
  ValueRestore<int> texture_width{Content_legacyShpTextureWidth};
  ValueRestore<int> texture_height{Content_legacyShpTextureHeight};
  ValueRestore<int> peak{Content_assetQueuePeakUsage};
  ValueRestore<int> load_phase{Content_lastLoadPhaseSnapshot};
  ValueRestore<int> ticks{Content_assetQueueFrameTicks};
  ValueRestore<int> kind{Content_lastAssetRequestType};
  ArrayRestore<Render_TextureSlot, VS::Content::kActorTextureSlots>
      actor_textures{Render_legacyActorTextureSlots};
  ArrayRestore<Render_TextureSlot, VS::Content::kWeaponTextureSlots>
      weapon_textures{Render_legacyWeaponTextureSlots};
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
  ValueRestore<char> reserved{Actors_reservedControlByte};
  ValueRestore<u_char> bank_index{Actors_sharedSequenceBankIndex};
  ValueRestore<char> update_complete{Actors_isActorUpdateComplete};
  ValueRestore<u_char> pushed_gimmick{Actors_pushedGimmickIndex};
  ValueRestore<decltype(Menu_Inventory_equipmentModelLoadStep)> equipment_step{
      Menu_Inventory_equipmentModelLoadStep};
  ValueRestore<void*> sequence_work{Actors_sequenceWorkBuffer};
};

struct AudioRestore {
  ArrayRestore<Audio_CommandHandler, std::size(Audio_Mixer_kCommandHandlers)>
      handlers{Audio_Mixer_kCommandHandlers};
  ValueRestore<Audio_CommandArguments> arguments{Audio_Driver_commandArguments};
  ValueRestore<Audio_CommandParams> parameters{Audio_Mixer_commandParams};
  ValueRestore<Audio_DriverFlags> controls{Audio_Driver_controlLatches};
  ValueRestore<Audio_VoiceScheduler> voices{Audio_Mixer_voiceScheduler};
  ArrayRestore<Audio_SeqChannel, std::size(Audio_SoundEffects_channels)>
      effects{Audio_SoundEffects_channels};

  AudioRestore() {
    const auto image = ReadOriginal(R"(Native\data\disc\SLUS_010.40)");
    const auto tables =
        std::span(Content_modulePointers, Content_modulePointerCount);
    const auto table = std::ranges::find(
        tables, reinterpret_cast<void**>(Audio_Mixer_kCommandHandlers),
        &Content_ModulePointerTable::target);
    if (table == tables.end() ||
        !VS::Content::RelocateModulePointerTable(
            *table, image,
            {Content_moduleSymbols, Content_moduleSymbolCount})) {
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
  ValueRestore<Render_FrameScratch> saved_scratch{Render_frameScratch};
  ValueRestore<int> saved_projection{Render_projectionDistance};
  ValueRestore<int> saved_framebuffer{Render_frameBuf};
  ValueRestore<void*> saved_primitives{Render_legacyPrimitiveBuffers[0]};
  ValueRestore<void*> saved_ordering{Render_legacyOrderingTables[0]};
  ValueRestore<void*> saved_current_ordering{Render_legacyCurrentOrderingTable};
  ValueRestore<char> saved_owner{Content_overlayWorkspaceOwner};
  ValueRestore<char> saved_hud{Battle_hudHidden};
  ValueRestore<char> saved_lower_ui{Battle_lowerScreenUiState};
  ValueRestore<decltype(Field_isFieldRenderingEnabled)> saved_field{
      Field_isFieldRenderingEnabled};
  ValueRestore<Menu_Framework_MenuControlState> saved_control{
      Menu_Framework_menuState};
  ValueRestore<Menu_Framework_MenuRow*> saved_rows{Menu_Framework_menuItems};
  ValueRestore<char*> saved_strings{Menu_Framework_stringBuf};
  ValueRestore<int*> saved_types{Menu_Framework_rowTypeBuf};
  ValueRestore<u_short*> saved_list_text{Menu_Framework_itemsText};
  ValueRestore<decltype(Menu_Framework_itemNames)> saved_names{
      Menu_Framework_itemNames};
  ValueRestore<u_short*> saved_help{Menu_Framework_itemHelp};
  ValueRestore<u_short*> saved_menu12{Menu_Framework_menu12Text};
  ValueRestore<Menu_Framework_MenuMemory> saved_memory{
      Menu_Framework_menuMemory};
  ValueRestore<Menu_StringContext> saved_context{Battle_stringContext};
  ArrayRestore<char, kMenuSubmenuStateCount> saved_submenus{
      Menu_Framework_submenuStates};
  ArrayRestore<Battle_Actor*, std::size(Battle_actors)> saved_actors{
      Battle_actors};
  ArrayRestore<Actors_ActorRecord*, std::size(Actors_actorRecords)>
      saved_models{Actors_actorRecords};
  ArrayRestore<Actors_WeaponModel*, std::size(Actors_weaponModels)>
      saved_weapons{Actors_weaponModels};
  ValueRestore<void*> saved_primary_sequence{Actors_primarySequenceBuffer};
  ValueRestore<VECTOR> saved_scale{Actors_roomActorScale};
  ValueRestore<Actors_SkeletonPose> saved_pose{Actors_skeletonPose};
  ValueRestore<MATRIX> saved_root_matrix{Actors_skeletonRootMatrix};
  ValueRestore<MATRIX> saved_bone_camera{Actors_boneMatrixScratch};
  ValueRestore<MATRIX> saved_camera_basis{Render_cameraBasisMatrix};
  ValueRestore<Actors_ModelRenderContext> saved_model_context{
      Actors_modelRenderContext};
  ValueRestore<char> saved_sequence_index{Actors_lastResolvedSequenceIndex};
  ValueRestore<std::remove_reference_t<decltype(*Render_ModelWorkspace())>>
      saved_model_workspace{*Render_ModelWorkspace()};
  AssetStateRestore saved_assets;
  AudioRestore saved_audio;
  ArrayRestore<u_short, std::size(Menu_Status_statusStrings)> saved_status_text{
      Menu_Status_statusStrings};
  ArrayRestore<u_short, std::size(Battle_statusStrings)> saved_battle_text{
      Battle_statusStrings};
  ArrayRestore<int16_t, fw::kEquipmentStatCount> saved_stats{
      Menu_Framework_equipmentStats};
  ArrayRestore<char, std::size(Menu_Framework_animationIndices)>
      saved_animation{Menu_Framework_animationIndices};
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
    Menu_Framework_menuItems = static_cast<Menu_Framework_MenuRow*>(workspace);
    Menu_Framework_stringBuf =
        reinterpret_cast<char*>(Menu_Framework_menuItems + fw::kRowCount);
    Menu_Framework_rowTypeBuf = reinterpret_cast<int*>(
        Menu_Framework_stringBuf + fw::kStringBufferBytes);
    Content_overlayWorkspaceOwner = 6;
    BindText();
    BindFrame();
    BindPreview();
    BindFrame();
  }
  ~Fixture() {
    st::ResetLiveLimbControllersModuleData();
    st::ResetLiveActorSwitchPhase();
    // The preview draw sets the GTE screen distance. Render_projectionDistance
    // is not that distance's previous value (0 versus 1000 at start-up), so
    // reset the GTE; saved_projection and geometry restore their own state.
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
    const auto entries = std::span(Content_moduleData, Content_moduleDataCount);
    for (void* target : {static_cast<void*>(Actors_textureSlotOrigins),
                         static_cast<void*>(Actors_textureSlotRowOffsets)}) {
      const auto entry =
          std::ranges::find(entries, target, &Content_ModuleDataEntry::target);
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
        Content_legacyModelLoadPhase != 0 || Content_modelParseResetFlag != 0 ||
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
    Menu_Framework_itemNames =
        static_cast<char (*)[fw::kItemNameRowBytes]>(text);
    Menu_Framework_itemHelp =
        reinterpret_cast<u_short*>(bytes.data() + fw::kItemNamesBytes);
    Menu_Framework_menu12Text = reinterpret_cast<u_short*>(
        bytes.data() + fw::kItemNamesBytes + fw::kItemHelpBytes);
    std::fill_n(Menu_Framework_itemHelp, equipment_detail_tests::kHelpEntries,
                u_short{4000});
    Menu_Framework_itemHelp[4000] = 0xE751;
    std::ranges::fill(Menu_Status_statusStrings, u_short{0xE7E7});
    std::fill_n(Menu_Status_statusStrings, 61, u_short{400});
    Menu_Status_statusStrings[400] = 0xE741;
    Menu_Status_statusStrings[253] = 0x00FF;
    Menu_Status_statusStrings[254] = 0xFF8F;
    Menu_Status_statusStrings[255] = 0xE701;
    std::ranges::fill(Battle_statusStrings, u_short{0xE7E7});
    Battle_statusStrings[1] = 400;
    Battle_statusStrings[400] = 0xE731;
    std::ranges::fill(Menu_Framework_animationIndices, char{10});
  }
  void BindFrame() {
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = primitives.data();
    Render_legacyOrderingTables[0] = ordering.data();
    Render_legacyCurrentOrderingTable = ordering.data();
    Render_frameScratch.orderingTable =
        ordering.data() + equipment_detail_tests::kSceneOrderingEntry;
    Render_frameScratch.overlayOrderingTable = ordering.data() + 2;
    Render_frameScratch.viewMatrix = {};
    Render_frameScratch.camera = {};
    BeginFrame();
  }
  void BeginFrame() {
    Render_frameScratch.primitiveCursor = primitives.data();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }
  void IdleRows() const {
    for (std::size_t row = 0; row < fw::kRowCount; ++row) {
      fw::LiveMenuRow(static_cast<int>(row)).state = fw::RowState::kIdle;
    }
  }
  [[nodiscard]] std::uint8_t* State() const {
    return reinterpret_cast<std::uint8_t*>(&Menu_Framework_submenuStates[4]);
  }
};
}  // namespace status_terminal_live_tests
