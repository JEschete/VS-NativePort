#pragma once

#include <filesystem>

#include "Content/ActionCategoryCatalog.hpp"
#include "Content/StatusCatalog.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "TestSupport/ActionCategoryRoot.hpp"
#include "TestSupport/ScratchDataRoot.hpp"
#include "TestSupport/StatusRoot.hpp"

namespace VS::test_support {
inline Content::StatusCatalog DefaultStatusCatalog() {
  Content::StatusCatalog status;
  status.timings = {15, 20, 15, 15, 2, 5};
  return status;
}

// Writes the status and action category catalogs and their manifest under a
// root that already lists its own files.
inline void PublishBattleCatalogs(
    const std::filesystem::path& root, const Content::StatusCatalog& status,
    const Content::ActionCategoryCatalog& categories) {
  Content::ConversionManifest manifest;
  AddStatusCatalog(root, status, manifest);
  AddActionCategoryCatalog(root, categories, manifest);
  const auto written = Extract::WriteConversionManifest(root, manifest);
  if (!written) FAIL(written.error());
}

// The active store for tests that reach live battle code without needing
// anything else from the disc: the status and action category catalogs.
class BattleCatalogRoot : public ScratchDataRoot {
 public:
  BattleCatalogRoot()
      : ScratchDataRoot("test-battle-catalogs"),
        status(DefaultStatusCatalog()),
        categories(OriginalActionCategories()) {
    Publish();
  }

  Content::StatusCatalog status;
  Content::ActionCategoryCatalog categories;
  void Publish() const {
    Write(Content::DataStore::ManifestName, "lba\tsize\tform\tpath\n");
    PublishBattleCatalogs(Root(), status, categories);
    Open();
  }
};
}  // namespace VS::test_support
