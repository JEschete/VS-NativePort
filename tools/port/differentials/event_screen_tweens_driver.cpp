// Native side of the event screen tween differentials (event_screen_tweens_common.py,
// #276). One vector per line: mode, the selected tween, six instruction bytes, the tick
// speed, the scale, tint and offset the readers return, the scratchpad word, the curve
// salt, the two roll tweens and the four screen tweens. Prints the result, every tween
// field, the scratchpad word and the port calls as [count, kind and arguments of each
// call...].
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

#include "Event/EventScreenTweens.hpp"

namespace {
namespace ev = VS::Event;

enum Mode : std::uint8_t { kCapture, kRoll, kAngle, kTransform, kTweenSet, kTween };

enum Call : std::uint8_t {
    kEnabled = 1,
    kDepth,
    kPrevious,
    kScale,
    kTint,
    kOffset,
    kBlend,
    kReadScale,
    kReadTint,
    kReadOffset,
    kWarp,
    kEase,
};

constexpr std::int64_t kCurveModulus = 0x2001;
constexpr std::int64_t kCurveBias = 0x800;

using Calls = std::vector<std::vector<std::int64_t>>;

class RecordingScreen final : public ev::EventScreenEffectPort {
public:
    RecordingScreen(Calls& calls, const std::array<std::int64_t, 7>& reads)
        : calls_(calls)
        , reads_(reads)
    {
    }

    void SetCaptureEnabled(std::int32_t enabled) override
    {
        calls_.push_back({ kEnabled, enabled });
    }
    void SetDepth(std::int32_t depth) override { calls_.push_back({ kDepth, depth }); }
    void SetPreviousFrame(std::int32_t use_previous_frame) override
    {
        calls_.push_back({ kPrevious, use_previous_frame });
    }
    void SetScale(ev::EventScreenPair scale) override
    {
        calls_.push_back({ kScale, scale.x, scale.y });
    }
    void SetTint(ev::EventScreenTint tint) override
    {
        calls_.push_back({ kTint, tint.red, tint.green, tint.blue });
    }
    void SetOffset(ev::EventScreenPair offset) override
    {
        calls_.push_back({ kOffset, offset.x, offset.y });
    }
    void SetBlendMode(std::int32_t blend_mode) override
    {
        calls_.push_back({ kBlend, blend_mode });
    }
    ev::EventScreenPair ReadScale() override
    {
        calls_.push_back({ kReadScale });
        return { .x = static_cast<std::int32_t>(reads_[0]),
            .y = static_cast<std::int32_t>(reads_[1]) };
    }
    ev::EventScreenTint ReadTint() override
    {
        calls_.push_back({ kReadTint });
        return { .red = static_cast<std::uint8_t>(reads_[2]),
            .green = static_cast<std::uint8_t>(reads_[3]),
            .blue = static_cast<std::uint8_t>(reads_[4]) };
    }
    ev::EventScreenPair ReadOffset() override
    {
        calls_.push_back({ kReadOffset });
        return { .x = static_cast<std::int32_t>(reads_[5]),
            .y = static_cast<std::int32_t>(reads_[6]) };
    }
    void SetRadialWarp(
        VS::Effects::ScreenDistortion::RadialWarpSettings settings) override
    {
        calls_.push_back({ kWarp, settings.scale, settings.growth });
    }

private:
    Calls& calls_;
    const std::array<std::int64_t, 7>& reads_;
};

class SaltedEasing final : public ev::EventTweenEasing {
public:
    SaltedEasing(Calls& calls, std::int64_t salt, std::int32_t& scratch)
        : calls_(calls)
        , salt_(salt)
        , scratch_(scratch)
    {
    }

    std::int32_t EaseFraction(
        std::uint8_t mode, std::int16_t frame, std::int16_t duration) override
    {
        calls_.push_back({ kEase, mode, frame, duration });
        const std::int64_t sum = std::int64_t { mode } * 0x35
                               + std::int64_t { frame } * 0x101
                               + std::int64_t { duration } * 7 + salt_;
        return static_cast<std::int32_t>(
            ((sum % kCurveModulus) + kCurveModulus) % kCurveModulus - kCurveBias);
    }
    void RecordEasedFraction(std::int32_t fraction) override { scratch_ = fraction; }

private:
    Calls& calls_;
    std::int64_t salt_;
    std::int32_t& scratch_;
};

ev::EventAngleTween ReadAngle()
{
    std::array<std::int64_t, 7> f { };
    for (auto& item : f) {
        std::cin >> item;
    }
    return { .changed = static_cast<std::uint8_t>(f[0]),
        .easing = static_cast<std::uint8_t>(f[1]),
        .angle = static_cast<std::int16_t>(f[2]),
        .start = static_cast<std::int16_t>(f[3]),
        .delta = static_cast<std::int16_t>(f[4]),
        .elapsed = static_cast<std::int16_t>(f[5]),
        .duration = static_cast<std::int16_t>(f[6]) };
}

ev::EventScreenTween ReadTween()
{
    std::array<std::int64_t, 12> f { };
    for (auto& item : f) {
        std::cin >> item;
    }
    ev::EventScreenTween tween { .easing = static_cast<std::uint8_t>(f[0]),
        .elapsed = static_cast<std::int16_t>(f[10]),
        .duration = static_cast<std::int16_t>(f[11]) };
    for (std::size_t index = 0; index < 3; ++index) {
        tween.current[index] = static_cast<std::int16_t>(f[1 + index]);
        tween.start[index] = static_cast<std::int16_t>(f[4 + index]);
        tween.delta[index] = static_cast<std::int16_t>(f[7 + index]);
    }
    return tween;
}

void PrintAngle(const ev::EventAngleTween& tween)
{
    std::cout << ' ' << int { tween.changed } << ' ' << int { tween.easing } << ' '
              << tween.angle << ' ' << tween.start << ' ' << tween.delta << ' '
              << tween.elapsed << ' ' << tween.duration;
}

void PrintTween(const ev::EventScreenTween& tween)
{
    std::cout << ' ' << int { tween.easing };
    for (const auto* values : { &tween.current, &tween.start, &tween.delta }) {
        for (const std::int16_t item : *values) {
            std::cout << ' ' << item;
        }
    }
    std::cout << ' ' << tween.elapsed << ' ' << tween.duration;
}
} // namespace

int main()
{
    std::int64_t mode = 0;
    while (std::cin >> mode) {
        std::int64_t which = 0;
        std::cin >> which;
        std::array<std::uint8_t, 6> instruction { };
        for (auto& byte : instruction) {
            std::int64_t item = 0;
            std::cin >> item;
            byte = static_cast<std::uint8_t>(item);
        }
        std::int64_t tick = 0;
        std::cin >> tick;
        std::array<std::int64_t, 7> reads { };
        for (auto& item : reads) {
            std::cin >> item;
        }
        std::int64_t scratch_input = 0;
        std::int64_t salt = 0;
        std::cin >> scratch_input >> salt;
        std::array<ev::EventAngleTween, 2> rolls { ReadAngle(), ReadAngle() };
        ev::EventScreenTweens tweens { .scale = ReadTween(),
            .tint = ReadTween(),
            .translation = ReadTween(),
            .radial_warp = ReadTween() };
        std::array<ev::EventScreenTween*, 4> tween_list { &tweens.scale, &tweens.tint,
            &tweens.translation, &tweens.radial_warp };

        Calls calls;
        auto scratch = static_cast<std::int32_t>(scratch_input);
        RecordingScreen screen(calls, reads);
        SaltedEasing easing(calls, salt, scratch);
        const auto speed = static_cast<std::int32_t>(tick);
        const ev::ScriptCommand command { instruction };
        std::int64_t result = 0;
        switch (mode) {
        case kCapture:
            result = ev::CommandSetScreenCapture(command, screen) ? 0 : -999;
            break;
        case kRoll:
            result = ev::CommandTweenCameraRoll(command, rolls[0], rolls[1]) ? 0 : -999;
            break;
        case kAngle:
            ev::UpdateEventAngleTween(
                rolls.at(static_cast<std::size_t>(which)), speed, easing);
            break;
        case kTransform:
            result = ev::CommandScreenTransform(command, tweens, screen).value_or(-999);
            break;
        case kTweenSet:
            ev::UpdateEventScreenTransforms(tweens, speed, easing, screen);
            break;
        case kTween:
            result = ev::UpdateEventTripleTween(
                         *tween_list.at(static_cast<std::size_t>(which)), speed, easing)
                             .has_value()
                       ? 1
                       : 0;
            break;
        default:
            break;
        }

        std::cout << result;
        for (const auto& roll : rolls) {
            PrintAngle(roll);
        }
        for (const auto* tween : tween_list) {
            PrintTween(*tween);
        }
        std::cout << ' ' << scratch << ' ' << calls.size();
        for (const auto& call : calls) {
            for (const std::int64_t item : call) {
                std::cout << ' ' << item;
            }
        }
        std::cout << '\n';
    }
    return 0;
}
