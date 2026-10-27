#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelDrawing.h"
#include "Actors/Skeleton.h"
#include "ModelLoading.h"
}

namespace {
struct AttachmentFixture {
  decltype(std::to_array(Actors_actorRecords)) saved =
      std::to_array(Actors_actorRecords);
  VS::Core::LogLevel console =
      VS::Core::SetConsoleLogLevel(VS::Core::LogLevel::Off);
  Actors_ActorRecord parent{};
  Actors_ActorRecord child{};
  Actors_ShpModel model{};

  AttachmentFixture() {
    std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
              nullptr);
    Actors_actorRecords[2] = &parent;
    Actors_actorRecords[7] = &child;
    parent.instance.slot = 2;
    parent.instance.model = &model;
    parent.instance.boneCount = 24;
    child.instance.slot = 7;
    child.instance.isAttached = 1;
    child.instance.attachParent = 2;
    child.instance.attachBone = 2;
    parent.boneMatrices[2].m[0][0] = 1234;
    parent.boneMatrices[2].t[0] = 3456;
  }

  ~AttachmentFixture() {
    std::copy(saved.begin(), saved.end(), std::begin(Actors_actorRecords));
    VS::Core::SetConsoleLogLevel(console);
  }
};
}  // namespace

TEST_CASE("Attached props borrow the actual resident parent bone matrix",
          "[actor-attachment]") {
  AttachmentFixture fixture;
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) ==
        &fixture.parent.boneMatrices[2]);
  CHECK(fixture.parent.boneMatrices[2].m[0][0] == 1234);
  CHECK(fixture.parent.boneMatrices[2].t[0] == 3456);
  fixture.child.instance.attachBone = 0;
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) ==
        &fixture.parent.boneMatrices[0]);
}

TEST_CASE("Scene prop survives parent release without a fabricated camera",
          "[actor-attachment]") {
  AttachmentFixture fixture;
  std::array<unsigned char, sizeof(Actors_ActorRecord)> before{};
  std::array<unsigned char, sizeof(Actors_ActorRecord)> after{};
  std::memcpy(before.data(), &fixture.child, before.size());
  REQUIRE(Actors_ResolveAttachmentParentMatrix(&fixture.child) != nullptr);
  Actors_actorRecords[2] = nullptr;
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) == nullptr);
  std::memcpy(after.data(), &fixture.child, after.size());
  CHECK(before == after);
  CHECK(Actors_actorRecords[7] == &fixture.child);

  Actors_ActorRecord replacement{};
  replacement.instance.slot = 2;
  replacement.instance.model = &fixture.model;
  replacement.instance.boneCount = 3;
  replacement.boneMatrices[2].t[0] = -9876;
  Actors_actorRecords[2] = &replacement;
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) ==
        &replacement.boneMatrices[2]);
  CHECK(replacement.boneMatrices[2].t[0] == -9876);
}

TEST_CASE("Attachments defer until a parent model and pose become resident",
          "[actor-attachment]") {
  AttachmentFixture fixture;
  fixture.parent.instance.model = nullptr;
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) == nullptr);
  fixture.parent.instance.model = &fixture.model;
  fixture.parent.instance.boneCount = 0;
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) == nullptr);
  fixture.parent.instance.boneCount = 3;
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) ==
        &fixture.parent.boneMatrices[2]);
}

TEST_CASE("Attachments reject invalid parent slots and bone bounds",
          "[actor-attachment]") {
  AttachmentFixture fixture;
  CHECK(Actors_ResolveAttachmentParentMatrix(nullptr) == nullptr);
  fixture.child.instance.attachParent =
      static_cast<u_char>(std::size(Actors_actorRecords));
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) == nullptr);
  fixture.child.instance.attachParent = 2;
  fixture.child.instance.attachBone = fixture.parent.instance.boneCount;
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) == nullptr);
  fixture.child.instance.attachBone = 2;
  fixture.parent.instance.boneCount =
      static_cast<u_char>(std::size(fixture.parent.boneMatrices) + 1);
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) == nullptr);
  fixture.parent.instance.boneCount = 3;
  CHECK(Actors_ResolveAttachmentParentMatrix(&fixture.child) ==
        &fixture.parent.boneMatrices[2]);
}
