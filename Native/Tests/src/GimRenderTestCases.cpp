#include <catch2/catch_test_macros.hpp>

extern "C" int Tests_GimEventLayerLayout(void);
extern "C" int Tests_EncodedTextFormatting(void);

TEST_CASE(
    "Encoded text formatting retains zero glyphs and terminates nested strings",
    "[render][text]") {
  REQUIRE(Tests_EncodedTextFormatting() == 1);
}

TEST_CASE("Event splash controls share the renderer's three native GIM layers",
          "[gim][layout]") {
  REQUIRE(Tests_GimEventLayerLayout() == 1);
}

extern "C" int Tests_GimTilePackets(int columns, int rows, int scale,
                                    int offsetX, int offsetY, int routine,
                                    int brightness, int framebuffer);
extern "C" int Tests_ScaledDialogueGlyphs(void);
extern "C" int Tests_CompactTextAndGaugeLinks(void);
extern "C" int CheckFeedbackEffectDialogueLinks(int framebuffer,
                                                int feedback_layer);

TEST_CASE("Compact text and gauge chains retain native packet links",
          "[render][text]") {
  REQUIRE(Tests_CompactTextAndGaugeLinks() == 1);
}

TEST_CASE("Scaled dialogue preserves glyph layout and unsigned control bytes",
          "[render][text]") {
  REQUIRE(Tests_ScaledDialogueGlyphs() == 1);
}

TEST_CASE("GIM tiles preserve fixed-point coordinates and native packet links",
          "[render][gim]") {
  const int cases[][5] = {
      {5, 16, 4096, 0, 0},  {5, 16, 2048, 0, 0},          {5, 16, 4095, 0, 0},
      {5, 16, 4097, 0, 0},  {5, 16, 16383, 0, 0},         {2, 3, 4096, 17, -9},
      {2, 3, 2048, -17, 9}, {5, 16, 4096 | 0x4000, 0, 0},
  };
  for (const auto& c : cases) {
    CAPTURE(c[0], c[1], c[2], c[3], c[4]);
    REQUIRE(Tests_GimTilePackets(c[0], c[1], c[2], c[3], c[4], 0, 128, 0) == 0);
  }
}

TEST_CASE("GIM scaled tiles clip offscreen rows and columns", "[render][gim]") {
  const int offsets[][2] = {
      {0, 0}, {-200, -100}, {200, 100}, {5000, 5000}, {-5000, -5000}};
  for (const auto& offset : offsets) {
    CAPTURE(offset[0], offset[1]);
    REQUIRE(Tests_GimTilePackets(5, 16, 8192, offset[0], offset[1], 0, 128,
                                 0) == 0);
  }
}

TEST_CASE("GIM byte-sized coordinate bounds do not overlap shared scratch",
          "[render][gim][bounds]") {
  const int dimensions[][2] = {{0, 0}, {255, 0}, {0, 255}};
  for (const auto& d : dimensions) {
    CAPTURE(d[0], d[1]);
    REQUIRE(Tests_GimTilePackets(d[0], d[1], 16383, 0, 0, 0, 128, 0) == 0);
  }
}

TEST_CASE("GIM grids respect the embedded tile-table capacity",
          "[render][gim][bounds]") {
  const int dimensions[][2] = {
      {250, 1}, {1, 250}, {25, 10}, {251, 1}, {1, 251}, {255, 255},
  };
  for (const auto& d : dimensions) {
    CAPTURE(d[0], d[1]);
    // Zero scale keeps every valid tile visible, exercising the entire table
    // and packet-buffer bounds.
    REQUIRE(Tests_GimTilePackets(d[0], d[1], 0, 0, 0, 0, 128, 0) == 0);
  }
}

TEST_CASE("GIM framebuffer fades retain tile and framebuffer packet chains",
          "[render][gim][fade]") {
  for (int framebuffer = 0; framebuffer < 2; ++framebuffer) {
    for (const int scale : {2048, 4096}) {
      for (const int brightness : {0, 64, 128}) {
        CAPTURE(framebuffer, scale, brightness);
        REQUIRE(Tests_GimTilePackets(5, 16, scale, 0, 0, 1, brightness,
                                     framebuffer) == 0);
      }
    }
  }
}

TEST_CASE("GIM wipe and crossfade fast paths use the shared tile renderer",
          "[render][gim][fade]") {
  REQUIRE(Tests_GimTilePackets(5, 16, 4096, 0, 0, 2, 32, 0) == 0);
  REQUIRE(Tests_GimTilePackets(5, 16, 4096, 0, 0, 3, 128, 0) == 0);
  REQUIRE(Tests_GimTilePackets(5, 16, 2048, 0, 0, 3, 128, 0) == 0);
}

TEST_CASE("Screen feedback preserves the dialogue ordering chain",
          "[render][text][feedback]") {
  for (const int framebuffer : {0, 1}) {
    for (const int feedback_layer : {1, 2, 128}) {
      CAPTURE(framebuffer, feedback_layer);
      REQUIRE(CheckFeedbackEffectDialogueLinks(framebuffer, feedback_layer) ==
              1);
    }
  }
}

extern "C" int CheckMenuRowNativeLinks(int scrolling);

TEST_CASE("Menu rows use native overlay links without changing scene ordering",
          "[render][menu][links]") {
  REQUIRE(CheckMenuRowNativeLinks(0) == 1);
  REQUIRE(CheckMenuRowNativeLinks(1) == 1);
}

extern "C" int CheckMenuWorkspaceReset(void);

TEST_CASE("Menu initialization clears every native row and scratch byte",
          "[render][menu][workspace]") {
  REQUIRE(CheckMenuWorkspaceReset() == 1);
}

extern "C" int CheckStatusPacketLinks(int background_first);

TEST_CASE("Status background and limb packets preserve the complete UI chain",
          "[render][menu][status]") {
  REQUIRE(CheckStatusPacketLinks(0) == 1);
  REQUIRE(CheckStatusPacketLinks(1) == 1);
}
