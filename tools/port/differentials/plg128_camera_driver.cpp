// Native side of tools\port\differentials\plg128_camera.py (F02064-F02068):
// one vector per stdin line, one line of integers per vector. The field order
// is the case's INPUT/OUTPUT order; see the case for the layout.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Effects/Camera.hpp"
#include "Effects/Plugins/Plg128Camera.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }

namespace {
namespace p = VS::Effects::Plg128;
namespace m = VS::Core::Math;
using VS::Effects::EffectCameraPose;

class Reader {
public:
    explicit Reader(const std::string& line)
        : in_(line)
    {
    }
    std::int64_t Next()
    {
        std::int64_t value = 0;
        if (!(in_ >> value))
            std::abort();
        return value;
    }
    std::int32_t Int() { return static_cast<std::int32_t>(Next()); }
    std::int16_t Short() { return static_cast<std::int16_t>(Next()); }
    std::uint8_t Byte() { return static_cast<std::uint8_t>(Next()); }
    m::Vec3 Vec()
    {
        const std::int32_t x = Int(), y = Int(), z = Int();
        return { x, y, z };
    }
    m::Position3 Pos()
    {
        const std::int16_t x = Short(), y = Short(), z = Short();
        return { x, y, z };
    }
    p::VectorRange Range()
    {
        p::VectorRange range;
        for (auto& span : range) {
            const std::int32_t start = Short(), end = Short();
            span = { start, end };
        }
        return range;
    }
    EffectCameraPose Pose()
    {
        EffectCameraPose pose;
        pose.position = Vec();
        pose.look_at = Vec();
        pose.shake_lateral = Int();
        pose.shake_vertical = Int();
        pose.shake_depth = Int();
        pose.shake_roll = Int();
        pose.roll = Int();
        pose.near_clip = Int();
        pose.projection_distance = Int();
        pose.far_clip = Int();
        return pose;
    }

private:
    std::istringstream in_;
};

class Writer {
public:
    void Int(std::int64_t value) { values_.push_back(value); }
    void Vec(const m::Vec3& value)
    {
        Int(value.x);
        Int(value.y);
        Int(value.z);
    }
    void Pos(const m::Position3& value)
    {
        Int(value.x);
        Int(value.y);
        Int(value.z);
    }
    void Pose(const EffectCameraPose& pose)
    {
        Vec(pose.position);
        Vec(pose.look_at);
        for (std::int32_t value :
            { pose.shake_lateral, pose.shake_vertical, pose.shake_depth, pose.shake_roll,
                pose.roll, pose.near_clip, pose.projection_distance, pose.far_clip }) {
            Int(value);
        }
    }
    void Print() const
    {
        std::string line;
        for (std::int64_t value : values_) {
            line += std::to_string(value);
            line += ' ';
        }
        std::puts(line.c_str());
    }

private:
    std::vector<std::int64_t> values_;
};

struct Capture {
    m::Vec3 position, look_at;
    std::int32_t roll, near_clip, projection, far_clip;
};

class Host final : public p::CameraEffectHost {
public:
    p::CameraDefinition definition;
    m::Position3 origin;
    std::uint8_t transform_index = 0, attraction_index = 0;
    m::Vec3 transform_translation, attraction_translation;
    std::int16_t facing = 0;
    m::Mat3 look_at;
    std::array<Capture, 2> captures { };
    std::array<std::int32_t, 7> samples { };
    std::int16_t motion_frame = 0, motion_duration = 0;
    std::array<EffectCameraPose, 2> ends { };

    std::vector<std::array<std::int32_t, 6>> look_ats;
    std::vector<std::int32_t> transitions;
    std::vector<std::array<std::int32_t, 2>> curves;
    int captured = 0;

    std::optional<p::CameraDefinition> Definition(std::uint8_t) override
    {
        return definition;
    }
    std::int32_t SampleCurve(std::uint8_t curve, std::int32_t age) override
    {
        if (curves.size() >= samples.size())
            std::abort();
        const std::int32_t value = samples[curves.size()];
        curves.push_back({ curve, age });
        return value;
    }
    m::Position3 EffectOrigin() override { return origin; }
    std::optional<m::Vec3> TransformTranslation(std::uint8_t index) override
    {
        if (index == attraction_index)
            return attraction_translation;
        if (index == transform_index)
            return transform_translation;
        std::abort();
    }
    std::optional<std::int16_t> TransformFacing(std::uint8_t index) override
    {
        if (index != transform_index)
            std::abort();
        return facing;
    }
    m::Mat3 LookAt(m::Vec3 from, m::Vec3 to) override
    {
        look_ats.push_back({ from.x, from.y, from.z, to.x, to.y, to.z });
        return look_at;
    }
    void CaptureCamera(EffectCameraPose& pose) override
    {
        if (captured >= 2)
            std::abort();
        const Capture& capture = captures[captured++];
        pose.position = capture.position;
        pose.look_at = capture.look_at;
        pose.roll = capture.roll;
        pose.near_clip = capture.near_clip;
        pose.projection_distance = capture.projection;
        pose.far_clip = capture.far_clip;
    }
    void BeginCameraMotion(std::int16_t duration) override
    {
        motion_frame = 0;
        motion_duration = duration;
    }
    void SetTransitionMode(std::int32_t mode) override { transitions.push_back(mode); }
    void PublishCameraMotionEnds(
        const EffectCameraPose& start, const EffectCameraPose& end) override
    {
        ends = { start, end };
    }
};

p::CameraEffectState ReadState(Reader& in)
{
    p::CameraEffectState state;
    state.lifetime = in.Byte();
    state.definition = in.Byte();
    state.mode = in.Short();
    state.age = in.Int();
    state.start_camera = in.Pose();
    state.end_camera = in.Pose();
    state.position = in.Vec();
    state.target = in.Vec();
    for (auto& row : state.view.rotation.m) {
        for (auto& value : row)
            value = in.Short();
    }
    state.view.translation = in.Vec();
    state.near_clip = in.Int();
    return state;
}

p::CameraScratch ReadScratch(Reader& in)
{
    p::CameraScratch scratch;
    scratch.flags = static_cast<std::uint32_t>(in.Next());
    scratch.vector = in.Pos();
    scratch.position = in.Vec();
    scratch.extent = in.Vec();
    scratch.base_direction = in.Vec();
    scratch.speed = { in.Int(), in.Int() };
    scratch.attraction_offset = in.Pos();
    scratch.attraction_jitter = in.Pos();
    scratch.sin_angle0 = in.Int();
    scratch.sin_angle1 = in.Int();
    scratch.cos_angle0 = in.Int();
    scratch.cos_angle1 = in.Int();
    return scratch;
}

p::CameraDefinition ReadDefinition(Reader& in)
{
    p::CameraDefinition definition;
    definition.flags = static_cast<std::uint32_t>(in.Next()) & p::kDefinitionFlagsMask;
    definition.position_curve = in.Byte();
    definition.direction_curve = in.Byte();
    definition.speed_curve = in.Byte();
    definition.attraction_offset_curve = in.Byte();
    definition.attraction_jitter_curve = in.Byte();
    definition.inner_extent_curve = in.Byte();
    definition.position_range = in.Range();
    definition.extent_range = in.Range();
    definition.direction_range = in.Range();
    for (auto& span : definition.speed_range) {
        const std::int32_t start = in.Short(), end = in.Short();
        span = { start, end };
    }
    definition.attraction_offset_range = in.Range();
    definition.attraction_jitter_range = in.Range();
    const std::int32_t start = in.Short(), end = in.Short();
    definition.inner_extent_range = { start, end };
    definition.transform_index = in.Byte();
    definition.attraction_transform = in.Byte();
    return definition;
}

void Resolve(
    Host& host, p::CameraEffectState& state, p::CameraScratch& scratch, std::int16_t mode)
{
    const auto anchors =
        p::GatherCameraAnchors(host, host.definition, scratch.flags, mode);
    if (!anchors)
        std::abort();
    if (mode == p::kEndpointMode) {
        p::ResolveCameraEndpoints(state, scratch, *anchors);
    } else {
        p::ResolveActorFacingCamera(state, scratch, *anchors);
    }
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        Reader in(line);
        const int kind = in.Int();
        const auto command = static_cast<std::uint32_t>(in.Next());
        const std::int32_t packed = in.Int();
        p::CameraEffectState state = ReadState(in);
        p::CameraScratch scratch = ReadScratch(in);
        Host host;
        host.definition = ReadDefinition(in);
        host.transform_index = host.definition.transform_index;
        host.attraction_index = host.definition.attraction_transform;
        host.origin = in.Pos();
        host.transform_translation = in.Vec();
        host.attraction_translation = in.Vec();
        host.facing = in.Short();
        for (auto& row : host.look_at.m) {
            for (auto& value : row)
                value = in.Short();
        }
        const m::Vec3 translation = in.Vec();
        for (Capture& capture : host.captures) {
            capture.position = in.Vec();
            capture.look_at = in.Vec();
            capture.roll = in.Int();
            capture.near_clip = in.Int();
            capture.projection = in.Int();
            capture.far_clip = in.Int();
        }
        for (auto& sample : host.samples)
            sample = in.Int();
        host.motion_frame = in.Short();
        host.motion_duration = in.Short();
        host.ends = { in.Pose(), in.Pose() };

        std::int32_t result = 0;
        switch (kind) {
        case 0:
            p::BuildCameraOffsetTransform(host, state, translation);
            break;
        case 1:
            Resolve(host, state, scratch, p::kEndpointMode);
            break;
        case 2:
            Resolve(host, state, scratch, p::kActorFacingMode);
            break;
        case 3:
            p::StoreCameraEndpoints(state);
            break;
        default:
            result = p::UpdateEffectCamera(host, state, scratch, command, packed);
            break;
        }

        Writer out;
        out.Int(result);
        out.Int(state.lifetime);
        out.Int(state.definition);
        out.Int(state.mode);
        out.Int(state.age);
        out.Pose(state.start_camera);
        out.Pose(state.end_camera);
        out.Vec(state.position);
        out.Vec(state.target);
        for (const auto& row : state.view.rotation.m) {
            for (std::int16_t value : row)
                out.Int(value);
        }
        out.Vec(state.view.translation);
        out.Int(state.near_clip);
        out.Int(scratch.flags);
        out.Pos(scratch.vector);
        out.Vec(scratch.position);
        out.Vec(scratch.extent);
        out.Vec(scratch.base_direction);
        out.Int(scratch.speed[0]);
        out.Int(scratch.speed[1]);
        out.Pos(scratch.attraction_offset);
        out.Pos(scratch.attraction_jitter);
        out.Int(scratch.sin_angle0);
        out.Int(scratch.sin_angle1);
        out.Int(scratch.cos_angle0);
        out.Int(scratch.cos_angle1);
        out.Int(host.motion_frame);
        out.Int(host.motion_duration);
        out.Pose(host.ends[0]);
        out.Pose(host.ends[1]);
        out.Int(static_cast<std::int64_t>(host.look_ats.size()));
        for (const auto& call : host.look_ats) {
            for (std::int32_t value : call)
                out.Int(value);
        }
        out.Int(static_cast<std::int64_t>(host.transitions.size()));
        for (std::int32_t mode : host.transitions)
            out.Int(mode);
        out.Int(host.captured);
        out.Int(static_cast<std::int64_t>(host.curves.size()));
        for (const auto& call : host.curves) {
            out.Int(call[0]);
            out.Int(call[1]);
        }
        out.Print();
    }
}
