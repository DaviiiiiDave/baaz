// Keep assert() active in Release builds too
#undef NDEBUG
#include "SchemeReader.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
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

// A complete scheme with one menu bar, so edge cases only spell out the menus
static std::string FloorPlanBar(const std::string& menus) {
    return "<Scheme><GlobalSettings><CommandLayout>"
           "<MenuBar ID=\"B\" menubarguid=\"2085E4D3-5694-4BB9-ACAC-3AED5BE50091\" name=\"x\">" +
           menus + "</MenuBar></CommandLayout></GlobalSettings></Scheme>";
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

int main() {
    TestReadsFloorPlanBarOnly();
    TestRulerPosition();
    TestIdrefResolved();
    TestPlaceholderNumbers();
    TestNamesUnescaped();
    TestBomCommentsAndCrlf();
    TestMalformedIsEmpty();
    TestReferenceCycleIsEmpty();
    std::cout << "All command line tests passed\n";
    return 0;
}
