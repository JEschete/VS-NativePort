// Native side of tools\port\differentials\floating_placement_*.py (F00719,
// F00720, F00722, F00725, F00738, F00739 and F00742): one vector per stdin
// line, one line of integers per vector, in the order
// floating_placement_common.py prints the original's observation.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Battle/FloatingLabelPlacement.hpp"
#include "Battle/FloatingLabelQueue.hpp"
#include "Battle/FloatingLabelRendering.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiTexturedQuads.hpp"
#include "TestSupport/FloatingLabelQueueFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace r = VS::Render;
namespace f = VS::test_support::floating_labels;
namespace math = VS::Core::Math;

enum Mode : int { kUpdate, kPlace, kValue, kStatus, kQueue, kEquipDraw, kCommit };
enum Call : int { kCueCall = 1, kBoneCall, kProjectCall, kLeafCall, kCompactCall };

constexpr std::uint32_t kModuleBase = 0x80068800;
constexpr std::uint32_t kEnd = 0xFFFFFF;
constexpr std::uint32_t kHeadLengthMask = 0xFF000000;
constexpr std::uint32_t kSeedStride = 0x9E3779B9;
constexpr std::size_t kRecordWords = 6;
constexpr std::size_t kCapacity = b::kFloatingLabelCapacity;
constexpr std::size_t kActors = 4;
constexpr std::size_t kWordBytes = 4;
constexpr int kByteBits = 8;
constexpr std::int32_t kProjectX = 160;
constexpr std::int32_t kProjectY = 120;
constexpr std::int32_t kVisibleBias = 50;
// Printed in place of a value when the typed core refuses.
constexpr long long kRefused = 0x7FFFFFFF;

constexpr std::uint32_t kCaptionMap = 0x800E861C;
constexpr std::uint32_t kCaptionAtlas = 0x80068FAC;
constexpr std::uint32_t kHalfHeights = 0x800E8634;
constexpr std::uint32_t kLockedPieces = 0x800E8640;
constexpr std::uint32_t kStatusLayouts = 0x800690B4;
constexpr std::uint32_t kStatusColors = 0x80069134;
constexpr std::uint32_t kFirstWords = 0x80069144;
constexpr std::uint32_t kSecondWords = 0x8006917C;
constexpr std::size_t kRectBytes = 8;
constexpr std::uint32_t kFirstCaption = 3;
constexpr std::size_t kCaptionKinds = 6;
constexpr std::size_t kCaptionRects = 33;
constexpr std::size_t kHeightSteps = 5;
constexpr std::size_t kPieceCount = 4;
constexpr std::size_t kLayoutCount = 32;
constexpr std::size_t kColorCount = 4;
constexpr std::size_t kFirstWordCount = 7;
constexpr std::size_t kSecondWordCount = 13;

using Words = std::array<std::uint32_t, kRecordWords>;
using Output = std::vector<long long>;

long long Next(std::istringstream& in) {
  long long value = 0;
  if (!(in >> value)) std::abort();
  return value;
}

b::FloatingLabelRecord ReadRecord(std::istringstream& in) {
  Words words{};
  for (auto& word : words) word = static_cast<std::uint32_t>(Next(in));
  return std::bit_cast<b::FloatingLabelRecord>(words);
}

void PrintRecord(Output& out, const b::FloatingLabelRecord& record) {
  const auto words = std::bit_cast<Words>(record);
  out.insert(out.end(), words.begin(), words.end());
}

std::uint32_t PointWord(b::LabelScreenPoint point) {
  return std::bit_cast<std::uint32_t>(point);
}

class Image {
 public:
  explicit Image(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) std::exit(2);
    bytes_.assign(std::istreambuf_iterator<char>(stream),
                  std::istreambuf_iterator<char>());
  }
  [[nodiscard]] std::span<const std::uint8_t> Bytes(std::uint32_t address,
                                                    std::size_t size) const {
    const auto offset = static_cast<std::size_t>(address - kModuleBase);
    if (address < kModuleBase || offset > bytes_.size() ||
        size > bytes_.size() - offset) {
      std::exit(3);
    }
    return std::span(bytes_).subspan(offset, size);
  }
  [[nodiscard]] std::uint8_t Byte(std::uint32_t address) const {
    return Bytes(address, 1).front();
  }
  [[nodiscard]] std::uint32_t Word(std::uint32_t address) const {
    std::uint32_t value = 0;
    const auto bytes = Bytes(address, kWordBytes);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
      value |= static_cast<std::uint32_t>(bytes[index]) << (index * kByteBits);
    }
    return value;
  }
  [[nodiscard]] std::int16_t Half(std::uint32_t address) const {
    const auto bytes = Bytes(address, 2);
    return static_cast<std::int16_t>(
        bytes.front() |
        (static_cast<std::uint32_t>(bytes.back()) << kByteBits));
  }
  [[nodiscard]] b::FloatingLabelAtlasRect Rect(std::uint32_t address) const {
    return {Half(address), Half(address + 2), Half(address + 4),
            Half(address + 6)};
  }

 private:
  std::vector<std::uint8_t> bytes_;
};

class Queue final : public r::PacketQueue {
 public:
  Queue() {
    constexpr std::array<std::uint32_t, 4> kPrefix{0x03FFFFFF, 0x60ABCDEF,
                                                   0x000B000A, 0x00020001};
    packets_.emplace_back(kPrefix.begin(), kPrefix.end());
  }
  [[nodiscard]] std::uint32_t ChainLink() const override { return head_; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    packets_.emplace_back(packet.begin(), packet.end());
    head_ = static_cast<std::uint32_t>(packets_.size());
    return true;
  }
  [[nodiscard]] std::vector<std::uint32_t> Traversal() const {
    std::vector<std::uint32_t> words;
    auto handle = head_;
    for (std::size_t steps = 0; handle != kEnd; ++steps) {
      if (handle == 0 || handle > packets_.size() || steps > packets_.size()) {
        std::exit(4);
      }
      const auto& packet = packets_[handle - 1];
      words.push_back(packet.front() & kHeadLengthMask);
      words.insert(words.end(), packet.begin() + 1, packet.end());
      handle = packet.front() & r::kPacketLinkMask;
    }
    return words;
  }

 private:
  std::vector<std::vector<std::uint32_t>> packets_;
  std::uint32_t head_ = 1;
};

class DrawWorld final : public b::FloatingLabelFeedbackWorld {
 public:
  DrawWorld(const Image& image, std::uint32_t seed)
      : image_(image), seed_(seed) {}
  Queue labels;
  Queue text;
  Queue equipment;
  std::size_t words = 0;

  std::optional<b::FloatingLabelAtlasRect> ValueCaption(
      std::uint8_t raw_mode) override {
    const auto index = static_cast<std::uint32_t>(raw_mode) - kFirstCaption;
    if (index >= kCaptionKinds) return std::nullopt;
    const auto rectangle = image_.Word(kCaptionMap + index * kWordBytes);
    if (rectangle >= kCaptionRects) return std::nullopt;
    return image_.Rect(kCaptionAtlas + rectangle * kRectBytes);
  }
  std::optional<std::int16_t> LockedHalfHeight(std::int32_t step) override {
    if (step < 0 || static_cast<std::size_t>(step) >= kHeightSteps) {
      return std::nullopt;
    }
    return image_.Half(kHalfHeights + static_cast<std::uint32_t>(step) * 2);
  }
  std::optional<b::FloatingLabelAtlasRect> LockedPiece(
      std::size_t piece) override {
    if (piece >= kPieceCount) return std::nullopt;
    return image_.Rect(kLockedPieces +
                       static_cast<std::uint32_t>(piece * kRectBytes));
  }
  bool DrawQuad(const b::FloatingLabelQuad& quad) override {
    return Draw(labels, quad);
  }
  bool DrawText(std::string_view /*value*/,
                b::FloatingLabelScreenPoint /*origin*/,
                std::uint32_t /*color*/) override {
    return false;
  }
  std::optional<b::StatusLabelLayout> StatusLayout(
      std::uint16_t status) override {
    if (status >= kLayoutCount) return std::nullopt;
    const auto address = kStatusLayouts + status * kWordBytes;
    return b::StatusLabelLayout{
        .count = image_.Byte(address),
        .captions = {image_.Byte(address + 1), image_.Byte(address + 2)},
        .color = image_.Byte(address + 3)};
  }
  std::optional<b::FloatingLabelAtlasRect> CaptionRect(
      std::uint8_t caption) override {
    return Rect(kCaptionAtlas, kCaptionRects, caption);
  }
  std::optional<r::QuadColor> StatusColor(std::uint8_t color) override {
    if (color >= kColorCount) return std::nullopt;
    const auto address = kStatusColors + color * kWordBytes;
    return r::QuadColor{image_.Byte(address), image_.Byte(address + 1),
                        image_.Byte(address + 2)};
  }
  std::optional<b::FloatingLabelAtlasRect> StatChangeFirstWord(
      std::uint8_t word) override {
    return Rect(kFirstWords, kFirstWordCount, word);
  }
  std::optional<b::FloatingLabelAtlasRect> StatChangeSecondWord(
      std::uint8_t word) override {
    return Rect(kSecondWords, kSecondWordCount, word);
  }
  bool DrawEquipmentQuad(const b::FloatingLabelQuad& quad) override {
    return Draw(equipment, quad);
  }

 private:
  [[nodiscard]] std::optional<b::FloatingLabelAtlasRect> Rect(
      std::uint32_t table, std::size_t count, std::uint8_t index) const {
    if (index >= count) return std::nullopt;
    return image_.Rect(table + static_cast<std::uint32_t>(index * kRectBytes));
  }
  bool Draw(Queue& queue, const b::FloatingLabelQuad& quad) {
    r::UiTexturedQuadPacket storage{};
    for (std::size_t index = 0; index < storage.size(); ++index) {
      storage[index] =
          seed_ ^ (static_cast<std::uint32_t>(words + index) * kSeedStride);
    }
    const auto packet =
        r::BuildUiTexturedQuadPacket(quad, storage, queue.ChainLink());
    words += packet.size();
    return queue.Emit(packet);
  }

  const Image& image_;
  std::uint32_t seed_;
};

class Records : public b::FloatingLabelRecords {
 public:
  std::array<b::FloatingLabelRecord, kCapacity> records{};
  std::int16_t count = 0;
  std::int16_t LabelCount() override { return count; }
  std::optional<b::FloatingLabelRecord> Label(std::int32_t index) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) {
      return std::nullopt;
    }
    return records.at(static_cast<std::size_t>(index));
  }
  bool WriteLabel(std::int32_t index,
                  const b::FloatingLabelRecord& record) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) return false;
    records.at(static_cast<std::size_t>(index)) = record;
    return true;
  }
};

class PlacedPoints final : public b::PlacedLabelPositions {
 public:
  std::array<b::LabelScreenPoint, kCapacity> points{};
  std::int16_t count = 0;
  std::int16_t Count() override { return count; }
  void SetCount(std::int16_t value) override { count = value; }
  std::optional<b::LabelScreenPoint> Position(std::int32_t index) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) {
      return std::nullopt;
    }
    return points.at(static_cast<std::size_t>(index));
  }
  bool SetPosition(std::int32_t index, b::LabelScreenPoint point) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) return false;
    points.at(static_cast<std::size_t>(index)) = point;
    return true;
  }
};

struct Bone {
  std::int32_t status = 0;
  b::LabelPoint point{};
};

class UpdateWorld final : public b::FloatingLabelUpdateWorld {
 public:
  Records labels;
  PlacedPoints placed;
  std::array<Bone, kActors> bones{};
  std::vector<Output> calls;

  std::int16_t LabelCount() override { return labels.count; }
  std::optional<b::FloatingLabelRecord> Label(std::int32_t index) override {
    return labels.Label(index);
  }
  bool WriteLabel(std::int32_t index,
                  const b::FloatingLabelRecord& record) override {
    return labels.WriteLabel(index, record);
  }
  b::PlacedLabelPositions& Placed() override { return placed; }
  std::int32_t TaggedBonePosition(std::int32_t actor, std::int32_t tag,
                                  std::int32_t endpoint,
                                  b::LabelPoint& point) override {
    calls.push_back({kBoneCall, actor, tag, endpoint});
    const auto& bone = bones.at(static_cast<std::size_t>(actor) % kActors);
    if (bone.status == 0) point = bone.point;
    return bone.status;
  }
  std::int32_t Project(const b::LabelPoint& point,
                       b::LabelScreenPoint& screen) override {
    calls.push_back({kProjectCall, point.x, point.y, point.z});
    screen = {math::WrapSigned16(static_cast<std::uint32_t>(point.x + kProjectX)),
              math::WrapSigned16(static_cast<std::uint32_t>(point.y + kProjectY))};
    return point.z + kVisibleBias;
  }
  bool DrawAbilityTimingCue() override {
    calls.push_back({kCueCall});
    return true;
  }
  bool DrawLeaf(b::FloatingLabelLeaf leaf, std::int32_t index,
                b::FloatingLabelScreenPoint origin) override {
    const auto record = labels.Label(index);
    if (!record) return false;
    Output call{kLeafCall, static_cast<long long>(leaf), index, origin.x,
                origin.y};
    PrintRecord(call, record.value());
    calls.push_back(call);
    return true;
  }
  bool CompactLabels(std::int32_t removal_count) override {
    calls.push_back({kCompactCall, removal_count});
    return true;
  }
};

class EquipmentQueue final : public b::EquipmentStatChangeQueue {
 public:
  f::Queue queue;
  std::int16_t LabelCount() override { return queue.count; }
  std::optional<b::FloatingLabelRecord> Label(std::int32_t index) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) {
      return std::nullopt;
    }
    return std::bit_cast<b::FloatingLabelRecord>(
        queue.records.at(static_cast<std::size_t>(index)));
  }
  bool WriteLabel(std::int32_t index,
                  const b::FloatingLabelRecord& record) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) return false;
    queue.records.at(static_cast<std::size_t>(index)) =
        std::bit_cast<f::Record>(record);
    return true;
  }
  std::optional<std::int32_t> ReserveLabel() override {
    const auto slot = b::ReserveFloatingLabel(queue);
    if (!slot) return std::nullopt;
    return slot->index;
  }
  bool CommitLabel(std::int32_t index) override {
    return b::CommitFloatingLabel(queue, {index});
  }
};

void AppendCalls(Output& out, const std::vector<Output>& calls) {
  out.push_back(static_cast<long long>(calls.size()));
  for (const auto& call : calls) out.insert(out.end(), call.begin(), call.end());
}

Output Update(std::istringstream& in) {
  UpdateWorld world;
  world.labels.count = static_cast<std::int16_t>(Next(in));
  for (auto& record : world.labels.records) record = ReadRecord(in);
  for (auto& point : world.placed.points) {
    point = std::bit_cast<b::LabelScreenPoint>(
        static_cast<std::uint32_t>(Next(in)));
  }
  world.placed.count = 0x5A5A;
  for (auto& bone : world.bones) {
    bone.status = static_cast<std::int32_t>(Next(in));
    bone.point.x = math::WrapSigned16(static_cast<std::uint32_t>(Next(in)));
    bone.point.y = math::WrapSigned16(static_cast<std::uint32_t>(Next(in)));
    bone.point.z = math::WrapSigned16(static_cast<std::uint32_t>(Next(in)));
  }
  auto cue = static_cast<std::int16_t>(Next(in));
  auto timer = static_cast<std::int16_t>(Next(in));
  const auto tick = static_cast<std::int32_t>(Next(in));
  const bool paused = Next(in) != 0;
  const bool hidden = Next(in) != 0;
  const bool success = b::UpdateAndDrawFloatingLabels(
      world, {cue, timer, tick, paused, hidden});
  Output out;
  for (const auto& record : world.labels.records) PrintRecord(out, record);
  out.push_back(success ? world.placed.count : kRefused);
  for (const auto point : world.placed.points) out.push_back(PointWord(point));
  out.push_back(cue);
  out.push_back(timer);
  AppendCalls(out, world.calls);
  return out;
}

Output Place(std::istringstream& in) {
  b::LabelScreenPoint position{static_cast<std::int16_t>(Next(in)),
                               static_cast<std::int16_t>(Next(in))};
  auto label = ReadRecord(in);
  PlacedPoints placed;
  placed.count = static_cast<std::int16_t>(Next(in));
  for (auto& point : placed.points) {
    point = std::bit_cast<b::LabelScreenPoint>(
        static_cast<std::uint32_t>(Next(in)));
  }
  const bool success = b::PlaceActorValueLabel(position, label, placed);
  Output out{position.x, position.y};
  PrintRecord(out, label);
  out.push_back(success ? placed.count : kRefused);
  for (const auto point : placed.points) out.push_back(PointWord(point));
  out.push_back(0);
  return out;
}

Output Draw(std::istringstream& in, const Image& image, int mode) {
  const b::FloatingLabelScreenPoint origin{static_cast<std::int32_t>(Next(in)),
                                           static_cast<std::int32_t>(Next(in))};
  const auto label = ReadRecord(in);
  DrawWorld world(image, static_cast<std::uint32_t>(Next(in)));
  bool success = false;
  if (mode == kValue) {
    success = b::DrawActorValueLabel(world, origin, label);
  } else if (mode == kStatus) {
    success = b::DrawActorStatusLabel(world, origin, label);
  } else {
    success = b::DrawEquipmentStatChange(world, origin, label);
  }
  Output out{success ? static_cast<long long>(world.words) : kRefused};
  PrintRecord(out, label);
  for (const auto* queue : {&world.labels, &world.text, &world.equipment}) {
    const auto words = queue->Traversal();
    out.push_back(static_cast<long long>(words.size()));
    out.insert(out.end(), words.begin(), words.end());
  }
  out.push_back(0);
  return out;
}

Output QueueChange(std::istringstream& in) {
  const auto kind = static_cast<std::int32_t>(Next(in));
  const auto stat = static_cast<std::int32_t>(Next(in));
  const auto amount = static_cast<std::uint32_t>(Next(in));
  const b::EquipmentStatChangeSettings settings{.weapon = Next(in) != 0,
                                                .armor = Next(in) != 0};
  EquipmentQueue queue;
  queue.queue.count = static_cast<std::int16_t>(Next(in));
  for (auto& record : queue.queue.records) {
    record = std::bit_cast<f::Record>(ReadRecord(in));
  }
  const auto result =
      b::QueueEquipmentStatChange(kind, stat, amount, settings, queue);
  Output out{result ? *result : kRefused, queue.queue.count};
  for (const auto& record : queue.queue.records) {
    PrintRecord(out, std::bit_cast<b::FloatingLabelRecord>(record));
  }
  out.push_back(0);
  return out;
}

Output Commit(std::istringstream& in) {
  Records records;
  records.count = static_cast<std::int16_t>(Next(in));
  for (auto& record : records.records) record = ReadRecord(in);
  const bool success = b::CommitEquipmentStatChanges(records);
  Output out{success ? records.count : kRefused};
  for (const auto& record : records.records) PrintRecord(out, record);
  out.push_back(0);
  return out;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 1;
  const Image image(argv[1]);
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    const auto mode = static_cast<int>(Next(in));
    Output out;
    switch (mode) {
      case kUpdate:
        out = Update(in);
        break;
      case kPlace:
        out = Place(in);
        break;
      case kValue:
      case kStatus:
      case kEquipDraw:
        out = Draw(in, image, mode);
        break;
      case kQueue:
        out = QueueChange(in);
        break;
      case kCommit:
        out = Commit(in);
        break;
      default:
        std::abort();
    }
    for (std::size_t index = 0; index < out.size(); ++index) {
      std::cout << (index != 0 ? " " : "") << out[index];
    }
    std::cout << '\n';
  }
}
