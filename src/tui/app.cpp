#include "app.h"
#include "i18n.h"
#include "process.h"
#include "theme.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <ftxui/component/component.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/terminal.hpp>
#include <unistd.h>

using namespace ftxui;

namespace tui {

namespace {

// Teclas que o FTXUI entrega como sequência crua.
const std::string kCtrlE = "\x05";
const std::string kCtrlK = "\x0b";
const std::string kCtrlL = "\x0c";
const std::string kCtrlP = "\x10";
const std::string kCtrlQ = "\x11";
const std::string kCtrlR = "\x12";
const std::string kCtrlU = "\x15";
const std::string kAltA = "\x1b" "a";
const std::string kAltC = "\x1b" "c";
const std::string kAltI = "\x1b" "i";
const std::string kAltJ = "\x1b" "j";
const std::string kAltM = "\x1b" "m";
const std::string kAltT = "\x1b" "t";
const std::string kAltComma = "\x1b" ",";
const std::string kCtrlF = "\x06";
const std::string kCtrlG = "\x07";
const std::string kShiftF3 = "\x1b[1;2R";   // xterm; o FTXUI não tem um evento próprio

enum Menus { FileMenu = 0, ActionsMenu, HelpMenu };

// Linhas fixas da tela: título, separadores, cabeçalho, abas, status e teclas.
constexpr int kFixedLines = 9;
// Itens de uma lista mostrados no diálogo da transação; a lista completa vai à Saída.
constexpr size_t kDialogItems = 10;

// O cache das notícias fica ao lado do arquivo de preferências.
std::string newsCachePath(const std::string &settingsPath)
{
    const size_t slash = settingsPath.rfind('/');
    return (slash == std::string::npos ? std::string(".") : settingsPath.substr(0, slash)) + "/news.xml";
}

std::string defaultElevator()
{
    return ::geteuid() == 0 ? std::string() : std::string("doas");
}

// Remove o último caractere UTF-8 (Backspace no campo de texto).
void popUtf8(std::string &s)
{
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0) != 0x80)
            break;
    }
}

std::string join(const std::vector<std::string> &v, const std::string &sep)
{
    std::string out;
    for (size_t i = 0; i < v.size(); ++i)
        out += (i ? sep : std::string()) + v[i];
    return out;
}

// Até kDialogItems itens, um por linha, e "… e mais N".
std::string bulletList(const std::vector<std::string> &items)
{
    std::string out;
    for (size_t i = 0; i < items.size() && i < kDialogItems; ++i)
        out += "  • " + items[i] + "\n";
    if (items.size() > kDialogItems)
        out += "  " + i18n::trf("… and %1 more (see the Output tab)", {std::to_string(items.size() - kDialogItems)})
               + "\n";
    return out;
}

} // namespace

App::App(const Options &options)
    : m_settings(options.settingsPath)
    , m_mgr(options.pkgPath,
            options.elevator == "-" ? m_settings.value("elevator", defaultElevator()) : options.elevator,
            options.bectlPath)
    , m_iconMode(iconModeFromString(options.iconMode.empty() ? m_settings.value("icons", "nerd")
                                                             : options.iconMode))
    , m_remoteView(options.remote)
    , m_newsFeed(newsCachePath(m_settings.path()), options.fetchPath)
    , m_flavour(options.newsFlavour.empty() ? octo::systemFlavour() : octo::flavourFromString(options.newsFlavour))
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
    m_themeId = theme(options.theme.empty() ? m_settings.value("theme", "system") : options.theme).id;
    m_status = i18n::tr("Ready.");
}

App::~App()
{
    if (m_newsWorker.joinable())
        m_newsWorker.join();
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
    const auto list = [this, withRemote] {
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
        return l;
    };
    if (!m_screen) {   // testes
        applyListing(list());
        return;
    }
    if (m_worker.joinable())
        m_worker.join();
    m_worker = std::thread([this, list] {
        const Listing l = list();
        m_screen->Post([this, l] { applyListing(l); });
        m_screen->PostEvent(Event::Custom);
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

    if (l.error.empty() && m_operationError.empty()) {
        m_status = i18n::tr("Ready.");
        m_error = false;
    } else if (l.error.empty()) {
        // A listagem depois de uma operação que falhou não esconde o erro.
        m_status = i18n::trf("ERROR: %1", {m_operationError});
        m_error = true;
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
    const auto size = Terminal::Size();
    const int height = size.dimy;
    // Sobra espaço para o indicador de posição ("12/345") da aba com foco.
    m_contentWidth = std::max(20, size.dimx - 9);
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
    s.contentWidth = m_contentWidth;
    s.log = m_log;
    s.status = m_status;
    s.busy = m_busy;
    s.error = m_error;
    s.totalPackages = static_cast<int>(m_all.size());
    s.outdatedCount = m_outdated;
    s.iconMode = m_iconMode;
    s.sortChooserOpen = m_sortChooser;
    s.sortChooserIndex = m_sortChooserIndex;
    s.toInstall = m_toInstall;
    s.toRemove = m_toRemove;
    s.dialog = m_dialog;
    s.themeId = m_themeId;
    s.newsSource = octo::flavourName(m_flavour);
    s.news = m_news.items;
    s.newsLoading = m_newsBusy;
    s.newsFromCache = m_news.fromCache;
    s.newsError = m_news.error;
    s.newsUnseen = m_newsUnseen;
    s.findOpen = m_findOpen;
    s.findText = m_findText;
    if (m_findOpen && !m_findText.empty()) {
        const std::vector<int> found = findLines(tabText(s), m_findText);
        s.findCount = static_cast<int>(found.size());
        if (!found.empty()) {
            m_findIndex = ((m_findIndex % s.findCount) + s.findCount) % s.findCount;
            s.findMatchIndex = m_findIndex;
            s.findMatch = found[static_cast<size_t>(m_findIndex)];
        }
    }
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
    if (tab == NewsTab) {
        m_newsUnseen = false;
        if (!m_newsStarted)
            loadNews();
    }
    m_contentScroll = 0;
    if (tab == OutputTab && m_contentFocused)
        m_contentScroll = std::max(0, static_cast<int>(m_log.size()) - m_contentHeight);
}

void App::quit()
{
    // Uma listagem ou prévia pode ser abandonada (a saída espera o pkg
    // terminar); uma transação, não.
    if (m_inOperation) {
        m_status = i18n::tr("Wait for the current operation to finish.");
        return;
    }
    if (m_screen)
        m_screen->Exit();
}

// ---------------------------------------------------------------------------
// Marcas e transações (aba Ações do OctoPkg)
// ---------------------------------------------------------------------------

const octo::Package *App::current()
{
    const auto &r = rows();
    const int sel = model::rowToSelect(m_all, r, m_selectedName);
    return sel < 0 ? nullptr : &m_all[static_cast<size_t>(r[static_cast<size_t>(sel)])];
}

void App::markInstall()
{
    const octo::Package *p = current();
    if (!p)
        return;
    if (p->locked) {
        m_status = i18n::trf("%1 is locked.", {p->name});
        return;
    }
    // Como no OctoPkg, + de novo desmarca; instalar e remover se excluem.
    if (!m_toInstall.erase(p->name)) {
        m_toInstall.insert(p->name);
        m_toRemove.erase(p->name);
    }
}

void App::markRemove()
{
    const octo::Package *p = current();
    if (!p)
        return;
    if (!p->installed) {
        m_status = i18n::tr("Only installed packages can be removed.");
        return;
    }
    if (octo::isForbidden(p->name)) {
        m_status = i18n::trf("%1 cannot be removed.", {p->name});
        return;
    }
    if (p->locked) {
        m_status = i18n::trf("%1 is locked.", {p->name});
        return;
    }
    if (!m_toRemove.erase(p->name)) {
        m_toRemove.insert(p->name);
        m_toInstall.erase(p->name);
    }
}

void App::clearMarks()
{
    m_toInstall.clear();
    m_toRemove.clear();
}

bool App::bootEnvironments()
{
    if (m_beAvailable < 0)
        m_beAvailable = m_mgr.bootEnvironmentsAvailable() ? 1 : 0;
    return m_beAvailable == 1;
}

void App::actApply()
{
    if (refuseWhileBusy())
        return;
    if (m_toInstall.empty() && m_toRemove.empty()) {
        showMessage(i18n::tr("Actions"), i18n::tr("No packages marked. Use + to install and - to remove."));
        return;
    }
    const std::vector<std::string> rem(m_toRemove.begin(), m_toRemove.end());
    const std::vector<std::string> ins(m_toInstall.begin(), m_toInstall.end());
    // A prévia (dry-run) como usuário, como o OctoPkg faz antes do diálogo.
    runTask(i18n::tr("Collecting transaction data…"), [this, rem, ins]() -> Op {
        octo::TransactionPreview removal, install;
        if (!rem.empty())
            removal = m_mgr.previewRemove(rem);
        if (!ins.empty())
            install = m_mgr.previewInstall(ins);
        return [this, removal, install, rem, ins] { confirmTransaction(removal, install, rem, ins); };
    });
}

void App::confirmTransaction(const octo::TransactionPreview &removal, const octo::TransactionPreview &install,
                             const std::vector<std::string> &rem, const std::vector<std::string> &ins)
{
    // Removidos: os pedidos, os que dependem deles (-R) e os conflitos da instalação.
    std::vector<std::string> removed = removal.removed();
    for (const auto &i : install.removed())
        removed.push_back(i);
    const std::vector<std::string> retrieved = install.items("");

    std::string message;
    if (!retrieved.empty()) {
        message += retrieved.size() == 1 ? i18n::tr("The following package needs to be retrieved")
                                         : i18n::trf("The following %1 packages need to be retrieved",
                                                     {std::to_string(retrieved.size())});
        message += "\n" + bulletList(retrieved);
        if (!install.downloadSize.empty())
            message += i18n::trf("Total download size: %1", {install.downloadSize}) + "\n";
        message += "\n";
    }
    if (!removed.empty()) {
        message += removed.size() == 1 ? i18n::tr("The following package will be removed")
                                       : i18n::trf("The following %1 packages will be removed",
                                                   {std::to_string(removed.size())});
        message += "\n" + bulletList(removed) + "\n";
    }
    if (retrieved.empty() && removed.empty())
        message += i18n::tr("Nothing to do: the marked packages are already in this state.") + "\n\n";
    message += i18n::tr("Confirm?");

    log(i18n::tr("Transaction:"));
    for (const auto &i : retrieved)
        log("  + " + i);
    for (const auto &i : removed)
        log("  - " + i);

    // Como no OctoPkg: "Aviso" quando a remoção leva mais pacotes do que os marcados.
    const bool danger = removed.size() > rem.size();
    std::vector<DialogField> fields;
    const std::string be = octo::bootEnvironmentName(std::time(nullptr));
    if (bootEnvironments()) {
        DialogField f;
        f.kind = DialogField::Check;
        f.label = i18n::tr("Create a boot environment before the transaction");
        f.checked = !ins.empty();   // o OctoPkg desmarca na remoção
        f.hint = i18n::trf("Name: %1", {be});
        fields.push_back(f);
    }
    showForm(danger ? i18n::tr("Warning") : i18n::tr("Confirmation"), message, fields, danger,
             [this, rem, ins, be](const std::vector<DialogField> &f) {
                 const bool withBe = !f.empty() && f[0].checked;
                 std::string command;
                 if (withBe)
                     command = m_mgr.commandLine({"create", be}, true) + "; ";
                 if (!rem.empty())
                     command += m_mgr.commandLine({"remove", "-R", "-y", join(rem, " ")})
                                + (ins.empty() ? "" : "; ");
                 if (!ins.empty())
                     command += m_mgr.commandLine({"install", "-f", "-y", join(ins, " ")});
                 runOperation(
                     ins.empty() ? i18n::tr("Removing selected packages...")
                                 : i18n::tr("Installing selected packages..."),
                     command,
                     [this, withBe, be, rem, ins](const octo::OutputFn &out) {
                         // Sem shell: o pkg não roda se o Boot Environment falhar.
                         if (withBe)
                             m_mgr.createBootEnvironment(be, out);
                         if (!rem.empty())
                             m_mgr.remove(rem, out);
                         if (!ins.empty())
                             m_mgr.install(ins, out);
                     },
                     [this, rem, ins, withBe, be] {
                         for (const auto &n : rem)
                             m_toRemove.erase(n);
                         for (const auto &n : ins)
                             m_toInstall.erase(n);
                         std::string summary;
                         if (withBe)
                             summary += i18n::trf("Boot environment: %1", {be}) + "\n";
                         if (!ins.empty())
                             summary += i18n::trf("Installed: %1", {join(ins, ", ")}) + "\n";
                         if (!rem.empty())
                             summary += i18n::trf("Removed: %1", {join(rem, ", ")}) + "\n";
                         showMessage(i18n::tr("Command finished OK!"), summary + i18n::tr("Details in the Output tab."));
                     });
             });
}

void App::actUpgrade()
{
    if (refuseWhileBusy())
        return;
    runTask(i18n::tr("Collecting transaction data…"), [this]() -> Op {
        const octo::TransactionPreview p = m_mgr.previewUpgrade();
        return [this, p] {
            if (p.count() == 0) {
                showMessage(i18n::tr("System upgrade"), i18n::tr("There are no new updates available!"));
                return;
            }
            const std::vector<std::string> items = p.items("");
            std::string message = items.size() == 1 ? i18n::tr("The following package needs to be retrieved")
                                                    : i18n::trf("The following %1 packages need to be retrieved",
                                                                {std::to_string(items.size())});
            message += "\n" + bulletList(items);
            if (!p.removed().empty())
                message += i18n::trf("The following %1 packages will be removed", {std::to_string(p.removed().size())})
                           + "\n" + bulletList(p.removed());
            if (!p.downloadSize.empty())
                message += i18n::trf("Total download size: %1", {p.downloadSize}) + "\n";
            message += "\n" + i18n::tr("Confirm?");
            log(i18n::tr("Transaction:"));
            for (const auto &i : items)
                log("  + " + i);
            std::vector<DialogField> fields;
            const std::string be = octo::bootEnvironmentName(std::time(nullptr));
            if (bootEnvironments()) {
                DialogField f;
                f.kind = DialogField::Check;
                f.label = i18n::tr("Create a boot environment before the transaction");
                f.checked = true;
                f.hint = i18n::trf("Name: %1", {be});
                fields.push_back(f);
            }
            showForm(i18n::tr("System upgrade"), message, fields, false,
                     [this, be](const std::vector<DialogField> &f) {
                         const bool withBe = !f.empty() && f[0].checked;
                         runOperation(i18n::tr("Installing updates..."),
                                      (withBe ? m_mgr.commandLine({"create", be}, true) + "; " : std::string())
                                          + m_mgr.commandLine({"upgrade", "-y"}),
                                      [this, withBe, be](const octo::OutputFn &out) {
                                          if (withBe)
                                              m_mgr.createBootEnvironment(be, out);
                                          m_mgr.upgrade(out);
                                      },
                                      [this, withBe, be] {
                                          showMessage(i18n::tr("Command finished OK!"),
                                                      (withBe ? i18n::trf("Boot environment: %1", {be}) + "\n"
                                                              : std::string())
                                                          + i18n::tr("Details in the Output tab."));
                                      });
                     });
        };
    });
}

void App::actUpdate()
{
    runOperation(i18n::tr("Synchronizing the package database…"), m_mgr.commandLine({"update", "-f"}),
                 [this](const octo::OutputFn &out) { m_mgr.update(out); }, nullptr);
}

void App::actClean()
{
    if (refuseWhileBusy())
        return;
    showConfirm(i18n::tr("Clean the local cache"), i18n::tr("Do you really want to clean the local package cache?"),
                false, [this] {
                    runOperation(i18n::tr("Cleaning the local cache..."), m_mgr.commandLine({"clean", "-a", "-y"}),
                                 [this](const octo::OutputFn &out) { m_mgr.clean(out); }, nullptr);
                });
}

void App::actLock(bool lock)
{
    const octo::Package *p = current();
    if (!p || !p->installed) {
        m_status = i18n::tr("Select an installed package.");
        return;
    }
    const std::string name = p->name;
    runOperation(lock ? i18n::tr("Locking…") : i18n::tr("Unlocking…"),
                 m_mgr.commandLine({lock ? "lock" : "unlock", "-y", name}),
                 [this, lock, name](const octo::OutputFn &out) {
                     if (lock)
                         m_mgr.lock(name, out);
                     else
                         m_mgr.unlock(name, out);
                 },
                 nullptr);
}

void App::actAbout()
{
    std::string pkgVersion;
    try {
        pkgVersion = m_mgr.version();
    } catch (const std::exception &) {
        pkgVersion = "?";
    }
    showMessage(i18n::tr("About"), std::string("octopkg-tui ") + OCTOPKG_TUI_VERSION
                                        + "\npkg(8) " + pkgVersion + " · FTXUI"
                                        + "\n" + i18n::tr("Based on OctoPkg, © Alexandre Albuquerque Arnt")
                                        + "\nGPL-2.0-or-later"
                                        + "\nhttps://github.com/cleberalves40/octopkg-tui");
}

// ---------------------------------------------------------------------------
// Preferências, temas e busca nas abas
// ---------------------------------------------------------------------------

void App::nextThemeKey()
{
    m_themeId = nextTheme(m_themeId).id;
    m_settings.setValue("theme", m_themeId);
    m_settings.save();
    const Theme t = theme(m_themeId);
    m_status = i18n::trf("Theme: %1", {t.system ? i18n::tr("System") : std::string(t.label)});
}

void App::actPreferences()
{
    // As mesmas preferências do bectl-gui (tema e idioma), mais os ícones.
    DialogField themeField;
    themeField.kind = DialogField::Choice;
    themeField.label = i18n::tr("Theme:");
    const auto &list = themes();
    for (size_t i = 0; i < list.size(); ++i) {
        themeField.choices.push_back(list[i].system ? i18n::tr("System (terminal colors)") : list[i].label);
        if (m_themeId == list[i].id)
            themeField.choice = static_cast<int>(i);
    }
    DialogField lang;
    lang.kind = DialogField::Choice;
    lang.label = i18n::tr("Language:");
    const std::vector<std::string> langIds{"system", "en_US", "pt_BR", "es_ES"};
    lang.choices = {i18n::tr("System default"), "English", "Português (Brasil)", "Español"};
    const std::string savedLang = m_settings.value("language", "system");
    for (size_t i = 0; i < langIds.size(); ++i)
        if (savedLang == langIds[i])
            lang.choice = static_cast<int>(i);
    DialogField iconsField;
    iconsField.kind = DialogField::Choice;
    iconsField.label = i18n::tr("Icons:");
    const std::vector<IconMode> modes{IconMode::Nerd, IconMode::Unicode, IconMode::Ascii};
    iconsField.choices = {"Nerd Fonts", "Unicode", "ASCII"};
    for (size_t i = 0; i < modes.size(); ++i)
        if (m_iconMode == modes[i])
            iconsField.choice = static_cast<int>(i);
    iconsField.hint = i18n::tr("Nerd Fonts need a Nerd Font in the terminal.");

    showForm(i18n::tr("Preferences"), std::string(), {themeField, lang, iconsField}, false,
             [this, langIds, modes](const std::vector<DialogField> &f) {
                 m_themeId = themes()[static_cast<size_t>(f[0].choice)].id;
                 const std::string language = langIds[static_cast<size_t>(f[1].choice)];
                 m_iconMode = modes[static_cast<size_t>(f[2].choice)];
                 if (language == "system")
                     i18n::initFromEnvironment();
                 else
                     i18n::setLocale(language);
                 m_settings.setValue("theme", m_themeId);
                 m_settings.setValue("language", language);
                 m_settings.setValue("icons", iconModeName(m_iconMode));
                 m_status = m_settings.save() ? i18n::tr("Preferences saved.")
                                              : i18n::trf("Could not save %1", {m_settings.path()});
             });
    // O foco começa no primeiro campo, não no OK.
    m_dialog.focus = 0;
}

// ---------------------------------------------------------------------------
// Notícias (aba 5, Ctrl+G), como no OctoPkg
// ---------------------------------------------------------------------------

void App::loadNews()
{
    // Na primeira visita: as do cache na hora, e as novas em segundo plano.
    m_newsStarted = true;
    m_news = m_newsFeed.cached();
    refreshNews();
}

void App::refreshNews()
{
    m_newsStarted = true;
    if (m_newsBusy)
        return;
    if (octo::newsUrl(m_flavour).empty()) {
        m_news = m_newsFeed.cached();
        return;
    }
    log(i18n::trf("Searching for %1 latest news...", {octo::flavourName(m_flavour)}));
    m_newsBusy = true;
    if (!m_screen) {   // testes
        applyNews(m_newsFeed.refresh(m_flavour));
        return;
    }
    if (m_newsWorker.joinable())
        m_newsWorker.join();
    m_newsWorker = std::thread([this] {
        const octo::NewsResult r = m_newsFeed.refresh(m_flavour);
        m_screen->Post([this, r] { applyNews(r); });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::applyNews(const octo::NewsResult &result)
{
    m_newsBusy = false;
    m_news = result;
    if (!result.error.empty())
        log(i18n::trf("WARNING: %1", {result.error}));
    else
        log(i18n::trf("%1 news: %2 item(s).", {octo::flavourName(m_flavour), std::to_string(result.items.size())}));
    if (result.updated && m_tab != NewsTab)
        m_newsUnseen = true;
}

void App::openFind()
{
    // Como o Ctrl+F do OctoPkg: nas abas Arquivos, Saída, Notícias e Ajuda.
    if (m_tab != FilesTab && m_tab != OutputTab && m_tab != NewsTab && m_tab != HelpTab) {
        m_status = i18n::tr("Find works in the Files, Output, News and Help tabs.");
        return;
    }
    m_findOpen = true;
    m_findText.clear();
    m_findIndex = 0;
    m_contentFocused = false;
}

void App::findStep(int delta)
{
    m_findIndex += delta;
    // Rola até o resultado, no meio da aba se der.
    const ViewState s = viewState();
    if (s.findMatch >= 0)
        m_contentScroll = std::max(0, s.findMatch - std::max(1, m_contentHeight - 1) / 2);
}

bool App::handleFindEvent(const Event &event)
{
    const std::string &raw = event.input();
    if (event == Event::Escape || raw == kCtrlF) {
        m_findOpen = false;
        m_findText.clear();
        return true;
    }
    if (event == Event::Return || event == Event::F3 || event == Event::ArrowDown) {
        findStep(1);
        return true;
    }
    if (raw == kShiftF3 || event == Event::ArrowUp) {
        findStep(-1);
        return true;
    }
    if (event == Event::Backspace) {
        popUtf8(m_findText);
        m_findIndex = 0;
        findStep(0);
        return true;
    }
    if (event.is_character()) {
        m_findText += event.character();
        m_findIndex = 0;
        findStep(0);
        return true;
    }
    if (event.is_mouse())
        return handleMouse(event);
    return true;   // as demais teclas não saem da busca por engano
}

// ---------------------------------------------------------------------------
// Execução: prévias e operações elevadas
// ---------------------------------------------------------------------------

bool App::refuseWhileBusy()
{
    if (!m_busy)
        return false;
    m_status = i18n::tr("Wait for the current operation to finish.");
    return true;
}

void App::runTask(const std::string &status, const Task &task)
{
    if (refuseWhileBusy())
        return;
    const auto attempt = [task](std::string *error) -> Op {
        try {
            return task();
        } catch (const std::exception &e) {
            *error = e.what();
        }
        return nullptr;
    };
    const auto apply = [this](const Op &then, const std::string &error) {
        m_busy = false;
        m_status = i18n::tr("Ready.");
        if (!error.empty()) {
            m_error = true;
            m_status = i18n::trf("ERROR: %1", {error});
            log(m_status);
            showMessage(i18n::tr("Error"), error);
            return;
        }
        m_error = false;
        if (then)
            then();
    };
    if (!m_screen) {   // testes
        std::string error;
        const Op then = attempt(&error);
        apply(then, error);
        return;
    }
    m_busy = true;
    m_error = false;
    m_status = status;
    log(status);
    if (m_worker.joinable())
        m_worker.join();
    m_worker = std::thread([this, attempt, apply] {
        std::string error;
        const Op then = attempt(&error);
        m_screen->Post([apply, then, error] { apply(then, error); });
        m_screen->PostEvent(Event::Custom);
    });
}

// O doas/sudo pede senha? Testa com -n (não interativo) e uma chamada
// inofensiva do próprio pkg, que é o que a regra do doas.conf costuma liberar.
bool App::passwordNeeded()
{
    if (m_mgr.elevator().empty())
        return false;
    const std::string tool = octo::findExecutable(m_mgr.elevator());
    if (tool.empty())
        return false;   // a operação informa que o elevador não existe
    const auto r = octo::runProcess({tool, "-n", m_mgr.pkgPath(), "-v"}, 10000);
    return !r.started || r.status != 0;
}

void App::appendOutput(const std::string &chunk)
{
    // Linha a linha; "\r" (barras de progresso) recomeça a linha atual.
    for (char c : chunk) {
        if (c == '\n') {
            m_log.push_back("  " + m_partial);
            m_partial.clear();
        } else if (c == '\r') {
            m_partial.clear();
        } else {
            m_partial += c;
        }
    }
    while (m_log.size() > 2000)
        m_log.erase(m_log.begin());
}

void App::runOperation(const std::string &status, const std::string &command, const Work &work,
                       const Op &onSuccess)
{
    if (refuseWhileBusy())
        return;
    m_operationError.clear();
    log(command);
    // Como no OctoPkg, a aba Saída acompanha a transação.
    m_tab = OutputTab;
    m_contentFocused = false;
    m_partial.clear();
    const auto attempt = [work](const octo::OutputFn &out) {
        try {
            work(out);
        } catch (const std::exception &e) {
            return std::string(e.what());
        }
        return std::string();
    };

    if (!m_screen) {   // testes
        finishOperation(attempt([this](const std::string &c) { appendOutput(c); }), onSuccess);
        return;
    }

    if (passwordNeeded()) {
        // O doas/sudo pede a senha no terminal: a tela é suspensa enquanto
        // isso, e a saída aparece no terminal e depois na aba Saída.
        std::string error, captured;
        m_screen->WithRestoredIO([&] {
            std::printf("\noctopkg-tui — %s\n%s\n\n", i18n::trf("Running: %1", {command}).c_str(),
                        i18n::trf("%1 may ask for your password.", {m_mgr.elevator()}).c_str());
            std::fflush(stdout);
            error = attempt([&captured](const std::string &c) {
                std::fwrite(c.data(), 1, c.size(), stdout);
                std::fflush(stdout);
                captured += c;
            });
        })();
        appendOutput(captured);
        finishOperation(error, onSuccess);
        return;
    }

    m_busy = true;
    m_inOperation = true;
    m_error = false;
    m_status = status;
    if (m_worker.joinable())
        m_worker.join();
    m_worker = std::thread([this, attempt, onSuccess] {
        const std::string error = attempt([this](const std::string &c) {
            m_screen->Post([this, c] { appendOutput(c); });
            m_screen->PostEvent(Event::Custom);
        });
        m_screen->Post([this, error, onSuccess] { finishOperation(error, onSuccess); });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::finishOperation(const std::string &error, const Op &onSuccess)
{
    m_busy = false;
    m_inOperation = false;
    if (!m_partial.empty())
        appendOutput("\n");
    if (!error.empty()) {
        m_operationError = error;
        m_error = true;
        m_status = i18n::trf("ERROR: %1", {error});
        log(m_status);
        showMessage(i18n::tr("Error"), error);
        refresh();   // a transação pode ter feito parte do trabalho
        return;
    }
    m_error = false;
    m_status = i18n::tr("Ready.");
    log(i18n::tr("Command finished OK!"));
    if (onSuccess)
        onSuccess();
    refresh();
}

// ---------------------------------------------------------------------------
// Diálogos e menus
// ---------------------------------------------------------------------------

void App::showMessage(const std::string &title, const std::string &message)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Message;
    m_dialog.title = title;
    m_dialog.message = message;
}

void App::showConfirm(const std::string &title, const std::string &message, bool danger, const Op &onYes)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Confirm;
    m_dialog.title = title;
    m_dialog.message = message;
    m_dialog.danger = danger;
    m_dialog.focus = danger ? 1 : 0;   // destrutivo: "Não" por padrão
    m_onAccept = onYes;
}

void App::showForm(const std::string &title, const std::string &message, std::vector<DialogField> fields,
                   bool danger, FormOp onAccept)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Form;
    m_dialog.title = title;
    m_dialog.message = message;
    m_dialog.fields = std::move(fields);
    m_dialog.danger = danger;
    // O foco começa no botão: OK, ou Cancelar se a transação remove a mais.
    m_dialog.focus = static_cast<int>(m_dialog.fields.size()) + (danger ? 1 : 0);
    m_onForm = std::move(onAccept);
}

void App::openMenu(int menu)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Menu;
    m_dialog.menus = {i18n::tr("File"), i18n::tr("Actions"), i18n::tr("Help")};
    const auto sep = std::make_pair(std::string(), std::string());
    m_dialog.items = {
        {{i18n::tr("Refresh the package list"), "F5"},
         {i18n::tr("Switch between installed and repository packages"), "i"},
         sep,
         {i18n::tr("Check updates"), "Ctrl+K"},
         {i18n::tr("Install updates"), "Ctrl+U"},
         {i18n::tr("Clean the local cache"), ""},
         {i18n::tr("Get the latest news"), "Ctrl+G"},
         sep,
         {i18n::tr("Preferences…"), "Alt+,"},
         {i18n::tr("Next theme"), "Alt+T"},
         sep,
         {i18n::tr("Exit"), "Ctrl+Q"}},
        {{i18n::tr("Mark for installation"), "+"},
         {i18n::tr("Mark for removal"), "-"},
         sep,
         {i18n::tr("Apply"), "Alt+M"},
         {i18n::tr("Cancel"), "Ctrl+E"},
         sep,
         {i18n::tr("Lock"), ""},
         {i18n::tr("Unlock"), ""}},
        {{i18n::tr("Keyboard shortcuts"), "F1"}, {i18n::tr("About"), ""}},
    };
    m_menuActions = {
        {[this] { refresh(); }, [this] { toggleView(); }, nullptr, [this] { actUpdate(); },
         [this] { actUpgrade(); }, [this] { actClean(); },
         [this] { setTab(NewsTab); refreshNews(); }, nullptr, [this] { actPreferences(); },
         [this] { nextThemeKey(); }, nullptr, [this] { quit(); }},
        {[this] { markInstall(); }, [this] { markRemove(); }, nullptr, [this] { actApply(); },
         [this] { clearMarks(); }, nullptr, [this] { actLock(true); }, [this] { actLock(false); }},
        {[this] { setTab(HelpTab); }, [this] { actAbout(); }},
    };
    m_dialog.menu = std::max(0, std::min(menu, static_cast<int>(m_dialog.menus.size()) - 1));
}

bool App::handleDialogEvent(const Event &event)
{
    Dialog &d = m_dialog;
    switch (d.kind) {
    case Dialog::Message:
        if (event == Event::Return || event == Event::Escape || event == Event::Character(' '))
            d = Dialog{};
        return true;

    case Dialog::Confirm:
        if (event == Event::ArrowLeft || event == Event::ArrowRight || event == Event::Tab
            || event == Event::TabReverse)
            d.focus = 1 - d.focus;
        else if (event == Event::Escape || event == Event::Character('n') || event == Event::Character('N'))
            d = Dialog{};
        else if (event == Event::Return || event == Event::Character('y') || event == Event::Character('s')
                 || event == Event::Character('Y') || event == Event::Character('S')) {
            const bool yes = event != Event::Return || d.focus == 0;
            const Op cb = m_onAccept;
            d = Dialog{};
            if (yes && cb)
                cb();
        }
        return true;

    case Dialog::Form: {
        const int total = static_cast<int>(d.fields.size()) + 2;   // campos + OK + Cancelar
        if (event == Event::Escape) {
            d = Dialog{};
            return true;
        }
        if (event == Event::Tab || event == Event::ArrowDown) {
            d.focus = (d.focus + 1) % total;
            return true;
        }
        if (event == Event::TabReverse || event == Event::ArrowUp) {
            d.focus = (d.focus + total - 1) % total;
            return true;
        }
        if (event == Event::Return) {
            if (d.focus == total - 1) {   // Cancelar
                d = Dialog{};
                return true;
            }
            const std::vector<DialogField> fields = d.fields;
            const FormOp cb = m_onForm;
            d = Dialog{};
            if (cb)
                cb(fields);
            return true;
        }
        if (d.focus >= static_cast<int>(d.fields.size()))
            return true;
        DialogField &f = d.fields[static_cast<size_t>(d.focus)];
        switch (f.kind) {
        case DialogField::Text:
            if (event == Event::Backspace)
                popUtf8(f.text);
            else if (event.is_character())
                f.text += event.character();
            break;
        case DialogField::Check:
            if (event == Event::Character(' '))
                f.checked = !f.checked;
            break;
        case DialogField::Choice:
            if (!f.choices.empty() && (event == Event::ArrowRight || event == Event::Character(' ')))
                f.choice = (f.choice + 1) % static_cast<int>(f.choices.size());
            else if (!f.choices.empty() && event == Event::ArrowLeft)
                f.choice = (f.choice + static_cast<int>(f.choices.size()) - 1) % static_cast<int>(f.choices.size());
            break;
        }
        return true;
    }

    case Dialog::Menu: {
        const int menus = static_cast<int>(d.menus.size());
        const auto &items = d.items[static_cast<size_t>(d.menu)];
        const auto step = [&](int delta) {
            const int n = static_cast<int>(items.size());
            for (int i = 0; i < n; ++i) {
                d.item = (d.item + delta + n) % n;
                if (!items[static_cast<size_t>(d.item)].first.empty())
                    break;   // separadores não são selecionáveis
            }
        };
        if (event == Event::Escape || event == Event::F9 || event == Event::F10)
            d = Dialog{};
        else if (event == Event::ArrowLeft) {
            d.menu = (d.menu + menus - 1) % menus;
            d.item = 0;
        } else if (event == Event::ArrowRight) {
            d.menu = (d.menu + 1) % menus;
            d.item = 0;
        } else if (event == Event::ArrowUp)
            step(-1);
        else if (event == Event::ArrowDown)
            step(1);
        else if (event == Event::Return) {
            const Op action = m_menuActions[static_cast<size_t>(d.menu)][static_cast<size_t>(d.item)];
            d = Dialog{};
            if (action)
                action();
        }
        return true;
    }

    case Dialog::None:
        break;
    }
    return false;
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
    // Diálogos e menus têm o teclado só para eles.
    if (m_dialog.kind != Dialog::None) {
        if (event.is_mouse())
            return true;
        return handleDialogEvent(event);
    }

    if (m_findOpen)
        return handleFindEvent(event);

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
        m_operationError.clear();
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
    if (event == Event::F9 || raw == kAltA) {
        openMenu(FileMenu);
        return true;
    }
    if (raw == kAltC) {
        openMenu(ActionsMenu);
        return true;
    }
    if (raw == kAltJ) {
        openMenu(HelpMenu);
        return true;
    }
    // Marcas do OctoPkg: + instalar, - remover (e Insert/Delete).
    if (event == Event::Character('+') || event == Event::Insert) {
        markInstall();
        return true;
    }
    if (event == Event::Character('-') || event == Event::Delete) {
        markRemove();
        return true;
    }
    if (raw == kCtrlE) {
        clearMarks();
        return true;
    }
    if (raw == kCtrlF) {
        openFind();
        return true;
    }
    if (raw == kCtrlG) {
        setTab(NewsTab);
        if (m_newsStarted && !m_newsBusy)
            refreshNews();
        return true;
    }
    if (raw == kAltT) {
        nextThemeKey();
        return true;
    }
    if (raw == kAltComma) {
        actPreferences();
        return true;
    }
    // Ctrl+M é Enter no terminal: aplicar fica em Alt+M.
    if (raw == kAltM) {
        actApply();
        return true;
    }
    if (raw == kCtrlK) {
        actUpdate();
        return true;
    }
    if (raw == kCtrlU) {
        actUpgrade();
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
        // A primeira listagem parte da primeira renderização, já dentro do
        // laço: no FTXUI 6, uma tarefa enviada com Post() antes de Loop() é
        // descartada, e a lista nunca aparecia.
        if (!m_started) {
            m_started = true;
            refresh();
        }
        const ViewState state = viewState();
        Element filter = m_filterMode ? m_input->Render()
                                      : (m_filter.empty() ? text(i18n::tr("Filter by name…")) | dim
                                                          : text(m_filter));
        Element e = render(state, filter, &m_boxes);
        requestDetails();
        return e;
    });
    auto root = CatchEvent(renderer, [this](Event event) { return handleEvent(event); });

    screen.Loop(root);

    if (m_newsWorker.joinable())
        m_newsWorker.join();
    if (m_worker.joinable())
        m_worker.join();
    if (m_detailWorker.joinable())
        m_detailWorker.join();
    m_screen = nullptr;
    return 0;
}

} // namespace tui
