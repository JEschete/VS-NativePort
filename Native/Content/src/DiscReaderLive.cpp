// Hand-maintained live binding for Content/DiscReader.cpp.
#include "Content/DiscReaderLive.hpp"

#include <cstdint>
#include <limits>

#include "Content/DiscReader.hpp"
#include "Core/Diagnostics.h"
extern "C" {
#include "Audio/XaPlayback.h"
#include "Content/DataAccess.h"
#include "DiscQueue.h"
#include "Legacy/Psx.h"
}

namespace VS::Content {
namespace {

// The XA markers read 128 while no XA stream owns the drive.
constexpr int kInactiveXaMarker = 128;
std::uint64_t read_generation = 0;
bool read_completed = false;

DiscDrivePhase PhaseOf(int state) {
  switch (state) {
    case Content_DiscDriveState_Idle:
      return DiscDrivePhase::kIdle;
    case Content_DiscDriveState_SeekReady:
      return DiscDrivePhase::kSeekReady;
    case Content_DiscDriveState_ReadReady:
      return DiscDrivePhase::kReadReady;
    case Content_DiscDriveState_ReadError:
      return DiscDrivePhase::kReadError;
    case Content_DiscDriveState_ReadInit:
      return DiscDrivePhase::kReadInit;
    case Content_DiscDriveState_ReadXaInit:
      return DiscDrivePhase::kReadXaInit;
    case Content_DiscDriveState_StreamXa:
      return DiscDrivePhase::kStreamXa;
    case Content_DiscDriveState_CommandFailed:
      return DiscDrivePhase::kCommandFailed;
    case Content_DiscDriveState_StreamXaEnd:
      return DiscDrivePhase::kStreamXaEnd;
    default:
      return DiscDrivePhase::kOther;
  }
}

char StateOf(DiscDrivePhase phase) {
  switch (phase) {
    case DiscDrivePhase::kIdle:
      return Content_DiscDriveState_Idle;
    case DiscDrivePhase::kSeekReady:
      return Content_DiscDriveState_SeekReady;
    case DiscDrivePhase::kReadReady:
      return Content_DiscDriveState_ReadReady;
    case DiscDrivePhase::kReadError:
      return Content_DiscDriveState_ReadError;
    case DiscDrivePhase::kReadInit:
      return Content_DiscDriveState_ReadInit;
    case DiscDrivePhase::kReadXaInit:
      return Content_DiscDriveState_ReadXaInit;
    case DiscDrivePhase::kStreamXa:
      return Content_DiscDriveState_StreamXa;
    case DiscDrivePhase::kCommandFailed:
      return Content_DiscDriveState_CommandFailed;
    case DiscDrivePhase::kStreamXaEnd:
      return Content_DiscDriveState_StreamXaEnd;
    case DiscDrivePhase::kOther:
      break;
  }
  return Content_discDriveState.state;
}

class LiveDiscDriveHost final : public DiscDriveHost {
 public:
  explicit LiveDiscDriveHost(void* destination = nullptr)
      : destination_(destination) {}

  [[nodiscard]] DiscDrivePhase Phase() const override {
    return PhaseOf(Content_discDriveState.state);
  }
  void SetPhase(DiscDrivePhase phase) override {
    Content_discDriveState.state = StateOf(phase);
  }

  void ArmRead(const DiscReadPlan& plan) override {
    Content_DiscDrive& drive = Content_discDriveState;
    drive.startSector = plan.start_sector;
    drive.byteCount = plan.byte_count;
    drive.destination = destination_;
    drive.ringReadIndex = 0;
    drive.ringWriteIndex = 0;
    drive.destinationSector = 0;
    drive.bufferedSectors = 0;
    drive.sectorsReceived = 0;
    drive.failureCount = 0;
    drive.ringSectorsWritten = 0;
    drive.sectorCount = plan.sector_count;
  }
  [[nodiscard]] DiscReadPlan PendingRead() const override {
    const Content_DiscDrive& drive = Content_discDriveState;
    return DiscReadPlan{
        .start_sector = drive.startSector,
        .byte_count = drive.byteCount,
        .sector_count = drive.sectorCount,
    };
  }
  [[nodiscard]] bool ReadPendingData() override {
    read_completed = false;
    const Content_DiscDrive& drive = Content_discDriveState;
    return Content_ReadDiscBytes(static_cast<unsigned>(drive.startSector),
                                 static_cast<unsigned>(drive.byteCount),
                                 drive.destination) != 0;
  }
  void MarkSectorsDelivered(std::uint32_t sectors) override {
    read_completed = true;
    Content_discDriveState.sectorsReceived = sectors;
    Content_discDriveState.destinationSector = sectors;
  }
  void CountFailure() override { ++Content_discDriveState.failureCount; }

  [[nodiscard]] DiscXaRequest PendingXaRequest() const override {
    return DiscXaRequest{
        .start_sector = Content_discDriveState.startSector,
        .volume = Content_discDriveState.byteCount,
    };
  }
  [[nodiscard]] bool StartXaStream(const DiscXaRequest& request) override {
    return Audio_Native_StartXaStream(request.start_sector, request.volume) !=
           0;
  }
  [[nodiscard]] XaStreamStatus PollXaStream() override {
    const int status = Audio_Native_XaStreamStatus();
    if (status < 0) {
      return XaStreamStatus::kFailed;
    }
    return status == 0 ? XaStreamStatus::kFinished : XaStreamStatus::kPlaying;
  }
  void StopXaStream() override { Audio_Native_StopXaStream(); }
  void MarkXaStreamStarted() override {
    Content_discDriveState.xaModeMarker = 0;
    Content_discDriveState.position.minute = 0;
  }
  void ClearXaMarkers() override {
    Content_discDriveState.xaModeMarker = static_cast<char>(kInactiveXaMarker);
    Content_discDriveState.position.minute =
        static_cast<u_char>(kInactiveXaMarker);
    Content_discDriveState.reserved2 = 0;
  }

  void UseDirectBuffer() override {
    Content_discDriveState.bufferMode = Content_DiscBufferMode_Direct;
    Content_legacyDiscReadBuffer = nullptr;
  }
  void WaitFrames(std::uint32_t frames) override {
    VSync(static_cast<int>(frames));
  }

 private:
  void* destination_;
};

}  // namespace

bool BeginLiveDiscRead(std::int32_t sector, std::uint32_t bytes,
                       void* destination) {
  if ((bytes != 0 && destination == nullptr) ||
      read_generation == std::numeric_limits<std::uint64_t>::max()) {
    Core_Warnf(
        "content",
        "Disc read refused: missing destination or exhausted generation");
    return false;
  }
  LiveDiscDriveHost host(destination);
  if (!BeginDiscRead(host, sector, bytes)) return false;
  ++read_generation;
  read_completed = false;
  return true;
}

std::uint64_t LiveDiscReadGeneration() noexcept { return read_generation; }

bool LiveDiscReadCompleted(std::uint64_t generation) noexcept {
  return generation != 0 && generation == read_generation && read_completed;
}

void CancelLiveDiscRead(const void* destination) {
  if (destination == nullptr) {
    Core_Warnf("content", "CancelDiscRead refused: null destination");
    return;
  }
  if (Content_discDriveState.destination != destination) return;
  read_completed = false;
  Content_discDriveState.destination = nullptr;
  const auto phase = PhaseOf(Content_discDriveState.state);
  if (phase == DiscDrivePhase::kReadXaInit ||
      phase == DiscDrivePhase::kStreamXa ||
      phase == DiscDrivePhase::kStreamXaEnd) {
    return;
  }
  Content_discDriveState.state = Content_DiscDriveState_Idle;
  Content_discDriveState.byteCount = 0;
  Content_discDriveState.sectorCount = 0;
}

void UpdateLiveDiscCommands() {
  LiveDiscDriveHost host;
  UpdateDiscCommands(host);
}

void ResetLiveDiscDrive() {
  read_completed = false;
  Content_discDriveState.destination = nullptr;
  LiveDiscDriveHost host;
  ResetDiscDrive(host);
}

}  // namespace VS::Content
