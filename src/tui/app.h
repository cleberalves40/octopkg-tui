#ifndef OCTOPKG_TUI_APP_H
#define OCTOPKG_TUI_APP_H

#include "icons.h"
#include "listmodel.h"
#include "pkg.h"
#include "settings.h"
#include "view.h"

#include <atomic>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
// FTXUI 7 define ScreenInteractive como apelido de ftxui::App; o cabeçalho
// real evita conflito com uma declaração antecipada.
#include <ftxui/component/screen_interactive.hpp>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace tui {

struct Options {
    std::string pkgPath;         // vazio: pkg(8) do sistema
    std::string language;        // vazio: preferência salva ou ambiente
    std::string iconMode;        // vazio: preferência salva ou "nerd"
    std::string settingsPath;    // vazio: ~/.config/octopkg-tui/config
    bool remote = false;         // começa na visão Repositório
};

/** Resultado de uma listagem, aplicado na thread da interface. */
struct Listing {
    std::vector<octo::Package> installed;
    std::map<std::string, octo::Upgrade> outdated;
    bool hasRemote = false;
    std::vector<octo::Package> remote;
    std::string error;
    std::string warning;         // falha só na lista de desatualizados
};

class App
{
public:
    explicit App(const Options &options);
    ~App();

    /** Laço da interface em tela cheia; devolve o código de saída. */
    int run();

    // Para os testes: o estado que a tela mostraria e o tratamento de eventos.
    ViewState viewState();
    bool handleEvent(const ftxui::Event &event);
    void applyListing(const Listing &listing);
    void applyDetails(const std::string &key, const octo::Details &details, const std::string &error);
    /** Chave do cache de detalhes do pacote selecionado ("I:nome" ou "R:nome"). */
    std::string currentDetailsKey();
    bool filterMode() const { return m_filterMode; }
    std::string &filterText() { return m_filter; }
    const octo::Settings &settings() const { return m_settings; }
    void setTableHeight(int rows) { m_tableHeight = rows; }

private:
    void refresh();
    void toggleView();
    void requestDetails();
    void log(const std::string &message);
    void setSort(model::SortSpec sort);
    void move(int delta);
    void scrollContent(int delta);
    void setTab(int tab);
    bool handleMouse(const ftxui::Event &event);
    void quit();
    const std::vector<int> &rows();
    void rebuild();
    void layout();

    octo::Settings m_settings;
    octo::Manager m_mgr;
    IconMode m_iconMode;

    std::vector<octo::Package> m_installed;    // com os desatualizados marcados
    std::vector<octo::Package> m_remote;
    bool m_hasRemote = false;
    bool m_remoteView = false;
    std::vector<octo::Package> m_all;          // a visão atual
    int m_outdated = 0;

    // Linhas filtradas e ordenadas, refeitas só quando algo muda.
    std::vector<int> m_rows;
    std::string m_rowsFilter;
    model::SortSpec m_rowsSort{-2, true};
    bool m_rowsDirty = true;

    std::string m_filter;
    model::SortSpec m_sort;
    std::string m_selectedName;
    std::string m_shownName;                   // pacote da última tela, para zerar a rolagem
    int m_offset = 0;
    int m_tableHeight = 1000;
    int m_contentHeight = 12;
    int m_tab = InfoTab;
    bool m_contentFocused = false;
    int m_contentScroll = 0;
    bool m_filterMode = false;
    bool m_started = false;                    // primeira listagem já disparada
    bool m_sortChooser = false;
    int m_sortChooserIndex = 0;

    std::map<std::string, octo::Details> m_details;   // cache, por currentDetailsKey()
    std::map<std::string, std::string> m_detailErrors;
    std::atomic<bool> m_detailBusy{false};
    std::thread m_detailWorker;

    std::vector<std::string> m_log;
    std::string m_status;
    bool m_error = false;
    std::atomic<bool> m_busy{false};
    std::thread m_worker;

    ftxui::ScreenInteractive *m_screen = nullptr;
    HitBoxes m_boxes;
    ftxui::Component m_input;
};

} // namespace tui

#endif
