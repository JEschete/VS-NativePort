#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/Disc.hpp"
#include "Content/MovieEncoder.hpp"
#include "Launcher/Setup.hpp"
#include "Platform/App.hpp"
#include "Platform/Paths.hpp"

namespace {
namespace fs = std::filesystem;
namespace Content = VS::Content;
using VS::u32;
using VS::u8;
using VS::usize;

constexpr usize kSectorRaw = Content::Disc::SectorRaw;
constexpr usize kSectorData = Content::Disc::SectorData;
constexpr usize kMode2DataOffset = 24;
constexpr usize kChdFrameBytes = 2448;
constexpr usize kChdFramesPerHunk = 8;
constexpr usize kChdHeaderBytes = 124;
constexpr u32 kFileSector = 19;
constexpr u32 kImageSectors = 21;
constexpr std::string_view kFileContents = "synthetic movie bytes";

struct DiscFixture {
  fs::path root =
      fs::temp_directory_path() /
      ("vs-disc-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count())) /
      VS::Platform::Utf8Path("paths with spaces \xE6\xB8\xAC\xE8\xA9\xA6");

  DiscFixture() { fs::create_directories(root); }
  ~DiscFixture() {
    try {
      std::error_code error;
      fs::remove_all(root.parent_path(), error);
    } catch (const std::exception& failure) {
      std::fprintf(stderr, "disc fixture left files: %s\n", failure.what());
    }
  }
};

void WriteLittle32(std::vector<u8>& bytes, usize offset, u32 value) {
  for (usize index = 0; index < sizeof(u32); ++index) {
    bytes[offset + index] = static_cast<u8>(value >> (index * 8));
  }
}

// A Mode 2 image with an ISO9660 root holding one interleaved file, TITLE.STR.
std::vector<u8> SyntheticImage(u32 sectors = kImageSectors) {
  std::vector<u8> image(sectors * kSectorRaw);
  const auto data = [&](u32 sector) {
    return sector * kSectorRaw + kMode2DataOffset;
  };
  const usize descriptor = data(16);
  image[descriptor] = 1;
  constexpr std::string_view kSignature = "CD001";
  std::ranges::copy(kSignature, image.begin() + descriptor + 1);
  const usize root = descriptor + 156;
  image[root] = 34;
  WriteLittle32(image, root + 2, 18);
  WriteLittle32(image, root + 10, static_cast<u32>(kSectorData));
  image[root + 25] = 2;
  image[root + 32] = 1;

  constexpr std::string_view kName = "TITLE.STR;1";
  const usize record = data(18);
  const usize extension = 33 + kName.size();
  image[record] = static_cast<u8>(extension + 14 + 1);
  WriteLittle32(image, record + 2, kFileSector);
  WriteLittle32(image, record + 10, static_cast<u32>(kFileContents.size()));
  image[record + 32] = static_cast<u8>(kName.size());
  std::ranges::copy(kName, image.begin() + record + 33);
  image[record + extension + 4] = 0x60;  // Form 2 and interleaved
  image[record + extension + 6] = 'X';
  image[record + extension + 7] = 'A';
  std::ranges::copy(kFileContents, image.begin() + data(kFileSector));
  return image;
}

void AppendBig(std::vector<u8>& bytes, std::uint64_t value, usize width) {
  for (usize index = width; index > 0; --index) {
    bytes.push_back(static_cast<u8>(value >> ((index - 1) * 8)));
  }
}

struct ChdLayout {
  std::vector<std::string> tracks;
  usize unit_bytes = kChdFrameBytes;
};

std::string TrackText(u32 frames) {
  return "TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:" +
         std::to_string(frames) + " PREGAP:0 PGTYPE:MODE1 PGSUB:NONE POSTGAP:0";
}

// An uncompressed CHD v5. Hunks beyond the image are unmapped, so they read
// back as zeros and a disc-sized file stays small.
std::vector<u8> ChdFile(const std::vector<u8>& image, u32 frames,
                        const ChdLayout& layout) {
  const usize hunk_bytes = layout.unit_bytes * kChdFramesPerHunk;
  const usize hunks = (frames + kChdFramesPerHunk - 1) / kChdFramesPerHunk;
  std::vector<u8> metadata;
  usize next = kChdHeaderBytes;
  std::vector<std::vector<u8>> blocks;
  for (usize track = 0; track < layout.tracks.size(); ++track) {
    const std::string& text = layout.tracks[track];
    const usize length = text.size() + 1;
    next += 16 + length;
    std::vector<u8> block{'C', 'H', 'T', '2'};
    AppendBig(block, 0x01000000U | length, 4);
    AppendBig(block, track + 1 < layout.tracks.size() ? next : 0, 8);
    block.insert(block.end(), text.begin(), text.end());
    block.push_back(0);
    blocks.push_back(std::move(block));
  }
  for (const auto& block : blocks) {
    metadata.insert(metadata.end(), block.begin(), block.end());
  }
  const usize map_offset = next;
  const usize image_hunks =
      (image.size() / kSectorRaw + kChdFramesPerHunk - 1) / kChdFramesPerHunk;
  const usize first_hunk =
      (map_offset + hunks * 4 + hunk_bytes - 1) / hunk_bytes;

  std::vector<u8> file{'M', 'C', 'o', 'm', 'p', 'r', 'H', 'D'};
  AppendBig(file, kChdHeaderBytes, 4);
  AppendBig(file, 5, 4);
  file.resize(32, 0);  // no compressors
  AppendBig(file, hunks * hunk_bytes, 8);
  AppendBig(file, map_offset, 8);
  AppendBig(file, layout.tracks.empty() ? 0 : kChdHeaderBytes, 8);
  AppendBig(file, hunk_bytes, 4);
  AppendBig(file, layout.unit_bytes, 4);
  file.resize(kChdHeaderBytes, 0);  // checksums are not read on this path
  file.insert(file.end(), metadata.begin(), metadata.end());
  for (usize hunk = 0; hunk < hunks; ++hunk) {
    AppendBig(file, hunk < image_hunks ? first_hunk + hunk : 0, 4);
  }
  // kSectorRaw more, as a frame that is too small still holds a whole sector.
  file.resize(first_hunk * hunk_bytes + image_hunks * hunk_bytes + kSectorRaw,
              0);
  for (usize frame = 0; frame < image.size() / kSectorRaw; ++frame) {
    std::copy_n(
        image.begin() + static_cast<std::ptrdiff_t>(frame * kSectorRaw),
        kSectorRaw,
        file.begin() + static_cast<std::ptrdiff_t>(first_hunk * hunk_bytes +
                                                   frame * layout.unit_bytes));
  }
  return file;
}

void Write(const fs::path& path, const std::vector<u8>& bytes) {
  std::ofstream stream(path, std::ios::binary);
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

std::string Contents(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

fs::path WriteChd(const DiscFixture& fixture, const std::vector<u8>& image,
                  u32 frames, const ChdLayout& layout) {
  const auto path = fixture.root / "game.chd";
  Write(path, ChdFile(image, frames, layout));
  return path;
}

// Stands in for FFmpeg: lists `encoders`, and either fails a conversion or
// writes its last argument the way a successful one would.
fs::path WriteFakeFfmpeg(const DiscFixture& fixture, std::string_view encoders,
                         bool converts) {
  // Windows cannot start a .cmd by a path with spaces, unlike an .exe.
  const auto path = fixture.root.parent_path() / "fake-ffmpeg.cmd";
  std::ofstream script(path, std::ios::binary);
  script << "@echo off\r\n"
            "if \"%~1\"==\"-version\" exit /b 0\r\n"
            "if \"%~2\"==\"-encoders\" (\r\n";
  std::string_view rest = encoders;
  while (!rest.empty()) {
    const auto end = rest.find('\n');
    script << " echo " << rest.substr(0, end) << "\r\n";
    rest = end == std::string_view::npos ? std::string_view{}
                                         : rest.substr(end + 1);
  }
  script << " exit /b 0\r\n)\r\n";
  if (converts) {
    script << "for %%a in (%*) do set \"last=%%~a\"\r\n"
              "echo converted> \"%last%\"\r\n"
              "exit /b 0\r\n";
  } else {
    script << "echo Unknown encoder 1>&2\r\nexit /b 8\r\n";
  }
  return path;
}

struct ExtractRun {
  int exit_code;
  std::string output;
  std::string manifest;
};

// Runs VS.Extract on a CUE over the synthetic image.
ExtractRun RunExtract(const DiscFixture& fixture, const fs::path& ffmpeg) {
  const auto extract = VS::Platform::ExecutableDir() / "VS.Extract.exe";
  const auto cue = fixture.root / "game.cue";
  Write(fixture.root / "game.bin", SyntheticImage());
  std::ofstream(cue) << "FILE \"game.bin\" BINARY\n"
                        " TRACK 01 MODE2/2352\n INDEX 01 00:00:00\n";
  // Beside the root, as a batch file cannot write to its Unicode name.
  const auto out = fixture.root.parent_path() / "out";
  const auto log = fixture.root / "extract.log";
  const auto result = VS::Platform::RunProcess(
      std::vector<std::string>{VS::Platform::PathText(extract), "extract",
                               VS::Platform::PathText(cue),
                               VS::Platform::PathText(out), "--ffmpeg",
                               VS::Platform::PathText(ffmpeg)},
      log);
  REQUIRE(result.has_value());
  return {*result, Contents(log), Contents(out / "files.tsv")};
}
}  // namespace

TEST_CASE("A CHD reads back as the same files as its image",
          "[content][disc][chd]") {
  const DiscFixture fixture;
  const auto disc = Content::Disc::Open(WriteChd(
      fixture, SyntheticImage(), kImageSectors, {{TrackText(kImageSectors)}}));
  REQUIRE(disc.has_value());
  CHECK((*disc)->IsImage());
  CHECK((*disc)->SectorCount() == kImageSectors);
  const auto file =
      (*disc)->Find("TITLE.STR").value_or(Content::DiscFile{.path = "missing"});
  REQUIRE(file.path == "TITLE.STR");
  CHECK(file.lba == kFileSector);
  CHECK(file.interleaved);
  const auto bytes = (*disc)->ReadFile(file);
  REQUIRE(bytes.has_value());
  CHECK(std::string_view(reinterpret_cast<const char*>(bytes->data()),
                         bytes->size()) == kFileContents);
}

TEST_CASE("A CHD reads back as the same sectors as its image",
          "[content][disc][chd]") {
  const DiscFixture fixture;
  const auto image = SyntheticImage();
  const auto disc = Content::Disc::Open(
      WriteChd(fixture, image, kImageSectors, {{TrackText(kImageSectors)}}));
  REQUIRE(disc.has_value());
  // Crosses a hunk, and goes back to an earlier one.
  const u32 first = GENERATE(0U, 6U, 15U, 16U, kImageSectors - 2);
  std::vector<u8> raw(2 * kSectorRaw);
  REQUIRE((*disc)->ReadRawSectors(first, 2, raw).has_value());
  CHECK(std::ranges::equal(
      raw, std::span(image).subspan(first * kSectorRaw, raw.size())));
}

TEST_CASE("A CHD sector count comes from its track, not its padded hunks",
          "[content][disc][chd]") {
  const DiscFixture fixture;
  constexpr u32 kFrames = kImageSectors + 1;
  const auto disc = Content::Disc::Open(
      WriteChd(fixture, SyntheticImage(), kFrames, {{TrackText(kFrames)}}));
  REQUIRE(disc.has_value());
  CHECK((*disc)->SectorCount() == kFrames);
  std::vector<u8> raw(kSectorRaw);
  CHECK((*disc)->ReadRawSectors(kFrames - 1, 1, raw).has_value());
  CHECK_FALSE((*disc)->ReadRawSectors(kFrames, 1, raw).has_value());
}

TEST_CASE("CHD layouts other than one raw data track are refused",
          "[content][disc][chd]") {
  const DiscFixture fixture;
  const auto frames = std::to_string(kImageSectors);
  const auto track = TrackText(kImageSectors);
  struct Row {
    ChdLayout layout;
    std::string reason;
  };
  const auto row = GENERATE_COPY(
      Row{ChdLayout{{}}, "not a CD image"},
      Row{ChdLayout{{track,
                     "TRACK:2 TYPE:AUDIO SUBTYPE:NONE FRAMES:300 "
                     "PREGAP:0 PGTYPE:MODE1 PGSUB:NONE POSTGAP:0"}},
          "more than one track"},
      Row{ChdLayout{{"TRACK:1 TYPE:AUDIO SUBTYPE:NONE FRAMES:" + frames +
                     " PREGAP:0 PGTYPE:MODE1 PGSUB:NONE POSTGAP:0"}},
          "track type 'AUDIO'"},
      Row{ChdLayout{{"TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:" + frames +
                     " PREGAP:150 PGTYPE:VMODE1 PGSUB:NONE POSTGAP:0"}},
          "gaps"},
      Row{ChdLayout{{"TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:broken"}},
          "track length 'broken'"},
      Row{ChdLayout{{"TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:9999"}},
          "track length '9999'"},
      Row{ChdLayout{{track}, 512}, "not a CD image"});
  CAPTURE(row.reason);
  const auto disc = Content::Disc::Open(
      WriteChd(fixture, SyntheticImage(), kImageSectors, row.layout));
  REQUIRE_FALSE(disc.has_value());
  CHECK(disc.error().find(row.reason) != std::string::npos);
}

TEST_CASE("A damaged CHD is refused with its path in the error",
          "[content][disc][chd]") {
  const DiscFixture fixture;
  const auto path = fixture.root / "game.chd";
  std::ofstream(path, std::ios::binary) << "not a compressed disc";
  const auto disc = Content::Disc::Open(path);
  REQUIRE_FALSE(disc.has_value());
  CHECK(disc.error().find(VS::Platform::PathText(path)) != std::string::npos);
}

TEST_CASE("A CHD of the wrong size is rejected before hashing",
          "[launcher][disc-identity][chd]") {
  const DiscFixture fixture;
  bool hashed = false;
  const auto result = VS::Launcher::VerifyDisc(
      WriteChd(fixture, SyntheticImage(), kImageSectors,
               {{TrackText(kImageSectors)}}),
      [&](int) { hashed = true; });
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find("Wrong CHD size") != std::string::npos);
  CHECK_FALSE(hashed);
}

TEST_CASE("A CHD of the right size with the wrong bytes fails the checksum",
          "[launcher][disc-identity][chd]") {
  const DiscFixture fixture;
  const auto frames =
      static_cast<u32>(VS::Launcher::kSupportedDiscBytes / kSectorRaw);
  const auto chd =
      WriteChd(fixture, SyntheticImage(), frames, {{TrackText(frames)}});
  const auto result = VS::Launcher::VerifyDisc(chd);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find("checksum mismatch") != std::string::npos);
  CHECK(result.error().find("No extraction or game launch") !=
        std::string::npos);
}

TEST_CASE("A CHD holding the approved disc passes identity verification",
          "[launcher][disc-identity][disc][chd]") {
  auto dir = VS::Platform::ExecutableDir();
  fs::path chd;
  for (; !dir.empty() && dir != dir.parent_path() && chd.empty();
       dir = dir.parent_path()) {
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(dir / "disks", error)) {
      if (entry.path().extension() == ".chd") chd = entry.path();
    }
  }
  if (chd.empty()) SKIP("No owner-provided CHD is available");
  const auto result = VS::Launcher::VerifyDisc(chd);
  REQUIRE(result.has_value());
  CHECK(result->sha256 == VS::Launcher::kSupportedDiscSha256);
  CHECK(result->image == fs::absolute(chd).lexically_normal());
  CHECK(result->read_lock.get() != nullptr);
  const auto disc = Content::Disc::Open(result->image);
  REQUIRE(disc.has_value());
  CHECK((*disc)->Find("SLUS_010.40").has_value());
  CHECK((*disc)->Find("MOV/TITLE.STR").has_value());
}

TEST_CASE("The launcher's picker error names every supported disc file",
          "[launcher]") {
  const DiscFixture fixture;
  const auto result = VS::Launcher::VerifyDisc(fixture.root / "game.iso");
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find(".cue") != std::string::npos);
  CHECK(result.error().find(".chd") != std::string::npos);
  CHECK(result.error().find(".bin") != std::string::npos);
}

TEST_CASE("A directly chosen BIN goes straight to the disc identity check",
          "[launcher][disc-identity]") {
  const DiscFixture fixture;
  const auto bin = fixture.root / "any name at all.BIN";
  std::ofstream(bin, std::ios::binary) << "too short";
  const auto result = VS::Launcher::VerifyDisc(bin);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find("Wrong BIN size") != std::string::npos);
}

TEST_CASE("The approved BIN passes without a CUE",
          "[launcher][disc-identity][disc]") {
  auto dir = VS::Platform::ExecutableDir();
  while (!dir.empty() && dir != dir.parent_path() &&
         !fs::exists(dir / "disks" / "SLUS-01040.bin")) {
    dir = dir.parent_path();
  }
  const auto bin = dir / "disks" / "SLUS-01040.bin";
  if (!fs::exists(bin)) SKIP("Owner-provided supported disc is unavailable");
  const auto result = VS::Launcher::VerifyDisc(bin);
  REQUIRE(result.has_value());
  CHECK(result->sha256 == VS::Launcher::kSupportedDiscSha256);
  CHECK(result->image == fs::absolute(bin).lexically_normal());
}

TEST_CASE("The extractor stops when a movie cannot be converted",
          "[extract][movie]") {
  if (!fs::exists(VS::Platform::ExecutableDir() / "VS.Extract.exe")) {
    SKIP("VS.Extract.exe is not built beside the tests");
  }
  const DiscFixture fixture;
  const auto run = RunExtract(
      fixture, WriteFakeFfmpeg(fixture, " V....D libsvtav1 SVT-AV1", false));
  INFO(run.output);
  CHECK(run.exit_code != 0);
  CHECK(run.output.find("TITLE.STR") != std::string::npos);
  CHECK(run.output.find("Unknown encoder") != std::string::npos);
  CHECK(run.manifest.find("TITLE.STR") == std::string::npos);
  // It stopped there rather than carrying on to the audio step.
  CHECK(run.output.find("no effect-package") == std::string::npos);
}

TEST_CASE("The extractor records a movie it did convert", "[extract][movie]") {
  if (!fs::exists(VS::Platform::ExecutableDir() / "VS.Extract.exe")) {
    SKIP("VS.Extract.exe is not built beside the tests");
  }
  const DiscFixture fixture;
  const auto run = RunExtract(
      fixture, WriteFakeFfmpeg(fixture, " V....D libsvtav1 SVT-AV1", true));
  INFO(run.output);
  CHECK(run.manifest.find("mkv\tTITLE.STR") != std::string::npos);
  CHECK(fs::exists(fixture.root.parent_path() / "out" / "disc" /
                   "TITLE.STR.mkv"));
}

TEST_CASE("Movies use the faster AV1 encoder the FFmpeg build has",
          "[extract][movie]") {
  struct Row {
    const char* listing;
    const char* encoder;
  };
  const auto row = GENERATE(
      Row{" V....D libaom-av1  x\n V....D libsvtav1  x\n", "libsvtav1"},
      Row{"Encoders:\n V..... = Video\n ------\n V....D libaom-av1  x\n",
          "libaom-av1"},
      Row{" V....D libx264  x\n A....D flac  x\n", nullptr},
      // Named in passing, or as an audio encoder, is not having it.
      Row{" V....D wrapper  uses libsvtav1\n A....D libsvtav1  x\n", nullptr},
      Row{"", nullptr});
  CAPTURE(row.listing);
  const auto encoder =
      Content::ChooseAv1Encoder(row.listing).value_or(Content::Av1Encoder{});
  CHECK(encoder.name == (row.encoder == nullptr ? "" : row.encoder));
  CHECK(encoder.options.empty() == (row.encoder == nullptr));
}

TEST_CASE("An FFmpeg without an AV1 encoder is named in the error",
          "[extract][movie]") {
  const DiscFixture fixture;
  const auto ffmpeg = WriteFakeFfmpeg(fixture, " V....D libx264 H.264", false);
  const auto scratch = fixture.root / "encoders.txt";
  const auto result = Content::FindAv1Encoder(ffmpeg, scratch);
  REQUIRE_FALSE(result.has_value());
  INFO(result.error());
  CHECK(result.error().find("libsvtav1") != std::string::npos);
  CHECK(result.error().find("libaom-av1") != std::string::npos);
  CHECK(result.error().find(VS::Platform::PathText(ffmpeg)) !=
        std::string::npos);
  CHECK_FALSE(fs::exists(scratch));
}

TEST_CASE("The encoder check ignores an earlier answer", "[extract][movie]") {
  const DiscFixture fixture;
  const auto ffmpeg = WriteFakeFfmpeg(fixture, " V....D libaom-av1 AV1", false);
  const auto scratch = fixture.root / "encoders.txt";
  std::ofstream(scratch) << " V....D libsvtav1 stale\n";
  const auto result = Content::FindAv1Encoder(ffmpeg, scratch);
  REQUIRE(result.has_value());
  CHECK(result->name == "libaom-av1");
}

TEST_CASE("An FFmpeg that cannot list its encoders is reported",
          "[extract][movie]") {
  const DiscFixture fixture;
  const auto missing = fixture.root / "missing-ffmpeg.exe";
  const auto result =
      Content::FindAv1Encoder(missing, fixture.root / "encoders.txt");
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find(VS::Platform::PathText(missing)) !=
        std::string::npos);
}

TEST_CASE("The extractor reports a missing encoder before writing files",
          "[extract][movie]") {
  if (!fs::exists(VS::Platform::ExecutableDir() / "VS.Extract.exe")) {
    SKIP("VS.Extract.exe is not built beside the tests");
  }
  const DiscFixture fixture;
  const auto run =
      RunExtract(fixture, WriteFakeFfmpeg(fixture, " V....D libx264 H", true));
  CHECK(run.exit_code != 0);
  CHECK(run.output.find("libaom-av1") != std::string::npos);
  CHECK_FALSE(fs::exists(fixture.root.parent_path() / "out" / "disc"));
}

TEST_CASE("Setup names a missing encoder before it starts extracting",
          "[launcher][disc]") {
  auto dir = VS::Platform::ExecutableDir();
  while (!dir.empty() && dir != dir.parent_path() &&
         !fs::exists(dir / "disks" / "SLUS-01040.cue")) {
    dir = dir.parent_path();
  }
  const auto cue = dir / "disks" / "SLUS-01040.cue";
  if (!fs::exists(cue)) SKIP("Owner-provided supported disc is unavailable");
  const DiscFixture fixture;
  for (const auto* name : {"VS.Desktop.exe", "VS.Extract.exe", "SDL3.dll",
                           "dav1d.dll", "sfizz.dll", "webm.dll", "chdr.dll"}) {
    std::ofstream(fixture.root / name) << "synthetic installation";
  }
  const auto ffmpeg = WriteFakeFfmpeg(fixture, " V....D libx264 H.264", false);
  const auto state = fixture.root / "state";
  const auto result =
      VS::Launcher::Prepare({cue, fixture.root / "saves", ffmpeg}, fixture.root,
                            state, [](const VS::Launcher::ProgressUpdate&) {});
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().starts_with(
      "This FFmpeg cannot convert the game's intro movie"));
  for (const auto& entry : fs::directory_iterator(state)) {
    CHECK_FALSE(entry.path().filename().string().starts_with("assets"));
  }
}
