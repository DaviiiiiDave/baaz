// Keep assert() active in Release builds too
#undef NDEBUG
#include "MenuLink.hpp"
#include "SchemeReader.hpp"
#include <cassert>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace Baaz;

static std::string ReadFixture() {
    std::ifstream file(std::string(BAAZ_TEST_FIXTURES) + "/command_scheme_ac29_de.xml", std::ios::binary);
    assert(file.is_open());
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static std::vector<std::wstring> Names(const std::vector<MenuNode>& nodes) {
    std::vector<std::wstring> names;
    for (const MenuNode& node : nodes) names.push_back(node.name);
    return names;
}

static const MenuNode* FindChild(const MenuNode& parent, const std::wstring& name) {
    for (const MenuNode& child : parent.children) {
        if (child.name == name) return &child;
    }
    return nullptr;
}

static bool SameTree(const std::vector<MenuNode>& a, const std::vector<MenuNode>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].type != b[i].type || a[i].name != b[i].name || a[i].commandNumber != b[i].commandNumber ||
            !SameTree(a[i].children, b[i].children)) {
            return false;
        }
    }
    return true;
}

static const char* const kFloorPlanGuid = "2085E4D3-5694-4BB9-ACAC-3AED5BE50091";
static const char* const kTextGuid = "2CAC215F-221C-420F-AAEB-8462A659B8D9";

static std::string Scheme(const std::string& contents) {
    return "<Scheme><GlobalSettings><CommandLayout>" + contents + "</CommandLayout></GlobalSettings></Scheme>";
}

static std::string BarWithGuid(const char* guid, const std::string& menus) {
    return std::string("<MenuBar ID=\"B\" menubarguid=\"") + guid + "\" name=\"x\">" + menus + "</MenuBar>";
}

// A complete scheme with one menu bar, so edge cases only spell out the menus
static std::string FloorPlanBar(const std::string& menus) {
    return Scheme(BarWithGuid(kFloorPlanGuid, menus));
}

void TestReadsFloorPlanBarOnly() {
    auto menus = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    assert((Names(menus) == std::vector<std::wstring>{ L"Ansicht", L"Dokumentation" }));
    for (const MenuNode& menu : menus) assert(menu.type == MenuNodeType::Popup);
    std::cout << "[PASS] TestReadsFloorPlanBarOnly\n";
}

void TestRulerPosition() {
    auto menus = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    assert(menus.size() == 2);
    const MenuNode& ruler = menus[0].children.at(9);
    assert(ruler.type == MenuNodeType::Item);
    assert(ruler.name == L"Lineal");
    assert(ruler.commandNumber == 1557);
    assert(menus[0].children.at(8).type == MenuNodeType::Separator);
    std::cout << "[PASS] TestRulerPosition\n";
}

void TestIdrefResolved() {
    auto menus = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    assert(menus.size() == 2);
    const MenuNode& displayOptions = menus[0].children.at(0);
    assert(displayOptions.type == MenuNodeType::Popup);
    assert(displayOptions.name == L"Bildschirmdarstellungs-Optionen");
    assert(!displayOptions.children.empty());
    // Referenced from a menu that is itself defined inline: the second hop also resolves
    const MenuNode& gridOptions = menus[0].children.at(6);
    const MenuNode* projection = FindChild(gridOptions, L"Projektion auf Bearbeitungsebene");
    assert(projection != nullptr && !projection->children.empty());
    std::cout << "[PASS] TestIdrefResolved\n";
}

void TestPlaceholderNumbers() {
    auto menus = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    assert(menus.size() == 2);
    const MenuNode* overrides = FindChild(menus[1], L"Grafische \u00dcberschreibung");
    assert(overrides != nullptr);
    assert(overrides->children.at(2).commandNumber == 23040);
    const MenuNode* viewOptions = FindChild(menus[1], L"Modelldarstellung");
    assert(viewOptions != nullptr);
    assert(viewOptions->children.at(2).commandNumber == 9472);
    std::cout << "[PASS] TestPlaceholderNumbers\n";
}

void TestNamesUnescaped() {
    auto menus = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    assert(menus.size() == 2);
    assert(menus[0].children.at(6).name == L"Raster- & Bearbeitungsebenen-Optionen");
    assert(menus[0].children.at(13).name == L"Elementfang");

    // Entities, access keys and non-ASCII text in titles and comments
    auto parsed = SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar(
        "<Menu ID=\"M\" title=\"&amp;Ab&#108;age &#xE4;\">"
        "<Item command=\"7-0x0-0x0\" comment=\"a&lt;b&gt; &quot;c&quot; &apos;d&apos; Neu &amp;&amp; &amp;W\"/>"
        "<Item command=\"8-0x0-0x0\" comment=\"Gr\xC3\xB6\xC3\x9F" "e\"/>"
        "</Menu>"));
    assert(parsed.size() == 1);
    assert(parsed[0].name == L"Ablage \u00e4");
    assert(parsed[0].children.at(0).name == L"a<b> \"c\" 'd' Neu & W");
    assert(parsed[0].children.at(1).name == L"Gr\u00f6\u00dfe");
    std::cout << "[PASS] TestNamesUnescaped\n";
}

void TestBomCommentsAndCrlf() {
    std::string raw = ReadFixture();
    // The fixture really has what the test is about
    assert(raw.compare(0, 3, "\xEF\xBB\xBF") == 0);
    assert(raw.find("\r\n") != std::string::npos);
    assert(raw.find("<!--") != std::string::npos);

    std::string plain = raw.substr(3);
    size_t commentStart = plain.find("<!--");
    size_t commentEnd = plain.find("-->", commentStart) + 3;
    plain.erase(commentStart, commentEnd - commentStart);
    for (size_t pos = plain.find("\r\n"); pos != std::string::npos; pos = plain.find("\r\n", pos)) {
        plain.erase(pos, 1);
    }
    assert(plain.find('\r') == std::string::npos && plain.find("<!--") == std::string::npos);

    auto withExtras = SchemeReader::ReadFloorPlanMenuBar(raw);
    auto without = SchemeReader::ReadFloorPlanMenuBar(plain);
    assert(!without.empty());
    assert(SameTree(withExtras, without));

    // A comment may hold markup and a '>' without confusing the reader
    auto tricky = SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar(
        "<!-- <Menu ID=\"Ghost\" title=\"Ghost\"> -->"
        "<Menu ID=\"M\" title=\"Real\"/>"));
    assert((Names(tricky) == std::vector<std::wstring>{ L"Real" }));
    std::cout << "[PASS] TestBomCommentsAndCrlf\n";
}

void TestMalformedIsEmpty() {
    assert(SchemeReader::ReadFloorPlanMenuBar("").empty());
    assert(SchemeReader::ReadFloorPlanMenuBar("<Scheme>").empty());
    assert(SchemeReader::ReadFloorPlanMenuBar("not xml at all").empty());
    // Unclosed tag, mismatched end tag, unterminated attribute and comment
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu ID=\"M\" title=\"A\">")).empty());
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu ID=\"M\" title=\"A\"></Item>")).empty());
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu ID=\"M\" title=\"A/>")).empty());
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<!-- never closed")).empty());
    // Unknown entity, invalid UTF-8, IDREF to nothing
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu ID=\"M\" title=\"&bogus;\"/>")).empty());
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu ID=\"M\" title=\"\xC3\"/>")).empty());
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu IDREF=\"Nowhere\"/>")).empty());
    // Only the text bar, or no CommandLayout at all
    assert(SchemeReader::ReadFloorPlanMenuBar(
        "<Scheme><GlobalSettings><CommandLayout>"
        "<MenuBar ID=\"B\" menubarguid=\"2CAC215F-221C-420F-AAEB-8462A659B8D9\" name=\"Text\">"
        "<Menu ID=\"M\" title=\"Text-Only\"/></MenuBar></CommandLayout></GlobalSettings></Scheme>").empty());
    assert(SchemeReader::ReadFloorPlanMenuBar("<Scheme/>").empty());
    // A menu bar with the right GUID in a document that is otherwise fine still works
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu ID=\"M\" title=\"A\"/>")).size() == 1);
    std::cout << "[PASS] TestMalformedIsEmpty\n";
}

void TestReferenceCycleIsEmpty() {
    auto loop = SchemeReader::ReadFloorPlanMenuBar(
        "<Scheme><GlobalSettings><CommandLayout>"
        "<Menu ID=\"A\" title=\"A\"><PopupItem><Menu IDREF=\"B\"/></PopupItem></Menu>"
        "<Menu ID=\"B\" title=\"B\"><PopupItem><Menu IDREF=\"A\"/></PopupItem></Menu>"
        "<MenuBar ID=\"M\" menubarguid=\"2085E4D3-5694-4BB9-ACAC-3AED5BE50091\" name=\"x\">"
        "<Menu IDREF=\"A\"/></MenuBar></CommandLayout></GlobalSettings></Scheme>");
    assert(loop.empty());
    std::cout << "[PASS] TestReferenceCycleIsEmpty\n";
}

static const std::string kValidItem = "<Item command=\"1-0x0-0x0\" comment=\"x\"/>";
static const std::string kValidMenuBefore = "<Menu ID=\"V1\" title=\"V1\">" + kValidItem + "</Menu>";
static const std::string kValidMenuAfter = "<Menu ID=\"V2\" title=\"V2\">" + kValidItem + "</Menu>";

// A popup nested in a menu, with the construct between valid items
static std::string InsideNestedPopup(const std::string& construct) {
    return "<Menu ID=\"P\" title=\"P\">" + kValidItem + "<PopupItem><Menu ID=\"N\" title=\"N\">" + kValidItem +
           construct + kValidItem + "</Menu></PopupItem></Menu>";
}

static std::wstring Dump(const MenuNode& node) {
    if (node.type == MenuNodeType::Separator) return L"S";
    if (node.type == MenuNodeType::Item) return L"I:" + node.name + L"#" + std::to_wstring(node.commandNumber);
    std::wstring text = L"P:" + node.name + L"[";
    for (size_t i = 0; i < node.children.size(); ++i) {
        if (i > 0) text += L",";
        text += Dump(node.children[i]);
    }
    return text + L"]";
}

// One bad construct as a child of the menu bar, and as an entry of a menu
struct BadConstruct {
    const char* asBarChild;
    const char* asEntry;
};

void TestRejectionNeverYieldsPartialTree() {
    const BadConstruct constructs[] = {
        { "<PopupItem/>", "<PopupItem/>" },
        { "<PopupItem><Menu ID=\"a\" title=\"a\"/><Menu ID=\"b\" title=\"b\"/></PopupItem>",
          "<PopupItem><Menu ID=\"a\" title=\"a\"/><Menu ID=\"b\" title=\"b\"/></PopupItem>" },
        { "<PopupItem><Item command=\"1-0x0-0x0\" comment=\"x\"/></PopupItem>",
          "<PopupItem><Item command=\"1-0x0-0x0\" comment=\"x\"/></PopupItem>" },
        { "<Item command=\"x-0x0-0x0\" comment=\"a\"/>", "<Item command=\"x-0x0-0x0\" comment=\"a\"/>" },
        { "<Item command=\"99999999999-0x0-0x0\" comment=\"a\"/>", "<Item command=\"99999999999-0x0-0x0\" comment=\"a\"/>" },
        { "<Menu ID=\"Z\"/>", "<PopupItem><Menu ID=\"Z\"/></PopupItem>" },
        { "<Separator/>", "<Bogus/>" },
        { "<Menu IDREF=\"Nowhere\"/>", "<PopupItem><Menu IDREF=\"Nowhere\"/></PopupItem>" },
    };
    // The same documents without a bad construct read fine, so only the construct can empty them
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar(kValidMenuBefore + kValidMenuAfter)).size() == 2);
    auto control = SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar(kValidMenuBefore + InsideNestedPopup("") + kValidMenuAfter));
    assert(control.size() == 3);
    assert(Dump(control[1]) == L"P:P[I:x#1,P:N[I:x#1,I:x#1]]");

    for (const BadConstruct& construct : constructs) {
        assert(SchemeReader::ReadFloorPlanMenuBar(
            FloorPlanBar(kValidMenuBefore + construct.asBarChild + kValidMenuAfter)).empty());
        assert(SchemeReader::ReadFloorPlanMenuBar(
            FloorPlanBar(kValidMenuBefore + InsideNestedPopup(construct.asEntry) + kValidMenuAfter)).empty());
    }
    std::cout << "[PASS] TestRejectionNeverYieldsPartialTree\n";
}

// Menus M0..M(count-1), each holding the next one by IDREF; the last holds an item
static std::string MenuChain(int count) {
    std::string definitions;
    for (int i = 0; i < count; ++i) {
        std::string id = "M" + std::to_string(i);
        std::string inner = i + 1 < count ? "<PopupItem><Menu IDREF=\"M" + std::to_string(i + 1) + "\"/></PopupItem>" : kValidItem;
        definitions += "<Menu ID=\"" + id + "\" title=\"" + id + "\">" + inner + "</Menu>";
    }
    return Scheme(definitions + BarWithGuid(kFloorPlanGuid, "<Menu IDREF=\"M0\"/>"));
}

void TestDepthLimitNotCycle() {
    // kMaxMenuDepth is 16 in SchemeReader.cpp: a chain of 16 menus is allowed, 17 is not
    assert(SchemeReader::ReadFloorPlanMenuBar(MenuChain(1)).size() == 1);
    assert(SchemeReader::ReadFloorPlanMenuBar(MenuChain(16)).size() == 1);
    assert(SchemeReader::ReadFloorPlanMenuBar(MenuChain(17)).empty());
    std::cout << "[PASS] TestDepthLimitNotCycle\n";
}

static std::string MenuWithSeparators(size_t count) {
    std::string menu = "<Menu ID=\"M\" title=\"M\">";
    for (size_t i = 0; i < count; ++i) menu += "<Separator/>";
    return FloorPlanBar(menu + "</Menu>");
}

void TestNodeBudgetStopsExpansion() {
    // 15 menus with three links each to the next: 3^14 nodes if expanded, a few KB of XML
    std::string definitions;
    for (int i = 0; i < 15; ++i) {
        std::string id = "M" + std::to_string(i);
        std::string link = "<PopupItem><Menu IDREF=\"M" + std::to_string(i + 1) + "\"/></PopupItem>";
        definitions += "<Menu ID=\"" + id + "\" title=\"" + id + "\">" + (i < 14 ? link + link + link : kValidItem) + "</Menu>";
    }
    assert(SchemeReader::ReadFloorPlanMenuBar(
        Scheme(definitions + BarWithGuid(kFloorPlanGuid, "<Menu IDREF=\"M0\"/>"))).empty());

    // kMaxMenuNodes is 100000 in SchemeReader.cpp and counts the menu itself
    assert(SchemeReader::ReadFloorPlanMenuBar(MenuWithSeparators(99999)).size() == 1);
    assert(SchemeReader::ReadFloorPlanMenuBar(MenuWithSeparators(100000)).empty());
    std::cout << "[PASS] TestNodeBudgetStopsExpansion\n";
}

void TestInputSizeLimit() {
    // kMaxInputBytes is 8 MiB in SchemeReader.cpp; trailing white space is ignored by the reader
    std::string document = FloorPlanBar("<Menu ID=\"M\" title=\"M\"/>");
    const size_t limit = 8u * 1024u * 1024u;
    assert(SchemeReader::ReadFloorPlanMenuBar(document + std::string(limit - document.size(), ' ')).size() == 1);
    assert(SchemeReader::ReadFloorPlanMenuBar(document + std::string(limit - document.size() + 1, ' ')).empty());
    std::cout << "[PASS] TestInputSizeLimit\n";
}

void TestExactTreeShape() {
    auto menus = SchemeReader::ReadFloorPlanMenuBar(Scheme(
        "<Menu ID=\"C\" title=\"C\"><Item command=\"3-0x0-0x0\" comment=\"z\"/></Menu>" +
        BarWithGuid(kFloorPlanGuid,
            "<Menu ID=\"A\" title=\"A\">"
            "<Item command=\"1-0x0-0x0\" comment=\"x\"/>"
            "<Separator/>"
            "<PopupItem><Menu ID=\"B\" title=\"B\"><Item command=\"2-0x0-0x0\" comment=\"y\"/></Menu></PopupItem>"
            "<PopupItem><Menu IDREF=\"C\"/></PopupItem>"
            "<Item command=\"4-0x0-0x0\" comment=\"w\"/>"
            "<PopupItem><Menu ID=\"D\" title=\"D\"><Separator/></Menu></PopupItem>"
            "</Menu>"
            "<Menu ID=\"E\" title=\"E\"/>")));
    assert(menus.size() == 2);
    assert(Dump(menus[0]) == L"P:A[I:x#1,S,P:B[I:y#2],P:C[I:z#3],I:w#4,P:D[S]]");
    assert(Dump(menus[1]) == L"P:E[]");

    auto fixture = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    assert(fixture.size() == 2);
    assert(fixture[0].children.size() == 14);
    assert(fixture[1].children.size() == 4);
    assert(Dump(fixture[1]) ==
           L"P:Dokumentation["
           L"P:Modelldarstellung[I:Modelldarstellung erstellen...#1550,S,I:Keine Modelldarstellungs-Kombinationen#9472],"
           L"P:Grafische \u00dcberschreibung[I:Grafische \u00dcberschreibungen...#1564,S,"
           L"I:Keine grafischen \u00dcberschreibungen#23040,S,I:Grafische \u00dcberschreibungsregel...#1563],"
           L"S,I:Grundriss-Schnittebene...#6407]");
    std::cout << "[PASS] TestExactTreeShape\n";
}

void TestPicksBarByGuidNotOrder() {
    auto menus = SchemeReader::ReadFloorPlanMenuBar(Scheme(
        BarWithGuid(kTextGuid, "<Menu ID=\"T\" title=\"Text-Only\"/>") +
        BarWithGuid(kFloorPlanGuid, "<Menu ID=\"F\" title=\"Floor\"/>")));
    assert((Names(menus) == std::vector<std::wstring>{ L"Floor" }));
    std::cout << "[PASS] TestPicksBarByGuidNotOrder\n";
}

void TestDuplicateBarGuidIsEmpty() {
    assert(SchemeReader::ReadFloorPlanMenuBar(Scheme(
        BarWithGuid(kFloorPlanGuid, "<Menu ID=\"F1\" title=\"One\"/>") +
        BarWithGuid(kFloorPlanGuid, "<Menu ID=\"F2\" title=\"Two\"/>"))).empty());
    std::cout << "[PASS] TestDuplicateBarGuidIsEmpty\n";
}

static std::string OneMenu(const std::string& entries) {
    return FloorPlanBar("<Menu ID=\"M\" title=\"M\">" + entries + "</Menu>");
}

void TestAmbiguousInputIsEmpty() {
    assert(SchemeReader::ReadFloorPlanMenuBar(OneMenu(kValidItem + "<Separator/>")).size() == 1);

    // A repeated attribute: which value is meant?
    assert(SchemeReader::ReadFloorPlanMenuBar(OneMenu("<Item command=\"1-0x0-0x0\" comment=\"A\" comment=\"B\"/>")).empty());
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu ID=\"M\" title=\"a\" title=\"b\"/>")).empty());

    // Children of a leaf entry would be dropped silently
    assert(SchemeReader::ReadFloorPlanMenuBar(OneMenu(
        "<Item command=\"1-0x0-0x0\" comment=\"A\"><Item command=\"2-0x0-0x0\" comment=\"B\"/></Item>")).empty());
    assert(SchemeReader::ReadFloorPlanMenuBar(OneMenu("<Separator><Separator/></Separator>")).empty());

    // A reference that also has content would hide that content
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu ID=\"M\" title=\"M\"/><Menu IDREF=\"M\"/>")).size() == 2);
    assert(SchemeReader::ReadFloorPlanMenuBar(FloorPlanBar("<Menu ID=\"M\" title=\"M\"/><Menu IDREF=\"M\">" + kValidItem + "</Menu>")).empty());

    // command is <decimal>-0x<hex>-0x<hex> and nothing else
    const char* badCommands[] = { "12-garbage", "12-0x0", "12", "12-0x0-0x0-0x0", "12-0x-0x0",
                                  "12-0xZZ-0x0", "12-0x0-0x0 ", "-0x0-0x0", "12-x0-0x0" };
    for (const char* command : badCommands) {
        assert(SchemeReader::ReadFloorPlanMenuBar(
            OneMenu(std::string("<Item command=\"") + command + "\" comment=\"A\"/>")).empty());
    }
    auto good = SchemeReader::ReadFloorPlanMenuBar(OneMenu("<Item command=\"12-0x47732F64-0xd1c74938\" comment=\"A\"/>"));
    assert(good.size() == 1 && good[0].children.at(0).commandNumber == 12);
    std::cout << "[PASS] TestAmbiguousInputIsEmpty\n";
}

static const int P = -1;  // popup
static const int S = -2;  // separator
static const unsigned kAddOnPlaceholder = 1803;
static const unsigned kSecondAddOnPlaceholder = 1802;
static const unsigned kModifierDependent = 514;
static const unsigned kGraphicOverridePlaceholder = 23040;
static const unsigned kModelViewPlaceholder = 9472;
static const wchar_t* const kSeparator = L" \u203a ";

// Live menu spec: a positive number is an item with that live id
static std::vector<LiveEntry> Live(const std::vector<int>& spec) {
    std::vector<LiveEntry> entries;
    for (int item : spec) {
        LiveEntry entry;
        entry.type = item == P ? MenuNodeType::Popup : item == S ? MenuNodeType::Separator : MenuNodeType::Item;
        entry.liveId = item > 0 ? static_cast<unsigned>(item) : 0;
        entries.push_back(entry);
    }
    return entries;
}

class FakeMenu : public MenuSource {
public:
    explicit FakeMenu(std::vector<std::wstring> titles) : m_titles(std::move(titles)) {}

    void Set(const std::vector<int>& path, const std::vector<int>& spec) { m_menus[path] = Live(spec); }
    void Disable(const std::vector<int>& path, size_t index) { m_menus.at(path).at(index).enabled = false; }

    std::vector<std::wstring> TopLevelTitles() override { return m_titles; }
    std::vector<LiveEntry> Entries(const std::vector<int>& path) override {
        auto found = m_menus.find(path);
        return found == m_menus.end() ? std::vector<LiveEntry>() : found->second;
    }

private:
    std::vector<std::wstring> m_titles;
    std::map<std::vector<int>, std::vector<LiveEntry>> m_menus;
};

static MenuNode ItemNode(const wchar_t* name, unsigned number) {
    MenuNode node;
    node.name = name;
    node.commandNumber = number;
    return node;
}

static MenuNode SeparatorNode() {
    MenuNode node;
    node.type = MenuNodeType::Separator;
    return node;
}

static MenuNode PopupNode(const wchar_t* name, std::vector<MenuNode> children) {
    MenuNode node;
    node.type = MenuNodeType::Popup;
    node.name = name;
    node.children = std::move(children);
    return node;
}

static const LinkedCommand* FindCommand(const std::vector<LinkedCommand>& commands, const std::wstring& name) {
    for (const LinkedCommand& command : commands) {
        if (command.name == name) return &command;
    }
    return nullptr;
}

static bool HasLiveId(const std::vector<LinkedCommand>& commands, unsigned liveId) {
    for (const LinkedCommand& command : commands) {
        if (command.liveId == liveId) return true;
    }
    return false;
}

static std::vector<std::wstring> LinkedNames(const std::vector<LinkedCommand>& commands) {
    std::vector<std::wstring> names;
    for (const LinkedCommand& command : commands) names.push_back(command.name);
    return names;
}

static std::vector<unsigned> LinkedIds(const std::vector<LinkedCommand>& commands) {
    std::vector<unsigned> ids;
    for (const LinkedCommand& command : commands) ids.push_back(command.liveId);
    return ids;
}

// The measured Ansicht (View) menu of Archicad 29, as ids and kinds in menu order (evidence E8)
static const std::vector<int> kMeasuredAnsicht = { P, S, 1233, 256, 391, P, P, 393, S, 255, 265, P, S, 415, 416, 371, 369,
                                                   P, 261, P, S, P, P, S, P, 1180, 1176, P, S, P };
static const int kAnsichtIndex = 1;

// Catalogue of that menu: the first segments as in the real export, then segments with the same
// shape and invented names. One add-on placeholder sits where the live menu dropped it.
static MenuNode AnsichtMenu() {
    return PopupNode(L"Ansicht", {
        PopupNode(L"Bildschirmdarstellungs-Optionen", { ItemNode(L"Reinzeichnung", 10241) }),
        SeparatorNode(),
        ItemNode(L"Transparentpause", 21761),
        ItemNode(L"Konstruktionsraster darstellen", 1540),
        ItemNode(L"Darstellung der Bearbeitungsebene", 1818),
        PopupNode(L"Transparentpausen-Optionen", { ItemNode(L"Transparentpause aktivieren", 21766) }),
        PopupNode(L"Raster-Optionen", { ItemNode(L"Raster-Einstellungen...", 1538) }),
        ItemNode(L"Anzeige Ursprung", 1830),
        SeparatorNode(),
        ItemNode(L"Lineal", 1557),
        ItemNode(L"Hilfslinien", 7937),
        PopupNode(L"Hilfslinien-Optionen", { ItemNode(L"Hilfslinien-Segment erstellen", 7942) }),
        SeparatorNode(),
        ItemNode(L"Entry 1", 9001), ItemNode(L"Entry 2", 9002), ItemNode(L"Entry 3", 9003), ItemNode(L"Entry 4", 9004),
        PopupNode(L"Submenu 1", { ItemNode(L"Sub 1", 9011) }),
        ItemNode(L"Entry 5", 9005),
        PopupNode(L"Submenu 2", { ItemNode(L"Sub 2", 9012) }),
        SeparatorNode(),
        PopupNode(L"Submenu 3", { ItemNode(L"Sub 3", 9013) }),
        PopupNode(L"Submenu 4", { ItemNode(L"Sub 4", 9014) }),
        SeparatorNode(),
        ItemNode(L"Add-On commands", kAddOnPlaceholder),
        PopupNode(L"Submenu 5", { ItemNode(L"Sub 5", 9015) }),
        ItemNode(L"Orbit", 1850),
        ItemNode(L"3D Rundgang", 1851),
        PopupNode(L"Submenu 6", { ItemNode(L"Sub 6", 9016) }),
        SeparatorNode(),
        PopupNode(L"Submenu 7", { ItemNode(L"Sub 7", 9017) }),
    });
}

static std::vector<MenuNode> AnsichtBar() {
    return { PopupNode(L"Ablage", { ItemNode(L"Neu", 1) }), AnsichtMenu() };
}

static FakeMenu AnsichtFake(const std::vector<int>& ansicht) {
    FakeMenu live({ L"Ablage", L"Ansicht" });
    live.Set({ 0 }, { 77 });
    live.Set({ kAnsichtIndex }, ansicht);
    return live;
}

static std::vector<MenuNode> OneMenu(std::vector<MenuNode> children) {
    return { PopupNode(L"M", std::move(children)) };
}

static FakeMenu OneLive(const std::vector<int>& spec) {
    FakeMenu live({ L"M" });
    live.Set({ 0 }, spec);
    return live;
}

static std::vector<LinkedCommand> LinkOne(std::vector<MenuNode> children, const std::vector<int>& spec,
                                          const std::vector<DynamicList>& lists = {}) {
    FakeMenu live = OneLive(spec);
    return MenuLink::Link(OneMenu(std::move(children)), live, lists);
}

static std::vector<std::wstring> Wide(std::initializer_list<const wchar_t*> names) {
    return std::vector<std::wstring>(names.begin(), names.end());
}

static std::vector<std::wstring> GraphicOverrideNames(size_t count) {
    std::vector<std::wstring> names = { L"No overrides", L"20 Vorentwurfsplanung" };
    for (size_t i = names.size(); i < count; ++i) names.push_back(L"Combination " + std::to_wstring(i));
    names.resize(count);
    return names;
}

// Documentation as in the fixture: model view options, graphic overrides, one command
static FakeMenu DocumentationFake() {
    FakeMenu live({ L"Ansicht", L"Dokumentation" });
    live.Set({ 1 }, { P, P, S, 6001 });
    return live;
}

static const std::vector<int> kGraphicOverrideLive = { 408, S, 1840, 1841, 1842, 1843, 1844, 1845, 1846, 1847, 1848, 1849,
                                                       S, 566, S, 407 };

void TestLinksRuler() {
    auto bar = AnsichtBar();
    FakeMenu live = AnsichtFake(kMeasuredAnsicht);
    auto commands = MenuLink::Link(bar, live, {});
    const LinkedCommand* ruler = FindCommand(commands, L"Lineal");
    assert(ruler != nullptr);
    assert(ruler->liveId == 255);
    assert(ruler->menuPath == L"Ansicht");
    assert((ruler->position == std::vector<int>{ kAnsichtIndex, 9 }));
    assert(ruler->enabled);
    const LinkedCommand* neu = FindCommand(commands, L"Neu");
    assert(neu != nullptr && neu->liveId == 77 && (neu->position == std::vector<int>{ 0, 0 }) && neu->menuPath == L"Ablage");
    std::cout << "[PASS] TestLinksRuler\n";
}

void TestLinksAreOrderedByPosition() {
    auto bar = AnsichtBar();
    FakeMenu live = AnsichtFake(kMeasuredAnsicht);
    live.Set({ kAnsichtIndex, 0 }, { 501 });
    auto commands = MenuLink::Link(bar, live, {});
    assert(commands.size() > 3);
    for (size_t i = 1; i < commands.size(); ++i) assert(commands[i - 1].position < commands[i].position);
    std::cout << "[PASS] TestLinksAreOrderedByPosition\n";
}

void TestAnchorsFromSegmentEnd() {
    auto bar = AnsichtBar();
    FakeMenu live = AnsichtFake(kMeasuredAnsicht);
    auto commands = MenuLink::Link(bar, live, {});
    const LinkedCommand* orbit = FindCommand(commands, L"Orbit");
    const LinkedCommand* walkthrough = FindCommand(commands, L"3D Rundgang");
    assert(orbit != nullptr && orbit->liveId == 1180);
    assert(walkthrough != nullptr && walkthrough->liveId == 1176);
    assert(FindCommand(commands, L"Add-On commands") == nullptr);
    std::cout << "[PASS] TestAnchorsFromSegmentEnd\n";
}

void TestCountMismatchSkipsSegment() {
    std::vector<int> withoutOne;
    for (int entry : kMeasuredAnsicht) {
        if (entry != 256) withoutOne.push_back(entry);
    }
    auto bar = AnsichtBar();
    FakeMenu live = AnsichtFake(withoutOne);
    // Were the two popups of the skipped segment entered, these would link wrongly
    live.Set({ kAnsichtIndex, 4 }, { 601 });
    live.Set({ kAnsichtIndex, 5 }, { 602 });
    auto commands = MenuLink::Link(bar, live, {});
    assert(FindCommand(commands, L"Transparentpause") == nullptr);
    assert(FindCommand(commands, L"Konstruktionsraster darstellen") == nullptr);
    assert(FindCommand(commands, L"Darstellung der Bearbeitungsebene") == nullptr);
    assert(FindCommand(commands, L"Anzeige Ursprung") == nullptr);
    assert(FindCommand(commands, L"Transparentpause aktivieren") == nullptr);
    assert(FindCommand(commands, L"Raster-Einstellungen...") == nullptr);
    assert(!HasLiveId(commands, 601) && !HasLiveId(commands, 602));
    const LinkedCommand* ruler = FindCommand(commands, L"Lineal");
    assert(ruler != nullptr && ruler->liveId == 255 && (ruler->position == std::vector<int>{ kAnsichtIndex, 8 }));
    std::cout << "[PASS] TestCountMismatchSkipsSegment\n";
}

void TestDisabledCarriedOver() {
    auto bar = AnsichtBar();
    FakeMenu live = AnsichtFake(kMeasuredAnsicht);
    live.Disable({ kAnsichtIndex }, 9);
    auto commands = MenuLink::Link(bar, live, {});
    const LinkedCommand* ruler = FindCommand(commands, L"Lineal");
    assert(ruler != nullptr && !ruler->enabled);
    const LinkedCommand* guideLines = FindCommand(commands, L"Hilfslinien");
    assert(guideLines != nullptr && guideLines->enabled);
    std::cout << "[PASS] TestDisabledCarriedOver\n";
}

void TestRecursesIntoLinkedPopups() {
    auto bar = AnsichtBar();
    FakeMenu live = AnsichtFake(kMeasuredAnsicht);
    live.Set({ kAnsichtIndex, 0 }, { 501 });
    auto commands = MenuLink::Link(bar, live, {});
    const LinkedCommand* inner = FindCommand(commands, L"Reinzeichnung");
    assert(inner != nullptr && inner->liveId == 501);
    assert(inner->menuPath == std::wstring(L"Ansicht") + kSeparator + L"Bildschirmdarstellungs-Optionen");
    assert((inner->position == std::vector<int>{ kAnsichtIndex, 0, 0 }));
    std::cout << "[PASS] TestRecursesIntoLinkedPopups\n";
}

void TestGraphicOverrideList() {
    auto bar = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    FakeMenu live = DocumentationFake();
    live.Set({ 1, 1 }, kGraphicOverrideLive);
    auto commands = MenuLink::Link(bar, live, { { kGraphicOverridePlaceholder, GraphicOverrideNames(24) } });
    const std::wstring path = std::wstring(L"Dokumentation") + kSeparator + L"Grafische \u00dcberschreibung";

    const LinkedCommand* second = FindCommand(commands, L"20 Vorentwurfsplanung");
    assert(second != nullptr && second->liveId == 1841 && second->menuPath == path);
    assert((second->position == std::vector<int>{ 1, 1, 3 }));
    const LinkedCommand* rule = FindCommand(commands, L"Grafische \u00dcberschreibungsregel...");
    assert(rule != nullptr && rule->liveId == 407 && rule->menuPath == path);
    const LinkedCommand* dialog = FindCommand(commands, L"Grafische \u00dcberschreibungen...");
    assert(dialog != nullptr && dialog->liveId == 408);

    size_t listEntries = 0;
    for (const LinkedCommand& command : commands) {
        if (command.liveId >= 1840 && command.liveId <= 1849) ++listEntries;
    }
    assert(listEntries == 10);
    assert(!HasLiveId(commands, 566));
    assert(FindCommand(commands, L"Keine grafischen \u00dcberschreibungen") == nullptr);
    std::cout << "[PASS] TestGraphicOverrideList\n";
}

void TestModelViewOptionsList() {
    auto bar = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    FakeMenu live = DocumentationFake();
    live.Set({ 1, 0 }, { 412, S, 3001, 3002 });
    auto commands = MenuLink::Link(bar, live, { { kModelViewPlaceholder, Wide({ L"View A", L"View B", L"View C" }) } });
    const LinkedCommand* second = FindCommand(commands, L"View B");
    assert(second != nullptr && second->liveId == 3002);
    assert(second->menuPath == std::wstring(L"Dokumentation") + kSeparator + L"Modelldarstellung");
    assert(FindCommand(commands, L"View A")->liveId == 3001);
    assert(FindCommand(commands, L"View C") == nullptr);
    assert(FindCommand(commands, L"Modelldarstellung erstellen...")->liveId == 412);
    std::cout << "[PASS] TestModelViewOptionsList\n";
}

void TestListTooShortNotOffered() {
    auto bar = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    FakeMenu live = DocumentationFake();
    live.Set({ 1, 1 }, kGraphicOverrideLive);
    auto commands = MenuLink::Link(bar, live, { { kGraphicOverridePlaceholder, GraphicOverrideNames(3) } });
    for (unsigned id = 1840; id <= 1849; ++id) assert(!HasLiveId(commands, id));
    assert(FindCommand(commands, L"20 Vorentwurfsplanung") == nullptr);
    assert(FindCommand(commands, L"Grafische \u00dcberschreibungsregel...")->liveId == 407);
    std::cout << "[PASS] TestListTooShortNotOffered\n";
}

static std::vector<MenuNode> ListMenu() {
    return { ItemNode(L"A", 11), SeparatorNode(), ItemNode(L"Placeholder", kGraphicOverridePlaceholder),
             SeparatorNode(), ItemNode(L"B", 12) };
}

void TestListWithoutRegisteredPlaceholderStaysUnlinked() {
    const std::vector<int> spec = { 1, S, 10, 11, 12, S, 2 };
    const std::vector<DynamicList> notRegistered[] = {
        {},
        { { kModelViewPlaceholder, Wide({ L"X", L"Y", L"Z" }) } },
        // Two lists for one placeholder: which one is meant?
        { { kGraphicOverridePlaceholder, Wide({ L"X", L"Y", L"Z" }) },
          { kGraphicOverridePlaceholder, Wide({ L"X", L"Y", L"Z" }) } } };
    for (const auto& lists : notRegistered) {
        auto commands = LinkOne(ListMenu(), spec, lists);
        assert((LinkedNames(commands) == std::vector<std::wstring>{ L"A", L"B" }));
        assert((LinkedIds(commands) == std::vector<unsigned>{ 1, 2 }));
    }
    std::cout << "[PASS] TestListWithoutRegisteredPlaceholderStaysUnlinked\n";
}

void TestListNeedsOnlyItems() {
    const std::vector<DynamicList> lists = { { kGraphicOverridePlaceholder, Wide({ L"X", L"Y", L"Z" }) } };
    auto commands = LinkOne(ListMenu(), { 1, S, 10, P, 12, S, 2 }, lists);
    assert((LinkedNames(commands) == std::vector<std::wstring>{ L"A", L"B" }));
    // Two separators in a row leave an empty first segment, which is no list
    commands = LinkOne(ListMenu(), { 1, S, S, 10, 11, S, 2 }, lists);
    assert((LinkedNames(commands) == std::vector<std::wstring>{ L"A", L"B" }));
    std::cout << "[PASS] TestListNeedsOnlyItems\n";
}

void TestFitScoreRejectsOtherLanguage() {
    auto bar = SchemeReader::ReadFloorPlanMenuBar(ReadFixture());
    FakeMenu live({ L"View", L"Document" });
    live.Set({ 0 }, kMeasuredAnsicht);
    assert(MenuLink::FitScore(bar, live) == -1);
    assert(MenuLink::Link(bar, live, {}).empty());
    std::cout << "[PASS] TestFitScoreRejectsOtherLanguage\n";
}

void TestFitScoreCountsLinks() {
    auto bar = AnsichtBar();
    FakeMenu live = AnsichtFake(kMeasuredAnsicht);
    int score = MenuLink::FitScore(bar, live);
    assert(score > 0);
    assert(static_cast<size_t>(score) == MenuLink::Link(bar, live, {}).size());
    FakeMenu unreadable({ L"Ablage", L"Ansicht" });
    assert(MenuLink::FitScore(bar, unreadable) == 0);
    std::cout << "[PASS] TestFitScoreCountsLinks\n";
}

void TestTopLevelTitlesMustMatchExactly() {
    auto bar = AnsichtBar();
    FakeMenu none({});
    assert(MenuLink::FitScore(bar, none) == -1 && MenuLink::Link(bar, none, {}).empty());

    FakeMenu fewer({ L"Ablage" });
    fewer.Set({ 0 }, { 77 });
    assert(MenuLink::FitScore(bar, fewer) == -1 && MenuLink::Link(bar, fewer, {}).empty());

    FakeMenu more({ L"Ablage", L"Ansicht", L"Extras" });
    more.Set({ 0 }, { 77 });
    more.Set({ kAnsichtIndex }, kMeasuredAnsicht);
    assert(MenuLink::FitScore(bar, more) == -1 && MenuLink::Link(bar, more, {}).empty());

    FakeMenu reordered({ L"Ansicht", L"Ablage" });
    reordered.Set({ 0 }, kMeasuredAnsicht);
    assert(MenuLink::FitScore(bar, reordered) == -1 && MenuLink::Link(bar, reordered, {}).empty());
    std::cout << "[PASS] TestTopLevelTitlesMustMatchExactly\n";
}

void TestEmptyBarDoesNotFit() {
    FakeMenu none({});
    assert(MenuLink::FitScore({}, none) == -1);
    assert(MenuLink::Link({}, none, {}).empty());
    std::cout << "[PASS] TestEmptyBarDoesNotFit\n";
}

void TestTypeMismatchDropsSegment() {
    std::vector<MenuNode> bar = { ItemNode(L"A", 11), ItemNode(L"B", 12), SeparatorNode(), ItemNode(L"C", 13), ItemNode(L"D", 14) };
    auto commands = LinkOne(bar, { 1, P, S, 3, 4 });
    assert((LinkedNames(commands) == std::vector<std::wstring>{ L"C", L"D" }));

    // The reverse: a popup in the catalogue where the live menu has an item
    std::vector<MenuNode> withPopup = { PopupNode(L"Sub", { ItemNode(L"Inner", 20) }), ItemNode(L"B", 12) };
    FakeMenu live = OneLive({ 1, 2 });
    live.Set({ 0, 0 }, { 9 });
    assert(MenuLink::Link(OneMenu(withPopup), live, {}).empty());
    std::cout << "[PASS] TestTypeMismatchDropsSegment\n";
}

void TestTypeMismatchAtAnchorDropsSegment() {
    std::vector<MenuNode> bar = { ItemNode(L"A", 11), ItemNode(L"Placeholder", kAddOnPlaceholder), ItemNode(L"B", 12) };
    assert(LinkOne(bar, { 1, 5, 6, P }).empty());
    assert(LinkOne(bar, { P, 5, 6, 2 }).empty());
    std::cout << "[PASS] TestTypeMismatchAtAnchorDropsSegment\n";
}

void TestMismatchedMenuLinksNothing() {
    std::vector<MenuNode> bar = { ItemNode(L"A", 11), SeparatorNode(), ItemNode(L"B", 12) };
    assert(LinkOne(bar, { 1, 2 }).empty());           // separator missing
    assert(LinkOne(bar, { 1, S, 2, S, 3 }).empty());  // separator added
    assert(LinkOne(bar, {}).empty());                 // live menu unreadable
    assert(LinkOne(bar, { 1, S, 2 }).size() == 2);
    std::cout << "[PASS] TestMismatchedMenuLinksNothing\n";
}

void TestPlaceholderAtSegmentStart() {
    std::vector<MenuNode> bar = { ItemNode(L"Placeholder", kAddOnPlaceholder), ItemNode(L"A", 11), ItemNode(L"B", 12) };
    assert((LinkedIds(LinkOne(bar, { 2, 3 })) == std::vector<unsigned>{ 2, 3 }));             // placeholder dropped
    assert((LinkedIds(LinkOne(bar, { 7, 8, 9, 2, 3 })) == std::vector<unsigned>{ 2, 3 }));    // replaced by three entries
    assert((LinkedIds(LinkOne(bar, { 7, 2, 3 })) == std::vector<unsigned>{ 2, 3 }));          // replaced by one entry
    assert(LinkOne(bar, { 3 }).empty());                                                      // too short to anchor both
    std::cout << "[PASS] TestPlaceholderAtSegmentStart\n";
}

void TestPlaceholderAtSegmentEnd() {
    std::vector<MenuNode> bar = { ItemNode(L"A", 11), ItemNode(L"B", 12), ItemNode(L"Placeholder", kAddOnPlaceholder) };
    assert((LinkedIds(LinkOne(bar, { 1, 2 })) == std::vector<unsigned>{ 1, 2 }));
    assert((LinkedIds(LinkOne(bar, { 1, 2, 7, 8, 9 })) == std::vector<unsigned>{ 1, 2 }));
    assert(LinkOne(bar, { 1 }).empty());
    std::cout << "[PASS] TestPlaceholderAtSegmentEnd\n";
}

void TestOnePlaceholderEqualCount() {
    std::vector<MenuNode> bar = { ItemNode(L"A", 11), ItemNode(L"Placeholder", kAddOnPlaceholder), ItemNode(L"B", 12) };
    auto commands = LinkOne(bar, { 1, 2, 3 });
    assert((LinkedNames(commands) == std::vector<std::wstring>{ L"A", L"B" }));
    assert((LinkedIds(commands) == std::vector<unsigned>{ 1, 3 }));
    assert((LinkedIds(LinkOne(bar, { 1, 2, 3, 4 })) == std::vector<unsigned>{ 1, 4 }));
    assert(LinkOne(bar, { 1 }).empty());
    std::cout << "[PASS] TestOnePlaceholderEqualCount\n";
}

void TestTwoPlaceholdersInOneSegment() {
    std::vector<MenuNode> bar = { ItemNode(L"A", 11), ItemNode(L"Placeholder 1", kAddOnPlaceholder), ItemNode(L"B", 12),
                                  ItemNode(L"Placeholder 2", kSecondAddOnPlaceholder), ItemNode(L"C", 13) };
    // Equal count, but one placeholder may have grown while the other vanished: B is in doubt
    assert((LinkedIds(LinkOne(bar, { 1, 2, 3, 4, 5 })) == std::vector<unsigned>{ 1, 5 }));
    assert((LinkedIds(LinkOne(bar, { 1, 5 })) == std::vector<unsigned>{ 1, 5 }));
    assert((LinkedIds(LinkOne(bar, { 1, 2, 3, 4, 5, 6 })) == std::vector<unsigned>{ 1, 6 }));
    assert(LinkOne(bar, { 1 }).empty());
    std::cout << "[PASS] TestTwoPlaceholdersInOneSegment\n";
}

void TestPlaceholderIsNeverOffered() {
    std::vector<MenuNode> bar = { ItemNode(L"A", 11), ItemNode(L"Keine Stift-Sets", 1551), ItemNode(L"B", 12) };
    auto commands = LinkOne(bar, { 1, 2, 3 });
    assert(FindCommand(commands, L"Keine Stift-Sets") == nullptr && commands.size() == 2);
    std::cout << "[PASS] TestPlaceholderIsNeverOffered\n";
}

void TestPlaceholderSegmentGoneLinksNothing() {
    // The whole segment of the placeholder is missing, so neither separator can be told from the other
    assert(LinkOne(ListMenu(), { 1, S, 2 }).empty());
    assert(LinkOne(ListMenu(), { 1, 2 }).empty());
    std::cout << "[PASS] TestPlaceholderSegmentGoneLinksNothing\n";
}

void TestPlaceholderRegionSizeMismatch() {
    std::vector<MenuNode> bar = { ItemNode(L"A", 11), SeparatorNode(), ItemNode(L"Placeholder 1", kAddOnPlaceholder), SeparatorNode(),
                                  ItemNode(L"B", 12), SeparatorNode(), ItemNode(L"Placeholder 2", kSecondAddOnPlaceholder),
                                  SeparatorNode(), ItemNode(L"D", 14) };
    // Three catalogue segments between the outer ones, four live ones: B cannot be placed
    auto commands = LinkOne(bar, { 1, S, 5, S, 6, S, 7, S, 8, S, 4 });
    assert((LinkedNames(commands) == std::vector<std::wstring>{ L"A", L"D" }));
    // Same number of segments, but it cannot show which is which: 2 may be a list entry that
    // spilled over, with the second placeholder gone and B being 6
    commands = LinkOne(bar, { 1, S, 5, S, 2, S, 6, S, 4 });
    assert((LinkedNames(commands) == std::vector<std::wstring>{ L"A", L"D" }));
    assert((LinkedIds(commands) == std::vector<unsigned>{ 1, 4 }));
    std::cout << "[PASS] TestPlaceholderRegionSizeMismatch\n";
}

void TestDynamicItemIsNeverOffered() {
    std::vector<MenuNode> bar = { ItemNode(L"A", 11), ItemNode(L"Dynamic", kModifierDependent), ItemNode(L"B", 12) };
    auto commands = LinkOne(bar, { 1, 2, 3 });
    assert((LinkedNames(commands) == std::vector<std::wstring>{ L"A", L"B" }));
    assert((LinkedIds(commands) == std::vector<unsigned>{ 1, 3 }));
    commands = LinkOne(bar, { 1, 2, 3, 4 });
    assert((LinkedIds(commands) == std::vector<unsigned>{ 1, 4 }));
    assert(FindCommand(commands, L"Dynamic") == nullptr);
    std::cout << "[PASS] TestDynamicItemIsNeverOffered\n";
}

static FILETIME FileTimeFrom(unsigned long long ticks) {
    FILETIME time;
    time.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFFull);
    time.dwHighDateTime = static_cast<DWORD>(ticks >> 32);
    return time;
}

void TestStaleExport() {
    // Across the 32-bit boundary of the low word
    const unsigned long long ticks = 0x1FFFFFFFFull;
    FILETIME exportTime = FileTimeFrom(ticks);
    assert(MenuLink::IsExportStale(exportTime, { FileTimeFrom(ticks - 1), FileTimeFrom(ticks + 1) }));
    assert(!MenuLink::IsExportStale(exportTime, { FileTimeFrom(ticks - 1) }));
    assert(!MenuLink::IsExportStale(exportTime, {}));
    assert(!MenuLink::IsExportStale(exportTime, { FileTimeFrom(ticks) }));
    std::cout << "[PASS] TestStaleExport\n";
}

int main() {
    TestReadsFloorPlanBarOnly();
    TestRulerPosition();
    TestIdrefResolved();
    TestPlaceholderNumbers();
    TestNamesUnescaped();
    TestBomCommentsAndCrlf();
    TestMalformedIsEmpty();
    TestReferenceCycleIsEmpty();
    TestRejectionNeverYieldsPartialTree();
    TestDepthLimitNotCycle();
    TestNodeBudgetStopsExpansion();
    TestInputSizeLimit();
    TestExactTreeShape();
    TestPicksBarByGuidNotOrder();
    TestDuplicateBarGuidIsEmpty();
    TestAmbiguousInputIsEmpty();
    TestLinksRuler();
    TestLinksAreOrderedByPosition();
    TestAnchorsFromSegmentEnd();
    TestCountMismatchSkipsSegment();
    TestDisabledCarriedOver();
    TestRecursesIntoLinkedPopups();
    TestGraphicOverrideList();
    TestModelViewOptionsList();
    TestListTooShortNotOffered();
    TestListWithoutRegisteredPlaceholderStaysUnlinked();
    TestListNeedsOnlyItems();
    TestFitScoreRejectsOtherLanguage();
    TestFitScoreCountsLinks();
    TestTopLevelTitlesMustMatchExactly();
    TestEmptyBarDoesNotFit();
    TestTypeMismatchDropsSegment();
    TestTypeMismatchAtAnchorDropsSegment();
    TestMismatchedMenuLinksNothing();
    TestPlaceholderAtSegmentStart();
    TestPlaceholderAtSegmentEnd();
    TestOnePlaceholderEqualCount();
    TestTwoPlaceholdersInOneSegment();
    TestPlaceholderIsNeverOffered();
    TestPlaceholderSegmentGoneLinksNothing();
    TestPlaceholderRegionSizeMismatch();
    TestDynamicItemIsNeverOffered();
    TestStaleExport();
    std::cout << "All command line tests passed\n";
    return 0;
}
