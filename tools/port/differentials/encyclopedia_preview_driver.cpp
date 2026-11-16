// Native side of the encyclopedia preview differentials
// (encyclopedia_preview_common.py, #276). One vector per line: mode, load
// step, ZUD id, the six statics (id read last, disc request, file block,
// model buffer, variant, palette flag), the id's file table entry (sector,
// bytes, palette flag), the disc queue's, high-end and low-end heap's
// replies, the disc request's state, the first free asset request, the
// preview record's address, flags word and animation byte, then the ZUD
// file's length and bytes. Prints the return value, the load step, the
// statics, the record's flags word and animation byte, then the calls.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "Content/ActorAssets.hpp"
#include "Menu/Score/EncyclopediaPreview.hpp"

namespace {
namespace score = VS::Menu::Score;
using VS::Content::ActorAssetRequest;
using VS::Content::AssetAddress;

enum Mode : int {
    kLoad,
    kFree,
    kPose,
    kQueueRead,
    kParts,
    kFreeBuffer,
};
enum CallKind : std::int64_t {
    kCallAllocateRead = 1,
    kCallAllocateFile,
    kCallEnqueue,
    kCallAllocateBuffer,
    kCallReleaseRead,
    kCallQueue,
    kCallUpdate,
    kCallFirstFree,
    kCallFree,
    kCallReleaseModel,
    kCallResetPose,
    kCallPlay,
    kCallPose,
    kCallDisplay,
    kCallCompanion,
    // Calls the original never makes.
    kCallOtherFile = 90,
    kCallOtherSlot,
    kCallRefuse,
    kCallReadRefused,
};

// The original's model buffer parts (592C.c:229-253) and sizes (592C.c:197-199).
constexpr std::uintptr_t kAttachmentOffset = 0x1900;
constexpr std::uintptr_t kShieldOffset = 0x1EE8;
constexpr std::int64_t kCompanionBufferBytes = 0x3200;
constexpr std::int64_t kEquippedBufferBytes = 0x24D0;
constexpr std::uint32_t kWeaponDrawn = 0x00800000;
constexpr std::int64_t kPoseOffset = 0x704;
constexpr int kDiscLoaded = 4;

using Call = std::vector<std::int64_t>;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

std::int64_t Number(AssetAddress address)
{
    return static_cast<std::int64_t>(static_cast<std::uintptr_t>(address));
}

AssetAddress Address(std::int64_t value)
{
    return static_cast<AssetAddress>(static_cast<std::uintptr_t>(value));
}

struct Vector {
    int mode = 0;
    std::uint8_t step = 0;
    int zud_id = 0;
    std::int64_t statics[6] = { };
    score::EnemyPreviewFile file;
    std::int64_t replies[3] = { };
    int disc_state = 0;
    int first_free = 0;
    std::int64_t record = 0;
    std::uint32_t flags = 0;
    std::uint8_t animation = 0;
    std::vector<std::byte> bytes;
};

class DriverHost final : public score::EnemyPreviewHost {
public:
    explicit DriverHost(Vector& vector)
        : vector_(vector)
    {
    }

    std::vector<Call> calls;

    [[nodiscard]] std::uint8_t LoadStep() const override { return vector_.step; }
    void SetLoadStep(std::uint8_t step) override { vector_.step = step; }

    [[nodiscard]] score::EnemyPreviewFile File(int zud_id) const override
    {
        if (zud_id != vector_.zud_id) {
            const_cast<DriverHost*>(this)->calls.push_back({ kCallOtherFile, zud_id });
            return { };
        }
        return vector_.file;
    }

    [[nodiscard]] AssetAddress AllocateRead(const score::EnemyPreviewFile& file) override
    {
        calls.push_back({ kCallAllocateRead, file.sector, file.bytes });
        return Address(vector_.replies[0]);
    }
    void EnqueueRead(AssetAddress read, AssetAddress destination) override
    {
        calls.push_back({ kCallEnqueue, Number(read), Number(destination) });
    }
    [[nodiscard]] AssetAddress ReadRefused(
        const score::EnemyPreviewFile&, AssetAddress) override
    {
        calls.push_back({ kCallReadRefused });
        return AssetAddress::kNone;
    }
    [[nodiscard]] bool ReadLoaded(AssetAddress read) const override
    {
        return Number(read) == vector_.statics[1] && vector_.disc_state == kDiscLoaded;
    }
    void ReleaseRead(AssetAddress read) override
    {
        calls.push_back({ kCallReleaseRead, Number(read) });
    }

    [[nodiscard]] AssetAddress AllocateFile(std::uint32_t bytes) override
    {
        calls.push_back({ kCallAllocateFile, bytes });
        return Address(vector_.replies[1]);
    }
    void FreeFile(AssetAddress file) override
    {
        calls.push_back({ kCallFree, Number(file) });
    }
    [[nodiscard]] AssetAddress AllocateModelBuffer(
        score::EnemyPreviewLayout layout) override
    {
        calls.push_back({ kCallAllocateBuffer,
            layout == score::EnemyPreviewLayout::kCompanion ? kCompanionBufferBytes
                                                            : kEquippedBufferBytes });
        return Address(vector_.replies[2]);
    }
    void FreeModelBuffer(AssetAddress buffer) override
    {
        calls.push_back({ kCallFree, Number(buffer) });
    }
    [[nodiscard]] AssetAddress ModelPart(
        AssetAddress buffer, score::EnemyPreviewPart part) const override
    {
        switch (part) {
        case score::EnemyPreviewPart::kEnemy:
            return buffer;
        case score::EnemyPreviewPart::kCompanion:
        case score::EnemyPreviewPart::kWeapon:
            return VS::Content::AdvanceAddress(buffer, kAttachmentOffset);
        case score::EnemyPreviewPart::kShield:
            return VS::Content::AdvanceAddress(buffer, kShieldOffset);
        }
        return AssetAddress::kNone;
    }
    // The vector's file is the block at the statics' file address.
    [[nodiscard]] std::span<const std::byte> FileBytes(
        AssetAddress file, std::uint32_t bytes) const override
    {
        if (Number(file) != vector_.statics[2] || bytes > vector_.bytes.size()) {
            return { };
        }
        return std::span<const std::byte>(vector_.bytes).first(bytes);
    }

    void QueueAsset(const ActorAssetRequest& request) override
    {
        calls.push_back({ kCallQueue, request.kind, request.target_slot, request.model_id,
            Number(request.destination), Number(request.data), request.placement,
            request.sub_kind, request.texture_slot, request.material, request.variant });
    }
    void UpdateAssetQueue() override { calls.push_back({ kCallUpdate }); }
    [[nodiscard]] int FirstFreeAssetRequest() const override
    {
        const_cast<DriverHost*>(this)->calls.push_back({ kCallFirstFree });
        return vector_.first_free;
    }
    void ReleaseModel(int slot) override { calls.push_back({ kCallReleaseModel, slot }); }

    void ResetPreviewPose() override { calls.push_back({ kCallResetPose }); }
    void SetWeaponDrawn(int slot, bool drawn) override
    {
        if (!Preview(slot))
            return;
        vector_.flags =
            drawn ? (vector_.flags | kWeaponDrawn) : (vector_.flags & ~kWeaponDrawn);
    }
    void SetAnimation(int slot, std::uint8_t animation) override
    {
        if (Preview(slot))
            vector_.animation = animation;
    }
    void PlayAnimation(int slot, int animation, int blend_frames) override
    {
        calls.push_back({ kCallPlay, slot, animation, 0, blend_frames });
    }
    void InitializeAnimationPose(int slot) override
    {
        if (!Preview(slot))
            return;
        calls.push_back({ kCallPose, vector_.record, vector_.record + kPoseOffset });
    }
    void SetDisplayFlags(int slot, int flags) override
    {
        calls.push_back({ kCallDisplay, slot, flags });
    }
    void ClearCompanion(int slot) override
    {
        calls.push_back({ kCallCompanion, slot, 0, 0, 0 });
    }

    void Refuse(std::string_view) override { calls.push_back({ kCallRefuse }); }

private:
    // The vector holds the record of the preview actor slot only.
    bool Preview(int slot)
    {
        if (slot == score::kEnemyPreviewActorSlot)
            return true;
        calls.push_back({ kCallOtherSlot, slot });
        return false;
    }

    Vector& vector_;
};

score::EnemyPreviewState State(const Vector& vector)
{
    score::EnemyPreviewState state;
    state.zud_id = static_cast<int>(vector.statics[0]);
    state.read = Address(vector.statics[1]);
    state.file = Address(vector.statics[2]);
    state.file_bytes = state.file == AssetAddress::kNone
                         ? 0
                         : static_cast<std::uint32_t>(vector.bytes.size());
    state.model_buffer = Address(vector.statics[3]);
    state.variant = static_cast<std::uint8_t>(vector.statics[4]);
    state.palette_flag = static_cast<std::uint8_t>(vector.statics[5]);
    return state;
}

bool ReadVector(Vector& vector)
{
    if (!Read(vector.mode))
        return false;
    std::size_t length = 0;
    bool read = Read(vector.step) && Read(vector.zud_id);
    for (std::int64_t& value : vector.statics)
        read = read && Read(value);
    read = read && Read(vector.file.sector) && Read(vector.file.bytes)
        && Read(vector.file.palette_flag);
    for (std::int64_t& value : vector.replies)
        read = read && Read(value);
    read = read && Read(vector.disc_state) && Read(vector.first_free)
        && Read(vector.record) && Read(vector.flags) && Read(vector.animation)
        && Read(length);
    vector.bytes.resize(read ? length : 0);
    for (std::byte& value : vector.bytes)
        read = read && Read(value);
    return read;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    Vector vector;
    while (ReadVector(vector)) {
        DriverHost host(vector);
        score::EnemyPreviewState state = State(vector);
        int result = 0;
        switch (vector.mode) {
        case kLoad:
            result = score::LoadEnemyPreview(host, state, vector.zud_id);
            break;
        case kFree:
            score::FreeEnemyPreview(host, state);
            break;
        case kPose:
            score::InitializeAshleyPreviewPose(host);
            break;
        case kQueueRead:
            score::QueueEnemyModelRead(host, state, vector.zud_id);
            break;
        case kParts:
            result = score::LoadEnemyModelParts(host, state);
            break;
        case kFreeBuffer:
            score::FreeEnemyModelBuffer(host, state);
            break;
        default:
            return 1;
        }
        std::cout << result << ' ' << static_cast<int>(vector.step) << ' ' << state.zud_id
                  << ' ' << Number(state.read) << ' ' << Number(state.file) << ' '
                  << Number(state.model_buffer) << ' ' << static_cast<int>(state.variant)
                  << ' ' << static_cast<int>(state.palette_flag) << ' ' << vector.flags
                  << ' ' << static_cast<int>(vector.animation) << ' '
                  << host.calls.size();
        for (const Call& call : host.calls) {
            for (const std::int64_t word : call)
                std::cout << ' ' << word;
        }
        std::cout << '\n';
    }
    return 0;
}
