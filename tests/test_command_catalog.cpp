// Keep assert() active in Release builds too
#undef NDEBUG
#include "CommandCatalog.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace Baaz;

static CommandEntry Entry(unsigned long commandId, unsigned long developerId = 0, unsigned long productId = 0) {
    return { commandId, developerId, productId, L"Lineal", L"04 Ausschnitt", true, true };
}

void TestOfferedPlainCommand() {
    const CommandEntry entry = { 1557, 0, 0, L"Lineal", L"04 Ausschnitt", true, true };
    assert(CommandCatalog::IsOffered(entry));
    std::cout << "[PASS] TestOfferedPlainCommand\n";
}

void TestInvisibleAndUnnamedAreDropped() {
    CommandEntry hidden = Entry(1557);
    hidden.visible = false;
    assert(!CommandCatalog::IsOffered(hidden));
    CommandEntry unnamed = Entry(1557);
    unnamed.name = L"";
    assert(!CommandCatalog::IsOffered(unnamed));
    std::cout << "[PASS] TestInvisibleAndUnnamedAreDropped\n";
}

void TestFilterRanges() {
    for (unsigned long id : { 256ul, 267ul, 13824ul, 14001ul, 19200ul, 19225ul, 30464ul, 30472ul })
        assert(CommandCatalog::IsOffered(Entry(id)));
    for (unsigned long id : { 257ul, 266ul, 13825ul, 13900ul, 14000ul, 19201ul, 19224ul, 30465ul, 30471ul })
        assert(!CommandCatalog::IsOffered(Entry(id)));
    for (unsigned long id : { 13983ul, 13984ul, 13988ul })
        assert(CommandCatalog::IsOffered(Entry(id)));
    // The ranges are Archicad's own commands; an add-on's id may fall inside them
    assert(CommandCatalog::IsOffered(Entry(13880, 1, 2)));
    assert(CommandCatalog::IsOffered(Entry(13880, 0, 5)));
    assert(CommandCatalog::IsOffered(Entry(19210, 7, 0)));
    std::cout << "[PASS] TestFilterRanges\n";
}

void TestDrawingContextMenuGroupIsDropped() {
    // Archicad's own "Individuelles Kontext-Menue" group: acts on context the user does not see
    const unsigned long dropped[] = {
        2339ul, 2340ul, 2341ul, 2344ul, 2345ul, 2355ul, 2364ul, 2366ul, 8197ul, 9985ul, 9989ul,
        9990ul, 9991ul, 9992ul, 9993ul, 9994ul, 9995ul, 9996ul, 9997ul, 9999ul, 10000ul, 10005ul,
        10006ul, 10009ul, 10010ul, 10012ul, 10013ul, 10014ul, 10015ul, 10016ul, 10017ul, 10021ul,
        10022ul, 10024ul, 10025ul, 10026ul, 10030ul, 10031ul, 10032ul, 10033ul, 10035ul, 10037ul,
        10038ul, 10039ul, 10040ul, 10041ul, 10042ul, 10043ul, 10044ul, 10045ul, 10046ul, 10047ul,
        10049ul, 10052ul, 10053ul, 10054ul, 10055ul, 10056ul, 10057ul, 10058ul, 10059ul, 10061ul,
        10062ul, 10063ul, 10064ul, 10065ul, 10066ul, 10067ul, 10068ul, 10069ul, 10070ul, 10071ul,
        10072ul, 10073ul, 10074ul, 10075ul, 10076ul, 10078ul, 10079ul, 10080ul, 10081ul, 10082ul,
        10083ul, 19464ul, 19468ul,
    };
    for (unsigned long id : dropped)
        assert(!CommandCatalog::IsOffered(Entry(id)));
    // Ids next to the group are real commands
    for (unsigned long id : { 2338ul, 2342ul, 8196ul, 8198ul, 9984ul, 9986ul, 10084ul, 19463ul, 19465ul, 19469ul })
        assert(CommandCatalog::IsOffered(Entry(id)));
    // An add-on's id may collide with the group
    assert(CommandCatalog::IsOffered(Entry(9994, 1, 2)));
    std::cout << "[PASS] TestDrawingContextMenuGroupIsDropped\n";
}

void TestCategoryLabel() {
    assert(CommandCatalog::CategoryLabel(L"04 Ausschnitt") == L"Ausschnitt");
    assert(CommandCatalog::CategoryLabel(L"12 Werkzeuge") == L"Werkzeuge");
    assert(CommandCatalog::CategoryLabel(L"Navigator Kontext Men\x00FCs") == L"Navigator Kontext Men\x00FCs");
    assert(CommandCatalog::CategoryLabel(L"").empty());
    assert(CommandCatalog::CategoryLabel(L"4 Foo") == L"4 Foo");
    assert(CommandCatalog::CategoryLabel(L"04") == L"04");
    assert(CommandCatalog::CategoryLabel(L"04Ausschnitt") == L"04Ausschnitt");
    std::cout << "[PASS] TestCategoryLabel\n";
}

void TestMakeItemsMapsAndFilters() {
    CommandEntry disabled = Entry(1558, 3, 4);
    disabled.name = L"Winkel";
    disabled.category = L"12 Werkzeuge";
    disabled.enabled = false;
    CommandEntry hidden = Entry(1559);
    hidden.visible = false;
    const std::vector<SearchItem> items = CommandCatalog::MakeItems({ Entry(1557), disabled, hidden });
    assert(items.size() == 2);
    assert(items[0].kind == ItemKind::Command && items[0].title == L"Lineal" && items[0].tag == L"Ausschnitt");
    assert(items[0].commandId == 1557 && items[0].developerId == 0 && items[0].productId == 0);
    assert(items[0].enabled);
    assert(items[1].kind == ItemKind::Command && items[1].title == L"Winkel" && items[1].tag == L"Werkzeuge");
    assert(items[1].commandId == 1558 && items[1].developerId == 3 && items[1].productId == 4);
    assert(!items[1].enabled);
    std::cout << "[PASS] TestMakeItemsMapsAndFilters\n";
}

void TestRunnableNeedsSameKeyOfferedAndEnabled() {
    const SearchItem item = CommandCatalog::MakeItems({ Entry(1557) })[0];
    assert(CommandCatalog::IsRunnable({ Entry(1557) }, item));

    CommandEntry disabled = Entry(1557);
    disabled.enabled = false;
    assert(!CommandCatalog::IsRunnable({ disabled }, item));
    assert(!CommandCatalog::IsRunnable({}, item));
    assert(!CommandCatalog::IsRunnable({ Entry(1557, 1, 0) }, item));
    assert(!CommandCatalog::IsRunnable({ Entry(1557, 0, 1) }, item));

    // A filtered command stays unrunnable even if a stale item carries its id
    SearchItem filtered;
    filtered.kind = ItemKind::Command;
    filtered.commandId = 13880;
    assert(!CommandCatalog::IsRunnable({ Entry(13880) }, filtered));
    std::cout << "[PASS] TestRunnableNeedsSameKeyOfferedAndEnabled\n";
}

int main() {
    TestOfferedPlainCommand();
    TestInvisibleAndUnnamedAreDropped();
    TestFilterRanges();
    TestDrawingContextMenuGroupIsDropped();
    TestCategoryLabel();
    TestMakeItemsMapsAndFilters();
    TestRunnableNeedsSameKeyOfferedAndEnabled();
    std::cout << "ALL COMMAND CATALOG TESTS PASSED\n";
    return 0;
}
