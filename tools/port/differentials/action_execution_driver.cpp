// Native side of action_execution.py. Movement shares the preceding group's
// fixture; execution snapshots include every state, candidate and callback write.
#define VS_MOVEMENT_DRIVER_INCLUDED
#include "candidate_movement_driver.cpp"
#include "Battle/Ai/ActionExecution.hpp"

namespace {
constexpr int kStates = 4, kFields = 23, kEntries = 5, kEntryWords = 25;
constexpr int kStateFields = kFields + kEntryWords;
constexpr int kGlobal = kStates * kStateFields;
constexpr int kNext = kGlobal + 7;
constexpr int kSnapshot = kNext + 4;
enum ExecutionCall { eCan, eEngage, eReady, eRandom, eCommand, eCooldown, eElapsed, eDispatch, eLocomotion, eCount };

struct ExecutionData {
    std::array<long long, kSnapshot> fields{};
    std::array<bool, 16> moving{};
    std::array<int, 4> specials{};
    std::array<bool, 16> gimmicks{};
    DecisionPlayer player;
    VS::Core::Math::Position3 battle_position;
    std::array<std::uint16_t, 1024> terrain{};
    std::array<std::vector<long long>, eCount> answers;
    std::array<int, eCount> occurrences{};
    struct Write { int call, occurrence, field; long long value; };
    std::vector<Write> writes;
    std::vector<std::vector<long long>> calls;
    int current = 1;
    long long& Field(int actor, int field) { return fields.at(actor * kStateFields + field); }
    long long Field(int actor, int field) const { return fields.at(actor * kStateFields + field); }
    long long Note(int code, long long argument = 0) {
        std::vector<long long> call{code, current, argument, 0};
        call.insert(call.end(), fields.begin(), fields.end());
        calls.push_back(std::move(call));
        const int at = occurrences.at(code)++;
        for (const auto& write : writes)
            if (write.call == code && write.occurrence == at)
                fields.at(write.field) = write.value;
        return at < static_cast<int>(answers.at(code).size()) ? answers.at(code).at(at) : 0;
    }
};

class ExecutionWorld final : public ActionExecutionWorld {
public:
    explicit ExecutionWorld(ExecutionData& data) : d(data) {}
    int CandidateCount() const override { return static_cast<int>(d.Field(d.current, 21)); }
    std::optional<std::size_t> RankedCandidate(std::size_t rank) const override {
        return rank < 4 ? std::optional(rank) : std::nullopt;
    }
    std::size_t FallbackCandidate() const override { return 4; }
    std::optional<ActionCandidate> Candidate(std::size_t handle) const override {
        if (handle >= kEntries) return std::nullopt;
        ActionCandidate::Words words;
        for (int i = 0; i < 5; ++i) words.at(i) = static_cast<std::uint32_t>(d.Field(d.current, kFields + static_cast<int>(handle) * 5 + i));
        return ActionCandidate(words);
    }
    void StoreCandidate(std::size_t handle, const ActionCandidate& candidate) override {
        for (int i = 0; i < 5; ++i) d.Field(d.current, kFields + static_cast<int>(handle) * 5 + i) = candidate.RawWords().at(i);
    }
    bool CanExecute(std::size_t handle) override { return d.Note(eCan, handle) != 0; }
    void TryEngage() override { d.Note(eEngage); }
    bool EngagementReady() override { return d.Note(eReady) != 0; }
    void SelectCandidate(std::size_t handle) override { d.Field(d.current, 22) = handle; d.fields.at(kGlobal) = 1; }
    bool TargetMoving(unsigned target) const override { return d.moving.at(target); }
    int RandomMasked(int mask) override { return static_cast<int>(d.Note(eRandom, mask)); }
    void IssueCommand(int command) override { d.Note(eCommand, command); }
    bool RequiresCooldown(std::size_t handle) override { return d.Note(eCooldown, handle) != 0; }
    bool UsesRoomDelay() const override { return d.Field(d.current, 5) == 0; }
    std::optional<std::uint8_t> RoomDelay() const override { return static_cast<std::uint8_t>(d.fields.at(kGlobal + 6)); }
    void SetDelay(std::uint8_t delay) override { d.Field(d.current, 16) = delay; }
    bool HasRoom() const override { return true; }
    std::optional<int> SpecialActor(unsigned slot) const override { return d.specials.at(slot) < 0 ? std::nullopt : std::optional(d.specials.at(slot)); }
    std::optional<bool> GimmickReady(int actor) const override { return d.gimmicks.at(actor); }
private:
    ExecutionData& d;
};

class UpdateWorld final : public DecisionUpdateWorld {
public:
    explicit UpdateWorld(ExecutionData& data) : d(data) {}
    std::size_t ActorHead() const override { return 1; }
    std::optional<int> ActorId(std::size_t node) const override { return static_cast<int>(node - 1); }
    std::size_t ActorNext(std::size_t node) const override { return static_cast<std::size_t>(d.fields.at(kNext + node - 1)); }
    bool SelectActor(int actor) override { d.current = actor; return true; }
    DecisionDispatchState State() const override {
        return {.restricted = d.Field(d.current, 6) != 0, .partial = d.Field(d.current, 7) != 0,
            .tile_x = static_cast<unsigned>(d.Field(d.current, 8)), .tile_z = static_cast<unsigned>(d.Field(d.current, 9)),
            .position = {static_cast<std::int16_t>(d.Field(d.current, 11)), static_cast<std::int16_t>(d.Field(d.current, 12)), static_cast<std::int16_t>(d.Field(d.current, 13))},
            .point_height = static_cast<std::int16_t>(d.Field(d.current, 14))};
    }
    int ElapsedTicks() override { return static_cast<int>(d.Note(eElapsed)); }
    int TickBudget() const override { return static_cast<int>(d.fields.at(kGlobal + 5)); }
    void CacheTick(int tick) override { d.fields.at(kGlobal + 1) = tick; }
    void IssueCommand(int command) override { d.Note(eCommand, command); }
    std::optional<DecisionPlayer> Player() const override { return d.player; }
    std::optional<VS::Core::Math::Position3> PlayerBattlePosition() const override { return d.battle_position; }
    void StorePlayerDistance(std::uint32_t value) override { d.Field(d.current, 20) = value; }
    std::optional<int> ExecutePlannedActions() override { ExecutionWorld world(d); return TryExecutePlannedActions(world); }
    std::optional<std::uint16_t> TileFlags(unsigned x, unsigned z) const override { return d.terrain.at(z * 32 + x); }
    void ResetMovement(std::uint8_t area) override {
        for (int i = 0; i < 4; ++i) d.Field(d.current, i) = 0;
        d.Field(d.current, 10) = 0xFFFFFFFF;
        d.Field(d.current, 17) = area; d.Field(d.current, 18) = area;
        d.Field(d.current, 19) = 16;
    }
    void ClearFirstDirectionWord() override { d.fields.at(kGlobal + 2) = 0; }
    void DispatchMovement() override { d.Note(eDispatch); }
    void UpdateLocomotion() override { d.Note(eLocomotion); }
private:
    ExecutionData& d;
};

void RunExecution(std::istringstream& in, int kind) {
    ExecutionData d;
    for (auto& field : d.fields) field = Read<long long>(in);
    for (auto& moving : d.moving) moving = Read<int>(in) != 0;
    for (auto& actor : d.specials) actor = Read<int>(in);
    for (auto& active : d.gimmicks) active = Read<int>(in) != 0;
    d.player = {.has_ai = Read<int>(in) != 0, .moving = Read<int>(in) != 0,
        .y = Read<std::int16_t>(in), .point_height = Read<std::int16_t>(in), .body_height = Read<std::int16_t>(in)};
    d.battle_position = {Read<std::int16_t>(in), Read<std::int16_t>(in), Read<std::int16_t>(in)};
    for (auto& tile : d.terrain) tile = Read<std::uint16_t>(in);
    for (auto& queue : d.answers) queue = ReadQueue(in);
    d.writes.resize(Read<std::size_t>(in));
    for (auto& write : d.writes) write = {Read<int>(in), Read<int>(in), Read<int>(in), Read<long long>(in)};
    ExecutionWorld actions(d);
    UpdateWorld update(d);
    std::optional<int> result;
    if (kind == 1) result = TryExecuteActionCandidate(actions, 0);
    else if (kind == 2) result = TryExecutePlannedActions(actions);
    else result = UpdateActorDecisions(update) ? std::optional(0) : std::nullopt;
    std::cout << (!result ? 1 : 0) << ' ' << result.value_or(0);
    for (auto field : d.fields) std::cout << ' ' << field;
    std::cout << ' ' << d.calls.size();
    for (const auto& call : d.calls) for (auto field : call) std::cout << ' ' << field;
    std::cout << '\n';
}
} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const int kind = Read<int>(in);
        if (kind == 0) RunMovementLine(in);
        else RunExecution(in, kind);
    }
}
