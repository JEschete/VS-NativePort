// Native side of the event camera frame differentials (event_camera_frame_common.py,
// #276). One vector per line: mode, the field camera override, the script backdrop flag,
// the wait operand, the nine completion fields, the look-at and eye positions, the six
// partner words, the origin actor and offset, the shake count and offsets, five (changed,
// value) settings, the field camera's look-at and eye, the yaw, the rotation matrix and
// the product salt. Prints the result, the positions, the partner positions, the changed
// flags and the port calls as [count, kind and arguments of each call...].
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Event/EventCameraFrame.hpp"

namespace {
namespace ev = VS::Event;
using VS::Core::Math::Mat3;
using VS::Core::Math::Vec3;

enum Mode : std::uint8_t { kWait, kApply, kUpdate, kMask };

enum Call : std::uint8_t {
    kInit = 1,
    kLookAt,
    kPosition,
    kRoll,
    kScreen,
    kProjection,
    kNear,
    kFar,
    kAtan,
    kRotation,
    kApplyMatrix,
    kTrack,
    kAngle,
    kLens,
    kShake,
};

constexpr std::int64_t kRotationScratch = 0xD8;
constexpr std::int64_t kMatrixScratch = 0xF8;
constexpr std::int64_t kLookAtScratch = 0x98;
constexpr std::int64_t kEyeScratch = 0xA8;

using Calls = std::vector<std::vector<std::int64_t>>;

std::int64_t Read()
{
    std::int64_t item = 0;
    std::cin >> item;
    return item;
}

Vec3 ReadVec()
{
    const auto x = static_cast<std::int32_t>(Read());
    const auto y = static_cast<std::int32_t>(Read());
    const auto z = static_cast<std::int32_t>(Read());
    return { .x = x, .y = y, .z = z };
}

VS::Core::Math::Position3 ReadPosition()
{
    const auto x = static_cast<std::int16_t>(Read());
    const auto y = static_cast<std::int16_t>(Read());
    const auto z = static_cast<std::int16_t>(Read());
    return { .x = x, .y = y, .z = z };
}

class RecordingOutput final : public ev::EventCameraOutputPort {
public:
    RecordingOutput(Calls& calls, ev::EventCameraPair field, std::int32_t yaw,
        Mat3 matrix, std::array<std::int64_t, 3> salt)
        : calls_(calls)
        , field_(field)
        , yaw_(yaw)
        , matrix_(matrix)
        , salt_(salt)
    {
    }

    ev::EventCameraPair InitializeFieldCamera() override
    {
        calls_.push_back({ kInit, -1, -1 });
        return field_;
    }
    std::int32_t FixedAtan2(std::int32_t y, std::int32_t x) override
    {
        calls_.push_back({ kAtan, y, x });
        return yaw_;
    }
    Mat3 BuildRotation(VS::Core::Math::Angles3 rotation) override
    {
        calls_.push_back({ kRotation, kRotationScratch, kMatrixScratch, rotation.x,
            rotation.y, rotation.z });
        return matrix_;
    }
    Vec3 ApplyMatrix(
        const Mat3& rotation, Vec3 vector, ev::EventShakeTarget target) override
    {
        const std::int64_t slot =
            target == ev::EventShakeTarget::kLookAt ? kLookAtScratch : kEyeScratch;
        std::vector<std::int64_t> call { kApplyMatrix, kMatrixScratch, slot, slot };
        for (const auto& row : rotation.m) {
            for (const std::int16_t element : row) {
                call.push_back(element);
            }
        }
        const std::array<std::int64_t, 3> words { vector.x, vector.y, vector.z };
        call.insert(call.end(), words.begin(), words.end());
        calls_.push_back(call);
        std::array<std::int32_t, 3> out { };
        for (std::size_t row = 0; row < 3; ++row) {
            std::int64_t sum = 0;
            for (std::size_t column = 0; column < 3; ++column) {
                sum += std::int64_t { rotation.m.at(row).at(column) } * words.at(column);
            }
            out.at(row) = static_cast<std::int32_t>(
                static_cast<std::uint32_t>((sum >> 12) + salt_.at(row)));
        }
        return { .x = out[0], .y = out[1], .z = out[2] };
    }
    void SetLookAt(Vec3 look_at) override
    {
        calls_.push_back({ kLookAt, look_at.x, look_at.y, look_at.z });
    }
    void SetPosition(Vec3 position) override
    {
        calls_.push_back({ kPosition, position.x, position.y, position.z });
    }
    void SetRoll(std::int16_t angle) override { calls_.push_back({ kRoll, angle }); }
    void SetScreenRotation(std::int16_t angle) override
    {
        calls_.push_back({ kScreen, angle });
    }
    void SetProjectionDistance(std::int16_t distance) override
    {
        calls_.push_back({ kProjection, distance });
    }
    void SetNearClip(std::int16_t near_clip) override
    {
        calls_.push_back({ kNear, near_clip });
    }
    void SetFarClip(std::int16_t far_clip, std::uint8_t preserve_texture_lod) override
    {
        calls_.push_back({ kFar, far_clip, preserve_texture_lod });
    }

private:
    Calls& calls_;
    ev::EventCameraPair field_;
    std::int32_t yaw_;
    Mat3 matrix_;
    std::array<std::int64_t, 3> salt_;
};

class RecordingSteps final : public ev::EventCameraStepPort {
public:
    RecordingSteps(
        Calls& calls, ev::EventCameraPair current, ev::EventCameraPair& partners)
        : calls_(calls)
        , current_(current)
        , partners_(partners)
    {
    }

    ev::EventCameraPair CurrentPositions() override { return current_; }
    void SetPartnerPositions(const ev::EventCameraPair& partners) override
    {
        partners_ = partners;
    }
    void Advance(ev::EventCameraStep step) override
    {
        switch (step) {
        case ev::EventCameraStep::kLookAtTrack:
            calls_.push_back({ kTrack, 0, 0xB4 });
            break;
        case ev::EventCameraStep::kEyeTrack:
            calls_.push_back({ kTrack, 0xB4, 0 });
            break;
        case ev::EventCameraStep::kCameraRoll:
            calls_.push_back({ kAngle, 0x168 });
            break;
        case ev::EventCameraStep::kScreenRoll:
            calls_.push_back({ kAngle, 0x174 });
            break;
        case ev::EventCameraStep::kProjectionDistance:
            calls_.push_back({ kLens, 0x180 });
            break;
        case ev::EventCameraStep::kNearClip:
            calls_.push_back({ kLens, 0x18C });
            break;
        case ev::EventCameraStep::kFarClip:
            calls_.push_back({ kLens, 0x198 });
            break;
        case ev::EventCameraStep::kShake:
            calls_.push_back({ kShake });
            break;
        }
    }

private:
    Calls& calls_;
    ev::EventCameraPair current_;
    ev::EventCameraPair& partners_;
};

void Print(Vec3 vector)
{
    std::cout << ' ' << vector.x << ' ' << vector.y << ' ' << vector.z;
}
} // namespace

int main()
{
    std::int64_t mode = 0;
    while (std::cin >> mode) {
        const bool use_field_camera = Read() != 0;
        const auto backdrop = static_cast<std::uint8_t>(Read());
        std::array<std::uint8_t, 2> instruction { 0xE0,
            static_cast<std::uint8_t>(Read()) };
        ev::EventCameraProgress progress;
        progress.look_at_mode = static_cast<std::uint8_t>(Read());
        progress.eye_mode = static_cast<std::uint8_t>(Read());
        progress.camera_roll_duration = static_cast<std::int16_t>(Read());
        progress.projection_duration = static_cast<std::uint8_t>(Read());
        progress.near_clip_duration = static_cast<std::uint8_t>(Read());
        progress.far_clip_duration = static_cast<std::uint8_t>(Read());
        progress.screen_effect_duration = static_cast<std::int16_t>(Read());
        for (auto& shake : progress.shake_durations) {
            shake = static_cast<std::uint8_t>(Read());
        }
        ev::EventCameraFrame frame;
        frame.look_at = ReadVec();
        frame.eye = ReadVec();
        ev::EventCameraPair partners { .look_at = ReadVec(), .eye = ReadVec() };
        frame.origin_actor = static_cast<std::uint16_t>(Read());
        frame.origin_offset = ReadVec();
        frame.shake_active_count = static_cast<std::int16_t>(Read());
        frame.look_at_shake = ReadPosition();
        frame.eye_shake = ReadPosition();
        for (auto* setting : { &frame.camera_roll, &frame.screen_roll,
                 &frame.projection_distance, &frame.near_clip, &frame.far_clip }) {
            setting->changed = static_cast<std::uint8_t>(Read());
            setting->value = static_cast<std::int16_t>(Read());
        }
        ev::EventCameraPair field { .look_at = ReadVec(), .eye = ReadVec() };
        const auto yaw = static_cast<std::int32_t>(Read());
        Mat3 matrix;
        for (auto& row : matrix.m) {
            for (auto& element : row) {
                element = static_cast<std::int16_t>(Read());
            }
        }
        std::array<std::int64_t, 3> salt { Read(), Read(), Read() };

        Calls calls;
        std::int64_t result = 0;
        switch (mode) {
        case kWait:
            result = ev::CommandWaitCameraTracks(ev::ScriptCommand { instruction },
                ev::EventCameraCompletionMask(progress))
                         .value_or(-999);
            break;
        case kApply: {
            RecordingOutput output(calls, field, yaw, matrix, salt);
            ev::ApplyEventCameraState(frame, use_field_camera, backdrop, output);
            break;
        }
        case kUpdate: {
            RecordingSteps steps(
                calls, { .look_at = frame.look_at, .eye = frame.eye }, partners);
            ev::UpdateEventCameraTracks(steps);
            break;
        }
        case kMask:
            result = ev::EventCameraCompletionMask(progress);
            break;
        default:
            break;
        }

        std::cout << result;
        Print(frame.look_at);
        Print(frame.eye);
        Print(partners.look_at);
        Print(partners.eye);
        for (const auto* setting : { &frame.camera_roll, &frame.screen_roll,
                 &frame.projection_distance, &frame.near_clip, &frame.far_clip }) {
            std::cout << ' ' << int { setting->changed };
        }
        std::cout << ' ' << calls.size();
        for (const auto& call : calls) {
            for (const std::int64_t item : call) {
                std::cout << ' ' << item;
            }
        }
        std::cout << '\n';
    }
    return 0;
}
