#pragma once

#include <vector>
#include "CommandCatalog.hpp"
#include "SearchEngine.hpp"

namespace Baaz {

// Views, layouts and layer combinations of the open project
class NavigatorIndexer {
public:
    static std::vector<SearchItem> BuildProjectIndex();
    static bool HasOpenProject();
    // Every row of Archicad's command table; may throw, callers catch. Empty without the Archicad API.
    static std::vector<CommandEntry> ReadCommandTable();
};

} // namespace Baaz
