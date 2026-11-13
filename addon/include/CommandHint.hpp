#pragma once

#include <string>
#include <vector>

namespace Baaz {

// The pinned line above the results: that Archicad's commands cannot be read, and, once, that the last
// command did not run
struct HintRow {
    std::wstring title;                // empty: no row
    std::vector<std::wstring> guide;   // one line each, below the title
};

// lastRunRefused: the last command Baaz was asked to run did not run (shown once, by the caller)
HintRow MakeHintRow(bool commandsReadable, bool lastRunRefused);

} // namespace Baaz
