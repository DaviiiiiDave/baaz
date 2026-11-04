#pragma once

#include "CommandSource.hpp"
#include <string>
#include <vector>

namespace Baaz {

// The pinned line above the results that says why Archicad's commands are missing, with a short guide
struct HintRow {
    std::wstring title;                // empty: no row
    std::vector<std::wstring> guide;   // numbered steps, one per line below the title
    bool canOpenWorkEnvironment = false;   // Enter or a click on the row opens Options > Work Environment
};

// lastRunRefused: the last command Baaz was asked to run did not run (shown once, by the caller)
HintRow MakeHintRow(CommandState state, size_t unnamed, bool workEnvironmentFound, bool lastRunRefused);

// Index of the first result row selected after a query. The hint row is not a result, so it never
// shifts or takes this selection.
size_t DefaultSelection(bool hasHint, size_t resultCount);

} // namespace Baaz
