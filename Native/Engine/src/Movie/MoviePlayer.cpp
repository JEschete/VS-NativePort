#include "Movie/MoviePlayer.h"

#include "Audio/AudioDevice.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Render/PageSpace.hpp"

#include <dav1d/dav1d.h>
#include <SDL3/SDL.h>
#include <mkvparser/mkvparser.h>
#include <mkvparser/mkvreader.h>

#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Plays a movie VS.Extract converted (Matroska: AV1 video, FLAC audio). Both streams are read into memory at open
// (the title movie is about 10 MB): the soundtrack is decoded at once and queued on its own audio source, so it
// runs on in real time while the game decides what to show; the picture is decoded frame by frame as the game asks
// for the frame due at a tick and written as 24-bit pixels into the page space, where the MDEC's output went.

namespace {

using namespace VS;

struct Packet {
    long long timeNs = 0;
    std::vector<u8> bytes;
};

class Player {
public:
    ~Player() { Close(); }

    bool Open(const std::string& file)
    {
        mkvparser::MkvReader reader;
        if (reader.Open(file.c_str()) != 0) {
            VS_LOG_ERROR("movie", "cannot open {}", file);
            return false;
        }
        long long pos = 0;
        mkvparser::EBMLHeader ebml;
        if (ebml.Parse(&reader, pos) < 0) {
            VS_LOG_ERROR("movie", "{}: not a Matroska file", file);
            return false;
        }
        mkvparser::Segment* raw = nullptr;
        if (mkvparser::Segment::CreateInstance(&reader, pos, raw) != 0 || raw == nullptr) {
            return false;
        }
        std::unique_ptr<mkvparser::Segment> segment(raw);
        if (segment->Load() < 0) {
            return false;
        }
        long videoTrack = -1, audioTrack = -1;
        std::vector<u8> flac;
        const mkvparser::Tracks* tracks = segment->GetTracks();
        for (unsigned long i = 0; i < tracks->GetTracksCount(); ++i) {
            const mkvparser::Track* t = tracks->GetTrackByIndex(i);
            if (t == nullptr) {
                continue;
            }
            const std::string codec = t->GetCodecId() ? t->GetCodecId() : "";
            if (t->GetType() == mkvparser::Track::kVideo && codec == "V_AV1") {
                videoTrack = t->GetNumber();
                const auto* v = static_cast<const mkvparser::VideoTrack*>(t);
                m_width = static_cast<i32>(v->GetWidth());
                m_height = static_cast<i32>(v->GetHeight());
            } else if (t->GetType() == mkvparser::Track::kAudio && codec == "A_FLAC") {
                audioTrack = t->GetNumber();
                const auto* a = static_cast<const mkvparser::AudioTrack*>(t);
                m_sampleRate = static_cast<i32>(a->GetSamplingRate());
                m_channels = static_cast<i32>(a->GetChannels());
                size_t size = 0;
                const unsigned char* priv = t->GetCodecPrivate(size);
                flac.assign(priv, priv + size);  // "fLaC" and the metadata blocks
            }
        }
        if (videoTrack < 0) {
            VS_LOG_ERROR("movie", "{}: no AV1 video track", file);
            return false;
        }
        for (const mkvparser::Cluster* c = segment->GetFirst(); c != nullptr && !c->EOS(); c = segment->GetNext(c)) {
            const mkvparser::BlockEntry* be = nullptr;
            if (c->GetFirst(be) < 0) {
                break;
            }
            while (be != nullptr && !be->EOS()) {
                const mkvparser::Block* b = be->GetBlock();
                for (int k = 0; k < b->GetFrameCount(); ++k) {
                    const mkvparser::Block::Frame& f = b->GetFrame(k);
                    std::vector<u8> bytes(static_cast<usize>(f.len));
                    if (f.Read(&reader, bytes.data()) != 0) {
                        continue;
                    }
                    if (b->GetTrackNumber() == videoTrack) {
                        m_video.push_back({b->GetTime(c), std::move(bytes)});
                    } else if (b->GetTrackNumber() == audioTrack) {
                        flac.insert(flac.end(), bytes.begin(), bytes.end());
                    }
                }
                if (c->GetNext(be, be) < 0) {
                    break;
                }
            }
        }
        if (m_video.empty()) {
            return false;
        }
        Dav1dSettings settings;
        dav1d_default_settings(&settings);
        settings.n_threads = 2;
        settings.max_frame_delay = 1;
        if (dav1d_open(&m_decoder, &settings) != 0) {
            VS_LOG_ERROR("movie", "dav1d_open failed");
            return false;
        }
        m_rgb.assign(static_cast<usize>(m_width) * m_height * 3, 0);
        StartSoundtrack(flac);
        VS_LOG_INFO("movie", "{}: {}x{}, {} frames, {:.1f} s", file, m_width, m_height, m_video.size(),
                    static_cast<double>(m_video.back().timeNs) / 1e9);
        return true;
    }

    void Close()
    {
        if (m_decoder != nullptr) {
            dav1d_data_unref(&m_pending);
            dav1d_close(&m_decoder);
        }
        Audio::AudioDevice::CloseSource(m_audio);
        m_audio = nullptr;
    }

    // Decodes up to the frame due at `timeNs`. False once past the last frame.
    bool Advance(long long timeNs)
    {
        while (m_shown + 1 < static_cast<long long>(m_video.size()) && m_video[m_shown + 1].timeNs <= timeNs) {
            if (!DecodeNext()) {
                return false;
            }
            ++m_shown;
        }
        const long long frameNs = m_video.size() > 1 ? m_video[1].timeNs - m_video[0].timeNs : 66'666'667;
        return timeNs < m_video.back().timeNs + frameNs;
    }

    void Draw(i32 x, i32 y) const
    {
        // 24-bit pixels are stored as consecutive R, G, B bytes: a row of w pixels covers w * 3 / 2 halfwords
        Render::GamePages().Upload({x, y, m_width * 3 / 2, m_height}, reinterpret_cast<const u16*>(m_rgb.data()));
    }

private:
    bool DecodeNext()
    {
        Dav1dPicture pic{};
        while (true) {
            if (m_pending.sz == 0 && m_sent < m_video.size()) {
                const Packet& p = m_video[m_sent++];
                u8* dst = dav1d_data_create(&m_pending, p.bytes.size());
                std::memcpy(dst, p.bytes.data(), p.bytes.size());
            }
            if (m_pending.sz != 0) {
                const int r = dav1d_send_data(m_decoder, &m_pending);
                if (r < 0 && r != DAV1D_ERR(EAGAIN)) {
                    VS_LOG_ERROR("movie", "dav1d_send_data: {}", r);
                    return false;
                }
            }
            const int r = dav1d_get_picture(m_decoder, &pic);
            if (r == 0) {
                break;
            }
            if (r != DAV1D_ERR(EAGAIN) || (m_pending.sz == 0 && m_sent >= m_video.size())) {
                return false;
            }
        }
        ToRgb(pic);
        dav1d_picture_unref(&pic);
        return true;
    }

    // YCbCr 4:2:0 to RGB; the MDEC's output range is full (the conversion keeps it), BT.601 coefficients.
    void ToRgb(const Dav1dPicture& pic)
    {
        const bool full = pic.seq_hdr == nullptr || pic.seq_hdr->color_range != 0;
        const auto* py = static_cast<const u8*>(pic.data[0]);
        const auto* pu = static_cast<const u8*>(pic.data[1]);
        const auto* pv = static_cast<const u8*>(pic.data[2]);
        const i32 w = std::min(m_width, pic.p.w), h = std::min(m_height, pic.p.h);
        for (i32 row = 0; row < h; ++row) {
            u8* out = m_rgb.data() + static_cast<usize>(row) * m_width * 3;
            for (i32 col = 0; col < w; ++col) {
                float Y = py[row * pic.stride[0] + col];
                const float U = pu[(row / 2) * pic.stride[1] + col / 2] - 128.0f;
                const float V = pv[(row / 2) * pic.stride[1] + col / 2] - 128.0f;
                float s = 1.0f;
                if (!full) {
                    Y = (Y - 16.0f) * (255.0f / 219.0f);
                    s = 255.0f / 224.0f;
                }
                const auto clamp = [](float v) { return static_cast<u8>(std::clamp(v + 0.5f, 0.0f, 255.0f)); };
                out[col * 3 + 0] = clamp(Y + 1.402f * s * V);
                out[col * 3 + 1] = clamp(Y - 0.344136f * s * U - 0.714136f * s * V);
                out[col * 3 + 2] = clamp(Y + 1.772f * s * U);
            }
        }
    }

    void StartSoundtrack(const std::vector<u8>& flac)
    {
        Audio::AudioDevice* device = Audio::AudioDevice::Primary();
        if (device == nullptr || flac.size() < 4 || m_sampleRate <= 0) {
            VS_LOG_WARN("movie", "soundtrack not played (device {}, {} bytes of FLAC, {} Hz)", device != nullptr,
                        flac.size(), m_sampleRate);
            return;
        }
        unsigned int channels = 0, rate = 0;
        drflac_uint64 frames = 0;
        drflac_int16* pcm = drflac_open_memory_and_read_pcm_frames_s16(flac.data(), flac.size(), &channels, &rate,
                                                                       &frames, nullptr);
        if (pcm == nullptr) {
            VS_LOG_ERROR("movie", "soundtrack: FLAC decode failed");
            return;
        }
        m_audio = device->OpenSource(static_cast<i32>(rate), static_cast<i32>(channels));
        if (m_audio != nullptr) {
            SDL_PutAudioStreamData(m_audio, pcm, static_cast<int>(frames * channels * sizeof(drflac_int16)));
            VS_LOG_INFO("movie", "soundtrack: {:.1f} s, {} Hz, {} ch", static_cast<double>(frames) / rate, rate,
                        channels);
        }
        drflac_free(pcm, nullptr);
    }

    std::vector<Packet> m_video;
    std::vector<u8> m_rgb;
    Dav1dContext* m_decoder = nullptr;
    Dav1dData m_pending{};
    usize m_sent = 0;
    long long m_shown = -1;
    i32 m_width = 0, m_height = 0, m_sampleRate = 0, m_channels = 0;
    SDL_AudioStream* m_audio = nullptr;
};

std::unique_ptr<Player> g_player;

} // namespace

extern "C" int Movie_PlayerOpen(const char* discPath)
{
    g_player.reset();
    const Content::DataStore* store = Content::DataStore::Get();
    const Content::DataStore::Entry* e = store ? store->Find(discPath) : nullptr;
    if (e == nullptr || e->form == "raw" || e->form == "stream") {
        VS_LOG_WARN("movie", "{}: no converted movie (run VS.Extract extract with FFmpeg available)", discPath);
        return 0;
    }
    auto player = std::make_unique<Player>();
    if (!player->Open(store->FileOf(*e).string())) {
        return 0;
    }
    g_player = std::move(player);
    return 1;
}

extern "C" int Movie_PlayerDrawFrame(int ticks, int x, int y)
{
    if (!g_player) {
        return 0;
    }
    const bool more = g_player->Advance(static_cast<long long>(ticks) * 1'000'000'000LL / 60);
    g_player->Draw(x, y);
    return more ? 1 : 0;
}

extern "C" void Movie_PlayerClose(void)
{
    g_player.reset();
}
