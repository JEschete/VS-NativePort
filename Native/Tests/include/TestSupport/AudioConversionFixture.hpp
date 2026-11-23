#pragma once

#include <array>
#include <filesystem>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Content/AudioStream.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/MovieEncoder.hpp"
#include "Extract/AudioBankConversion.hpp"
#include "Extract/ConverterBatch.hpp"

namespace VS::test_support {

// Runs the sound-bank converter over `root` and records the XA and movie
// converters with no outputs, as extracting a disc without those files does,
// so a synthetic store's conversion check covers only what a test converts.
// Needs a manifest.json from an earlier converter.
inline void RecordAudioConversions(const std::filesystem::path& root) {
  const auto banks = Extract::ConvertDiscBanks(root, false);
  INFO((banks ? std::string{} : banks.error()));
  REQUIRE(banks.has_value());
  const auto loaded = Content::LoadConversionManifest(root);
  if (!loaded || !*loaded) {
    FAIL("no manifest.json to record the audio converters in");
    return;
  }
  const Content::ConversionManifest& previous = **loaded;
  const std::array batches{
      Extract::StartBatch(previous, Content::kXaFlacConverterName,
                          Content::kXaFlacConverterVersion, false),
      Extract::StartBatch(previous, Content::kStrMkvConverterName,
                          Content::kStrMkvConverterVersion, false)};
  const auto published = Extract::PublishBatches(root, previous, batches);
  INFO((published ? std::string{} : published.error()));
  REQUIRE(published.has_value());
}

}  // namespace VS::test_support
