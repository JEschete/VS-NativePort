// Native side of tools\port\differentials\floating_label_producers_*.py (F00714-F00718):
// one vector per stdin line, one line of integers per vector, in the order
// floating_label_producers_common.py prints the original's observation.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/FloatingLabelProducers.hpp"
#include "TestSupport/FloatingLabelProducersFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace fixture = VS::test_support::floating_label_producers;

enum Mode : int { kValue, kTiming, kChain, kStatuses, kStatus };
constexpr int kRecordWords = 6;
constexpr int kBoneResults = 1, kTaggedResults = 2, kProjections = 29;
// Printed in place of the return value when the typed core refuses.
constexpr long long kRefused = 0x7FFFFFFF;

long long Next(std::istringstream& in)
{
    long long value = 0;
    if (!(in >> value))
        std::abort();
    return value;
}

std::vector<fixture::PointResult> ReadResults(std::istringstream& in, int count)
{
    std::vector<fixture::PointResult> results;
    for (int i = 0; i < count; ++i) {
        fixture::PointResult result;
        result.status = static_cast<std::int32_t>(Next(in));
        result.point.x = static_cast<std::int16_t>(Next(in));
        result.point.y = static_cast<std::int16_t>(Next(in));
        result.point.z = static_cast<std::int16_t>(Next(in));
        result.point.w = static_cast<std::int16_t>(Next(in));
        results.push_back(result);
    }
    return results;
}
} // namespace

int main(int, char**)
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const int mode = static_cast<int>(Next(in));
        std::array<std::int32_t, 4> arguments { };
        for (auto& argument : arguments)
            argument = static_cast<std::int32_t>(Next(in));
        fixture::World world;
        world.queue.count = static_cast<std::int16_t>(Next(in));
        for (std::size_t record = 0; record < b::kFloatingLabelCapacity; ++record) {
            std::array<std::uint32_t, kRecordWords> words { };
            for (auto& word : words)
                word = static_cast<std::uint32_t>(Next(in));
            world.SetRecord(record, std::bit_cast<b::FloatingLabelRecord>(words));
        }
        world.found_bone = static_cast<std::int32_t>(Next(in));
        world.bone_positions = ReadResults(in, kBoneResults);
        world.tagged_positions = ReadResults(in, kTaggedResults);
        world.projections = ReadResults(in, kProjections);
        world.random = static_cast<std::int32_t>(Next(in));
        std::optional<std::int32_t> result;
        switch (mode) {
        case kValue:
            result = b::QueueActorValueLabel(arguments[0], arguments[1],
                static_cast<std::uint32_t>(arguments[2]), arguments[3], world);
            break;
        case kTiming:
            result = b::QueueAbilityTimingResult(arguments[0], world);
            break;
        case kChain:
            result = b::QueueChainCount(arguments[0], world);
            break;
        case kStatuses:
            result = b::QueueActorStatusLabels(
                arguments[0], arguments[2], arguments[3], world);
            break;
        case kStatus:
            result = b::QueueActorStatusLabel(
                arguments[0], arguments[1], arguments[2], arguments[3], world);
            break;
        default:
            std::abort();
        }
        std::vector<long long> values { result ? *result : kRefused, world.queue.count };
        for (std::size_t record = 0; record < b::kFloatingLabelCapacity; ++record) {
            const auto words = std::bit_cast<std::array<std::uint32_t, kRecordWords>>(
                world.Record(record));
            values.insert(values.end(), words.begin(), words.end());
        }
        values.push_back(static_cast<long long>(world.calls.size()));
        for (const auto& call : world.calls) {
            values.push_back(static_cast<long long>(call.service));
            values.insert(values.end(), call.arguments.begin(), call.arguments.end());
        }
        for (std::size_t i = 0; i < values.size(); ++i)
            std::cout << (i ? " " : "") << values[i];
        std::cout << '\n';
    }
}
