#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorRecord.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Actors/ShpModel.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.hpp"

namespace {
struct AttachmentFixture {
  decltype(VS::Actors::actorRecords) saved = VS::Actors::actorRecords;
  VS::Core::LogLevel console =
      VS::Core::SetConsoleLogLevel(VS::Core::LogLevel::Off);
  VS::Actors::ActorRecord parent{};
  VS::Actors::ActorRecord child{};
  VS::Actors::ShpModel model{};

  AttachmentFixture() {
    std::fill(std::begin(VS::Actors::actorRecords),
              std::end(VS::Actors::actorRecords), nullptr);
    VS::Actors::actorRecords[2] = &parent;
    VS::Actors::actorRecords[7] = &child;
    parent.instance.slot = 2;
    parent.instance.model = &model;
    parent.instance.bone_count = 24;
    child.instance.slot = 7;
    child.instance.is_attached = 1;
    child.instance.attach_parent = 2;
    child.instance.attach_bone = 2;
    parent.bone_matrices[2].m[0][0] = 1234;
    parent.bone_matrices[2].t[0] = 3456;
  }

  ~AttachmentFixture() {
    std::copy(saved.begin(), saved.end(), std::begin(VS::Actors::actorRecords));
    VS::Core::SetConsoleLogLevel(console);
  }
};
}  // namespace

TEST_CASE("Attached props borrow the actual resident parent bone matrix",
          "[actor-attachment]") {
  AttachmentFixture fixture;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        &fixture.parent.bone_matrices[2]);
  CHECK(fixture.parent.bone_matrices[2].m[0][0] == 1234);
  CHECK(fixture.parent.bone_matrices[2].t[0] == 3456);
  fixture.child.instance.attach_bone = 0;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        &fixture.parent.bone_matrices[0]);
}

TEST_CASE("Scene prop survives parent release without a fabricated camera",
          "[actor-attachment]") {
  AttachmentFixture fixture;
  std::array<unsigned char, sizeof(VS::Actors::ActorRecord)> before{};
  std::array<unsigned char, sizeof(VS::Actors::ActorRecord)> after{};
  std::memcpy(before.data(), &fixture.child, before.size());
  REQUIRE(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) !=
          nullptr);
  VS::Actors::actorRecords[2] = nullptr;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        nullptr);
  std::memcpy(after.data(), &fixture.child, after.size());
  CHECK(before == after);
  CHECK(VS::Actors::actorRecords[7] == &fixture.child);

  VS::Actors::ActorRecord replacement{};
  replacement.instance.slot = 2;
  replacement.instance.model = &fixture.model;
  replacement.instance.bone_count = 3;
  replacement.bone_matrices[2].t[0] = -9876;
  VS::Actors::actorRecords[2] = &replacement;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        &replacement.bone_matrices[2]);
  CHECK(replacement.bone_matrices[2].t[0] == -9876);
}

TEST_CASE("Attachments defer until a parent model and pose become resident",
          "[actor-attachment]") {
  AttachmentFixture fixture;
  fixture.parent.instance.model = nullptr;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        nullptr);
  fixture.parent.instance.model = &fixture.model;
  fixture.parent.instance.bone_count = 0;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        nullptr);
  fixture.parent.instance.bone_count = 3;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        &fixture.parent.bone_matrices[2]);
}

TEST_CASE("Attachments reject invalid parent slots and bone bounds",
          "[actor-attachment]") {
  AttachmentFixture fixture;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(nullptr) == nullptr);
  fixture.child.instance.attach_parent =
      static_cast<std::uint8_t>(std::size(VS::Actors::actorRecords));
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        nullptr);
  fixture.child.instance.attach_parent = 2;
  fixture.child.instance.attach_bone = fixture.parent.instance.bone_count;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        nullptr);
  fixture.child.instance.attach_bone = 2;
  fixture.parent.instance.bone_count =
      static_cast<std::uint8_t>(std::size(fixture.parent.bone_matrices) + 1);
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        nullptr);
  fixture.parent.instance.bone_count = 3;
  CHECK(VS::Actors::ResolveLiveAttachmentParentMatrix(&fixture.child) ==
        &fixture.parent.bone_matrices[2]);
}
