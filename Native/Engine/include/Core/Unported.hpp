#pragma once

#include "Core/Types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace VS::Core {

// Records each call into code that is copied but not yet ported (T-class shims, plan §7).
// Each distinct site is logged once; counts feed SHIM_LEDGER via tools\ledger\scan_native.py.
struct UnportedHit {
    std::string site;
    u64 count = 0;
};

void RecordUnported(std::string_view site);
std::vector<UnportedHit> UnportedSnapshot();
void DumpUnportedToLog();

} // namespace VS::Core

#define VS_UNPORTED(site) ::VS::Core::RecordUnported(site)
