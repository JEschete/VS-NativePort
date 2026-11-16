// Hand-maintained (tools/port/owned.csv).
#include "Menu/Score/EncyclopediaPreview.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Content/ActorAssets.hpp"

namespace VS::Menu::Score {
namespace {
using Content::ActorAssetKind;
using Content::ActorAssetRequest;

// The requests' sub-kinds and the attachments' texture slots
// (592C.c:220-275).
constexpr std::uint8_t kEnemySubKind = 0xFF;
constexpr std::uint8_t kAttachmentSubKind = 1;
constexpr std::uint8_t kSequenceSubKind = 0;
constexpr std::uint8_t kCompanionTextureSlot = 0xFC;
constexpr std::uint8_t kWeaponTextureSlot = 0xF0;
constexpr std::uint8_t kShieldTextureSlot = 0xF1;
// The animation the posed preview plays (57BC.c:33).
constexpr int kPreviewAnimation = 1;
// _zudData_t's byte fields and sections (592C.c:25).
constexpr std::size_t kCharacterIdByte = 0;
constexpr std::size_t kWeaponByte = 1;
constexpr std::size_t kBattleSequenceModelByte = 2;
constexpr std::size_t kWeaponMaterialByte = 3;
constexpr std::size_t kShieldByte = 4;
constexpr std::size_t kShieldMaterialByte = 5;
constexpr std::size_t kDefaultMaterialByte = 6;
constexpr std::size_t kShapeSection = 0x08;
constexpr std::size_t kWeaponSection = 0x10;
constexpr std::size_t kShieldSection = 0x18;
constexpr std::size_t kCommonSequenceSection = 0x20;
constexpr std::size_t kBattleSequenceSection = 0x28;

[[nodiscard]] std::uint8_t Kind(ActorAssetKind kind) {
  return static_cast<std::uint8_t>(kind);
}

[[nodiscard]] constexpr std::uint8_t Step(EnemyPreviewStep step) {
  return static_cast<std::uint8_t>(step);
}

[[nodiscard]] std::uint32_t Word(std::span<const std::byte> bytes,
                                 std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < sizeof value; ++index) {
    value |= std::to_integer<std::uint32_t>(bytes[offset + index])
             << (CHAR_BIT * index);
  }
  return value;
}

[[nodiscard]] ZudSection Section(std::span<const std::byte> bytes,
                                 std::size_t offset) {
  return {.offset = Word(bytes, offset),
          .bytes = Word(bytes, offset + sizeof(std::uint32_t))};
}

// The address of `section` in the file block, or none when it runs past the
// file. The original added the offset unchecked (592C.c:217-273).
[[nodiscard]] std::optional<AssetAddress> SectionAddress(
    const EnemyPreviewState& state, std::size_t file_bytes,
    const ZudSection& section) {
  if (!ZudSectionFits(section, file_bytes)) return std::nullopt;
  return Content::AdvanceAddress(state.file,
                                 static_cast<std::ptrdiff_t>(section.offset));
}

// One part of a ZUD file. The original reused one request for every part,
// so a part's fields left unset carry over from the part queued before it.
struct PartQueue {
  EnemyPreviewHost& host;
  const EnemyPreviewState& state;
  std::size_t file_bytes = 0;
  ActorAssetRequest request{};
};

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:236-258)
// A weapon or shield model of the equipped layout.
void QueueEquipment(PartQueue& queue, const ZudSection& section,
                    std::uint8_t model, int slot, EnemyPreviewPart part,
                    std::uint8_t texture_slot, std::uint8_t material) {
  const std::optional<AssetAddress> data =
      SectionAddress(queue.state, queue.file_bytes, section);
  if (!data) {
    queue.host.Refuse("ZUD weapon or shield section past the file");
    return;
  }
  ActorAssetRequest& request = queue.request;
  request.kind = Kind(ActorAssetKind::kWeaponModelParse);
  request.target_slot = static_cast<std::int8_t>(slot);
  request.data = *data;
  request.model_id = model;
  request.destination = queue.host.ModelPart(queue.state.model_buffer, part);
  request.sub_kind = kAttachmentSubKind;
  request.texture_slot = texture_slot;
  request.material = material;
  queue.host.QueueAsset(request);
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:225-233)
void QueueCompanion(PartQueue& queue) {
  if (queue.state.variant >= kCompanionModels.size()) {
    queue.host.Refuse("companion variant past the companion models");
    return;
  }
  ActorAssetRequest& request = queue.request;
  request.kind = Kind(ActorAssetKind::kActorModelLoad);
  request.target_slot = kEnemyPreviewCompanionSlot;
  request.model_id = kCompanionModels[queue.state.variant];
  request.destination = queue.host.ModelPart(queue.state.model_buffer,
                                             EnemyPreviewPart::kCompanion);
  request.sub_kind = kAttachmentSubKind;
  request.texture_slot = kCompanionTextureSlot;
  request.variant = 0;
  queue.host.QueueAsset(request);
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:261-277)
// A battle or common sequence, queued when its section is not empty.
void QueueSequence(PartQueue& queue, const ZudSection& section,
                   std::uint8_t model) {
  if (section.bytes == 0) return;
  const std::optional<AssetAddress> data =
      SectionAddress(queue.state, queue.file_bytes, section);
  if (!data) {
    queue.host.Refuse("ZUD sequence section past the file");
    return;
  }
  ActorAssetRequest& request = queue.request;
  request.kind = Kind(ActorAssetKind::kSequenceParse);
  request.target_slot = kEnemyPreviewActorSlot;
  request.data = *data;
  request.sub_kind = kSequenceSubKind;
  request.model_id = model;
  queue.host.QueueAsset(request);
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:215-277, 0x80108278-0x80108480)
void QueueEnemyParts(EnemyPreviewHost& host, const EnemyPreviewState& state) {
  const std::span<const std::byte> file =
      host.FileBytes(state.file, state.file_bytes);
  const std::optional<ZudHeader> header = ParseZudHeader(file);
  if (!header) {
    host.Refuse("ZUD file shorter than its header: no parts queued");
    return;
  }
  if (state.model_buffer == AssetAddress::kNone) {
    host.Refuse("no model buffer: no parts queued");
    return;
  }
  const std::optional<AssetAddress> shape =
      SectionAddress(state, file.size(), header->shape);
  if (!shape) {
    host.Refuse("ZUD SHP section past the file: no parts queued");
    return;
  }

  PartQueue queue{.host = host, .state = state, .file_bytes = file.size()};
  ActorAssetRequest& request = queue.request;
  request.kind = Kind(ActorAssetKind::kActorModelParse);
  request.target_slot = kEnemyPreviewActorSlot;
  request.data = *shape;
  request.model_id = header->character_id;
  request.destination =
      host.ModelPart(state.model_buffer, EnemyPreviewPart::kEnemy);
  request.sub_kind = kEnemySubKind;
  request.variant = state.palette_flag & 1U;
  request.material = header->default_material;
  host.QueueAsset(request);

  if (state.variant != 0) {
    QueueCompanion(queue);
  } else {
    if (header->weapon != 0) {
      QueueEquipment(queue, header->weapon_model, header->weapon,
                     kEnemyPreviewWeaponSlot, EnemyPreviewPart::kWeapon,
                     kWeaponTextureSlot, header->weapon_material);
    }
    if (header->shield != 0) {
      QueueEquipment(queue, header->shield_model, header->shield,
                     kEnemyPreviewShieldSlot, EnemyPreviewPart::kShield,
                     kShieldTextureSlot, header->shield_material);
    }
  }
  QueueSequence(queue, header->battle_sequence, header->battle_sequence_model);
  QueueSequence(queue, header->common_sequence, 0);
}
}  // namespace

std::optional<ZudHeader> ParseZudHeader(std::span<const std::byte> file) {
  if (file.size() < kZudHeaderBytes) return std::nullopt;
  const auto byte = [&](std::size_t offset) {
    return std::to_integer<std::uint8_t>(file[offset]);
  };
  return ZudHeader{.character_id = byte(kCharacterIdByte),
                   .weapon = byte(kWeaponByte),
                   .battle_sequence_model = byte(kBattleSequenceModelByte),
                   .weapon_material = byte(kWeaponMaterialByte),
                   .shield = byte(kShieldByte),
                   .shield_material = byte(kShieldMaterialByte),
                   .default_material = byte(kDefaultMaterialByte),
                   .shape = Section(file, kShapeSection),
                   .weapon_model = Section(file, kWeaponSection),
                   .shield_model = Section(file, kShieldSection),
                   .common_sequence = Section(file, kCommonSequenceSection),
                   .battle_sequence = Section(file, kBattleSequenceSection)};
}

bool ZudSectionFits(const ZudSection& section, std::size_t file_bytes) {
  return section.offset <= file_bytes &&
         section.bytes <= file_bytes - section.offset;
}

// decomp: MENU9.PRG vs_menu9_loadZudFile (F02839,
// src/MENU/MENU9.PRG/592C.c:189-194, 0x801081CC-0x801081EC)
std::uint8_t CompanionVariant(int zud_id) {
  const auto found = std::ranges::find(kCompanionEnemies, zud_id);
  if (found == kCompanionEnemies.end()) return 0;
  return static_cast<std::uint8_t>(found - kCompanionEnemies.begin());
}

// decomp: MENU9.PRG vs_menu9_loadZud (F02836,
// src/MENU/MENU9.PRG/57BC.c:10-40, 0x80107FBC-0x80108094)
int LoadEnemyPreview(EnemyPreviewHost& host, EnemyPreviewState& state,
                     int zud_id) {
  switch (host.LoadStep()) {
    case Step(EnemyPreviewStep::kQueueRead):
      QueueEnemyModelRead(host, state, zud_id);
      host.SetLoadStep(static_cast<std::uint8_t>(host.LoadStep() + 1));
      break;
    case Step(EnemyPreviewStep::kLoadParts):
      if (LoadEnemyModelParts(host, state) != 1) {
        host.SetLoadStep(static_cast<std::uint8_t>(host.LoadStep() + 1));
      }
      break;
    case Step(EnemyPreviewStep::kPose):
      host.ResetPreviewPose();
      host.SetWeaponDrawn(kEnemyPreviewActorSlot, true);
      host.PlayAnimation(kEnemyPreviewActorSlot, kPreviewAnimation, 0);
      host.SetLoadStep(Step(EnemyPreviewStep::kQueueRead));
      return 0;
    default:
      break;
  }
  return 1;
}

// decomp: MENU9.PRG vs_menu9_freeZud (F02837,
// src/MENU/MENU9.PRG/57BC.c:42-47, 0x80108098-0x801080C4)
void FreeEnemyPreview(EnemyPreviewHost& host, EnemyPreviewState& state) {
  host.ReleaseModel(kEnemyPreviewActorSlot);
  host.ReleaseModel(kEnemyPreviewCompanionSlot);
  FreeEnemyModelBuffer(host, state);
}

// decomp: MENU9.PRG func_801080C8 (F02838,
// src/MENU/MENU9.PRG/57BC.c:49-59, 0x801080C8-0x80108128)
void InitializeAshleyPreviewPose(EnemyPreviewHost& host) {
  host.SetAnimation(kEnemyPreviewActorSlot, kAshleyPreviewAnimation);
  host.SetWeaponDrawn(kEnemyPreviewActorSlot, false);
  host.InitializeAnimationPose(kEnemyPreviewActorSlot);
  host.SetDisplayFlags(kEnemyPreviewActorSlot, kAshleyPreviewDisplayFlags);
  host.ClearCompanion(kEnemyPreviewActorSlot);
}

// decomp: MENU9.PRG vs_menu9_loadZudFile (F02839,
// src/MENU/MENU9.PRG/592C.c:171-201, 0x8010812C-0x80108238)
void QueueEnemyModelRead(EnemyPreviewHost& host, EnemyPreviewState& state,
                         int zud_id) {
  // The original indexed its 256-entry tables unchecked.
  if (zud_id < 0 || zud_id >= kEnemyPreviewFiles) {
    host.Refuse("ZUD id past the file table: no read queued");
    return;
  }
  // The original overwrote the handles, leaking the earlier blocks and
  // request; the encyclopedia frees and consumes them before a new read.
  if (state.read != AssetAddress::kNone || state.file != AssetAddress::kNone ||
      state.model_buffer != AssetAddress::kNone) {
    host.Refuse("an earlier preview still holds its read or blocks");
    return;
  }
  const EnemyPreviewFile file = host.File(zud_id);
  if (file.bytes < 0) {
    host.Refuse("negative ZUD file size: no read queued");
    return;
  }
  const auto file_bytes = static_cast<std::uint32_t>(file.bytes);
  state.zud_id = zud_id;
  AssetAddress read = host.AllocateRead(file);
  const AssetAddress data = host.AllocateFile(file_bytes);
  if (data == AssetAddress::kNone) {
    host.Refuse("no heap block for the ZUD file: no read queued");
    if (read != AssetAddress::kNone) host.ReleaseRead(read);
    return;
  }
  if (read != AssetAddress::kNone) {
    host.EnqueueRead(read, data);
  } else {
    read = host.ReadRefused(file, data);
  }
  state.read = read;
  state.file = data;
  state.file_bytes = file_bytes;
  state.palette_flag = file.palette_flag;
  state.variant = CompanionVariant(zud_id);
  state.model_buffer = host.AllocateModelBuffer(
      state.variant != 0 ? EnemyPreviewLayout::kCompanion
                         : EnemyPreviewLayout::kEquipped);
  if (state.model_buffer == AssetAddress::kNone) {
    host.Refuse("no heap block for the preview models");
  }
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:203-293, 0x8010823C-0x801084E0)
int LoadEnemyModelParts(EnemyPreviewHost& host, EnemyPreviewState& state) {
  if (state.read != AssetAddress::kNone) {
    if (!host.ReadLoaded(state.read)) return 1;
    host.ReleaseRead(state.read);
    state.read = AssetAddress::kNone;
    QueueEnemyParts(host, state);
    return 1;
  }
  if (state.file == AssetAddress::kNone) return 0;
  host.UpdateAssetQueue();
  // The queued parts read the file block until the queue drains.
  if (host.FirstFreeAssetRequest() == 0) {
    host.FreeFile(state.file);
    state.file = AssetAddress::kNone;
    state.file_bytes = 0;
  }
  return 1;
}

// decomp: MENU9.PRG vs_menu9_freeZudFile (F02841,
// src/MENU/MENU9.PRG/592C.c:295-299, 0x801084E4-0x80108510)
void FreeEnemyModelBuffer(EnemyPreviewHost& host, EnemyPreviewState& state) {
  // The original freed a null buffer as well; there is nothing to release.
  if (state.model_buffer != AssetAddress::kNone) {
    host.FreeModelBuffer(state.model_buffer);
  }
  state.model_buffer = AssetAddress::kNone;
}
}  // namespace VS::Menu::Score
