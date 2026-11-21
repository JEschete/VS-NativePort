#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AppearanceLiveState.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Actors/Movement.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/Textures.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/GazetteTerminalLiveFixture.hpp"
#include "Menu/Score/EncyclopediaPreview.hpp"
#include "Menu/Score/EncyclopediaPreviewLive.hpp"
#include "Render/ModelDraw.hpp"
#include "Render/ModelWorkspace.hpp"
#include "Render/Textures.hpp"

namespace gazette_terminal_live_tests {
inline content::AssetAddress Address(const void* pointer) {
  return static_cast<content::AssetAddress>(
      reinterpret_cast<std::uintptr_t>(pointer));
}

inline bool AssetsIdle() {
  return std::ranges::all_of(
      VS::Content::legacyActorAssetRequests,
      [](const auto& request) { return request.asset_kind == 0; });
}

inline auto PreviewOwners() {
  return std::array{BytesOf(VS::Audio::Mixer::kCommandHandlers),
                    BytesOf(VS::Audio::Driver::commandArguments),
                    BytesOf(VS::Audio::Mixer::commandParams),
                    BytesOf(VS::Audio::Driver::controlLatches),
                    BytesOf(VS::Audio::Mixer::voiceScheduler),
                    BytesOf(VS::Audio::SoundEffects::channels),
                    BytesOf(VS::Actors::actorRecords),
                    BytesOf(VS::Actors::weaponModels),
                    BytesOf(VS::Field::gimmickInstances),
                    BytesOf(VS::Actors::primarySequenceBuffer),
                    BytesOf(VS::Actors::sequenceWorkBuffer),
                    BytesOf(VS::Actors::forcedModelOverrides),
                    BytesOf(VS::Content::legacyWeaponModelBuffers),
                    BytesOf(VS::Render::legacyActorTextureSlots),
                    BytesOf(VS::Render::legacyWeaponTextureSlots),
                    BytesOf(VS::Actors::sharedSequenceBankTexturePages),
                    BytesOf(VS::Actors::sharedSequenceData),
                    BytesOf(VS::Actors::sharedSequenceAnimationIds),
                    BytesOf(VS::Actors::sharedSequenceEntryParams),
                    BytesOf(VS::Actors::sharedSequenceAnimationData),
                    BytesOf(VS::Actors::sharedSequenceBankIndex),
                    BytesOf(VS::Actors::lastResolvedSequenceIndex),
                    BytesOf(VS::Actors::roomActorScale),
                    BytesOf(VS::Actors::pushedGimmickIndex),
                    BytesOf(VS::Actors::isActorUpdateComplete),
                    BytesOf(VS::Actors::skeletonPose),
                    BytesOf(VS::Actors::skeletonRootMatrix),
                    BytesOf(VS::Actors::boneMatrixScratch),
                    BytesOf(VS::Actors::modelRenderContext),
                    BytesOf(*VS::Render::ModelWorkspace()),
                    BytesOf(VS::Content::legacyActorAssetRequests),
                    BytesOf(VS::Content::legacyModelLoadPhase),
                    BytesOf(VS::Content::legacySequenceParsePhase),
                    BytesOf(VS::Content::legacyEtmLoadPhase),
                    BytesOf(VS::Content::legacyWeaponFileBuffer),
                    BytesOf(VS::Content::legacyWeaponFileLoaded),
                    BytesOf(VS::Content::legacyWeaponFileLocation),
                    BytesOf(VS::Content::legacyWeaponDiscRequest),
                    BytesOf(VS::Content::legacyWeaponParseCursor),
                    BytesOf(VS::Content::legacyShpFileBuffer),
                    BytesOf(VS::Content::legacyShpAllocated),
                    BytesOf(VS::Content::legacyShpFileLocation),
                    BytesOf(VS::Content::legacyShpDiscRequest),
                    BytesOf(VS::Content::legacyShpParseCursor),
                    BytesOf(VS::Content::legacyShpCopiedWordCount),
                    BytesOf(VS::Content::legacyShpHeaderWord),
                    BytesOf(VS::Content::legacyShpTextureWidth),
                    BytesOf(VS::Content::legacyShpTextureHeight),
                    BytesOf(VS::Content::legacySequenceFileBuffer),
                    BytesOf(VS::Content::legacySequenceBufferOwned),
                    BytesOf(VS::Content::legacySequenceFileLocation),
                    BytesOf(VS::Content::legacySequenceDiscRequest),
                    BytesOf(VS::Content::legacySequenceParseData),
                    BytesOf(VS::Content::legacyEtmFileBuffer),
                    BytesOf(VS::Content::legacyEtmFileLocation),
                    BytesOf(VS::Content::legacyEtmDiscRequest),
                    BytesOf(VS::Content::modelParseResetFlag),
                    BytesOf(VS::Content::modelParseBoundRecord),
                    BytesOf(VS::Content::assetQueuePeakUsage),
                    BytesOf(VS::Content::lastLoadPhaseSnapshot),
                    BytesOf(VS::Content::assetQueueFrameTicks),
                    BytesOf(VS::Content::lastAssetRequestType),
                    BytesOf(VS::Content::objectDataLoadStep),
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
        std::ranges::any_of(VS::Actors::actorRecords, [](const auto* actor) {
          return actor != nullptr;
        })) {
      throw std::runtime_error("terminal preview fixture requires idle actors");
    }
    for (const auto owner : PreviewOwners()) saved_.Keep(owner);
    ImportTable(VS::Actors::textureSlotOrigins);
    ImportTable(VS::Actors::textureSlotRowOffsets);
    ImportTable(VS::Actors::sheathedWeaponAnimationBases);
    ImportTable(VS::Actors::drawnWeaponAnimationBases);
    ImportTable(VS::Actors::animationEventLengths);
    ImportTable(VS::Actors::animationEventPoints);
    ImportTable(VS::Actors::sharedSequenceIdsByKind);
    VS::Actors::InitializeLiveActorModelResources();
    score::ResetLiveEnemyPreview();
  }

  ~PreviewFixture() {
    if (score::LiveEnemyPreviewState().file != content::AssetAddress::kNone ||
        !AssetsIdle()) {
      VS::Core::Errorf("tests",
                       "terminal preview did not drain its genuine assets");
      std::abort();
    }
    if (score::LiveEnemyPreviewState().model_buffer !=
        content::AssetAddress::kNone) {
      score::FreeLiveEnemyPreview();
    } else if (VS::Actors::actorRecords[1] != nullptr) {
      fw::ReleaseLiveActorPreview();
    }
    static_cast<void>(content::ReleaseLiveActorModel(0));
    for (void* allocation :
         {source_record_, static_cast<void*>(source_file_),
          VS::Actors::primarySequenceBuffer, VS::Actors::sequenceWorkBuffer,
          static_cast<void*>(VS::Content::legacyWeaponModelBuffers[0]),
          static_cast<void*>(VS::Content::legacyWeaponModelBuffers[1])}) {
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
    source_record_ =
        memory::AllocateLiveFromLowEnd(sizeof(VS::Actors::ActorRecord));
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
    // A drained queue leaves no parse in progress over the source file.
    if (VS::Content::modelParseResetFlag != 0 ||
        VS::Content::modelParseBoundRecord ||
        VS::Content::legacySequenceParsePhase != 0) {
      throw std::runtime_error("terminal source parser did not finish");
    }
    memory::FreeLiveFromHighEnd(source_file_);
    source_file_ = nullptr;
  }

  void LoadEnemy(int model) {
    for (std::size_t tick = 0; tick < kFrameBudget; ++tick) {
      const int result = TickEnemyPreview(model);
      if (result == 0) return;
      if (result == score::kEnemyPreviewFailed) {
        throw std::runtime_error("terminal real enemy preview failed");
      }
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
      if (VS::Menu::Score::menuState == phase) return;
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
    if (VS::Menu::Score::menuState != 3) {
      throw std::runtime_error("real confirmation did not leave overview");
    }
  }
};
}  // namespace gazette_terminal_live_tests
