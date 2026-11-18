#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "Actors/ModelRenderingLive.hpp"
#include "Audio/SequencerOutput.h"
#include "Content/ActorAssetsLive.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/GazetteTerminalLiveFixture.hpp"
#include "Menu/Score/EncyclopediaPreview.hpp"
#include "Menu/Score/EncyclopediaPreviewLive.hpp"

extern "C" {
#include "ActorAssetsLiveState.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/AppearanceLiveState.h"
#include "Actors/ModelRenderingLiveState.h"
#include "Actors/Movement.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/Textures.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Field/Gimmicks.h"
#include "Menu/Inventory/Inventory.h"
#include "ModelLoading.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Textures.h"
}

namespace gazette_terminal_live_tests {
inline content::AssetAddress Address(const void* pointer) {
  return static_cast<content::AssetAddress>(
      reinterpret_cast<std::uintptr_t>(pointer));
}

inline bool AssetsIdle() {
  return std::ranges::all_of(
      Content_legacyActorAssetRequests,
      [](const auto& request) { return request.assetKind == 0; });
}

inline auto PreviewOwners() {
  return std::array{BytesOf(Audio_Mixer_kCommandHandlers),
                    BytesOf(Audio_Driver_commandArguments),
                    BytesOf(Audio_Mixer_commandParams),
                    BytesOf(Audio_Driver_controlLatches),
                    BytesOf(Audio_Mixer_voiceScheduler),
                    BytesOf(Audio_SoundEffects_channels),
                    BytesOf(Actors_actorRecords),
                    BytesOf(Actors_weaponModels),
                    BytesOf(Field_gimmickInstances),
                    BytesOf(Actors_primarySequenceBuffer),
                    BytesOf(Actors_sequenceWorkBuffer),
                    BytesOf(Actors_forcedModelOverrides),
                    BytesOf(Content_legacyWeaponModelBuffers),
                    BytesOf(Render_legacyActorTextureSlots),
                    BytesOf(Render_legacyWeaponTextureSlots),
                    BytesOf(Actors_sharedSequenceBankTexturePages),
                    BytesOf(Actors_sharedSequenceData),
                    BytesOf(Actors_sharedSequenceAnimationIds),
                    BytesOf(Actors_sharedSequenceEntryParams),
                    BytesOf(Actors_sharedSequenceAnimationData),
                    BytesOf(Actors_sharedSequenceBankIndex),
                    BytesOf(Actors_lastResolvedSequenceIndex),
                    BytesOf(Actors_roomActorScale),
                    BytesOf(Actors_pushedGimmickIndex),
                    BytesOf(Actors_isActorUpdateComplete),
                    BytesOf(Actors_skeletonPose),
                    BytesOf(Actors_skeletonRootMatrix),
                    BytesOf(Actors_boneMatrixScratch),
                    BytesOf(Actors_modelRenderContext),
                    BytesOf(*Render_ModelWorkspace()),
                    BytesOf(Content_legacyActorAssetRequests),
                    BytesOf(Content_legacyModelLoadPhase),
                    BytesOf(Content_legacySequenceParsePhase),
                    BytesOf(Content_legacyEtmLoadPhase),
                    BytesOf(Content_legacyWeaponFileBuffer),
                    BytesOf(Content_legacyWeaponFileLoaded),
                    BytesOf(Content_legacyWeaponFileLocation),
                    BytesOf(Content_legacyWeaponDiscRequest),
                    BytesOf(Content_legacyWeaponParseCursor),
                    BytesOf(Content_legacyShpFileBuffer),
                    BytesOf(Content_legacyShpAllocated),
                    BytesOf(Content_legacyShpFileLocation),
                    BytesOf(Content_legacyShpDiscRequest),
                    BytesOf(Content_legacyShpParseCursor),
                    BytesOf(Content_legacyShpCopiedWordCount),
                    BytesOf(Content_legacyShpHeaderWord),
                    BytesOf(Content_legacyShpTextureWidth),
                    BytesOf(Content_legacyShpTextureHeight),
                    BytesOf(Content_legacySequenceFileBuffer),
                    BytesOf(Content_legacySequenceBufferOwned),
                    BytesOf(Content_legacySequenceFileLocation),
                    BytesOf(Content_legacySequenceDiscRequest),
                    BytesOf(Content_legacySequenceParseData),
                    BytesOf(Content_legacyEtmFileBuffer),
                    BytesOf(Content_legacyEtmFileLocation),
                    BytesOf(Content_legacyEtmDiscRequest),
                    BytesOf(Content_modelParseResetFlag),
                    BytesOf(Content_assetQueuePeakUsage),
                    BytesOf(Content_lastLoadPhaseSnapshot),
                    BytesOf(Content_assetQueueFrameTicks),
                    BytesOf(Content_lastAssetRequestType),
                    BytesOf(Menu_Inventory_equipmentModelLoadStep),
                    BytesOf(score::LiveEnemyPreviewState())};
}

// Pending file/asset sources must be drained before destruction: the existing
// preview release owns completed models, not cancellation of pending reads.
class PreviewFixture : public TerminalFixture {
 private:
  SavedRegions saved_;
  void* source_record_ = nullptr;
  std::byte* source_file_ = nullptr;

 public:
  PreviewFixture() {
    if (!AssetsIdle() ||
        score::LiveEnemyPreviewState() != score::EnemyPreviewState() ||
        std::ranges::any_of(Actors_actorRecords, [](const auto* actor) {
          return actor != nullptr;
        })) {
      throw std::runtime_error("terminal preview fixture requires idle actors");
    }
    for (const auto owner : PreviewOwners()) saved_.Keep(owner);
    const auto pointers =
        std::span(Content_modulePointers, Content_modulePointerCount);
    const auto commands = std::ranges::find_if(pointers, [](const auto& entry) {
      return static_cast<void*>(entry.target) ==
             static_cast<void*>(Audio_Mixer_kCommandHandlers);
    });
    const auto executable =
        shared::ReadImage(R"(Native\data\disc\SLUS_010.40)");
    if (commands == pointers.end() ||
        commands->capacity != std::size(Audio_Mixer_kCommandHandlers) ||
        commands->module == nullptr ||
        std::string_view(commands->module) != "SLUS_010.40" ||
        !content::RelocateModulePointerTable(
            *commands, executable,
            std::span(Content_moduleSymbols, Content_moduleSymbolCount))) {
      throw std::runtime_error("terminal native sound command import failed");
    }
    ImportTable(Actors_textureSlotOrigins);
    ImportTable(Actors_textureSlotRowOffsets);
    ImportTable(Actors_sheathedWeaponAnimationBases);
    ImportTable(Actors_drawnWeaponAnimationBases);
    ImportTable(Actors_animationEventLengths);
    ImportTable(Actors_animationEventPoints);
    ImportTable(Actors_sharedSequenceIdsByKind);
    VS::Actors::InitializeLiveActorModelResources();
    score::ResetLiveEnemyPreview();
  }

  ~PreviewFixture() {
    if (score::LiveEnemyPreviewState().file != content::AssetAddress::kNone ||
        !AssetsIdle()) {
      Core_Errorf("tests", "terminal preview did not drain its genuine assets");
      std::abort();
    }
    if (score::LiveEnemyPreviewState().model_buffer !=
        content::AssetAddress::kNone) {
      score::FreeLiveEnemyPreview();
    } else if (Actors_actorRecords[1] != nullptr) {
      fw::ReleaseLiveActorPreview();
    }
    static_cast<void>(content::ReleaseLiveActorModel(0));
    for (void* allocation :
         {source_record_, static_cast<void*>(source_file_),
          Actors_primarySequenceBuffer, Actors_sequenceWorkBuffer,
          static_cast<void*>(Content_legacyWeaponModelBuffers[0]),
          static_cast<void*>(Content_legacyWeaponModelBuffers[1])}) {
      if (allocation != nullptr && memory::LivePayloadCapacity(allocation)) {
        memory::FreeLiveFromLowEnd(allocation);
      }
    }
    score::ResetLiveEnemyPreview();
    saved_.Restore();
  }
  PreviewFixture(const PreviewFixture&) = delete;
  PreviewFixture& operator=(const PreviewFixture&) = delete;

  void DrainAssets() {
    for (std::size_t tick = 0; tick < kFrameBudget; ++tick) {
      content::UpdateLiveDiscQueue();
      if (content::UpdateLiveActorAssetQueue() == 0) return;
    }
    throw std::runtime_error("terminal real actor asset queue did not drain");
  }

  void LoadSourceActor() {
    const auto* store = content::DataStore::Get();
    const auto* entry =
        store == nullptr ? nullptr : store->Find("MAP/Z063U00.ZUD");
    if (entry == nullptr) {
      throw std::runtime_error("terminal fixture needs original Ashley ZUD");
    }
    source_file_ =
        static_cast<std::byte*>(memory::AllocateLiveFromHighEnd(entry->size));
    source_record_ = memory::AllocateLiveFromLowEnd(sizeof(Actors_ActorRecord));
    if (source_file_ == nullptr || source_record_ == nullptr ||
        content::ReadLiveDiscFileBlocking(static_cast<int>(entry->lba),
                                          static_cast<int>(entry->size),
                                          source_file_) != 1) {
      throw std::runtime_error("terminal source actor could not be read");
    }
    const auto header = score::ParseZudHeader({source_file_, entry->size});
    if (!header || header->character_id != 0 ||
        !score::ZudSectionFits(header->shape, entry->size) ||
        !score::ZudSectionFits(header->common_sequence, entry->size) ||
        !score::ZudSectionFits(header->battle_sequence, entry->size)) {
      throw std::runtime_error("terminal source ZUD sections are not bounded");
    }
    content::ActorAssetRequest model{
        .kind = static_cast<std::uint8_t>(
            content::ActorAssetKind::kActorModelParse),
        .target_slot = 0,
        .model_id = header->character_id,
        .destination = Address(source_record_),
        .data = Address(source_file_ + header->shape.offset),
        .sub_kind = 0xFF,
        .material = header->default_material};
    if (content::QueueLiveActorAsset(model) < 0) {
      throw std::runtime_error("terminal source actor request was refused");
    }
    for (const auto& [section, model_id] : std::array{
             std::pair{
                 header->battle_sequence,
                 static_cast<std::int16_t>(header->battle_sequence_model)},
             std::pair{header->common_sequence, std::int16_t{0}}}) {
      if (section.bytes == 0) continue;
      const content::ActorAssetRequest sequence{
          .kind = static_cast<std::uint8_t>(
              content::ActorAssetKind::kSequenceParse),
          .target_slot = 0,
          .model_id = model_id,
          .data = Address(source_file_ + section.offset)};
      if (content::QueueLiveActorAsset(sequence) < 0) {
        throw std::runtime_error(
            "terminal source sequence request was refused");
      }
    }
    DrainAssets();
    if (!content::CancelLiveActorAssetBorrows({source_file_, entry->size},
                                              {})) {
      throw std::runtime_error("terminal source parser borrows did not drain");
    }
    memory::FreeLiveFromHighEnd(source_file_);
    source_file_ = nullptr;
  }

  void LoadEnemy(int model) {
    for (std::size_t tick = 0; tick < kFrameBudget; ++tick) {
      if (TickEnemyPreview(model) == 0) return;
    }
    throw std::runtime_error("terminal real enemy preview did not load");
  }

  int TickEnemyPreview(int model, bool pump_disc = true) {
    if (pump_disc) content::UpdateLiveDiscQueue();
    return score::LoadLiveEnemyPreview(model);
  }

  int TickGazette(bool pump_disc = true) {
    ResetFrame();
    if (pump_disc) content::UpdateLiveDiscQueue();
    fw::UpdateLiveScreen();
    const int result = score::UpdateLiveGazette();
    Input();
    return result;
  }

  void AdvanceTo(int phase, bool pump_disc = true) {
    for (std::size_t tick = 0; tick < kFrameBudget; ++tick) {
      if (Menu_Score_menuState == phase) return;
      if (TickGazette(pump_disc) != 0) break;
    }
    throw std::runtime_error("terminal controller did not reach phase " +
                             std::to_string(phase));
  }

  void SettleOverview(bool pump_disc = true) {
    AdvanceTo(2, pump_disc);
    for (int tick = 0; tick < 32; ++tick) {
      if (TickGazette(pump_disc) != 0) {
        throw std::runtime_error("terminal overview unexpectedly exited");
      }
    }
  }

  void SelectPage(int page, bool pump_disc = true) {
    SettleOverview(pump_disc);
    for (int attempt = 0; attempt < 4 && fw::LiveHighlightedRow() != page;
         ++attempt) {
      Input(kDown, kDown);
      static_cast<void>(TickGazette(pump_disc));
    }
    if (fw::LiveHighlightedRow() != page) {
      throw std::runtime_error("real controller did not select Gazette page");
    }
    Input(kCircle);
    static_cast<void>(TickGazette(pump_disc));
    if (Menu_Score_menuState != 3) {
      throw std::runtime_error("real confirmation did not leave overview");
    }
  }
};
}  // namespace gazette_terminal_live_tests
