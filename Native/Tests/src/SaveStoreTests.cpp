#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Save/SaveStore.hpp"

namespace {
namespace fs = std::filesystem;
using VS::Save::SaveStore;

std::expected<void, std::string> Validate(std::string_view data) {
  if (!data.starts_with("v1:") || data.size() <= 3) {
    return std::unexpected("unsupported or incomplete test snapshot");
  }
  return {};
}

struct TemporaryDirectory {
  fs::path root;

  TemporaryDirectory() {
    static std::atomic<unsigned> sequence{0};
    root = fs::temp_directory_path() /
           std::format("vs-native-save-{}-{}-{}", GetCurrentProcessId(),
                       GetTickCount64(), sequence.fetch_add(1));
    REQUIRE(fs::create_directory(root));
  }

  ~TemporaryDirectory() {
    std::error_code error;
    try {
      fs::remove_all(root, error);
    } catch (const std::bad_alloc&) {
      std::fputs("Save test directory cleanup ran out of memory\n", stderr);
      std::abort();
    }
    if (error) {
      std::fprintf(stderr, "Save test directory cleanup failed: %d\n",
                   error.value());
      std::abort();
    }
  }
};

struct CloseHandleDeleter {
  void operator()(HANDLE handle) const {
    if (!CloseHandle(handle)) {
      std::fprintf(stderr, "Save test handle cleanup failed: %lu\n",
                   GetLastError());
      std::abort();
    }
  }
};
using Handle = std::unique_ptr<void, CloseHandleDeleter>;

Handle OpenLocked(const fs::path& path, DWORD sharing) {
  const HANDLE handle =
      CreateFileW(path.c_str(), GENERIC_READ, sharing, nullptr, OPEN_ALWAYS,
                  FILE_ATTRIBUTE_NORMAL, nullptr);
  REQUIRE(handle != INVALID_HANDLE_VALUE);
  return Handle(handle);
}

void Put(const fs::path& path, std::string_view data) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  REQUIRE(output.is_open());
  output.write(data.data(), static_cast<std::streamsize>(data.size()));
  output.close();
  REQUIRE(output.good());
}

std::string Get(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.is_open());
  return {std::istreambuf_iterator<char>(input), {}};
}

fs::path Backup(fs::path path) {
  path += L".bak";
  return path;
}

void CheckNoTemporaryFiles(const fs::path& root) {
  for (const auto& entry : fs::directory_iterator(root)) {
    CHECK_FALSE(entry.path().extension() == ".tmp");
    CHECK_FALSE(entry.path().extension() == ".lock");
  }
}

void CheckPresence(const SaveStore& store, int slot, bool expected) {
  const auto exists = store.SlotExists(slot);
  REQUIRE(exists);
  CHECK(*exists == expected);
}

void CheckMissingSlot(const SaveStore& store, int slot) {
  const auto missing = store.Load(slot, Validate);
  REQUIRE_FALSE(missing);
  CHECK_FALSE(missing.error().empty());
}

void CheckLoaded(const SaveStore& store, int slot, std::string_view expected,
                 bool recovered) {
  const auto loaded = store.Load(slot, Validate);
  REQUIRE(loaded);
  CHECK(loaded->data == expected);
  CHECK(loaded->recovered_from_backup == recovered);
  CHECK(loaded->recovery_reason.empty() != recovered);
}

void CheckRefusedWrite(const SaveStore& store, int slot,
                       std::string_view data) {
  const auto result = store.Save(slot, data, Validate);
  REQUIRE_FALSE(result);
  CHECK_FALSE(result.error().empty());
}

void WriteTwoGenerations(const SaveStore& store, int slot) {
  REQUIRE(store.Save(slot, "v1:first", Validate));
  REQUIRE(store.Save(slot, "v1:second", Validate));
}

void CheckRecoveryWrite(const SaveStore& store, int slot,
                        const fs::path& primary) {
  if (fs::exists(primary)) {
    CheckRefusedWrite(store, slot, "v1:third");
    REQUIRE(store.Save(slot, "v1:third", Validate,
                       VS::Save::WriteConsent::kReplaceRecovered));
  } else {
    REQUIRE(store.Save(slot, "v1:third", Validate));
  }
}
}  // namespace

TEST_CASE("Save storage reports invalid roots rather than changing location",
          "[save-store]") {
  TemporaryDirectory directory;
  CHECK_FALSE(SaveStore::Open({}));
  const auto file = directory.root / "file";
  Put(file, "not a directory");
  CHECK_FALSE(SaveStore::Open(file));
  CHECK_FALSE(SaveStore::Open(file / "child"));
  const auto root = directory.root / fs::path(u8"native-\u00E9") / "saves";
  const auto store = SaveStore::Open(root);
  REQUIRE(store);
  CHECK(store->Root() == fs::absolute(root));
}

TEST_CASE("Native slot names reject invalid indices without creating files",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const int slot =
      GENERATE(-1, SaveStore::kSlotCount, std::numeric_limits<int>::max());
  CHECK_FALSE(store->SlotPath(slot));
  CHECK_FALSE(store->SlotExists(slot));
  CHECK_FALSE(store->Load(slot, Validate));
  CHECK_FALSE(store->Save(slot, "v1:valid", Validate));
  CHECK(fs::is_empty(directory.root));
}

TEST_CASE("Empty and boundary slots have explicit presence and load results",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const int slot = GENERATE(0, SaveStore::kSlotCount - 1);
  const auto path = store->SlotPath(slot);
  REQUIRE(path);
  CHECK(path->parent_path() == directory.root);
  CHECK(path->extension() == ".vssave");
  CheckPresence(*store, slot, false);
  CheckMissingSlot(*store, slot);
  REQUIRE(store->Save(slot, "v1:present", Validate));
  CheckPresence(*store, slot, true);
}

TEST_CASE("Atomic saves retain exact bytes and the previous valid generation",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(3);
  REQUIRE(path);
  std::string previous;
  for (int generation = 0; generation < 4; ++generation) {
    const auto value = std::format("v1:generation{}{}", generation, '\0');
    REQUIRE(store->Save(3, value, Validate));
    CheckLoaded(*store, 3, value, false);
    if (!previous.empty()) CHECK(Get(Backup(*path)) == previous);
    previous = value;
    CheckNoTemporaryFiles(directory.root);
  }
}

TEST_CASE("Invalid writes and missing validators leave saved data unchanged",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(0);
  REQUIRE(path);
  REQUIRE(store->Save(0, "v1:first", Validate));
  REQUIRE(store->Save(0, "v1:second", Validate));
  const std::string oversized(SaveStore::kMaximumBytes + 1, 'x');
  CheckRefusedWrite(*store, 0, "");
  CheckRefusedWrite(*store, 0, "incomplete");
  CheckRefusedWrite(*store, 0, oversized);
  CHECK_FALSE(store->Save(0, "v1:third", nullptr));
  CHECK_FALSE(store->Load(0, nullptr));
  CHECK(Get(*path) == "v1:second");
  CHECK(Get(Backup(*path)) == "v1:first");
  CheckNoTemporaryFiles(directory.root);
}

TEST_CASE("Save validation retains per-load results through primary recovery",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(0);
  REQUIRE(path);
  WriteTwoGenerations(*store, 0);
  const bool recover = GENERATE(false, true);
  if (recover) Put(*path, "corrupt");
  std::vector<std::string> attempted;
  std::string accepted;
  const auto loaded = store->Load(
      0, [&](std::string_view data) -> std::expected<void, std::string> {
        attempted.emplace_back(data);
        const auto valid = Validate(data);
        if (valid) accepted = data;
        return valid;
      });
  REQUIRE(loaded);
  CHECK(loaded->recovered_from_backup == recover);
  CHECK(accepted == loaded->data);
  const auto expected = recover
                            ? std::vector<std::string>{"corrupt", "v1:first"}
                            : std::vector<std::string>{"v1:second"};
  CHECK(attempted == expected);
  CHECK(accepted == (recover ? "v1:first" : "v1:second"));
}

TEST_CASE("Corrupt or missing primary saves explicitly recover valid backups",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(2);
  REQUIRE(path);
  WriteTwoGenerations(*store, 2);
  SECTION("corrupt primary") { Put(*path, "corrupt"); }
  SECTION("missing primary") { REQUIRE(fs::remove(*path)); }
  SECTION("oversized primary") {
    fs::resize_file(*path, SaveStore::kMaximumBytes + 1);
  }
  CheckPresence(*store, 2, true);
  CheckLoaded(*store, 2, "v1:first", true);
  CheckRecoveryWrite(*store, 2, *path);
  CHECK(Get(*path) == "v1:third");
  CHECK(Get(Backup(*path)) == "v1:first");
}

TEST_CASE("Invalid primary and backup never become successful empty saves",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(1);
  REQUIRE(path);
  Put(*path, "invalid current");
  Put(Backup(*path), "invalid backup");
  const auto loaded = store->Load(1, Validate);
  REQUIRE_FALSE(loaded);
  CHECK(loaded.error().find("unsupported") != std::string::npos);
  CHECK_FALSE(store->Save(1, "v1:new", Validate));
  CHECK_FALSE(store->Save(1, "v1:new", Validate,
                          VS::Save::WriteConsent::kReplaceRecovered));
  CHECK(Get(*path) == "invalid current");
  CHECK(Get(Backup(*path)) == "invalid backup");
  CheckNoTemporaryFiles(directory.root);
}

TEST_CASE("Interrupted uncommitted temporary writes are ignored",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  Put(directory.root / "slot00.vssave.interrupted.tmp", "v1:partial");
  const auto present = store->SlotExists(0);
  REQUIRE(present);
  CHECK_FALSE(*present);
  CHECK_FALSE(store->Load(0, Validate));
  REQUIRE(store->Save(0, "v1:committed", Validate));
  const auto loaded = store->Load(0, Validate);
  REQUIRE(loaded);
  CHECK(loaded->data == "v1:committed");
  CHECK_FALSE(loaded->recovered_from_backup);
}

TEST_CASE("A failed primary commit preserves both the old save and backup",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(4);
  REQUIRE(path);
  REQUIRE(store->Save(4, "v1:first", Validate));
  auto handle = OpenLocked(*path, FILE_SHARE_READ);
  const auto failed = store->Save(4, "v1:second", Validate);
  REQUIRE_FALSE(failed);
  CHECK(failed.error().find("Commit") != std::string::npos);
  CHECK(Get(*path) == "v1:first");
  CHECK(Get(Backup(*path)) == "v1:first");
  CheckNoTemporaryFiles(directory.root);
  handle.reset();
  REQUIRE(store->Save(4, "v1:second", Validate));
}

TEST_CASE("A failed backup commit never replaces the current save",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(5);
  REQUIRE(path);
  REQUIRE(store->Save(5, "v1:first", Validate));
  REQUIRE(store->Save(5, "v1:second", Validate));
  const auto handle = OpenLocked(Backup(*path), FILE_SHARE_READ);
  CHECK_FALSE(store->Save(5, "v1:third", Validate));
  CHECK(Get(*path) == "v1:second");
  CHECK(Get(Backup(*path)) == "v1:first");
  CheckNoTemporaryFiles(directory.root);
}

TEST_CASE(
    "Native same-slot write locks fail explicitly without blocking others",
    "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(6);
  REQUIRE(path);
  auto lock_path = *path;
  lock_path += L".lock";
  const auto handle = OpenLocked(lock_path, 0);
  const auto locked = store->Save(6, "v1:first", Validate);
  REQUIRE_FALSE(locked);
  CHECK(locked.error().find("Lock") != std::string::npos);
  CHECK_FALSE(fs::exists(*path));
  REQUIRE(store->Save(7, "v1:other slot", Validate));
}

TEST_CASE("Save storage refuses non-file slots and preserves their contents",
          "[save-store]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(8);
  REQUIRE(path);
  REQUIRE(fs::create_directory(*path));
  Put(*path / "keep", "owned fixture");
  CHECK_FALSE(store->SlotExists(8));
  CHECK_FALSE(store->Load(8, Validate));
  CHECK_FALSE(store->Save(8, "v1:first", Validate));
  CHECK(Get(*path / "keep") == "owned fixture");
  CheckNoTemporaryFiles(directory.root);
}

TEST_CASE(
    "Unverified current saves are not overwritten through backup recovery",
    "[save-store][save-overwrite]") {
  TemporaryDirectory directory;
  const auto store = SaveStore::Open(directory.root);
  REQUIRE(store);
  const auto path = store->SlotPath(9);
  REQUIRE(path);
  WriteTwoGenerations(*store, 9);
  Put(*path, "v2:unsupported newer save");
  CheckLoaded(*store, 9, "v1:first", true);
  const auto overwritten = store->Save(9, "v1:third", Validate);
  CHECK_FALSE(overwritten);
  CHECK(Get(*path) == "v2:unsupported newer save");
  CHECK(Get(Backup(*path)) == "v1:first");
  REQUIRE(store->Save(9, "v1:third", Validate,
                      VS::Save::WriteConsent::kReplaceRecovered));
  CHECK(Get(*path) == "v1:third");
  CHECK(Get(Backup(*path)) == "v1:first");
}
