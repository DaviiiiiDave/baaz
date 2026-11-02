#pragma once

#include "SchemeReader.hpp"
#include <windows.h>
#include <string>
#include <vector>

namespace Baaz {

struct LiveEntry {
    MenuNodeType type = MenuNodeType::Item;
    unsigned liveId = 0;
    bool enabled = true;
};

// The live Win32 menu bar, behind an interface so linking can be tested without Archicad.
class MenuSource {
public:
    virtual ~MenuSource() = default;
    virtual std::vector<std::wstring> TopLevelTitles() = 0;  // access keys removed
    // path[0] = top-level index, then popup indices; fills the menu before reading it
    virtual std::vector<LiveEntry> Entries(const std::vector<int>& path) = 0;
};

struct LinkedCommand {
    std::wstring name;
    std::wstring menuPath;      // menu titles joined with U+203A, e.g. View or Documentation > Graphic Override
    std::vector<int> position;  // full path including the item's own index
    unsigned liveId = 0;
    bool enabled = true;
};

// Names for the entries Archicad puts in place of one placeholder item, in menu order
struct DynamicList {
    unsigned placeholder = 0;
    std::vector<std::wstring> names;
};

// Links the exported menu tree to the live menu by position. Whenever a position is in doubt,
// nothing is linked there: a name must never run a different command.
class MenuLink {
public:
    // Assumes equal item counts mean the same items in the same order. A placeholder may expand to
    // any number of live entries, so anchoring around it cannot tell an expanded placeholder from
    // an unexplained extra entry or a missing static item. This relies on a current export (the
    // staleness check) and on callers re-checking the live entry before running it.
    static std::vector<LinkedCommand> Link(const std::vector<MenuNode>& bar, MenuSource& live,
                                           const std::vector<DynamicList>& lists);
    // -1 if top-level titles differ; otherwise the number of linked commands
    static int FitScore(const std::vector<MenuNode>& bar, MenuSource& live);
    // true when any scheme file is newer than the export
    static bool IsExportStale(const FILETIME& exportTime, const std::vector<FILETIME>& schemeTimes);
};

} // namespace Baaz
