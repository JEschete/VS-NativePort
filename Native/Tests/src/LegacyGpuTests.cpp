#include "Legacy/Render/LibGpu.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

namespace {

struct Packet {
    u_long tag;
    u_long body[3];
};

std::vector<const void*> Walk(u_long* head)
{
    std::vector<const void*> out;
    for (void* p = head; p != nullptr && out.size() < 64; p = isendprim(p) ? nullptr : nextPrim(p)) {
        out.push_back(p);
    }
    return out;
}

} // namespace

TEST_CASE("Ordering-table links survive 64-bit addresses", "[legacy][gpu]")
{
    // Two packets exactly 16 MiB apart share their low 24 address bits: a raw 24-bit link cannot tell them apart.
    std::vector<unsigned char> memory((16u << 20) + 2 * sizeof(Packet));
    auto* a = reinterpret_cast<Packet*>(memory.data());
    auto* b = reinterpret_cast<Packet*>(memory.data() + (16u << 20));
    std::memset(a, 0, sizeof *a);
    std::memset(b, 0, sizeof *b);
    setlen(a, 3);
    setlen(b, 3);

    u_long ot[4];
    ClearOTagR(ot, 4);
    addPrim(&ot[2], a);
    addPrim(&ot[1], b);

    // reverse table: drawing starts at the last entry and follows each link down to entry 0
    const auto order = Walk(&ot[3]);
    REQUIRE(order.size() == 6);
    CHECK(order[0] == &ot[3]);
    CHECK(order[1] == &ot[2]);
    CHECK(order[2] == a);
    CHECK(order[3] == &ot[1]);
    CHECK(order[4] == b);
    CHECK(order[5] == &ot[0]);
    CHECK(LegacyGpu_Handle(a) != LegacyGpu_Handle(b));
    CHECK(LegacyGpu_WalkList(&ot[3]) == 6);
}

TEST_CASE("setaddr accepts a packet pointer or a copied link word", "[legacy][gpu]")
{
    Packet p{}, q{};
    setaddr(&p, &q);                // pointer: registered as a handle
    setaddr(&q, getaddr(&p) | 0x05000000);  // link word (length bits included) copied from another packet
    CHECK(nextPrim(&p) == &q);
    CHECK(getaddr(&q) == getaddr(&p));
    termPrim(&q);
    CHECK(isendprim(&q));
    CHECK(nextPrim(&q) == nullptr);
}
