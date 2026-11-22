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
using Content::ActorAssetOutcome;
using Content::ActorAssetRequest;
using Content::ActorAssetTicket;

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
  EnemyPreviewState& state;
  std::size_t file_bytes = 0;
  ActorAssetRequest request{};
};

// Native (#48): the original dropped a part the full queue refused. The part
// is submitted under the preview's owner and its ticket kept; a part the
// queue refuses fails the whole load.
bool Submit(PartQueue& queue) {
  auto& tickets = queue.state.tickets;
  const auto free = std::ranges::find(tickets, ActorAssetTicket::kNone);
  if (free == tickets.end()) {
    queue.host.Refuse("more ZUD parts than preview tickets");
    return false;
  }
  const std::optional<ActorAssetTicket> ticket =
      queue.host.SubmitAsset(queue.request);
  if (!ticket) {
    queue.host.Refuse("the asset queue refused a ZUD part");
    return false;
  }
  *free = *ticket;
  return true;
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:236-258)
// A weapon or shield model of the equipped layout. Native (#48): its record
// binds into the model buffer (EnemyPreviewRecordMemory).
bool QueueEquipment(PartQueue& queue, const ZudSection& section,
                    std::uint8_t model, int slot, std::uint8_t texture_slot,
                    std::uint8_t material) {
  const std::optional<AssetAddress> data =
      SectionAddress(queue.state, queue.file_bytes, section);
  if (!data) {
    queue.host.Refuse("ZUD weapon or shield section past the file");
    return false;
  }
  ActorAssetRequest& request = queue.request;
  request.kind = Kind(ActorAssetKind::kWeaponModelParse);
  request.target_slot = static_cast<std::int8_t>(slot);
  request.data = *data;
  request.model_id = model;
  request.sub_kind = kAttachmentSubKind;
  request.texture_slot = texture_slot;
  request.material = material;
  return Submit(queue);
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:225-233)
bool QueueCompanion(PartQueue& queue) {
  if (queue.state.variant >= kCompanionModels.size()) {
    queue.host.Refuse("companion variant past the companion models");
    return false;
  }
  ActorAssetRequest& request = queue.request;
  request.kind = Kind(ActorAssetKind::kActorModelLoad);
  request.target_slot = kEnemyPreviewCompanionSlot;
  request.model_id = kCompanionModels[queue.state.variant];
  request.sub_kind = kAttachmentSubKind;
  request.texture_slot = kCompanionTextureSlot;
  request.variant = 0;
  return Submit(queue);
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:261-277)
// A battle or common sequence, queued when its section is not empty.
bool QueueSequence(PartQueue& queue, const ZudSection& section,
                   std::uint8_t model) {
  if (section.bytes == 0) return true;
  const std::optional<AssetAddress> data =
      SectionAddress(queue.state, queue.file_bytes, section);
  if (!data) {
    queue.host.Refuse("ZUD sequence section past the file");
    return false;
  }
  ActorAssetRequest& request = queue.request;
  request.kind = Kind(ActorAssetKind::kSequenceParse);
  request.target_slot = kEnemyPreviewActorSlot;
  request.data = *data;
  request.sub_kind = kSequenceSubKind;
  request.model_id = model;
  return Submit(queue);
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:215-277, 0x80108278-0x80108480)
// False at the first part that cannot be queued; the caller abandons the
// parts already queued.
bool QueueEnemyParts(EnemyPreviewHost& host, EnemyPreviewState& state) {
  const std::span<const std::byte> file =
      host.FileBytes(state.file, state.file_bytes);
  const std::optional<ZudHeader> header = ParseZudHeader(file);
  if (!header) {
    host.Refuse("ZUD file shorter than its header: no parts queued");
    return false;
  }
  if (state.model_buffer == AssetAddress::kNone) {
    host.Refuse("no model buffer: no parts queued");
    return false;
  }
  const std::optional<AssetAddress> shape =
      SectionAddress(state, file.size(), header->shape);
  if (!shape) {
    host.Refuse("ZUD SHP section past the file: no parts queued");
    return false;
  }

  PartQueue queue{.host = host, .state = state, .file_bytes = file.size()};
  ActorAssetRequest& request = queue.request;
  request.kind = Kind(ActorAssetKind::kActorModelParse);
  request.target_slot = kEnemyPreviewActorSlot;
  request.data = *shape;
  request.model_id = header->character_id;
  request.sub_kind = kEnemySubKind;
  request.variant = state.palette_flag & 1U;
  request.material = header->default_material;
  if (!Submit(queue)) return false;

  if (state.variant != 0) {
    if (!QueueCompanion(queue)) return false;
  } else {
    if (header->weapon != 0 &&
        !QueueEquipment(queue, header->weapon_model, header->weapon,
                        kEnemyPreviewWeaponSlot, kWeaponTextureSlot,
                        header->weapon_material)) {
      return false;
    }
    if (header->shield != 0 &&
        !QueueEquipment(queue, header->shield_model, header->shield,
                        kEnemyPreviewShieldSlot, kShieldTextureSlot,
                        header->shield_material)) {
      return false;
    }
  }
  return QueueSequence(queue, header->battle_sequence,
                       header->battle_sequence_model) &&
         QueueSequence(queue, header->common_sequence, 0);
}

// The queued parts' outcome: kFailed once any part has failed, been
// cancelled or lost its ticket, kLoaded once every part has loaded.
ActorAssetOutcome PartsOutcome(EnemyPreviewHost& host,
                               const EnemyPreviewState& state) {
  bool pending = false;
  for (const ActorAssetTicket ticket : state.tickets) {
    if (ticket == ActorAssetTicket::kNone) continue;
    const ActorAssetOutcome outcome = host.AssetOutcome(ticket);
    if (outcome == ActorAssetOutcome::kPending) {
      pending = true;
    } else if (outcome != ActorAssetOutcome::kLoaded) {
      return ActorAssetOutcome::kFailed;
    }
  }
  return pending ? ActorAssetOutcome::kPending : ActorAssetOutcome::kLoaded;
}

void ForgetTickets(EnemyPreviewHost& host, EnemyPreviewState& state) {
  for (ActorAssetTicket& ticket : state.tickets) {
    if (ticket != ActorAssetTicket::kNone) host.ForgetAsset(ticket);
    ticket = ActorAssetTicket::kNone;
  }
}

// Cancels and forgets the queued parts; false, keeping them, when the
// queue's head cannot be abandoned.
bool CancelParts(EnemyPreviewHost& host, EnemyPreviewState& state) {
  if (std::ranges::all_of(state.tickets, [](ActorAssetTicket ticket) {
        return ticket == ActorAssetTicket::kNone;
      })) {
    return true;
  }
  if (!host.CancelAssets()) {
    host.Refuse("the preview parts could not be cancelled");
    return false;
  }
  ForgetTickets(host, state);
  return true;
}

// Releases the records bound in the model buffer: the enemy (with the
// weapons it owns), its companion, weapon and shield.
bool ReleaseParts(EnemyPreviewHost& host, const EnemyPreviewState& state) {
  if (state.model_buffer == AssetAddress::kNone) return true;
  bool released = true;
  for (const EnemyPreviewPart part :
       {EnemyPreviewPart::kEnemy, EnemyPreviewPart::kCompanion,
        EnemyPreviewPart::kWeapon, EnemyPreviewPart::kShield}) {
    released = host.ReleasePart(state.model_buffer, part) && released;
  }
  if (!released) {
    host.Refuse("a preview record stays bound: the model buffer is kept");
  }
  return released;
}

void FreeFileBlock(EnemyPreviewHost& host, EnemyPreviewState& state) {
  if (state.file != AssetAddress::kNone) host.FreeFile(state.file);
  state.file = AssetAddress::kNone;
  state.file_bytes = 0;
}

// Native (#48): abandons a load whose read has settled. The parts are
// cancelled before the blocks they write are freed, and the records bound
// in the model buffer are released before it is.
int AbandonEnemyPreview(EnemyPreviewHost& host, EnemyPreviewState& state) {
  if (!CancelParts(host, state)) return kEnemyPreviewFailed;
  const bool released = ReleaseParts(host, state);
  FreeFileBlock(host, state);
  if (released) FreeEnemyModelBuffer(host, state);
  return kEnemyPreviewFailed;
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
      // Native (#48): a failed load whose parts could not yet be cancelled
      // (a disc write was still landing) keeps its blocks; the next load
      // retries the cancellation before reading.
      if (state.read == AssetAddress::kNone &&
          state.file != AssetAddress::kNone) {
        static_cast<void>(AbandonEnemyPreview(host, state));
      }
      // Native (#48): the original advanced past a read it never queued.
      if (!QueueEnemyModelRead(host, state, zud_id)) return kEnemyPreviewFailed;
      host.SetLoadStep(static_cast<std::uint8_t>(host.LoadStep() + 1));
      break;
    case Step(EnemyPreviewStep::kLoadParts): {
      const int loaded = LoadEnemyModelParts(host, state);
      if (loaded == kEnemyPreviewFailed) {
        host.SetLoadStep(Step(EnemyPreviewStep::kQueueRead));
        return kEnemyPreviewFailed;
      }
      if (loaded != 1) {
        host.SetLoadStep(static_cast<std::uint8_t>(host.LoadStep() + 1));
      }
      break;
    }
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
// src/MENU/MENU9.PRG/57BC.c:44-49, 0x80108098-0x801080C4)
void FreeEnemyPreview(EnemyPreviewHost& host, EnemyPreviewState& state) {
  // Native (#48): the original released actors 1 and 16 whatever they held
  // and freed the buffer under any part still queued. Queued parts are
  // cancelled first, only the records bound in this preview's buffer are
  // released, and the buffer is kept while one of them stays bound.
  if (state.read != AssetAddress::kNone) {
    host.Refuse("the preview's read is still in flight: nothing freed");
    return;
  }
  const bool loading = state.file != AssetAddress::kNone;
  if (!CancelParts(host, state) || !ReleaseParts(host, state)) return;
  FreeFileBlock(host, state);
  FreeEnemyModelBuffer(host, state);
  if (loading) host.SetLoadStep(Step(EnemyPreviewStep::kQueueRead));
}

// decomp: MENU9.PRG func_801080C8 (F02838,
// src/MENU/MENU9.PRG/57BC.c:53-63, 0x801080C8-0x80108128)
void InitializeAshleyPreviewPose(EnemyPreviewHost& host) {
  host.SetAnimation(kEnemyPreviewActorSlot, kAshleyPreviewAnimation);
  host.SetWeaponDrawn(kEnemyPreviewActorSlot, false);
  host.InitializeAnimationPose(kEnemyPreviewActorSlot);
  host.SetDisplayFlags(kEnemyPreviewActorSlot, kAshleyPreviewDisplayFlags);
  host.ClearCompanion(kEnemyPreviewActorSlot);
}

// decomp: MENU9.PRG vs_menu9_loadZudFile (F02839,
// src/MENU/MENU9.PRG/592C.c:172-202, 0x8010812C-0x80108238)
bool QueueEnemyModelRead(EnemyPreviewHost& host, EnemyPreviewState& state,
                         int zud_id) {
  // The original indexed its 256-entry tables unchecked.
  if (zud_id < 0 || zud_id >= kEnemyPreviewFiles) {
    host.Refuse("ZUD id past the file table: no read queued");
    return false;
  }
  // The original overwrote the handles, leaking the earlier blocks and
  // request; the encyclopedia frees and consumes them before a new read.
  if (state.read != AssetAddress::kNone || state.file != AssetAddress::kNone ||
      state.model_buffer != AssetAddress::kNone) {
    host.Refuse("an earlier preview still holds its read or blocks");
    return false;
  }
  const EnemyPreviewFile file = host.File(zud_id);
  if (file.bytes < 0) {
    host.Refuse("negative ZUD file size: no read queued");
    return false;
  }
  const auto file_bytes = static_cast<std::uint32_t>(file.bytes);
  const std::uint8_t variant = CompanionVariant(zud_id);
  AssetAddress read = host.AllocateRead(file);
  const AssetAddress data = host.AllocateFile(file_bytes);
  if (data == AssetAddress::kNone) {
    host.Refuse("no heap block for the ZUD file: no read queued");
    if (read != AssetAddress::kNone) host.ReleaseRead(read);
    return false;
  }
  // Native (#48): the original allocated the model buffer after starting the
  // read and kept the read when it failed. Both blocks are allocated (from
  // opposite heap ends, so their addresses are unchanged) before the read
  // starts, so a failure frees a block no read writes.
  const AssetAddress buffer =
      host.AllocateModelBuffer(variant != 0 ? EnemyPreviewLayout::kCompanion
                                            : EnemyPreviewLayout::kEquipped);
  if (buffer == AssetAddress::kNone) {
    host.Refuse("no heap block for the preview models: no read queued");
    if (read != AssetAddress::kNone) host.ReleaseRead(read);
    host.FreeFile(data);
    return false;
  }
  if (read != AssetAddress::kNone) {
    host.EnqueueRead(read, data);
  } else {
    read = host.ReadRefused(file, data);
    if (read == AssetAddress::kNone) {
      host.Refuse("the ZUD read could not be recorded: no read queued");
      host.FreeModelBuffer(buffer);
      host.FreeFile(data);
      return false;
    }
  }
  state.zud_id = zud_id;
  state.read = read;
  state.file = data;
  state.file_bytes = file_bytes;
  state.palette_flag = file.palette_flag;
  state.variant = variant;
  state.model_buffer = buffer;
  state.tickets = {};
  return true;
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:206-296, 0x8010823C-0x801084E0)
int LoadEnemyModelParts(EnemyPreviewHost& host, EnemyPreviewState& state) {
  if (state.read != AssetAddress::kNone) {
    const EnemyPreviewRead read = host.ReadStatus(state.read);
    if (read == EnemyPreviewRead::kPending) return 1;
    host.ReleaseRead(state.read);
    state.read = AssetAddress::kNone;
    // Native (#48): the original waited forever on a read that failed.
    if (read == EnemyPreviewRead::kFailed) {
      host.Refuse("the ZUD read failed");
      return AbandonEnemyPreview(host, state);
    }
    if (!QueueEnemyParts(host, state)) return AbandonEnemyPreview(host, state);
    return 1;
  }
  if (state.file == AssetAddress::kNone) return 0;
  host.UpdateAssetQueue();
  // Native (#48): the original freed the file block once the whole queue
  // drained, whoever's requests filled it, and never saw a failed part. The
  // preview's own tickets decide: the block is freed once every part has
  // loaded, and a failed part abandons the load.
  switch (PartsOutcome(host, state)) {
    case ActorAssetOutcome::kPending:
      return 1;
    case ActorAssetOutcome::kLoaded:
      ForgetTickets(host, state);
      FreeFileBlock(host, state);
      return 1;
    default:
      host.Refuse("a ZUD part failed to load");
      return AbandonEnemyPreview(host, state);
  }
}

// decomp: MENU9.PRG vs_menu9_freeZudFile (F02841,
// src/MENU/MENU9.PRG/592C.c:300-304, 0x801084E4-0x80108510)
void FreeEnemyModelBuffer(EnemyPreviewHost& host, EnemyPreviewState& state) {
  // The original freed a null buffer as well; there is nothing to release.
  if (state.model_buffer != AssetAddress::kNone) {
    host.FreeModelBuffer(state.model_buffer);
  }
  state.model_buffer = AssetAddress::kNone;
}

std::optional<EnemyPreviewPart> EnemyPreviewPartAt(
    Content::ActorRecordTable table, int slot, std::uint8_t variant) {
  if (table == Content::ActorRecordTable::kActor) {
    if (slot == kEnemyPreviewActorSlot) return EnemyPreviewPart::kEnemy;
    if (variant != 0 && slot == kEnemyPreviewCompanionSlot) {
      return EnemyPreviewPart::kCompanion;
    }
    return std::nullopt;
  }
  if (variant != 0) return std::nullopt;
  if (slot == kEnemyPreviewWeaponSlot) return EnemyPreviewPart::kWeapon;
  if (slot == kEnemyPreviewShieldSlot) return EnemyPreviewPart::kShield;
  return std::nullopt;
}

AssetAddress EnemyPreviewRecordMemory(const EnemyPreviewHost& host,
                                      const EnemyPreviewState& state,
                                      Content::ActorRecordTable table,
                                      int slot) {
  const std::optional<EnemyPreviewPart> part =
      EnemyPreviewPartAt(table, slot, state.variant);
  if (!part || state.model_buffer == AssetAddress::kNone) {
    return AssetAddress::kNone;
  }
  return host.ModelPart(state.model_buffer, *part);
}
}  // namespace VS::Menu::Score
