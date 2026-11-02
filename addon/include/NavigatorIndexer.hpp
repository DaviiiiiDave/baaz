#pragma once

#include <vector>
#include "SearchEngine.hpp"

namespace Baaz {

// Views, layouts and layer combinations of the open project
class NavigatorIndexer {
public:
    static std::vector<SearchItem> BuildProjectIndex();
    static bool HasOpenProject();
};

} // namespace Baaz
