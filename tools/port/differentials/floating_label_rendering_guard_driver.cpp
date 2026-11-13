#include <crtdbg.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <span>
#include <string>
#include <string_view>

#include "Battle/FloatingLabelRenderingLive.hpp"
#include "Core/Log.hpp"
#include "Render/UiTexturedQuads.hpp"
#include "TestSupport/FloatingLabelRenderingLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::floating_label_rendering;
constexpr int kX = 100;
constexpr int kY = 100;
constexpr std::size_t kColorWord = 1;
constexpr std::size_t kTwoQuads = VS::Render::kUiTexturedQuadWords * 2;
constexpr std::uint8_t kInvalidByte = 255;
constexpr std::uint8_t kInvalidTimingFrame = 46;
constexpr std::uint8_t kInvalidChainFrame = 91;
constexpr std::uint16_t kInvalidResult = 65535;

void Observe(const VS::Core::LogRecord& record, const f::LiveFrame& frame,
             const f::LabelWords& before, const char* path) {
  const auto base = reinterpret_cast<std::uintptr_t>(frame.primitives.data());
  const auto cursor =
      reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
  const auto limit = base + frame.primitives.size() * sizeof(u_long);
  const auto used =
      cursor >= base && cursor <= limit
          ? static_cast<std::int64_t>((cursor - base) / sizeof(u_long))
          : -1;
  const auto quads = frame.Traversal(f::kQuadBucket);
  const auto text = frame.Traversal(f::kTextBucket);
  std::ofstream output(path, std::ios::trunc);
  output << "{\"error\":"
         << (record.level == VS::Core::LogLevel::Error ? "true" : "false")
         << ",\"message\":" << std::quoted(std::string(record.message))
         << ",\"used_words\":" << used << ",\"quad_packets\":" << quads.size()
         << ",\"text_packets\":" << text.size() << ",\"payload_unchanged\":"
         << (f::Words(frame.labels.front()) == before ? "true" : "false")
         << ",\"quad_command\":"
         << (quads.empty() ? 0 : quads.front().at(kColorWord)) << "}\n";
}

bool Invoke(std::string_view mode, f::LiveFrame& frame) {
  auto* label = &frame.labels.front();
  if (mode == "caption-null") {
    b::DrawLiveValueLabelCaption(kX, kY, nullptr);
  } else if (mode == "caption-foreign") {
    Battle_FloatingLabel foreign{};
    b::DrawLiveValueLabelCaption(kX, kY, &foreign);
  } else if (mode == "caption-rebound") {
    Battle_FloatingLabel rebound[1][f::kLabelCapacity]{};
    Battle_floatingLabels = rebound;
    b::DrawLiveValueLabelCaption(kX, kY, label);
  } else if (mode == "caption-mode") {
    b::DrawLiveValueLabelCaption(kX, kY, label);
  } else if (mode == "caption-one-quad") {
    Render_frameScratch.primitiveCursor = frame.primitives.data() +
                                          frame.primitives.size() -
                                          VS::Render::kUiTexturedQuadWords;
    b::DrawLiveValueLabelCaption(kX, kY, label);
  } else if (mode == "locked-two-quads") {
    Render_frameScratch.primitiveCursor =
        frame.primitives.data() + frame.primitives.size() - kTwoQuads;
    b::DrawLiveLockedFloatingLabel(kX, kY, label);
  } else if (mode == "locked-frame") {
    b::DrawLiveLockedFloatingLabel(kX, kY, label);
  } else if (mode == "timing-frame") {
    b::DrawLiveAbilityTimingResult(kX, kY, label);
  } else if (mode == "timing-result") {
    b::DrawLiveAbilityTimingResult(kX, kY, label);
  } else if (mode == "timing-phrase") {
    b::DrawLiveAbilityTimingResult(kX, kY, label);
  } else if (mode == "chain-frame") {
    b::DrawLiveChainCount(kX, kY, label);
  } else if (mode == "text-one-glyph") {
    constexpr std::size_t kCompactGlyphWords = 5;
    Render_frameScratch.primitiveCursor =
        frame.primitives.data() + frame.primitives.size() - kCompactGlyphWords;
    b::DrawLiveChainCount(kX, kY, label);
  } else if (mode == "marker-no-ot") {
    Render_frameScratch.orderingTable = nullptr;
    b::DrawLiveStatusRemovalMarker(kX, kY, 0);
  } else if (mode == "marker-ot-before-owner") {
    Render_frameScratch.orderingTable = frame.ordering.data();
    b::DrawLiveStatusRemovalMarker(kX, kY, 0);
  } else if (mode == "marker-misaligned-ot") {
    Render_frameScratch.orderingTable =
        reinterpret_cast<char*>(frame.ordering.data()) + 1;
    b::DrawLiveStatusRemovalMarker(kX, kY, 0);
  } else if (mode == "marker-no-primitives") {
    Render_legacyPrimitiveBuffers[0] = nullptr;
    b::DrawLiveStatusRemovalMarker(kX, kY, 0);
  } else if (mode == "marker-misaligned-primitives") {
    Render_frameScratch.primitiveCursor =
        reinterpret_cast<char*>(frame.primitives.data()) + 1;
    b::DrawLiveStatusRemovalMarker(kX, kY, 0);
  } else if (mode == "marker-wrong-frame") {
    Render_frameBuf = 1;
    Render_legacyPrimitiveBuffers[1] = nullptr;
    Render_legacyOrderingTables[1] = nullptr;
    b::DrawLiveStatusRemovalMarker(kX, kY, 0);
  } else if (mode == "marker-invalid-frame") {
    Render_frameBuf = -1;
    b::DrawLiveStatusRemovalMarker(kX, kY, 0);
  } else if (mode == "marker-no-label-owner") {
    Battle_floatingLabels = nullptr;
    b::DrawLiveStatusRemovalMarker(kX, kY, 0);
    return true;
  } else if (mode == "cue-no-font-or-label-owner") {
    Battle_floatingLabels = nullptr;
    std::ranges::fill(Render_compactGlyphWidths, 0);
    std::ranges::fill(Render_compactGlyphIndices, 0);
    b::DrawLiveAbilityTimingCue();
    return true;
  } else if (mode == "locked-cull-no-frame") {
    constexpr int kOutside = 336;
    Render_frameScratch.orderingTable = nullptr;
    Render_frameScratch.primitiveCursor = nullptr;
    b::DrawLiveLockedFloatingLabel(kOutside, kY, label);
    return true;
  } else if (mode == "locked-cull-invalid-frame") {
    constexpr int kOutside = 336;
    Render_frameBuf = -1;
    b::DrawLiveLockedFloatingLabel(kOutside, kY, label);
    return true;
  }
  return false;
}

void PreparePayload(std::string_view mode, Battle_FloatingLabel& label) {
  if (mode == "caption-mode") label.colorFlags = kInvalidByte;
  if (mode == "locked-frame") label.frame = kInvalidByte;
  if (mode == "timing-frame") label.frame = kInvalidTimingFrame;
  if (mode == "timing-result") label.value = kInvalidResult;
  if (mode == "timing-phrase") {
    label.value = 2;
    label.textLength = kInvalidByte;
  }
  if (mode == "chain-frame") label.frame = kInvalidChainFrame;
  if (mode == "text-one-glyph") label.value = 1;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) return 2;
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  if (!VS::Core::SetLogFile(argv[2])) return 3;
  const f::ProductionTables tables;
  f::LiveFrame frame;
  constexpr std::uint8_t kCaptionMode = 3;
  frame.labels.front().colorFlags = kCaptionMode;
  PreparePayload(argv[1], frame.labels.front());
  const auto before = f::Words(frame.labels.front());
  VS::Core::SetLogObserver([&](const VS::Core::LogRecord& record) {
    Observe(record, frame, before, argv[3]);
  });
  const bool positive = Invoke(argv[1], frame);
  VS::Core::LogWrite(VS::Core::LogLevel::Info, "battle.labels.guard",
                     "Guard operation completed");
  VS::Core::SetLogObserver({});
  return positive ? 0 : 97;
}
