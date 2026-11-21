#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Legacy/Render/LibGpu.hpp"
#include "Render/DrawList.hpp"
#include "Render/DrawPolygon.hpp"
#include "Render/PageSpace.hpp"

namespace {
using VS::u16;
using VS::u32;
using VS::Render::DrawCorner;
using VS::Render::DrawPolygon;
using VS::Render::DrawTarget;
using VS::Render::FrameDrawList;
using VS::Render::GameFrameDrawList;
using VS::Render::GamePages;
using VS::Render::PageSpace;

struct CornerSpec {
  std::int16_t x, y;
  std::uint8_t red, green, blue, u, v;
};

constexpr std::array<CornerSpec, 4> kCorners{{
    {12, 8, 0x40, 0x80, 0xC0, 0, 0},
    {70, 14, 0xFF, 0x20, 0x10, 31, 2},
    {16, 60, 0x10, 0xFF, 0x80, 1, 14},
    {66, 58, 0x80, 0x80, 0x80, 30, 15},
}};

constexpr u16 kClut = static_cast<u16>(VS::Legacy::getClut(768, 0));
constexpr u16 kPage = static_cast<u16>(VS::Legacy::getTPage(0, 2, 640, 256));

u32 Rgb(const CornerSpec& c) {
  return c.red | (static_cast<u32>(c.green) << 8) |
         (static_cast<u32>(c.blue) << 16);
}

u32 Xy(const CornerSpec& c, std::int16_t dx, std::int16_t dy) {
  return static_cast<u16>(c.x + dx) |
         (static_cast<u32>(static_cast<u16>(c.y + dy)) << 16);
}

// The GP0 polygon packet for `command` (0x20-0x3F) over kCorners, moved by
// (dx, dy).
std::vector<u32> Packet(u32 command, std::int16_t dx = 0, std::int16_t dy = 0) {
  const bool gouraud = (command & 0x10) != 0;
  const bool textured = (command & 0x04) != 0;
  const std::size_t n = (command & 0x08) != 0 ? 4 : 3;
  std::vector<u32> words{(command << 24) | Rgb(kCorners[0])};
  for (std::size_t k = 0; k < n; ++k) {
    if (gouraud && k > 0) {
      words.push_back(Rgb(kCorners[k]));
    }
    words.push_back(Xy(kCorners[k], dx, dy));
    if (textured) {
      u32 uv = kCorners[k].u | (static_cast<u32>(kCorners[k].v) << 8);
      uv |= k == 0 ? static_cast<u32>(kClut) << 16 : 0;
      uv |= k == 1 ? static_cast<u32>(kPage) << 16 : 0;
      words.push_back(uv);
    }
  }
  return words;
}

// The same polygon as a typed draw, built from the specification rather than
// from the packet.
DrawPolygon Typed(u32 command, std::int16_t dx = 0, std::int16_t dy = 0) {
  DrawPolygon polygon;
  polygon.material.gouraud = (command & 0x10) != 0;
  polygon.material.textured = (command & 0x04) != 0;
  polygon.material.semi_transparent = (command & 0x02) != 0;
  polygon.material.raw_texture = (command & 0x01) != 0;
  polygon.material.page.word = kPage;
  polygon.material.palette.word = kClut;
  polygon.corner_count = (command & 0x08) != 0 ? 4 : 3;
  for (std::size_t k = 0; k < kCorners.size(); ++k) {
    const CornerSpec& c = kCorners[k];
    polygon.corners[k] = DrawCorner{static_cast<std::int16_t>(c.x + dx),
                                    static_cast<std::int16_t>(c.y + dy),
                                    c.red,
                                    c.green,
                                    c.blue,
                                    c.u,
                                    c.v};
  }
  return polygon;
}

// A shaded background (so semi-transparency is visible) and a 4-bit texture
// with a palette.
void Prepare(PageSpace& pages, bool offset) {
  pages.Fill({0, 0, 160, 120}, 0x30, 0x60, 0x90);
  const std::array<u16, 4> palette{0, 0x03E0, 0x7C00, 0x801F};
  pages.Upload({768, 0, 4, 1}, palette.data());
  std::array<u16, std::size_t{8} * 16> texels{};
  for (std::size_t i = 0; i < texels.size(); ++i) {
    texels[i] = static_cast<u16>(0x1230 + (i % 3));
  }
  pages.Upload({640, 256, 8, 16}, texels.data());
  if (offset) {
    pages.SetDrawOffset(5, 3);
  }
}

// An untextured semi-transparent quad: it blends in whichever mode the last
// textured polygon made current.
constexpr std::array<u32, 5> SemiFollower() {
  return {0x2A604020u, 0x00140008u, 0x00140050u, 0x00460008u, 0x00460050u};
}

bool SamePixels(const PageSpace& a, const PageSpace& b) {
  return std::ranges::equal(a.Pixels(), b.Pixels());
}

// ---- frame draw list

// One scene addition: a polygon packet or, when typed, the same polygon as a
// draw-list item. kDrawArea adds a packet that narrows the draw area instead.
struct SceneOp {
  std::size_t slot;
  u32 command;
  std::int16_t dx, dy;
};
constexpr u32 kDrawArea = 0;

// Overlapping polygons, some semi-transparent (so order changes pixels), in
// a slot that receives several additions, a slot with one and an empty one.
constexpr std::array<SceneOp, 7> kSceneOps{{
    {3, 0x3C, 0, 0},
    {0, 0x30, 10, 5},
    {3, 0x2E, 6, 4},
    {1, 0x38, 3, 8},
    {2, kDrawArea, 0, 0},
    {3, 0x34, 12, 2},
    {0, 0x3A, 4, 4},
}};

// The packets for `op`, area limits included.
std::vector<u32> OpPacket(const SceneOp& op) {
  if (op.command == kDrawArea) {
    return {0xE3000000u | (4u << 10) | 6u, 0xE4000000u | (50u << 10) | 60u};
  }
  return Packet(op.command, op.dx, op.dy);
}

// A reverse ordering table of `entries` with packets, walked from its last
// entry like the gameplay frame's.
class SceneTable {
 public:
  explicit SceneTable(std::size_t entries) : table_(entries) {
    VS::Legacy::ClearOTagR(table_.data(), static_cast<int>(entries));
    packets_.reserve(kSceneOps.size() + 1);
  }
  std::span<u32> Entries(std::size_t first) {
    return std::span(table_).subspan(first);
  }
  const void* AddPacket(std::size_t entry, std::vector<u32> words) {
    words.insert(words.begin(), 0);
    std::vector<u32>& packet = packets_.emplace_back(std::move(words));
    VS::Legacy::setlen(packet.data(), packet.size() - 1);
    VS::Legacy::addPrim(&table_[entry], packet.data());
    return packet.data();
  }
  void Clear() {
    VS::Legacy::ClearOTagR(table_.data(), static_cast<int>(table_.size()));
  }
  int Walk(std::size_t from_entry) {
    return VS::Legacy::GpuWalkList(&table_[from_entry]);
  }
  int Walk() { return Walk(table_.size() - 1); }

 private:
  std::vector<u32> table_;
  std::vector<std::vector<u32>> packets_;
};

// Leaves the game's list closed whatever a test does.
struct DrawListGuard {
  DrawListGuard() = default;
  DrawListGuard(const DrawListGuard&) = delete;
  DrawListGuard& operator=(const DrawListGuard&) = delete;
  ~DrawListGuard() { static_cast<void>(GameFrameDrawList().Abandon()); }
};

void ResetGamePages() {
  PageSpace& pages = GamePages();
  pages.SetDrawArea(0, 0, PageSpace::Width - 1, PageSpace::Height - 1);
  pages.SetDrawOffset(0, 0);
  pages.SetTextureWindow(0);
  pages.SetTexturePage(0);
  pages.SetDither(false);
  pages.SetMask(false, false);
  Prepare(pages, false);
}

std::vector<u16> GamePixels() {
  const std::span<const u16> pixels = GamePages().Pixels();
  return {pixels.begin(), pixels.end()};
}

// The index of the first game page pixel that differs from `expected`, or
// its size when none does: a failed check reports one position rather than
// printing both page images.
std::size_t FirstChangedPixel(const std::vector<u16>& expected) {
  const std::span<const u16> pixels = GamePages().Pixels();
  if (pixels.size() != expected.size()) return 0;
  return static_cast<std::size_t>(std::ranges::mismatch(pixels, expected).in1 -
                                  pixels.begin());
}

// The pixels the ops draw as packets alone.
std::vector<u16> PacketScene(std::size_t first_entry) {
  SceneTable scene(first_entry + 4);
  for (const SceneOp& op : kSceneOps) {
    static_cast<void>(scene.AddPacket(first_entry + op.slot, OpPacket(op)));
  }
  ResetGamePages();
  static_cast<void>(scene.Walk());
  return GamePixels();
}
}  // namespace

TEST_CASE("Typed polygons rasterize exactly like their GP0 packets",
          "[render][gpu][typed-draw]") {
  const u32 command = GENERATE(range(0x20u, 0x40u));
  const bool offset = GENERATE(false, true);
  CAPTURE(command, offset);
  PageSpace packet;
  PageSpace typed;
  Prepare(packet, offset);
  Prepare(typed, offset);
  const std::vector<u32> words = Packet(command);
  CHECK(packet.Execute(words) == words.size());
  CHECK(typed.Draw(Typed(command)));
  CHECK(SamePixels(packet, typed));
  CHECK(packet.Execute(SemiFollower()) == SemiFollower().size());
  CHECK(typed.Execute(SemiFollower()) == SemiFollower().size());
  CHECK(SamePixels(packet, typed));
}

TEST_CASE("A typed draw into a target writes only inside it",
          "[render][gpu][typed-draw][bounds]") {
  PageSpace pages;
  Prepare(pages, false);
  pages.SetDrawArea(0, 0, 50, 40);
  const DrawPolygon polygon = Typed(0x28);
  CHECK(pages.Draw(polygon, PageSpace::Rect{30, 20, 40, 30}));
  bool outside_untouched = true;
  bool inside_drawn = true;
  for (int y = 0; y < 70; ++y) {
    for (int x = 0; x < 80; ++x) {
      const bool inside = x >= 30 && x <= 50 && y >= 20 && y <= 40;
      const bool background = pages.At(x, y) == pages.At(0, 0);
      outside_untouched = outside_untouched && (inside || background);
      inside_drawn = inside_drawn && (!inside || !background);
    }
  }
  CHECK(outside_untouched);
  CHECK(inside_drawn);
  // The draw area is restored afterwards.
  CHECK(pages.Draw(polygon));
  CHECK(pages.At(20, 15) != pages.At(0, 0));
  CHECK(pages.At(60, 30) == pages.At(0, 0));
}

TEST_CASE("A typed draw outside its target still selects its texture page",
          "[render][gpu][typed-draw][bounds]") {
  PageSpace typed;
  PageSpace packet;
  Prepare(typed, false);
  Prepare(packet, false);
  CHECK(typed.Draw(Typed(0x3C), PageSpace::Rect{200, 200, 0, 0}));
  packet.SetTexturePage(kPage);
  CHECK(SamePixels(packet, typed));
  CHECK(typed.Execute(SemiFollower()) == SemiFollower().size());
  CHECK(packet.Execute(SemiFollower()) == SemiFollower().size());
  CHECK(SamePixels(packet, typed));
}

TEST_CASE("A typed polygon with an invalid corner count is refused",
          "[render][gpu][typed-draw]") {
  const std::uint8_t corners =
      GENERATE(std::uint8_t{0}, std::uint8_t{2}, std::uint8_t{5});
  CAPTURE(corners);
  PageSpace typed;
  PageSpace untouched;
  Prepare(typed, false);
  Prepare(untouched, false);
  DrawPolygon polygon = Typed(0x3C);
  polygon.corner_count = corners;
  CHECK_FALSE(typed.Draw(polygon));
  CHECK(typed.Execute(SemiFollower()) == SemiFollower().size());
  CHECK(untouched.Execute(SemiFollower()) == SemiFollower().size());
  CHECK(SamePixels(untouched, typed));
}

TEST_CASE("Draw-list items draw where their packets would",
          "[render][gpu][typed-draw][draw-list]") {
  // Bit k makes op k a typed item; the draw-area op stays a packet.
  const unsigned typed_ops = GENERATE(range(0u, 1u << kSceneOps.size()));
  // From entry 0 the table's slot 0 ends the list; from 1 it links on.
  const std::size_t first_entry = GENERATE(std::size_t{0}, std::size_t{1});
  CAPTURE(typed_ops, first_entry);
  const std::vector<u16> expected = PacketScene(first_entry);
  const DrawListGuard guard;
  FrameDrawList& list = GameFrameDrawList();
  SceneTable scene(first_entry + 4);
  REQUIRE(list.Open(scene.Entries(first_entry),
                    DrawTarget{.surface = &GamePages()}));
  for (std::size_t k = 0; k < kSceneOps.size(); ++k) {
    const SceneOp& op = kSceneOps[k];
    if (((typed_ops >> k) & 1u) == 0 || op.command == kDrawArea) {
      static_cast<void>(scene.AddPacket(first_entry + op.slot, OpPacket(op)));
    } else {
      CHECK(list.Submit(Typed(op.command, op.dx, op.dy), op.slot).has_value());
    }
  }
  ResetGamePages();
  static_cast<void>(scene.Walk());
  CHECK(FirstChangedPixel(expected) == expected.size());
  // A second walk of the same frame draws the items again.
  ResetGamePages();
  static_cast<void>(scene.Walk());
  CHECK(FirstChangedPixel(expected) == expected.size());
  CHECK(list.Close() == 0);
}

TEST_CASE("Clearing a scene table discards its draw-list items",
          "[render][gpu][typed-draw][draw-list]") {
  const DrawListGuard guard;
  FrameDrawList& list = GameFrameDrawList();
  SceneTable scene(4);
  REQUIRE(list.Open(scene.Entries(0), DrawTarget{.surface = &GamePages()}));
  CHECK(list.Submit(Typed(0x3C), 2).has_value());
  CHECK(list.Submit(Typed(0x30, 10, 5), 0).has_value());
  scene.Clear();
  CHECK(list.LiveCount() == 0);
  ResetGamePages();
  const std::vector<u16> before = GamePixels();
  CHECK(scene.Walk() == 4);
  CHECK(FirstChangedPixel(before) == before.size());
  CHECK(list.Close() == 0);
}

TEST_CASE("A walk that skips an item's slot entry does not draw it",
          "[render][gpu][typed-draw][draw-list]") {
  const DrawListGuard guard;
  FrameDrawList& list = GameFrameDrawList();
  SceneTable scene(4);
  REQUIRE(list.Open(scene.Entries(0), DrawTarget{.surface = &GamePages()}));
  static_cast<void>(scene.AddPacket(3, Packet(0x38, 3, 8)));
  CHECK(list.Submit(Typed(0x3C), 3).has_value());
  const void* later = scene.AddPacket(3, Packet(0x30, 10, 5));
  CHECK(list.Submit(Typed(0x34, 12, 2), 1).has_value());
  ResetGamePages();
  // Starting at the later packet follows the link to the earlier one, the
  // slot 3 item's anchor, without passing slot 3's entry; the slot 1 item is
  // reached through its entry. Packets: later, earlier and entries 2-0.
  CHECK(VS::Legacy::GpuWalkList(later) == 5);
  CHECK(list.Close() == 1);
  CHECK_FALSE(list.IsOpen());
}

TEST_CASE("The frame draw list refuses submissions it cannot place",
          "[render][gpu][typed-draw][draw-list][bounds]") {
  const DrawListGuard guard;
  FrameDrawList& list = GameFrameDrawList();
  static_cast<void>(list.Abandon());
  SceneTable scene(4);
  CHECK_FALSE(list.Submit(Typed(0x3C), 0).has_value());
  CHECK_FALSE(list.Open(scene.Entries(4), DrawTarget{.surface = &GamePages()}));
  CHECK_FALSE(list.Open(scene.Entries(0), DrawTarget{}));
  CHECK_FALSE(list.IsOpen());
  REQUIRE(list.Open(scene.Entries(0), DrawTarget{.surface = &GamePages()}));
  CHECK_FALSE(list.Submit(Typed(0x3C), 4).has_value());
  DrawPolygon invalid = Typed(0x3C);
  invalid.corner_count = 2;
  CHECK_FALSE(list.Submit(invalid, 0).has_value());
  CHECK(list.RefusedCount() == 2);
  CHECK(list.ItemCount() == 0);
}

TEST_CASE("The frame draw list holds a bounded number of items",
          "[render][gpu][typed-draw][draw-list][bounds]") {
  const DrawListGuard guard;
  FrameDrawList& list = GameFrameDrawList();
  SceneTable scene(4);
  REQUIRE(list.Open(scene.Entries(0), DrawTarget{.surface = &GamePages()}));
  const DrawPolygon polygon = Typed(0x30);
  std::size_t accepted = 0;
  while (list.Submit(polygon, accepted % 4).has_value()) {
    ++accepted;
  }
  CHECK(accepted == FrameDrawList::kCapacity);
  CHECK(list.RefusedCount() == 1);
  // Reopening for the next frame discards the unclosed frame's items.
  REQUIRE(list.Open(scene.Entries(0), DrawTarget{.surface = &GamePages()}));
  CHECK(list.ItemCount() == 0);
  CHECK(list.RefusedCount() == 0);
  CHECK(list.Submit(polygon, 0).has_value());
  CHECK(list.Abandon() == 1);
  CHECK_FALSE(list.IsOpen());
}
