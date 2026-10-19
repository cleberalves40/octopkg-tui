#include "listmodel.h"

#include <algorithm>
#include <cctype>

namespace model {

namespace {

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Versões em pedaços numéricos e de texto: "1.10" > "1.9", como o
// rpmvercmp do OctoPkg compara.
int compareVersions(const std::string &a, const std::string &b)
{
    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        while (i < a.size() && !std::isalnum(static_cast<unsigned char>(a[i])))
            ++i;
        while (j < b.size() && !std::isalnum(static_cast<unsigned char>(b[j])))
            ++j;
        if (i >= a.size() || j >= b.size())
            break;
        const bool numeric = std::isdigit(static_cast<unsigned char>(a[i]));
        const auto isPart = [numeric](char c) {
            const auto u = static_cast<unsigned char>(c);
            return numeric ? std::isdigit(u) != 0 : std::isalpha(u) != 0;
        };
        size_t ie = i, je = j;
        while (ie < a.size() && isPart(a[ie]))
            ++ie;
        while (je < b.size() && isPart(b[je]))
            ++je;
        if (je == j)   // tipos diferentes: o número vence
            return numeric ? 1 : -1;
        std::string pa = a.substr(i, ie - i), pb = b.substr(j, je - j);
        if (numeric) {
            pa.erase(0, std::min(pa.find_first_not_of('0'), pa.size()));
            pb.erase(0, std::min(pb.find_first_not_of('0'), pb.size()));
            if (pa.size() != pb.size())
                return pa.size() < pb.size() ? -1 : 1;
        }
        if (const int c = pa.compare(pb))
            return c < 0 ? -1 : 1;
        i = ie;
        j = je;
    }
    const bool restA = i < a.size(), restB = j < b.size();
    return restA == restB ? 0 : (restA ? 1 : -1);
}

} // namespace

SortSpec nextOnHeaderClick(SortSpec current, int column)
{
    if (current.column != column)
        return {column, true};
    if (current.ascending)
        return {column, false};
    return {-1, true};
}

std::vector<int> visibleRows(const std::vector<octo::Package> &all, const std::string &filter, SortSpec sort)
{
    const std::string needle = lower(filter);
    std::vector<int> rows;
    std::vector<std::string> keys(all.size());   // nomes em minúsculas, calculados uma vez
    for (size_t i = 0; i < all.size(); ++i) {
        keys[i] = lower(all[i].name);
        if (needle.empty() || keys[i].find(needle) != std::string::npos)
            rows.push_back(static_cast<int>(i));
    }
    if (sort.column < 0 || sort.column >= ColumnCount)
        return rows;

    const auto less = [&](int x, int y) {
        const octo::Package &a = all[static_cast<size_t>(x)], &b = all[static_cast<size_t>(y)];
        switch (sort.column) {
        case StateIcon:   return a.status < b.status;
        case Name:        return keys[static_cast<size_t>(x)] < keys[static_cast<size_t>(y)];
        case Version:     return compareVersions(a.version, b.version) < 0;
        case Size:        return a.size < b.size;
        case InstalledOn: return a.installedOn < b.installedOn;
        default:          return false;
        }
    };
    std::stable_sort(rows.begin(), rows.end(),
                     [&](int x, int y) { return sort.ascending ? less(x, y) : less(y, x); });
    return rows;
}

int rowToSelect(const std::vector<octo::Package> &all, const std::vector<int> &rows, const std::string &previous)
{
    if (rows.empty())
        return -1;
    for (size_t i = 0; i < rows.size(); ++i)
        if (all[static_cast<size_t>(rows[i])].name == previous)
            return static_cast<int>(i);
    return 0;
}

int scrollOffset(int selected, int offset, int height, int count)
{
    if (height <= 0 || count <= 0)
        return 0;
    if (selected < 0)
        selected = 0;
    if (selected < offset)
        offset = selected;
    else if (selected >= offset + height)
        offset = selected - height + 1;
    return std::max(0, std::min(offset, std::max(0, count - height)));
}

} // namespace model
