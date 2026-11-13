// Keep assert() active in Release builds too
#undef NDEBUG
#include "CommandHint.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace Baaz;

static const wchar_t* const kRefused = L"The last command did not run: it is no longer available or is disabled.";
static const wchar_t* const kUnreadable = L"Commands cannot be read in this Archicad version.";

void TestNoHintWhenReadable() {
    const HintRow row = MakeHintRow(true, false);
    assert(row.title.empty() && row.guide.empty());
    std::cout << "[PASS] TestNoHintWhenReadable\n";
}

void TestUnreadableHint() {
    const HintRow row = MakeHintRow(false, false);
    assert(row.title == kUnreadable && row.guide.empty());
    std::cout << "[PASS] TestUnreadableHint\n";
}

void TestRefusedRunIsShownOnce() {
    // Readable commands have no hint of their own: the refusal is the row
    const HintRow refused = MakeHintRow(true, true);
    assert(refused.title == kRefused && refused.guide.empty());
    // Next to the unreadable hint it is one more line
    const HintRow both = MakeHintRow(false, true);
    assert(both.title == kUnreadable && both.guide == std::vector<std::wstring>{ kRefused });
    std::cout << "[PASS] TestRefusedRunIsShownOnce\n";
}

int main() {
    TestNoHintWhenReadable();
    TestUnreadableHint();
    TestRefusedRunIsShownOnce();
    std::cout << "ALL COMMAND HINT TESTS PASSED\n";
    return 0;
}
