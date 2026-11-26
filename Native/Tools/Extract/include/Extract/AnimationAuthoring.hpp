#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>

namespace VS::Extract {

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
