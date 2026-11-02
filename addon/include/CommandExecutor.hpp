#pragma once

#include "SearchEngine.hpp"

namespace Baaz {

class CommandExecutor {
public:
    static bool Execute(const SearchItem& item);
};

} // namespace Baaz
