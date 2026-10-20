#include "view.h"
#include "i18n.h"

using namespace ftxui;

namespace tui {

namespace {

// Larguras fixas das colunas; o nome ocupa o restante.
constexpr int kIconWidth = 3;
constexpr int kVersionWidth = 20;
constexpr int kSizeWidth = 12;
constexpr int kDateWidth = 17;

Element cell(Element e, int width)
{
    return e | size(WIDTH, EQUAL, width);
}

Element maybeReflect(Element e, Box *box)
{
    return box ? e | reflect(*box) : e;
}

std::string sortMark(const ViewState &s, int column)
{
    if (s.sort.column != column)
        return std::string();
    const IconSet &ic = icons(s.iconMode);
    return std::string(" ") + (s.sort.ascending ? ic.sortAscending : ic.sortDescending);
}

const char *stateIcon(IconMode mode, const octo::Package &p)
{
    const IconSet &ic = icons(mode);
    switch (p.status) {
    case octo::Status::Locked:       return ic.locked;
    case octo::Status::Outdated:     return ic.outdated;
    case octo::Status::Newer:        return ic.newer;
    case octo::Status::NotInstalled: return ic.notInstalled;
    default:                         return p.automatic ? ic.dependency : ic.installed;
    }
}

Color stateColor(const octo::Package &p)
{
    switch (p.status) {
    case octo::Status::Locked:       return Color::Magenta;
    case octo::Status::Outdated:     return Color::Yellow;
    case octo::Status::Newer:        return Color::Cyan;
    case octo::Status::NotInstalled: return Color::Default;
    default:                         return Color::Green;
    }
}

Element header(const ViewState &s, HitBoxes *boxes)
{
    const auto title = [&](int col) { return text(columnTitle(col) + sortMark(s, col)); };
    Box *b = boxes ? boxes->header.data() : nullptr;
    const std::string iconMark = sortMark(s, model::StateIcon);
    return hbox({
               maybeReflect(cell(text(iconMark.empty() ? " " : iconMark), kIconWidth),
                            b ? &b[model::StateIcon] : nullptr),
               maybeReflect(title(model::Name) | flex, b ? &b[model::Name] : nullptr),
               maybeReflect(cell(title(model::Version), kVersionWidth), b ? &b[model::Version] : nullptr),
               maybeReflect(cell(title(model::Size) | align_right, kSizeWidth), b ? &b[model::Size] : nullptr),
               text("  "),
               maybeReflect(cell(title(model::InstalledOn), kDateWidth), b ? &b[model::InstalledOn] : nullptr),
           })
        | bold;
}

Element row(const ViewState &s, size_t i, HitBoxes *boxes)
{
    const octo::Package &p = s.rows[i];
    const IconSet &ic = icons(s.iconMode);
    Element icon;
    if (s.toInstall.count(p.name))
        icon = text(std::string(" ") + ic.toInstall) | color(Color::Green) | bold;
    else if (s.toRemove.count(p.name))
        icon = text(std::string(" ") + ic.toRemove) | color(Color::Red) | bold;
    else {
        icon = text(std::string(" ") + stateIcon(s.iconMode, p));
        if (p.installed)
            icon = icon | color(stateColor(p));
    }
    // Na visão Repositório, como no OctoPkg, a versão é a do repositório.
    Element line = hbox({
        cell(icon, kIconWidth),
        text(p.name) | flex,
        cell(text(p.version), kVersionWidth),
        cell(text(octo::formatSize(p.size)) | align_right, kSizeWidth),
        text("  "),
        cell(text(octo::formatTime(p.installedOn)), kDateWidth),
    });
    if (!p.installed && !s.toInstall.count(p.name))
        line = line | dim;
    if (static_cast<int>(i) == s.selected)
        line = line | (s.contentFocused ? underlined : inverted);
    if (boxes)
        line = line | reflect(boxes->rows[i]);
    return line;
}

Element table(const ViewState &s, HitBoxes *boxes)
{
    if (boxes)
        boxes->rows.assign(s.rows.size(), Box{});
    Elements lines;
    for (size_t i = 0; i < s.rows.size(); ++i)
        lines.push_back(row(s, i, boxes));
    if (lines.empty())
        lines.push_back(text(s.busy ? std::string() : i18n::tr("No packages.")) | dim);
    return vbox({header(s, boxes), separatorLight(), vbox(std::move(lines)) | flex | yframe});
}

Element tabs(const ViewState &s, HitBoxes *boxes)
{
    const IconSet &ic = icons(s.iconMode);
    std::string files = std::string(ic.files) + " " + i18n::tr("Files");
    if (s.detailsReady && s.details.installed)
        files += " (" + std::to_string(s.details.files.size()) + ")";
    std::string actions = std::string(ic.actions) + " " + i18n::tr("Actions");
    const size_t marked = s.toInstall.size() + s.toRemove.size();
    if (marked)
        actions += " (" + std::to_string(marked) + ")";
    const std::string labels[TabCount] = {
        std::string(ic.info) + " " + i18n::tr("Info"),
        files,
        actions,
        std::string(ic.output) + " " + i18n::tr("Output"),
        std::string(ic.help) + " " + i18n::tr("Help"),
    };
    Elements items;
    for (int t = 0; t < TabCount; ++t) {
        Element e = text(std::string(" ") + tabKey(t) + " " + labels[t] + " ");
        e = t == s.tab ? e | inverted | bold : e | dim;
        if (boxes)
            e = e | reflect(boxes->tabs[t]);
        items.push_back(e);
        items.push_back(text(" "));
    }
    return hbox(std::move(items));
}

constexpr int kLabelWidth = 18;

Element field(const std::string &label, const std::string &value)
{
    return hbox({text(label + ":") | bold | size(WIDTH, EQUAL, kLabelWidth), text(value)});
}

// Lista ("Depende de", "Licenças"...): a primeira linha com o rótulo.
void listField(Elements &out, const std::string &label, const std::vector<std::string> &items)
{
    if (items.empty())
        return;
    for (size_t i = 0; i < items.size(); ++i)
        out.push_back(i == 0 ? field(label, items[i]) : hbox({text("") | size(WIDTH, EQUAL, kLabelWidth),
                                                             text(items[i])}));
}

Elements infoLines(const ViewState &s)
{
    Elements out;
    if (!s.hasCurrent) {
        out.push_back(text(i18n::tr("Select a package to see details.")) | dim);
        return out;
    }
    const octo::Package &p = s.current;
    out.push_back(hbox({text(p.name) | bold, text("  " + p.comment)}));
    out.push_back(hbox({text(i18n::tr("Status") + ":") | bold | size(WIDTH, EQUAL, kLabelWidth),
                        text(statusText(p)) | color(stateColor(p))}));
    if (!s.detailsError.empty()) {
        out.push_back(text(i18n::trf("ERROR: %1", {s.detailsError})) | color(Color::Red));
        return out;
    }
    if (!s.detailsReady) {
        out.push_back(text(i18n::tr("Loading…")) | dim);
        return out;
    }
    const octo::Details &d = s.details;
    if (p.installed && !p.installedVersion.empty() && p.installedVersion != p.version)
        out.push_back(field(i18n::tr("Installed version"), p.installedVersion));
    out.push_back(field(i18n::tr("Version"), d.version));
    out.push_back(field(i18n::tr("Origin"), d.origin));
    out.push_back(field(i18n::tr("Repository"), d.repository));
    out.push_back(field(i18n::tr("URL"), d.www));
    listField(out, i18n::tr("Licenses"), d.licenses);
    out.push_back(field(i18n::tr("Maintainer"), d.maintainer));
    out.push_back(field(i18n::tr("Architecture"), d.arch));
    out.push_back(field(i18n::tr("Installed size"), octo::formatSize(d.size)));
    if (d.installedOn > 0)
        out.push_back(field(i18n::tr("Installed on"), octo::formatTime(d.installedOn)));
    listField(out, i18n::tr("Depends on"), d.dependencies);
    listField(out, i18n::tr("Required by"), d.requiredBy);
    listField(out, i18n::tr("Options"), d.options);
    if (!d.description.empty()) {
        out.push_back(text(""));
        size_t start = 0;
        while (start <= d.description.size()) {
            size_t end = d.description.find('\n', start);
            if (end == std::string::npos)
                end = d.description.size();
            out.push_back(text(d.description.substr(start, end - start)));
            start = end + 1;
        }
    }
    return out;
}

Elements filesLines(const ViewState &s)
{
    Elements out;
    if (!s.hasCurrent)
        out.push_back(text(i18n::tr("Select a package to see details.")) | dim);
    else if (!s.current.installed)
        out.push_back(text(i18n::tr("Files are listed for installed packages only.")) | dim);
    else if (!s.detailsError.empty())
        out.push_back(text(i18n::trf("ERROR: %1", {s.detailsError})) | color(Color::Red));
    else if (!s.detailsReady)
        out.push_back(text(i18n::tr("Loading…")) | dim);
    else if (s.details.filesTruncated)
        out.push_back(text(i18n::tr("This package has too many files to be listed.")) | dim);
    else if (s.details.files.empty())
        out.push_back(text(i18n::tr("No files.")) | dim);
    else
        for (const auto &f : s.details.files)
            out.push_back(text(f));
    return out;
}

Elements helpLines()
{
    const auto key = [](const std::string &k, const std::string &what) {
        return hbox({text(k) | bold | size(WIDTH, EQUAL, 24), text(what)});
    };
    return {
        key("F5 / Ctrl+R", i18n::tr("Refresh the package list")),
        key("↑ ↓ PgUp PgDn Home End", i18n::tr("Move")),
        key("Ctrl+L  /", i18n::tr("Find a package in the package list")),
        key("Esc  Enter  Ctrl+P", i18n::tr("Go back to the package list")),
        key("i  Alt+I", i18n::tr("Switch between installed and repository packages")),
        key("+  Insert", i18n::tr("Mark the selected package for installation")),
        key("-  Delete", i18n::tr("Mark the selected package for removal")),
        key("Alt+M", i18n::tr("Apply: install and remove the marked packages")),
        key("Ctrl+E", i18n::tr("Cancel: clear the marks")),
        key("Ctrl+K", i18n::tr("Check updates (pkg update)")),
        key("Ctrl+U", i18n::tr("Install updates (pkg upgrade)")),
        key("F9  Alt+A Alt+C Alt+J", i18n::tr("Menus File, Actions and Help")),
        key("F6", i18n::tr("Choose the sort column (again: reverse; (pkg order): original order)")),
        key("1 2 3 4 6  Alt+1…Alt+6", i18n::tr("Tabs Info, Files, Actions, Output and Help")),
        key("F1", i18n::tr("Show this help page")),
        key("Enter", i18n::tr("Show the Info tab of the selected package")),
        key("Tab", i18n::tr("Scroll the tab with the arrow keys")),
        key("Ctrl+Q  F10  q", i18n::tr("Exit")),
        text(i18n::tr("Click a column header to sort")) | dim,
    };
}

Elements actionsLines(const ViewState &s)
{
    Elements out;
    if (s.toInstall.empty() && s.toRemove.empty()) {
        out.push_back(text(i18n::tr("No packages marked. Use + to install and - to remove.")) | dim);
        return out;
    }
    const IconSet &ic = icons(s.iconMode);
    const auto section = [&](const std::string &title, const std::set<std::string> &names, const char *icon,
                             Color c) {
        if (names.empty())
            return;
        out.push_back(text(title + " (" + std::to_string(names.size()) + ")") | bold);
        for (const auto &n : names)
            out.push_back(hbox({text(std::string("  ") + icon + " ") | color(c), text(n)}));
    };
    section(i18n::tr("To be installed"), s.toInstall, ic.toInstall, Color::Green);
    section(i18n::tr("To be removed"), s.toRemove, ic.toRemove, Color::Red);
    out.push_back(text(i18n::tr("Alt+M: apply   Ctrl+E: cancel")) | dim);
    return out;
}

Elements outputLines(const ViewState &s)
{
    Elements out;
    for (const auto &l : s.log)
        out.push_back(text(l));
    return out;
}

Element tabContent(const ViewState &s, HitBoxes *boxes)
{
    Elements lines;
    switch (s.tab) {
    case FilesTab:  lines = filesLines(s); break;
    case ActionsTab: lines = actionsLines(s); break;
    case OutputTab: lines = outputLines(s); break;
    case HelpTab:   lines = helpLines(); break;
    default:        lines = infoLines(s); break;
    }
    const int count = static_cast<int>(lines.size());
    if (boxes)
        boxes->contentLines = count;
    // A Saída mostra o fim, a menos que o usuário a role.
    int first = s.contentScroll;
    if (s.tab == OutputTab && !s.contentFocused)
        first = count - s.contentHeight;
    first = std::max(0, std::min(first, count - 1));
    Elements shown(lines.begin() + std::min(first, count), lines.end());
    Element body = vbox(std::move(shown));
    if (s.contentFocused)
        body = hbox({body | flex, text(std::to_string(std::min(first + 1, count)) + "/" + std::to_string(count))
                                      | dim});
    return body | size(HEIGHT, EQUAL, s.contentHeight) | yframe;
}

Element statusBar(const ViewState &s)
{
    const IconSet &ic = icons(s.iconMode);
    std::string left = s.status;
    if (s.busy)
        left = std::string(ic.busy) + " " + left;
    else if (s.error)
        left = std::string(ic.error) + " " + left;
    Element l = text(left);
    if (s.error)
        l = l | color(Color::Red);
    Elements right;
    if (!s.toInstall.empty())
        right.push_back(text("+" + std::to_string(s.toInstall.size()) + "  ") | color(Color::Green));
    if (!s.toRemove.empty())
        right.push_back(text("-" + std::to_string(s.toRemove.size()) + "  ") | color(Color::Red));
    right.push_back(text(i18n::trf("%1 package(s)", {std::to_string(s.totalPackages)})));
    if (s.outdatedCount > 0)
        right.push_back(text("  " + i18n::trf("%1 outdated", {std::to_string(s.outdatedCount)}))
                        | color(Color::Yellow));
    Elements all{l | flex};
    for (auto &e : right)
        all.push_back(e);
    return hbox(std::move(all));
}

Element keyBar()
{
    const auto key = [](const char *k, const std::string &label) {
        return hbox({text(k) | bold, text(label + " ") | inverted});
    };
    // Como no Midnight Commander (e no bectl-tui): teclas de função numeradas.
    return hbox({key("F1", i18n::tr("Help")), text(" "), key("F5", i18n::tr("Refresh")), text(" "),
                 key("F6", i18n::tr("Sort")), text(" "), key("F9", i18n::tr("Menu")), text(" "),
                 key("F10", i18n::tr("Exit")), text(" "), key("+/-", i18n::tr("Mark")), text(" "),
                 key("M-m", i18n::tr("Apply")), filler()});
}

Element button(const std::string &label, bool focused, bool danger)
{
    Element b = text("[ " + label + " ]");
    if (focused)
        b = b | inverted | bold;
    if (danger && focused)
        b = b | color(Color::Red);
    return b;
}

Elements paragraph(const std::string &message)
{
    Elements lines;
    size_t start = 0;
    for (;;) {
        const size_t nl = message.find('\n', start);
        lines.push_back(text(message.substr(start, nl == std::string::npos ? std::string::npos : nl - start)));
        if (nl == std::string::npos)
            break;
        start = nl + 1;
    }
    return lines;
}

Element formDialog(const Dialog &d)
{
    Elements rows;
    if (!d.message.empty()) {
        rows = paragraph(d.message);
        rows.push_back(text(""));
    }
    for (size_t i = 0; i < d.fields.size(); ++i) {
        const DialogField &f = d.fields[i];
        const bool focused = static_cast<int>(i) == d.focus;
        Element value;
        switch (f.kind) {
        case DialogField::Text:
            value = hbox({text(f.text), focused ? text("█") | blink : text("")}) | size(WIDTH, GREATER_THAN, 30)
                    | borderLight;
            break;
        case DialogField::Check:
            value = text(std::string(f.checked ? "[x] " : "[ ] ") + f.label);
            break;
        case DialogField::Choice:
            value = text("< " + (f.choices.empty() ? std::string() : f.choices[f.choice]) + " >")
                    | size(WIDTH, GREATER_THAN, 30);
            break;
        }
        if (focused && f.kind != DialogField::Text)
            value = value | inverted;
        if (f.kind == DialogField::Check)
            rows.push_back(value);
        else
            rows.push_back(hbox({text(f.label) | size(WIDTH, EQUAL, 28), value}));
        if (!f.hint.empty())
            rows.push_back(text("    " + f.hint) | dim);
    }
    const int ok = static_cast<int>(d.fields.size());
    rows.push_back(text(""));
    rows.push_back(hbox({filler(), button(i18n::tr("OK"), d.focus == ok, d.danger), text("  "),
                         button(i18n::tr("Cancel"), d.focus == ok + 1, false)}));
    rows.push_back(text(i18n::tr("Tab / ↑ ↓: field   Space: check   Enter: confirm   Esc: cancel")) | dim);
    Element w = window(text(" " + d.title + " ") | bold, vbox(std::move(rows)));
    return d.danger ? w | color(Color::Red) : w;
}

Element confirmDialog(const Dialog &d)
{
    Elements rows = paragraph(d.message);
    rows.push_back(text(""));
    rows.push_back(hbox({filler(), button(i18n::tr("Yes"), d.focus == 0, d.danger), text("  "),
                         button(i18n::tr("No"), d.focus == 1, false)}));
    Element w = window(text(" " + d.title + " ") | bold, vbox(std::move(rows)));
    return d.danger ? w | color(Color::Red) : w;
}

Element messageDialog(const Dialog &d)
{
    Elements rows = paragraph(d.message);
    rows.push_back(text(""));
    rows.push_back(hbox({filler(), button(i18n::tr("OK"), true, false)}));
    return window(text(" " + d.title + " ") | bold, vbox(std::move(rows)));
}

Element menuDialog(const Dialog &d)
{
    Elements titles;
    for (size_t m = 0; m < d.menus.size(); ++m) {
        Element t = text(" " + d.menus[m] + " ");
        titles.push_back(static_cast<int>(m) == d.menu ? t | inverted | bold : t);
    }
    Elements items;
    if (d.menu >= 0 && d.menu < static_cast<int>(d.items.size())) {
        const auto &list = d.items[d.menu];
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].first.empty()) {
                items.push_back(separatorLight());
                continue;
            }
            Element e = hbox({text(" " + list[i].first) | flex, text("  " + list[i].second + " ") | dim});
            items.push_back(static_cast<int>(i) == d.item ? e | inverted : e);
        }
    }
    return vbox({hbox(std::move(titles)), window(text(""), vbox(std::move(items)) | size(WIDTH, GREATER_THAN, 40))});
}

Element sortChooser(const ViewState &s)
{
    Elements items;
    for (int i = 0; i <= model::ColumnCount; ++i) {
        const std::string label = i == 0 ? i18n::tr("(pkg order)") : columnTitle(i - 1) + sortMark(s, i - 1);
        Element e = text(" " + label + " ");
        if (i == s.sortChooserIndex)
            e = e | inverted;
        items.push_back(e);
    }
    return window(text(" " + i18n::tr("Sort by") + " "), vbox(std::move(items))) | clear_under | center;
}

} // namespace

char tabKey(int tab)
{
    static const char keys[TabCount] = {'1', '2', '3', '4', '6'};
    return tab >= 0 && tab < TabCount ? keys[tab] : '?';
}

std::string columnTitle(int column)
{
    switch (column) {
    case model::StateIcon:   return i18n::tr("Status");
    case model::Name:        return i18n::tr("Name");
    case model::Version:     return i18n::tr("Version");
    case model::Size:        return i18n::tr("Size");
    case model::InstalledOn: return i18n::tr("Installed on");
    default:                 return std::string();
    }
}

std::string statusText(const octo::Package &p)
{
    switch (p.status) {
    case octo::Status::Locked:
        return i18n::tr("Locked");
    case octo::Status::Outdated:
        return i18n::tr("Outdated") + " " + i18n::trf("(version %1 is available)", {p.newVersion});
    case octo::Status::Newer:
        return i18n::tr("Newer than the repository") + i18n::trf(" (newer installed version is %1)",
                                                                 {p.installedVersion});
    case octo::Status::NotInstalled:
        return i18n::tr("Not installed");
    default:
        return p.automatic ? i18n::tr("Installed as a dependency") : i18n::tr("Installed");
    }
}

Element render(const ViewState &s, Element filter, HitBoxes *boxes)
{
    const IconSet &ic = icons(s.iconMode);
    const std::string view = s.remoteView ? std::string(ic.remoteView) + " " + i18n::tr("Repository")
                                          : std::string(ic.installedView) + " " + i18n::tr("Installed packages");
    Element main = vbox({
        hbox({text(" OctoPkg — pkg(8) ") | bold, text("[" + view + "]") | color(Color::Cyan), filler(),
              text(std::string(ic.filter) + " "), filter | size(WIDTH, EQUAL, 30)}),
        separator(),
        table(s, boxes) | flex,
        separator(),
        tabs(s, boxes),
        tabContent(s, boxes),
        separator(),
        statusBar(s),
        keyBar(),
    });
    if (s.sortChooserOpen)
        return dbox({main, sortChooser(s)});
    switch (s.dialog.kind) {
    case Dialog::Form:    return dbox({main, formDialog(s.dialog) | clear_under | center});
    case Dialog::Confirm: return dbox({main, confirmDialog(s.dialog) | clear_under | center});
    case Dialog::Message: return dbox({main, messageDialog(s.dialog) | clear_under | center});
    case Dialog::Menu:    return dbox({main, menuDialog(s.dialog) | clear_under});
    case Dialog::None:    break;
    }
    return main;
}

} // namespace tui
