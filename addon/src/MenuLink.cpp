#include "MenuLink.hpp"
#include <algorithm>
#include <iterator>
#include <utility>

namespace Baaz {

// Command numbers of the AC29 standard scheme whose item Archicad replaces at run time. Found by
// the German names of the built-in items (MDID 0) that start with "Keine ", "Kein ", "Letzte" or
// "Add-On" (which also covers "Add-On-"); this includes graphic override combinations (23040) and
// model view options (9472). The numbers do not depend on the language. 1027, 1293 and 20754 are
// static commands that match by name only; they stay in the table, so they are never offered,
// which is the safe side. Sorted for binary search.
static const unsigned kPlaceholderCommands[] = {
    522, 542, 550, 551, 778, 787, 794, 1027, 1293, 1549, 1551, 1802,
    1803, 2055, 2578, 2817, 2825, 2826, 2832, 2833, 2834, 2835, 2836, 2841,
    2848, 2851, 3331, 3332, 3333, 3356, 5901, 6409, 6658, 6660, 8196, 8449,
    8450, 8706, 8962, 9218, 9472, 9986, 9987, 10008, 10018, 10019, 10020, 10027,
    10028, 10034, 10497, 10754, 10755, 11010, 11011, 11266, 11267, 11522, 11523, 11778,
    11779, 12034, 12035, 14341, 15105, 16129, 16641, 16642, 17923, 17924, 20754, 20994,
    20995, 21250, 21251, 21506, 21507, 21768, 22018, 22019, 22530, 22785, 23040
};

static const wchar_t* const kPathSeparator = L" \u203a ";

static bool IsPlaceholder(const MenuNode& node) {
    return node.type == MenuNodeType::Item &&
           std::binary_search(std::begin(kPlaceholderCommands), std::end(kPlaceholderCommands), node.commandNumber);
}

namespace {

// Entries [begin, end) between two separators
struct Segment {
    size_t begin = 0;
    size_t end = 0;
    size_t size() const { return end - begin; }
};

// Indices into the catalogue entries and the live entries of one menu
struct EntryPair {
    size_t catalogue = 0;
    size_t live = 0;
};

// Offsets of the placeholders within a segment
struct PlaceholderSpan {
    size_t count = 0;
    size_t first = 0;
    size_t last = 0;
};

struct LinkRun {
    MenuSource& source;
    const std::vector<DynamicList>& lists;
    std::vector<LinkedCommand> commands;
};

} // namespace

template <typename Entry>
static std::vector<Segment> SplitSegments(const std::vector<Entry>& entries) {
    std::vector<Segment> segments;
    size_t begin = 0;
    for (size_t i = 0; i <= entries.size(); ++i) {
        if (i == entries.size() || entries[i].type == MenuNodeType::Separator) {
            segments.push_back({ begin, i });
            begin = i + 1;
        }
    }
    return segments;
}

static void AddFromStart(std::vector<EntryPair>& pairs, const Segment& catalogue, const Segment& live, size_t count) {
    for (size_t i = 0; i < count; ++i) pairs.push_back({ catalogue.begin + i, live.begin + i });
}

static void AddFromEnd(std::vector<EntryPair>& pairs, const Segment& catalogue, const Segment& live, size_t count) {
    for (size_t i = 1; i <= count; ++i) pairs.push_back({ catalogue.end - i, live.end - i });
}

namespace {

// One menu: pairs its segments, then its entries, and descends into the popups it could pair
class MenuLevel {
public:
    MenuLevel(LinkRun& run, const std::vector<MenuNode>& catalogue, std::vector<int> path, std::wstring menuPath)
        : m_run(run), m_catalogue(catalogue), m_path(std::move(path)), m_menuPath(std::move(menuPath)),
          m_live(run.source.Entries(m_path)), m_catalogueSegments(SplitSegments(catalogue)),
          m_liveSegments(SplitSegments(m_live)) {}

    void Link() {
        LinkSegments();
        EmitPairs();
    }

private:
    PlaceholderSpan FindPlaceholders(const Segment& segment) const {
        PlaceholderSpan span;
        for (size_t i = segment.begin; i < segment.end; ++i) {
            if (!IsPlaceholder(m_catalogue[i])) continue;
            if (span.count == 0) span.first = i - segment.begin;
            span.last = i - segment.begin;
            ++span.count;
        }
        return span;
    }

    bool FindPlaceholderSegments(size_t& first, size_t& last) const {
        bool found = false;
        for (size_t i = 0; i < m_catalogueSegments.size(); ++i) {
            if (FindPlaceholders(m_catalogueSegments[i]).count == 0) continue;
            if (!found) first = i;
            last = i;
            found = true;
        }
        return found;
    }

    // Aligns from both ends up to the segments with placeholders. What Archicad builds in their
    // place (a list, or nothing) is the region in between.
    void LinkSegments() {
        const size_t count = m_catalogueSegments.size();
        size_t first = 0;
        size_t last = 0;
        if (!FindPlaceholderSegments(first, last)) {
            // Without a placeholder, a different number of segments means something else changed
            if (count == m_liveSegments.size()) PairRun(0, 0, count);
            return;
        }
        const size_t head = first;
        const size_t tail = count - 1 - last;
        // The region needs at least one live segment, or the two ends overlap
        if (m_liveSegments.size() < head + tail + 1) return;
        PairRun(0, 0, head);
        PairRun(count - tail, m_liveSegments.size() - tail, tail);
        LinkRegion(first, last, head, m_liveSegments.size() - head - tail);
    }

    void LinkRegion(size_t first, size_t last, size_t liveFirst, size_t liveCount) {
        if (first == last && IsLonePlaceholder(m_catalogueSegments[first])) {
            LinkList(m_catalogueSegments[first], m_liveSegments[liveFirst]);
            return;
        }
        if (liveCount == last - first + 1) PairRun(first, liveFirst, liveCount);
    }

    bool IsLonePlaceholder(const Segment& segment) const {
        return segment.size() == 1 && IsPlaceholder(m_catalogue[segment.begin]);
    }

    void PairRun(size_t catalogueFirst, size_t liveFirst, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            PairSegment(m_catalogueSegments[catalogueFirst + i], m_liveSegments[liveFirst + i]);
        }
    }

    void PairSegment(const Segment& catalogue, const Segment& live) {
        std::vector<EntryPair> pairs = SegmentPairs(catalogue, live);
        for (const EntryPair& pair : pairs) {
            if (m_catalogue[pair.catalogue].type != m_live[pair.live].type) return;
        }
        m_pairs.insert(m_pairs.end(), pairs.begin(), pairs.end());
    }

    std::vector<EntryPair> SegmentPairs(const Segment& catalogue, const Segment& live) const {
        std::vector<EntryPair> pairs;
        const PlaceholderSpan span = FindPlaceholders(catalogue);
        // One placeholder cannot change the count unless it became exactly one entry. With two,
        // one may have grown while the other vanished, so only the entries at the ends are sure.
        if (catalogue.size() == live.size() && span.count <= 1) {
            AddFromStart(pairs, catalogue, live, catalogue.size());
            return pairs;
        }
        if (span.count == 0) return pairs;
        const size_t before = span.first;
        const size_t after = catalogue.size() - 1 - span.last;
        if (live.size() < before + after) return pairs;
        AddFromStart(pairs, catalogue, live, before);
        AddFromEnd(pairs, catalogue, live, after);
        return pairs;
    }

    const DynamicList* FindList(unsigned placeholder) const {
        const DynamicList* found = nullptr;
        for (const DynamicList& list : m_run.lists) {
            if (list.placeholder != placeholder) continue;
            // Two lists for one placeholder: which one is meant?
            if (found != nullptr) return nullptr;
            found = &list;
        }
        return found;
    }

    void LinkList(const Segment& placeholder, const Segment& entries) {
        const DynamicList* list = FindList(m_catalogue[placeholder.begin].commandNumber);
        if (list == nullptr || entries.size() == 0 || list->names.size() < entries.size()) return;
        for (size_t i = entries.begin; i < entries.end; ++i) {
            if (m_live[i].type != MenuNodeType::Item) return;
        }
        for (size_t i = entries.begin; i < entries.end; ++i) {
            m_run.commands.push_back(MakeCommand(list->names[i - entries.begin], i));
        }
    }

    std::vector<int> Position(size_t liveIndex) const {
        std::vector<int> position = m_path;
        position.push_back(static_cast<int>(liveIndex));
        return position;
    }

    LinkedCommand MakeCommand(const std::wstring& name, size_t liveIndex) const {
        LinkedCommand command;
        command.name = name;
        command.menuPath = m_menuPath;
        command.position = Position(liveIndex);
        command.liveId = m_live[liveIndex].liveId;
        command.enabled = m_live[liveIndex].enabled;
        return command;
    }

    void EmitPairs() {
        for (const EntryPair& pair : m_pairs) {
            const MenuNode& node = m_catalogue[pair.catalogue];
            if (node.type == MenuNodeType::Popup) {
                MenuLevel(m_run, node.children, Position(pair.live), m_menuPath + kPathSeparator + node.name).Link();
            } else if (!IsPlaceholder(node)) {
                m_run.commands.push_back(MakeCommand(node.name, pair.live));
            }
        }
    }

    LinkRun& m_run;
    const std::vector<MenuNode>& m_catalogue;
    std::vector<int> m_path;
    std::wstring m_menuPath;
    std::vector<LiveEntry> m_live;
    std::vector<Segment> m_catalogueSegments;
    std::vector<Segment> m_liveSegments;
    std::vector<EntryPair> m_pairs;
};

} // namespace

static bool TitlesMatch(const std::vector<MenuNode>& bar, const std::vector<std::wstring>& titles) {
    if (bar.empty() || bar.size() != titles.size()) return false;
    for (size_t i = 0; i < bar.size(); ++i) {
        if (bar[i].type != MenuNodeType::Popup || bar[i].name != titles[i]) return false;
    }
    return true;
}

// false if the live menu bar is not the one the catalogue describes
static bool LinkBar(const std::vector<MenuNode>& bar, MenuSource& live, const std::vector<DynamicList>& lists,
             std::vector<LinkedCommand>& linked) {
    if (!TitlesMatch(bar, live.TopLevelTitles())) return false;
    LinkRun run{ live, lists, {} };
    for (size_t i = 0; i < bar.size(); ++i) {
        MenuLevel(run, bar[i].children, { static_cast<int>(i) }, bar[i].name).Link();
    }
    std::sort(run.commands.begin(), run.commands.end(),
              [](const LinkedCommand& a, const LinkedCommand& b) { return a.position < b.position; });
    linked = std::move(run.commands);
    return true;
}

std::vector<LinkedCommand> MenuLink::Link(const std::vector<MenuNode>& bar, MenuSource& live,
                                          const std::vector<DynamicList>& lists) {
    std::vector<LinkedCommand> linked;
    LinkBar(bar, live, lists, linked);
    return linked;
}

int MenuLink::FitScore(const std::vector<MenuNode>& bar, MenuSource& live) {
    std::vector<LinkedCommand> linked;
    if (!LinkBar(bar, live, {}, linked)) return -1;
    return static_cast<int>(linked.size());
}

bool MenuLink::IsExportStale(const FILETIME& exportTime, const std::vector<FILETIME>& schemeTimes) {
    for (const FILETIME& schemeTime : schemeTimes) {
        if (CompareFileTime(&schemeTime, &exportTime) > 0) return true;
    }
    return false;
}

} // namespace Baaz
