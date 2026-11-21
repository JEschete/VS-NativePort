// Hand-maintained live binding for Content/DiscReader.cpp.
#include "Content/DiscReaderLive.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "Audio/XaPlayback.hpp"
#include "Content/AssetCatalog.hpp"
#include "Content/AssetHandle.hpp"
#include "Content/DataAccess.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscQueue.hpp"
#include "Content/DiscReader.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Content {
namespace {

// The XA markers read 128 while no XA stream owns the drive.
constexpr std::uint8_t kInactiveXaMarker = 128;
std::uint64_t readGeneration = 0;
bool readCompleted = false;
// The asset the armed read names; none for a sector read. The drive record
// keeps the original's fields, so the handle lives beside it.
AssetHandle armedAsset{};

// Reads the armed asset's first `bytes` into `destination`.
bool ReadArmedAsset(std::uint32_t bytes, void* destination) {
  const DataStore* store = DataStore::Get();
  if (store == nullptr) {
    VS::Core::LogError("content", "asset read refused: no data is open");
    return false;
  }
  const auto read = store->ReadAsset(
      armedAsset, std::span(static_cast<std::uint8_t*>(destination), bytes));
  if (!read) {
    VS::Core::LogError("content", "{}", read.error());
    return false;
  }
  const AssetRecord* record = store->Catalog() != nullptr
                                  ? store->Catalog()->Record(armedAsset)
                                  : nullptr;
  VS::Core::LogDebug(
      "content", "asset read: {} ({} bytes)",
      record != nullptr ? std::string_view(record->id) : std::string_view("?"),
      bytes);
  return true;
}

DiscDrivePhase PhaseOf(DiscDriveState state) {
  switch (state) {
    case DiscDriveState::kIdle:
      return DiscDrivePhase::kIdle;
    case DiscDriveState::kSeekReady:
      return DiscDrivePhase::kSeekReady;
    case DiscDriveState::kReadReady:
      return DiscDrivePhase::kReadReady;
    case DiscDriveState::kReadError:
      return DiscDrivePhase::kReadError;
    case DiscDriveState::kReadInit:
      return DiscDrivePhase::kReadInit;
    case DiscDriveState::kReadXaInit:
      return DiscDrivePhase::kReadXaInit;
    case DiscDriveState::kStreamXa:
      return DiscDrivePhase::kStreamXa;
    case DiscDriveState::kCommandFailed:
      return DiscDrivePhase::kCommandFailed;
    case DiscDriveState::kStreamXaEnd:
      return DiscDrivePhase::kStreamXaEnd;
    default:
      return DiscDrivePhase::kOther;
  }
}

DiscDriveState StateOf(DiscDrivePhase phase) {
  switch (phase) {
    case DiscDrivePhase::kIdle:
      return DiscDriveState::kIdle;
    case DiscDrivePhase::kSeekReady:
      return DiscDriveState::kSeekReady;
    case DiscDrivePhase::kReadReady:
      return DiscDriveState::kReadReady;
    case DiscDrivePhase::kReadError:
      return DiscDriveState::kReadError;
    case DiscDrivePhase::kReadInit:
      return DiscDriveState::kReadInit;
    case DiscDrivePhase::kReadXaInit:
      return DiscDriveState::kReadXaInit;
    case DiscDrivePhase::kStreamXa:
      return DiscDriveState::kStreamXa;
    case DiscDrivePhase::kCommandFailed:
      return DiscDriveState::kCommandFailed;
    case DiscDrivePhase::kStreamXaEnd:
      return DiscDriveState::kStreamXaEnd;
    case DiscDrivePhase::kOther:
      break;
  }
  return discDriveState.state;
}

class LiveDiscDriveHost final : public DiscDriveHost {
 public:
  explicit LiveDiscDriveHost(void* destination = nullptr,
                             AssetHandle asset = {})
      : destination_(destination), asset_(asset) {}

  [[nodiscard]] DiscDrivePhase Phase() const override {
    return PhaseOf(discDriveState.state);
  }
  void SetPhase(DiscDrivePhase phase) override {
    discDriveState.state = StateOf(phase);
  }

  void ArmRead(const DiscReadPlan& plan) override {
    DiscDrive& drive = discDriveState;
    drive.start_sector = plan.start_sector;
    drive.byte_count = plan.byte_count;
    drive.destination = destination_;
    drive.ring_read_index = 0;
    drive.ring_write_index = 0;
    drive.destination_sector = 0;
    drive.buffered_sectors = 0;
    drive.sectors_received = 0;
    drive.failure_count = 0;
    drive.ring_sectors_written = 0;
    drive.sector_count = plan.sector_count;
    armedAsset = asset_;
  }
  [[nodiscard]] DiscReadPlan PendingRead() const override {
    const DiscDrive& drive = discDriveState;
    return DiscReadPlan{
        .start_sector = drive.start_sector,
        .byte_count = drive.byte_count,
        .sector_count = drive.sector_count,
    };
  }
  [[nodiscard]] bool ReadPendingData() override {
    readCompleted = false;
    const DiscDrive& drive = discDriveState;
    if (armedAsset.Valid()) {
      return ReadArmedAsset(static_cast<std::uint32_t>(drive.byte_count),
                            drive.destination);
    }
    return ReadDiscBytes(static_cast<unsigned>(drive.start_sector),
                         static_cast<unsigned>(drive.byte_count),
                         drive.destination) != 0;
  }
  void MarkSectorsDelivered(std::uint32_t sectors) override {
    readCompleted = true;
    discDriveState.sectors_received = sectors;
    discDriveState.destination_sector = sectors;
  }
  void CountFailure() override { ++discDriveState.failure_count; }

  [[nodiscard]] DiscXaRequest PendingXaRequest() const override {
    return DiscXaRequest{
        .start_sector = discDriveState.start_sector,
        .volume = discDriveState.byte_count,
    };
  }
  [[nodiscard]] bool StartXaStream(const DiscXaRequest& request) override {
    return VS::Audio::StartXaStream(request.start_sector, request.volume) != 0;
  }
  [[nodiscard]] XaStreamStatus PollXaStream() override {
    const int status = VS::Audio::XaStreamStatus();
    if (status < 0) {
      return XaStreamStatus::kFailed;
    }
    return status == 0 ? XaStreamStatus::kFinished : XaStreamStatus::kPlaying;
  }
  void StopXaStream() override { VS::Audio::StopXaStream(); }
  void MarkXaStreamStarted() override {
    discDriveState.xa_mode_marker = 0;
    discDriveState.position.minute = 0;
  }
  void ClearXaMarkers() override {
    discDriveState.xa_mode_marker = kInactiveXaMarker;
    discDriveState.position.minute = kInactiveXaMarker;
    discDriveState.reserved2 = 0;
  }

  void UseDirectBuffer() override {
    discDriveState.buffer_mode = DiscBufferMode::kDirect;
    legacyDiscReadBuffer = nullptr;
  }

 private:
  void* destination_;
  AssetHandle asset_;
};

bool BeginRead(std::int32_t sector, std::uint32_t bytes, void* destination,
               AssetHandle asset) {
  if ((bytes != 0 && destination == nullptr) ||
      readGeneration == std::numeric_limits<std::uint64_t>::max()) {
    VS::Core::Warnf(
        "content",
        "Disc read refused: missing destination or exhausted generation");
    return false;
  }
  LiveDiscDriveHost host(destination, asset);
  if (!BeginDiscRead(host, sector, bytes)) return false;
  ++readGeneration;
  readCompleted = false;
  return true;
}

}  // namespace

bool BeginLiveDiscRead(std::int32_t sector, std::uint32_t bytes,
                       void* destination) {
  return BeginRead(sector, bytes, destination, {});
}

bool BeginLiveAssetRead(AssetHandle asset, std::uint32_t bytes,
                        void* destination) {
  if (!asset.Valid()) {
    VS::Core::Warnf("content", "Asset read refused: no asset");
    return false;
  }
  return BeginRead(0, bytes, destination, asset);
}

std::uint64_t LiveDiscReadGeneration() noexcept { return readGeneration; }

bool LiveDiscReadCompleted(std::uint64_t generation) noexcept {
  return generation != 0 && generation == readGeneration && readCompleted;
}

void CancelLiveDiscRead(const void* destination) {
  if (destination == nullptr) {
    VS::Core::Warnf("content", "CancelDiscRead refused: null destination");
    return;
  }
  if (discDriveState.destination != destination) return;
  readCompleted = false;
  armedAsset = {};
  discDriveState.destination = nullptr;
  const auto phase = PhaseOf(discDriveState.state);
  if (phase == DiscDrivePhase::kReadXaInit ||
      phase == DiscDrivePhase::kStreamXa ||
      phase == DiscDrivePhase::kStreamXaEnd) {
    return;
  }
  discDriveState.state = DiscDriveState::kIdle;
  discDriveState.byte_count = 0;
  discDriveState.sector_count = 0;
}

void UpdateLiveDiscCommands() {
  LiveDiscDriveHost host;
  UpdateDiscCommands(host);
}

void ResetLiveDiscDrive() {
  readCompleted = false;
  armedAsset = {};
  discDriveState.destination = nullptr;
  LiveDiscDriveHost host;
  ResetDiscDrive(host);
}

}  // namespace VS::Content
