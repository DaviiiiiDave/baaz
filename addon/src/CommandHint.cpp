#include "CommandHint.hpp"

namespace Baaz {

static const wchar_t* const kUnreadableLine = L"Commands cannot be read in this Archicad version.";
static const wchar_t* const kRefusedLine = L"The last command did not run: it is no longer available or is disabled.";

HintRow MakeHintRow(bool commandsReadable, bool lastRunRefused) {
    HintRow row;
    if (!commandsReadable) row.title = kUnreadableLine;
    if (lastRunRefused && row.title.empty()) row.title = kRefusedLine;
    else if (lastRunRefused) row.guide.push_back(kRefusedLine);
    return row;
}

} // namespace Baaz
