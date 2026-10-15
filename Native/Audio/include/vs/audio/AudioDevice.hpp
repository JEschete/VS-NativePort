#pragma once

#include "vs/core/Types.hpp"

#include <expected>
#include <span>
#include <string>

struct SDL_AudioStream;

namespace VS::Audio {

// Output device. The SPU-semantics mixer (voices, ADSR, reverb) and the XA streamer
// feed interleaved stereo s16 frames here (plan §6.3).
class AudioDevice {
public:
    static constexpr i32 SampleRate = 44100;
    static constexpr i32 Channels = 2;

    AudioDevice() = default;
    ~AudioDevice();
    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    std::expected<void, std::string> Open();
    void Close();

    void Submit(std::span<const i16> interleavedFrames);
    i32 QueuedBytes() const;
    bool IsOpen() const { return m_stream != nullptr; }

private:
    SDL_AudioStream* m_stream = nullptr;
};

} // namespace VS::Audio
