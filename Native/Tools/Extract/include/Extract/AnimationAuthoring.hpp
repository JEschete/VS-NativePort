#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>

#include "Content/JsonText.hpp"

namespace VS::Extract {

// Reads the JSON output `file` under `root` and writes it back formatted when
// `edit` reports that it changed it; whether it was written. A missing file
// is left for its converter to write again.
[[nodiscard]] std::expected<bool, std::string> EditJsonOutput(
    const std::filesystem::path& root, const std::string& file,
    const std::function<
        std::expected<bool, std::string>(Content::JsonDocument&)>& edit);

// `named`, led by the display_name label `numbered` carries, when it has one.
[[nodiscard]] Content::JsonDocument LabeledJson(
    const Content::JsonDocument& numbered, Content::JsonDocument named);

struct AnimationAuthoringReport {
  std::size_t documents = 0;  // documents moved to the current schema version
  std::size_t events = 0;     // events rewritten in their named form
  std::size_t written = 0;    // files written, with the manifest
};

// Rewrites the animation event scripts of existing character model, animation
// set and archetype outputs in their named form (Content/SeqEventForms.hpp)
// without reconverting source assets or replacing authored values. Each
// event is checked to read back as the same bytes; the converter versions are
// published last, and a file whose documents are already current is left as
// it is, so an interrupted run resumes.
[[nodiscard]] std::expected<AnimationAuthoringReport, std::string>
UpgradeAnimationAuthoring(const std::filesystem::path& root);

}  // namespace VS::Extract
