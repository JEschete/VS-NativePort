// Native side of the gazette camera and panel differentials
// (gazette_camera_common.py, #276). One vector per line, starting with the
// mode.
//
// RESTORE: the eight backup words, then the twelve camera words. Prints the
// position and angles words after RestoreCamera and 1 (lookAt kept).
// CAMERA: pitch, yaw, roll, distance and height offset. Prints the position
// and angles x, y and z, the nine rotation halfwords, the translation and 1
// (pads and lookAt kept).
// BANNER and PANEL: x, y, width, height and gradient; FOOTER: the top; FADE:
// nothing. Print 0 drawn or 1 refused, then per packet 1, its slot, its body
// length and the body words.
// OVERVIEW: step, score, the saved offset x and y and the camera salt. Prints
// the call count, then each recorded call.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string_view>
#include <vector>

#include "Core/Math/Matrix.hpp"
#include "Menu/Score/GazetteCamera.hpp"

namespace {
namespace score = VS::Menu::Score;
namespace math = VS::Core::Math;

enum Mode : int {
  kRestore,
  kCamera,
  kBanner,
  kPanel,
  kFooter,
  kOverview,
  kFade,
};
enum CallKind : std::int64_t {
  kCallFade = 1,
  kCallGet,
  kCallSet,
  kCallCamera,
  kCallPreview,
  kCallText,
  kCallPanel,
  kCallFooter,
};

using Call = std::vector<std::int64_t>;

template <typename Value>
bool Read(Value& value) {
  long long read = 0;
  if (!(std::cin >> read)) return false;
  value = static_cast<Value>(read);
  return true;
}

template <std::size_t Words>
void PrintPacket(std::int32_t slot,
                 const std::array<std::uint32_t, Words>& body) {
  std::printf(" 1 %d %zu", slot, Words);
  for (const std::uint32_t word : body) std::printf(" %u", word);
}

class Printer final : public score::PanelTarget {
 public:
  void DrawLine(std::int32_t slot, const score::PanelLine& line) override {
    PrintPacket(slot, score::PackPanelLine(line));
  }
  void DrawFlatQuad(std::int32_t slot,
                    const score::PanelFlatQuad& quad) override {
    PrintPacket(slot, score::PackPanelFlatQuad(quad));
  }
  void DrawShadedQuad(std::int32_t slot,
                      const score::PanelShadedQuad& quad) override {
    PrintPacket(slot, score::PackPanelShadedQuad(quad));
  }
  void SetTexturePage(std::int32_t slot, std::uint16_t page) override {
    PrintPacket(slot, score::PackPanelTexturePage(page));
  }
};

// Draws whose refusal is decided before their first primitive.
class Silent final : public score::PanelTarget {
 public:
  void DrawLine(std::int32_t, const score::PanelLine&) override {}
  void DrawFlatQuad(std::int32_t, const score::PanelFlatQuad&) override {}
  void DrawShadedQuad(std::int32_t, const score::PanelShadedQuad&) override {}
  void SetTexturePage(std::int32_t, std::uint16_t) override {}
};

template <typename Draw>
void PrintDraw(Draw draw) {
  Silent silent;
  const score::PanelDrawResult result = draw(silent);
  std::printf("%d", result == score::PanelDrawResult::kDrawn ? 0 : 1);
  if (result == score::PanelDrawResult::kDrawn) {
    Printer printer;
    draw(printer);
  }
}

// The camera stub's salted matrix (gazette_camera_common.matrix_marker).
math::Transform3 Marker(std::uint32_t salt) {
  math::Transform3 transform{};
  for (std::uint32_t index = 0; index < 9; ++index) {
    const std::uint32_t half =
        ((salt * (index + 3U)) ^ (index * 0x1111U)) & 0xFFFFU;
    transform.rotation.m[index / 3][index % 3] =
        static_cast<std::int16_t>(static_cast<std::uint16_t>(half));
  }
  transform.translation = {.x = static_cast<std::int32_t>((salt * 7U) + 0U),
                           .y = static_cast<std::int32_t>((salt * 8U) + 1U),
                           .z = static_cast<std::int32_t>((salt * 9U) + 2U)};
  return transform;
}

class Host final : public score::OverviewHost {
 public:
  Host(score::ProjectionOffset saved, std::uint32_t salt)
      : saved_(saved), salt_(salt) {}

  void DrawViewerEdgeFade() override { calls_.push_back({kCallFade}); }
  score::ProjectionOffset GetProjectionOffset() override {
    calls_.push_back({kCallGet});
    return saved_;
  }
  void SetProjectionOffset(const score::ProjectionOffset& offset) override {
    calls_.push_back({kCallSet, offset.x, offset.y});
  }
  math::Transform3 BuildViewerCamera() override {
    calls_.push_back({kCallCamera});
    return Marker(salt_);
  }
  void DrawActorPreview(const math::Transform3& camera) override {
    Call call{kCallPreview};
    for (const auto& row : camera.rotation.m) {
      for (const std::int16_t value : row) call.push_back(value);
    }
    call.push_back(camera.translation.x);
    call.push_back(camera.translation.y);
    call.push_back(camera.translation.z);
    calls_.push_back(call);
  }
  void DrawText(std::string_view text, std::uint32_t position,
                std::uint32_t color) override {
    Call call{kCallText, static_cast<std::int64_t>(text.size())};
    for (const char letter : text) {
      call.push_back(static_cast<unsigned char>(letter));
    }
    call.push_back(position);
    call.push_back(color);
    call.push_back(score::kPanelSlot);
    calls_.push_back(call);
  }
  void DrawGradientPanel(const score::PanelRect& rect,
                         std::int32_t gradient) override {
    calls_.push_back(
        {kCallPanel, rect.x, rect.y, rect.width, rect.height, gradient});
  }
  void DrawFooterBackground(std::int32_t top) override {
    calls_.push_back({kCallFooter, top});
  }

  void Print() const {
    std::printf("%zu", calls_.size());
    for (const Call& call : calls_) {
      for (const std::int64_t value : call) {
        std::printf(" %lld", static_cast<long long>(value));
      }
    }
  }

 private:
  score::ProjectionOffset saved_;
  std::uint32_t salt_;
  std::vector<Call> calls_;
};

bool ReadVector(score::CameraVector& vector) {
  for (std::int32_t& word : vector) {
    std::uint32_t raw = 0;
    if (!Read(raw)) return false;
    word = static_cast<std::int32_t>(raw);
  }
  return true;
}

void PrintVector(const score::CameraVector& vector) {
  for (const std::int32_t word : vector) {
    std::printf(" %u", static_cast<std::uint32_t>(word));
  }
}

bool Restore() {
  score::CameraPose backup;
  score::CameraPose camera;
  score::CameraVector look_at{};
  if (!ReadVector(backup.position) || !ReadVector(backup.angles) ||
      !ReadVector(camera.position) || !ReadVector(look_at) ||
      !ReadVector(camera.angles)) {
    return false;
  }
  score::RestoreCamera(backup, camera);
  std::printf("%u", static_cast<std::uint32_t>(camera.position[0]));
  for (std::size_t word = 1; word < score::kCameraVectorWords; ++word) {
    std::printf(" %u", static_cast<std::uint32_t>(camera.position[word]));
  }
  PrintVector(camera.angles);
  // lookAt kept, no calls.
  std::printf(" 1 0");
  return true;
}

bool Camera() {
  score::ViewerCameraSettings settings;
  if (!Read(settings.pitch) || !Read(settings.yaw) || !Read(settings.roll) ||
      !Read(settings.distance) || !Read(settings.height_offset)) {
    return false;
  }
  const score::ViewerCamera camera = score::BuildViewerCamera(settings);
  std::printf("%d %d %d %d %d %d", camera.position.x, camera.position.y,
              camera.position.z, camera.angles.x, camera.angles.y,
              camera.angles.z);
  for (const auto& row : camera.view.rotation.m) {
    for (const std::int16_t value : row) std::printf(" %d", value);
  }
  // Pads and lookAt kept, no calls.
  std::printf(" %d %d %d 1 0", camera.view.translation.x,
              camera.view.translation.y, camera.view.translation.z);
  return true;
}

bool ReadRect(score::PanelRect& rect, std::int32_t& gradient) {
  return Read(rect.x) && Read(rect.y) && Read(rect.width) &&
         Read(rect.height) && Read(gradient);
}
}  // namespace

int main() {
  int mode = 0;
  while (std::cin >> mode) {
    score::PanelRect rect;
    std::int32_t gradient = 0;
    if (mode == kRestore) {
      if (!Restore()) return 2;
    } else if (mode == kCamera) {
      if (!Camera()) return 2;
    } else if (mode == kBanner || mode == kPanel) {
      if (!ReadRect(rect, gradient)) return 2;
      PrintDraw([&](score::PanelTarget& target) {
        return mode == kBanner
                   ? score::DrawEnemyBanner(rect, gradient, target)
                   : score::DrawGradientPanel(rect, gradient, target);
      });
    } else if (mode == kFooter) {
      std::int32_t top = 0;
      if (!Read(top)) return 2;
      PrintDraw([&](score::PanelTarget& target) {
        score::DrawFooterBackground(top, target);
        return score::PanelDrawResult::kDrawn;
      });
    } else if (mode == kFade) {
      PrintDraw([&](score::PanelTarget& target) {
        score::DrawViewerEdgeFade(target);
        return score::PanelDrawResult::kDrawn;
      });
    } else if (mode == kOverview) {
      std::int32_t step = 0;
      std::uint32_t value = 0;
      score::ProjectionOffset saved;
      std::uint32_t salt = 0;
      if (!Read(step) || !Read(value) || !Read(saved.x) || !Read(saved.y) ||
          !Read(salt)) {
        return 2;
      }
      Host host(saved, salt);
      score::RenderOverview(host, step, value);
      host.Print();
    } else {
      return 2;
    }
    std::printf("\n");
  }
  return 0;
}
