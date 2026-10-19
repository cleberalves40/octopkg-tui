#include "app.h"
#include "i18n.h"

#include <algorithm>
#include <ctime>
#include <ftxui/component/component.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/terminal.hpp>

using namespace ftxui;

namespace tui {

namespace {

// Teclas que o FTXUI entrega como sequência crua.
const std::string kCtrlL = "\x0c";
const std::string kCtrlP = "\x10";
const std::string kCtrlQ = "\x11";
const std::string kCtrlR = "\x12";
const std::string kAltI = "\x1b" "i";

// Linhas fixas da tela: título, separadores, cabeçalho, abas, status e teclas.
constexpr int kFixedLines = 9;

} // namespace

App::App(const Options &options)
    : m_settings(options.settingsPath)
    , m_mgr(options.pkgPath)
    , m_iconMode(iconModeFromString(options.iconMode.empty() ? m_settings.value("icons", "nerd")
                                                             : options.iconMode))
    , m_remoteView(options.remote)
{
    const std::string lang = options.language.empty() ? m_settings.value("language", "system")
                                                      : options.language;
    if (lang == "system")
        i18n::initFromEnvironment();
    else
        i18n::setLocale(lang);
    m_sort.column = m_settings.intValue("sort.column", model::Name);
    if (m_sort.column < -1 || m_sort.column >= model::ColumnCount)
        m_sort.column = model::Name;
    m_sort.ascending = m_settings.value("sort.order", "asc") != "desc";
    m_status = i18n::tr("Ready.");
}

App::~App()
{
    if (m_worker.joinable())
        m_worker.join();
    if (m_detailWorker.joinable())
        m_detailWorker.join();
}

void App::log(const std::string &message)
{
    char ts[16];
    const std::time_t now = std::time(nullptr);
    std::tm tm {};
    ::localtime_r(&now, &tm);
    std::strftime(ts, sizeof ts, "%H:%M:%S", &tm);
    m_log.push_back(std::string("[") + ts + "] " + message);
    if (m_log.size() > 2000)
        m_log.erase(m_log.begin());
}

// Lista em segundo plano: a tela continua respondendo enquanto o pkg roda.
void App::refresh()
{
    if (m_busy)
        return;
    m_busy = true;
    m_error = false;
    const bool withRemote = m_remoteView;
    m_status = withRemote ? i18n::tr("Listing repository packages…") : i18n::tr("Listing installed packages…");
    if (m_worker.joinable())
        m_worker.join();
    m_worker = std::thread([this, withRemote] {
        Listing l;
        try {
            l.installed = m_mgr.listInstalled();
            l.outdated = m_mgr.listOutdated(&l.warning);
            if (withRemote) {
                l.remote = m_mgr.listRemote();
                l.hasRemote = true;
            }
        } catch (const std::exception &e) {
            l.error = e.what();
        }
        if (m_screen) {
            m_screen->Post([this, l] { applyListing(l); });
            m_screen->PostEvent(Event::Custom);
        } else {
            applyListing(l);
        }
    });
}

void App::applyListing(const Listing &l)
{
    m_busy = false;
    m_details.clear();
    m_detailErrors.clear();
    if (!l.error.empty()) {
        m_error = true;
        m_status = i18n::trf("ERROR: %1", {l.error});
        log(m_status);
        // Sem o catálogo dos repositórios, volta aos instalados.
        if (m_remoteView && !m_hasRemote && !l.installed.empty()) {
            m_remoteView = false;
        }
        if (l.installed.empty())
            return;
    }
    m_installed = l.installed;
    octo::applyUpgrades(m_installed, l.outdated);
    m_outdated = static_cast<int>(l.outdated.size());
    if (l.hasRemote) {
        m_remote = l.remote;
        m_hasRemote = true;
    }
    rebuild();

    if (l.error.empty()) {
        m_status = i18n::tr("Ready.");
        m_error = false;
    }
    log(i18n::trf("%1 package(s) installed.", {std::to_string(m_installed.size())}));
    if (l.hasRemote)
        log(i18n::trf("%1 package(s) in the repositories.", {std::to_string(m_remote.size())}));
    if (!l.warning.empty())
        log(i18n::trf("WARNING: %1", {l.warning}));
    if (!l.outdated.empty()) {
        log(i18n::trf("Outdated packages (%1):", {std::to_string(l.outdated.size())}));
        for (const auto &kv : l.outdated)
            log("  " + kv.first + "  " + kv.second.oldVersion + " -> " + kv.second.newVersion);
    }
}

void App::rebuild()
{
    m_all = (m_remoteView && m_hasRemote) ? octo::mergeRemote(m_remote, m_installed) : m_installed;
    m_rowsDirty = true;
}

void App::toggleView()
{
    if (m_busy) {
        m_status = i18n::tr("Wait for the current operation to finish.");
        return;
    }
    m_remoteView = !m_remoteView;
    if (m_remoteView && !m_hasRemote) {
        refresh();
        return;
    }
    rebuild();
}

const std::vector<int> &App::rows()
{
    if (m_rowsDirty || m_rowsFilter != m_filter || !(m_rowsSort == m_sort)) {
        m_rows = model::visibleRows(m_all, m_filter, m_sort);
        m_rowsFilter = m_filter;
        m_rowsSort = m_sort;
        m_rowsDirty = false;
    }
    return m_rows;
}

void App::layout()
{
    if (!m_screen)
        return;
    const int height = Terminal::Size().dimy;
    const int free = std::max(2, height - kFixedLines);
    m_contentHeight = std::max(3, std::min(16, free * 2 / 5));
    m_tableHeight = std::max(1, free - m_contentHeight);
}

std::string App::currentDetailsKey()
{
    const auto &r = rows();
    const int sel = model::rowToSelect(m_all, r, m_selectedName);
    if (sel < 0)
        return std::string();
    const octo::Package &p = m_all[static_cast<size_t>(r[static_cast<size_t>(sel)])];
    return (p.installed ? "I:" : "R:") + p.name;
}

void App::requestDetails()
{
    if (!m_screen || m_detailBusy)
        return;
    const std::string key = currentDetailsKey();
    if (key.empty() || m_details.count(key) || m_detailErrors.count(key))
        return;
    m_detailBusy = true;
    if (m_detailWorker.joinable())
        m_detailWorker.join();
    const bool installed = key[0] == 'I';
    const std::string name = key.substr(2);
    m_detailWorker = std::thread([this, key, name, installed] {
        octo::Details d;
        std::string error;
        try {
            d = m_mgr.details(name, installed);
        } catch (const std::exception &e) {
            error = e.what();
        }
        m_screen->Post([this, key, d, error] { applyDetails(key, d, error); });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::applyDetails(const std::string &key, const octo::Details &details, const std::string &error)
{
    m_detailBusy = false;
    if (m_details.size() > 256) {
        m_details.clear();
        m_detailErrors.clear();
    }
    if (error.empty())
        m_details[key] = details;
    else
        m_detailErrors[key] = error;
}

ViewState App::viewState()
{
    layout();
    ViewState s;
    const auto &r = rows();
    // A seleção acompanha o pacote, não a linha: sobrevive a filtro, ordem e atualização.
    const int sel = model::rowToSelect(m_all, r, m_selectedName);
    m_selectedName = sel >= 0 ? m_all[static_cast<size_t>(r[static_cast<size_t>(sel)])].name : std::string();
    // Outro pacote (seta, filtro, ordem ou atualização): a aba volta ao início.
    if (m_selectedName != m_shownName) {
        m_shownName = m_selectedName;
        m_contentScroll = 0;
    }
    const int count = static_cast<int>(r.size());
    m_offset = model::scrollOffset(sel, m_offset, m_tableHeight, count);
    for (int i = m_offset; i < count && i < m_offset + m_tableHeight; ++i)
        s.rows.push_back(m_all[static_cast<size_t>(r[static_cast<size_t>(i)])]);
    s.firstRow = m_offset;
    s.rowCount = count;
    s.selected = sel >= m_offset && sel < m_offset + m_tableHeight ? sel - m_offset : -1;
    if (sel >= 0) {
        s.hasCurrent = true;
        s.current = m_all[static_cast<size_t>(r[static_cast<size_t>(sel)])];
        const std::string key = (s.current.installed ? "I:" : "R:") + s.current.name;
        const auto d = m_details.find(key);
        if (d != m_details.end()) {
            s.detailsReady = true;
            s.details = d->second;
        }
        const auto e = m_detailErrors.find(key);
        if (e != m_detailErrors.end())
            s.detailsError = e->second;
    }
    s.sort = m_sort;
    s.remoteView = m_remoteView && m_hasRemote;
    s.tab = m_tab;
    s.contentFocused = m_contentFocused;
    s.contentScroll = m_contentScroll;
    s.contentHeight = m_contentHeight;
    s.log = m_log;
    s.status = m_status;
    s.busy = m_busy;
    s.error = m_error;
    s.totalPackages = static_cast<int>(m_all.size());
    s.outdatedCount = m_outdated;
    s.iconMode = m_iconMode;
    s.sortChooserOpen = m_sortChooser;
    s.sortChooserIndex = m_sortChooserIndex;
    return s;
}

void App::setSort(model::SortSpec sort)
{
    m_sort = sort;
    m_settings.setValue("sort.column", m_sort.column);
    m_settings.setValue("sort.order", m_sort.ascending ? "asc" : "desc");
    m_settings.save();
}

void App::move(int delta)
{
    if (m_contentFocused) {
        scrollContent(delta);
        return;
    }
    const auto &r = rows();
    if (r.empty())
        return;
    int i = model::rowToSelect(m_all, r, m_selectedName) + delta;
    i = std::max(0, std::min(i, static_cast<int>(r.size()) - 1));
    m_selectedName = m_all[static_cast<size_t>(r[static_cast<size_t>(i)])].name;
}

void App::scrollContent(int delta)
{
    const int last = std::max(0, m_boxes.contentLines - m_contentHeight);
    m_contentScroll = std::max(0, std::min(m_contentScroll + delta, last));
}

void App::setTab(int tab)
{
    m_tab = tab;
    m_contentScroll = 0;
    if (tab == OutputTab && m_contentFocused)
        m_contentScroll = std::max(0, static_cast<int>(m_log.size()) - m_contentHeight);
}

void App::quit()
{
    if (m_busy) {
        m_status = i18n::tr("Wait for the current operation to finish.");
        return;
    }
    if (m_screen)
        m_screen->Exit();
}

bool App::handleMouse(const Event &event)
{
    Event copy = event;   // mouse() não é const em todas as versões do FTXUI
    Mouse mouse = copy.mouse();
    if (mouse.button == Mouse::WheelUp) {
        move(-1);
        return true;
    }
    if (mouse.button == Mouse::WheelDown) {
        move(1);
        return true;
    }
    if (mouse.button != Mouse::Left || mouse.motion != Mouse::Pressed)
        return true;
    for (int c = 0; c < model::ColumnCount; ++c)
        if (m_boxes.header[c].Contain(mouse.x, mouse.y)) {
            setSort(model::nextOnHeaderClick(m_sort, c));
            return true;
        }
    for (int t = 0; t < TabCount; ++t)
        if (m_boxes.tabs[t].Contain(mouse.x, mouse.y)) {
            setTab(t);
            return true;
        }
    const auto &r = rows();
    for (size_t i = 0; i < m_boxes.rows.size(); ++i) {
        const size_t index = static_cast<size_t>(m_offset) + i;
        if (index < r.size() && m_boxes.rows[i].Contain(mouse.x, mouse.y)) {
            m_selectedName = m_all[static_cast<size_t>(r[index])].name;
            m_contentFocused = false;
            return true;
        }
    }
    return true;
}

bool App::handleEvent(const Event &event)
{
    // Escolha da coluna de ordenação (F6).
    if (m_sortChooser) {
        if (event == Event::ArrowUp || event == Event::Character('k'))
            m_sortChooserIndex = std::max(0, m_sortChooserIndex - 1);
        else if (event == Event::ArrowDown || event == Event::Character('j'))
            m_sortChooserIndex = std::min(static_cast<int>(model::ColumnCount), m_sortChooserIndex + 1);
        else if (event == Event::Return) {
            const int column = m_sortChooserIndex - 1;
            if (column < 0)
                setSort({-1, true});
            else if (column == m_sort.column)
                setSort({column, !m_sort.ascending});
            else
                setSort({column, true});
            m_sortChooser = false;
        } else if (event == Event::Escape || event == Event::F6)
            m_sortChooser = false;
        return true;
    }

    const std::string &raw = event.input();

    // Campo de filtro: Esc, Enter ou Ctrl+P devolvem o foco à lista; o resto vai ao Input.
    if (m_filterMode) {
        if (event == Event::Escape || event == Event::Return || raw == kCtrlP) {
            m_filterMode = false;
            return true;
        }
        if (event.is_mouse())
            return handleMouse(event);
        return false;
    }

    if (event.is_mouse())
        return handleMouse(event);

    if (event == Event::F5 || raw == kCtrlR) {
        refresh();
        return true;
    }
    if (event == Event::F6) {
        m_sortChooser = true;
        m_sortChooserIndex = m_sort.column + 1;
        return true;
    }
    if (raw == kCtrlL || event == Event::Character('/')) {
        m_filterMode = true;
        m_contentFocused = false;
        return true;
    }
    if (raw == kCtrlQ || event == Event::F10 || event == Event::Character('q')) {
        quit();
        return true;
    }
    if (raw == kAltI || event == Event::Character('i')) {
        toggleView();
        return true;
    }
    if (event == Event::F1) {
        setTab(HelpTab);
        return true;
    }
    for (char digit = '1'; digit <= '6'; ++digit) {
        const std::string d(1, digit);
        // Os números das abas do OctoPkg; Alt+n chega como ESC n.
        if (raw != d && raw != "\x1b" + d)
            continue;
        int tab = -1;
        for (int t = 0; t < TabCount; ++t)
            if (tabKey(t) == digit)
                tab = t;
        if (tab >= 0)
            setTab(tab);
        else
            m_status = i18n::tr("This tab arrives in a later version.");
        return true;
    }
    if (event == Event::Tab || event == Event::TabReverse) {
        m_contentFocused = !m_contentFocused;
        setTab(m_tab);
        return true;
    }
    if (raw == kCtrlP || event == Event::Escape) {
        m_contentFocused = false;
        return true;
    }
    if (event == Event::Return) {
        setTab(InfoTab);
        m_contentFocused = false;
        return true;
    }
    const int page = m_contentFocused ? m_contentHeight : m_tableHeight;
    if (event == Event::ArrowUp || event == Event::Character('k'))
        move(-1);
    else if (event == Event::ArrowDown || event == Event::Character('j'))
        move(1);
    else if (event == Event::PageUp)
        move(-std::max(1, page - 1));
    else if (event == Event::PageDown)
        move(std::max(1, page - 1));
    else if (event == Event::Home)
        move(-1000000);
    else if (event == Event::End)
        move(1000000);
    return true;
}

int App::run()
{
    auto screen = ScreenInteractive::Fullscreen();
    m_screen = &screen;

    InputOption option;
    option.multiline = false;
    m_input = Input(&m_filter, i18n::tr("Filter by name…"), option);

    auto renderer = Renderer(m_input, [this] {
        const ViewState state = viewState();
        Element filter = m_filterMode ? m_input->Render()
                                      : (m_filter.empty() ? text(i18n::tr("Filter by name…")) | dim
                                                          : text(m_filter));
        Element e = render(state, filter, &m_boxes);
        requestDetails();
        return e;
    });
    auto root = CatchEvent(renderer, [this](Event event) { return handleEvent(event); });

    screen.Post([this] { refresh(); });
    screen.Loop(root);

    if (m_worker.joinable())
        m_worker.join();
    if (m_detailWorker.joinable())
        m_detailWorker.join();
    m_screen = nullptr;
    return 0;
}

} // namespace tui
