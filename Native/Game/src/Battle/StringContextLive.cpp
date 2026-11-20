#include "Battle/StringContextLive.hpp"

#include <cstddef>
#include <span>

#include "Battle/TargetingState.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"

namespace VS::Battle {
static_assert(VS::Menu::kStringContextSlots ==
              Render::kEncodedTextArgumentCount);

Render::EncodedTextArguments LiveEncodedTextArguments() {
  Render::EncodedTextArguments arguments;
  const std::span integers(stringContext.integers);
  const std::span strings(stringContext.strings);
  for (std::size_t index = 0; index < Render::kEncodedTextArgumentCount;
       ++index) {
    arguments.integers.at(index) = integers[index];
    arguments.strings.at(index) =
        Render::encoded_text_live::TrustedText(strings[index]);
  }
  return arguments;
}
}  // namespace VS::Battle
