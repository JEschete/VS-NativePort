#pragma once

#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Log.hpp"

namespace VS::test_support {
// Collects the warnings logged on one channel while it lives, then restores
// the previous observer.
struct ChannelWarnings {
  std::string channel;
  std::vector<std::string> messages;
  Core::LogObserver saved;

  explicit ChannelWarnings(std::string name)
      : channel(std::move(name)),
        saved(Core::SetLogObserver([this](const Core::LogRecord& record) {
          if (record.channel == channel) {
            CHECK(record.level == Core::LogLevel::Warn);
            messages.emplace_back(record.message);
          }
        })) {}
  ChannelWarnings(const ChannelWarnings&) = delete;
  ChannelWarnings& operator=(const ChannelWarnings&) = delete;
  ChannelWarnings(ChannelWarnings&&) = delete;
  ChannelWarnings& operator=(ChannelWarnings&&) = delete;
  ~ChannelWarnings() { Core::SetLogObserver(std::move(saved)); }
};

// "<operation> refused: <reason>", the refusal warnings' form.
[[nodiscard]] inline std::string Refused(const std::string& operation,
                                         const std::string& reason) {
  return operation + " refused: " + reason;
}
}  // namespace VS::test_support
